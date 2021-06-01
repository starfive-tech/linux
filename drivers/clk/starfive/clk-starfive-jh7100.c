// SPDX-License-Identifier: GPL-2.0
/*
 * StarFive JH7100 Clock Generator Driver
 * This is part of the PCR (Power/Clock/Reset) Management Unit Driver
 *
 * FIXME PRELIMINARY
 * For now, all clocks are implemented as fixed-factor clocks relative to osc0
 *
 * TODO Real clock topology, clock register programming
 * PLL0 used for system main logic, including CPU, bus
 * PLL1 output to support DDR, DLA and DSP
 * PLL2 output to support slow speed peripherals, video input and video output
 *
 * Copyright (C) 2021 Glider bv
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/overflow.h>
#include <linux/platform_device.h>

#include <dt-bindings/clock/starfive-jh7100-clkgen.h>

static const struct jh7100_clk {
	const char *name;
	unsigned int mult;
	unsigned int div;
} jh7100_clks[] = {
	[JH7100_CLK_AXI] =	 { "axi",	.mult =  20,	.div =   1 },
	[JH7100_CLK_AHB0] =	 { "ahb0",	.mult =  10,	.div =   1 },
	[JH7100_CLK_AHB2] =	 { "ahb2",	.mult =   5,	.div =   1 },
	[JH7100_CLK_APB1] =	 { "apb1",	.mult =   5,	.div =   1 },
	[JH7100_CLK_APB2] =	 { "apb2",	.mult =   5,	.div =   1 },
	[JH7100_CLK_VPU] =	 { "vpu",	.mult =  16,	.div =   1 },
	[JH7100_CLK_JPU] =	 { "jpu",	.mult =  40,	.div =   3 },
	[JH7100_CLK_PWM] =	 { "pwm",	.mult =   5,	.div =   1 },
	[JH7100_CLK_DWMMC_BIU] = { "dwmmc-biu",	.mult =   4,	.div =   1 },
	[JH7100_CLK_DWMMC_CIU] = { "dwmmc-ciu",	.mult =   4,	.div =   1 },
	[JH7100_CLK_UART] =	 { "uart",	.mult =   4,	.div =   1 },
	[JH7100_CLK_HS_UART] =	 { "hs_uart",	.mult = 297,	.div = 100 },
	[JH7100_CLK_I2C0] =	 { "i2c0",	.mult =  99,	.div =  50 },
	[JH7100_CLK_I2C2] =	 { "i2c2",	.mult =   2,	.div =   1 },
	[JH7100_CLK_QSPI] =	 { "qspi",	.mult =   2,	.div =   1 },
	[JH7100_CLK_SPI] =	 { "spi",	.mult =   2,	.div =   1 },
	[JH7100_CLK_GMAC] =	 { "gmac",	.mult =   1,	.div =   1 },
	[JH7100_CLK_HF] =	 { "hf",	.mult =   1,	.div =   1 },
	[JH7100_CLK_RTC] =	 { "rtc",	.mult =   1,	.div =   4 }
};

struct clk_starfive_jh7100_priv {
	struct clk_onecell_data data;
	void __iomem *base;
	struct clk *clks[];
};

static int __init clk_starfive_jh7100_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	unsigned int nclks = ARRAY_SIZE(jh7100_clks);
	struct clk_starfive_jh7100_priv *priv;
	const char *osc0_name;
	struct clk_hw *hw;
	struct clk *osc0;
	unsigned int i;

	priv = devm_kzalloc(dev, struct_size(priv, clks, nclks), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	osc0 = devm_clk_get(dev, "osc0");
	if (IS_ERR(osc0))
		return PTR_ERR(osc0);

	osc0_name = __clk_get_name(osc0);

	for (i = 0; i < nclks; i++) {
		hw = devm_clk_hw_register_fixed_factor(dev,
			jh7100_clks[i].name, osc0_name, 0, jh7100_clks[i].mult,
			jh7100_clks[i].div);
		if (IS_ERR(hw))
			return PTR_ERR(hw);

		priv->clks[i] = hw->clk;
	}

	priv->data.clks = priv->clks;
	priv->data.clk_num = nclks;

	return of_clk_add_provider(np, of_clk_src_onecell_get, &priv->data);
}

static const struct of_device_id clk_starfive_jh7100_match[] = {
	{
		.compatible = "starfive,jh7100-clkgen",
	},
	{ /* sentinel */ }
};
static struct platform_driver clk_starfive_jh7100_driver = {
	.driver		= {
		.name	= "clk-starfive-jh7100",
		.of_match_table = clk_starfive_jh7100_match,
	},
};

static int __init clk_starfive_jh7100_init(void)
{
	return platform_driver_probe(&clk_starfive_jh7100_driver,
				     clk_starfive_jh7100_probe);
}

subsys_initcall(clk_starfive_jh7100_init);

MODULE_DESCRIPTION("StarFive JH7100 Clock Generator Driver");
MODULE_AUTHOR("Geert Uytterhoeven <geert@linux-m68k.org>");
MODULE_LICENSE("GPL v2");
