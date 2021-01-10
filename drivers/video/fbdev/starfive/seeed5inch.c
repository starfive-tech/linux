/*
 ******************************************************************************
 * @file  seeed5inch.c
 * @author  StarFive Technology
 * @version  V1.0
 * @date  01/07/2021
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
 * <h2><center>&copy; COPYRIGHT 2021 Shanghai StarFive Technology Co., Ltd. </center></h2>
 */
#include <linux/clk.h>
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

/* I2C registers of the Atmel microcontroller. */
enum REG_ADDR {
    REG_ID = 0x80,
    REG_PORTA, /* BIT(2) for horizontal flip, BIT(3) for vertical flip */
    REG_PORTB,
    REG_PORTC,
    REG_PORTD,
    REG_POWERON,
    REG_PWM,
    REG_DDRA,
    REG_DDRB,
    REG_DDRC,
    REG_DDRD,
    REG_TEST,
    REG_WR_ADDRL,
    REG_WR_ADDRH,
    REG_READH,
    REG_READL,
    REG_WRITEH,
    REG_WRITEL,
    REG_ID2,
};

struct seeed_panel_dev {
	struct i2c_client *client;
	struct device   *dev;
	int 			irq;
};

struct dcs_buffer {
    u32 len;
    union {
        u32 val32;
        char val8[4];
    };
};

static int seeed_panel_i2c_write(struct i2c_client *client, u8 reg, u8 val)
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
		"seeed panel i2c write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

static int seeed_panel_i2c_read(struct i2c_client *client, u8 reg, u8 *val)
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
		"seeed panel i2c read reg(0x%x val:0x%x) failed,ret = %d !\n", reg, *val, ret);

	return ret;
}

#if 0
static int seeed_panel_disable(struct i2c_client *client)
{
    seeed_panel_i2c_write(client, REG_PWM, 0);
    seeed_panel_i2c_write(client, REG_POWERON, 0);
    udelay(1);

    return 0;
}
#endif

enum dsi_rgb_pattern_t {
    RGB_PAT_WHITE,
    RGB_PAT_BLACK,
    RGB_PAT_RED,
    RGB_PAT_GREEN,
    RGB_PAT_BLUE,
    RGB_PAT_HORIZ_COLORBAR,
    RGB_PAT_VERT_COLORBAR,
    RGB_PAT_NUM
};

static int seeed_panel_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	u8 reg_value = 0;
//	int ret = 0;
	int i;
	struct seeed_panel_dev *seeed_panel;
//	struct device *dev = &client->dev;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_warn(&client->dev,
			 "I2C adapter doesn't support I2C_FUNC_SMBUS_BYTE\n");
		return -EIO;
	}
	
	seeed_panel = devm_kzalloc(&client->dev, sizeof(struct seeed_panel_dev), GFP_KERNEL);
	if (!seeed_panel)
		return -ENOMEM;

	seeed_panel->client = client;
	i2c_set_clientdata(client, seeed_panel);

	seeed_panel_i2c_read(client, REG_ID, &reg_value);
	dev_info(&client->dev, "%s[%d],reg[0x80] = 0x%x\n",__func__,__LINE__,reg_value);
	switch (reg_value) {
		case 0xde: /* ver 1 */
		case 0xc3: /* ver 2 */
		break;

		default:
			dev_err(&client->dev, "Unknown Atmel firmware revision: 0x%02x\n", reg_value);
			return -ENODEV;
	}

    seeed_panel_i2c_write(client, REG_POWERON, 1);
    mdelay(5);
    /* Wait for nPWRDWN to go low to indicate poweron is done. */
    for (i = 0; i < 100; i++) {
		seeed_panel_i2c_read(client, REG_PORTB, &reg_value);
        if (reg_value & 1)
            break;
    }

	seeed_panel_i2c_write(client, REG_PWM, 255);
	seeed_panel_i2c_write(client, REG_PORTA, BIT(2));

	return 0;
}

static int seeed_panel_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id seeed_panel_id[] = {
	{ "seeed_panel", 0 },
	{ }
};

static const struct of_device_id seeed_panel_dt_ids[] = {
	{ .compatible = "seeed_panel", },
	{ /* sentinel */ }
};

static struct i2c_driver seeed_panel_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "seeed_panel",
        .of_match_table = seeed_panel_dt_ids,
	},
	.probe		= seeed_panel_probe,
	.remove		= seeed_panel_remove,
	.id_table	= seeed_panel_id,
};

static __init int init_seeed_panel(void)
{
    int err;

	err = i2c_add_driver(&seeed_panel_driver);
    if (err != 0)
		printk("i2c driver registration failed, error=%d\n", err);

	return err;
}

static __exit void exit_seeed_panel(void)
{
	i2c_del_driver(&seeed_panel_driver);
}

//module_init(init_seeed_panel);
fs_initcall(init_seeed_panel);
module_exit(exit_seeed_panel);

MODULE_DESCRIPTION("A driver for seeed_panel");
MODULE_LICENSE("GPL");
