// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#include <linux/component.h>
#include <linux/pm_runtime.h>

#include <drm/drm_aperture.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fbdev_generic.h>
#include <drm/drm_file.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_module.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "vs_drv.h"
#include "vs_modeset.h"
#include "vs_dc.h"

#define DRV_NAME	"verisilicon"
#define DRV_DESC	"Verisilicon DRM driver"
#define DRV_DATE	"20230516"
#define DRV_MAJOR	1
#define DRV_MINOR	0

static int vs_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
			      struct drm_mode_create_dumb *args)
{
	struct vs_drm_device *priv = to_vs_drm_private(dev);

	unsigned int pitch = DIV_ROUND_UP(args->width * args->bpp, 8);

	if (args->bpp % 10)
		args->pitch = ALIGN(pitch, priv->pitch_alignment);
	else
		/* for costum 10bit format with no bit gaps */
		args->pitch = pitch;

	return drm_gem_dma_dumb_create_internal(file, dev, args);
}

DEFINE_DRM_GEM_FOPS(vs_drm_fops);

static struct drm_driver vs_drm_driver = {
	.driver_features	= DRIVER_MODESET | DRIVER_ATOMIC | DRIVER_GEM,

	DRM_GEM_DMA_DRIVER_OPS_WITH_DUMB_CREATE(vs_gem_dumb_create),

	.fops			= &vs_drm_fops,
	.name			= DRV_NAME,
	.desc			= DRV_DESC,
	.date			= DRV_DATE,
	.major			= DRV_MAJOR,
	.minor			= DRV_MINOR,
};

static int vs_drm_bind(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct vs_drm_device *priv;
	int ret;
	struct drm_device *drm_dev;

	/* Remove existing drivers that may own the framebuffer memory. */
	ret = drm_aperture_remove_framebuffers(&vs_drm_driver);
	if (ret)
		return ret;

	priv = devm_drm_dev_alloc(dev, &vs_drm_driver, struct vs_drm_device, base);
	if (IS_ERR(priv))
		return PTR_ERR(priv);

	priv->pitch_alignment = 64;

	ret = dma_set_coherent_mask(priv->base.dev, DMA_BIT_MASK(40));
	if (ret)
		return ret;

	drm_dev = &priv->base;
	platform_set_drvdata(pdev, drm_dev);

	vs_mode_config_init(drm_dev);

	/* Now try and bind all our sub-components */
	ret = component_bind_all(dev, drm_dev);
	if (ret)
		return ret;

	ret = drm_vblank_init(drm_dev, drm_dev->mode_config.num_crtc);
	if (ret)
		return ret;

	drm_mode_config_reset(drm_dev);

	drm_kms_helper_poll_init(drm_dev);

	ret = drm_dev_register(drm_dev, 0);
	if (ret)
		return ret;

	drm_fbdev_generic_setup(drm_dev, 32);

	return 0;
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
#ifdef CONFIG_DRM_VERISILICON_STARFIVE_HDMI
	&starfive_hdmi_driver,
#endif

};

static struct component_match *vs_drm_match_add(struct device *dev)
{
	struct component_match *match = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(drm_sub_drivers); ++i) {
		struct platform_driver *drv = drm_sub_drivers[i];
		struct device *p = NULL, *d;

		while ((d = platform_find_device_by_driver(p, &drv->driver))) {
			put_device(p);

			component_match_add(dev, &match, component_compare_dev, d);
			p = d;
		}
		put_device(p);
	}

	return match ? match : ERR_PTR(-ENODEV);
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

	ret = platform_register_drivers(drm_sub_drivers, ARRAY_SIZE(drm_sub_drivers));
	if (ret)
		return ret;

	ret = drm_platform_driver_register(&vs_drm_platform_driver);
	if (ret)
		platform_unregister_drivers(drm_sub_drivers, ARRAY_SIZE(drm_sub_drivers));

	return ret;
}

static void __exit vs_drm_fini(void)
{
	platform_driver_unregister(&vs_drm_platform_driver);
	platform_unregister_drivers(drm_sub_drivers, ARRAY_SIZE(drm_sub_drivers));
}

module_init(vs_drm_init);
module_exit(vs_drm_fini);

MODULE_DESCRIPTION("VeriSilicon DRM Driver");
MODULE_LICENSE("GPL");
