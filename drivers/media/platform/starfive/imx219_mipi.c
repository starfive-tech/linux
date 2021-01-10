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
#include <linux/gpio.h>
#include <linux/delay.h>
#include <video/stf-vin.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#define SF_MIPI_DEBUG

#ifdef SF_MIPI_DEBUG
	#define MIPI_PRT(format, args...)    printk(KERN_DEBUG "[MIPI]: " format, ## args)
	#define MIPI_INFO(format, args...)   printk(KERN_INFO "[MIPI]: " format, ## args)
	#define MIPI_ERR(format, args...)	 printk(KERN_ERR "[MIPI]: " format, ## args)
#else
	#define MIPI_PRT(x...)  do{} while(0)
	#define MIPI_INFO(x...)  do{} while(0)
	#define MIPI_ERR(x...)  do{} while(0)
#endif

#define MCLK_HZ             24000000UL
#define MCLK_INTERVAL_US   (MCLK_HZ/1000000UL)
#define REGVALLIST(list) (list), sizeof(list)/sizeof((list)[0])
#define IMX_CSI0_RST_PIN        58 //20
#define IMX_CSI1_RST_PIN        57 //21
/* Chip ID */
#define IMX219_REG_CHIP_ID              0x0000
#define IMX219_CHIP_ID                  0x0219

#define IMX219_RESET_PIN_NAME	"imx219-reset-pin"

typedef struct
{
    u16 addr;
    u8 val;
} regval_t;

typedef enum
{
    MODE_1080P_30FPS_4D,
    MODE_1080P_60FPS_4D,
    MODE_1080P_30FPS_2D,
    MODE_4K_30FPS_4D,
} mipicam_mode_t;

static const regval_t S_RASPI_CAM_V2_1080P_30FPS_2D[] =
{
	{0x0103, 0x01},
	{0x30eb, 0x05},
	{0x30eb, 0x0c},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	{0x0162, 0x0d},
	{0x0163, 0x78},
	{0x0164, 0x02},
	{0x0165, 0xa8},
	{0x0166, 0x0a},
	{0x0167, 0x27},
	{0x0168, 0x02},
	{0x0169, 0xb4},
	{0x016a, 0x06},
	{0x016b, 0xeb},
	{0x016c, 0x07},
	{0x016d, 0x80},
	{0x016e, 0x04},
	{0x016f, 0x38},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x02},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x07},
	{0x0625, 0x80},
	{0x0626, 0x04},
	{0x0627, 0x38},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
	{0x0162, 0x0d},
	{0x0163, 0x78},
};


const struct {
		mipicam_mode_t mode;
		const regval_t * regval;
		int regval_num;
} imx219_mode_params[] = {
		{ MODE_1080P_30FPS_2D, REGVALLIST(S_RASPI_CAM_V2_1080P_30FPS_2D) },
	};

struct reg_value {
	u16 u16RegAddr;
	u8 u8Val;
	u8 u8Mask;
	u32 u32Delay_ms;
};

struct imx219 {
	struct i2c_client *i2c_client;
	struct device *dev;

	int reset_pin;
	mipicam_mode_t mode;

	struct mutex lock; /* lock to protect power state, ctrls and mode */
	bool power_on;

	struct gpio_desc *enable_gpio;
};

static int imx219_write_reg(struct i2c_client *client,u16 reg, u8 val)
{
	u8 au8Buf[3] = {0};

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val;

	if (i2c_master_send(client, au8Buf, 3) < 0) {
		pr_err("%s:write reg error:reg=%x,val=%x\n",
			__func__, reg, val);
		return -1;
	}

	mdelay(5);

	return 0;
}

static int imx219_read_reg(struct i2c_client *client,u16 reg, u8 *val)
{
	u8 au8RegBuf[2] = {0};
	u8 u8RdVal = 0;
	int valrtn = 0;

	au8RegBuf[0] = reg >> 8;
	au8RegBuf[1] = reg & 0xff;

    valrtn = i2c_master_send(client, au8RegBuf, 2);
	if (2 != valrtn) {
		pr_err("%s:write reg error:reg=%x,valrtn=%d\n",
				__func__, reg,valrtn);
		return -1;
	}
	
	if (1 != i2c_master_recv(client, &u8RdVal, 1)) {
		pr_err("%s:read reg error:reg=%x,val=%x\n",
				__func__, reg, u8RdVal);
		return -1;
	}

	*val = u8RdVal;

	return u8RdVal;
}

static u16 imx219_cam_v2_read_16(struct i2c_client *client, u16 addr, u16 mask)
{
    u16 val = 0;
    u8 temp;

    imx219_read_reg(client, addr, &temp);
    val = (temp & (mask>>8)) << 8;
    imx219_read_reg(client, addr+1, &temp);
    val |= (temp & (mask&0xff));

    return val;
}


static int imx219_gpio_reserve(int pin, int dir,const char *name)
{
	int ret = -ENODEV;
	if (!gpio_is_valid(pin))
		return ret;

	ret = gpio_request(pin, name);
	if (ret) {
		return ret;
	}

	ret = gpio_direction_output(pin, dir);
	if (ret) {
		gpio_free(pin);
		return ret;
	}

	return 0;
}

static void imx219_gpio_release(int pin, const char *name)
{
	if (gpio_is_valid(pin)) {
		gpio_free(pin);
	}
}
static int imx219_power_down(struct imx219 *imx219)
{
    // software standby
    imx219_write_reg(imx219->i2c_client,0x100, 0);
    
    imx219_gpio_reserve(imx219->reset_pin, 0, IMX219_RESET_PIN_NAME);
    return 0;
}

static int imx219_power_up(struct imx219 *imx219)
{
	int ret;
	u16 chip_id;
	
	/* Power configuration */
	ret = imx219_gpio_reserve(imx219->reset_pin, 0, IMX219_RESET_PIN_NAME);
	if (ret)
		goto disable;

	if (gpio_is_valid(imx219->reset_pin)) {
	    //gpio_direction_output(dev->reset_pin, 1);
	    gpio_set_value(imx219->reset_pin, 1);
	}
	udelay(200 + 32000*MCLK_INTERVAL_US); 

	chip_id = imx219_cam_v2_read_16(imx219->i2c_client, IMX219_REG_CHIP_ID, 0xffff);
	if (chip_id != IMX219_CHIP_ID) {
		MIPI_INFO("raspi_cam_v2 failed to power up!\n");
		return -EINVAL;
	} else {
		MIPI_INFO("raspi_cam_v2 powered up!\n");
	}
    return 0;
disable:
	return ret;
}

static int imx219_set_power_on(struct imx219 *imx219)
{

    return imx219_power_up(imx219);
}

static int imx219_start(struct imx219 *imx219)
{
    int result = imx219_write_reg(imx219->i2c_client,0x100, 1);
    mdelay(200);
    return result;
}

static int imx219_stop(struct imx219 *imx219)
{
    int result = imx219_write_reg(imx219->i2c_client,0x100, 0);
    mdelay(200);
    return result;
}

static int imx219_set_mode(struct imx219 *imx219)
{
	int i, j;
	for (i = 0; i < sizeof(imx219_mode_params)/sizeof(imx219_mode_params[0]); i++) {
		if (imx219->mode == imx219_mode_params[i].mode) {
			for (j = 0; j < imx219_mode_params[i].regval_num; j++) {
				imx219_write_reg(imx219->i2c_client,imx219_mode_params[i].regval[j].addr, imx219_mode_params[i].regval[j].val);
			}
			return 0;
		}
	}
	
	return -EINVAL;
}

int mipicam_set_mode(struct imx219 *imx219)
{
	imx219_stop(imx219);

    return imx219_set_mode(imx219);
}

static void imx219_set_power_down(struct imx219 *imx219)
{
    imx219_stop(imx219);
    imx219_power_down(imx219);
}

/*!
 * ov4689 I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int imx219_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct imx219 *imx219;
	struct device_node *np = client->dev.of_node;
	int ret;
	MIPI_INFO("imx219_probe enter\n");

	imx219 = devm_kzalloc(dev, sizeof(struct imx219), GFP_KERNEL);
	if (!imx219)
		return -ENOMEM;
		
    imx219->i2c_client = client;
	imx219->dev = dev;
	imx219->mode = MODE_1080P_30FPS_2D;
	
	imx219->reset_pin = of_get_named_gpio(np, "reset-gpio", 0);

	i2c_set_clientdata(client, imx219);
	
    ret = imx219_set_power_on(imx219);
	if (ret) {
	   MIPI_ERR("failed to open sensor\n");
	   goto release_gpios;
	}

	if (mipicam_set_mode(imx219)) {
		   MIPI_ERR("mode %d not supported by sensor[i2c_bus 0]\n", imx219->mode);
		   goto EXIT;
	}

	imx219_write_reg(imx219->i2c_client, 0x0157, 0x5f);
	imx219_write_reg(imx219->i2c_client, 0x015a, 0x09);
	imx219_write_reg(imx219->i2c_client, 0x015b, 0x60);
	imx219_write_reg(imx219->i2c_client, 0x0165, 0xa8);
	imx219_write_reg(imx219->i2c_client, 0x0167, 0x27);

    imx219_start(imx219);
    return 0;

   EXIT:
	   /* 8. stop and close sensor */
	   imx219_set_power_down(imx219);
	   return 0;
   release_gpios:
	   imx219_gpio_release(imx219->reset_pin, IMX219_RESET_PIN_NAME);
       return 0;
}

static int imx219_remove(struct i2c_client *client)
{
	struct imx219 *imx219 = i2c_get_clientdata(client);

	imx219_set_power_down(imx219);
	imx219_gpio_release(imx219->reset_pin, IMX219_RESET_PIN_NAME);
	return 0;
}

static const struct i2c_device_id imx219_id[] = {
	{"imx219", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, imx219_id);

static struct i2c_driver imx219_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "imx219_mipi",
		  },
	.probe  = imx219_probe,
	.remove = imx219_remove,
	.id_table = imx219_id,
};

static __init int init_imx219(void)
{ 
    int err;

	err = i2c_add_driver(&imx219_i2c_driver);
    if (err != 0)
		printk("i2c driver registration failed, error=%d\n", err);

	return err;
}

static __exit void exit_imx219(void)
{
	i2c_del_driver(&imx219_i2c_driver);
}

fs_initcall(init_imx219);
module_exit(exit_imx219);

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("imx219 Mipi Camera Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("DVP");
