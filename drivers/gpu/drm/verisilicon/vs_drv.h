/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_DRV_H__
#define __VS_DRV_H__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem.h>
#include <drm/drm_managed.h>

/*
 *
 * @dma_dev: device for DMA API.
 *	- use the first attached device if support iommu
	else use drm device (only contiguous buffer support)
 * @domain: iommu domain for DRM.
 *	- all DC IOMMU share same domain to reduce mapping
 * @pitch_alignment: buffer pitch alignment required by sub-devices.
 *
 */
struct vs_drm_private {
	struct drm_device base;
	struct device *dma_dev;
	struct iommu_domain *domain;
	unsigned int pitch_alignment;
};

static inline struct vs_drm_private *
to_vs_dev(const struct drm_device *dev)
{
	return container_of(dev, struct vs_drm_private, base);
}

void vs_drm_update_pitch_alignment(struct drm_device *drm_dev,
				   unsigned int alignment);


static inline bool is_iommu_enabled(struct drm_device *dev)
{
	struct vs_drm_private *priv = to_vs_dev(dev);

	return priv->domain ? true : false;
}

#ifdef CONFIG_STARFIVE_HDMI
extern struct platform_driver starfive_hdmi_driver;
#endif

#endif /* __VS_DRV_H__ */
