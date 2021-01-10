/*
 * Copyright (C) 2011-2013 StarFive Technology Co., Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#define OV5640_CHIP_ID_HIGH_BYTE	0x300A   // max should be 0x56
#define OV5640_CHIP_ID_LOW_BYTE		0x300B   // max should be 0x40

#define OV5640_REG_3820	    0x3820
#define OV5640_REG_3821		0x3821
struct sensor_data {
	struct i2c_client *i2c_client;
};

struct reg_value {
	u16 u16RegAddr;
	u8 u8Val;
	u8 u8Mask;
	u32 u32Delay_ms;
};

/*!
 * Maintains the information on the current state of the sesor.
 */
static struct sensor_data ov5640_data;

static int ov5640_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int ov5640_remove(struct i2c_client *client);

static s32 ov5640_read_reg(u16 reg, u8 *val);
static s32 ov5640_write_reg(u16 reg, u8 val);

static const struct i2c_device_id ov5640_id[] = {
	{"ov5640", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ov5640_id);

static struct i2c_driver ov5640_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "ov564x_dvp",
		  },
	.probe  = ov5640_probe,
	.remove = ov5640_remove,
	.id_table = ov5640_id,
};

static struct reg_value ov5640_init_setting_30fps_VGA[] = {
	{0x3008, 0x42,0, 0}, // software power down
	{0x3103, 0x03,0, 0}, // sysclk from pll
	{0x3017, 0xff,0, 0}, // Frex, Vsync, Href, PCLK, D[9:6] output
	{0x3018, 0xff,0, 0}, // D[5:0], GPIO[1:0] output
	{0x3034, 0x1a,0, 0}, // PLL, MIPI 10-bit
	{0x3037, 0x13,0, 0}, // PLL
	{0x3108, 0x01,0, 0}, // clock divider
	{0x3630, 0x36,0, 0},
	{0x3631, 0x0e,0, 0},
	{0x3632, 0xe2,0, 0},
	{0x3633, 0x12,0, 0},
	{0x3621, 0xe0,0, 0},
	{0x3704, 0xa0,0, 0},
	{0x3703, 0x5a,0, 0},
	{0x3715, 0x78,0, 0},
	{0x3717, 0x01,0, 0},
	{0x370b, 0x60,0, 0},
	{0x3705, 0x1a,0, 0},
	{0x3905, 0x02,0, 0},
	{0x3906, 0x10,0, 0},
	{0x3901, 0x0a,0, 0},
	{0x3731, 0x12,0, 0},
	{0x3600, 0x08,0, 0}, // VCM debug
	{0x3601, 0x33,0, 0}, // VCM debug
	{0x302d, 0x60,0, 0}, // system control
	{0x3620, 0x52,0, 0},
	{0x371b, 0x20,0, 0},
	{0x471c, 0x50,0, 0},
	{0x3a13, 0x43,0, 0}, // pre-gain = 1.05x
	{0x3a18, 0x00,0, 0}, // AEC gain ceiling = 7.75x
	{0x3a19, 0x7c,0, 0}, // AEC gain ceiling
	{0x3635, 0x13,0, 0},
	{0x3636, 0x03,0, 0},
	{0x3634, 0x40,0, 0},
	{0x3622, 0x01,0, 0},
	{0x3c01, 0x34,0, 0}, // sum auto, band counter enable, threshold = 4
	{0x3c04, 0x28,0, 0}, // threshold low sum
	{0x3c05, 0x98,0, 0}, // threshold high sum
	{0x3c06, 0x00,0, 0}, // light meter 1 threshold H
	{0x3c07, 0x07,0, 0}, // light meter 1 threshold L
	{0x3c08, 0x00,0, 0}, // light meter 2 threshold H
	{0x3c09, 0x1c,0, 0}, // light meter 2 threshold L
	{0x3c0a, 0x9c,0, 0}, // sample number H
	{0x3c0b, 0x40,0, 0}, // sample number L
	{0x3810, 0x00,0, 0}, // X offset
	{0x3811, 0x10,0, 0}, // X offset
	{0x3812, 0x00,0, 0}, // Y offset
	{0x3708, 0x64,0, 0},
	{0x4001, 0x02,0, 0}, // BLC start line
	{0x4005, 0x1a,0, 0}, // BLC always update
	{0x3000, 0x00,0, 0}, // enable MCU, OTP
	{0x3004, 0xff,0, 0}, // enable BIST, MCU memory, MCU, OTP, STROBE, D5060, timing, array clock
	{0x300e, 0x58,0, 0}, // MIPI 2 lane? power down PHY HS TX, PHY LP RX, DVP enable
	{0x302e, 0x00,0, 0},
	{0x4300, 0x6f,0, 0}, //rgb565 {0x4300, 0x3f,0, 0}, // YUV 422, YUYV
	{0x501f, 0x01,0, 0}, //rgb565 {0x501f, 0x00,0, 0}, // ISP YUV 422
	{0x440e, 0x00,0, 0},
	{0x5000, 0xa7,0, 0}, // LENC on, raw gamma on, BPC on, WPC on, CIP on
	{0x3a0f, 0x30,0, 0}, // stable in high
	{0x3a10, 0x28,0, 0}, // stable in low
	{0x3a1b, 0x30,0, 0}, // stable out high
	{0x3a1e, 0x26,0, 0}, // stable out low
	{0x3a11, 0x60,0, 0}, // fast zone high
	{0x3a1f, 0x14,0, 0}, // fast zone low
	{0x5800, 0x23,0, 0},
	{0x5801, 0x14,0, 0},
	{0x5802, 0x0f,0, 0},
	{0x5803, 0x0f,0, 0},
	{0x5804, 0x12,0, 0},
	{0x5805, 0x26,0, 0},
	{0x5806, 0x0c,0, 0},
	{0x5807, 0x08,0, 0},
	{0x5808, 0x05,0, 0},
	{0x5809, 0x05,0, 0},
	{0x580a, 0x08,0, 0},
	{0x580b, 0x0d,0, 0},
	{0x580c, 0x08,0, 0},
	{0x580d, 0x03,0, 0},
	{0x580e, 0x00,0, 0},
	{0x580f, 0x00,0, 0},
	{0x5810, 0x03,0, 0},
	{0x5811, 0x09,0, 0},
	{0x5812, 0x07,0, 0},
	{0x5813, 0x03,0, 0},
	{0x5814, 0x00,0, 0},
	{0x5815, 0x01,0, 0},
	{0x5816, 0x03,0, 0},
	{0x5817, 0x08,0, 0},
	{0x5818, 0x0d,0, 0},
	{0x5819, 0x08,0, 0},
	{0x581a, 0x05,0, 0},
	{0x581b, 0x06,0, 0},
	{0x581c, 0x08,0, 0},
	{0x581d, 0x0e,0, 0},
	{0x581e, 0x29,0, 0},
	{0x581f, 0x17,0, 0},
	{0x5820, 0x11,0, 0},
	{0x5821, 0x11,0, 0},
	{0x5822, 0x15,0, 0},
	{0x5823, 0x28,0, 0},
	{0x5824, 0x46,0, 0},
	{0x5825, 0x26,0, 0},
	{0x5826, 0x08,0, 0},
	{0x5827, 0x26,0, 0},
	{0x5828, 0x64,0, 0},
	{0x5829, 0x26,0, 0},
	{0x582a, 0x24,0, 0},
	{0x582b, 0x22,0, 0},
	{0x582c, 0x24,0, 0},
	{0x582d, 0x24,0, 0},
	{0x582e, 0x06,0, 0},
	{0x582f, 0x22,0, 0},
	{0x5830, 0x40,0, 0},
	{0x5831, 0x42,0, 0},
	{0x5832, 0x24,0, 0},
	{0x5833, 0x26,0, 0},
	{0x5834, 0x24,0, 0},
	{0x5835, 0x22,0, 0},
	{0x5836, 0x22,0, 0},
	{0x5837, 0x26,0, 0},
	{0x5838, 0x44,0, 0},
	{0x5839, 0x24,0, 0},
	{0x583a, 0x26,0, 0},
	{0x583b, 0x28,0, 0},
	{0x583c, 0x42,0, 0},
	{0x583d, 0xce,0, 0}, // LENC BR offset
	{0x5180, 0xff,0, 0}, // AWB B block
	{0x5181, 0xf2,0, 0}, // AWB control
	{0x5182, 0x00,0, 0}, // [7:4] max local counter, [3:0] max fast counter
	{0x5183, 0x14,0, 0}, // AWB advance
	{0x5184, 0x25,0, 0},
	{0x5185, 0x24,0, 0},
	{0x5186, 0x09,0, 0},
	{0x5187, 0x09,0, 0},
	{0x5188, 0x09,0, 0},
	{0x5189, 0x75,0, 0},
	{0x518a, 0x54,0, 0},
	{0x518b, 0xe0,0, 0},
	{0x518c, 0xb2,0, 0},
	{0x518d, 0x42,0, 0},
	{0x518e, 0x3d,0, 0},
	{0x518f, 0x56,0, 0},
	{0x5190, 0x46,0, 0},
	{0x5191, 0xf8,0, 0}, // AWB top limit
	{0x5192, 0x04,0, 0}, // AWB botton limit
	{0x5193, 0x70,0, 0}, // Red limit
	{0x5194, 0xf0,0, 0}, // Green Limit
	{0x5195, 0xf0,0, 0}, // Blue limit
	{0x5196, 0x03,0, 0}, // AWB control
	{0x5197, 0x01,0, 0}, // local limit
	{0x5198, 0x04,0, 0},
	{0x5199, 0x12,0, 0},
	{0x519a, 0x04,0, 0},
	{0x519b, 0x00,0, 0},
	{0x519c, 0x06,0, 0},
	{0x519d, 0x82,0, 0},
	{0x519e, 0x38,0, 0}, // AWB control
	{0x5480, 0x01,0, 0}, // BIAS plus on
	{0x5481, 0x08,0, 0},
	{0x5482, 0x14,0, 0},
	{0x5483, 0x28,0, 0},
	{0x5484, 0x51,0, 0},
	{0x5485, 0x65,0, 0},
	{0x5486, 0x71,0, 0},
	{0x5487, 0x7d,0, 0},
	{0x5488, 0x87,0, 0},
	{0x5489, 0x91,0, 0},
	{0x548a, 0x9a,0, 0},
	{0x548b, 0xaa,0, 0},
	{0x548c, 0xb8,0, 0},
	{0x548d, 0xcd,0, 0},
	{0x548e, 0xdd,0, 0},
	{0x548f, 0xea,0, 0},
	{0x5490, 0x1d,0, 0},
	{0x5381, 0x1e,0, 0}, // CMX1 for Y
	{0x5382, 0x5b,0, 0}, // CMX2 for Y
	{0x5383, 0x08,0, 0}, // CMX3 for Y
	{0x5384, 0x0a,0, 0}, // CMX4 for U
	{0x5385, 0x7e,0, 0}, // CMX5 for U
	{0x5386, 0x88,0, 0}, // CMX6 for U
	{0x5387, 0x7c,0, 0}, // CMX7 for V
	{0x5388, 0x6c,0, 0}, // CMX8 for V
	{0x5389, 0x10,0, 0}, // CMX9 for V
	{0x538a, 0x01,0, 0}, // sign[9]
	{0x538b, 0x98,0, 0}, // sign[8:1]
	{0x5580, 0x06,0, 0}, // brightness on, saturation on
	{0x5583, 0x40,0, 0}, // Sat U
	{0x5584, 0x10,0, 0}, // Sat V
	{0x5589, 0x10,0, 0}, // UV adjust th1
	{0x558a, 0x00,0, 0}, // UV adjust th2[8]
	{0x558b, 0xf8,0, 0}, // UV adjust th2[7:0]
	{0x501d, 0x40,0, 0}, // enable manual offset in contrast
	{0x5300, 0x08,0, 0}, // sharpen-MT th1
	{0x5301, 0x30,0, 0}, // sharpen-MT th2
	{0x5302, 0x10,0, 0}, // sharpen-MT off1
	{0x5303, 0x00,0, 0}, // sharpen-MT off2
	{0x5304, 0x08,0, 0}, // De-noise th1
	{0x5305, 0x30,0, 0}, // De-noise th2
	{0x5306, 0x08,0, 0}, // De-noise off1
	{0x5307, 0x16,0, 0}, // De-noise off2
	{0x5309, 0x08,0, 0}, // sharpen-TH th1
	{0x530a, 0x30,0, 0}, // sharpen-TH th2
	{0x530b, 0x04,0, 0}, // sharpen-TH off1
	{0x530c, 0x06,0, 0}, // sharpen-TH off2
	{0x5025, 0x00,0, 0},
	{0x3008, 0x02,0, 0}, // wake up from software power downd
};

static struct reg_value ov5640_setting_30fps_1080P_1920_1080[] = {
	{0x3824, 0x02, 0, 0}, {0x5001, 0x83, 0, 0}, {0x3035, 0x11, 0, 0},
	{0x3036, 0x54, 0, 0}, {0x3c07, 0x07, 0, 0}, {0x3c08, 0x00, 0, 0},
	{0x3c09, 0x1c, 0, 0}, {0x3c0a, 0x9c, 0, 0}, {0x3c0b, 0x40, 0, 0},
	{0x3800, 0x01, 0, 0}, {0x3801, 0x50, 0, 0}, {0x3802, 0x01, 0, 0},
	{0x3803, 0xb2, 0, 0}, {0x3804, 0x08, 0, 0}, {0x3805, 0xef, 0, 0},
	{0x3806, 0x05, 0, 0}, {0x3807, 0xf1, 0, 0}, {0x3808, 0x07, 0, 0},
	{0x3809, 0x80, 0, 0}, {0x380a, 0x04, 0, 0}, {0x380b, 0x38, 0, 0},
	{0x380c, 0x09, 0, 0}, {0x380d, 0xc4, 0, 0}, {0x380e, 0x04, 0, 0},
	{0x380f, 0x60, 0, 0}, {0x3612, 0x2b, 0, 0}, {0x3708, 0x64, 0, 0},
	{0x3a02, 0x04, 0, 0}, {0x3a03, 0x60, 0, 0}, {0x3a08, 0x01, 0, 0},
	{0x3a09, 0x50, 0, 0}, {0x3a0a, 0x01, 0, 0}, {0x3a0b, 0x18, 0, 0},
	{0x3a0e, 0x03, 0, 0}, {0x3a0d, 0x04, 0, 0}, {0x3a14, 0x04, 0, 0},
	{0x3a15, 0x60, 0, 0}, {0x4713, 0x02, 0, 0}, {0x4407, 0x04, 0, 0},
	{0x460b, 0x37, 0, 0}, {0x460c, 0x20, 0, 0}, {0x3824, 0x04, 0, 0},
	{0x4005, 0x1a, 0, 0}, {0x3008, 0x02, 0, 0}, {0x3503, 0, 0, 0},
};

static s32 ov5640_write_reg(u16 reg, u8 val)
{
	u8 au8Buf[3] = {0};

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val;

	if (i2c_master_send(ov5640_data.i2c_client, au8Buf, 3) < 0) {
		pr_err("%s:write reg error:reg=%x,val=%x\n",
			__func__, reg, val);
		return -1;
	}

	return 0;
}

static s32 ov5640_read_reg(u16 reg, u8 *val)
{
	u8 au8RegBuf[2] = {0};
	u8 u8RdVal = 0;

	au8RegBuf[0] = reg >> 8;
	au8RegBuf[1] = reg & 0xff;

	if (2 != i2c_master_send(ov5640_data.i2c_client, au8RegBuf, 2)) {
		pr_err("%s:write reg error:reg=%x\n",
				__func__, reg);
		return -1;
	}

	if (1 != i2c_master_recv(ov5640_data.i2c_client, &u8RdVal, 1)) {
		pr_err("%s:read reg error:reg=%x,val=%x\n",
				__func__, reg, u8RdVal);
		return -1;
	}

	*val = u8RdVal;

	return u8RdVal;
}

/* download ov5640 settings to sensor through i2c */
static int ov5640_download_firmware(struct reg_value *pModeSetting, s32 ArySize)
{
	register u32 Delay_ms = 0;
	register u16 RegAddr = 0;
	register u8 Mask = 0;
	register u8 Val = 0;
	u8 RegVal = 0;
	int i, retval = 0;

	for (i = 0; i < ArySize; ++i, ++pModeSetting) {
		Delay_ms = pModeSetting->u32Delay_ms;
		RegAddr = pModeSetting->u16RegAddr;
		Val = pModeSetting->u8Val;
		Mask = pModeSetting->u8Mask;

		if (Mask) {
			retval = ov5640_read_reg(RegAddr, &RegVal);
			if (retval < 0)
				goto err;

			RegVal &= ~(u8)Mask;
			Val &= Mask;
			Val |= RegVal;
		}

		retval = ov5640_write_reg(RegAddr, Val);
		if (retval < 0)
			goto err;

		if (Delay_ms)
			msleep(Delay_ms);
	}
err:
	return retval;
}

/*!
 * ov5640 I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int ov5640_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int retval, ArySize;
	u8 chip_id_high, chip_id_low;
	u8 reg3820, reg3821;
	struct reg_value *pModeSetting = NULL;

	ov5640_data.i2c_client = client;
	retval = ov5640_read_reg(OV5640_CHIP_ID_HIGH_BYTE, &chip_id_high);
	if (retval < 0 || chip_id_high != 0x56) {
		pr_warn("camera ov5640_dvp is not found\n");
		return -ENODEV;
	}
	retval = ov5640_read_reg(OV5640_CHIP_ID_LOW_BYTE, &chip_id_low);
	if (retval < 0 || chip_id_low != 0x40) {
		pr_warn("camera ov5640_dvp is not found\n");
		return -ENODEV;
	}
	/* initialize dvp sensor */
	pModeSetting = ov5640_init_setting_30fps_VGA;

	retval = ov5640_read_reg(OV5640_REG_3820, &reg3820);
	if (retval < 0 ) {
		pr_warn("OV5640_REG_3820 fail to read\n");
		return -ENODEV;
	}
	ov5640_write_reg(OV5640_REG_3820, reg3820|0x06);

    retval = ov5640_read_reg(OV5640_REG_3821, &reg3821);
	if (retval < 0 ) {
		pr_warn("OV5640_REG_3821 fail to read\n");
		return -ENODEV;
	}
	ov5640_write_reg(OV5640_REG_3821, reg3821&0xf9);

	ArySize = ARRAY_SIZE(ov5640_init_setting_30fps_VGA);
	retval = ov5640_download_firmware(pModeSetting, ArySize);
	/* set resolution to 1920x1080, 30 fps*/
	pModeSetting = ov5640_setting_30fps_1080P_1920_1080;
	ArySize = ARRAY_SIZE(ov5640_setting_30fps_1080P_1920_1080);
	retval = ov5640_download_firmware(pModeSetting, ArySize);

    return 0;
}

/*!
 * ov5640 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int ov5640_remove(struct i2c_client *client)
{
	return 0;
}

//module_i2c_driver(ov5640_i2c_driver);
static __init int init_ov5640(void)
{
    int err;

	err = i2c_add_driver(&ov5640_i2c_driver);
    if (err != 0)
		printk("i2c driver registration failed, error=%d\n", err);

	return err;
}

static __exit void exit_ov5640(void)
{
	i2c_del_driver(&ov5640_i2c_driver);
}

fs_initcall(init_ov5640);
module_exit(exit_ov5640);

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("OV5640 DVP Camera Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("DVP");
