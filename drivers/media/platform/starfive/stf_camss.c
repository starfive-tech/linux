// SPDX-License-Identifier: GPL-2.0
/*
 * stf_camss.c
 *
 * Starfive Camera Subsystem driver
 *
 * Copyright (C) 2021-2023 StarFive Technology Co., Ltd.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/videodev2.h>
#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mc.h>

#include "stf_camss.h"

static const char * const stfcamss_clocks[] = {
	"clk_apb_func",
	"clk_wrapper_clk_c",
	"clk_dvp_inv",
	"clk_axiwr",
	"clk_mipi_rx0_pxl",
	"clk_ispcore_2x",
	"clk_isp_axi",
};

static const char * const stfcamss_resets[] = {
	"rst_wrapper_p",
	"rst_wrapper_c",
	"rst_axird",
	"rst_axiwr",
	"rst_isp_top_n",
	"rst_isp_top_axi",
};

static int stfcamss_get_mem_res(struct platform_device *pdev,
				struct stfcamss *stfcamss)
{
	stfcamss->syscon_base =
		devm_platform_ioremap_resource_byname(pdev, "syscon");
	if (IS_ERR(stfcamss->syscon_base))
		return PTR_ERR(stfcamss->syscon_base);

	stfcamss->isp_base =
		devm_platform_ioremap_resource_byname(pdev, "isp");
	if (IS_ERR(stfcamss->isp_base))
		return PTR_ERR(stfcamss->isp_base);

	return 0;
}

/*
 * stfcamss_of_parse_endpoint_node - Parse port endpoint node
 * @dev: Device
 * @node: Device node to be parsed
 * @csd: Parsed data from port endpoint node
 *
 * Return 0 on success or a negative error code on failure
 */
static int stfcamss_of_parse_endpoint_node(struct device *dev,
					   struct device_node *node,
					   struct stfcamss_async_subdev *csd)
{
	struct v4l2_fwnode_endpoint vep = { { 0 } };

	v4l2_fwnode_endpoint_parse(of_fwnode_handle(node), &vep);
	dev_dbg(dev, "vep.base.port = 0x%x, id = 0x%x\n",
		vep.base.port, vep.base.id);

	csd->port = vep.base.port;

	return 0;
}

/*
 * stfcamss_of_parse_ports - Parse ports node
 * @stfcamss: STFCAMSS device
 *
 * Return number of "port" nodes found in "ports" node
 */
static int stfcamss_of_parse_ports(struct stfcamss *stfcamss)
{
	struct device *dev = stfcamss->dev;
	struct device_node *node = NULL;
	struct device_node *remote = NULL;
	int ret, num_subdevs = 0;

	for_each_endpoint_of_node(dev->of_node, node) {
		struct stfcamss_async_subdev *csd;

		if (!of_device_is_available(node))
			continue;

		remote = of_graph_get_remote_port_parent(node);
		if (!remote) {
			dev_err(dev, "Cannot get remote parent\n");
			ret = -EINVAL;
			goto err_cleanup;
		}

		csd = v4l2_async_nf_add_fwnode(&stfcamss->notifier,
					       of_fwnode_handle(remote),
					       struct stfcamss_async_subdev);
		of_node_put(remote);
		if (IS_ERR(csd)) {
			ret = PTR_ERR(csd);
			goto err_cleanup;
		}

		ret = stfcamss_of_parse_endpoint_node(dev, node, csd);
		if (ret < 0)
			goto err_cleanup;

		num_subdevs++;
	}

	return num_subdevs;

err_cleanup:
	of_node_put(node);
	return ret;
}

/*
 * stfcamss_init_subdevices - Initialize subdev structures and resources
 * @stfcamss: STFCAMSS device
 *
 * Return 0 on success or a negative error code on failure
 */
static int stfcamss_init_subdevices(struct stfcamss *stfcamss)
{
	int ret;

	ret = stf_isp_subdev_init(stfcamss);
	if (ret < 0) {
		dev_err(stfcamss->dev, "Failed to init isp subdev: %d\n", ret);
		return ret;
	}

	return ret;
}

static int stfcamss_register_subdevices(struct stfcamss *stfcamss)
{
	int ret;
	struct stf_isp_dev *isp_dev = &stfcamss->isp_dev;

	ret = stf_isp_register(isp_dev, &stfcamss->v4l2_dev);
	if (ret < 0) {
		dev_err(stfcamss->dev,
			"Failed to register stf isp%d entity: %d\n", 0, ret);
		return ret;
	}

	return ret;
}

static void stfcamss_unregister_subdevices(struct stfcamss *stfcamss)
{
	stf_isp_unregister(&stfcamss->isp_dev);
}

static int stfcamss_subdev_notifier_bound(struct v4l2_async_notifier *async,
					  struct v4l2_subdev *subdev,
					  struct v4l2_async_subdev *asd)
{
	struct stfcamss *stfcamss =
		container_of(async, struct stfcamss, notifier);
	struct stfcamss_async_subdev *csd =
		container_of(asd, struct stfcamss_async_subdev, asd);
	enum port_num port = csd->port;
	struct stf_isp_dev *isp_dev = &stfcamss->isp_dev;
	struct host_data *host_data = &stfcamss->host_data;
	struct media_entity *source;
	int i, j;

	if (port == PORT_NUMBER_CSI2RX) {
		host_data->host_entity[1] = &isp_dev->subdev.entity;
	} else if (port == PORT_NUMBER_DVP_SENSOR) {
		dev_err(stfcamss->dev, "Not support DVP sensor\n");
		return -EPERM;
	}

	source = &subdev->entity;

	for (i = 0; i < source->num_pads; i++) {
		if (source->pads[i].flags & MEDIA_PAD_FL_SOURCE)
			break;
	}

	if (i == source->num_pads) {
		dev_err(stfcamss->dev, "No source pad in external entity\n");
		return -EINVAL;
	}

	for (j = 0; host_data->host_entity[j] && (j < HOST_ENTITY_MAX); j++) {
		struct media_entity *input;
		int ret;

		input = host_data->host_entity[j];

		ret = media_create_pad_link(
			source,
			i,
			input,
			STF_PAD_SINK,
			source->function == MEDIA_ENT_F_CAM_SENSOR ?
			MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED :
			0);
		if (ret < 0) {
			dev_err(stfcamss->dev,
				"Failed to link %s->%s entities: %d\n",
				source->name, input->name, ret);
			return ret;
		}
	}

	return 0;
}

static int stfcamss_subdev_notifier_complete(struct v4l2_async_notifier *ntf)
{
	struct stfcamss *stfcamss =
		container_of(ntf, struct stfcamss, notifier);

	return v4l2_device_register_subdev_nodes(&stfcamss->v4l2_dev);
}

static const struct v4l2_async_notifier_operations
stfcamss_subdev_notifier_ops = {
	.bound = stfcamss_subdev_notifier_bound,
	.complete = stfcamss_subdev_notifier_complete,
};

static const struct media_device_ops stfcamss_media_ops = {
	.link_notify = v4l2_pipeline_link_notify,
};

static void stfcamss_mc_init(struct platform_device *pdev,
			     struct stfcamss *stfcamss)
{
	stfcamss->media_dev.dev = stfcamss->dev;
	strscpy(stfcamss->media_dev.model, "Starfive Camera Subsystem",
		sizeof(stfcamss->media_dev.model));
	snprintf(stfcamss->media_dev.bus_info,
		 sizeof(stfcamss->media_dev.bus_info),
		 "%s:%s", dev_bus_name(&pdev->dev), pdev->name);
	stfcamss->media_dev.hw_revision = 0x01;
	stfcamss->media_dev.ops = &stfcamss_media_ops;
	media_device_init(&stfcamss->media_dev);

	stfcamss->v4l2_dev.mdev = &stfcamss->media_dev;
}

/*
 * stfcamss_probe - Probe STFCAMSS platform device
 * @pdev: Pointer to STFCAMSS platform device
 *
 * Return 0 on success or a negative error code on failure
 */
static int stfcamss_probe(struct platform_device *pdev)
{
	struct stfcamss *stfcamss;
	struct device *dev = &pdev->dev;
	int ret = 0, i, num_subdevs;

	stfcamss = devm_kzalloc(dev, sizeof(*stfcamss), GFP_KERNEL);
	if (!stfcamss)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(stfcamss->irq); ++i) {
		stfcamss->irq[i] = platform_get_irq(pdev, i);
		if (stfcamss->irq[i] < 0)
			return dev_err_probe(&pdev->dev, stfcamss->irq[i],
					     "Failed to get clock%d", i);
	}

	stfcamss->nclks = ARRAY_SIZE(stfcamss->sys_clk);
	for (i = 0; i < ARRAY_SIZE(stfcamss->sys_clk); ++i)
		stfcamss->sys_clk[i].id = stfcamss_clocks[i];
	ret = devm_clk_bulk_get(dev, stfcamss->nclks, stfcamss->sys_clk);
	if (ret) {
		dev_err(dev, "Failed to get clk controls\n");
		return ret;
	}

	stfcamss->nrsts = ARRAY_SIZE(stfcamss->sys_rst);
	for (i = 0; i < ARRAY_SIZE(stfcamss->sys_rst); ++i)
		stfcamss->sys_rst[i].id = stfcamss_resets[i];
	ret = devm_reset_control_bulk_get_shared(dev, stfcamss->nrsts,
						 stfcamss->sys_rst);
	if (ret) {
		dev_err(dev, "Failed to get reset controls\n");
		return ret;
	}

	ret = stfcamss_get_mem_res(pdev, stfcamss);
	if (ret) {
		dev_err(dev, "Could not map registers\n");
		return ret;
	}

	stfcamss->dev = dev;
	platform_set_drvdata(pdev, stfcamss);

	v4l2_async_nf_init(&stfcamss->notifier);

	num_subdevs = stfcamss_of_parse_ports(stfcamss);
	if (num_subdevs < 0) {
		dev_err(dev, "Failed to find subdevices\n");
		return -ENODEV;
	}

	ret = stfcamss_init_subdevices(stfcamss);
	if (ret < 0) {
		dev_err(dev, "Failed to init subdevice: %d\n", ret);
		goto err_cleanup_notifier;
	}

	stfcamss_mc_init(pdev, stfcamss);

	ret = v4l2_device_register(stfcamss->dev, &stfcamss->v4l2_dev);
	if (ret < 0) {
		dev_err(dev, "Failed to register V4L2 device: %d\n", ret);
		goto err_cleanup_media_device;
	}

	ret = media_device_register(&stfcamss->media_dev);
	if (ret) {
		dev_err(dev, "Failed to register media device: %d\n", ret);
		goto err_unregister_device;
	}

	ret = stfcamss_register_subdevices(stfcamss);
	if (ret < 0) {
		dev_err(dev, "Failed to register subdevice: %d\n", ret);
		goto err_unregister_media_dev;
	}

	stfcamss->notifier.ops = &stfcamss_subdev_notifier_ops;
	ret = v4l2_async_nf_register(&stfcamss->v4l2_dev, &stfcamss->notifier);
	if (ret) {
		dev_err(dev, "Failed to register async subdev nodes: %d\n",
			ret);
		goto err_unregister_subdevs;
	}

	pm_runtime_enable(dev);

	return 0;

err_unregister_subdevs:
	stfcamss_unregister_subdevices(stfcamss);
err_unregister_media_dev:
	media_device_unregister(&stfcamss->media_dev);
err_unregister_device:
	v4l2_device_unregister(&stfcamss->v4l2_dev);
err_cleanup_media_device:
	media_device_cleanup(&stfcamss->media_dev);
err_cleanup_notifier:
	v4l2_async_nf_cleanup(&stfcamss->notifier);
	return ret;
}

/*
 * stfcamss_remove - Remove STFCAMSS platform device
 * @pdev: Pointer to STFCAMSS platform device
 *
 * Always returns 0.
 */
static int stfcamss_remove(struct platform_device *pdev)
{
	struct stfcamss *stfcamss = platform_get_drvdata(pdev);

	stfcamss_unregister_subdevices(stfcamss);
	v4l2_device_unregister(&stfcamss->v4l2_dev);
	media_device_cleanup(&stfcamss->media_dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id stfcamss_of_match[] = {
	{ .compatible = "starfive,jh7110-camss" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, stfcamss_of_match);

static int __maybe_unused stfcamss_runtime_suspend(struct device *dev)
{
	struct stfcamss *stfcamss = dev_get_drvdata(dev);

	reset_control_assert(stfcamss->sys_rst[STF_RST_ISP_TOP_AXI].rstc);
	reset_control_assert(stfcamss->sys_rst[STF_RST_ISP_TOP_N].rstc);
	clk_disable_unprepare(stfcamss->sys_clk[STF_CLK_ISP_AXI].clk);
	clk_disable_unprepare(stfcamss->sys_clk[STF_CLK_ISPCORE_2X].clk);

	return 0;
}

static int __maybe_unused stfcamss_runtime_resume(struct device *dev)
{
	struct stfcamss *stfcamss = dev_get_drvdata(dev);

	clk_prepare_enable(stfcamss->sys_clk[STF_CLK_ISPCORE_2X].clk);
	clk_prepare_enable(stfcamss->sys_clk[STF_CLK_ISP_AXI].clk);
	reset_control_deassert(stfcamss->sys_rst[STF_RST_ISP_TOP_N].rstc);
	reset_control_deassert(stfcamss->sys_rst[STF_RST_ISP_TOP_AXI].rstc);

	return 0;
}

static const struct dev_pm_ops stfcamss_pm_ops = {
	SET_RUNTIME_PM_OPS(stfcamss_runtime_suspend,
			   stfcamss_runtime_resume,
			   NULL)
};

static struct platform_driver stfcamss_driver = {
	.probe = stfcamss_probe,
	.remove = stfcamss_remove,
	.driver = {
		.name = DRV_NAME,
		.pm = &stfcamss_pm_ops,
		.of_match_table = of_match_ptr(stfcamss_of_match),
	},
};

module_platform_driver(stfcamss_driver);

MODULE_AUTHOR("StarFive Corporation");
MODULE_DESCRIPTION("StarFive Camera Subsystem driver");
MODULE_LICENSE("GPL");
