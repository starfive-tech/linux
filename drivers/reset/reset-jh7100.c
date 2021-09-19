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

struct jh7100_reset {
	struct reset_controller_dev rcdev;
	spinlock_t lock;
	void __iomem *base;
};

static inline struct jh7100_reset *
jh7100_reset_from(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct jh7100_reset, rcdev);
}

static const u32 jh7100_reset_asserted[4] = {
	BIT(JH7100_RST_U74 % 32) |
	BIT(JH7100_RST_VP6_DRESET % 32) |
	BIT(JH7100_RST_VP6_BRESET % 32),

	BIT(JH7100_RST_HIFI4_DRESET % 32) |
	BIT(JH7100_RST_HIFI4_BRESET % 32),

	BIT_MASK(JH7100_RST_E24	% 32)
};

static int jh7100_reset_update(struct reset_controller_dev *rcdev,
			       unsigned long id, bool assert)
{
	struct jh7100_reset *data = jh7100_reset_from(rcdev);
	unsigned long offset = id / 32;
	void __iomem *reg_assert = data->base + JH7100_RESET_ASSERT0 + 4 * offset;
	void __iomem *reg_status = data->base + JH7100_RESET_STATUS0 + 4 * offset;
	u32 mask = BIT(id % 32);
	u32 done = jh7100_reset_asserted[offset] & mask;
	unsigned long flags;
	u32 value;

	if (!assert)
		done ^= mask;

	spin_lock_irqsave(&data->lock, flags);

	value = readl(reg_assert);
	if (assert)
		value |= mask;
	else
		value &= ~mask;
	writel(value, reg_assert);

	do {
		value = readl(reg_status) & mask;
	} while (value != done);

	spin_unlock_irqrestore(&data->lock, flags);
	return 0;
}

static int jh7100_reset_assert(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	dev_dbg(rcdev->dev, "assert(%lu)\n", id);
	return jh7100_reset_update(rcdev, id, true);
}

static int jh7100_reset_deassert(struct reset_controller_dev *rcdev,
				 unsigned long id)
{
	dev_dbg(rcdev->dev, "deassert(%lu)\n", id);
	return jh7100_reset_update(rcdev, id, false);
}

static int jh7100_reset_reset(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	int ret;

	dev_dbg(rcdev->dev, "reset(%lu)\n", id);
	ret = jh7100_reset_assert(rcdev, id);
	if (ret)
		return ret;

	return jh7100_reset_deassert(rcdev, id);
}

static int jh7100_reset_status(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	struct jh7100_reset *data = jh7100_reset_from(rcdev);
	unsigned long offset = id / 32;
	void __iomem *reg_status = data->base + JH7100_RESET_STATUS0 + 4 * offset;
	u32 mask = BIT(id % 32);
	u32 value = (readl(reg_status) ^ jh7100_reset_asserted[offset]) & mask;

	dev_dbg(rcdev->dev, "status(%lu) = %d\n", id, !value);
	return !value;
}

static const struct reset_control_ops jh7100_reset_ops = {
	.assert		= jh7100_reset_assert,
	.deassert	= jh7100_reset_deassert,
	.reset		= jh7100_reset_reset,
	.status		= jh7100_reset_status,
};

static int jh7100_reset_probe(struct platform_device *pdev)
{
	struct jh7100_reset *data;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	data->rcdev.ops = &jh7100_reset_ops;
	data->rcdev.owner = THIS_MODULE;
	data->rcdev.nr_resets = JH7100_RSTN_END;
	data->rcdev.dev = &pdev->dev;
	data->rcdev.of_node = pdev->dev.of_node;
	spin_lock_init(&data->lock);

	return devm_reset_controller_register(&pdev->dev, &data->rcdev);
}

static const struct of_device_id jh7100_reset_dt_ids[] = {
	{ .compatible = "starfive,jh7100-reset" },
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
