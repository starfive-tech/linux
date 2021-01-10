/*
 * Analog Devices ADV7513 HDMI transmitter driver
 *
 * Copyright 2020 StarFive Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef __AD_I2C_ADV7513_H__
#define __AD_I2C_ADV7513_H__

#define  H_SIZE   1920//352//1920//1280
#define  V_SIZE   1080//288//1080//720

struct adv7513_data {
	struct i2c_client *client;
	struct device   *dev;
	int 			irq;
	int		def_width;
};

#endif
