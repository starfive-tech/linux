// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 */
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/module.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_of.h>
#include <linux/regmap.h>
#include <linux/media-bus-format.h>
#include <linux/mfd/syscon.h>

#include "vs_crtc.h"
#include "vs_simple_enc.h"

static const struct simple_encoder_priv dsi_priv = {
	.encoder_type = DRM_MODE_ENCODER_DSI
};

static inline struct simple_encoder *to_simple_encoder(struct drm_encoder *enc)
{
	return container_of(enc, struct simple_encoder, encoder);
}

static int encoder_parse_dt(struct device *dev)
{
	struct simple_encoder *simple = dev_get_drvdata(dev);
	unsigned int args[2];

	simple->dss_regmap = syscon_regmap_lookup_by_phandle_args(dev->of_node,
								  "starfive,syscon",
								  2, args);

	if (IS_ERR(simple->dss_regmap)) {
		return dev_err_probe(dev, PTR_ERR(simple->dss_regmap),
				     "getting the regmap failed\n");
	}

	simple->offset = args[0];
	simple->mask = args[1];

	return 0;
}

void encoder_atomic_enable(struct drm_encoder *encoder,
			   struct drm_atomic_state *state)
{
	struct simple_encoder *simple = to_simple_encoder(encoder);

	regmap_update_bits(simple->dss_regmap, simple->offset, simple->mask,
			   simple->mask);
}

int encoder_atomic_check(struct drm_encoder *encoder,
			 struct drm_crtc_state *crtc_state,
			 struct drm_connector_state *conn_state)
{
	struct vs_crtc_state *vs_crtc_state = to_vs_crtc_state(crtc_state);
	struct drm_connector *connector = conn_state->connector;
	int ret = 0;

	struct drm_bridge *first_bridge = drm_bridge_chain_get_first_bridge(encoder);
	struct drm_bridge_state *bridge_state = ERR_PTR(-EINVAL);

	vs_crtc_state->encoder_type = encoder->encoder_type;

	if (first_bridge && first_bridge->funcs->atomic_duplicate_state)
		bridge_state = drm_atomic_get_bridge_state(crtc_state->state, first_bridge);

	if (IS_ERR(bridge_state)) {
		if (connector->display_info.num_bus_formats)
			vs_crtc_state->output_fmt = connector->display_info.bus_formats[0];
		else
			vs_crtc_state->output_fmt = MEDIA_BUS_FMT_FIXED;
	} else {
		vs_crtc_state->output_fmt = bridge_state->input_bus_cfg.format;
	}

	switch (vs_crtc_state->output_fmt) {
	case MEDIA_BUS_FMT_FIXED:
	case MEDIA_BUS_FMT_RGB565_1X16:
	case MEDIA_BUS_FMT_RGB666_1X18:
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
	case MEDIA_BUS_FMT_RGB101010_1X30:
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_YUV8_1X24:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
	case MEDIA_BUS_FMT_UYVY10_1X20:
	case MEDIA_BUS_FMT_YUV10_1X30:
		ret = 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	/* If MEDIA_BUS_FMT_FIXED, set it to default value */
	if (vs_crtc_state->output_fmt == MEDIA_BUS_FMT_FIXED)
		vs_crtc_state->output_fmt = MEDIA_BUS_FMT_RGB888_1X24;

	return ret;
}

static const struct drm_encoder_helper_funcs encoder_helper_funcs = {
	.atomic_check = encoder_atomic_check,
	.atomic_enable = encoder_atomic_enable,
};

static int encoder_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;
	struct simple_encoder *simple = dev_get_drvdata(dev);
	struct drm_encoder *encoder;
	struct drm_bridge *bridge;
	int ret;

	encoder = &simple->encoder;

	ret = drmm_encoder_init(drm_dev, encoder, NULL, simple->priv->encoder_type, NULL);
	if (ret)
		return ret;

	drm_encoder_helper_add(encoder, &encoder_helper_funcs);

	encoder->possible_crtcs =
			drm_of_find_possible_crtcs(drm_dev, dev->of_node);

	/* output port is port1*/
	bridge = devm_drm_of_get_bridge(dev, dev->of_node, 1, 0);
	if (IS_ERR(bridge))
		return 0;

	return drm_bridge_attach(encoder, bridge, NULL, 0);
}

static const struct component_ops encoder_component_ops = {
	.bind = encoder_bind,
};

static const struct of_device_id simple_encoder_dt_match[] = {
	{ .compatible = "starfive,dsi-encoder", .data = &dsi_priv},
	{},
};
MODULE_DEVICE_TABLE(of, simple_encoder_dt_match);

static int encoder_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct simple_encoder *simple;
	int ret;

	simple = devm_kzalloc(dev, sizeof(*simple), GFP_KERNEL);
	if (!simple)
		return -ENOMEM;

	simple->priv = of_device_get_match_data(dev);

	simple->dev = dev;

	dev_set_drvdata(dev, simple);

	ret = encoder_parse_dt(dev);
	if (ret)
		return ret;

	return component_add(dev, &encoder_component_ops);
}

static int encoder_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	component_del(dev, &encoder_component_ops);
	dev_set_drvdata(dev, NULL);

	return 0;
}

struct platform_driver simple_encoder_driver = {
	.probe = encoder_probe,
	.remove = encoder_remove,
	.driver = {
		.name = "vs-simple-encoder",
		.of_match_table = of_match_ptr(simple_encoder_dt_match),
	},
};

MODULE_DESCRIPTION("Simple Encoder Driver");
MODULE_LICENSE("GPL");
