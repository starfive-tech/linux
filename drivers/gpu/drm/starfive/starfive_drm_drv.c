// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-mmsys.h>
#include <linux/dma-mapping.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include "starfive_drm_drv.h"
#include "starfive_drm_gem.h"

#define DRIVER_NAME	"starfive"
#define DRIVER_DESC	"starfive Soc DRM"
#define DRIVER_DATE	"20210519"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

static struct drm_framebuffer *
starfive_drm_mode_fb_create(struct drm_device *dev, struct drm_file *file,
			    const struct drm_mode_fb_cmd2 *mode_cmd)
{
	return drm_gem_fb_create(dev, file, mode_cmd);
}

static const struct drm_mode_config_funcs starfive_drm_mode_config_funcs = {
	.fb_create = starfive_drm_mode_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static const struct drm_mode_config_helper_funcs starfive_drm_mode_config_helpers = {
	.atomic_commit_tail = drm_atomic_helper_commit_tail_rpm,
};

static const struct file_operations starfive_drm_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.mmap = starfive_drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
	.unlocked_ioctl = drm_ioctl,
	.compat_ioctl = drm_compat_ioctl,
	.release = drm_release,
};

static struct drm_driver starfive_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,
	.dumb_create = starfive_drm_gem_dumb_create,
	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_import_sg_table = starfive_drm_gem_prime_import_sg_table,
	.gem_prime_mmap = starfive_drm_gem_mmap_buf,
	.fops = &starfive_drm_driver_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
};

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static void starfive_drm_match_add(struct device *dev,
				   struct component_match **match,
				   struct platform_driver *const *drivers,
				   int count)
{
	int i;

	for (i = 0; i < count; i++) {
		struct device_driver *drv = &drivers[i]->driver;
		struct device *p = NULL, *d;

		while ((d = platform_find_device_by_driver(p, drv))) {
			put_device(p);
			component_match_add(dev, match, compare_dev, d);
			p = d;
		}
		put_device(p);
	}
}

static void starfive_cleanup(struct drm_device *ddev)
{
	struct starfive_drm_private *private = ddev->dev_private;

	drm_kms_helper_poll_fini(ddev);
	drm_atomic_helper_shutdown(ddev);
	drm_mode_config_cleanup(ddev);
	component_unbind_all(ddev->dev, ddev);
	kfree(private);
	ddev->dev_private = NULL;
}

static int starfive_drm_bind(struct device *dev)
{
	struct drm_device *drm_dev;
	struct starfive_drm_private *private;
	int ret;

	drm_dev = drm_dev_alloc(&starfive_drm_driver, dev);
	if (IS_ERR(drm_dev))
		return PTR_ERR(drm_dev);

	dev_set_drvdata(dev, drm_dev);

	private = devm_kzalloc(drm_dev->dev, sizeof(*private), GFP_KERNEL);
	if (!private) {
		ret = -ENOMEM;
		goto err_free;
	}

	drm_dev->dev_private = private;

	/*
	ret = starfive_drm_init_iommu(drm_dev);
	if (ret)
		goto err_free;
	*/

	ret = drmm_mode_config_init(drm_dev);
	if (ret)
		goto err_free;

	drm_dev->mode_config.min_width = 64;
	drm_dev->mode_config.min_height = 64;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	drm_dev->mode_config.max_width = 4096;
	drm_dev->mode_config.max_height = 4096;
	drm_dev->mode_config.funcs = &starfive_drm_mode_config_funcs;
	drm_dev->mode_config.helper_private = &starfive_drm_mode_config_helpers;
	drm_dev->mode_config.async_page_flip = 1;

	ret = component_bind_all(dev, drm_dev);
	if (ret)
		goto err_free;

	ret = drm_vblank_init(drm_dev, drm_dev->mode_config.num_crtc);
	if (ret)
		goto err_cleanup;

	drm_mode_config_reset(drm_dev);

	/* init kms poll for handling hpd */
	drm_kms_helper_poll_init(drm_dev);

	ret = drm_dev_register(drm_dev, 0);
	if (ret)
		goto err_cleanup;
#ifdef CONFIG_FRAMEBUFFER_CONSOLE
	drm_fbdev_generic_setup(drm_dev, 16);
#endif
	return 0;

err_cleanup:
	starfive_cleanup(drm_dev);
err_free:
	drm_dev_put(drm_dev);
	return ret;
}

static void starfive_drm_unbind(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	drm_dev_unregister(drm_dev);
}

static const struct component_master_ops starfive_drm_ops = {
	.bind = starfive_drm_bind,
	.unbind = starfive_drm_unbind,
};

static struct platform_driver * const starfive_component_drivers[] = {
	&starfive_crtc_driver,
#ifdef CONFIG_DRM_STARFIVE_MIPI_DSI
	&starfive_dsi_platform_driver,
#endif
	&starfive_encoder_driver,
};

static int starfive_drm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct component_match *match = NULL;

	starfive_drm_match_add(dev, &match,
			       starfive_component_drivers,
			       ARRAY_SIZE(starfive_component_drivers));
	if (IS_ERR(match))
		return PTR_ERR(match);

	return component_master_add_with_match(dev, &starfive_drm_ops, match);
}

static int starfive_drm_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &starfive_drm_ops);
	return 0;
}

static const struct of_device_id starfive_drm_dt_ids[] = {
	{ .compatible = "starfive,display-subsystem" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, starfive_drm_dt_ids);

static struct platform_driver starfive_drm_platform_driver = {
	.probe	= starfive_drm_probe,
	.remove	= starfive_drm_remove,
	.driver	= {
		.name		= "starfive-drm",
		.of_match_table	= starfive_drm_dt_ids,
		//.pm     = &starfive_drm_pm_ops,
	},
};

static int __init starfive_drm_init(void)
{
	int ret;

	ret = platform_register_drivers(starfive_component_drivers,
					ARRAY_SIZE(starfive_component_drivers));
	if (ret)
		return ret;

	return platform_driver_register(&starfive_drm_platform_driver);
}

static void __exit starfive_drm_exit(void)
{
	platform_unregister_drivers(starfive_component_drivers,
				    ARRAY_SIZE(starfive_component_drivers));
	platform_driver_unregister(&starfive_drm_platform_driver);
}

module_init(starfive_drm_init);
module_exit(starfive_drm_exit);

MODULE_AUTHOR("StarFive <StarFive@starfivetech.com>");
MODULE_DESCRIPTION("StarFive SoC DRM driver");
MODULE_LICENSE("GPL v2");
