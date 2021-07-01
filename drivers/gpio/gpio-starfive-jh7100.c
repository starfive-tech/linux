// SPDX-License-Identifier: GPL-2.0
/*
 * GPIO driver for StarFive JH7100 SoC
 *
 * Copyright (C) 2020 Shanghai StarFive Technology Co., Ltd.
 */

#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

/*
 * refer to Section 12. GPIO Registers in JH7100 datasheet:
 * https://github.com/starfive-tech/beaglev_doc
 */

/* global enable */
#define GPIO_EN		0x0

/* interrupt type */
#define GPIO_IS_LOW	0x10
#define GPIO_IS_HIGH	0x14

/* edge trigger interrupt type */
#define GPIO_IBE_LOW	0x18
#define GPIO_IBE_HIGH	0x1c

/* edge trigger interrupt polarity */
#define GPIO_IEV_LOW	0x20
#define GPIO_IEV_HIGH	0x24

/* interrupt max */
#define GPIO_IE_LOW	0x28
#define GPIO_IE_HIGH	0x2c

/* clear edge-triggered interrupt */
#define GPIO_IC_LOW	0x30
#define GPIO_IC_HIGH	0x34

/* edge-triggered interrupt status (read-only) */
#define GPIO_RIS_LOW	0x38
#define GPIO_RIS_HIGH	0x3c

/* interrupt status after masking (read-only) */
#define GPIO_MIS_LOW	0x40
#define GPIO_MIS_HIGH	0x44

/* data value of gpio */
#define GPIO_DIN_LOW	0x48
#define GPIO_DIN_HIGH	0x4c

/* GPIO0_DOUT_CFG is 0x50, GPIOn_DOUT_CFG is 0x50+(n*8) */
#define GPIO_DOUT_X_REG	0x50

/* GPIO0_DOEN_CFG is 0x54, GPIOn_DOEN_CFG is 0x54+(n*8) */
#define GPIO_DOEN_X_REG	0x54

#define MAX_GPIO	 64

struct starfive_gpio {
	raw_spinlock_t		lock;
	void __iomem		*base;
	struct gpio_chip	gc;
	unsigned long		enabled;
	unsigned int		trigger[MAX_GPIO];
	unsigned int		irq_parent[MAX_GPIO];
};

static int starfive_direction_input(struct gpio_chip *gc, unsigned int offset)
{
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	unsigned long flags;

	if (offset >= MAX_GPIO)
		return -EINVAL;

	raw_spin_lock_irqsave(&chip->lock, flags);
	writel_relaxed(0x1, chip->base + GPIO_DOEN_X_REG + offset * 8);
	raw_spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int starfive_direction_output(struct gpio_chip *gc, unsigned int offset, int value)
{
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	unsigned long flags;

	if (offset >= MAX_GPIO)
		return -EINVAL;

	raw_spin_lock_irqsave(&chip->lock, flags);
	writel_relaxed(0x0, chip->base + GPIO_DOEN_X_REG + offset * 8);
	writel_relaxed(value, chip->base + GPIO_DOUT_X_REG + offset * 8);
	raw_spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int starfive_get_direction(struct gpio_chip *gc, unsigned int offset)
{
	struct starfive_gpio *chip = gpiochip_get_data(gc);

	if (offset >= MAX_GPIO)
		return -EINVAL;

	return readl_relaxed(chip->base + GPIO_DOEN_X_REG + offset * 8) & 0x1;
}

static int starfive_get_value(struct gpio_chip *gc, unsigned int offset)
{
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	u32 value;

	if (offset >= MAX_GPIO)
		return -EINVAL;

	if (offset < 32) {
		value = readl_relaxed(chip->base + GPIO_DIN_LOW);
		value = (value >> offset) & 0x1;
	} else {
		value = readl_relaxed(chip->base + GPIO_DIN_HIGH);
		value = (value >> (offset - 32)) & 0x1;
	}

	return value;
}

static void starfive_set_value(struct gpio_chip *gc, unsigned int offset, int value)
{
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	unsigned long flags;

	if (offset >= MAX_GPIO)
		return;

	raw_spin_lock_irqsave(&chip->lock, flags);
	writel_relaxed(value, chip->base + GPIO_DOUT_X_REG + offset * 8);
	raw_spin_unlock_irqrestore(&chip->lock, flags);
}

static void starfive_set_ie(struct starfive_gpio *chip, int offset)
{
	unsigned long flags;
	u32 old_value, new_value;
	int reg_offset, index;

	if (offset < 32) {
		reg_offset = 0;
		index = offset;
	} else {
		reg_offset = 4;
		index = offset - 32;
	}
	raw_spin_lock_irqsave(&chip->lock, flags);
	old_value = readl_relaxed(chip->base + GPIO_IE_LOW + reg_offset);
	new_value = old_value | BIT(index);
	writel_relaxed(new_value, chip->base + GPIO_IE_LOW + reg_offset);
	raw_spin_unlock_irqrestore(&chip->lock, flags);
}

static int starfive_irq_set_type(struct irq_data *d, unsigned int trigger)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	int offset = irqd_to_hwirq(d);
	unsigned int reg_is, reg_ibe, reg_iev;
	int reg_offset, index;

	if (offset < 0 || offset >= MAX_GPIO)
		return -EINVAL;

	if (offset < 32) {
		reg_offset = 0;
		index = offset;
	} else {
		reg_offset = 4;
		index = offset - 32;
	}

	reg_is = readl_relaxed(chip->base + GPIO_IS_LOW + reg_offset);
	reg_ibe = readl_relaxed(chip->base + GPIO_IBE_LOW + reg_offset);
	reg_iev = readl_relaxed(chip->base + GPIO_IEV_LOW + reg_offset);

	switch (trigger) {
	case IRQ_TYPE_LEVEL_HIGH:
		reg_is  &= ~BIT(index);
		reg_ibe &= ~BIT(index);
		reg_iev |= BIT(index);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		reg_is  &= ~BIT(index);
		reg_ibe &= ~BIT(index);
		reg_iev &= ~BIT(index);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		reg_is  |= BIT(index);
		reg_ibe |= BIT(index);
		// no need to set edge type when both
		break;
	case IRQ_TYPE_EDGE_RISING:
		reg_is  |= BIT(index);
		reg_ibe &= ~BIT(index);
		reg_iev |= BIT(index);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		reg_is  |= BIT(index);
		reg_ibe &= ~BIT(index);
		reg_iev &= ~BIT(index);
		break;
	}

	writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
	writel_relaxed(reg_ibe, chip->base + GPIO_IBE_LOW + reg_offset);
	writel_relaxed(reg_iev, chip->base + GPIO_IEV_LOW + reg_offset);
	chip->trigger[offset] = trigger;
	starfive_set_ie(chip, offset);
	return 0;
}

/* chained_irq_{enter,exit} already mask the parent */
static void starfive_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	int offset = irqd_to_hwirq(d);
	int reg_offset, index;
	u32 value;

	if (offset < 0 || offset >= MAX_GPIO)
		return;

	if (offset < 32) {
		reg_offset = 0;
		index = offset;
	} else {
		reg_offset = 4;
		index = offset - 32;
	}

	value = readl_relaxed(chip->base + GPIO_IE_LOW + reg_offset);
	value &= ~BIT(index);
	writel_relaxed(value, chip->base + GPIO_IE_LOW + reg_offset);
}

static void starfive_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	int offset = irqd_to_hwirq(d);
	int reg_offset, index;
	u32 value;

	if (offset < 0 || offset >= MAX_GPIO)
		return;

	if (offset < 32) {
		reg_offset = 0;
		index = offset;
	} else {
		reg_offset = 4;
		index = offset - 32;
	}

	value = readl_relaxed(chip->base + GPIO_IE_LOW + reg_offset);
	value |= BIT(index);
	writel_relaxed(value, chip->base + GPIO_IE_LOW + reg_offset);
}

static void starfive_irq_enable(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	int offset = irqd_to_hwirq(d);

	starfive_irq_unmask(d);
	assign_bit(offset, &chip->enabled, 1);
}

static void starfive_irq_disable(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	int offset = irqd_to_hwirq(d) % MAX_GPIO; // must not fail

	assign_bit(offset, &chip->enabled, 0);
	starfive_set_ie(chip, offset);
}

static struct irq_chip starfive_irqchip = {
	.name		= "starfive-jh7100-gpio",
	.irq_set_type	= starfive_irq_set_type,
	.irq_mask	= starfive_irq_mask,
	.irq_unmask	= starfive_irq_unmask,
	.irq_enable	= starfive_irq_enable,
	.irq_disable	= starfive_irq_disable,
};

static irqreturn_t starfive_irq_handler(int irq, void *data)
{
	struct starfive_gpio *chip = data;
	int offset;

	for (offset = 0; offset < MAX_GPIO; offset++) {
		unsigned long flags;
		int reg_offset, index;
		u32 value;

		if (offset < 32) {
			reg_offset = 0;
			index = offset;
		} else {
			reg_offset = 4;
			index = offset - 32;
		}

		raw_spin_lock_irqsave(&chip->lock, flags);
		value = readl_relaxed(chip->base + GPIO_MIS_LOW + reg_offset);
		if (value & BIT(index))
			writel_relaxed(BIT(index), chip->base + GPIO_IC_LOW +
					reg_offset);
		raw_spin_unlock_irqrestore(&chip->lock, flags);
	}

	return IRQ_HANDLED;
}

static int starfive_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct starfive_gpio *chip;
	struct resource *res;
	int irq, ret;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	chip->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(chip->base)) {
		dev_err(dev, "failed to allocate device memory\n");
		return PTR_ERR(chip->base);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "Cannot get IRQ resource\n");
		return irq;
	}

	raw_spin_lock_init(&chip->lock);
	chip->gc.direction_input = starfive_direction_input;
	chip->gc.direction_output = starfive_direction_output;
	chip->gc.get_direction = starfive_get_direction;
	chip->gc.get = starfive_get_value;
	chip->gc.set = starfive_set_value;
	chip->gc.base = 0;
	chip->gc.ngpio = MAX_GPIO;
	chip->gc.label = dev_name(dev);
	chip->gc.parent = dev;
	chip->gc.owner = THIS_MODULE;

	chip->gc.irq.chip = &starfive_irqchip;
	chip->gc.irq.parent_handler = NULL;
	chip->gc.irq.num_parents = 0;
	chip->gc.irq.parents = NULL;
	chip->gc.irq.default_type = IRQ_TYPE_NONE;
	chip->gc.irq.handler = handle_simple_irq;

	ret = gpiochip_add_data(&chip->gc, chip);
	if (ret) {
		dev_err(dev, "gpiochip_add_data ret=%d!\n", ret);
		return ret;
	}

	/* Disable all GPIO interrupts before enabling parent interrupts */
	writel(0, chip->base + GPIO_IE_LOW);
	writel(0, chip->base + GPIO_IE_HIGH);
	chip->enabled = 0;

	ret = devm_request_irq(dev, irq, starfive_irq_handler, IRQF_SHARED,
			       dev_name(dev), chip);
	if (ret) {
		dev_err(dev, "IRQ handler registering failed (%d)\n", ret);
		return ret;
	}

	writel_relaxed(1, chip->base + GPIO_EN);

	dev_info(dev, "StarFive GPIO chip registered %d GPIOs\n", chip->gc.ngpio);

	return 0;
}

static const struct of_device_id starfive_gpio_match[] = {
	{ .compatible = "starfive,jh7100-gpio", },
	{ /* sentinel */ },
};

static struct platform_driver starfive_gpio_driver = {
	.probe	= starfive_gpio_probe,
	.driver	= {
		.name = "gpio_starfive_jh7100",
		.of_match_table = of_match_ptr(starfive_gpio_match),
	},
};

static int __init starfive_gpio_init(void)
{
	return platform_driver_register(&starfive_gpio_driver);
}
subsys_initcall(starfive_gpio_init);

static void __exit starfive_gpio_exit(void)
{
	platform_driver_unregister(&starfive_gpio_driver);
}
module_exit(starfive_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huan Feng <huan.feng@starfivetech.com>");
MODULE_DESCRIPTION("StarFive JH7100 GPIO driver");
