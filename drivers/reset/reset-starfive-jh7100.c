// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Reset driver for the StarFive JH7100 SoC
 *
 * Copyright (C) 2021 Emil Renner Berthing <kernel@esmil.dk>
 */

#include <linux/bitmap.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mod_devicetable.h>
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

#if BITS_PER_LONG == 64
#define jh7100_reset_read	readq
#define jh7100_reset_write	writeq
#else
#define jh7100_reset_read	readl
#define jh7100_reset_write	writel
#endif

/*
 * Writing a 1 to the n'th bit of the m'th ASSERT register asserts
 * line 32m + n, and writing a 0 deasserts the same line.
 * Most reset lines have their status inverted so a 0 bit in the STATUS
 * register means the line is asserted and a 1 means it's deasserted. A few
 * lines don't though, so store the expected value of the status registers when
 * all lines are asserted.
 */
static const DECLARE_BITMAP(jh7100_reset_asserted, 4 * 32) = {
	BITMAP_FROM_U64(BIT_ULL_MASK(JH7100_RST_U74) |
			BIT_ULL_MASK(JH7100_RST_VP6_DRESET) |
			BIT_ULL_MASK(JH7100_RST_VP6_BRESET) |
			BIT_ULL_MASK(JH7100_RST_HIFI4_DRESET) |
			BIT_ULL_MASK(JH7100_RST_HIFI4_BRESET)),
	BITMAP_FROM_U64(BIT_ULL_MASK(JH7100_RST_E24)),
};

struct jh7100_reset {
	struct reset_controller_dev rcdev;
	/* protect registers against concurrent read-modify-write */
	spinlock_t lock;
	void __iomem *base;
};

static inline struct jh7100_reset *
jh7100_reset_from(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct jh7100_reset, rcdev);
}

static int jh7100_reset_update(struct reset_controller_dev *rcdev,
			       unsigned long id, bool assert)
{
	struct jh7100_reset *data = jh7100_reset_from(rcdev);
	unsigned long offset = id / BITS_PER_LONG;
	void __iomem *reg_assert = data->base + JH7100_RESET_ASSERT0 + offset * sizeof(long);
	void __iomem *reg_status = data->base + JH7100_RESET_STATUS0 + offset * sizeof(long);
	unsigned long mask = BIT_MASK(id);
	unsigned long done = jh7100_reset_asserted[offset] & mask;
	unsigned long value;
	unsigned long flags;
	int ret;

	if (!assert)
		done ^= mask;

	spin_lock_irqsave(&data->lock, flags);

	value = jh7100_reset_read(reg_assert);
	if (assert)
		value |= mask;
	else
		value &= ~mask;
	jh7100_reset_write(value, reg_assert);

	/* if the associated clock is gated, deasserting might otherwise hang forever */
	ret = readx_poll_timeout_atomic(jh7100_reset_read, reg_status, value,
				        (value & mask) == done, 0, 1000);

	spin_unlock_irqrestore(&data->lock, flags);
	return ret;
}

static int jh7100_reset_assert(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	return jh7100_reset_update(rcdev, id, true);
}

static int jh7100_reset_deassert(struct reset_controller_dev *rcdev,
				 unsigned long id)
{
	return jh7100_reset_update(rcdev, id, false);
}

static int jh7100_reset_reset(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	int ret;

	ret = jh7100_reset_assert(rcdev, id);
	if (ret)
		return ret;

	return jh7100_reset_deassert(rcdev, id);
}

static int jh7100_reset_status(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	struct jh7100_reset *data = jh7100_reset_from(rcdev);
	unsigned long offset = id / BITS_PER_LONG;
	void __iomem *reg_status = data->base + JH7100_RESET_STATUS0 + offset * sizeof(long);
	unsigned long value = jh7100_reset_read(reg_status);
	unsigned long mask = BIT_MASK(id);

	return !((value ^ jh7100_reset_asserted[offset]) & mask);
}

static const struct reset_control_ops jh7100_reset_ops = {
	.assert		= jh7100_reset_assert,
	.deassert	= jh7100_reset_deassert,
	.reset		= jh7100_reset_reset,
	.status		= jh7100_reset_status,
};

static int __init jh7100_reset_probe(struct platform_device *pdev)
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
	{ /* sentinel */ }
};

static struct platform_driver jh7100_reset_driver = {
	.driver = {
		.name = "jh7100-reset",
		.of_match_table = jh7100_reset_dt_ids,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver_probe(jh7100_reset_driver, jh7100_reset_probe);
