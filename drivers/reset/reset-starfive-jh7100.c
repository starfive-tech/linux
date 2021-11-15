// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Reset driver for the StarFive JH7100 SoC
 *
 * Copyright (C) 2021 Emil Renner Berthing <kernel@esmil.dk>
 *
 */

#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/reset-controller.h>
#include <linux/spinlock.h>

#include <dt-bindings/reset/starfive-jh7100.h>

/* register offsets */
#define JH7100_RESET_ASSERT0	0x00
#define JH7100_RESET_ASSERT1	0x04
#define JH7100_RESET_ASSERT2	0x08
#define JH7100_RESET_ASSERT3	0x0c
#define JH7100_RESET_STATUS0	0x10
#define JH7100_RESET_STATUS1	0x14
#define JH7100_RESET_STATUS2	0x18
#define JH7100_RESET_STATUS3	0x1c

#define JH7100_AUDIO_RESET_STATUS0	0x4
#define JH7100_ISP_RESET_STATUS0	0x4
#define JH7100_VOUT_RESET_STATUS0	0x4

enum jh7100_reset_ctrl_type {
	PERIPHERAL = 0,
	AUDIO,
	ISP,
};

struct jh7100_reset_data {
	struct reset_controller_dev rcdev;
	spinlock_t lock;
	//void __iomem *base;
	struct regmap *regmap;
};

static inline struct jh7100_reset_data *
jh7100_reset_from(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct jh7100_reset_data, rcdev);
}

static const u32 jh7100_reset_asserted[4] = {
	BIT(JH7100_RST_U74 % 32) |
	BIT(JH7100_RST_VP6_DRESET % 32) |
	BIT(JH7100_RST_VP6_BRESET % 32),

	BIT(JH7100_RST_HIFI4_DRESET % 32) |
	BIT(JH7100_RST_HIFI4_BRESET % 32),

	BIT_MASK(JH7100_RST_E24	% 32)
};

static int jh7100_reset_update(struct reset_controller_dev *rcdev, unsigned long id,
				unsigned int reg_status, bool assert)
{
	struct jh7100_reset_data *data = jh7100_reset_from(rcdev);
	unsigned long bank = id / 32;
	u32 offset = JH7100_RESET_ASSERT0 + 4 * bank;
	u32 status = reg_status + 4 * bank;
	u32 mask = BIT(id % 32);
	u32 done = jh7100_reset_asserted[bank] & mask; //0
	unsigned long flags;
	u32 value = 0;
	u32 tmp = 0;

	if (!assert)
		done ^= mask;// BIT(id % 32)

	spin_lock_irqsave(&data->lock, flags);

	regmap_read(data->regmap, offset, &value);
	if (assert)
		value |= mask;
	else
		value &= ~mask;
	regmap_write(data->regmap, offset, value);

	do {
		regmap_read(data->regmap, status, &tmp);
		value = tmp & mask;
	} while (value != done);

	spin_unlock_irqrestore(&data->lock, flags);
	return 0;
}

static int jh7100_periph_reset_assert(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	dev_dbg(rcdev->dev, "assert(%lu)\n", id);
	return jh7100_reset_update(rcdev, id, JH7100_RESET_STATUS0, true);
}

static int jh7100_periph_reset_deassert(struct reset_controller_dev *rcdev,
				 unsigned long id)
{
	dev_dbg(rcdev->dev, "deassert(%lu)\n", id);
	return jh7100_reset_update(rcdev, id, JH7100_RESET_STATUS0, false);
}

static int jh7100_periph_reset_reset(struct reset_controller_dev *rcdev,
					unsigned long id)
{
	int ret;

	dev_dbg(rcdev->dev, "reset(%lu)\n", id);
	ret = jh7100_periph_reset_assert(rcdev, id);
	if (ret)
		return ret;

	return jh7100_periph_reset_deassert(rcdev, id);
}

static int jh7100_get_reset_status(struct reset_controller_dev *rcdev,
					unsigned int reg_status,
					unsigned long id)
{
	struct jh7100_reset_data *data = jh7100_reset_from(rcdev);
	unsigned long bank = id / 32;
	u32 mask = BIT(id % 32);
	u32 tmp = 0;
	u32 value = 0;

	reg_status = reg_status + 4 * bank;
	regmap_read(data->regmap, reg_status, &tmp);
	value = (tmp ^ jh7100_reset_asserted[bank]) & mask;

	dev_dbg(rcdev->dev, "status(%lu) = %d\n", id, !value);
	return !value;
}

static int jh7100_periph_reset_status(struct reset_controller_dev *rcdev,
					unsigned long id)
{
	return jh7100_get_reset_status(rcdev, JH7100_RESET_STATUS0, id);
}

static const struct reset_control_ops jh7100_periph_reset_ops = {
	.assert		= jh7100_periph_reset_assert,
	.deassert	= jh7100_periph_reset_deassert,
	.reset		= jh7100_periph_reset_reset,
	.status		= jh7100_periph_reset_status,
};

static int jh7100_audio_reset_assert(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	dev_dbg(rcdev->dev, "assert(%lu)\n", id);
	return jh7100_reset_update(rcdev, id, JH7100_AUDIO_RESET_STATUS0, true);
}

static int jh7100_audio_reset_deassert(struct reset_controller_dev *rcdev,
				 unsigned long id)
{
	dev_dbg(rcdev->dev, "deassert(%lu)\n", id);
	return jh7100_reset_update(rcdev, id, JH7100_AUDIO_RESET_STATUS0, false);
}

static int jh7100_audio_reset_reset(struct reset_controller_dev *rcdev,
					unsigned long id)
{
	int ret;

	dev_dbg(rcdev->dev, "reset(%lu)\n", id);
	ret = jh7100_audio_reset_assert(rcdev, id);
	if (ret)
		return ret;

	return jh7100_audio_reset_deassert(rcdev, id);
}

static int jh7100_audio_reset_status(struct reset_controller_dev *rcdev,
					unsigned long id)
{
	return jh7100_get_reset_status(rcdev, JH7100_AUDIO_RESET_STATUS0, id);
}

static const struct reset_control_ops jh7100_audio_reset_ops = {
	.assert		= jh7100_audio_reset_assert,
	.deassert	= jh7100_audio_reset_deassert,
	.reset		= jh7100_audio_reset_reset,
	.status		= jh7100_audio_reset_status,
};

static int jh7100_reset_probe(struct platform_device *pdev)
{
	enum jh7100_reset_ctrl_type type;
	struct jh7100_reset_data *data;
	struct regmap *regmap;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	type = (enum jh7100_reset_ctrl_type)of_device_get_match_data(dev);

	regmap = syscon_node_to_regmap(np);
	if (IS_ERR(regmap)) {
		dev_err(dev, "failed to get reset controller regmap\n");
		return PTR_ERR(regmap);
	}

	data->regmap = regmap;
	data->rcdev.of_node = np;
	data->rcdev.dev = dev;
	data->rcdev.owner = THIS_MODULE;

	if (type == PERIPHERAL) { 
		data->rcdev.ops = &jh7100_periph_reset_ops;
		data->rcdev.nr_resets = JH7100_RSTN_END;
	} else if (type == AUDIO) {
		data->rcdev.ops = &jh7100_audio_reset_ops;
		data->rcdev.nr_resets = JH7100_AUDIO_RSTN_END;
	} else {
		;
	}

	spin_lock_init(&data->lock);

	return devm_reset_controller_register(&pdev->dev, &data->rcdev);
}

static const struct of_device_id jh7100_reset_dt_ids[] = {
	{
		.compatible = "starfive,jh7100-reset",
		.data = (void *)PERIPHERAL,
	},
	{
		.compatible = "starfive,jh7100-reset-audio",
		.data = (void *)AUDIO,
	},
	{ /* sentinel */ },
};

static struct platform_driver jh7100_reset_driver = {
	.probe = jh7100_reset_probe,
	.driver = {
		.name = "jh7100-reset",
		.of_match_table = jh7100_reset_dt_ids,
	},
};
builtin_platform_driver(jh7100_reset_driver);
