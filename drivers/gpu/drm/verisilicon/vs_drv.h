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
#include <linux/clk.h>
#include <linux/reset.h>

enum rst_vout {
	RST_VOUT_AXI = 0,
	RST_VOUT_AHB,
	RST_VOUT_CORE,
	RST_VOUT_NUM
};

/*@pitch_alignment: buffer pitch alignment required by sub-devices.*/
struct vs_drm_device {
	struct drm_device base;
	unsigned int pitch_alignment;
	/* clocks */
	unsigned int clk_count;
	struct clk **clks;

	struct reset_control_bulk_data rst_vout[RST_VOUT_NUM];
	int	nrsts;
};

static inline struct vs_drm_device *
to_vs_drm_private(const struct drm_device *dev)
{
	return container_of(dev, struct vs_drm_device, base);
}

#endif /* __VS_DRV_H__ */
