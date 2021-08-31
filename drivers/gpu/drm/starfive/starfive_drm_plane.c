// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#include <drm/drm.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_atomic_helper.h>
#include "starfive_drm_crtc.h"
#include "starfive_drm_plane.h"
#include "starfive_drm_gem.h"
#include "starfive_drm_lcdc.h"
#include "starfive_drm_vpp.h"

static const u32 formats[] = {
	DRM_FORMAT_RGB565,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YVYU,

	DRM_FORMAT_YUV420,
	DRM_FORMAT_NV21,
	DRM_FORMAT_NV12,

	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
};

static void starfive_plane_destroy(struct drm_plane *plane)
{
	drm_plane_cleanup(plane);
}

static const struct drm_plane_funcs starfive_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = starfive_plane_destroy,
	.set_property = NULL,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static void starfive_plane_atomic_disable(struct drm_plane *plane,
					  struct drm_atomic_state *old_state)
{
}

static int starfive_plane_atomic_check(struct drm_plane *plane,
				       struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state =
		drm_atomic_get_new_plane_state(state, plane);
	struct drm_framebuffer *fb = new_plane_state->fb;
	struct drm_crtc_state *crtc_state;

	if (!fb)
		return 0;

	if (WARN_ON(!new_plane_state->crtc))
		return 0;

	/*
	ret = starfive_drm_plane_check(state->crtc, plane,
				       to_starfive_plane_state(state));
	if (ret)
		return ret;
	*/

	//crtc_state = drm_atomic_get_crtc_state(new_plane_state->state, new_plane_state->crtc);
	crtc_state = drm_atomic_get_crtc_state(state, new_plane_state->crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	return drm_atomic_helper_check_plane_state(new_plane_state, crtc_state,
				DRM_PLANE_HELPER_NO_SCALING,
				DRM_PLANE_HELPER_NO_SCALING,
				true, true);
}

static void starfive_plane_atomic_update(struct drm_plane *plane,
					 struct drm_atomic_state *old_state)
{
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(old_state,
										plane);
	struct drm_crtc *crtc = new_state->crtc;
	struct drm_framebuffer *fb = new_state->fb;
	//struct drm_plane_state *state = plane->state;
	//struct drm_crtc *crtc = state->crtc;
	//struct drm_framebuffer *fb = state->fb;

	dma_addr_t dma_addr;
	struct drm_gem_object *obj;
	struct starfive_drm_gem_obj *starfive_obj;
	unsigned int pitch, format;

	struct starfive_crtc *sf_crtc = to_starfive_crtc(crtc);

	if (!crtc || WARN_ON(!fb))
		return;

	//if (!plane->state->visible) {
	if (!new_state->visible) {
		starfive_plane_atomic_disable(plane, old_state);
		return;
	}

	obj = fb->obj[0];
	starfive_obj = to_starfive_gem_obj(obj);
	dma_addr = starfive_obj->dma_addr;
	pitch = fb->pitches[0];
	format = fb->format->format;

	//dma_addr += (plane->state->src.x1 >> 16) * fb->format->cpp[0];
	//dma_addr += (plane->state->src.y1 >> 16) * pitch;
	dma_addr += (new_state->src.x1 >> 16) * fb->format->cpp[0];
	dma_addr += (new_state->src.y1 >> 16) * pitch;
	if (sf_crtc->ddr_format != format) {
		sf_crtc->ddr_format = format;
		sf_crtc->ddr_format_change = true;
	} else {
		sf_crtc->ddr_format_change = false;
	}

	if (sf_crtc->dma_addr != dma_addr) {
		sf_crtc->dma_addr = dma_addr;
		sf_crtc->dma_addr_change = true;
	} else {
		sf_crtc->dma_addr_change = false;
	}
	sf_crtc->size = obj->size;
}

static int starfive_plane_atomic_async_check(struct drm_plane *plane,
					     struct drm_atomic_state *state)
{
	struct drm_crtc_state *crtc_state;
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
										 plane);

	if (plane != new_plane_state->crtc->cursor)
		return -EINVAL;

	if (!plane->state)
		return -EINVAL;

	if (!plane->state->fb)
		return -EINVAL;

	//if (new_plane_state->state)
	//   crtc_state = drm_atomic_get_existing_crtc_state(new_plane_state->state,
	//						   new_plane_state->crtc);
	//else /* Special case for asynchronous cursor updates. */
	//  crtc_state = new_plane_state->crtc->state;

	if (state)
		crtc_state = drm_atomic_get_existing_crtc_state(state,
								new_plane_state->crtc);
	else /* Special case for asynchronous cursor updates. */
		//crtc_state = plane->crtc->state;
		crtc_state = new_plane_state->crtc->state;

	return drm_atomic_helper_check_plane_state(plane->state, crtc_state,
				DRM_PLANE_HELPER_NO_SCALING,
				DRM_PLANE_HELPER_NO_SCALING,
				true, true);
}

static void starfive_plane_atomic_async_update(struct drm_plane *plane,
					       struct drm_atomic_state *new_state)
{
	struct drm_plane_state *new_plane_state =
		drm_atomic_get_new_plane_state(new_state, plane);
	struct starfive_crtc *crtcp = to_starfive_crtc(plane->state->crtc);

	plane->state->crtc_x = new_plane_state->crtc_x;
	plane->state->crtc_y = new_plane_state->crtc_y;
	plane->state->crtc_h = new_plane_state->crtc_h;
	plane->state->crtc_w = new_plane_state->crtc_w;
	plane->state->src_x = new_plane_state->src_x;
	plane->state->src_y = new_plane_state->src_y;
	plane->state->src_h = new_plane_state->src_h;
	plane->state->src_w = new_plane_state->src_w;
	swap(plane->state->fb, new_plane_state->fb);

	if (crtcp->is_enabled) {
		starfive_plane_atomic_update(plane, new_state);
		spin_lock(&crtcp->reg_lock);
		starfive_crtc_hw_config_simple(crtcp);
		spin_unlock(&crtcp->reg_lock);
	}
}

static const struct drm_plane_helper_funcs starfive_plane_helper_funcs = {
	.atomic_check = starfive_plane_atomic_check,
	.atomic_update = starfive_plane_atomic_update,
	//.prepare_fb = drm_gem_fb_prepare_fb,
	.prepare_fb = drm_gem_plane_helper_prepare_fb,
	.atomic_disable = starfive_plane_atomic_disable,
	.atomic_async_check = starfive_plane_atomic_async_check,
	.atomic_async_update = starfive_plane_atomic_async_update,
};

int starfive_plane_init(struct drm_device *dev,
			struct starfive_crtc *starfive_crtc,
			enum drm_plane_type type)
{
	int ret;

	ret = drm_universal_plane_init(dev, starfive_crtc->planes, 0,
				       &starfive_plane_funcs, formats,
				       ARRAY_SIZE(formats), NULL, type, NULL);
	if (ret) {
		dev_err(dev->dev, "failed to initialize plane\n");
		return ret;
	}

	drm_plane_helper_add(starfive_crtc->planes, &starfive_plane_helper_funcs);

	return 0;
}
