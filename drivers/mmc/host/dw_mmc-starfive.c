// SPDX-License-Identifier: GPL-2.0
/*
 * StarFive Designware Mobile Storage Host Controller Driver
 *
 * Copyright (c) 2022 StarFive Technology Co., Ltd.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/mfd/syscon.h>
#include <linux/mmc/host.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"

#define ALL_INT_CLR		0x1ffff
#define MAX_DELAY_CHAIN		32

#define STARFIVE_SMPL_PHASE     GENMASK(20, 16)
#define EVB_SD_SEL1V8_EN_PIN	25

static void dw_mci_starfive_set_ios(struct dw_mci *host, struct mmc_ios *ios)
{
	int ret;
	unsigned int clock;

	if (ios->timing == MMC_TIMING_MMC_DDR52 || ios->timing == MMC_TIMING_UHS_DDR50) {
		clock = (ios->clock > 50000000 && ios->clock <= 52000000) ? 100000000 : ios->clock;
		ret = clk_set_rate(host->ciu_clk, clock);
		if (ret)
			dev_dbg(host->dev, "Use an external frequency divider %uHz\n", ios->clock);
		host->bus_hz = clk_get_rate(host->ciu_clk);
	} else {
		dev_dbg(host->dev, "Using the internal divider\n");
	}
}

static void dw_mci_starfive_set_sample_phase(struct dw_mci *host, u32 smpl_phase)
{
	/* change driver phase and sample phase */
	u32 reg_value = mci_readl(host, UHS_REG_EXT);

	/* In UHS_REG_EXT, only 5 bits valid in DRV_PHASE and SMPL_PHASE */
	reg_value &= ~STARFIVE_SMPL_PHASE;
	reg_value |= FIELD_PREP(STARFIVE_SMPL_PHASE, smpl_phase);
	mci_writel(host, UHS_REG_EXT, reg_value);

	/* We should delay 1ms wait for timing setting finished. */
	mdelay(1);
}

static int dw_mci_starfive_execute_tuning(struct dw_mci_slot *slot,
					     u32 opcode)
{
	static const int grade  = MAX_DELAY_CHAIN;
	struct dw_mci *host = slot->host;
	int smpl_phase, smpl_raise = -1, smpl_fall = -1;
	int ret;

	for (smpl_phase = 0; smpl_phase < grade; smpl_phase++) {
		dw_mci_starfive_set_sample_phase(host, smpl_phase);
		mci_writel(host, RINTSTS, ALL_INT_CLR);

		ret = mmc_send_tuning(slot->mmc, opcode, NULL);

		if (!ret && smpl_raise < 0) {
			smpl_raise = smpl_phase;
		} else if (ret && smpl_raise >= 0) {
			smpl_fall = smpl_phase - 1;
			break;
		}
	}

	if (smpl_phase >= grade)
		smpl_fall = grade - 1;

	if (smpl_raise < 0) {
		smpl_phase = 0;
		dev_err(host->dev, "No valid delay chain! use default\n");
		ret = -EINVAL;
		goto out;
	}

	smpl_phase = (smpl_raise + smpl_fall) / 2;
	dev_dbg(host->dev, "Found valid delay chain! use it [delay=%d]\n", smpl_phase);
	ret = 0;

out:
	dw_mci_starfive_set_sample_phase(host, smpl_phase);
	mci_writel(host, RINTSTS, ALL_INT_CLR);
	return ret;
}

static int dw_mci_starfive_switch_voltage(struct mmc_host *mmc, struct mmc_ios *ios)
{

	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct dw_mci *host = slot->host;
	u32 ret;

	if (device_property_read_bool(host->dev, "board-is-evb")) {
		if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330)
			ret = gpio_direction_output(EVB_SD_SEL1V8_EN_PIN, 0);
		else if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			ret = gpio_direction_output(EVB_SD_SEL1V8_EN_PIN, 1);
		if (ret)
			return ret;
	}

	if (!IS_ERR(mmc->supply.vqmmc)) {
		ret = mmc_regulator_set_vqmmc(mmc, ios);
		if (ret < 0) {
			dev_err(host->dev, "Regulator set error %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static const struct dw_mci_drv_data starfive_data = {
	.common_caps		= MMC_CAP_CMD23,
	.set_ios		= dw_mci_starfive_set_ios,
	.execute_tuning		= dw_mci_starfive_execute_tuning,
	.switch_voltage		= dw_mci_starfive_switch_voltage,
};

static const struct of_device_id dw_mci_starfive_match[] = {
	{ .compatible = "starfive,jh7110-mmc",
		.data = &starfive_data },
	{},
};
MODULE_DEVICE_TABLE(of, dw_mci_starfive_match);

static int dw_mci_starfive_probe(struct platform_device *pdev)
{
	struct gpio_desc *power_gpio;
	int gpio_wl_reg_on = -1;
	int ret;

	if (device_property_read_bool(&pdev->dev, "board-is-devkits")) {
		power_gpio = devm_gpiod_get_optional(&pdev->dev, "power", GPIOD_OUT_LOW);
		if (IS_ERR(power_gpio)) {
			dev_err(&pdev->dev, "Failed to get power-gpio\n");
			return -EINVAL;
		}

		gpiod_set_value_cansleep(power_gpio, 1);

		gpio_wl_reg_on = of_get_named_gpio(pdev->dev.of_node, "gpio_wl_reg_on", 0);
		if (gpio_wl_reg_on >= 0) {
			ret = gpio_request(gpio_wl_reg_on, "WL_REG_ON");
			if (ret < 0) {
				dev_err(&pdev->dev, "gpio_request(%d) for WL_REG_ON failed %d\n",
					gpio_wl_reg_on, ret);
				gpio_wl_reg_on = -1;
				return -EINVAL;
			}
			ret = gpio_direction_output(gpio_wl_reg_on, 0);
			if (ret) {
				dev_err(&pdev->dev, "WL_REG_ON didn't output high\n");
				return -EIO;
			}
			mdelay(10);
			ret = gpio_direction_output(gpio_wl_reg_on, 1);
			if (ret) {
				dev_err(&pdev->dev, "WL_REG_ON didn't output high\n");
				return -EIO;
			}
			mdelay(10);
		}
	}

	return dw_mci_pltfm_register(pdev, &starfive_data);
}

static struct platform_driver dw_mci_starfive_driver = {
	.probe = dw_mci_starfive_probe,
	.remove_new = dw_mci_pltfm_remove,
	.driver = {
		.name = "dwmmc_starfive",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = dw_mci_starfive_match,
	},
};
module_platform_driver(dw_mci_starfive_driver);

MODULE_DESCRIPTION("StarFive JH7110 Specific DW-MSHC Driver Extension");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:dwmmc_starfive");
