/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_CRTC_H__
#define __VS_CRTC_H__

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include "vs_type.h"

struct vs_crtc_state {
	struct drm_crtc_state base;

	u32 output_fmt;
	u8 encoder_type;
	u8 bpp;
	bool underflow;
};

struct vs_crtc {
	struct drm_crtc base;
	struct device *dev;
	unsigned int max_bpc;
	unsigned int color_formats;
};

struct vs_crtc *vs_crtc_create(struct drm_device *drm_dev,
			       struct vs_dc_info *info);

static inline struct vs_crtc *to_vs_crtc(struct drm_crtc *crtc)
{
	return container_of(crtc, struct vs_crtc, base);
}

static inline struct vs_crtc_state *
to_vs_crtc_state(struct drm_crtc_state *state)
{
	return container_of(state, struct vs_crtc_state, base);
}
#endif /* __VS_CRTC_H__ */
