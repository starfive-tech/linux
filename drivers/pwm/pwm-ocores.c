// SPDX-License-Identifier: GPL-2.0
/*
 * OpenCores PWM Driver
 *
 * https://opencores.org/projects/ptc
 *
 * Copyright (C) 2018-2023 StarFive Technology Co., Ltd.
 *
 * Limitations:
 * - The hardware only do inverted polarity.
 * - The hardware minimum period / duty_cycle is (1 / pwm_apb clock frequency) ns.
 * - The hardware maximum period / duty_cycle is (U32_MAX / pwm_apb clock frequency) ns.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/reset.h>
#include <linux/slab.h>

/* OCPWM_CTRL register bits*/
#define REG_OCPWM_EN      BIT(0)
#define REG_OCPWM_ECLK    BIT(1)
#define REG_OCPWM_NEC     BIT(2)
#define REG_OCPWM_OE      BIT(3)
#define REG_OCPWM_SIGNLE  BIT(4)
#define REG_OCPWM_INTE    BIT(5)
#define REG_OCPWM_INT     BIT(6)
#define REG_OCPWM_CNTRRST BIT(7)
#define REG_OCPWM_CAPTE   BIT(8)

struct ocores_pwm_device {
	struct pwm_chip chip;
	struct clk *clk;
	struct reset_control *rst;
	const struct ocores_pwm_data *data;
	void __iomem *regs;
	u32 clk_rate; /* PWM APB clock frequency */
};

struct ocores_pwm_data {
	void __iomem *(*get_ch_base)(void __iomem *base, unsigned int channel);
};

static inline u32 ocores_readl(struct ocores_pwm_device *ddata,
			       unsigned int channel,
			       unsigned int offset)
{
	void __iomem *base = ddata->data->get_ch_base ?
			     ddata->data->get_ch_base(ddata->regs, channel) : ddata->regs;

	return readl(base + offset);
}

static inline void ocores_writel(struct ocores_pwm_device *ddata,
				 unsigned int channel,
				 unsigned int offset, u32 val)
{
	void __iomem *base = ddata->data->get_ch_base ?
			     ddata->data->get_ch_base(ddata->regs, channel) : ddata->regs;

	writel(val, base + offset);
}

static inline struct ocores_pwm_device *chip_to_ocores(struct pwm_chip *chip)
{
	return container_of(chip, struct ocores_pwm_device, chip);
}

static void __iomem *starfive_jh71x0_get_ch_base(void __iomem *base,
						 unsigned int channel)
{
	unsigned int offset = (channel > 3 ? 1 << 15 : 0) + (channel & 3) * 0x10;

	return base + offset;
}

static int ocores_pwm_get_state(struct pwm_chip *chip,
				struct pwm_device *pwm,
				struct pwm_state *state)
{
	struct ocores_pwm_device *ddata = chip_to_ocores(chip);
	u32 period_data, duty_data, ctrl_data;

	period_data = ocores_readl(ddata, pwm->hwpwm, 0x8);
	duty_data = ocores_readl(ddata, pwm->hwpwm, 0x4);
	ctrl_data = ocores_readl(ddata, pwm->hwpwm, 0xC);

	state->period = DIV_ROUND_UP_ULL((u64)period_data * NSEC_PER_SEC, ddata->clk_rate);
	state->duty_cycle = DIV_ROUND_UP_ULL((u64)duty_data * NSEC_PER_SEC, ddata->clk_rate);
	state->polarity = PWM_POLARITY_INVERSED;
	state->enabled = (ctrl_data & REG_OCPWM_EN) ? true : false;

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

	ctrl_data = ocores_readl(ddata, pwm->hwpwm, 0xC);
	ocores_writel(ddata, pwm->hwpwm, 0xC, 0);

	period_data = DIV_ROUND_DOWN_ULL(state->period * ddata->clk_rate, NSEC_PER_SEC);
	if (period_data <= U32_MAX)
		ocores_writel(ddata, pwm->hwpwm, 0x8, (u32)period_data);
	else
		return -EINVAL;

	duty_data = DIV_ROUND_DOWN_ULL(state->duty_cycle * ddata->clk_rate, NSEC_PER_SEC);
	if (duty_data <= U32_MAX)
		ocores_writel(ddata, pwm->hwpwm, 0x4, (u32)duty_data);
	else
		return -EINVAL;

	ocores_writel(ddata, pwm->hwpwm, 0xC, 0);

	if (state->enabled) {
		ctrl_data = ocores_readl(ddata, pwm->hwpwm, 0xC);
		ocores_writel(ddata, pwm->hwpwm, 0xC, ctrl_data | REG_OCPWM_EN | REG_OCPWM_OE);
	}

	return 0;
}

static const struct pwm_ops ocores_pwm_ops = {
	.get_state	= ocores_pwm_get_state,
	.apply		= ocores_pwm_apply,
};

static const struct ocores_pwm_data jh7100_pwm_data = {
	.get_ch_base = starfive_jh71x0_get_ch_base,
};

static const struct ocores_pwm_data jh7110_pwm_data = {
	.get_ch_base = starfive_jh71x0_get_ch_base,
};

static const struct of_device_id ocores_pwm_of_match[] = {
	{ .compatible = "opencores,pwm-v1" },
	{ .compatible = "starfive,jh7100-pwm", .data = &jh7100_pwm_data},
	{ .compatible = "starfive,jh7110-pwm", .data = &jh7110_pwm_data},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ocores_pwm_of_match);

static void ocores_reset_control_assert(void *data)
{
	reset_control_assert(data);
}

static int ocores_pwm_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	struct device *dev = &pdev->dev;
	struct ocores_pwm_device *ddata;
	struct pwm_chip *chip;
	int ret;

	id = of_match_device(ocores_pwm_of_match, dev);
	if (!id)
		return -EINVAL;

	ddata = devm_kzalloc(dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;

	ddata->data = id->data;
	chip = &ddata->chip;
	chip->dev = dev;
	chip->ops = &ocores_pwm_ops;
	chip->npwm = 8;
	chip->of_pwm_n_cells = 3;

	ddata->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ddata->regs))
		return dev_err_probe(dev, PTR_ERR(ddata->regs),
				     "Unable to map IO resources\n");

	ddata->clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(ddata->clk))
		return dev_err_probe(dev, PTR_ERR(ddata->clk),
				     "Unable to get pwm's clock\n");

	ddata->rst = devm_reset_control_get_optional_exclusive(dev, NULL);
	if (IS_ERR(ddata->rst))
		return dev_err_probe(dev, PTR_ERR(ddata->rst),
				     "Unable to get pwm's reset\n");

	reset_control_deassert(ddata->rst);

	ret = devm_add_action_or_reset(dev, ocores_reset_control_assert, ddata->rst);
	if (ret)
		return ret;

	ddata->clk_rate = clk_get_rate(ddata->clk);
	if (ddata->clk_rate <= 0)
		return dev_err_probe(dev, ddata->clk_rate,
				     "Unable to get clock's rate\n");

	ret = devm_pwmchip_add(dev, chip);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Could not register PWM chip\n");

	platform_set_drvdata(pdev, ddata);

	return ret;
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
MODULE_DESCRIPTION("OpenCores PWM PTC driver");
MODULE_LICENSE("GPL");
