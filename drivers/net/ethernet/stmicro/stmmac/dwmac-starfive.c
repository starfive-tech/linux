// SPDX-License-Identifier: GPL-2.0
/*
 * dwmac-starfive.c - DWMAC glue layer for StarFive SoCs
 *
 * Copyright (C) 2021 Emil Renner Berthing <kernel@esmil.dk>
 */

#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "stmmac.h"
#include "stmmac_platform.h"

#define JH7100_SYSMAIN_REGISTER28 0x70
/* The value below is not a typo, just really bad naming by StarFive ¯\_(ツ)_/¯ */
#define JH7100_SYSMAIN_REGISTER49 0xc8

struct dwmac_starfive {
	struct device *dev;
	struct clk *gtxc;
};

static int dwmac_starfive_jh7100_syscon_init(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct regmap *sysmain;
	u32 gtxclk_dlychain;
	int ret;

	sysmain = syscon_regmap_lookup_by_phandle(np, "starfive,syscon");
	if (IS_ERR(sysmain))
		return dev_err_probe(dev, PTR_ERR(sysmain),
				     "error getting sysmain registers\n");

	/* Choose RGMII interface to the phy.
	 * TODO: support other interfaces once we know the meaning of other
	 * values in the register
	 */
	ret = regmap_update_bits(sysmain, JH7100_SYSMAIN_REGISTER28, 0x7, 1);
	if (ret)
		return dev_err_probe(dev, ret, "error selecting gmac interface\n");

	if (!of_property_read_u32(np, "starfive,gtxclk-dlychain", &gtxclk_dlychain)) {
		ret = regmap_write(sysmain, JH7100_SYSMAIN_REGISTER49, gtxclk_dlychain);
		if (ret)
			return dev_err_probe(dev, ret, "error selecting gtxclk delay chain\n");
	}

	return 0;
}

static void dwmac_starfive_fix_mac_speed(void *data, unsigned int speed)
{
	struct dwmac_starfive *dwmac = data;
	unsigned long rate;
	int ret;

	switch (speed) {
	case SPEED_1000:
		rate = 125000000;
		break;
	case SPEED_100:
		rate = 25000000;
		break;
	case SPEED_10:
		rate = 2500000;
		break;
	default:
		dev_warn(dwmac->dev, "unsupported link speed %u\n", speed);
		return;
	}

	ret = clk_set_rate(dwmac->gtxc, rate);
	if (ret)
		dev_err(dwmac->dev, "error setting gtx clock rate: %d\n", ret);
}

static int dwmac_starfive_probe(struct platform_device *pdev)
{
	struct stmmac_resources stmmac_res;
	struct plat_stmmacenet_data *plat;
	struct dwmac_starfive *dwmac;
	struct clk *txclk;
	int (*syscon_init)(struct device *dev);
	int ret;

	dwmac = devm_kzalloc(&pdev->dev, sizeof(*dwmac), GFP_KERNEL);
	if (!dwmac)
		return -ENOMEM;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	syscon_init = of_device_get_match_data(&pdev->dev);
	if (syscon_init) {
		ret = syscon_init(&pdev->dev);
		if (ret)
			return ret;
	}

	dwmac->gtxc = devm_clk_get_enabled(&pdev->dev, "gtxc");
	if (IS_ERR(dwmac->gtxc))
		return dev_err_probe(&pdev->dev, PTR_ERR(dwmac->gtxc),
				     "error getting/enabling gtxc clock\n");

	txclk = devm_clk_get_enabled(&pdev->dev, "tx");
	if (IS_ERR(txclk))
		return dev_err_probe(&pdev->dev, PTR_ERR(txclk),
				     "error getting/enabling tx clock\n");

	plat = stmmac_probe_config_dt(pdev, stmmac_res.mac);
	if (IS_ERR(plat))
		return dev_err_probe(&pdev->dev, PTR_ERR(plat),
				     "dt configuration failed\n");

	dwmac->dev = &pdev->dev;
	plat->bsp_priv = dwmac;
	plat->fix_mac_speed = dwmac_starfive_fix_mac_speed;

	ret = stmmac_dvr_probe(&pdev->dev, plat, &stmmac_res);
	if (ret) {
		stmmac_remove_config_dt(pdev, plat);
		return ret;
	}

	return 0;
}

static const struct of_device_id dwmac_starfive_match[] = {
	{
		.compatible = "starfive,jh7100-gmac",
		.data = dwmac_starfive_jh7100_syscon_init,
	},
	{
		.compatible = "starfive,jh7110-gmac",
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, dwmac_starfive_match);

static struct platform_driver dwmac_starfive_driver = {
	.probe  = dwmac_starfive_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		.name           = "dwmac-starfive",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = dwmac_starfive_match,
	},
};
module_platform_driver(dwmac_starfive_driver);

MODULE_AUTHOR("Emil Renner Berthing <kernel@esmil.dk>");
MODULE_DESCRIPTION("StarFive DWMAC Glue Layer");
MODULE_LICENSE("GPL");
