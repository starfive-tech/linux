/*
 * Copyright (C) 2021 samin.guo <samin.guo@starfivetech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <asm-generic/io.h>
#include <linux/regmap.h>

typedef union
{
	uint32_t v;
	struct {
		uint32_t rstn   : 1; /* TempSensor reset.The RSTN can be de-asserted once the analog core has powered up. Trst(min 100ns)                                0: reset    1: de-assert */
		uint32_t pd     : 1; /* TempSensor analog core power down.the analog core will be power up when PD de-asserted after Tpu(min 50us) has elapsed. the RSTN should be held low until the analog core is powered up.                                0:power up     1: power down */
		uint32_t run    : 1; /* TempSensor start conversion enable
								0:disable    1:enable */
		uint32_t rsvd_0 : 1;
		uint32_t cal    : 1; /* TempSensor calibration mode enable
								0:disable    1:enable */
		uint32_t sgn    : 1; /* TempSensor signature enable,generate a toggle value outputting on DOUT for test purposes.
								0:disable    1:enable */
		uint32_t rsvd_1 : 6;
		uint32_t tm     : 4; /* TempSensor test access control
								0000:normal     0001:Test 1  0010:Test 2  0011:Test 3
								0100:Test4      1000:Test 8  1001:Test 9 */
		uint32_t dout   : 12; /* TempSensor conversion value output
								Temp(c)=DOUT/4094*Y-K */
		uint32_t rsvd_2 : 3;
		uint32_t digo   : 1;  /* TempSensor digital test output */
	} bits;
} sfc_temp_sensor_reg_t;

static uint32_t s_temp_sensor_dout;

struct sfc_temp{
	const char *name;
	void __iomem *regs;
	int irq;
	int clk;
};

static ssize_t sfctmp_get_temp(struct device *dev, struct device_attribute *devattr,char *buf)
{
	long temp,temp_z,temp_x;
	const long Y100 = 23750, K100 = 8110,Z100 = 409400;

	temp  = ((long)s_temp_sensor_dout*100)*Y100/Z100-K100;
	temp_z = temp/100;
	temp_x = temp%100;
	return	sprintf(buf, "%ld.%ld\n", temp_z,temp_x);
}

static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, sfctmp_get_temp, NULL, 0);

static ssize_t sfctmp_temp_show(void)
{
	char buf[10];
	sfctmp_get_temp(NULL, NULL, buf);
	printk("temp(c): %s",buf);
	return 0;
}

static irqreturn_t sfc_temp_isr(int irq, void *priv)
{
	struct sfc_temp *sfc_temp = (struct sfc_temp *)priv;
	sfc_temp_sensor_reg_t reg;

	reg.v = readl(sfc_temp->regs);
	s_temp_sensor_dout = reg.bits.dout;
	return IRQ_HANDLED;
}

static void temp_sensor_power_up(struct sfc_temp *sfc_temp)
{
	sfc_temp_sensor_reg_t init;
	init.v = 0;
	init.bits.pd = 1;
	writel(init.v, sfc_temp->regs);
	udelay(1);

	init.bits.pd = 0;
	writel(init.v, sfc_temp->regs);
	// wait t_pu(50us) + t_rst(100ns)
	udelay(60);

	init.bits.rstn = 1;
	writel(init.v, sfc_temp->regs);
	// wait t_su(500ps)
	udelay(1);

	init.bits.run = 1;
	writel(init.v, sfc_temp->regs);
	// wait 1st sample (8192 temp_sense clk: ~2MHz)
	mdelay(10);
}

static void temp_sensor_power_down(struct sfc_temp *sfc_temp)
{
	sfc_temp_sensor_reg_t init;

	init.v = readl(sfc_temp->regs);
	init.bits.run = 0;
	writel(init.v, sfc_temp->regs);
	udelay(1);

	init.bits.pd   = 1;
	init.bits.rstn = 0;
	writel(init.v, sfc_temp->regs);
	udelay(1);
}

int temp_sensor_deinit(struct sfc_temp *sfc_temp)
{
	temp_sensor_power_down(sfc_temp);
	return 0;
}

static int sfc_temp_probe(struct platform_device *pdev)
{
	struct device *temp_dev = &pdev->dev;
	struct device *hwmon_dev;
	struct resource *mem;
	struct sfc_temp *sfc_temp;
	int ret;

	dev_info(temp_dev,"probe\n");
	sfc_temp = devm_kzalloc(&pdev->dev, sizeof(*sfc_temp), GFP_KERNEL);
	if (!sfc_temp)
		return -ENOMEM;

	temp_dev->driver_data = (void *)sfc_temp;

	sfc_temp->irq = platform_get_irq(pdev, 0);
	if (sfc_temp->irq < 0)
		return sfc_temp->irq;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sfc_temp->regs = devm_ioremap_resource(temp_dev, mem);
	if(IS_ERR(sfc_temp->regs)){
		return PTR_ERR(sfc_temp->regs);
	}

	sfc_temp->name = "sfc_tempsnsor";

	ret = devm_request_irq(temp_dev, sfc_temp->irq, sfc_temp_isr,
			       IRQF_SHARED, sfc_temp->name, sfc_temp);
	if(ret){
		printk("request_irq failed.\n");
		return ret;
	}

	temp_sensor_power_up(sfc_temp);
	sfctmp_temp_show();

	ret = device_create_file(temp_dev, &sensor_dev_attr_temp1_input.dev_attr);
	if (ret){
		return ret;
	}
	hwmon_device_register(temp_dev);
	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static int sfc_temp_remove(struct platform_device *pdev)
{
	struct device *tmp_dev = &pdev->dev;
	struct sfc_temp *sfc_temp = (struct sfc_temp *)tmp_dev->driver_data;
	hwmon_device_unregister(tmp_dev);
	temp_sensor_deinit(sfc_temp);
	return 0;
}

static const struct of_device_id sfc_temp_of_match[] = {
	{ .compatible = "sfc,tempsensor" },
	{ }
};

MODULE_DEVICE_TABLE(of, sfc_temp_of_match);

static struct platform_driver sfc_temp_sensor_driver = {
	.driver = {
		.name	= "sfc_temp_sensor",
		.of_match_table = of_match_ptr(sfc_temp_of_match),
	},
	.probe		= sfc_temp_probe,
	.remove		= sfc_temp_remove,
};
module_platform_driver(sfc_temp_sensor_driver);

MODULE_AUTHOR("samin.guo");
MODULE_DESCRIPTION("SFC temperature sensor driver");
MODULE_LICENSE("GPL");