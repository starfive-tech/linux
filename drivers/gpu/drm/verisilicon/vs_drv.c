// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/of_graph.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/version.h>

#include <drm/drm_aperture.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fbdev_generic.h>
#include <drm/drm_file.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_of.h>
#include <drm/drm_prime.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "vs_drv.h"
#include "vs_modeset.h"
#include "vs_gem.h"
#include "vs_dc.h"

#define DRV_NAME	"starfive"
#define DRV_DESC	"Starfive DRM driver"
#define DRV_DATE	"202305161"
#define DRV_MAJOR	1
#define DRV_MINOR	0

static struct platform_driver vs_drm_platform_driver;

static const struct file_operations fops = {
	.owner			= THIS_MODULE,
	.open			= drm_open,
	.release		= drm_release,
	.unlocked_ioctl	= drm_ioctl,
	.compat_ioctl	= drm_compat_ioctl,
	.poll			= drm_poll,
	.read			= drm_read,
	.mmap			= drm_gem_mmap,
};

static struct drm_driver vs_drm_driver = {
	.driver_features	= DRIVER_MODESET | DRIVER_ATOMIC | DRIVER_GEM,
	.lastclose		= drm_fb_helper_lastclose,
	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_import_sg_table = vs_gem_prime_import_sg_table,
	.gem_prime_mmap		= drm_gem_prime_mmap,
	.dumb_create		= vs_gem_dumb_create,
	.fops			= &fops,
	.name			= DRV_NAME,
	.desc			= DRV_DESC,
	.date			= DRV_DATE,
	.major			= DRV_MAJOR,
	.minor			= DRV_MINOR,
};

void vs_drm_update_pitch_alignment(struct drm_device *drm_dev,
				   unsigned int alignment)
{
	struct vs_drm_private *priv = to_vs_dev(drm_dev);

	if (alignment > priv->pitch_alignment)
		priv->pitch_alignment = alignment;
}

static int vs_drm_bind(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct vs_drm_private *priv;
	int ret;
	static u64 dma_mask = DMA_BIT_MASK(40);
	struct drm_device *drm_dev;

	/* Remove existing drivers that may own the framebuffer memory. */
	ret = drm_aperture_remove_framebuffers(&vs_drm_driver);
	if (ret) {
		dev_err(dev,
			    "Failed to remove existing framebuffers - %d.\n",
			    ret);
		return ret;
	}

	priv = devm_drm_dev_alloc(dev, &vs_drm_driver, struct vs_drm_private, base);
		if (IS_ERR(priv))
			return PTR_ERR(priv);

	priv->pitch_alignment = 64;
	priv->dma_dev = priv->base.dev;
	priv->dma_dev->coherent_dma_mask = dma_mask;
	drm_dev = &priv->base;
	platform_set_drvdata(pdev, drm_dev);

	vs_mode_config_init(drm_dev);

	/* Now try and bind all our sub-components */
	ret = component_bind_all(dev, drm_dev);
	if (ret)
		goto err_mode;

	ret = drm_vblank_init(drm_dev, drm_dev->mode_config.num_crtc);
	if (ret)
		goto err_bind;

	drm_mode_config_reset(drm_dev);

	drm_kms_helper_poll_init(drm_dev);

	ret = drm_dev_register(drm_dev, 0);
	if (ret)
		goto err_helper;

	drm_fbdev_generic_setup(drm_dev, 32);

	return 0;

err_helper:
	drm_kms_helper_poll_fini(drm_dev);
err_bind:
	component_unbind_all(drm_dev->dev, drm_dev);
err_mode:
	drm_mode_config_cleanup(drm_dev);

	return ret;
}

static void vs_drm_unbind(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	drm_dev_unregister(drm_dev);

	drm_kms_helper_poll_fini(drm_dev);

	component_unbind_all(drm_dev->dev, drm_dev);
}

static const struct component_master_ops vs_drm_ops = {
	.bind = vs_drm_bind,
	.unbind = vs_drm_unbind,
};

static struct platform_driver *drm_sub_drivers[] = {
	&dc_platform_driver,

	/* connector + encoder*/
#ifdef CONFIG_STARFIVE_HDMI
	&starfive_hdmi_driver,
#endif

};

#define NUM_DRM_DRIVERS \
	(sizeof(drm_sub_drivers) / sizeof(struct platform_driver *))

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static struct component_match *vs_drm_match_add(struct device *dev)
{
	struct component_match *match = NULL;
	int i;

	for (i = 0; i < NUM_DRM_DRIVERS; ++i) {
		struct platform_driver *drv = drm_sub_drivers[i];
		struct device *p = NULL, *d;

		while ((d = platform_find_device_by_driver(p, &drv->driver))) {
			put_device(p);

			component_match_add(dev, &match, compare_dev, d);
			p = d;
		}
		put_device(p);
	}

	return match ?: ERR_PTR(-ENODEV);
}

static int vs_drm_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct component_match *match;

	match = vs_drm_match_add(dev);
	if (IS_ERR(match))
		return PTR_ERR(match);

	return component_master_add_with_match(dev, &vs_drm_ops, match);
}

static int vs_drm_platform_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &vs_drm_ops);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int vs_drm_suspend(struct device *dev)
{
	return drm_mode_config_helper_suspend(dev_get_drvdata(dev));
}

static int vs_drm_resume(struct device *dev)
{
	drm_mode_config_helper_resume(dev_get_drvdata(dev));

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(vs_drm_pm_ops, vs_drm_suspend, vs_drm_resume);

static const struct of_device_id vs_drm_dt_ids[] = {
	{ .compatible = "starfive,display-subsystem", },
	{ },
};

MODULE_DEVICE_TABLE(of, vs_drm_dt_ids);

static struct platform_driver vs_drm_platform_driver = {
	.probe = vs_drm_platform_probe,
	.remove = vs_drm_platform_remove,

	.driver = {
		.name = DRV_NAME,
		.of_match_table = vs_drm_dt_ids,
		.pm = &vs_drm_pm_ops,
	},
};

static int __init vs_drm_init(void)
{
	int ret;

	ret = platform_register_drivers(drm_sub_drivers, NUM_DRM_DRIVERS);
	if (ret)
		return ret;

	ret = platform_driver_register(&vs_drm_platform_driver);
	if (ret)
		platform_unregister_drivers(drm_sub_drivers, NUM_DRM_DRIVERS);

	return ret;
}

static void __exit vs_drm_fini(void)
{
	platform_driver_unregister(&vs_drm_platform_driver);
	platform_unregister_drivers(drm_sub_drivers, NUM_DRM_DRIVERS);
}

module_init(vs_drm_init);
module_exit(vs_drm_fini);

MODULE_DESCRIPTION("VeriSilicon DRM Driver");
MODULE_LICENSE("GPL");
