// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#include <linux/component.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_blend.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_vblank.h>

#include "vs_dc_hw.h"
#include "vs_dc.h"
#include "vs_drv.h"

static const char * const vout_clocks[] = {
	"channel0",
	"channel1",
	"hdmi_tx",
	"dc_parent",
};

#define FRAC_16_16(mult, div)	 (((mult) << 16) / (div))

static const u32 primary_overlay_format0[] = {
	DRM_FORMAT_RGB565,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_RGBX8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_XBGR4444,
	DRM_FORMAT_RGBX4444,
	DRM_FORMAT_BGRX4444,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_ABGR4444,
	DRM_FORMAT_RGBA4444,
	DRM_FORMAT_BGRA4444,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_XBGR1555,
	DRM_FORMAT_RGBX5551,
	DRM_FORMAT_BGRX5551,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_ABGR1555,
	DRM_FORMAT_RGBA5551,
	DRM_FORMAT_BGRA5551,
	DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_RGBA1010102,
	DRM_FORMAT_BGRA1010102,
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YVYU,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,
	DRM_FORMAT_YVU420,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_NV16,
	DRM_FORMAT_NV61,
	DRM_FORMAT_P010,
};

static const u32 primary_overlay_format1[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_RGBX8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_RGBA1010102,
	DRM_FORMAT_BGRA1010102,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_YUV444,
};

static const u32 cursor_formats[] = {
	DRM_FORMAT_ARGB8888
};

static const u64 format_modifier0[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

static const u64 secondary_format_modifiers[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

static const struct vs_plane_info dc_hw_planes_rev0[PLANE_NUM] = {
	{
		.name			= "Primary",
		.id			= PRIMARY_PLANE_0,
		.type			= DRM_PLANE_TYPE_PRIMARY,
		.num_formats		= ARRAY_SIZE(primary_overlay_format0),
		.formats		= primary_overlay_format0,
		.num_modifiers		= ARRAY_SIZE(format_modifier0),
		.modifiers		= format_modifier0,
		.min_width		= 0,
		.min_height		= 0,
		.max_width		= 4096,
		.max_height		= 4096,
		.rotation		= DRM_MODE_ROTATE_0 |
					  DRM_MODE_ROTATE_90 |
					  DRM_MODE_ROTATE_180 |
					  DRM_MODE_ROTATE_270 |
					  DRM_MODE_REFLECT_X |
					  DRM_MODE_REFLECT_Y,
		.blend_mode		= BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					  BIT(DRM_MODE_BLEND_PREMULTI) |
					  BIT(DRM_MODE_BLEND_COVERAGE),
		.color_encoding		= BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
		.degamma_size		= DEGAMMA_SIZE,
		.min_scale		= FRAC_16_16(1, 3),
		.max_scale		= FRAC_16_16(10, 1),
		.zpos			= 0,
		.watermark		= true,
		.color_mgmt		= true,
		.roi			= true,
	},
	{
		.name			= "Overlay",
		.id			= OVERLAY_PLANE_0,
		.type			= DRM_PLANE_TYPE_OVERLAY,
		.num_formats		= ARRAY_SIZE(primary_overlay_format0),
		.formats		= primary_overlay_format0,
		.num_modifiers		= ARRAY_SIZE(format_modifier0),
		.modifiers		= format_modifier0,
		.min_width		= 0,
		.min_height		= 0,
		.max_width		= 4096,
		.max_height		= 4096,
		.rotation		= DRM_MODE_ROTATE_0 |
					  DRM_MODE_ROTATE_90 |
					  DRM_MODE_ROTATE_180 |
					  DRM_MODE_ROTATE_270 |
					  DRM_MODE_REFLECT_X |
					  DRM_MODE_REFLECT_Y,
		.blend_mode		= BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					  BIT(DRM_MODE_BLEND_PREMULTI) |
					  BIT(DRM_MODE_BLEND_COVERAGE),
		.color_encoding		= BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
		.degamma_size		= DEGAMMA_SIZE,
		.min_scale		= FRAC_16_16(1, 3),
		.max_scale		= FRAC_16_16(10, 1),
		.zpos			= 1,
		.watermark		= true,
		.color_mgmt		= true,
		.roi			= true,
	},
	{
		.name			= "Overlay_1",
		.id			= OVERLAY_PLANE_1,
		.type			= DRM_PLANE_TYPE_OVERLAY,
		.num_formats		= ARRAY_SIZE(primary_overlay_format0),
		.formats		= primary_overlay_format0,
		.num_modifiers		= ARRAY_SIZE(secondary_format_modifiers),
		.modifiers		= secondary_format_modifiers,
		.min_width		= 0,
		.min_height		= 0,
		.max_width		= 4096,
		.max_height		= 4096,
		.rotation		= 0,
		.blend_mode		= BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					  BIT(DRM_MODE_BLEND_PREMULTI) |
					  BIT(DRM_MODE_BLEND_COVERAGE),
		.color_encoding		= BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
		.degamma_size		= DEGAMMA_SIZE,
		.min_scale		= DRM_PLANE_NO_SCALING,
		.max_scale		= DRM_PLANE_NO_SCALING,
		.zpos			= 2,
		.watermark		= true,
		.color_mgmt		= true,
		.roi			= true,
	},
	{
		.name			= "Primary_1",
		.id			= PRIMARY_PLANE_1,
		.type			= DRM_PLANE_TYPE_PRIMARY,
		.num_formats		= ARRAY_SIZE(primary_overlay_format0),
		.formats		= primary_overlay_format0,
		.num_modifiers		= ARRAY_SIZE(format_modifier0),
		.modifiers		= format_modifier0,
		.min_width		= 0,
		.min_height		= 0,
		.max_width		= 4096,
		.max_height		= 4096,
		.rotation		= DRM_MODE_ROTATE_0 |
					  DRM_MODE_ROTATE_90 |
					  DRM_MODE_ROTATE_180 |
					  DRM_MODE_ROTATE_270 |
					  DRM_MODE_REFLECT_X |
					  DRM_MODE_REFLECT_Y,
		.blend_mode		= BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					  BIT(DRM_MODE_BLEND_PREMULTI) |
					  BIT(DRM_MODE_BLEND_COVERAGE),
		.color_encoding		= BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
		.degamma_size		= DEGAMMA_SIZE,
		.min_scale		= FRAC_16_16(1, 3),
		.max_scale		= FRAC_16_16(10, 1),
		.zpos			= 3,
		.watermark		= true,
		.color_mgmt		= true,
		.roi			= true,
	},
	{
		.name			= "Overlay_2",
		.id			= OVERLAY_PLANE_2,
		.type			= DRM_PLANE_TYPE_OVERLAY,
		.num_formats		= ARRAY_SIZE(primary_overlay_format0),
		.formats		= primary_overlay_format0,
		.num_modifiers		= ARRAY_SIZE(format_modifier0),
		.modifiers		= format_modifier0,
		.min_width		= 0,
		.min_height		= 0,
		.max_width		= 4096,
		.max_height		= 4096,
		.rotation		= DRM_MODE_ROTATE_0 |
					  DRM_MODE_ROTATE_90 |
					  DRM_MODE_ROTATE_180 |
					  DRM_MODE_ROTATE_270 |
					  DRM_MODE_REFLECT_X |
					  DRM_MODE_REFLECT_Y,
		.blend_mode		= BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					  BIT(DRM_MODE_BLEND_PREMULTI) |
					  BIT(DRM_MODE_BLEND_COVERAGE),
		.color_encoding		= BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
		.degamma_size		= DEGAMMA_SIZE,
		.min_scale		= FRAC_16_16(1, 3),
		.max_scale		= FRAC_16_16(10, 1),
		.zpos			= 4,
		.watermark		= true,
		.color_mgmt		= true,
		.roi			= true,
	},
	{
		.name			= "Overlay_3",
		.id			= OVERLAY_PLANE_3,
		.type			= DRM_PLANE_TYPE_OVERLAY,
		.num_formats		= ARRAY_SIZE(primary_overlay_format0),
		.formats		= primary_overlay_format0,
		.num_modifiers		= ARRAY_SIZE(secondary_format_modifiers),
		.modifiers		= secondary_format_modifiers,
		.min_width		= 0,
		.min_height		= 0,
		.max_width		= 4096,
		.max_height		= 4096,
		.rotation		= 0,
		.blend_mode		= BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					  BIT(DRM_MODE_BLEND_PREMULTI) |
					  BIT(DRM_MODE_BLEND_COVERAGE),
		.color_encoding		= BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
		.degamma_size		= DEGAMMA_SIZE,
		.min_scale		= DRM_PLANE_NO_SCALING,
		.max_scale		= DRM_PLANE_NO_SCALING,
		.zpos			= 5,
		.watermark		= true,
		.color_mgmt		= true,
		.roi			= true,
	},
	{
		.name			= "Cursor",
		.id			= CURSOR_PLANE_0,
		.type			= DRM_PLANE_TYPE_CURSOR,
		.num_formats		= ARRAY_SIZE(cursor_formats),
		.formats		= cursor_formats,
		.num_modifiers		= 0,
		.modifiers		= NULL,
		.min_width		= 32,
		.min_height		= 32,
		.max_width		= 64,
		.max_height		= 64,
		.rotation		= 0,
		.degamma_size		= 0,
		.min_scale		= DRM_PLANE_NO_SCALING,
		.max_scale		= DRM_PLANE_NO_SCALING,
		.zpos			= 255,
		.watermark		= false,
		.color_mgmt		= false,
		.roi			= false,
	},
	{
		.name			= "Cursor_1",
		.id			= CURSOR_PLANE_1,
		.type			= DRM_PLANE_TYPE_CURSOR,
		.num_formats		= ARRAY_SIZE(cursor_formats),
		.formats		= cursor_formats,
		.num_modifiers		= 0,
		.modifiers		= NULL,
		.min_width		= 32,
		.min_height		= 32,
		.max_width		= 64,
		.max_height		= 64,
		.rotation		= 0,
		.degamma_size		= 0,
		.min_scale		= DRM_PLANE_NO_SCALING,
		.max_scale		= DRM_PLANE_NO_SCALING,
		.zpos			= 255,
		.watermark		= false,
		.color_mgmt		= false,
		.roi			= false,
	},
};

static const struct vs_dc_info dc8200_info = {
	.name			= "DC8200",
	.panel_num		= 2,
	.plane_num		= 8,
	.planes			= dc_hw_planes_rev0,
	.layer_num		= 6,
	.max_bpc		= 10,
	.color_formats		= DRM_COLOR_FORMAT_RGB444 |
				  DRM_COLOR_FORMAT_YCBCR444 |
				  DRM_COLOR_FORMAT_YCBCR422 |
				  DRM_COLOR_FORMAT_YCBCR420,
	.gamma_size		= GAMMA_EX_SIZE,
	.gamma_bits		= 12,
	.pitch_alignment	= 128,
	.pipe_sync		= false,
	.background		= true,
	.panel_sync		= true,
	.cap_dec		= true,
};

static inline void update_format(u32 format, u64 mod, struct dc_hw_fb *fb)
{
	u8 f = FORMAT_A8R8G8B8;

	switch (format) {
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_BGRX4444:
		f = FORMAT_X4R4G4B4;
		break;
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_BGRA4444:
		f = FORMAT_A4R4G4B4;
		break;
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_BGRX5551:
		f = FORMAT_X1R5G5B5;
		break;
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_BGRA5551:
		f = FORMAT_A1R5G5B5;
		break;
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		f = FORMAT_R5G6B5;
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_BGRX8888:
		f = FORMAT_X8R8G8B8;
		break;
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_BGRA8888:
		f = FORMAT_A8R8G8B8;
		break;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		f = FORMAT_YUY2;
		break;
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		f = FORMAT_UYVY;
		break;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		f = FORMAT_YV12;
		break;
	case DRM_FORMAT_NV21:
		f = FORMAT_NV12;
		break;
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		f = FORMAT_NV16;
		break;
	case DRM_FORMAT_P010:
		f = FORMAT_P010;
		break;
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_BGRA1010102:
		f = FORMAT_A2R10G10B10;
		break;
	case DRM_FORMAT_NV12:
		f = FORMAT_NV12;
		break;
	case DRM_FORMAT_YUV444:
		f = FORMAT_YUV444;
		break;
	default:
		break;
	}

	fb->format = f;
}

static inline void update_swizzle(u32 format, struct dc_hw_fb *fb)
{
	fb->swizzle = SWIZZLE_ARGB;
	fb->uv_swizzle = 0;

	switch (format) {
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBA1010102:
		fb->swizzle = SWIZZLE_RGBA;
		break;
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_ABGR2101010:
		fb->swizzle = SWIZZLE_ABGR;
		break;
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRA1010102:
		fb->swizzle = SWIZZLE_BGRA;
		break;
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
		fb->uv_swizzle = 1;
		break;
	default:
		break;
	}
}

static inline u8 to_vs_rotation(unsigned int rotation)
{
	u8 rot;

	switch (rotation & DRM_MODE_REFLECT_MASK) {
	case DRM_MODE_REFLECT_X:
		rot = FLIP_X;
		return rot;
	case DRM_MODE_REFLECT_Y:
		rot = FLIP_Y;
		return rot;
	case DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y:
		rot = FLIP_XY;
		return rot;
	default:
		break;
	}

	switch (rotation & DRM_MODE_ROTATE_MASK) {
	case DRM_MODE_ROTATE_0:
		rot = ROT_0;
		break;
	case DRM_MODE_ROTATE_90:
		rot = ROT_90;
		break;
	case DRM_MODE_ROTATE_180:
		rot = ROT_180;
		break;
	case DRM_MODE_ROTATE_270:
		rot = ROT_270;
		break;
	default:
		rot = ROT_0;
		break;
	}

	return rot;
}

static inline u8 to_vs_yuv_color_space(u32 color_space)
{
	u8 cs;

	switch (color_space) {
	case DRM_COLOR_YCBCR_BT601:
		cs = COLOR_SPACE_601;
		break;
	case DRM_COLOR_YCBCR_BT709:
		cs = COLOR_SPACE_709;
		break;
	case DRM_COLOR_YCBCR_BT2020:
		cs = COLOR_SPACE_2020;
		break;
	default:
		cs = COLOR_SPACE_601;
		break;
	}

	return cs;
}

static inline u8 to_vs_display_id(struct vs_dc *dc, struct drm_crtc *crtc)
{
	u8 panel_num = dc->hw.info->panel_num;
	u32 index = drm_crtc_index(crtc);
	int i;

	for (i = 0; i < panel_num; i++) {
		if (index == dc->crtc[i]->base.index)
			return i;
	}

	return 0;
}

static void vs_drm_update_pitch_alignment(struct drm_device *drm_dev,
					  unsigned int alignment)
{
	struct vs_drm_device *priv = to_vs_drm_private(drm_dev);

	if (alignment > priv->pitch_alignment)
		priv->pitch_alignment = alignment;
}

static int plda_clk_rst_init(struct device *dev)
{
	struct vs_dc *dc = dev_get_drvdata(dev);

	return clk_bulk_prepare_enable(dc->nclks, dc->clk_vout);
}

static void plda_clk_rst_deinit(struct device *dev)
{
	struct vs_dc *dc = dev_get_drvdata(dev);

	clk_bulk_disable_unprepare(dc->nclks, dc->clk_vout);
}

static void dc_deinit(struct device *dev)
{
	struct vs_dc *dc = dev_get_drvdata(dev);

	dc_hw_enable_interrupt(&dc->hw, 0);
	dc_hw_deinit(&dc->hw);
	plda_clk_rst_deinit(dev);
}

static int dc_init(struct device *dev)
{
	struct vs_dc *dc = dev_get_drvdata(dev);
	int ret;

	ret = plda_clk_rst_init(dev);
	if (ret < 0) {
		dev_err(dev, "failed to init dc clk reset: %d\n", ret);
		return ret;
	}

	ret = dc_hw_init(&dc->hw);
	if (ret) {
		dev_err(dev, "failed to init DC HW\n");
		return ret;
	}
	return 0;
}

void vs_dc_enable(struct vs_dc *dc, struct drm_crtc *crtc)
{
	struct vs_crtc_state *crtc_state = to_vs_crtc_state(crtc->state);
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	struct dc_hw_display display;

	display.bus_format = crtc_state->output_fmt;
	display.h_active = mode->hdisplay;
	display.h_total = mode->htotal;
	display.h_sync_start = mode->hsync_start;
	display.h_sync_end = mode->hsync_end;
	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		display.h_sync_polarity = true;
	else
		display.h_sync_polarity = false;

	display.v_active = mode->vdisplay;
	display.v_total = mode->vtotal;
	display.v_sync_start = mode->vsync_start;
	display.v_sync_end = mode->vsync_end;

	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		display.v_sync_polarity = true;
	else
		display.v_sync_polarity = false;

	display.id = to_vs_display_id(dc, crtc);

	display.enable = true;

	if (crtc_state->encoder_type == DRM_MODE_ENCODER_DSI) {
		dc_hw_set_out(&dc->hw, OUT_DPI, display.id);
		clk_set_rate(dc->clk_vout[CLK_VOUT_SOC_PIX].clk, mode->clock * 1000);
		clk_set_parent(dc->clk_vout[CLK_VOUT_PIX1].clk,
			       dc->clk_vout[CLK_VOUT_SOC_PIX].clk);
	} else {
		dc_hw_set_out(&dc->hw, OUT_DP, display.id);
		clk_set_parent(dc->clk_vout[CLK_VOUT_PIX0].clk,
			       dc->clk_vout[CLK_VOUT_HDMI_PIX].clk);
	}

	dc_hw_setup_display(&dc->hw, &display);

	enable_irq(dc->irq);
}

void vs_dc_disable(struct vs_dc *dc, struct drm_crtc *crtc)
{
	struct dc_hw_display display;

	disable_irq(dc->irq);

	display.id = to_vs_display_id(dc, crtc);
	display.enable = false;

	dc_hw_setup_display(&dc->hw, &display);
}

void vs_dc_set_gamma(struct vs_dc *dc, struct drm_crtc *crtc,
		     struct drm_color_lut *lut, unsigned int size)
{
	u16 i, r, g, b;
	u8 bits, id;

	if (size != dc->hw.info->gamma_size) {
		drm_err(crtc->dev, "gamma size does not match!\n");
		return;
	}

	id = to_vs_display_id(dc, crtc);

	bits = dc->hw.info->gamma_bits;
	for (i = 0; i < size; i++) {
		r = drm_color_lut_extract(lut[i].red, bits);
		g = drm_color_lut_extract(lut[i].green, bits);
		b = drm_color_lut_extract(lut[i].blue, bits);
		dc_hw_update_gamma(&dc->hw, id, i, r, g, b);
	}
}

void vs_dc_enable_gamma(struct vs_dc *dc, struct drm_crtc *crtc,
			bool enable)
{
	u8 id;

	id = to_vs_display_id(dc, crtc);
	dc_hw_enable_gamma(&dc->hw, id, enable);
}

void vs_dc_enable_vblank(struct vs_dc *dc, bool enable)
{
	dc_hw_enable_interrupt(&dc->hw, enable);
}

static u32 calc_factor(u32 src, u32 dest)
{
	u32 factor = 1 << 16;

	if (src > 1 && dest > 1)
		factor = ((src - 1) << 16) / (dest - 1);

	return factor;
}

static void update_scale(struct drm_plane_state *state, struct dc_hw_roi *roi,
			 struct dc_hw_scale *scale)
{
	int dst_w = drm_rect_width(&state->dst);
	int dst_h = drm_rect_height(&state->dst);
	int src_w, src_h, temp;

	scale->enable = false;

	if (roi->enable) {
		src_w = roi->width;
		src_h = roi->height;
	} else {
		src_w = drm_rect_width(&state->src) >> 16;
		src_h = drm_rect_height(&state->src) >> 16;
	}

	if (drm_rotation_90_or_270(state->rotation)) {
		temp = src_w;
		src_w = src_h;
		src_h = temp;
	}

	if (src_w != dst_w) {
		scale->scale_factor_x = calc_factor(src_w, dst_w);
		scale->enable = true;
	} else {
		scale->scale_factor_x = 1 << 16;
	}
	if (src_h != dst_h) {
		scale->scale_factor_y = calc_factor(src_h, dst_h);
		scale->enable = true;
	} else {
		scale->scale_factor_y = 1 << 16;
	}
}

static void update_fb(struct vs_plane *plane, u8 display_id,
		      struct dc_hw_fb *fb, struct drm_plane_state *state)
{
	struct vs_plane_state *plane_state = to_vs_plane_state(state);
	struct drm_framebuffer *drm_fb = state->fb;
	struct drm_rect *src = &state->src;

	fb->display_id = display_id;
	fb->y_address = plane_state->dma_addr[0];
	fb->y_stride = drm_fb->pitches[0];
	if (drm_fb->format->format == DRM_FORMAT_YVU420) {
		fb->u_address = plane_state->dma_addr[2];
		fb->v_address = plane_state->dma_addr[1];
		fb->u_stride = drm_fb->pitches[2];
		fb->v_stride = drm_fb->pitches[1];
	} else {
		fb->u_address = plane_state->dma_addr[1];
		fb->v_address = plane_state->dma_addr[2];
		fb->u_stride = drm_fb->pitches[1];
		fb->v_stride = drm_fb->pitches[2];
	}
	fb->width = drm_rect_width(src) >> 16;
	fb->height = drm_rect_height(src) >> 16;
	fb->tile_mode = DRM_FORMAT_MOD_LINEAR;
	fb->rotation = to_vs_rotation(state->rotation);
	fb->yuv_color_space = to_vs_yuv_color_space(state->color_encoding);
	fb->zpos = state->zpos;
	fb->enable = state->visible;
	update_format(drm_fb->format->format, drm_fb->modifier, fb);
	update_swizzle(drm_fb->format->format, fb);
}

static void update_plane(struct vs_dc *dc, struct vs_plane *plane,
			 struct drm_plane *drm_plane,
			 struct drm_atomic_state *drm_state)
{
	struct dc_hw_fb fb = {0};
	struct dc_hw_scale scale;
	struct dc_hw_position pos;
	struct dc_hw_blend blend;
	struct dc_hw_roi roi;
	struct drm_plane_state *state = drm_atomic_get_new_plane_state(drm_state,
									   drm_plane);
	struct drm_rect *dest = &state->dst;
	bool dec_enable = false;
	u8 display_id = 0;

	display_id = to_vs_display_id(dc, state->crtc);
	update_fb(plane, display_id, &fb, state);
	fb.dec_enable = dec_enable;
	roi.enable = false;

	update_scale(state, &roi, &scale);

	pos.start_x = dest->x1;
	pos.start_y = dest->y1;
	pos.end_x = dest->x2;
	pos.end_y = dest->y2;

	blend.alpha = (u8)(state->alpha >> 8);
	blend.blend_mode = (u8)(state->pixel_blend_mode);

	dc_hw_update_plane(&dc->hw, plane->id, &fb, &scale, &pos, &blend);
}

static void update_cursor_size(struct drm_plane_state *state, struct dc_hw_cursor *cursor)
{
	u8 size_type;

	switch (state->crtc_w) {
	case 32:
		size_type = CURSOR_SIZE_32X32;
		break;
	case 64:
		size_type = CURSOR_SIZE_64X64;
		break;
	default:
		size_type = CURSOR_SIZE_32X32;
		break;
	}

	cursor->size = size_type;
}

static void update_cursor_plane(struct vs_dc *dc, struct vs_plane *plane,
				struct drm_plane *drm_plane,
				struct drm_atomic_state *drm_state)
{
	struct drm_plane_state *state = drm_atomic_get_new_plane_state(drm_state,
								       drm_plane);
	struct vs_plane_state *plane_state = to_vs_plane_state(state);
	struct drm_framebuffer *drm_fb = state->fb;
	struct dc_hw_cursor cursor;

	cursor.address = plane_state->dma_addr[0];
	cursor.x = state->crtc_x;
	cursor.y = state->crtc_y;
	cursor.hot_x = drm_fb->hot_x;
	cursor.hot_y = drm_fb->hot_y;
	cursor.display_id = to_vs_display_id(dc, state->crtc);
	update_cursor_size(state, &cursor);
	cursor.enable = true;

	dc_hw_update_cursor(&dc->hw, cursor.display_id, &cursor);
}

void vs_dc_update_plane(struct vs_dc *dc, struct vs_plane *plane,
			struct drm_plane *drm_plane,
			struct drm_atomic_state *drm_state)
{
	update_plane(dc, plane, drm_plane, drm_state);
}

void vs_dc_update_cursor_plane(struct vs_dc *dc, struct vs_plane *plane,
			       struct drm_plane *drm_plane,
			       struct drm_atomic_state *drm_state)
{
	update_cursor_plane(dc, plane, drm_plane, drm_state);
}

void vs_dc_disable_plane(struct vs_dc *dc, struct vs_plane *plane,
			 struct drm_plane_state *old_state)
{
	struct dc_hw_fb fb = {0};

	fb.enable = false;
	dc_hw_update_plane(&dc->hw, plane->id, &fb, NULL, NULL, NULL);
}

void vs_dc_disable_cursor_plane(struct vs_dc *dc, struct vs_plane *plane,
				struct drm_plane_state *old_state)
{
	struct dc_hw_cursor cursor = {0};

	cursor.enable = false;
	cursor.display_id = to_vs_display_id(dc, old_state->crtc);
	dc_hw_update_cursor(&dc->hw, cursor.display_id, &cursor);
}

static bool vs_dc_mod_supported(const struct vs_plane_info *plane_info,
				u64 modifier)
{
	const u64 *mods;

	if (!plane_info->modifiers)
		return false;

	for (mods = plane_info->modifiers; *mods != DRM_FORMAT_MOD_INVALID; mods++) {
		if (*mods == modifier)
			return true;
	}

	return false;
}

int vs_dc_check_plane(struct vs_dc *dc, struct drm_plane *plane,
		      struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
										 plane);

	struct drm_framebuffer *fb = new_plane_state->fb;
	const struct vs_plane_info *plane_info;
	struct drm_crtc *crtc = new_plane_state->crtc;
	struct drm_crtc_state *crtc_state;
	struct vs_plane *vs_plane = to_vs_plane(plane);

	plane_info = &dc->hw.info->planes[vs_plane->id];

	if (fb->width < plane_info->min_width ||
	    fb->width > plane_info->max_width ||
	    fb->height < plane_info->min_height ||
	    fb->height > plane_info->max_height)
		drm_err_once(plane->dev, "buffer size may not support on plane%d.\n",
			     vs_plane->id);

	if (!vs_dc_mod_supported(plane_info, fb->modifier)) {
		drm_err(plane->dev, "unsupported modifier on plane%d.\n", vs_plane->id);
		return -EINVAL;
	}

	crtc_state = drm_atomic_get_existing_crtc_state(state, crtc);
	return drm_atomic_helper_check_plane_state(new_plane_state, crtc_state,
						  plane_info->min_scale,
						  plane_info->max_scale,
						  true, true);
}

int vs_dc_check_cursor_plane(struct vs_dc *dc, struct drm_plane *plane,
			     struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
									   plane);
	struct drm_framebuffer *fb = new_plane_state->fb;
	const struct vs_plane_info *plane_info;
	struct drm_crtc *crtc = new_plane_state->crtc;
	struct drm_crtc_state *crtc_state;
	struct vs_plane *vs_plane = to_vs_plane(plane);

	plane_info = &dc->hw.info->planes[vs_plane->id];

	if (fb->width < plane_info->min_width ||
	    fb->width > plane_info->max_width ||
	    fb->height < plane_info->min_height ||
	    fb->height > plane_info->max_height)
		drm_err_once(plane->dev, "buffer size may not support on plane%d.\n", vs_plane->id);

	crtc_state = drm_atomic_get_existing_crtc_state(state, crtc);
	if (IS_ERR(crtc_state))
		return -EINVAL;

	return drm_atomic_helper_check_plane_state(new_plane_state, crtc_state,
						plane_info->min_scale,
						plane_info->max_scale,
						true, true);
}

static irqreturn_t dc_isr(int irq, void *data)
{
	struct vs_dc *dc = data;
	struct vs_dc_info *dc_info = dc->hw.info;
	u32 i, ret;

	if (!dc_info)
		return IRQ_HANDLED;

	ret = dc_hw_get_interrupt(&dc->hw);

	for (i = 0; i < dc_info->panel_num; i++)
		drm_crtc_handle_vblank(&dc->crtc[i]->base);

	return IRQ_HANDLED;
}

void vs_dc_commit(struct vs_dc *dc)
{
	dc_hw_enable_shadow_register(&dc->hw, false);

	dc_hw_commit(&dc->hw);

	dc_hw_enable_shadow_register(&dc->hw, true);
}

static int dc_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;
	struct vs_dc *dc = dev_get_drvdata(dev);
	struct device_node *port;
	struct vs_crtc *crtc;
	struct vs_dc_info *dc_info;
	struct vs_plane *plane;
	struct vs_plane_info *plane_info;
	int i, ret;
	u32 max_width = 0, max_height = 0;
	u32 min_width = 0xffff, min_heigth = 0xffff;
	u32 possible_crtc = 0;

	if (!drm_dev || !dc) {
		dev_err(dev, "devices are not created.\n");
		return -ENODEV;
	}

	ret = dc_init(dev);
	if (ret < 0) {
		drm_err(drm_dev, "Failed to initialize DC hardware.\n");
		return ret;
	}

	dc_info = dc->hw.info;

	for (i = 0; i < dc_info->panel_num; i++) {
		crtc = vs_crtc_create(drm_dev, dc_info);
		if (!crtc) {
			drm_err(drm_dev, "Failed to create CRTC.\n");
			ret = -ENOMEM;
			return ret;
		}

		port = of_graph_get_port_by_id(dev->of_node, i);
		if (!port) {
			drm_err(drm_dev, "no port node found for crtc_port%d\n", i);
			return -ENOENT;
		}

		crtc->base.port = port;
		crtc->dev = dev;
		dc->crtc[i] = crtc;
	}

	for (i = 0; i < dc_info->plane_num; i++) {
		plane_info = (struct vs_plane_info *)&dc_info->planes[i];

		if (!dc->crtc[0]->base.port || !dc->crtc[1]->base.port) {
			drm_err(drm_dev, "no port no crtc mask, fail to create plane\n");
			return -ENOENT;
		}

		if (plane_info->id == PRIMARY_PLANE_0 ||
		    plane_info->id == CURSOR_PLANE_0) {
			possible_crtc = drm_crtc_mask(&dc->crtc[0]->base);
		} else if (plane_info->id == PRIMARY_PLANE_1 ||
			plane_info->id == CURSOR_PLANE_1) {
			possible_crtc = drm_crtc_mask(&dc->crtc[1]->base);
		} else {
			possible_crtc = drm_crtc_mask(&dc->crtc[0]->base) |
					drm_crtc_mask(&dc->crtc[1]->base);
		}

		plane = vs_plane_create(drm_dev, plane_info,
					dc_info->layer_num, possible_crtc);
		if (IS_ERR(plane)) {
			dev_err(dev, "failed to construct plane\n");
			return PTR_ERR(plane);
		}

		plane->id = i;
		dc->planes[i].id = plane_info->id;

		if (plane_info->type == DRM_PLANE_TYPE_PRIMARY) {
			if (plane_info->id == PRIMARY_PLANE_0)
				dc->crtc[0]->base.primary = &plane->base;
			else
				dc->crtc[1]->base.primary = &plane->base;

			min_width = min_t(u32, min_width, plane_info->min_width);
			min_heigth = min_t(u32, min_heigth, plane_info->min_height);
			/*
			 * Note: these values are used for multiple independent things:
			 * hw display mode filtering, plane buffer sizes ...
			 * Use the combined maximum values here to cover all use cases,
			 * and do more specific checking in the respective code paths.
			 */
			max_width = max_t(u32, max_width, plane_info->max_width);
			max_height = max_t(u32, max_height, plane_info->max_height);
		}

		if (plane_info->type == DRM_PLANE_TYPE_CURSOR) {
			if (plane_info->id == CURSOR_PLANE_0)
				dc->crtc[0]->base.cursor = &plane->base;
			else
				dc->crtc[1]->base.cursor = &plane->base;
			drm_dev->mode_config.cursor_width =
				max_t(u32, drm_dev->mode_config.cursor_width,
				      plane_info->max_width);
			drm_dev->mode_config.cursor_height =
				max_t(u32, drm_dev->mode_config.cursor_height,
				      plane_info->max_height);
		}
	}

	drm_dev->mode_config.min_width = min_width;
	drm_dev->mode_config.min_height = min_heigth;
	drm_dev->mode_config.max_width = max_width;
	drm_dev->mode_config.max_height = max_height;

	vs_drm_update_pitch_alignment(drm_dev, dc_info->pitch_alignment);
	return 0;
}

static void dc_unbind(struct device *dev, struct device *master, void *data)
{
	dc_deinit(dev);
}

const struct component_ops dc_component_ops = {
	.bind = dc_bind,
	.unbind = dc_unbind,
};

static const struct of_device_id dc_driver_dt_match[] = {
	{
		.compatible = "starfive,jh7110-dc8200",
		.data = &dc8200_info,
	},
	{},
};
MODULE_DEVICE_TABLE(of, dc_driver_dt_match);

static int dc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vs_dc *dc;
	int ret, i;

	dc = devm_kzalloc(dev, sizeof(*dc), GFP_KERNEL);
	if (!dc)
		return -ENOMEM;

	dc->hw.hi_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dc->hw.hi_base))
		return PTR_ERR(dc->hw.hi_base);

	dc->hw.reg_base = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(dc->hw.reg_base))
		return PTR_ERR(dc->hw.reg_base);

	dc->nclks = ARRAY_SIZE(dc->clk_vout);
	for (i = 0; i < dc->nclks; ++i)
		dc->clk_vout[i].id = vout_clocks[i];
	ret = devm_clk_bulk_get(dev, dc->nclks, dc->clk_vout);
	if (ret) {
		dev_err(dev, "Failed to get clk controls\n");
		return ret;
	}

	dc->irq = platform_get_irq(pdev, 0);

	/* do not autoenable, will be enabled later */
	ret = devm_request_irq(dev, dc->irq, dc_isr, IRQF_NO_AUTOEN, dev_name(dev), dc);
	if (ret < 0) {
		dev_err(dev, "Failed to install irq:%u.\n", dc->irq);
		return ret;
	}

	dc->hw.info = (struct vs_dc_info *)of_device_get_match_data(dev);

	dev_set_drvdata(dev, dc);

	return component_add(dev, &dc_component_ops);
}

static int dc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	component_del(dev, &dc_component_ops);

	dev_set_drvdata(dev, NULL);

	return 0;
}

struct platform_driver dc_platform_driver = {
	.probe = dc_probe,
	.remove = dc_remove,
	.driver = {
		.name = "vs-dc",
		.of_match_table = of_match_ptr(dc_driver_dt_match),
	},
};

MODULE_AUTHOR("StarFive Corporation");
MODULE_DESCRIPTION("VeriSilicon DC Driver");
MODULE_LICENSE("GPL");
