// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 StarFive Technology Co., Ltd
 * Author: StarFive <StarFive@starfivetech.com>
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

static struct starfive_encoder_data {
	int endpoint_reg;
	int encoder_type;
};


static void starfive_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static const struct drm_encoder_funcs starfive_encoder_funcs = {
	.destroy = starfive_encoder_destroy,
};

static int starfive_encoder_of_parse_ports(struct device *dev,
				struct starfive_encoder_data **data)
{
	struct device_node *node = NULL;
	struct device_node *remote = NULL;
	struct starfive_encoder_data *encoder_data = NULL;
	int ret, num_port = 0;

	for_each_endpoint_of_node(dev->of_node, node) {

		if (!of_device_is_available(node))
			continue;

		remote = of_graph_get_remote_port_parent(node);
		if (!remote) {
			dev_err(dev, "Cannot get remote parent\n");
			ret = -EINVAL;
			goto err_cleanup;
		}

		num_port++;
	}

	encoder_data = kzalloc(num_port * sizeof(*encoder_data), GFP_KERNEL);
	*data = encoder_data;

	for_each_endpoint_of_node(dev->of_node, node) {

		if (!of_device_is_available(node))
			continue;

		of_property_read_u32(node, "encoder-type", &encoder_data->encoder_type);
		of_property_read_u32(node, "reg", &encoder_data->endpoint_reg);
		encoder_data ++;
	}

	return num_port;

error_alloc:
err_cleanup:
	of_node_put(node);
	return ret;
}


static int starfive_encoder_bind(struct device *dev, struct device *master, void *data)
{
	printk("-----%s: %d\n", __func__, __LINE__);
	struct drm_device *drm_dev = data;
	struct device_node *np = dev->of_node;
	struct starfive_encoder *encoderp;
	int ret;
	int i = 0;

	struct drm_panel *tmp_panel;
	struct drm_bridge *tmp_bridge;
	struct starfive_encoder_data *encoder_data = NULL;

	u32 num_ports = 0;

	num_ports = starfive_encoder_of_parse_ports(dev, &encoder_data);

	encoderp = devm_kzalloc(dev, num_ports * sizeof(*encoderp), GFP_KERNEL);
	if (!encoderp)
		return -ENOMEM;

	dev_set_drvdata(dev, encoderp);

	for (i = 0; i < num_ports; i++) {
		encoderp[i].dev = dev;
		encoderp[i].drm_dev = drm_dev;
		encoderp[i].data = &encoder_data[i];
		encoderp[i].encoder.possible_crtcs = 0x1;

		ret = drm_encoder_init(drm_dev, &encoderp[i].encoder, &starfive_encoder_funcs,
				   encoder_data[i].encoder_type, NULL);
		if (ret)
			goto err_encoder;

		ret = drm_of_find_panel_or_bridge(dev->of_node, 0,
					encoder_data[i].endpoint_reg, &tmp_panel, &tmp_bridge);
		if (ret) {
			dev_err(dev,"endpoint returns %d\n", ret);
		}

		if (tmp_panel) {
			DRM_DEBUG("found panel on endpoint \n");
		}
		if (tmp_bridge) {
			DRM_DEBUG("found bridge on endpoint \n");
		}

		ret = drm_bridge_attach(&encoderp[i].encoder, tmp_bridge, NULL, 0);
		if (ret)
			goto err_bridge;
	}

	return 0;

err_bridge:
	drm_encoder_cleanup(&encoderp[i].encoder);
err_encoder:
	return ret;

}

static void starfive_encoder_unbind(struct device *dev, struct device *master, void *data)
{
	printk("-----%s: %d\n", __func__, __LINE__);
	struct starfive_encoder *encoderp = dev_get_drvdata(dev);
}

static const struct component_ops starfive_encoder_component_ops = {
	.bind   = starfive_encoder_bind,
	.unbind = starfive_encoder_unbind,
};

static const struct of_device_id starfive_encoder_driver_dt_match[] = {
	{ .compatible = "starfive,display-encoder",
	  /*.data = &7100-crtc*/ },
	{},
};
MODULE_DEVICE_TABLE(of, starfive_encoder_driver_dt_match);

static int starfive_encoder_probe(struct platform_device *pdev)
{
	printk("-----%s: %d\n", __func__, __LINE__);
	return component_add(&pdev->dev, &starfive_encoder_component_ops);
}

static int starfive_encoder_remove(struct platform_device *pdev)
{
	printk("-----%s: %d\n", __func__, __LINE__);
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
