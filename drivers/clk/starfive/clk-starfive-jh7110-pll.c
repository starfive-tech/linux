// SPDX-License-Identifier: GPL-2.0
/*
 * StarFive JH7110 PLL Clock Generator Driver
 *
 * Copyright (C) 2023 StarFive Technology Co., Ltd.
 *
 * This driver is about to register JH7110 PLL clock generator and support ops.
 * The JH7110 have three PLL clock, PLL0, PLL1 and PLL2.
 * Each PLL clocks work in integer mode or fraction mode by some dividers,
 * and the configuration registers and dividers are set in several syscon registers.
 * The formula for calculating frequency is:
 * Fvco = Fref * (NI + NF) / M / Q1
 * Fref: OSC source clock rate
 * NI: integer frequency dividing ratio of feedback divider, set by fbdiv[11:0].
 * NF: fractional frequency dividing ratio, set by frac[23:0]. NF = frac[23:0] / 2^24 = 0 ~ 0.999.
 * M: frequency dividing ratio of pre-divider, set by prediv[5:0].
 * Q1: frequency dividing ratio of post divider, set by postdiv1[1:0], Q1= 1,2,4,8.
 */

#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/of_platform.h>

#include <dt-bindings/clock/starfive,jh7110-crg.h>

#include "clk-starfive-jh7110-pll.h"

struct jh7110_pll_conf_variant {
	unsigned int pll_nums;
	struct jh7110_pll_syscon_conf conf[];
};

static const struct jh7110_pll_conf_variant jh7110_pll_variant = {
	.pll_nums = JH7110_PLLCLK_END,
	.conf = {
		JH7110_PLL(JH7110_CLK_PLL0_OUT, "pll0_out",
			   JH7110_PLL0_FREQ_MAX, jh7110_pll0_syscon_val_preset),
		JH7110_PLL(JH7110_CLK_PLL1_OUT, "pll1_out",
			   JH7110_PLL1_FREQ_MAX, jh7110_pll1_syscon_val_preset),
		JH7110_PLL(JH7110_CLK_PLL2_OUT, "pll2_out",
			   JH7110_PLL2_FREQ_MAX, jh7110_pll2_syscon_val_preset),
	},
};

static struct jh7110_clk_pll_data *jh7110_pll_data_from(struct clk_hw *hw)
{
	return container_of(hw, struct jh7110_clk_pll_data, hw);
}

static struct jh7110_clk_pll_priv *jh7110_pll_priv_from(struct jh7110_clk_pll_data *data)
{
	return container_of(data, struct jh7110_clk_pll_priv, data[data->idx]);
}

/* Read register value from syscon and calculate PLL(x) frequency */
static unsigned long jh7110_pll_get_freq(struct jh7110_clk_pll_data *data,
					 unsigned long parent_rate)
{
	struct jh7110_clk_pll_priv *priv = jh7110_pll_priv_from(data);
	struct jh7110_pll_syscon_offset *offset = &data->conf.offsets;
	struct jh7110_pll_syscon_mask *mask = &data->conf.masks;
	struct jh7110_pll_syscon_shift *shift = &data->conf.shifts;
	unsigned long frac_cal;
	u32 dacpd;
	u32 dsmpd;
	u32 fbdiv;
	u32 prediv;
	u32 postdiv1;
	u32 frac;
	u32 reg_val;

	regmap_read(priv->syscon_regmap, offset->dacpd, &reg_val);
	dacpd = (reg_val & mask->dacpd) >> shift->dacpd;

	regmap_read(priv->syscon_regmap, offset->dsmpd, &reg_val);
	dsmpd = (reg_val & mask->dsmpd) >> shift->dsmpd;

	regmap_read(priv->syscon_regmap, offset->fbdiv, &reg_val);
	fbdiv = (reg_val & mask->fbdiv) >> shift->fbdiv;

	regmap_read(priv->syscon_regmap, offset->prediv, &reg_val);
	prediv = (reg_val & mask->prediv) >> shift->prediv;

	regmap_read(priv->syscon_regmap, offset->postdiv1, &reg_val);
	/* postdiv1 = 2 ^ reg_val */
	postdiv1 = 1 << ((reg_val & mask->postdiv1) >> shift->postdiv1);

	regmap_read(priv->syscon_regmap, offset->frac, &reg_val);
	frac = (reg_val & mask->frac) >> shift->frac;

	/*
	 * Integer Mode (Both 1) or Fraction Mode (Both 0).
	 * And the decimal places are counted by expanding them by
	 * a factor of STARFIVE_PLL_FRAC_PATR_SIZE.
	 */
	if (dacpd == 1 && dsmpd == 1)
		frac_cal = 0;
	else if (dacpd == 0 && dsmpd == 0)
		frac_cal = (unsigned long)frac * STARFIVE_PLL_FRAC_PATR_SIZE / (1 << 24);
	else
		return 0;

	/* Fvco = Fref * (NI + NF) / M / Q1 */
	return (parent_rate / STARFIVE_PLL_FRAC_PATR_SIZE *
		(fbdiv * STARFIVE_PLL_FRAC_PATR_SIZE + frac_cal) / prediv / postdiv1);
}

static unsigned long jh7110_pll_rate_sub_fabs(unsigned long rate1, unsigned long rate2)
{
	return rate1 > rate2 ? (rate1 - rate2) : (rate2 - rate1);
}

/* Select the appropriate frequency from the already configured registers value */
static void jh7110_pll_select_near_freq_id(struct jh7110_clk_pll_data *data,
					   unsigned long rate)
{
	const struct jh7110_pll_syscon_val *val;
	unsigned int id;
	unsigned long rate_diff;

	/* compare the frequency one by one from small to large in order */
	for (id = 0; id < data->conf.preset_val_nums; id++) {
		val = &data->conf.preset_val[id];

		if (rate == val->freq)
			goto match_end;

		/* select near frequency */
		if (rate < val->freq) {
			/* The last frequency is closer to the target rate than this time. */
			if (id > 0)
				if (rate_diff < jh7110_pll_rate_sub_fabs(rate, val->freq))
					id--;

			goto match_end;
		} else {
			rate_diff = jh7110_pll_rate_sub_fabs(rate, val->freq);
		}
	}

match_end:
	data->freq_select_idx = id;
}

static int jh7110_pll_set_freq_syscon(struct jh7110_clk_pll_data *data)
{
	struct jh7110_clk_pll_priv *priv = jh7110_pll_priv_from(data);
	struct jh7110_pll_syscon_offset *offset = &data->conf.offsets;
	struct jh7110_pll_syscon_mask *mask = &data->conf.masks;
	struct jh7110_pll_syscon_shift *shift = &data->conf.shifts;
	const struct jh7110_pll_syscon_val *val = &data->conf.preset_val[data->freq_select_idx];

	/* frac: Integer Mode (Both 1) or Fraction Mode (Both 0) */
	if (val->dacpd == 0 && val->dsmpd == 0)
		regmap_update_bits(priv->syscon_regmap, offset->frac, mask->frac,
				   (val->frac << shift->frac));
	else if (val->dacpd != val->dsmpd)
		return -EINVAL;

	/* fbdiv value should be 8 to 4095 */
	if (val->fbdiv < 8)
		return -EINVAL;

	regmap_update_bits(priv->syscon_regmap, offset->dacpd, mask->dacpd,
			   (val->dacpd << shift->dacpd));
	regmap_update_bits(priv->syscon_regmap, offset->dsmpd, mask->dsmpd,
			   (val->dsmpd << shift->dsmpd));
	regmap_update_bits(priv->syscon_regmap, offset->prediv, mask->prediv,
			   (val->prediv << shift->prediv));
	regmap_update_bits(priv->syscon_regmap, offset->fbdiv, mask->fbdiv,
			   (val->fbdiv << shift->fbdiv));
	regmap_update_bits(priv->syscon_regmap, offset->postdiv1, mask->postdiv1,
			   ((val->postdiv1 >> 1) << shift->postdiv1));

	return 0;
}

static unsigned long jh7110_pll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct jh7110_clk_pll_data *data = jh7110_pll_data_from(hw);

	return jh7110_pll_get_freq(data, parent_rate);
}

static int jh7110_pll_determine_rate(struct clk_hw *hw, struct clk_rate_request *req)
{
	struct jh7110_clk_pll_data *data = jh7110_pll_data_from(hw);

	jh7110_pll_select_near_freq_id(data, req->rate);
	req->rate = data->conf.preset_val[data->freq_select_idx].freq;

	return 0;
}

static int jh7110_pll_set_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long parent_rate)
{
	struct jh7110_clk_pll_data *data = jh7110_pll_data_from(hw);

	return jh7110_pll_set_freq_syscon(data);
}

#ifdef CONFIG_DEBUG_FS
static void jh7110_pll_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	static const struct debugfs_reg32 jh7110_clk_pll_reg = {
		.name = "CTRL",
		.offset = 0,
	};
	struct jh7110_clk_pll_data *data = jh7110_pll_data_from(hw);
	struct jh7110_clk_pll_priv *priv = jh7110_pll_priv_from(data);
	struct debugfs_regset32 *regset;

	regset = devm_kzalloc(priv->dev, sizeof(*regset), GFP_KERNEL);
	if (!regset)
		return;

	regset->regs = &jh7110_clk_pll_reg;
	regset->nregs = 1;

	debugfs_create_regset32("registers", 0400, dentry, regset);
}
#else
#define jh7110_pll_debug_init NULL
#endif

static const struct clk_ops jh7110_pll_ops = {
	.recalc_rate = jh7110_pll_recalc_rate,
	.determine_rate = jh7110_pll_determine_rate,
	.set_rate = jh7110_pll_set_rate,
	.debug_init = jh7110_pll_debug_init,
};

static struct clk_hw *jh7110_pll_get(struct of_phandle_args *clkspec, void *data)
{
	struct jh7110_clk_pll_priv *priv = data;
	unsigned int idx = clkspec->args[0];

	if (idx < priv->pll_nums)
		return &priv->data[idx].hw;

	return ERR_PTR(-EINVAL);
}

static int jh7110_pll_probe(struct platform_device *pdev)
{
	const struct jh7110_pll_conf_variant *variant;
	struct jh7110_clk_pll_priv *priv;
	struct jh7110_clk_pll_data *data;
	int ret;
	unsigned int idx;

	variant = of_device_get_match_data(&pdev->dev);
	if (!variant)
		return -ENOMEM;

	priv = devm_kzalloc(&pdev->dev, struct_size(priv, data, variant->pll_nums),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;
	priv->syscon_regmap = syscon_node_to_regmap(priv->dev->of_node->parent);
	if (IS_ERR(priv->syscon_regmap))
		return PTR_ERR(priv->syscon_regmap);

	priv->pll_nums = variant->pll_nums;
	for (idx = 0; idx < priv->pll_nums; idx++) {
		struct clk_parent_data parents = {
			.index = 0,
		};
		struct clk_init_data init = {
			.name = variant->conf[idx].name,
			.ops = &jh7110_pll_ops,
			.parent_data = &parents,
			.num_parents = 1,
			.flags = 0,
		};

		data = &priv->data[idx];
		data->conf = variant->conf[idx];
		data->hw.init = &init;
		data->idx = idx;

		ret = devm_clk_hw_register(&pdev->dev, &data->hw);
		if (ret)
			return ret;
	}

	return devm_of_clk_add_hw_provider(&pdev->dev, jh7110_pll_get, priv);
}

static const struct of_device_id jh7110_pll_match[] = {
	{ .compatible = "starfive,jh7110-pll", .data = &jh7110_pll_variant },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, jh7110_pll_match);

static struct platform_driver jh7110_pll_driver = {
	.driver = {
		.name = "clk-starfive-jh7110-pll",
		.of_match_table = jh7110_pll_match,
	},
};
builtin_platform_driver_probe(jh7110_pll_driver, jh7110_pll_probe);
