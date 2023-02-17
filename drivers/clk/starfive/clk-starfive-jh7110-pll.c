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

#include <dt-bindings/clock/starfive,jh7110-crg.h>

#include "clk-starfive-jh7110-pll.h"

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
	struct jh7110_pll_syscon_offset *offset = &data->offset;
	struct jh7110_pll_syscon_mask *mask = &data->mask;
	struct jh7110_pll_syscon_shift *shift = &data->shift;
	unsigned long freq = 0;
	unsigned long frac_cal;
	u32 dacpd;
	u32 dsmpd;
	u32 fbdiv;
	u32 prediv;
	u32 postdiv1;
	u32 frac;
	u32 reg_val;

	if (regmap_read(priv->syscon_regmap, offset->dacpd, &reg_val))
		goto read_error;
	dacpd = (reg_val & mask->dacpd) >> shift->dacpd;

	if (regmap_read(priv->syscon_regmap, offset->dsmpd, &reg_val))
		goto read_error;
	dsmpd = (reg_val & mask->dsmpd) >> shift->dsmpd;

	if (regmap_read(priv->syscon_regmap, offset->fbdiv, &reg_val))
		goto read_error;
	fbdiv = (reg_val & mask->fbdiv) >> shift->fbdiv;
	/* fbdiv value should be 8 to 4095 */
	if (fbdiv < 8)
		goto read_error;

	if (regmap_read(priv->syscon_regmap, offset->prediv, &reg_val))
		goto read_error;
	prediv = (reg_val & mask->prediv) >> shift->prediv;

	if (regmap_read(priv->syscon_regmap, offset->postdiv1, &reg_val))
		goto read_error;
	/* postdiv1 = 2 ^ reg_val */
	postdiv1 = 1 << ((reg_val & mask->postdiv1) >> shift->postdiv1);

	if (regmap_read(priv->syscon_regmap, offset->frac, &reg_val))
		goto read_error;
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
		goto read_error;

	/* Fvco = Fref * (NI + NF) / M / Q1 */
	freq = parent_rate / STARFIVE_PLL_FRAC_PATR_SIZE *
	       (fbdiv * STARFIVE_PLL_FRAC_PATR_SIZE + frac_cal) / prediv / postdiv1;

read_error:
	return freq;
}

static unsigned long jh7110_pll_rate_sub_fabs(unsigned long rate1, unsigned long rate2)
{
	return rate1 > rate2 ? (rate1 - rate2) : (rate2 - rate1);
}

/* Select the appropriate frequency from the already configured registers value */
static void jh7110_pll_select_near_freq_id(struct jh7110_clk_pll_data *data,
					   unsigned long rate)
{
	const struct starfive_pll_syscon_value *syscon_val;
	unsigned int id;
	unsigned int pll_arry_size;
	unsigned long rate_diff;

	if (data->idx == JH7110_CLK_PLL0_OUT)
		pll_arry_size = ARRAY_SIZE(jh7110_pll0_syscon_freq);
	else if (data->idx == JH7110_CLK_PLL1_OUT)
		pll_arry_size = ARRAY_SIZE(jh7110_pll1_syscon_freq);
	else
		pll_arry_size = ARRAY_SIZE(jh7110_pll2_syscon_freq);

	/* compare the frequency one by one from small to large in order */
	for (id = 0; id < pll_arry_size; id++) {
		if (data->idx == JH7110_CLK_PLL0_OUT)
			syscon_val = &jh7110_pll0_syscon_freq[id];
		else if (data->idx == JH7110_CLK_PLL1_OUT)
			syscon_val = &jh7110_pll1_syscon_freq[id];
		else
			syscon_val = &jh7110_pll2_syscon_freq[id];

		if (rate == syscon_val->freq)
			goto match_end;

		/* select near frequency */
		if (rate < syscon_val->freq) {
			/* The last frequency is closer to the target rate than this time. */
			if (id > 0)
				if (rate_diff < jh7110_pll_rate_sub_fabs(rate, syscon_val->freq))
					id--;

			goto match_end;
		} else {
			rate_diff = jh7110_pll_rate_sub_fabs(rate, syscon_val->freq);
		}
	}

match_end:
	data->freq_select_idx = id;
}

static int jh7110_pll_set_freq_syscon(struct jh7110_clk_pll_data *data)
{
	struct jh7110_clk_pll_priv *priv = jh7110_pll_priv_from(data);
	struct jh7110_pll_syscon_offset *offset = &data->offset;
	struct jh7110_pll_syscon_mask *mask = &data->mask;
	struct jh7110_pll_syscon_shift *shift = &data->shift;
	unsigned int freq_idx = data->freq_select_idx;
	const struct starfive_pll_syscon_value *syscon_val;
	int ret;

	if (data->idx == JH7110_CLK_PLL0_OUT)
		syscon_val = &jh7110_pll0_syscon_freq[freq_idx];
	else if (data->idx == JH7110_CLK_PLL1_OUT)
		syscon_val = &jh7110_pll1_syscon_freq[freq_idx];
	else
		syscon_val = &jh7110_pll2_syscon_freq[freq_idx];

	ret = regmap_update_bits(priv->syscon_regmap, offset->dacpd, mask->dacpd,
				 (syscon_val->dacpd << shift->dacpd));
	if (ret)
		goto set_failed;

	ret = regmap_update_bits(priv->syscon_regmap, offset->dsmpd, mask->dsmpd,
				 (syscon_val->dsmpd << shift->dsmpd));
	if (ret)
		goto set_failed;

	ret = regmap_update_bits(priv->syscon_regmap, offset->prediv, mask->prediv,
				 (syscon_val->prediv << shift->prediv));
	if (ret)
		goto set_failed;

	ret = regmap_update_bits(priv->syscon_regmap, offset->fbdiv, mask->fbdiv,
				 (syscon_val->fbdiv << shift->fbdiv));
	if (ret)
		goto set_failed;

	ret = regmap_update_bits(priv->syscon_regmap, offset->postdiv1, mask->postdiv1,
				 ((syscon_val->postdiv1 >> 1) << shift->postdiv1));
	if (ret)
		goto set_failed;

	/* frac: Integer Mode (Both 1) or Fraction Mode (Both 0) */
	if (syscon_val->dacpd == 0 && syscon_val->dsmpd == 0)
		ret = regmap_update_bits(priv->syscon_regmap, offset->frac, mask->frac,
					 (syscon_val->frac << shift->frac));
	else if (syscon_val->dacpd != syscon_val->dsmpd)
		ret = -EINVAL;

set_failed:
	return ret;
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

	if (data->idx == JH7110_CLK_PLL0_OUT)
		req->rate = jh7110_pll0_syscon_freq[data->freq_select_idx].freq;
	else if (data->idx == JH7110_CLK_PLL1_OUT)
		req->rate = jh7110_pll1_syscon_freq[data->freq_select_idx].freq;
	else
		req->rate = jh7110_pll2_syscon_freq[data->freq_select_idx].freq;

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

/* get offset, mask and shift of PLL(x) syscon */
static int jh7110_pll_data_get(struct jh7110_clk_pll_data *data, int index)
{
	struct jh7110_pll_syscon_offset *offset = &data->offset;
	struct jh7110_pll_syscon_mask *mask = &data->mask;
	struct jh7110_pll_syscon_shift *shift = &data->shift;

	if (index == JH7110_CLK_PLL0_OUT) {
		offset->dacpd = STARFIVE_JH7110_PLL0_DACPD_OFFSET;
		offset->dsmpd = STARFIVE_JH7110_PLL0_DSMPD_OFFSET;
		offset->fbdiv = STARFIVE_JH7110_PLL0_FBDIV_OFFSET;
		offset->frac = STARFIVE_JH7110_PLL0_FRAC_OFFSET;
		offset->prediv = STARFIVE_JH7110_PLL0_PREDIV_OFFSET;
		offset->postdiv1 = STARFIVE_JH7110_PLL0_POSTDIV1_OFFSET;

		mask->dacpd = STARFIVE_JH7110_PLL0_DACPD_MASK;
		mask->dsmpd = STARFIVE_JH7110_PLL0_DSMPD_MASK;
		mask->fbdiv = STARFIVE_JH7110_PLL0_FBDIV_MASK;
		mask->frac = STARFIVE_JH7110_PLL0_FRAC_MASK;
		mask->prediv = STARFIVE_JH7110_PLL0_PREDIV_MASK;
		mask->postdiv1 = STARFIVE_JH7110_PLL0_POSTDIV1_MASK;

		shift->dacpd = STARFIVE_JH7110_PLL0_DACPD_SHIFT;
		shift->dsmpd = STARFIVE_JH7110_PLL0_DSMPD_SHIFT;
		shift->fbdiv = STARFIVE_JH7110_PLL0_FBDIV_SHIFT;
		shift->frac = STARFIVE_JH7110_PLL0_FRAC_SHIFT;
		shift->prediv = STARFIVE_JH7110_PLL0_PREDIV_SHIFT;
		shift->postdiv1 = STARFIVE_JH7110_PLL0_POSTDIV1_SHIFT;

	} else if (index == JH7110_CLK_PLL1_OUT) {
		offset->dacpd = STARFIVE_JH7110_PLL1_DACPD_OFFSET;
		offset->dsmpd = STARFIVE_JH7110_PLL1_DSMPD_OFFSET;
		offset->fbdiv = STARFIVE_JH7110_PLL1_FBDIV_OFFSET;
		offset->frac = STARFIVE_JH7110_PLL1_FRAC_OFFSET;
		offset->prediv = STARFIVE_JH7110_PLL1_PREDIV_OFFSET;
		offset->postdiv1 = STARFIVE_JH7110_PLL1_POSTDIV1_OFFSET;

		mask->dacpd = STARFIVE_JH7110_PLL1_DACPD_MASK;
		mask->dsmpd = STARFIVE_JH7110_PLL1_DSMPD_MASK;
		mask->fbdiv = STARFIVE_JH7110_PLL1_FBDIV_MASK;
		mask->frac = STARFIVE_JH7110_PLL1_FRAC_MASK;
		mask->prediv = STARFIVE_JH7110_PLL1_PREDIV_MASK;
		mask->postdiv1 = STARFIVE_JH7110_PLL1_POSTDIV1_MASK;

		shift->dacpd = STARFIVE_JH7110_PLL1_DACPD_SHIFT;
		shift->dsmpd = STARFIVE_JH7110_PLL1_DSMPD_SHIFT;
		shift->fbdiv = STARFIVE_JH7110_PLL1_FBDIV_SHIFT;
		shift->frac = STARFIVE_JH7110_PLL1_FRAC_SHIFT;
		shift->prediv = STARFIVE_JH7110_PLL1_PREDIV_SHIFT;
		shift->postdiv1 = STARFIVE_JH7110_PLL1_POSTDIV1_SHIFT;

	} else if (index == JH7110_CLK_PLL2_OUT) {
		offset->dacpd = STARFIVE_JH7110_PLL2_DACPD_OFFSET;
		offset->dsmpd = STARFIVE_JH7110_PLL2_DSMPD_OFFSET;
		offset->fbdiv = STARFIVE_JH7110_PLL2_FBDIV_OFFSET;
		offset->frac = STARFIVE_JH7110_PLL2_FRAC_OFFSET;
		offset->prediv = STARFIVE_JH7110_PLL2_PREDIV_OFFSET;
		offset->postdiv1 = STARFIVE_JH7110_PLL2_POSTDIV1_OFFSET;

		mask->dacpd = STARFIVE_JH7110_PLL2_DACPD_MASK;
		mask->dsmpd = STARFIVE_JH7110_PLL2_DSMPD_MASK;
		mask->fbdiv = STARFIVE_JH7110_PLL2_FBDIV_MASK;
		mask->frac = STARFIVE_JH7110_PLL2_FRAC_MASK;
		mask->prediv = STARFIVE_JH7110_PLL2_PREDIV_MASK;
		mask->postdiv1 = STARFIVE_JH7110_PLL2_POSTDIV1_MASK;

		shift->dacpd = STARFIVE_JH7110_PLL2_DACPD_SHIFT;
		shift->dsmpd = STARFIVE_JH7110_PLL2_DSMPD_SHIFT;
		shift->fbdiv = STARFIVE_JH7110_PLL2_FBDIV_SHIFT;
		shift->frac = STARFIVE_JH7110_PLL2_FRAC_SHIFT;
		shift->prediv = STARFIVE_JH7110_PLL2_PREDIV_SHIFT;
		shift->postdiv1 = STARFIVE_JH7110_PLL2_POSTDIV1_SHIFT;

	} else {
		return -ENOENT;
	}

	return 0;
}

static struct clk_hw *jh7110_pll_get(struct of_phandle_args *clkspec, void *data)
{
	struct jh7110_clk_pll_priv *priv = data;
	unsigned int idx = clkspec->args[0];

	if (idx < JH7110_PLLCLK_END)
		return &priv->data[idx].hw;

	return ERR_PTR(-EINVAL);
}

static int jh7110_pll_probe(struct platform_device *pdev)
{
	const char *pll_name[JH7110_PLLCLK_END] = {
		"pll0_out",
		"pll1_out",
		"pll2_out"
	};
	struct jh7110_clk_pll_priv *priv;
	struct jh7110_clk_pll_data *data;
	int ret;
	unsigned int idx;

	priv = devm_kzalloc(&pdev->dev, struct_size(priv, data, JH7110_PLLCLK_END),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;
	priv->syscon_regmap = syscon_node_to_regmap(priv->dev->of_node->parent);
	if (IS_ERR(priv->syscon_regmap))
		return PTR_ERR(priv->syscon_regmap);

	for (idx = 0; idx < JH7110_PLLCLK_END; idx++) {
		struct clk_parent_data parents = {
			.index = 0,
		};
		struct clk_init_data init = {
			.name = pll_name[idx],
			.ops = &jh7110_pll_ops,
			.parent_data = &parents,
			.num_parents = 1,
			.flags = 0,
		};

		data = &priv->data[idx];

		ret = jh7110_pll_data_get(data, idx);
		if (ret)
			return ret;

		data->hw.init = &init;
		data->idx = idx;

		ret = devm_clk_hw_register(&pdev->dev, &data->hw);
		if (ret)
			return ret;
	}

	return devm_of_clk_add_hw_provider(&pdev->dev, jh7110_pll_get, priv);
}

static const struct of_device_id jh7110_pll_match[] = {
	{ .compatible = "starfive,jh7110-pll" },
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
