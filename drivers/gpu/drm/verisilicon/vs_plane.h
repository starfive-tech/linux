/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_PLANE_H__
#define __VS_PLANE_H__

#include <drm/drm_fourcc.h>
#include <drm/drm_plane_helper.h>

#include "vs_modeset.h"
#include "vs_type.h"

struct vs_plane;

struct vs_plane_status {
	u32 tile_mode;
	struct drm_rect src;
	struct drm_rect dest;
};

struct vs_plane_state {
	struct drm_plane_state base;
	struct vs_plane_status status; /* for debugfs */

	struct drm_property_blob *watermark;
	struct drm_property_blob *color_mgmt;
	struct drm_property_blob *roi;

	u32 degamma;
	bool degamma_changed;
};

struct vs_plane {
	struct drm_plane base;
	u8 id;
	dma_addr_t dma_addr[DRM_FORMAT_MAX_PLANES];

	struct drm_property *degamma_mode;
	struct drm_property *watermark_prop;
	struct drm_property *color_mgmt_prop;
	struct drm_property *roi_prop;

	const struct vs_plane_funcs *funcs;
};

void vs_plane_destory(struct drm_plane *plane);

struct vs_plane *vs_plane_create(struct drm_device *drm_dev,
				 struct vs_plane_info *info,
				 unsigned int layer_num,
				 unsigned int possible_crtcs);

static inline struct vs_plane *to_vs_plane(struct drm_plane *plane)
{
	return container_of(plane, struct vs_plane, base);
}

static inline struct vs_plane_state *
to_vs_plane_state(struct drm_plane_state *state)
{
	return container_of(state, struct vs_plane_state, base);
}
#endif /* __VS_PLANE_H__ */
