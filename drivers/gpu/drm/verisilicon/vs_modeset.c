// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#include <linux/module.h>
#include <linux/version.h>

#include <drm/drm_damage_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_dma_helper.h>

#include "vs_modeset.h"
#include "vs_gem.h"

#define fourcc_mod_vs_get_type(val) \
	(((val) & DRM_FORMAT_MOD_VS_TYPE_MASK) >> 54)

struct vs_gem_object *vs_fb_get_gem_obj(struct drm_framebuffer *fb,
					unsigned char plane)
{
	if (plane > DRM_FORMAT_MAX_PLANES)
		return NULL;

	return to_vs_gem_object(fb->obj[plane]);
}

static const struct drm_format_info vs_formats[] = {
	{.format = DRM_FORMAT_NV12, .depth = 0, .num_planes = 2, .char_per_block = { 20, 40, 0 },
	 .block_w = { 4, 4, 0 }, .block_h = { 4, 4, 0 }, .hsub = 2, .vsub = 2, .is_yuv = true},
	{.format = DRM_FORMAT_YUV444, .depth = 0, .num_planes = 3, .char_per_block = { 20, 20, 20 },
	 .block_w = { 4, 4, 4 }, .block_h = { 4, 4, 4 }, .hsub = 1, .vsub = 1, .is_yuv = true},
};

static const struct drm_format_info *
vs_lookup_format_info(const struct drm_format_info formats[],
		      int num_formats, u32 format)
{
	int i;

	for (i = 0; i < num_formats; i++) {
		if (formats[i].format == format)
			return &formats[i];
	}

	return NULL;
}

static const struct drm_format_info *
vs_get_format_info(const struct drm_mode_fb_cmd2 *cmd)
{
	if (fourcc_mod_vs_get_type(cmd->modifier[0]) ==
		DRM_FORMAT_MOD_VS_TYPE_CUSTOM_10BIT)
		return vs_lookup_format_info(vs_formats, ARRAY_SIZE(vs_formats),
									 cmd->pixel_format);
	else
		return NULL;
}

static const struct drm_mode_config_funcs vs_mode_config_funcs = {
	.fb_create			 = drm_gem_fb_create,
	.get_format_info	 = vs_get_format_info,
	.output_poll_changed = drm_fb_helper_output_poll_changed,
	.atomic_check		 = drm_atomic_helper_check,
	.atomic_commit		 = drm_atomic_helper_commit,
};

static struct drm_mode_config_helper_funcs vs_mode_config_helpers = {
	.atomic_commit_tail = drm_atomic_helper_commit_tail_rpm,
};

void vs_mode_config_init(struct drm_device *dev)
{
	drm_mode_config_init(dev);
	dev->mode_config.fb_modifiers_not_supported = false;

	if (dev->mode_config.max_width == 0 ||
	    dev->mode_config.max_height == 0) {
		dev->mode_config.min_width  = 0;
		dev->mode_config.min_height = 0;
		dev->mode_config.max_width  = 4096;
		dev->mode_config.max_height = 4096;
	}
	dev->mode_config.funcs = &vs_mode_config_funcs;
	dev->mode_config.helper_private = &vs_mode_config_helpers;
}
