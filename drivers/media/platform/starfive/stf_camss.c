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

static struct clk_bulk_data stfcamss_clocks[] = {
	{ .id = "clk_apb_func" },
	{ .id = "clk_wrapper_clk_c" },
	{ .id = "clk_dvp_inv" },
	{ .id = "clk_axiwr" },
	{ .id = "clk_mipi_rx0_pxl" },
	{ .id = "clk_ispcore_2x" },
	{ .id = "clk_isp_axi" },
};

static struct reset_control_bulk_data stfcamss_resets[] = {
	{ .id = "rst_wrapper_p" },
	{ .id = "rst_wrapper_c" },
	{ .id = "rst_axird" },
	{ .id = "rst_axiwr" },
	{ .id = "rst_isp_top_n" },
	{ .id = "rst_isp_top_axi" },
};

int stfcamss_get_mem_res(struct platform_device *pdev,
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
 * stfcamss_find_sensor - Find a linked media entity which represents a sensor
 * @entity: Media entity to start searching from
 *
 * Return a pointer to sensor media entity or NULL if not found
 */
struct media_entity *stfcamss_find_sensor(struct media_entity *entity)
{
	struct media_pad *pad;

	while (1) {
		if (!entity->pads)
			return NULL;

		pad = &entity->pads[0];
		if (!(pad->flags & MEDIA_PAD_FL_SINK))
			return NULL;

		pad = media_pad_remote_pad_first(pad);
		if (!pad || !is_media_entity_v4l2_subdev(pad->entity))
			return NULL;

		entity = pad->entity;

		if (entity->function == MEDIA_ENT_F_CAM_SENSOR)
			return entity;
	}
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
	st_debug(ST_CAMSS, "%s: vep.base.port = 0x%x, id = 0x%x\n",
		 __func__, vep.base.port, vep.base.id);

	csd->port = vep.base.port;
	switch (csd->port) {
	case PORT_NUMBER_DVP_SENSOR:
		break;
	case PORT_NUMBER_CSI2RX:
		break;
	default:
		break;
	};

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
			st_err(ST_CAMSS, "Cannot get remote parent\n");
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
		st_err(ST_CAMSS,
		       "Failed to init stf_isp sub-device: %d\n", ret);
		return ret;
	}

	ret = stf_vin_subdev_init(stfcamss);
	if (ret < 0) {
		st_err(ST_CAMSS,
		       "Failed to init stf_vin sub-device: %d\n", ret);
		return ret;
	}
	return ret;
}

static int stfcamss_register_subdevices(struct stfcamss *stfcamss)
{
	int ret;
	struct stf_vin_dev *vin_dev = stfcamss->vin_dev;
	struct stf_isp_dev *isp_dev = stfcamss->isp_dev;

	ret = stf_isp_register(isp_dev, &stfcamss->v4l2_dev);
	if (ret < 0) {
		st_err(ST_CAMSS,
		       "Failed to register stf isp%d entity: %d\n", 0, ret);
		goto err_reg_isp;
	}

	ret = stf_vin_register(vin_dev, &stfcamss->v4l2_dev);
	if (ret < 0) {
		st_err(ST_CAMSS,
		       "Failed to register vin entity: %d\n", ret);
		goto err_reg_vin;
	}

	ret = media_create_pad_link(&isp_dev->subdev.entity,
				    STF_ISP_PAD_SRC,
				    &vin_dev->line[VIN_LINE_ISP].subdev.entity,
				    STF_VIN_PAD_SINK,
				    0);
	if (ret < 0) {
		st_err(ST_CAMSS,
		       "Failed to link %s->%s entities: %d\n",
		       isp_dev->subdev.entity.name,
		       vin_dev->line[VIN_LINE_ISP].subdev.entity.name,
		       ret);
		goto err_link;
	}

	ret = media_create_pad_link(
		&isp_dev->subdev.entity,
		STF_ISP_PAD_SRC_SS0,
		&vin_dev->line[VIN_LINE_ISP_SS0].subdev.entity,
		STF_VIN_PAD_SINK,
		0);

	if (ret < 0) {
		st_err(ST_CAMSS,
		       "Failed to link %s->%s entities: %d\n",
		       isp_dev->subdev.entity.name,
		       vin_dev->line[VIN_LINE_ISP_SS0].subdev.entity.name,
		       ret);
		goto err_link;
	}

	ret = media_create_pad_link(
		&isp_dev->subdev.entity,
		STF_ISP_PAD_SRC_SS1,
		&vin_dev->line[VIN_LINE_ISP_SS1].subdev.entity,
		STF_VIN_PAD_SINK,
		0);
	if (ret < 0) {
		st_err(ST_CAMSS,
		       "Failed to link %s->%s entities: %d\n",
		       isp_dev->subdev.entity.name,
		       vin_dev->line[VIN_LINE_ISP_SS1].subdev.entity.name,
		       ret);
		goto err_link;
	}

	ret = media_create_pad_link(
		&isp_dev->subdev.entity,
		STF_ISP_PAD_SRC_RAW,
		&vin_dev->line[VIN_LINE_ISP_RAW].subdev.entity,
		STF_VIN_PAD_SINK,
		0);
	if (ret < 0) {
		st_err(ST_CAMSS,
		       "Failed to link %s->%s entities: %d\n",
		       isp_dev->subdev.entity.name,
		       vin_dev->line[VIN_LINE_ISP_RAW].subdev.entity.name,
		       ret);
		goto err_link;
	}

	return ret;

err_link:
	stf_vin_unregister(stfcamss->vin_dev);
err_reg_vin:
	stf_isp_unregister(stfcamss->isp_dev);
err_reg_isp:
	return ret;
}

static void stfcamss_unregister_subdevices(struct stfcamss *stfcamss)
{
	stf_isp_unregister(stfcamss->isp_dev);
	stf_vin_unregister(stfcamss->vin_dev);
}

static int stfcamss_register_media_subdev_nod(struct v4l2_async_notifier *async,
					      struct v4l2_subdev *sd)
{
	struct stfcamss *stfcamss =
		container_of(async, struct stfcamss, notifier);
	struct host_data *host_data = v4l2_get_subdev_hostdata(sd);
	struct media_entity *sensor;
	struct media_entity *input;
	int ret;
	int i, j;

	for (i = 0; host_data->host_entity[i] && (i < HOST_ENTITY_MAX); i++) {
		sensor = &sd->entity;
		input = host_data->host_entity[i];

		for (j = 0; j < sensor->num_pads; j++) {
			if (sensor->pads[j].flags & MEDIA_PAD_FL_SOURCE)
				break;
		}

		if (j == sensor->num_pads) {
			st_err(ST_CAMSS, "No source pad in external entity\n");
			return -EINVAL;
		}

		ret = media_create_pad_link(
			sensor,
			j,
			input,
			STF_PAD_SINK,
			sensor->function == MEDIA_ENT_F_CAM_SENSOR ?
			MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED :
			0);
		if (ret < 0) {
			st_err(ST_CAMSS, "Failed to link %s->%s entities: %d\n",
			       sensor->name, input->name, ret);
			return ret;
		}
	}

	ret = v4l2_device_register_subdev_nodes(&stfcamss->v4l2_dev);
	if (ret < 0)
		return ret;

	if (stfcamss->media_dev.devnode)
		return ret;

	st_debug(ST_CAMSS, "stfcamss register media device\n");
	return media_device_register(&stfcamss->media_dev);
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
	struct stf_isp_dev *isp_dev = stfcamss->isp_dev;
	struct stf_vin_dev *vin_dev = stfcamss->vin_dev;
	struct host_data *host_data = &stfcamss->host_data;

	switch (port) {
	case PORT_NUMBER_DVP_SENSOR:
		host_data->host_entity[0] = NULL;
		host_data->host_entity[1] = NULL;
		/* not support DVP sensor */
		break;
	case PORT_NUMBER_CSI2RX:
		host_data->host_entity[0] =
			&vin_dev->line[VIN_LINE_WR].subdev.entity;
		host_data->host_entity[1] = &isp_dev->subdev.entity;
		break;
	default:
		break;
	};

	v4l2_set_subdev_hostdata(subdev, host_data);
	stfcamss_register_media_subdev_nod(async, subdev);

	return 0;
}

static const struct v4l2_async_notifier_operations
stfcamss_subdev_notifier_ops = {
	.bound = stfcamss_subdev_notifier_bound,
};

static const struct media_device_ops stfcamss_media_ops = {
	.link_notify = v4l2_pipeline_link_notify,
};

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
	int ret = 0, num_subdevs;

	stfcamss = devm_kzalloc(dev, sizeof(struct stfcamss), GFP_KERNEL);
	if (!stfcamss)
		return -ENOMEM;

	stfcamss->isp_dev = devm_kzalloc(dev, sizeof(*stfcamss->isp_dev),
					 GFP_KERNEL);
	if (!stfcamss->isp_dev) {
		ret = -ENOMEM;
		goto err_cam;
	}

	stfcamss->vin_dev = devm_kzalloc(dev, sizeof(*stfcamss->vin_dev),
					 GFP_KERNEL);
	if (!stfcamss->vin_dev) {
		ret = -ENOMEM;
		goto err_cam;
	}

	stfcamss->irq = platform_get_irq(pdev, 0);
	if (stfcamss->irq <= 0) {
		st_err(ST_CAMSS, "Could not get irq\n");
		goto err_cam;
	}

	stfcamss->isp_irq = platform_get_irq(pdev, 1);
	if (stfcamss->isp_irq <= 0) {
		st_err(ST_CAMSS, "Could not get isp irq\n");
		goto err_cam;
	}

	stfcamss->isp_irq_csi = platform_get_irq(pdev, 2);
	if (stfcamss->isp_irq_csi <= 0) {
		st_err(ST_CAMSS, "Could not get isp csi irq\n");
		goto err_cam;
	}

	stfcamss->isp_irq_csiline = platform_get_irq(pdev, 3);
	if (stfcamss->isp_irq_csiline <= 0) {
		st_err(ST_CAMSS, "Could not get isp irq csiline\n");
		goto err_cam;
	}

	pm_runtime_enable(dev);

	stfcamss->nclks = ARRAY_SIZE(stfcamss_clocks);
	stfcamss->sys_clk = stfcamss_clocks;

	ret = devm_clk_bulk_get(dev, stfcamss->nclks, stfcamss->sys_clk);
	if (ret) {
		st_err(ST_CAMSS, "Failed to get clk controls\n");
		return ret;
	}

	stfcamss->nrsts = ARRAY_SIZE(stfcamss_resets);
	stfcamss->sys_rst = stfcamss_resets;

	ret = devm_reset_control_bulk_get_shared(dev, stfcamss->nrsts,
						 stfcamss->sys_rst);
	if (ret) {
		st_err(ST_CAMSS, "Failed to get reset controls\n");
		return ret;
	}

	ret = stfcamss_get_mem_res(pdev, stfcamss);
	if (ret) {
		st_err(ST_CAMSS, "Could not map registers\n");
		goto err_cam;
	}

	stfcamss->dev = dev;
	platform_set_drvdata(pdev, stfcamss);

	v4l2_async_nf_init(&stfcamss->notifier);

	num_subdevs = stfcamss_of_parse_ports(stfcamss);
	if (num_subdevs < 0) {
		ret = num_subdevs;
		goto err_cam_noti;
	}

	ret = stfcamss_init_subdevices(stfcamss);
	if (ret < 0) {
		st_err(ST_CAMSS, "Failed to init subdevice: %d\n", ret);
		goto err_cam_noti;
	}

	stfcamss->media_dev.dev = stfcamss->dev;
	strscpy(stfcamss->media_dev.model, "Starfive Camera Subsystem",
		sizeof(stfcamss->media_dev.model));
	strscpy(stfcamss->media_dev.serial, "0123456789ABCDEF",
		sizeof(stfcamss->media_dev.serial));
	snprintf(stfcamss->media_dev.bus_info,
		 sizeof(stfcamss->media_dev.bus_info),
		 "%s:%s", dev_bus_name(dev), pdev->name);
	stfcamss->media_dev.hw_revision = 0x01;
	stfcamss->media_dev.ops = &stfcamss_media_ops;
	media_device_init(&stfcamss->media_dev);

	stfcamss->v4l2_dev.mdev = &stfcamss->media_dev;

	ret = v4l2_device_register(stfcamss->dev, &stfcamss->v4l2_dev);
	if (ret < 0) {
		st_err(ST_CAMSS, "Failed to register V4L2 device: %d\n", ret);
		goto err_cam_noti_med;
	}

	ret = stfcamss_register_subdevices(stfcamss);
	if (ret < 0) {
		st_err(ST_CAMSS, "Failed to register subdevice: %d\n", ret);
		goto err_cam_noti_med_vreg;
	}

	if (num_subdevs) {
		stfcamss->notifier.ops = &stfcamss_subdev_notifier_ops;
		ret = v4l2_async_nf_register(&stfcamss->v4l2_dev,
					     &stfcamss->notifier);
		if (ret) {
			st_err(ST_CAMSS,
			       "Failed to register async subdev nodes: %d\n",
			       ret);
			goto err_cam_noti_med_vreg_sub;
		}
	} else {
		ret = v4l2_device_register_subdev_nodes(&stfcamss->v4l2_dev);
		if (ret < 0) {
			st_err(ST_CAMSS,
			       "Failed to register subdev nodes: %d\n",
			       ret);
			goto err_cam_noti_med_vreg_sub;
		}

		ret = media_device_register(&stfcamss->media_dev);
		if (ret < 0) {
			st_err(ST_CAMSS,
			       "Failed to register media device: %d\n",
			       ret);
			goto err_cam_noti_med_vreg_sub_medreg;
		}
	}

	dev_info(dev, "stfcamss probe success!\n");
	return 0;

err_cam_noti_med_vreg_sub_medreg:
err_cam_noti_med_vreg_sub:
	stfcamss_unregister_subdevices(stfcamss);
err_cam_noti_med_vreg:
	v4l2_device_unregister(&stfcamss->v4l2_dev);
err_cam_noti_med:
	media_device_cleanup(&stfcamss->media_dev);
err_cam_noti:
	v4l2_async_nf_cleanup(&stfcamss->notifier);
err_cam:
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

	dev_info(&pdev->dev, "remove done\n");

	stfcamss_unregister_subdevices(stfcamss);
	v4l2_device_unregister(&stfcamss->v4l2_dev);
	media_device_cleanup(&stfcamss->media_dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id stfcamss_of_match[] = {
	{ .compatible = "starfive,jh7110-camss" },
	{ /* end node */ },
};

MODULE_DEVICE_TABLE(of, stfcamss_of_match);

#ifdef CONFIG_PM_SLEEP
static int stfcamss_suspend(struct device *dev)
{
	struct stfcamss *stfcamss = dev_get_drvdata(dev);
	struct stf_vin_dev *vin_dev = stfcamss->vin_dev;
	struct media_entity *entity;
	struct media_pad *pad;
	struct v4l2_subdev *subdev;
	struct stfcamss_video *video;
	struct video_device *vdev;
	int i = 0;

	for (i = 0; i < VIN_LINE_MAX; i++) {
		if (vin_dev->line[i].stream_count) {
			vin_dev->line[i].stream_count++;
			video = &vin_dev->line[i].video_out;
			vdev = &vin_dev->line[i].video_out.vdev;
			entity = &vdev->entity;
			while (1) {
				pad = &entity->pads[0];
				if (!(pad->flags & MEDIA_PAD_FL_SINK))
					break;

				pad = media_pad_remote_pad_first(pad);
				if (!pad ||
				    !is_media_entity_v4l2_subdev(pad->entity))
					break;

				entity = pad->entity;
				subdev = media_entity_to_v4l2_subdev(entity);

				v4l2_subdev_call(subdev, video, s_stream, 0);
			}
			video_device_pipeline_stop(vdev);
			video->ops->flush_buffers(video, VB2_BUF_STATE_ERROR);
			v4l2_pipeline_pm_put(&vdev->entity);
		}
	}

	return pm_runtime_force_suspend(dev);
}

static int stfcamss_resume(struct device *dev)
{
	struct stfcamss *stfcamss = dev_get_drvdata(dev);
	struct stf_vin_dev *vin_dev = stfcamss->vin_dev;
	struct media_entity *entity;
	struct media_pad *pad;
	struct v4l2_subdev *subdev;
	struct stfcamss_video *video;
	struct video_device *vdev;
	int i = 0;
	int ret = 0;

	pm_runtime_force_resume(dev);

	for (i = 0; i < VIN_LINE_MAX; i++) {
		if (vin_dev->line[i].stream_count) {
			vin_dev->line[i].stream_count--;
			video = &vin_dev->line[i].video_out;
			vdev = &vin_dev->line[i].video_out.vdev;

			ret = v4l2_pipeline_pm_get(&vdev->entity);
			if (ret < 0)
				goto err;

			ret = video_device_pipeline_start(
				vdev, &video->stfcamss->pipe);
			if (ret < 0)
				goto err_pm_put;

			entity = &vdev->entity;
			while (1) {
				pad = &entity->pads[0];
				if (!(pad->flags & MEDIA_PAD_FL_SINK))
					break;

				pad = media_pad_remote_pad_first(pad);
				if (!pad ||
				    !is_media_entity_v4l2_subdev(pad->entity))
					break;

				entity = pad->entity;
				subdev = media_entity_to_v4l2_subdev(entity);

				ret = v4l2_subdev_call(subdev, video,
						       s_stream, 1);
				if (ret < 0 && ret != -ENOIOCTLCMD)
					goto err_pipeline_stop;
			}
		}
	}

	return 0;

err_pipeline_stop:
	video_device_pipeline_stop(vdev);
err_pm_put:
	v4l2_pipeline_pm_put(&vdev->entity);
err:
	return ret;
}
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM
static int stfcamss_runtime_suspend(struct device *dev)
{
	struct stfcamss *stfcamss = dev_get_drvdata(dev);

	reset_control_assert(stfcamss->sys_rst[STF_RST_ISP_TOP_AXI].rstc);
	reset_control_assert(stfcamss->sys_rst[STF_RST_ISP_TOP_N].rstc);
	clk_disable_unprepare(stfcamss->sys_clk[STF_CLK_ISP_AXI].clk);
	clk_disable_unprepare(stfcamss->sys_clk[STF_CLK_ISPCORE_2X].clk);

	return 0;
}

static int stfcamss_runtime_resume(struct device *dev)
{
	struct stfcamss *stfcamss = dev_get_drvdata(dev);

	clk_prepare_enable(stfcamss->sys_clk[STF_CLK_ISPCORE_2X].clk);
	clk_prepare_enable(stfcamss->sys_clk[STF_CLK_ISP_AXI].clk);
	reset_control_deassert(stfcamss->sys_rst[STF_RST_ISP_TOP_N].rstc);
	reset_control_deassert(stfcamss->sys_rst[STF_RST_ISP_TOP_AXI].rstc);

	return 0;
}
#endif /* CONFIG_PM */

static const struct dev_pm_ops stfcamss_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(stfcamss_suspend, stfcamss_resume)
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
