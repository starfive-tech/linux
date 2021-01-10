/*
 ******************************************************************************
 * @file  adv7513.c
 * @author  StarFive Technology
 * @version  V1.0
 * @date  09/21/2020
 * @brief
 ******************************************************************************
 * @copy
 *
 * THE PRESENT SOFTWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STARFIVE SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * <h2><center>&copy; COPYRIGHT 2020 Shanghai StarFive Technology Co., Ltd. </center></h2>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "adv7513.h"

static int adv7513_write(struct i2c_client *client, u16 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	buf[0] = reg;
	buf[1] = val;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.buf = buf;
	msg.len = 2;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		return 0;

	dev_err(&client->dev,
		"adv7513 write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

static int adv7513_read(struct i2c_client *client, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	buf[0] = reg;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = buf;
	msg[0].len = 1;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = val;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret >= 0) {
		return 0;
	}

	dev_err(&client->dev,
		"adv7513 read reg(0x%x val:0x%x) failed,ret = %d !\n", reg, *val, ret);

	return ret;
}

/*============================================================================
 * Read up to 8-bit field from a single 8-bit register
 *              ________
 * Example     |___***__|  Mask = 0x1C     BitPos = 2
 *
 *
 * Entry:   DevAddr = Device Address
 *          RegAddr = 8-bit register address
 *          Mask    = Field mask
 *          BitPos  = Field LSBit position in the register (0-7)
 *
 * Return:  Field value in the LSBits of the return value
 *
 *===========================================================================*/
static u8 adv7513_I2CReadField8 (struct i2c_client *client, u8 RegAddr, u8 Mask,
                         u8 BitPos)
{
    u8 data;

	adv7513_read(client, RegAddr, &data);
	return (data&Mask)>>BitPos;
}

/*============================================================================
 * Write up to 8-bit field to a single 8-bit register
 *              ________
 * Example     |___****_|  Mask = 0x1E     BitPos = 1
 *
 * Entry:   DevAddr = Device Address
 *          RegAddr = 8-bit register address
 *          Mask    = Field mask
 *          BitPos  = Field LSBit position in the register (0-7)
 *                    Set to 0 if FieldVal is in correct position of the reg
 *          FieldVal= Value (in the LSBits) of the field to be written
 *                    If FieldVal is already in the correct position (i.e.,
 *                    does not need to be shifted,) set BitPos to 0
 *
 * Return:  None
 *
 *===========================================================================*/
static void adv7513_I2CWriteField8 (struct i2c_client *client,u8 RegAddr, u8 Mask,
                         u8 BitPos, u8 FieldVal)
{
    u8 rdata, wdata;

    adv7513_read(client, RegAddr, &rdata);
    rdata &= (~Mask);
    wdata = rdata | ((FieldVal<<BitPos)&Mask);
    adv7513_write(client, RegAddr, wdata);
}

static int adv7513_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct adv7513_data *adv7513;
	struct device *dev = &client->dev;
    u8 value;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_warn(&client->dev,
			 "I2C adapter doesn't support I2C_FUNC_SMBUS_BYTE\n");
		return -EIO;
	}

	adv7513 = devm_kzalloc(&client->dev, sizeof(*adv7513), GFP_KERNEL);
	if (!adv7513)
		return -ENOMEM;

	if (of_property_read_u32(dev->of_node, "def-width", &adv7513->def_width)) {
		dev_err(dev,"Missing def_width property in the DT \n");
		ret = -EINVAL;
	}

	adv7513->client = client;
	i2c_set_clientdata(client, adv7513);

	adv7513_read(client, 0x00, &value);
	if (value != 0x13) {
		dev_info(&client->dev, "%s[%d],version = 0x%x(NOT 0x13), not find device !\n",__func__,__LINE__,value);
		return -ENODEV;
	}

	adv7513_I2CWriteField8(client, 0x41, 0x40, 0x6, 0x00);
	adv7513_I2CWriteField8(client, 0x98, 0xFF, 0x0, 0x03);
	adv7513_I2CWriteField8(client, 0x9A, 0xE0, 0x5, 0x07);
	adv7513_I2CWriteField8(client, 0x9C, 0xFF, 0x0, 0x30);
	adv7513_I2CWriteField8(client, 0x9D, 0x03, 0x0, 0x01);
	adv7513_I2CWriteField8(client, 0xA2, 0xFF, 0x0, 0xA4);
	adv7513_I2CWriteField8(client, 0xA3, 0xFF, 0x0, 0xA4);
	adv7513_I2CWriteField8(client, 0xE0, 0xFF, 0x0, 0xD0);
	adv7513_I2CWriteField8(client, 0xF9, 0xFF, 0x0, 0x00);

	adv7513_I2CWriteField8(client, 0x15, 0x0F, 0x0, 0x00);
	adv7513_I2CWriteField8(client, 0x16, 0x30, 0x4, 0x03);
	adv7513_I2CWriteField8(client, 0x16, 0x0C, 0x2, 0x00);
	adv7513_I2CWriteField8(client, 0x17, 0x02, 0x1, 0x00);

	adv7513_I2CWriteField8(client, 0x16, 0xC0, 0x6, 0x00);
	adv7513_I2CWriteField8(client, 0x18, 0x80, 0x7, 0x00);
	adv7513_I2CWriteField8(client, 0x18, 0x60, 0x5, 0x00);
	adv7513_I2CWriteField8(client, 0xAF, 0x02, 0x1, 0x01);
	adv7513_I2CWriteField8(client, 0x3C, 0x3F, 0x0, 0x01);

	switch(adv7513->def_width) {
		case 288:
			adv7513_I2CWriteField8(client, 0x3C, 0x3F, 0x0, 0x18);//288P
			adv7513_I2CWriteField8(client, 0x3B, 0xFF, 0x0, 0x00);
			break;
		case 640:
			adv7513_I2CWriteField8(client, 0x3C, 0x3F, 0x0, 0x01);
			adv7513_I2CWriteField8(client, 0x3B, 0xFF, 0x0, 0x4A); //b01001010
			break;
		case 1280:
			adv7513_I2CWriteField8(client, 0x3C, 0x3F, 0x0, 0x04);//720P
			adv7513_I2CWriteField8(client, 0x3B, 0xFF, 0x0, 0x00);//b01011000
			break;
		case 1920:
			adv7513_I2CWriteField8(client, 0x3C, 0x3F, 0x0, 0x10);//1080P
			adv7513_I2CWriteField8(client, 0x3B, 0xFF, 0x0, 0x00);//b01011000
			break;
		default:
			dev_err(dev,"not support width %d \n",adv7513->def_width);
	}

	return ret;
}

static int adv7513_remove(struct i2c_client *client)
{
	struct adv7513 *adv7513 = i2c_get_clientdata(client);

	return 0;
}

static const struct i2c_device_id adv7513_id[] = {
	{ "adv7513", 0 },
	{ }
};

static const struct of_device_id dvp_adv7513_dt_ids[] = {
	{ .compatible = "adv7513", },
	{ /* sentinel */ }
};

static struct i2c_driver adv7513_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "adv7513",
        .of_match_table = dvp_adv7513_dt_ids,
	},
	.probe		= adv7513_probe,
	.remove		= adv7513_remove,
	.id_table	= adv7513_id,
};

static __init int init_adv7513(void)
{
    int err;

	err = i2c_add_driver(&adv7513_driver);
    if (err != 0)
		printk("i2c driver registration failed, error=%d\n", err);

	return err;
}

static __exit void exit_adv7513(void)
{
	i2c_del_driver(&adv7513_driver);
}

//late_initcall(init_adv7513);
fs_initcall(init_adv7513);
module_exit(exit_adv7513);

MODULE_DESCRIPTION("A driver for adv7513");
MODULE_LICENSE("GPL");
