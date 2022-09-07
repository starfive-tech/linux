// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_of.h>
#include "starfive_drm_drv.h"
#include "starfive_drm_encoder.h"

static void starfive_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
	kfree(encoder);
}

static const struct drm_encoder_funcs starfive_encoder_funcs = {
	.destroy = starfive_encoder_destroy,
};

static int starfive_encoder_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;
	struct device_node *np = dev->of_node;
	struct starfive_encoder *encoderp;
	int ret;
	struct drm_panel *tmp_panel;
	struct drm_bridge *tmp_bridge;
	u32 crtcs = 0;

	encoderp = devm_kzalloc(dev, sizeof(*encoderp), GFP_KERNEL);
	if (!encoderp)
		return -ENOMEM;

	encoderp->dev = dev;
	encoderp->drm_dev = drm_dev;
	dev_set_drvdata(dev, encoderp);

	if (dev->of_node) {
		crtcs = drm_of_find_possible_crtcs(drm_dev, dev->of_node);

		if (of_property_read_u32(np, "encoder-type", &encoderp->encoder_type)) {
			DRM_ERROR("Missing encoder-type property in the DT.\n");
			encoderp->encoder_type = DRM_MODE_ENCODER_TMDS;
		}
	}

	/* If no CRTCs were found, fall back to our old behaviour */
	if (crtcs == 0) {
		dev_warn(dev, "Falling back to first CRTC\n");
		crtcs = 1 << 0;
	}

	encoderp->encoder.possible_crtcs = crtcs;

	ret = drm_encoder_init(drm_dev, &encoderp->encoder,
			       &starfive_encoder_funcs,
			       encoderp->encoder_type, NULL);
	if (ret)
		return dev_err_probe(dev, ret, "error initializing encoder\n");

	ret = drm_of_find_panel_or_bridge(dev->of_node, 0, 0,
					  &tmp_panel, &tmp_bridge);
	if (ret) {
		dev_err_probe(dev, ret, "endpoint returns %d\n", ret);
		goto err_bridge;
	}

	if (tmp_panel)
		DRM_INFO("found panel on endpoint\n");
	if (tmp_bridge)
		DRM_INFO("found bridge on endpoint\n");

	ret = drm_bridge_attach(&encoderp->encoder, tmp_bridge, NULL, 0);
	if (ret)
		goto err_bridge;

	return 0;

err_bridge:
	drm_encoder_cleanup(&encoderp->encoder);
	return ret;
}

static void starfive_encoder_unbind(struct device *dev, struct device *master, void *data)
{
	struct starfive_encoder *encoderp = dev_get_drvdata(dev);

	starfive_encoder_destroy(&encoderp->encoder);
}

static const struct component_ops starfive_encoder_component_ops = {
	.bind   = starfive_encoder_bind,
	.unbind = starfive_encoder_unbind,
};

static const struct of_device_id starfive_encoder_driver_dt_match[] = {
	{ .compatible = "starfive,display-encoder" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, starfive_encoder_driver_dt_match);

static int starfive_encoder_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &starfive_encoder_component_ops);
}

static int starfive_encoder_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &starfive_encoder_component_ops);
	return 0;
}

struct platform_driver starfive_encoder_driver = {
	.probe = starfive_encoder_probe,
	.remove = starfive_encoder_remove,
	.driver = {
		.name = "display-encoder",
		.of_match_table = of_match_ptr(starfive_encoder_driver_dt_match),
	},
};
