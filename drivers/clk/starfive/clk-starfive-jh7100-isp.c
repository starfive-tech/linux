// SPDX-License-Identifier: GPL-2.0
/*
 * StarFive JH7100 Isp Clock Driver
 *
 * Copyright (C) 2021 Emil Renner Berthing <kernel@esmil.dk>
 * Copyright (C) 2021 Hal Feng <hal.feng@starfivetech.com>
 */

#include <linux/bits.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include <dt-bindings/clock/starfive-jh7100-isp.h>

#include "clk-starfive-jh7100.h"

/* external clocks */
#define JH7100_ISPCLK_VIN_SRC			(JH7100_ISPCLK_END + 0)
#define JH7100_ISPCLK_ISPSLV_AXI			(JH7100_ISPCLK_END + 1)
#define JH7100_ISPCLK_DVP			(JH7100_ISPCLK_END + 2)

static const struct jh7100_clk_data jh7100_ispclk_data[] = {
	JH7100_GDIV(JH7100_ISPCLK_DPHY_CFGCLK, "dphy_cfgclk", CLK_IGNORE_UNUSED, 0x1f, JH7100_ISPCLK_ISPCORE_2X),	//reg offset: 0x00
	JH7100_GDIV(JH7100_ISPCLK_DPHY_REFCLK, "dphy_refclk", CLK_IGNORE_UNUSED, 0x1f, JH7100_ISPCLK_ISPCORE_2X),
	JH7100_GDIV(JH7100_ISPCLK_DPHY_TXCLKESC, "dphy_txclkesc", CLK_IGNORE_UNUSED, 0x3f, JH7100_ISPCLK_ISPCORE_2X),
	JH7100__DIV(JH7100_ISPCLK_MIPI_RX0_PXL, "mipi_rx0_pxl", 0x1f, JH7100_ISPCLK_ISPCORE_2X),
	JH7100__DIV(JH7100_ISPCLK_MIPI_RX1_PXL, "mipi_rx1_pxl", 0x1f, JH7100_ISPCLK_ISPCORE_2X),	//0x10
	
	JH7100_GATE(JH7100_ISPCLK_MIPI_RX0_PXL_0, "mipi_rx0_pxl_0", CLK_IGNORE_UNUSED, JH7100_ISPCLK_MIPI_RX0_PXL),
	JH7100_GATE(JH7100_ISPCLK_MIPI_RX0_PXL_1, "mipi_rx0_pxl_1", CLK_IGNORE_UNUSED, JH7100_ISPCLK_MIPI_RX0_PXL),
	JH7100_GATE(JH7100_ISPCLK_MIPI_RX0_PXL_2, "mipi_rx0_pxl_2", CLK_IGNORE_UNUSED, JH7100_ISPCLK_MIPI_RX0_PXL),
	JH7100_GATE(JH7100_ISPCLK_MIPI_RX0_PXL_3, "mipi_rx0_pxl_3", CLK_IGNORE_UNUSED, JH7100_ISPCLK_MIPI_RX0_PXL),	//0x20
	JH7100_GDIV(JH7100_ISPCLK_MIPI_RX0_SYS, "mipi_rx0_sys", CLK_IGNORE_UNUSED, 0x1f, JH7100_ISPCLK_ISPCORE_2X),
	
	JH7100_GATE(JH7100_ISPCLK_MIPI_RX1_PXL_0, "mipi_rx1_pxl_0", CLK_IGNORE_UNUSED, JH7100_ISPCLK_MIPI_RX1_PXL),
	JH7100_GATE(JH7100_ISPCLK_MIPI_RX1_PXL_1, "mipi_rx1_pxl_1", CLK_IGNORE_UNUSED, JH7100_ISPCLK_MIPI_RX1_PXL),
	JH7100_GATE(JH7100_ISPCLK_MIPI_RX1_PXL_2, "mipi_rx1_pxl_2", CLK_IGNORE_UNUSED, JH7100_ISPCLK_MIPI_RX1_PXL),	//0x30
	JH7100_GATE(JH7100_ISPCLK_MIPI_RX1_PXL_3, "mipi_rx1_pxl_3", CLK_IGNORE_UNUSED, JH7100_ISPCLK_MIPI_RX1_PXL),
	JH7100_GDIV(JH7100_ISPCLK_MIPI_RX1_SYS, "mipi_rx1_sys", CLK_IGNORE_UNUSED, 0x1f, JH7100_ISPCLK_ISPCORE_2X),

	JH7100_GDIV(JH7100_ISPCLK_ISP0, "isp0", CLK_IGNORE_UNUSED, 0x3, JH7100_ISPCLK_ISPCORE_2X),
	JH7100_GATE(JH7100_ISPCLK_ISP0_2X, "isp0_2x", CLK_IGNORE_UNUSED, JH7100_ISPCLK_ISPCORE_2X),	//0x40
	JH7100_GMUX(JH7100_ISPCLK_ISP0_MIPI, "isp0_mipi", CLK_IGNORE_UNUSED, 2,
		    JH7100_ISPCLK_MIPI_RX0_PXL,
		    JH7100_ISPCLK_MIPI_RX1_PXL,),

	JH7100_GDIV(JH7100_ISPCLK_ISP1, "isp1", CLK_IGNORE_UNUSED, 0x3, JH7100_ISPCLK_ISPCORE_2X),
	JH7100_GATE(JH7100_ISPCLK_ISP1_2X, "isp1_2x", CLK_IGNORE_UNUSED, JH7100_ISPCLK_ISPCORE_2X),
	JH7100_GMUX(JH7100_ISPCLK_ISP1_MIPI, "isp1_mipi", CLK_IGNORE_UNUSED, 2,	//0x50
		    JH7100_ISPCLK_MIPI_RX0_PXL,
		    JH7100_ISPCLK_MIPI_RX1_PXL,),

	JH7100__DIV(JH7100_ISPCLK_DOM4_APB, "dom4_apb", 0x7, JH7100_ISPCLK_ISPSLV_AXI),
	JH7100_GATE(JH7100_ISPCLK_CSI2RX_APB, "csi2rx_apb", CLK_IGNORE_UNUSED, JH7100_ISPCLK_DOM4_APB),
	JH7100__MUX(JH7100_ISPCLK_VIN_AXI_WR, "vin_axi_wr", 3,
		    JH7100_ISPCLK_MIPI_RX0_PXL,
		    JH7100_ISPCLK_MIPI_RX1_PXL,
		    JH7100_ISPCLK_DVP),
	JH7100__MUX(JH7100_ISPCLK_VIN_AXI_RD, "vin_axi_rd", 2,	//0x60
		    JH7100_ISPCLK_MIPI_RX0_PXL,
		    JH7100_ISPCLK_MIPI_RX1_PXL),			
	JH7100__MUX(JH7100_ISPCLK_C_ISP0, "c_isp0", 4,
		    JH7100_ISPCLK_MIPI_RX0_PXL,
		    JH7100_ISPCLK_MIPI_RX1_PXL,
		    JH7100_ISPCLK_DVP,
			JH7100_ISPCLK_ISP0),
	JH7100__MUX(JH7100_ISPCLK_C_ISP1, "c_isp1", 4,
		    JH7100_ISPCLK_MIPI_RX0_PXL,
		    JH7100_ISPCLK_MIPI_RX1_PXL,
		    JH7100_ISPCLK_DVP,
			JH7100_ISPCLK_ISP1),
};

static struct clk_hw *jh7100_ispclk_get(struct of_phandle_args *clkspec, void *data)
{
	struct jh7100_clk_priv *priv = data;
	unsigned int idx = clkspec->args[0];

	if (idx < JH7100_ISPCLK_ISPCORE_2X)
		return &priv->reg[idx].hw;

	if (idx < JH7100_ISPCLK_END)
		return priv->pll[idx - JH7100_ISPCLK_ISPCORE_2X];

	return ERR_PTR(-EINVAL);
}

static int jh7100_ispclk_probe(struct platform_device *pdev)
{
	struct jh7100_clk_priv *priv;
	unsigned int idx;
	int ret;

	priv = devm_kzalloc(&pdev->dev, struct_size(priv, reg, JH7100_ISPCLK_ISPCORE_2X), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	spin_lock_init(&priv->rmw_lock);
	priv->dev = &pdev->dev;
	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	priv->pll[0] = clk_hw_register_fixed_factor(priv->dev, "ispcore_2x",
							 "vin_src", 0, 1, 1);
	if (IS_ERR(priv->pll[0]))
		return PTR_ERR(priv->pll[0]);

	for (idx = 0; idx < JH7100_ISPCLK_ISPCORE_2X; idx++) {
		u32 max = jh7100_ispclk_data[idx].max;
		struct clk_parent_data parents[4] = {};
		struct clk_init_data init = {
			.name = jh7100_ispclk_data[idx].name,
			.ops = starfive_jh7100_clk_ops(max),
			.parent_data = parents,
			.num_parents = ((max & JH7100_CLK_MUX_MASK) >> JH7100_CLK_MUX_SHIFT) + 1,
			.flags = jh7100_ispclk_data[idx].flags,
		};
		struct jh7100_clk *clk = &priv->reg[idx];
		unsigned int i;

		for (i = 0; i < init.num_parents; i++) {
			unsigned int pidx = jh7100_ispclk_data[idx].parents[i];

			if (pidx < JH7100_ISPCLK_ISPCORE_2X)
				parents[i].hw = &priv->reg[pidx].hw;
			else if (pidx < JH7100_ISPCLK_END)
				parents[i].hw = priv->pll[pidx - JH7100_ISPCLK_ISPCORE_2X];
			else if (pidx == JH7100_ISPCLK_VIN_SRC)
				parents[i].fw_name = "vin_src";
			else if (pidx == JH7100_ISPCLK_ISPSLV_AXI)
				parents[i].fw_name = "ispslv_axi";
		}

		clk->hw.init = &init;
		clk->idx = idx;
		clk->max_div = max & JH7100_CLK_DIV_MASK;

		ret = devm_clk_hw_register(priv->dev, &clk->hw);
		if (ret)
			return ret;
	}

	return devm_of_clk_add_hw_provider(priv->dev, jh7100_ispclk_get, priv);
}

static const struct of_device_id jh7100_ispclk_match[] = {
	{ .compatible = "starfive,jh7100-ispclk" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, jh7100_ispclk_match);

static struct platform_driver jh7100_ispclk_driver = {
	.probe = jh7100_ispclk_probe,
	.driver = {
		.name = "clk-starfive-jh7100-isp",
		.of_match_table = jh7100_ispclk_match,
	},
};
module_platform_driver(jh7100_ispclk_driver);

MODULE_AUTHOR("Emil Renner Berthing");
MODULE_AUTHOR("Hal Feng");
MODULE_DESCRIPTION("StarFive JH7100 isp clock driver");
MODULE_LICENSE("GPL");
