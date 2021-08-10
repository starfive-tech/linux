/*
 * starfive DWMAC platform driver
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

struct starfive_priv_data {
	struct device *dev;
	struct clk *clk_ahb;
	struct clk *clk_gtx;
	struct plat_stmmacenet_data *plat_dat;
};

static void starfive_dwmac_fixe_speed(void *priv, unsigned int speed)
{
	struct starfive_priv_data *dwmac = priv;
	unsigned long rate;
	int err;
	
	/*0x118001ec地址为mac的时钟分频寄存器，低8位为分频值
	*mac的root时钟为500M,gtxclk需求的时钟如下：
	*1000M: gtxclk为125M，分频值为500/125=0x4
	*100M: gtxclk为25M，分频值为500/25=0x14
	*10M:gtxclk为2.5M，分频值为500/2.5=0xc8*/

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
		dev_err(dwmac->dev, "invalid speed %u\n", speed);
		return;
	}

	err = clk_set_rate(dwmac->clk_gtx, rate);
	if (err < 0)
		dev_err(dwmac->dev, "failed to set tx rate %lu\n", rate);
}

static int starfive_dwmac_init(struct platform_device *pdev, void *priv)
{
	struct starfive_priv_data *dwmac = priv;
	int ret;

	ret = clk_prepare_enable(dwmac->clk_ahb);
	if (ret) {
		dev_err(&pdev->dev, "gmac ahb clock enable failed\n");
		return ret;
	}

	ret = clk_prepare_enable(dwmac->clk_gtx);
	if (ret) {
		dev_err(&pdev->dev, "gmac tx clock enable failed\n");
		goto clk_tx_en_failed;
	}

	return 0;

clk_tx_en_failed:
	clk_disable_unprepare(dwmac->clk_ahb);
	return ret;
}

static void starfive_dwmac_exit(struct platform_device *pdev, void *priv)
{
	struct starfive_priv_data *dwmac = priv;

	clk_disable_unprepare(dwmac->clk_ahb);
	clk_disable_unprepare(dwmac->clk_gtx);
}

static int starfive_dwmac_parse_dt(struct starfive_priv_data *dwmac, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int err = 0;
	
	dwmac->clk_ahb = devm_clk_get(dev, "gmac_ahb");
	if (IS_ERR(dwmac->clk_ahb)) {
		dev_err(dev, "failed to get gmac ahb clock\n");
		return PTR_ERR(dwmac->clk_ahb);
	}

	dwmac->clk_gtx = devm_clk_get(dev, "gmac_gtx");
	if (IS_ERR(dwmac->clk_gtx)) {
		dev_err(dev, "failed to get gmac tx clock\n");
		return PTR_ERR(dwmac->clk_gtx);
	}

	return err;
}

static int dwmac_starfive_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	struct starfive_priv_data *dwmac;
	int ret;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;
	
	dwmac = devm_kzalloc(&pdev->dev, sizeof(*dwmac), GFP_KERNEL);
	if (!dwmac)
		return -ENOMEM;

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	dwmac->dev = &pdev->dev;
	
	ret = starfive_dwmac_parse_dt(dwmac, &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to parse OF data\n");
		goto err_remove_config_dt;
	}

	plat_dat->init = starfive_dwmac_init;
	plat_dat->exit = starfive_dwmac_exit;
	plat_dat->fix_mac_speed = starfive_dwmac_fixe_speed;
	plat_dat->bsp_priv = dwmac;
	dwmac->plat_dat = plat_dat;

	ret = starfive_dwmac_init(pdev, dwmac);
	if (ret)
		goto err_remove_config_dt;

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_exit;

	return 0;

err_exit:
	starfive_dwmac_exit(pdev, plat_dat->bsp_priv);
err_remove_config_dt:
	if (pdev->dev.of_node)
		stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

static const struct of_device_id dwmac_starfive_match[] = {
	{ .compatible = "snps,dwmac"},
	{ }
};
MODULE_DEVICE_TABLE(of, dwmac_starfive_match);

static struct platform_driver dwmac_starfive_driver = {
	.probe  = dwmac_starfive_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		.name           = STMMAC_RESOURCE_NAME,
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = of_match_ptr(dwmac_starfive_match),
	},
};
module_platform_driver(dwmac_starfive_driver);

MODULE_DESCRIPTION("starfive dwmac driver");
MODULE_LICENSE("GPL v2");
