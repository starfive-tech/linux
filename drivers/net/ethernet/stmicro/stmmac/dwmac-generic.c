/*
 * Generic DWMAC platform driver
 *
 * Copyright (C) 2007-2011  STMicroelectronics Ltd
 * Copyright (C) 2015 Joachim Eastwood <manabian@gmail.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "stmmac.h"
#include "stmmac_platform.h"

/*
 * GMAC_GTXCLK 为 gmac 的时钟分频寄存器，低8位为分频值
 * bit         name                    access  default         descript
 * [31]        clk_gmac_gtxclk enable  RW      0x0             "1:enable; 0:disable"
 * [30]        reserved                -       0x0             reserved
 * [29:8]      reserved                -       0x0             reserved
 * [7:0] clk_gmac_gtxclk divide ratio  RW      0x4             divide value
 *
 * gmac 的 root 时钟为500M, gtxclk 需求的时钟如下：
 * 1000M: gtxclk为125M，分频值为500/125 = 0x4
 * 100M:  gtxclk为25M， 分频值为500/25  = 0x14
 * 10M:   gtxclk为2.5M，分频值为500/2.5 = 0xc8
 */
#ifdef CONFIG_SOC_STARFIVE
#define CLKGEN_BASE                    0x11800000
#define CLKGEN_GMAC_GTXCLK_OFFSET      0x1EC
#define CLKGEN_GMAC_GTXCLK_ADDR        (CLKGEN_BASE + CLKGEN_GMAC_GTXCLK_OFFSET)

#define CLKGEN_125M_DIV                0x4
#define CLKGEN_25M_DIV                 0x14
#define CLKGEN_2_5M_DIV                0xc8

static void dwmac_fixed_speed(void *priv, unsigned int speed)
{
	u32 value;
	void *addr = ioremap(CLKGEN_GMAC_GTXCLK_ADDR, sizeof(value));
	if (!addr) {
		pr_err("%s can't remap CLKGEN_GMAC_GTXCLK_ADDR\n", __func__);
		return;
	}

	value = readl(addr) & (~0x000000FF);

	switch (speed) {
		case SPEED_1000: value |= CLKGEN_125M_DIV; break;
		case SPEED_100:  value |= CLKGEN_25M_DIV;  break;
		case SPEED_10:   value |= CLKGEN_2_5M_DIV; break;
		default: iounmap(addr); return;
	}
	writel(value, addr); /*set gmac gtxclk*/
	iounmap(addr);
}
#endif

static int dwmac_generic_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	int ret;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	if (pdev->dev.of_node) {
		plat_dat = stmmac_probe_config_dt(pdev, stmmac_res.mac);
		if (IS_ERR(plat_dat)) {
			dev_err(&pdev->dev, "dt configuration failed\n");
			return PTR_ERR(plat_dat);
		}
	} else {
		plat_dat = dev_get_platdata(&pdev->dev);
		if (!plat_dat) {
			dev_err(&pdev->dev, "no platform data provided\n");
			return  -EINVAL;
		}

		/* Set default value for multicast hash bins */
		plat_dat->multicast_filter_bins = HASH_TABLE_SIZE;

		/* Set default value for unicast filter entries */
		plat_dat->unicast_filter_entries = 1;
	}

	/* Custom initialisation (if needed) */
	if (plat_dat->init) {
		ret = plat_dat->init(pdev, plat_dat->bsp_priv);
		if (ret)
			goto err_remove_config_dt;
	}
#ifdef CONFIG_SOC_STARFIVE
	plat_dat->fix_mac_speed = dwmac_fixed_speed;
#endif

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_exit;

	return 0;

err_exit:
	if (plat_dat->exit)
		plat_dat->exit(pdev, plat_dat->bsp_priv);
err_remove_config_dt:
	if (pdev->dev.of_node)
		stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

static const struct of_device_id dwmac_generic_match[] = {
	{ .compatible = "st,spear600-gmac"},
	{ .compatible = "snps,dwmac-3.40a"},
	{ .compatible = "snps,dwmac-3.50a"},
	{ .compatible = "snps,dwmac-3.610"},
	{ .compatible = "snps,dwmac-3.70a"},
	{ .compatible = "snps,dwmac-3.710"},
	{ .compatible = "snps,dwmac-4.00"},
	{ .compatible = "snps,dwmac-4.10a"},
	{ .compatible = "snps,dwmac"},
	{ .compatible = "snps,dwxgmac-2.10"},
	{ .compatible = "snps,dwxgmac"},
	{ }
};
MODULE_DEVICE_TABLE(of, dwmac_generic_match);

static struct platform_driver dwmac_generic_driver = {
	.probe  = dwmac_generic_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		.name           = STMMAC_RESOURCE_NAME,
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = of_match_ptr(dwmac_generic_match),
	},
};
module_platform_driver(dwmac_generic_driver);

MODULE_DESCRIPTION("Generic dwmac driver");
MODULE_LICENSE("GPL v2");
