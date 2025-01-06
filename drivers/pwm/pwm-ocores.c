// SPDX-License-Identifier: GPL-2.0
/*
 * OpenCores PWM Driver
 *
 * https://opencores.org/projects/ptc
 *
 * Copyright (C) 2018-2023 StarFive Technology Co., Ltd.
 *
 * Limitations:
 * - The hardware only supports inverted polarity.
 * - The hardware minimum period / duty_cycle is (1 / pwm_apb clock frequency).
 * - The hardware maximum period / duty_cycle is (U32_MAX / pwm_apb clock frequency).
 * - The output is set to a low level immediately when disabled.
 * - When configuration changes are done, they get active immediately without resetting
 *   the counter. This might result in one period affected by both old and new settings.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/reset.h>
#include <linux/slab.h>

/* OpenCores Register offsets */
#define REG_OCPWM_CNTR    0x0
#define REG_OCPWM_HRC     0x4
#define REG_OCPWM_LRC     0x8
#define REG_OCPWM_CTRL    0xC

/* OCPWM_CTRL register bits*/
#define REG_OCPWM_CNTR_EN      BIT(0)
#define REG_OCPWM_CNTR_ECLK    BIT(1)
#define REG_OCPWM_CNTR_NEC     BIT(2)
#define REG_OCPWM_CNTR_OE      BIT(3)
#define REG_OCPWM_CNTR_SIGNLE  BIT(4)
#define REG_OCPWM_CNTR_INTE    BIT(5)
#define REG_OCPWM_CNTR_INT     BIT(6)
#define REG_OCPWM_CNTR_RST     BIT(7)
#define REG_OCPWM_CNTR_CAPTE   BIT(8)

struct ocores_pwm_data {
	void __iomem *(*get_ch_base)(void __iomem *base, unsigned int channel);
};

struct ocores_pwm_device {
	const struct ocores_pwm_data *data;
	void __iomem *regs;
	u32 clk_rate; /* PWM APB clock frequency */
};

static inline u32 ocores_pwm_readl(struct ocores_pwm_device *ddata,
				   unsigned int channel,
				   unsigned int offset)
{
	void __iomem *base = ddata->data->get_ch_base ?
			     ddata->data->get_ch_base(ddata->regs, channel) : ddata->regs;

	return readl(base + offset);
}

static inline void ocores_pwm_writel(struct ocores_pwm_device *ddata,
				     unsigned int channel,
				     unsigned int offset, u32 val)
{
	void __iomem *base = ddata->data->get_ch_base ?
			     ddata->data->get_ch_base(ddata->regs, channel) : ddata->regs;

	writel(val, base + offset);
}

static inline struct ocores_pwm_device *chip_to_ocores(struct pwm_chip *chip)
{
	return pwmchip_get_drvdata(chip);
}

static void __iomem *ocores_pwm_get_ch_base(void __iomem *base,
					    unsigned int channel)
{
	unsigned int offset = (channel & 4) << 13 | (channel & 3) << 4;

	return base + offset;
}

static int ocores_pwm_get_state(struct pwm_chip *chip,
				struct pwm_device *pwm,
				struct pwm_state *state)
{
	struct ocores_pwm_device *ddata = chip_to_ocores(chip);
	u32 period_data, duty_data, ctrl_data;

	period_data = ocores_pwm_readl(ddata, pwm->hwpwm, REG_OCPWM_LRC);
	duty_data = ocores_pwm_readl(ddata, pwm->hwpwm, REG_OCPWM_HRC);
	ctrl_data = ocores_pwm_readl(ddata, pwm->hwpwm, REG_OCPWM_CTRL);

	state->period = DIV_ROUND_UP_ULL((u64)period_data * NSEC_PER_SEC, ddata->clk_rate);
	state->duty_cycle = DIV_ROUND_UP_ULL((u64)duty_data * NSEC_PER_SEC, ddata->clk_rate);
	state->polarity = PWM_POLARITY_INVERSED;
	state->enabled = (ctrl_data & REG_OCPWM_CNTR_EN) ? true : false;

	return 0;
}

static int ocores_pwm_apply(struct pwm_chip *chip,
			    struct pwm_device *pwm,
			    const struct pwm_state *state)
{
	struct ocores_pwm_device *ddata = chip_to_ocores(chip);
	u32 ctrl_data = 0;
	u64 period_data, duty_data;

	if (state->polarity != PWM_POLARITY_INVERSED)
		return -EINVAL;

	period_data = mul_u64_u32_div(state->period, ddata->clk_rate, NSEC_PER_SEC);
	if (!period_data)
		return -EINVAL;

	if (period_data > U32_MAX)
		period_data = U32_MAX;

	ocores_pwm_writel(ddata, pwm->hwpwm, REG_OCPWM_LRC, period_data);

	duty_data = mul_u64_u32_div(state->duty_cycle, ddata->clk_rate, NSEC_PER_SEC);
	if (duty_data > U32_MAX)
		duty_data = U32_MAX;

	ocores_pwm_writel(ddata, pwm->hwpwm, REG_OCPWM_HRC, duty_data);

	ctrl_data = ocores_pwm_readl(ddata, pwm->hwpwm, REG_OCPWM_CTRL);
	if (state->enabled)
		ocores_pwm_writel(ddata, pwm->hwpwm, REG_OCPWM_CTRL,
				  ctrl_data | REG_OCPWM_CNTR_EN | REG_OCPWM_CNTR_OE);
	else
		ocores_pwm_writel(ddata, pwm->hwpwm, REG_OCPWM_CTRL,
				  ctrl_data & ~(REG_OCPWM_CNTR_EN | REG_OCPWM_CNTR_OE));

	return 0;
}

static const struct pwm_ops ocores_pwm_ops = {
	.get_state = ocores_pwm_get_state,
	.apply = ocores_pwm_apply,
};

static const struct ocores_pwm_data starfive_pwm_data = {
	.get_ch_base = ocores_pwm_get_ch_base,
};

static const struct of_device_id ocores_pwm_of_match[] = {
	{ .compatible = "opencores,pwm-v1" },
	{ .compatible = "starfive,jh7100-pwm", .data = &starfive_pwm_data},
	{ .compatible = "starfive,jh7110-pwm", .data = &starfive_pwm_data},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ocores_pwm_of_match);

static void ocores_pwm_reset_control_assert(void *data)
{
	reset_control_assert(data);
}

static int ocores_pwm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ocores_pwm_device *ddata;
	struct pwm_chip *chip;
	struct clk *clk;
	struct reset_control *rst;
	int ret;

	chip = devm_pwmchip_alloc(&pdev->dev, 8, sizeof(*ddata));
	if (IS_ERR(chip))
		return -ENOMEM;

	ddata = chip_to_ocores(chip);
	ddata->data = device_get_match_data(&pdev->dev);
	chip->ops = &ocores_pwm_ops;

	ddata->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ddata->regs))
		return dev_err_probe(dev, PTR_ERR(ddata->regs),
				     "Failed to map IO resources\n");

	clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(clk))
		return dev_err_probe(dev, PTR_ERR(clk),
				     "Failed to get pwm's clock\n");

	ret = devm_clk_rate_exclusive_get(dev, clk);
	if (ret)
		return dev_err_probe(dev, ret,
				     "Failed to lock clock rate\n");

	rst = devm_reset_control_get_optional_exclusive(dev, NULL);
	if (IS_ERR(rst))
		return dev_err_probe(dev, PTR_ERR(rst),
				     "Failed to get pwm's reset\n");

	ret = reset_control_deassert(rst);
	if (ret) {
		dev_err(dev, "Failed to deassert pwm's reset\n");
		return ret;
	}

	ret = devm_add_action_or_reset(dev, ocores_pwm_reset_control_assert, rst);
	if (ret) {
		dev_err(dev, "Failed to register assert devm action\n");
		return ret;
	}

	ddata->clk_rate = clk_get_rate(clk);
	if (ddata->clk_rate > NSEC_PER_SEC) {
		dev_err(dev, "Failed to get clock frequency\n");
		return -EINVAL;
	}

	ret = devm_pwmchip_add(dev, chip);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Could not register PWM chip\n");

	return 0;
}

static struct platform_driver ocores_pwm_driver = {
	.probe = ocores_pwm_probe,
	.driver = {
		.name = "ocores-pwm",
		.of_match_table = ocores_pwm_of_match,
	},
};
module_platform_driver(ocores_pwm_driver);

MODULE_AUTHOR("Jieqin Chen");
MODULE_AUTHOR("Hal Feng <hal.feng@starfivetech.com>");
MODULE_DESCRIPTION("OpenCores PTC PWM driver");
MODULE_LICENSE("GPL");
