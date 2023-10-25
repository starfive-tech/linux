// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 *
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/media-bus-format.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_vblank.h>
#include <drm/vs_drm.h>

#include "vs_crtc.h"
#include "vs_dc.h"
#include "vs_drv.h"

static void vs_crtc_reset(struct drm_crtc *crtc)
{
	struct vs_crtc_state *state;

	if (crtc->state) {
		__drm_atomic_helper_crtc_destroy_state(crtc->state);

		state = to_vs_crtc_state(crtc->state);
		kfree(state);
		crtc->state = NULL;
	}

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return;

	__drm_atomic_helper_crtc_reset(crtc, &state->base);
}

static struct drm_crtc_state *
vs_crtc_atomic_duplicate_state(struct drm_crtc *crtc)
{
	struct vs_crtc_state *ori_state;
	struct vs_crtc_state *state;

	if (!crtc->state)
		return NULL;

	ori_state = to_vs_crtc_state(crtc->state);
	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &state->base);

	state->output_fmt = ori_state->output_fmt;
	state->encoder_type = ori_state->encoder_type;
	state->bpp = ori_state->bpp;
	state->underflow = ori_state->underflow;

	return &state->base;
}

static void vs_crtc_atomic_destroy_state(struct drm_crtc *crtc,
					 struct drm_crtc_state *state)
{
	__drm_atomic_helper_crtc_destroy_state(state);
	kfree(to_vs_crtc_state(state));
}

static int vs_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	struct vs_dc *dc = dev_get_drvdata(vs_crtc->dev);

	vs_dc_enable_vblank(dc, true);

	return 0;
}

static void vs_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	struct vs_dc *dc = dev_get_drvdata(vs_crtc->dev);

	vs_dc_enable_vblank(dc, false);
}

static const struct drm_crtc_funcs vs_crtc_funcs = {
	.set_config		= drm_atomic_helper_set_config,
	.page_flip		= drm_atomic_helper_page_flip,
	.reset			= vs_crtc_reset,
	.atomic_duplicate_state = vs_crtc_atomic_duplicate_state,
	.atomic_destroy_state	= vs_crtc_atomic_destroy_state,
	.enable_vblank		= vs_crtc_enable_vblank,
	.disable_vblank		= vs_crtc_disable_vblank,
};

static u8 cal_pixel_bits(u32 bus_format)
{
	u8 bpp;

	switch (bus_format) {
	case MEDIA_BUS_FMT_RGB565_1X16:
	case MEDIA_BUS_FMT_UYVY8_1X16:
		bpp = 16;
		break;
	case MEDIA_BUS_FMT_RGB666_1X18:
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
		bpp = 18;
		break;
	case MEDIA_BUS_FMT_UYVY10_1X20:
		bpp = 20;
		break;
	case MEDIA_BUS_FMT_BGR888_1X24:
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
	case MEDIA_BUS_FMT_YUV8_1X24:
		bpp = 24;
		break;
	case MEDIA_BUS_FMT_RGB101010_1X30:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
	case MEDIA_BUS_FMT_YUV10_1X30:
		bpp = 30;
		break;
	default:
		bpp = 24;
		break;
	}

	return bpp;
}

static void vs_crtc_atomic_enable(struct drm_crtc *crtc,
				  struct drm_atomic_state *state)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	struct vs_dc *dc = dev_get_drvdata(vs_crtc->dev);
	struct vs_crtc_state *vs_crtc_state = to_vs_crtc_state(crtc->state);

	vs_crtc_state->bpp = cal_pixel_bits(vs_crtc_state->output_fmt);

	vs_dc_enable(dc, crtc);
	drm_crtc_vblank_on(crtc);
}

static void vs_crtc_atomic_disable(struct drm_crtc *crtc,
				   struct drm_atomic_state *state)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	struct vs_dc *dc = dev_get_drvdata(vs_crtc->dev);

	drm_crtc_vblank_off(crtc);

	vs_dc_disable(dc, crtc);

	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irq(&crtc->dev->event_lock);

		crtc->state->event = NULL;
	}
}

static void vs_crtc_atomic_begin(struct drm_crtc *crtc,
				 struct drm_atomic_state *state)
{
	struct drm_crtc_state *crtc_state = drm_atomic_get_new_crtc_state(state,
									  crtc);

	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	struct device *dev = vs_crtc->dev;
	struct drm_property_blob *blob = crtc->state->gamma_lut;
	struct drm_color_lut *lut;
	struct vs_dc *dc = dev_get_drvdata(dev);

	if (crtc_state->color_mgmt_changed) {
		if (blob && blob->length) {
			lut = blob->data;
			vs_dc_set_gamma(dc, crtc, lut,
					blob->length / sizeof(*lut));
			vs_dc_enable_gamma(dc, crtc, true);
		} else {
			vs_dc_enable_gamma(dc, crtc, false);
		}
	}
}

static void vs_crtc_atomic_flush(struct drm_crtc *crtc,
				 struct drm_atomic_state *state)
{
	struct vs_crtc *vs_crtc = to_vs_crtc(crtc);
	struct drm_pending_vblank_event *event = crtc->state->event;
	struct vs_dc *dc = dev_get_drvdata(vs_crtc->dev);

	vs_dc_commit(dc);

	if (event) {
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);

		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_arm_vblank_event(crtc, event);
		spin_unlock_irq(&crtc->dev->event_lock);
		crtc->state->event = NULL;
	}
}

static const struct drm_crtc_helper_funcs vs_crtc_helper_funcs = {
	.atomic_check = drm_crtc_helper_atomic_check,
	.atomic_enable	= vs_crtc_atomic_enable,
	.atomic_disable = vs_crtc_atomic_disable,
	.atomic_begin	= vs_crtc_atomic_begin,
	.atomic_flush	= vs_crtc_atomic_flush,
};

static const struct drm_prop_enum_list vs_sync_mode_enum_list[] = {
	{ VS_SINGLE_DC,				"single dc mode" },
	{ VS_MULTI_DC_PRIMARY,		"primary dc for multi dc mode" },
	{ VS_MULTI_DC_SECONDARY,	"secondary dc for multi dc mode" },
};

struct vs_crtc *vs_crtc_create(struct drm_device *drm_dev,
			       struct vs_dc_info *info)
{
	struct vs_crtc *crtc;
	int ret;

	if (!info)
		return NULL;

	crtc = drmm_kzalloc(drm_dev, sizeof(*crtc), GFP_KERNEL);
	if (!crtc)
		return NULL;

	ret = drmm_crtc_init_with_planes(drm_dev, &crtc->base,
					 NULL, NULL, &vs_crtc_funcs,
					 info->name ? info->name : NULL);
	if (ret)
		return NULL;

	drm_crtc_helper_add(&crtc->base, &vs_crtc_helper_funcs);

	if (info->gamma_size) {
		ret = drm_mode_crtc_set_gamma_size(&crtc->base,
						   info->gamma_size);
		if (ret)
			return NULL;

		drm_crtc_enable_color_mgmt(&crtc->base, 0, false,
					   info->gamma_size);
	}

	crtc->max_bpc = info->max_bpc;
	crtc->color_formats = info->color_formats;
	return crtc;
}
