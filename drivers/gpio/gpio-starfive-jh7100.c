// SPDX-License-Identifier: GPL-2.0
/*
 * GPIO driver for StarFive JH7100 SoC
 *
 * Copyright (C) 2020 Shanghai StarFive Technology Co., Ltd.
 */

#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#define GPIO_EN		0x0
#define GPIO_IS_LOW	0x10
#define GPIO_IS_HIGH	0x14
#define GPIO_IBE_LOW	0x18
#define GPIO_IBE_HIGH	0x1c
#define GPIO_IEV_LOW	0x20
#define GPIO_IEV_HIGH	0x24
#define GPIO_IE_LOW	0x28
#define GPIO_IE_HIGH	0x2c
#define GPIO_IC_LOW	0x30
#define GPIO_IC_HIGH	0x34
//read only
#define GPIO_RIS_LOW	0x38
#define GPIO_RIS_HIGH	0x3c
#define GPIO_MIS_LOW	0x40
#define GPIO_MIS_HIGH	0x44
#define GPIO_DIN_LOW	0x48
#define GPIO_DIN_HIGH	0x4c

#define GPIO_DOUT_X_REG	0x50
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

static DEFINE_SPINLOCK(sfg_lock);

static void __iomem *gpio_base;

static int starfive_direction_input(struct gpio_chip *gc, unsigned int offset)
{
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	unsigned long flags;

	if (offset >= gc->ngpio)
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

	if (offset >= gc->ngpio)
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

	if (offset >= gc->ngpio)
		return -EINVAL;

	return readl_relaxed(chip->base + GPIO_DOEN_X_REG + offset * 8) & 0x1;
}

static int starfive_get_value(struct gpio_chip *gc, unsigned int offset)
{
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	int value;

	if (offset >= gc->ngpio)
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

	if (offset >= gc->ngpio)
		return;

	raw_spin_lock_irqsave(&chip->lock, flags);
	writel_relaxed(value, chip->base + GPIO_DOUT_X_REG + offset * 8);
	raw_spin_unlock_irqrestore(&chip->lock, flags);
}

static void starfive_set_ie(struct starfive_gpio *chip, int offset)
{
	unsigned long flags;
	int old_value, new_value;
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
	new_value = old_value | (1 << index);
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

	if (offset < 0 || offset >= gc->ngpio)
		return -EINVAL;

	if (offset < 32) {
		reg_offset = 0;
		index = offset;
	} else {
		reg_offset = 4;
		index = offset - 32;
	}
	switch (trigger) {
	case IRQ_TYPE_LEVEL_HIGH:
		reg_is = readl_relaxed(chip->base + GPIO_IS_LOW + reg_offset);
		reg_ibe = readl_relaxed(chip->base + GPIO_IBE_LOW + reg_offset);
		reg_iev = readl_relaxed(chip->base + GPIO_IEV_LOW + reg_offset);
		reg_is  &= ~(1 << index);
		reg_ibe &= ~(1 << index);
		reg_iev |= (1 << index);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		reg_is = readl_relaxed(chip->base + GPIO_IS_LOW + reg_offset);
		reg_ibe = readl_relaxed(chip->base + GPIO_IBE_LOW + reg_offset);
		reg_iev = readl_relaxed(chip->base + GPIO_IEV_LOW + reg_offset);
		reg_is  &= ~(1 << index);
		reg_ibe &= ~(1 << index);
		reg_iev &= (1 << index);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		reg_is = readl_relaxed(chip->base + GPIO_IS_LOW + reg_offset);
		reg_ibe = readl_relaxed(chip->base + GPIO_IBE_LOW + reg_offset);
		//reg_iev = readl_relaxed(chip->base + GPIO_IEV_LOW + reg_offset);
		reg_is  |= ~(1 << index);
		reg_ibe |= ~(1 << index);
		//reg_iev |= (1 << index);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		//writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		break;
	case IRQ_TYPE_EDGE_RISING:
		reg_is = readl_relaxed(chip->base + GPIO_IS_LOW + reg_offset);
		reg_ibe = readl_relaxed(chip->base + GPIO_IBE_LOW + reg_offset);
		reg_iev = readl_relaxed(chip->base + GPIO_IEV_LOW + reg_offset);
		reg_is  |= ~(1 << index);
		reg_ibe &= ~(1 << index);
		reg_iev |= (1 << index);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		reg_is = readl_relaxed(chip->base + GPIO_IS_LOW + reg_offset);
		reg_ibe = readl_relaxed(chip->base + GPIO_IBE_LOW + reg_offset);
		reg_iev = readl_relaxed(chip->base + GPIO_IEV_LOW + reg_offset);
		reg_is  |= ~(1 << index);
		reg_ibe &= ~(1 << index);
		reg_iev &= (1 << index);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		writel_relaxed(reg_is, chip->base + GPIO_IS_LOW + reg_offset);
		break;
	}

	chip->trigger[offset] = trigger;
	starfive_set_ie(chip, offset);
	return 0;
}

/* chained_irq_{enter,exit} already mask the parent */
static void starfive_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	unsigned int value;
	int offset = irqd_to_hwirq(d);
	int reg_offset, index;

	if (offset < 0 || offset >= gc->ngpio)
		return;

	if (offset < 32) {
		reg_offset = 0;
		index = offset;
	} else {
		reg_offset = 4;
		index = offset - 32;
	}

	value = readl_relaxed(chip->base + GPIO_IE_LOW + reg_offset);
	value &= ~(0x1 << index);
	writel_relaxed(value, chip->base + GPIO_IE_LOW + reg_offset);
}

static void starfive_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct starfive_gpio *chip = gpiochip_get_data(gc);
	unsigned int value;
	int offset = irqd_to_hwirq(d);
	int reg_offset, index;

	if (offset < 0 || offset >= gc->ngpio)
		return;

	if (offset < 32) {
		reg_offset = 0;
		index = offset;
	} else {
		reg_offset = 4;
		index = offset - 32;
	}

	value = readl_relaxed(chip->base + GPIO_IE_LOW + reg_offset);
	value |= (0x1 << index);
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

static irqreturn_t starfive_irq_handler(int irq, void *gc)
{
	int offset;
	int reg_offset, index;
	unsigned int value;
	unsigned long flags;
	struct starfive_gpio *chip = gc;

	for (offset = 0; offset < 64; offset++) {
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

		/* generic_handle_irq(irq_find_mapping(chip->gc.irq.domain, offset)); */
		raw_spin_unlock_irqrestore(&chip->lock, flags);
	}

	return IRQ_HANDLED;
}

void sf_vic_gpio_dout_reverse(int gpio, int en)
{
	unsigned int value;
	int offset;

	if (!gpio_base)
		return;

	offset = gpio * 8 + GPIO_DOUT_X_REG;

	spin_lock(&sfg_lock);
	value = ioread32(gpio_base + offset);
	value &= ~(0x1 << 31);
	value |= (en & 0x1) << 31;
	iowrite32(value, gpio_base + offset);
	spin_unlock(&sfg_lock);
}
EXPORT_SYMBOL_GPL(sf_vic_gpio_dout_reverse);

void sf_vic_gpio_dout_value(int gpio, int v)
{
	unsigned int value;
	int offset;

	if (!gpio_base)
		return;

	offset = gpio * 8 + GPIO_DOUT_X_REG;
	spin_lock(&sfg_lock);
	value = ioread32(gpio_base + offset);
	value &= ~(0xFF);
	value |= (v&0xFF);
	iowrite32(value, gpio_base + offset);
	spin_unlock(&sfg_lock);
}
EXPORT_SYMBOL_GPL(sf_vic_gpio_dout_value);

void sf_vic_gpio_dout_low(int gpio)
{
	sf_vic_gpio_dout_value(gpio, 0);
}
EXPORT_SYMBOL_GPL(sf_vic_gpio_dout_low);

void sf_vic_gpio_dout_high(int gpio)
{
	sf_vic_gpio_dout_value(gpio, 1);
}
EXPORT_SYMBOL_GPL(sf_vic_gpio_dout_high);

void sf_vic_gpio_doen_reverse(int gpio, int en)
{
	unsigned int value;
	int offset;

	if (!gpio_base)
		return;

	offset = gpio * 8 + GPIO_DOEN_X_REG;

	spin_lock(&sfg_lock);
	value = ioread32(gpio_base + offset);
	value &= ~(0x1 << 31);
	value |= (en & 0x1) << 31;
	iowrite32(value, gpio_base + offset);
	spin_unlock(&sfg_lock);
}
EXPORT_SYMBOL_GPL(sf_vic_gpio_doen_reverse);

void sf_vic_gpio_doen_value(int gpio, int v)
{
	unsigned int value;
	int offset;

	if (!gpio_base)
		return;

	offset = gpio * 8 + GPIO_DOEN_X_REG;

	spin_lock(&sfg_lock);
	value = ioread32(gpio_base + offset);
	value &= ~(0xFF);
	value |= (v&0xFF);
	iowrite32(value, gpio_base + offset);
	spin_unlock(&sfg_lock);
}
EXPORT_SYMBOL_GPL(sf_vic_gpio_doen_value);

void sf_vic_gpio_doen_low(int gpio)
{
	sf_vic_gpio_doen_value(gpio, 0);
}
EXPORT_SYMBOL_GPL(sf_vic_gpio_doen_low);

void sf_vic_gpio_doen_high(int gpio)
{
	sf_vic_gpio_doen_value(gpio, 1);
}
EXPORT_SYMBOL_GPL(sf_vic_gpio_doen_high);

void sf_vic_gpio_manual(int offset, int v)
{
	unsigned int value;

	if (!gpio_base)
		return;

	spin_lock(&sfg_lock);
	value = ioread32(gpio_base + offset);
	value &= ~(0xFF);
	value |= (v&0xFF);
	iowrite32(value, gpio_base + offset);
	spin_unlock(&sfg_lock);
}
EXPORT_SYMBOL_GPL(sf_vic_gpio_manual);

static int starfive_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct starfive_gpio *chip;
	struct gpio_irq_chip *girq;
	struct resource *res;
	int irq, ret, ngpio;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	chip->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(chip->base)) {
		dev_err(dev, "failed to allocate device memory\n");
		return PTR_ERR(chip->base);
	}
	gpio_base = chip->base;

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
	chip->gc.ngpio = 64;
	chip->gc.label = dev_name(dev);
	chip->gc.parent = dev;
	chip->gc.owner = THIS_MODULE;

	girq = &chip->gc.irq;
	girq->chip = &starfive_irqchip;
	girq->parent_handler = NULL;
	girq->num_parents = 0;
	girq->parents = NULL;
	girq->default_type = IRQ_TYPE_NONE;
	girq->handler = handle_simple_irq;

	ret = gpiochip_add_data(&chip->gc, chip);
	if (ret) {
		dev_err(dev, "gpiochip_add_data ret=%d!\n", ret);
		return ret;
	}

	/* Disable all GPIO interrupts before enabling parent interrupts */
	iowrite32(0, chip->base + GPIO_IE_HIGH);
	iowrite32(0, chip->base + GPIO_IE_LOW);
	chip->enabled = 0;

	ret = devm_request_irq(dev, irq, starfive_irq_handler, IRQF_SHARED,
			dev_name(dev), chip);
	if (ret) {
		dev_err(dev, "IRQ handler registering failed (%d)\n", ret);
		return ret;
	}

	writel_relaxed(1, chip->base + GPIO_EN);

	dev_info(dev, "StarFive GPIO chip registered %d GPIOs\n", ngpio);

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
