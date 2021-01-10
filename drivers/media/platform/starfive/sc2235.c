/*
 * Copyright (C) 2020 StarFive Technology Co., Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/delay.h>


#define SC2235_CHIP_ID_H	(0x22)
#define SC2235_CHIP_ID_L	(0x35)
#define SC2235_REG_END		0xffff
#define SC2235_REG_DELAY	0xfffe

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/* sc2235 initial register */
static struct regval_list sc2235_init_regs_tbl_1080[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3039, 0x80},
	{0x3621, 0x28},

	{0x3309, 0x60},
	{0x331f, 0x4d},
	{0x3321, 0x4f},
	{0x33b5, 0x10},

	{0x3303, 0x20},
	{0x331e, 0x0d},
	{0x3320, 0x0f},

	{0x3622, 0x02},
	{0x3633, 0x42},
	{0x3634, 0x42},

	{0x3306, 0x66},
	{0x330b, 0xd1},

	{0x3301, 0x0e},

	{0x320c, 0x08},
	{0x320d, 0x98},

	{0x3364, 0x05},		// [2] 1: write at sampling ending

	{0x363c, 0x28},		//bypass nvdd
	{0x363b, 0x0a},		//HVDD
	{0x3635, 0xa0},		//TXVDD

	{0x4500, 0x59},
	{0x3d08, 0x02},
	{0x3908, 0x11},

	{0x363c, 0x08},

	{0x3e03, 0x03},
	{0x3e01, 0x46},

	//0703
	{0x3381, 0x0a},
	{0x3348, 0x09},
	{0x3349, 0x50},
	{0x334a, 0x02},
	{0x334b, 0x60},

	{0x3380, 0x04},
	{0x3340, 0x06},
	{0x3341, 0x50},
	{0x3342, 0x02},
	{0x3343, 0x60},

	//0707

	{0x3632, 0x88},		//anti sm
	{0x3309, 0xa0},
	{0x331f, 0x8d},
	{0x3321, 0x8f},

	{0x335e, 0x01},		//ana dithering
	{0x335f, 0x03},
	{0x337c, 0x04},
	{0x337d, 0x06},
	{0x33a0, 0x05},
	{0x3301, 0x05},

	{0x337f, 0x03},		//new auto precharge  330e in 3372   [7:6] 11: close div_rst 00:open div_rst
	{0x3368, 0x02},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3367, 0x08},
	{0x330e, 0x30},

	{0x3366, 0x7c},		// div_rst gap

	{0x3635, 0xc1},
	{0x363b, 0x09},
	{0x363c, 0x07},

	{0x391e, 0x00},

	{0x3637, 0x14},		//fullwell 7K

	{0x3306, 0x54},
	{0x330b, 0xd8},
	{0x366e, 0x08},		// ofs auto en [3]
	{0x366f, 0x2f},		// ofs+finegain  real ofs in { 0x3687[4:0]

	{0x3631, 0x84},
	{0x3630, 0x48},
	{0x3622, 0x06},

	//ramp by sc
	{0x3638, 0x1f},
	{0x3625, 0x02},
	{0x3636, 0x24},

	//0714
	{0x3348, 0x08},
	{0x3e03, 0x0b},

	//7.17 fpn
	{0x3342, 0x03},
	{0x3343, 0xa0},
	{0x334a, 0x03},
	{0x334b, 0xa0},

	//0718
	{0x3343, 0xb0},
	{0x334b, 0xb0},

	//0720
	//digital ctrl
	{0x3802, 0x01},
	{0x3235, 0x04},
	{0x3236, 0x63},		// vts-2

	//fpn
	{0x3343, 0xd0},
	{0x334b, 0xd0},
	{0x3348, 0x07},
	{0x3349, 0x80},

	//0724
	{0x391b, 0x4d},

	{0x3342, 0x04},
	{0x3343, 0x20},
	{0x334a, 0x04},
	{0x334b, 0x20},

	//0804
	{0x3222, 0x29},
	{0x3901, 0x02},

	//0808

	//digital ctrl
	{0x3f00, 0x07},		// bit[2] = 1
	{0x3f04, 0x08},
	{0x3f05, 0x74},		// hts - { 0x24

	//0809
	{0x330b, 0xc8},

	//0817
	{0x3306, 0x4a},
	{0x330b, 0xca},
	{0x3639, 0x09},

	//manual DPC
	{0x5780, 0xff},
	{0x5781, 0x04},
	{0x5785, 0x18},

	//0822
	{0x3039, 0x35},		//fps
	{0x303a, 0x2e},
	{0x3034, 0x05},
	{0x3035, 0x2a},

	{0x320c, 0x08},
	{0x320d, 0xca},
	{0x320e, 0x04},
	{0x320f, 0xb0},

	{0x3f04, 0x08},
	{0x3f05, 0xa6},		// hts - { 0x24

	{0x3235, 0x04},
	{0x3236, 0xae},		// vts-2

	//0825
	{0x3313, 0x05},
	{0x3678, 0x42},

	//for AE control per frame
	{0x3670, 0x00},
	{0x3633, 0x42},

	{0x3802, 0x00},

	//20180126
	{0x3677, 0x3f},
	{0x3306, 0x44},		//20180126[3c },4a]
	{0x330b, 0xca},		//20180126[c2 },d3]

	//20180202
	{0x3237, 0x08},
	{0x3238, 0x9a},		//hts-0x30

	//20180417
	{0x3640, 0x01},
	{0x3641, 0x02},

	{0x3301, 0x12},		//[8 },15]20180126
	{0x3631, 0x84},
	{0x366f, 0x2f},
	{0x3622, 0xc6},		//20180117
	{0x0100, 0x01},
	//{ 0x4501, 0xc8 },	//bar testing
	//{ 0x3902, 0x45 },
	{SC2235_REG_END, 0x00},
};

int sc2235_read(struct i2c_client *client, uint16_t reg, unsigned char *value)
{
	int ret;
	unsigned char buf[2] = { reg >> 8, reg & 0xff };
	struct i2c_msg msg[2] = {
		[0] = {
		       .addr = client->addr,
		       .flags = 0,
		       .len = 2,
		       .buf = buf,
		       },
		[1] = {
		       .addr = client->addr,
		       .flags = I2C_M_RD,
		       .len = 1,
		       .buf = value,
		       }
	};

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sc2235_write(struct i2c_client *client, uint16_t reg, unsigned char value)
{
	int ret;
	uint8_t buf[3] = { (reg >> 8) & 0xff, reg & 0xff, value };
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 3,
		.buf = buf,
	};

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

/*
static int sc2235_read_array(struct i2c_client *client,
			     struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC2235_REG_END) {
		if (vals->reg_num == SC2235_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sc2235_read(client, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
*/

static int sc2235_write_array(struct i2c_client *client,
			      struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC2235_REG_END) {
		if (vals->reg_num == SC2235_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sc2235_write(client, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}


/*
 *  sc2235 chip detect
 *
 *  @return   0: success, others: fail
 *
 */
static int sc2235_detect(struct i2c_client *client, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc2235_read(client, 0x3107, &v);
	if (ret < 0)
		return ret;
	if (v != SC2235_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc2235_read(client, 0x3108, &v);
	if (ret < 0)
		return ret;

	if (v != SC2235_CHIP_ID_L)
		return -ENODEV;

	*ident = (*ident << 8) | v;
	return 0;
}

static int sc2235_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	unsigned int chipid;
	dev_info(&client->dev, "sc2235_probe enter\n");

	ret = sc2235_detect(client, &chipid);
	if (ret < 0) {
		dev_err(&client->dev, "cannot detect sc2235 chip\n");
		return -ENODEV;
	}

	dev_info(&client->dev, "sc2235 id = 0x%x\n", chipid);

	ret = sc2235_write_array(client, sc2235_init_regs_tbl_1080);
	if (ret < 0) {
		dev_err(&client->dev, " sc2235 init failed\n");
		return -EINVAL;
	}

	return 0;
}

static int sc2235_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id sc2235_id[] = {
	{"sc2235", 0},
	{}
};

static const struct of_device_id sc2235_sensor_dt_ids[] = {
	{.compatible = "sc2235",},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, sc2235_sensor_dt_ids);

static struct i2c_driver sc2235_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "sc2235",
		   .of_match_table = sc2235_sensor_dt_ids,
		   },
	.probe = sc2235_probe,
	.remove = sc2235_remove,
	.id_table = sc2235_id,
};

//module_i2c_driver(sc2235_driver);
static __init int init_sc2235(void)
{
    int err;

	err = i2c_add_driver(&sc2235_driver);
    if (err != 0)
		printk("i2c driver registration failed, error=%d\n", err);

	return err;
}

static __exit void exit_sc2235(void)
{
	i2c_del_driver(&sc2235_driver);
}

fs_initcall(init_sc2235);
module_exit(exit_sc2235);

MODULE_DESCRIPTION("A low-level driver for SmartSens sc2235 sensors");
MODULE_LICENSE("GPL");
