// SPDX-License-Identifier: GPL-2.0
/*
 * StarFive JH7110 Image-Signal-Process Clock Driver
 *
 * Copyright (C) 2022 StarFive Technology Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include <dt-bindings/clock/starfive,jh7110-crg.h>

#include "clk-starfive-jh71x0.h"

/* external clocks */
#define JH7110_ISPCLK_ISP_TOP_CORE		(JH7110_ISPCLK_END + 0)
#define JH7110_ISPCLK_ISP_TOP_AXI		(JH7110_ISPCLK_END + 1)
#define JH7110_ISPCLK_NOC_BUS_ISP_AXI		(JH7110_ISPCLK_END + 2)
#define JH7110_ISPCLK_DVP_CLK			(JH7110_ISPCLK_END + 3)
#define JH7110_ISPCLK_EXT_END			(JH7110_ISPCLK_END + 4)

/* ISP domian clocks */
struct isp_top_crg {
	struct clk_bulk_data *top_clks;
	int top_clks_num;
	void __iomem *base;
};

static struct clk_bulk_data jh7110_isp_top_clks[] = {
	{ .id = "isp_top_core" },
	{ .id = "isp_top_axi" }
};

static const struct jh71x0_clk_data jh7110_ispclk_data[] = {
	/* syscon */
	JH71X0__DIV(JH7110_ISPCLK_DOM4_APB_FUNC, "dom4_apb_func", 15,
		    JH7110_ISPCLK_ISP_TOP_AXI),
	JH71X0__DIV(JH7110_ISPCLK_MIPI_RX0_PXL, "mipi_rx0_pxl", 8,
		    JH7110_ISPCLK_ISP_TOP_CORE),
	JH71X0__INV(JH7110_ISPCLK_DVP_INV, "dvp_inv", JH7110_ISPCLK_DVP_CLK),
	/* vin */
	JH71X0__DIV(JH7110_ISPCLK_M31DPHY_CFG_IN, "m31dphy_cfg_in", 16,
		    JH7110_ISPCLK_ISP_TOP_CORE),
	JH71X0__DIV(JH7110_ISPCLK_M31DPHY_REF_IN, "m31dphy_ref_in", 16,
		    JH7110_ISPCLK_ISP_TOP_CORE),
	JH71X0__DIV(JH7110_ISPCLK_M31DPHY_TX_ESC_LAN0, "m31dphy_tx_esc_lan0", 60,
		    JH7110_ISPCLK_ISP_TOP_CORE),
	JH71X0_GATE(JH7110_ISPCLK_VIN_ABP, "vin_abp", 0,
		    JH7110_ISPCLK_DOM4_APB_FUNC),
	JH71X0__DIV(JH7110_ISPCLK_VIN_SYS, "vin_sys", 8, JH7110_ISPCLK_ISP_TOP_CORE),
	JH71X0_GATE(JH7110_ISPCLK_VIN_PIXEL_IF0, "vin_pixel_if0", 0,
		    JH7110_ISPCLK_MIPI_RX0_PXL),
	JH71X0_GATE(JH7110_ISPCLK_VIN_PIXEL_IF1, "vin_pixel_if1", 0,
		    JH7110_ISPCLK_MIPI_RX0_PXL),
	JH71X0_GATE(JH7110_ISPCLK_VIN_PIXEL_IF2, "vin_pixel_if2", 0,
		    JH7110_ISPCLK_MIPI_RX0_PXL),
	JH71X0_GATE(JH7110_ISPCLK_VIN_PIXEL_IF3, "vin_pixel_if3", 0,
		    JH7110_ISPCLK_MIPI_RX0_PXL),
	JH71X0__MUX(JH7110_ISPCLK_VIN_P_AXI_WR, "vin_p_axi_wr", 2,
		    JH7110_ISPCLK_MIPI_RX0_PXL,
		    JH7110_ISPCLK_DVP_INV),
	/* ispv2_top_wrapper */
	JH71X0_GMUX(JH7110_ISPCLK_ISPV2_TOP_WRAPPER_C, "ispv2_top_wrapper_c", 0, 2,
		    JH7110_ISPCLK_MIPI_RX0_PXL,
		    JH7110_ISPCLK_DVP_INV),
};

static struct isp_top_crg *top_crg_from(void __iomem **base)
{
	return container_of(base, struct isp_top_crg, base);
}

static inline int jh7110_isp_top_crg_init(struct jh71x0_clk_priv *priv, struct isp_top_crg *top)
{
	struct reset_control *top_rsts;
	int ret;

	top->top_clks = jh7110_isp_top_clks;
	top->top_clks_num = ARRAY_SIZE(jh7110_isp_top_clks);
	ret = devm_clk_bulk_get(priv->dev, top->top_clks_num, top->top_clks);
	if (ret)
		return dev_err_probe(priv->dev, ret, "failed to get top clocks\n");

	/* The resets should be shared and other ISP modules will use its. */
	top_rsts = devm_reset_control_array_get_shared(priv->dev);
	if (IS_ERR(top_rsts))
		return dev_err_probe(priv->dev, PTR_ERR(top_rsts),
				     "failed to get top resets\n");

	ret = clk_bulk_prepare_enable(top->top_clks_num, top->top_clks);
	if (ret)
		return dev_err_probe(priv->dev, ret, "failed to enable top clocks\n");

	return reset_control_deassert(top_rsts);
}

static struct clk_hw *jh7110_ispclk_get(struct of_phandle_args *clkspec, void *data)
{
	struct jh71x0_clk_priv *priv = data;
	unsigned int idx = clkspec->args[0];

	if (idx < JH7110_ISPCLK_END)
		return &priv->reg[idx].hw;

	return ERR_PTR(-EINVAL);
}

static int jh7110_ispcrg_probe(struct platform_device *pdev)
{
	struct jh71x0_clk_priv *priv;
	struct isp_top_crg *top;
	unsigned int idx;
	int ret;

	priv = devm_kzalloc(&pdev->dev,
			    struct_size(priv, reg, JH7110_ISPCLK_END),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	top = devm_kzalloc(&pdev->dev, sizeof(*top), GFP_KERNEL);
	if (!top)
		return -ENOMEM;

	spin_lock_init(&priv->rmw_lock);
	priv->dev = &pdev->dev;
	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	pm_runtime_enable(priv->dev);
	ret = pm_runtime_get_sync(priv->dev);
	if (ret < 0)
		return dev_err_probe(priv->dev, ret, "failed to turn on power\n");

	ret = jh7110_isp_top_crg_init(priv, top);
	if (ret)
		goto err_clk;

	top->base = priv->base;
	dev_set_drvdata(priv->dev, (void *)(&top->base));

	for (idx = 0; idx < JH7110_ISPCLK_END; idx++) {
		u32 max = jh7110_ispclk_data[idx].max;
		struct clk_parent_data parents[4] = {};
		struct clk_init_data init = {
			.name = jh7110_ispclk_data[idx].name,
			.ops = starfive_jh71x0_clk_ops(max),
			.parent_data = parents,
			.num_parents =
				((max & JH71X0_CLK_MUX_MASK) >> JH71X0_CLK_MUX_SHIFT) + 1,
			.flags = jh7110_ispclk_data[idx].flags,
		};
		struct jh71x0_clk *clk = &priv->reg[idx];
		unsigned int i;
		const char *fw_name[JH7110_ISPCLK_EXT_END - JH7110_ISPCLK_END] = {
			"isp_top_core",
			"isp_top_axi",
			"noc_bus_isp_axi",
			"dvp_clk"
		};

		for (i = 0; i < init.num_parents; i++) {
			unsigned int pidx = jh7110_ispclk_data[idx].parents[i];

			if (pidx < JH7110_ISPCLK_END)
				parents[i].hw = &priv->reg[pidx].hw;
			else
				parents[i].fw_name = fw_name[pidx - JH7110_ISPCLK_END];
		}

		clk->hw.init = &init;
		clk->idx = idx;
		clk->max_div = max & JH71X0_CLK_DIV_MASK;

		ret = devm_clk_hw_register(&pdev->dev, &clk->hw);
		if (ret)
			goto err_exit;
	}

	ret = devm_of_clk_add_hw_provider(&pdev->dev, jh7110_ispclk_get, priv);
	if (ret)
		goto err_exit;

	ret = jh7110_reset_controller_register(priv, "reset-isp", 3);
	if (ret)
		goto err_exit;

	return 0;

err_exit:
	clk_bulk_disable_unprepare(top->top_clks_num, top->top_clks);
err_clk:
	pm_runtime_put_sync(priv->dev);
	pm_runtime_disable(priv->dev);
	return ret;
}

static int jh7110_ispcrg_remove(struct platform_device *pdev)
{
	void __iomem **base = dev_get_drvdata(&pdev->dev);
	struct isp_top_crg *top = top_crg_from(base);

	clk_bulk_disable_unprepare(top->top_clks_num, top->top_clks);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id jh7110_ispcrg_match[] = {
	{ .compatible = "starfive,jh7110-ispcrg" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, jh7110_ispcrg_match);

static struct platform_driver jh7110_ispcrg_driver = {
	.probe = jh7110_ispcrg_probe,
	.remove = jh7110_ispcrg_remove,
	.driver = {
		.name = "clk-starfive-jh7110-isp",
		.of_match_table = jh7110_ispcrg_match,
	},
};
module_platform_driver(jh7110_ispcrg_driver);

MODULE_AUTHOR("Xingyu Wu <xingyu.wu@starfivetech.com>");
MODULE_DESCRIPTION("StarFive JH7110 Image-Signal-Process clock driver");
MODULE_LICENSE("GPL");
