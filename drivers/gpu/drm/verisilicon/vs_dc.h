/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#ifndef __VS_DC_H__
#define __VS_DC_H__

#include <linux/clk.h>
#include <linux/mm_types.h>
#include <linux/reset.h>

#include <drm/drm_fourcc.h>
#include <drm/drm_modes.h>

#include "vs_crtc.h"
#include "vs_dc_hw.h"
#include "vs_plane.h"

struct vs_dc_plane {
	enum dc_hw_plane_id id;
};

enum vout_clk {
	CLK_VOUT_NOC_DISP = 0,
	CLK_VOUT_PIX0,
	CLK_VOUT_PIX1,
	CLK_VOUT_CORE,
	CLK_VOUT_AXI,
	CLK_VOUT_AHB,
	CLK_VOUT_HDMI_PIX,
	CLK_VOUT_SOC_PIX,
	CLK_VOUT_NUM
};

enum rst_vout {
	RST_VOUT_AXI = 0,
	RST_VOUT_AHB,
	RST_VOUT_CORE,
	RST_VOUT_NUM
};

struct vs_dc {
	struct vs_crtc		*crtc[DC_DISPLAY_NUM];
	struct dc_hw		hw;
	bool			first_frame;

	struct vs_dc_plane	planes[PLANE_NUM];
	struct clk_bulk_data	clk_vout[CLK_VOUT_NUM];
	int			nclks;
	struct reset_control_bulk_data rst_vout[RST_VOUT_NUM];
	int			nrsts;
};

void vs_dc_enable(struct vs_dc *dc, struct drm_crtc *crtc);
void vs_dc_disable(struct vs_dc *dc, struct drm_crtc *crtc);

void vs_dc_set_gamma(struct vs_dc *dc, struct drm_crtc *crtc,
		     struct drm_color_lut *lut, unsigned int size);
void vs_dc_enable_gamma(struct vs_dc *dc, struct drm_crtc *crtc, bool enable);
void vs_dc_enable_vblank(struct vs_dc *dc, bool enable);
void vs_dc_commit(struct vs_dc *dc);
void vs_dc_update_plane(struct vs_dc *dc, struct vs_plane *plane,
			struct drm_plane *drm_plane,
			struct drm_atomic_state *drm_state);
void vs_dc_disable_plane(struct vs_dc *dc, struct vs_plane *plane,
			 struct drm_plane_state *old_state);
int vs_dc_check_plane(struct vs_dc *dc, struct drm_plane *plane,
		      struct drm_atomic_state *state);
void vs_dc_update_cursor_plane(struct vs_dc *dc, struct vs_plane *plane,
			       struct drm_plane *drm_plane,
			       struct drm_atomic_state *drm_state);
void vs_dc_disable_cursor_plane(struct vs_dc *dc, struct vs_plane *plane,
				struct drm_plane_state *old_state);
int vs_dc_check_cursor_plane(struct vs_dc *dc, struct drm_plane *plane,
			     struct drm_atomic_state *state);

extern struct platform_driver dc_platform_driver;

#endif /* __VS_DC_H__ */
