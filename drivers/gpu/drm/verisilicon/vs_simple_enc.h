/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_SIMPLE_ENC_H_
#define __VS_SIMPLE_ENC_H_

struct simple_encoder_priv {
	unsigned char encoder_type;
};

struct simple_encoder {
	struct drm_encoder encoder;
	struct device *dev;
	const struct simple_encoder_priv *priv;
	struct regmap *dss_regmap;
	unsigned int offset;
	unsigned int mask;
};

extern struct platform_driver simple_encoder_driver;
#endif /* __VS_SIMPLE_ENC_H_ */
