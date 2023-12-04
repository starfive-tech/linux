// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_blend.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_plane.h>
#include <drm/drm_plane_helper.h>

#include "vs_plane.h"
#include "vs_drv.h"
#include "vs_dc.h"

static void vs_plane_atomic_destroy_state(struct drm_plane *plane,
					  struct drm_plane_state *state)
{
	struct vs_plane_state *vs_plane_state = to_vs_plane_state(state);

	__drm_atomic_helper_plane_destroy_state(state);

	kfree(vs_plane_state);
}

static void vs_plane_reset(struct drm_plane *plane)
{
	struct vs_plane_state *state;
	struct vs_plane *vs_plane = to_vs_plane(plane);

	if (plane->state)
		vs_plane_atomic_destroy_state(plane, plane->state);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return;

	state->base.zpos = vs_plane->id;
	__drm_atomic_helper_plane_reset(plane, &state->base);
}

static struct drm_plane_state *
vs_plane_atomic_duplicate_state(struct drm_plane *plane)
{
	struct vs_plane_state *old_state;
	struct vs_plane_state *state;

	if (WARN_ON(!plane->state))
		return NULL;

	old_state = to_vs_plane_state(plane->state);
	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &state->base);

	return &state->base;
}

static bool vs_format_mod_supported(struct drm_plane *plane,
				    u32 format,
				    u64 modifier)
{
	int i;

	/* We always have to allow these modifiers:
	 * 1. Core DRM checks for LINEAR support if userspace does not provide modifiers.
	 * 2. Not passing any modifiers is the same as explicitly passing INVALID.
	 */
	if (modifier == DRM_FORMAT_MOD_LINEAR)
		return true;

	/* Check that the modifier is on the list of the plane's supported modifiers. */
	for (i = 0; i < plane->modifier_count; i++) {
		if (modifier == plane->modifiers[i])
			break;
	}

	if (i == plane->modifier_count)
		return false;

	return true;
}

const struct drm_plane_funcs vs_plane_funcs = {
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.reset			= vs_plane_reset,
	.atomic_duplicate_state = vs_plane_atomic_duplicate_state,
	.atomic_destroy_state	= vs_plane_atomic_destroy_state,
	.format_mod_supported	= vs_format_mod_supported,
};

static unsigned char vs_get_plane_number(struct drm_framebuffer *fb)
{
	const struct drm_format_info *info;

	if (!fb)
		return 0;

	info = drm_format_info(fb->format->format);
	if (!info || info->num_planes > DRM_FORMAT_MAX_PLANES)
		return 0;

	return info->num_planes;
}

static int vs_plane_atomic_check(struct drm_plane *plane,
				 struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
										 plane);
	unsigned char i, num_planes;
	struct drm_framebuffer *fb = new_plane_state->fb;
	struct drm_crtc *crtc = new_plane_state->crtc;
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	struct vs_dc *dc = dev_get_drvdata(vs_crtc->dev);
	struct vs_plane_state *plane_state = to_vs_plane_state(new_plane_state);

	if (!crtc || !fb)
		return 0;

	num_planes = vs_get_plane_number(fb);

	for (i = 0; i < num_planes; i++) {
		dma_addr_t dma_addr;

		dma_addr = drm_fb_dma_get_gem_addr(fb, new_plane_state, i);
		plane_state->dma_addr[i] = dma_addr;
	}

	return vs_dc_check_plane(dc, plane, state);
}

static int vs_cursor_plane_atomic_check(struct drm_plane *plane,
					struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
									  plane);
	unsigned char i, num_planes;
	struct drm_framebuffer *fb = new_plane_state->fb;
	struct drm_crtc *crtc = new_plane_state->crtc;
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	struct vs_dc *dc = dev_get_drvdata(vs_crtc->dev);
	struct vs_plane_state *plane_state = to_vs_plane_state(new_plane_state);

	if (!crtc || !fb)
		return 0;

	num_planes = vs_get_plane_number(fb);

	for (i = 0; i < num_planes; i++) {
		dma_addr_t dma_addr;

		dma_addr = drm_fb_dma_get_gem_addr(fb, new_plane_state, i);
		plane_state->dma_addr[i] = dma_addr;
	}

	return vs_dc_check_cursor_plane(dc, plane, state);
}

static void vs_plane_atomic_update(struct drm_plane *plane,
				   struct drm_atomic_state *state)
{
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state,
									  plane);
	struct drm_plane_state *old_state = drm_atomic_get_old_plane_state(state,
									  plane);

	unsigned char i, num_planes;
	struct drm_framebuffer *fb;
	struct vs_plane *vs_plane = to_vs_plane(plane);
	struct vs_crtc *vs_crtc = to_vs_crtc(new_state->crtc);
	struct vs_plane_state *plane_state = to_vs_plane_state(new_state);
	struct vs_dc *dc = dev_get_drvdata(vs_crtc->dev);

	if (!new_state->fb || !new_state->crtc)
		return;

	fb = new_state->fb;

	drm_fb_dma_sync_non_coherent(fb->dev, old_state, new_state);

	num_planes = vs_get_plane_number(fb);

	for (i = 0; i < num_planes; i++) {
		dma_addr_t dma_addr;

		dma_addr = drm_fb_dma_get_gem_addr(fb, new_state, i);
		plane_state->dma_addr[i] = dma_addr;
	}

	vs_dc_update_plane(dc, vs_plane, plane, state);
}

static void vs_cursor_plane_atomic_update(struct drm_plane *plane,
					  struct drm_atomic_state *state)
{
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state,
									   plane);
	struct drm_plane_state *old_state = drm_atomic_get_old_plane_state(state,
									   plane);
	unsigned char i, num_planes;
	struct drm_framebuffer *fb;
	struct vs_plane *vs_plane = to_vs_plane(plane);
	struct vs_crtc *vs_crtc = to_vs_crtc(new_state->crtc);
	struct vs_plane_state *plane_state = to_vs_plane_state(new_state);
	struct vs_dc *dc = dev_get_drvdata(vs_crtc->dev);

	if (!new_state->fb || !new_state->crtc)
		return;

	fb = new_state->fb;
	drm_fb_dma_sync_non_coherent(fb->dev, old_state, new_state);

	num_planes = vs_get_plane_number(fb);

	for (i = 0; i < num_planes; i++) {
		dma_addr_t dma_addr;

		dma_addr = drm_fb_dma_get_gem_addr(fb, new_state, i);
		plane_state->dma_addr[i] = dma_addr;
	}

	vs_dc_update_cursor_plane(dc, vs_plane, plane, state);
}

static void vs_plane_atomic_disable(struct drm_plane *plane,
				    struct drm_atomic_state *state)
{
	struct drm_plane_state *old_state = drm_atomic_get_old_plane_state(state,
									   plane);
	struct vs_plane *vs_plane = to_vs_plane(plane);
	struct vs_crtc *vs_crtc = to_vs_crtc(old_state->crtc);
	struct vs_dc *dc = dev_get_drvdata(vs_crtc->dev);

	vs_dc_disable_plane(dc, vs_plane, old_state);
}

static void vs_cursor_plane_atomic_disable(struct drm_plane *plane,
					   struct drm_atomic_state *state)
{
	struct drm_plane_state *old_state = drm_atomic_get_old_plane_state(state,
									   plane);
	struct vs_plane *vs_plane = to_vs_plane(plane);
	struct vs_crtc *vs_crtc = to_vs_crtc(old_state->crtc);
	struct vs_dc *dc = dev_get_drvdata(vs_crtc->dev);

	vs_dc_disable_cursor_plane(dc, vs_plane, old_state);
}

const struct drm_plane_helper_funcs primary_plane_helpers = {
	.atomic_check	= vs_plane_atomic_check,
	.atomic_update	= vs_plane_atomic_update,
	.atomic_disable = vs_plane_atomic_disable,
};

const struct drm_plane_helper_funcs overlay_plane_helpers = {
	.atomic_check	= vs_plane_atomic_check,
	.atomic_update	= vs_plane_atomic_update,
	.atomic_disable = vs_plane_atomic_disable,
};

const struct drm_plane_helper_funcs cursor_plane_helpers = {
	.atomic_check	= vs_cursor_plane_atomic_check,
	.atomic_update	= vs_cursor_plane_atomic_update,
	.atomic_disable = vs_cursor_plane_atomic_disable,
};

struct vs_plane *vs_plane_create(struct drm_device *drm_dev,
				 struct vs_plane_info *info,
				 unsigned int layer_num,
				 unsigned int possible_crtcs)
{
	struct vs_plane *plane;

	if (!info)
		return NULL;

	plane = drmm_universal_plane_alloc(drm_dev, struct vs_plane, base,
					   possible_crtcs,
					   &vs_plane_funcs,
					   info->formats, info->num_formats,
					   info->modifiers, info->type,
					   info->name ? info->name : NULL);
	if (IS_ERR(plane))
		return ERR_CAST(plane);

	if (info->type == DRM_PLANE_TYPE_PRIMARY)
		drm_plane_helper_add(&plane->base, &primary_plane_helpers);
	else if (info->type == DRM_PLANE_TYPE_CURSOR)
		drm_plane_helper_add(&plane->base, &cursor_plane_helpers);
	else
		drm_plane_helper_add(&plane->base, &overlay_plane_helpers);

	return plane;
}
