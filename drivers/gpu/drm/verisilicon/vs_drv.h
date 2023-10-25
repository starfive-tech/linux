/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_DRV_H__
#define __VS_DRV_H__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem.h>
#include <drm/drm_managed.h>

/*@pitch_alignment: buffer pitch alignment required by sub-devices.*/
struct vs_drm_device {
	struct drm_device base;
	unsigned int pitch_alignment;
};

static inline struct vs_drm_device *
to_vs_drm_private(const struct drm_device *dev)
{
	return container_of(dev, struct vs_drm_device, base);
}

#ifdef CONFIG_DRM_VERISILICON_STARFIVE_HDMI
extern struct platform_driver starfive_hdmi_driver;
#endif

#endif /* __VS_DRV_H__ */
