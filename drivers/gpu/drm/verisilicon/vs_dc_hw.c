// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 VeriSilicon Holdings Co., Ltd.
 */

#include <linux/bits.h>
#include <linux/io.h>
#include <linux/media-bus-format.h>
//#include <drm/drm_atomic_helper.h>
#include <drm/drm_blend.h>
#include <drm/drm_fourcc.h>

#include "vs_dc_hw.h"
#include "vs_type.h"

static const u32 horkernel[] = {
	0x00000000, 0x20000000, 0x00002000, 0x00000000,
	0x00000000, 0x00000000, 0x23fd1c03, 0x00000000,
	0x00000000, 0x00000000, 0x181f0000, 0x000027e1,
	0x00000000, 0x00000000, 0x00000000, 0x2b981468,
	0x00000000, 0x00000000, 0x00000000, 0x10f00000,
	0x00002f10, 0x00000000, 0x00000000, 0x00000000,
	0x32390dc7, 0x00000000, 0x00000000, 0x00000000,
	0x0af50000, 0x0000350b, 0x00000000, 0x00000000,
	0x00000000, 0x3781087f, 0x00000000, 0x00000000,
	0x00000000, 0x06660000, 0x0000399a, 0x00000000,
	0x00000000, 0x00000000, 0x3b5904a7, 0x00000000,
	0x00000000, 0x00000000, 0x033c0000, 0x00003cc4,
	0x00000000, 0x00000000, 0x00000000, 0x3de1021f,
	0x00000000, 0x00000000, 0x00000000, 0x01470000,
	0x00003eb9, 0x00000000, 0x00000000, 0x00000000,
	0x3f5300ad, 0x00000000, 0x00000000, 0x00000000,
	0x00480000, 0x00003fb8, 0x00000000, 0x00000000,
	0x00000000, 0x3fef0011, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00004000, 0x00000000,
	0x00000000, 0x00000000, 0x20002000, 0x00000000,
	0x00000000, 0x00000000, 0x1c030000, 0x000023fd,
	0x00000000, 0x00000000, 0x00000000, 0x27e1181f,
	0x00000000, 0x00000000, 0x00000000, 0x14680000,
	0x00002b98, 0x00000000, 0x00000000, 0x00000000,
	0x2f1010f0, 0x00000000, 0x00000000, 0x00000000,
	0x0dc70000, 0x00003239, 0x00000000, 0x00000000,
	0x00000000, 0x350b0af5, 0x00000000, 0x00000000,
	0x00000000, 0x087f0000, 0x00003781, 0x00000000,
	0x00000000, 0x00000000, 0x399a0666, 0x00000000,
	0x00000000, 0x00000000, 0x04a70000, 0x00003b59,
	0x00000000, 0x00000000, 0x00000000, 0x3cc4033c,
	0x00000000, 0x00000000, 0x00000000, 0x021f0000,
};

#define H_COEF_SIZE ARRAY_SIZE(horkernel)

static const u32 verkernel[] = {
	0x00000000, 0x20000000, 0x00002000, 0x00000000,
	0x00000000, 0x00000000, 0x23fd1c03, 0x00000000,
	0x00000000, 0x00000000, 0x181f0000, 0x000027e1,
	0x00000000, 0x00000000, 0x00000000, 0x2b981468,
	0x00000000, 0x00000000, 0x00000000, 0x10f00000,
	0x00002f10, 0x00000000, 0x00000000, 0x00000000,
	0x32390dc7, 0x00000000, 0x00000000, 0x00000000,
	0x0af50000, 0x0000350b, 0x00000000, 0x00000000,
	0x00000000, 0x3781087f, 0x00000000, 0x00000000,
	0x00000000, 0x06660000, 0x0000399a, 0x00000000,
	0x00000000, 0x00000000, 0x3b5904a7, 0x00000000,
	0x00000000, 0x00000000, 0x033c0000, 0x00003cc4,
	0x00000000, 0x00000000, 0x00000000, 0x3de1021f,
	0x00000000, 0x00000000, 0x00000000, 0x01470000,
	0x00003eb9, 0x00000000, 0x00000000, 0x00000000,
	0x3f5300ad, 0x00000000, 0x00000000, 0x00000000,
	0x00480000, 0x00003fb8, 0x00000000, 0x00000000,
	0x00000000, 0x3fef0011, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00004000, 0x00000000,
	0xcdcd0000, 0xfdfdfdfd, 0xabababab, 0xabababab,
	0x00000000, 0x00000000, 0x5ff5f456, 0x000f5f58,
	0x02cc6c78, 0x02cc0c28, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
};

#define V_COEF_SIZE ARRAY_SIZE(verkernel)

/*
 * RGB 709->2020 conversion parameters
 */
static const u16 RGB2RGB[RGB_TO_RGB_TABLE_SIZE] = {
	10279,	5395,	709,
	1132,	15065,	187,
	269,	1442,	14674
};

/*
 * YUV601 to RGB conversion parameters
 * YUV2RGB[0]  - [8] : C0 - C8;
 * YUV2RGB[9]  - [11]: D0 - D2;
 * YUV2RGB[12] - [13]: Y clamp min & max calue;
 * YUV2RGB[14] - [15]: UV clamp min & max calue;
 */
static const s32 YUV601_2RGB[YUV_TO_RGB_TABLE_SIZE] = {
	1196,	0,		1640,	1196,
	-404,	-836,		1196,	2076,
	0,	-916224,	558336,	-1202944,
	64,	940,		64,	960
};

/*
 * YUV709 to RGB conversion parameters
 * YUV2RGB[0]  - [8] : C0 - C8;
 * YUV2RGB[9]  - [11]: D0 - D2;
 * YUV2RGB[12] - [13]: Y clamp min & max calue;
 * YUV2RGB[14] - [15]: UV clamp min & max calue;
 */
static s32 YUV709_2RGB[YUV_TO_RGB_TABLE_SIZE] = {
	1196,		0,		1844,	1196,
	-220,		-548,	1196,	2172,
	0,			-1020672, 316672,  -1188608,
	64,			940,		64,		960
};

/*
 * YUV2020 to RGB conversion parameters
 * YUV2RGB[0]  - [8] : C0 - C8;
 * YUV2RGB[9]  - [11]: D0 - D2;
 * YUV2RGB[12] - [13]: Y clamp min & max calue;
 * YUV2RGB[14] - [15]: UV clamp min & max calue;
 */
static s32 YUV2020_2RGB[YUV_TO_RGB_TABLE_SIZE] = {
	1196, 0, 1724, 1196,
	-192, -668, 1196, 2200,
	0, -959232, 363776, -1202944,
	64, 940, 64, 960
};

/*
 * RGB to YUV2020 conversion parameters
 * RGB2YUV[0] - [8] : C0 - C8;
 * RGB2YUV[9] - [11]: D0 - D2;
 */
static s16 RGB2YUV[RGB_TO_YUV_TABLE_SIZE] = {
	230,	594,	52,
	-125,  -323,	448,
	448,   -412,   -36,
	64,		512,	512
};

/* one is for primary plane and the other is for all overlay planes */
static const struct dc_hw_plane_reg dc_plane_reg[] = {
	{
		.y_address		= DC_FRAMEBUFFER_ADDRESS,
		.u_address		= DC_FRAMEBUFFER_U_ADDRESS,
		.v_address		= DC_FRAMEBUFFER_V_ADDRESS,
		.y_stride		= DC_FRAMEBUFFER_STRIDE,
		.u_stride		= DC_FRAMEBUFFER_U_STRIDE,
		.v_stride		= DC_FRAMEBUFFER_V_STRIDE,
		.size			= DC_FRAMEBUFFER_SIZE,
		.top_left		= DC_FRAMEBUFFER_TOP_LEFT,
		.bottom_right	= DC_FRAMEBUFFER_BOTTOM_RIGHT,
		.scale_factor_x			= DC_FRAMEBUFFER_SCALE_FACTOR_X,
		.scale_factor_y			= DC_FRAMEBUFFER_SCALE_FACTOR_Y,
		.h_filter_coef_index	= DC_FRAMEBUFFER_H_FILTER_COEF_INDEX,
		.h_filter_coef_data		= DC_FRAMEBUFFER_H_FILTER_COEF_DATA,
		.v_filter_coef_index	= DC_FRAMEBUFFER_V_FILTER_COEF_INDEX,
		.v_filter_coef_data		= DC_FRAMEBUFFER_V_FILTER_COEF_DATA,
		.init_offset			= DC_FRAMEBUFFER_INIT_OFFSET,
		.color_key				= DC_FRAMEBUFFER_COLOR_KEY,
		.color_key_high			= DC_FRAMEBUFFER_COLOR_KEY_HIGH,
		.clear_value			= DC_FRAMEBUFFER_CLEAR_VALUE,
		.color_table_index		= DC_FRAMEBUFFER_COLOR_TABLE_INDEX,
		.color_table_data		= DC_FRAMEBUFFER_COLOR_TABLE_DATA,
		.scale_config			= DC_FRAMEBUFFER_SCALE_CONFIG,
		.water_mark				= DC_FRAMEBUFFER_WATER_MARK,
		.degamma_index			= DC_FRAMEBUFFER_DEGAMMA_INDEX,
		.degamma_data			= DC_FRAMEBUFFER_DEGAMMA_DATA,
		.degamma_ex_data		= DC_FRAMEBUFFER_DEGAMMA_EX_DATA,
		.src_global_color		= DC_FRAMEBUFFER_SRC_GLOBAL_COLOR,
		.dst_global_color		= DC_FRAMEBUFFER_DST_GLOBAL_COLOR,
		.blend_config			= DC_FRAMEBUFFER_BLEND_CONFIG,
		.roi_origin				= DC_FRAMEBUFFER_ROI_ORIGIN,
		.roi_size				= DC_FRAMEBUFFER_ROI_SIZE,
		.yuv_to_rgb_coef0			= DC_FRAMEBUFFER_YUVTORGB_COEF0,
		.yuv_to_rgb_coef1			= DC_FRAMEBUFFER_YUVTORGB_COEF1,
		.yuv_to_rgb_coef2			= DC_FRAMEBUFFER_YUVTORGB_COEF2,
		.yuv_to_rgb_coef3			= DC_FRAMEBUFFER_YUVTORGB_COEF3,
		.yuv_to_rgb_coef4			= DC_FRAMEBUFFER_YUVTORGB_COEF4,
		.yuv_to_rgb_coefd0			= DC_FRAMEBUFFER_YUVTORGB_COEFD0,
		.yuv_to_rgb_coefd1			= DC_FRAMEBUFFER_YUVTORGB_COEFD1,
		.yuv_to_rgb_coefd2			= DC_FRAMEBUFFER_YUVTORGB_COEFD2,
		.y_clamp_bound				= DC_FRAMEBUFFER_Y_CLAMP_BOUND,
		.uv_clamp_bound				= DC_FRAMEBUFFER_UV_CLAMP_BOUND,
		.rgb_to_rgb_coef0			= DC_FRAMEBUFFER_RGBTORGB_COEF0,
		.rgb_to_rgb_coef1			= DC_FRAMEBUFFER_RGBTORGB_COEF1,
		.rgb_to_rgb_coef2			= DC_FRAMEBUFFER_RGBTORGB_COEF2,
		.rgb_to_rgb_coef3			= DC_FRAMEBUFFER_RGBTORGB_COEF3,
		.rgb_to_rgb_coef4			= DC_FRAMEBUFFER_RGBTORGB_COEF4,
	},
	{
		.y_address		= DC_OVERLAY_ADDRESS,
		.u_address		= DC_OVERLAY_U_ADDRESS,
		.v_address		= DC_OVERLAY_V_ADDRESS,
		.y_stride		= DC_OVERLAY_STRIDE,
		.u_stride		= DC_OVERLAY_U_STRIDE,
		.v_stride		= DC_OVERLAY_V_STRIDE,
		.size			= DC_OVERLAY_SIZE,
		.top_left		= DC_OVERLAY_TOP_LEFT,
		.bottom_right	= DC_OVERLAY_BOTTOM_RIGHT,
		.scale_factor_x	= DC_OVERLAY_SCALE_FACTOR_X,
		.scale_factor_y	= DC_OVERLAY_SCALE_FACTOR_Y,
		.h_filter_coef_index = DC_OVERLAY_H_FILTER_COEF_INDEX,
		.h_filter_coef_data  = DC_OVERLAY_H_FILTER_COEF_DATA,
		.v_filter_coef_index = DC_OVERLAY_V_FILTER_COEF_INDEX,
		.v_filter_coef_data  = DC_OVERLAY_V_FILTER_COEF_DATA,
		.init_offset		 = DC_OVERLAY_INIT_OFFSET,
		.color_key			 = DC_OVERLAY_COLOR_KEY,
		.color_key_high			= DC_OVERLAY_COLOR_KEY_HIGH,
		.clear_value		 = DC_OVERLAY_CLEAR_VALUE,
		.color_table_index	 = DC_OVERLAY_COLOR_TABLE_INDEX,
		.color_table_data	 = DC_OVERLAY_COLOR_TABLE_DATA,
		.scale_config		 = DC_OVERLAY_SCALE_CONFIG,
		.water_mark				= DC_OVERLAY_WATER_MARK,
		.degamma_index		 = DC_OVERLAY_DEGAMMA_INDEX,
		.degamma_data		 = DC_OVERLAY_DEGAMMA_DATA,
		.degamma_ex_data	 = DC_OVERLAY_DEGAMMA_EX_DATA,
		.src_global_color	 = DC_OVERLAY_SRC_GLOBAL_COLOR,
		.dst_global_color	 = DC_OVERLAY_DST_GLOBAL_COLOR,
		.blend_config		 = DC_OVERLAY_BLEND_CONFIG,
		.roi_origin				= DC_OVERLAY_ROI_ORIGIN,
		.roi_size				= DC_OVERLAY_ROI_SIZE,
		.yuv_to_rgb_coef0		 = DC_OVERLAY_YUVTORGB_COEF0,
		.yuv_to_rgb_coef1		 = DC_OVERLAY_YUVTORGB_COEF1,
		.yuv_to_rgb_coef2		 = DC_OVERLAY_YUVTORGB_COEF2,
		.yuv_to_rgb_coef3		 = DC_OVERLAY_YUVTORGB_COEF3,
		.yuv_to_rgb_coef4			= DC_OVERLAY_YUVTORGB_COEF4,
		.yuv_to_rgb_coefd0			= DC_OVERLAY_YUVTORGB_COEFD0,
		.yuv_to_rgb_coefd1			= DC_OVERLAY_YUVTORGB_COEFD1,
		.yuv_to_rgb_coefd2			= DC_OVERLAY_YUVTORGB_COEFD2,
		.y_clamp_bound		 = DC_OVERLAY_Y_CLAMP_BOUND,
		.uv_clamp_bound		 = DC_OVERLAY_UV_CLAMP_BOUND,
		.rgb_to_rgb_coef0		 = DC_OVERLAY_RGBTORGB_COEF0,
		.rgb_to_rgb_coef1		 = DC_OVERLAY_RGBTORGB_COEF1,
		.rgb_to_rgb_coef2		 = DC_OVERLAY_RGBTORGB_COEF2,
		.rgb_to_rgb_coef3		 = DC_OVERLAY_RGBTORGB_COEF3,
		.rgb_to_rgb_coef4		 = DC_OVERLAY_RGBTORGB_COEF4,
	},
};

static const struct dc_hw_funcs hw_func;

static inline u32 hi_read(struct dc_hw *hw, u32 reg)
{
	return readl(hw->hi_base + reg);
}

static inline void hi_write(struct dc_hw *hw, u32 reg, u32 value)
{
	writel(value, hw->hi_base + reg);
}

static inline void dc_write(struct dc_hw *hw, u32 reg, u32 value)
{
	writel(value, hw->reg_base + reg - DC_REG_BASE);
}

static inline u32 dc_read(struct dc_hw *hw, u32 reg)
{
	u32 value = readl(hw->reg_base + reg - DC_REG_BASE);

	return value;
}

static inline void dc_set_clear(struct dc_hw *hw, u32 reg, u32 set, u32 clear)
{
	u32 value = dc_read(hw, reg);

	value &= ~clear;
	value |= set;
	dc_write(hw, reg, value);
}

static void load_default_filter(struct dc_hw *hw,
				const struct dc_hw_plane_reg *reg, u32 offset)
{
	u8 i;

	dc_write(hw, reg->scale_config + offset, 0x33);
	dc_write(hw, reg->init_offset + offset, 0x80008000);
	dc_write(hw, reg->h_filter_coef_index + offset, 0x00);
	for (i = 0; i < H_COEF_SIZE; i++)
		dc_write(hw, reg->h_filter_coef_data + offset, horkernel[i]);

	dc_write(hw, reg->v_filter_coef_index + offset, 0x00);
	for (i = 0; i < V_COEF_SIZE; i++)
		dc_write(hw, reg->v_filter_coef_data + offset, verkernel[i]);
}

static void load_rgb_to_rgb(struct dc_hw *hw, const struct dc_hw_plane_reg *reg,
			    u32 offset, const u16 *table)
{
	dc_write(hw, reg->rgb_to_rgb_coef0 + offset, table[0] | (table[1] << 16));
	dc_write(hw, reg->rgb_to_rgb_coef1 + offset, table[2] | (table[3] << 16));
	dc_write(hw, reg->rgb_to_rgb_coef2 + offset, table[4] | (table[5] << 16));
	dc_write(hw, reg->rgb_to_rgb_coef3 + offset, table[6] | (table[7] << 16));
	dc_write(hw, reg->rgb_to_rgb_coef4 + offset, table[8]);
}

static void load_yuv_to_rgb(struct dc_hw *hw, const struct dc_hw_plane_reg *reg,
			    u32 offset, const s32 *table)
{
	dc_write(hw, reg->yuv_to_rgb_coef0 + offset,
		 (0xFFFF & table[0]) | (table[1] << 16));
	dc_write(hw, reg->yuv_to_rgb_coef1 + offset,
		 (0xFFFF & table[2]) | (table[3] << 16));
	dc_write(hw, reg->yuv_to_rgb_coef2 + offset,
		 (0xFFFF & table[4]) | (table[5] << 16));
	dc_write(hw, reg->yuv_to_rgb_coef3 + offset,
		 (0xFFFF & table[6]) | (table[7] << 16));
	dc_write(hw, reg->yuv_to_rgb_coef4 + offset, table[8]);
	dc_write(hw, reg->yuv_to_rgb_coefd0 + offset, table[9]);
	dc_write(hw, reg->yuv_to_rgb_coefd1 + offset, table[10]);
	dc_write(hw, reg->yuv_to_rgb_coefd2 + offset, table[11]);
	dc_write(hw, reg->y_clamp_bound + offset, table[12] | (table[13] << 16));
	dc_write(hw, reg->uv_clamp_bound + offset, table[14] | (table[15] << 16));
}

static void load_rgb_to_yuv(struct dc_hw *hw, u32 offset, s16 *table)
{
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF0 + offset,
		 table[0] | (table[1] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF1 + offset,
		 table[2] | (table[3] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF2 + offset,
		 table[4] | (table[5] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF3 + offset,
		 table[6] | (table[7] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF4 + offset, table[8]);
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEFD0 + offset, table[9]);
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEFD1 + offset, table[10]);
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEFD2 + offset, table[11]);
}

static bool is_rgb(enum dc_hw_color_format format)
{
	switch (format) {
	case FORMAT_X4R4G4B4:
	case FORMAT_A4R4G4B4:
	case FORMAT_X1R5G5B5:
	case FORMAT_A1R5G5B5:
	case FORMAT_R5G6B5:
	case FORMAT_X8R8G8B8:
	case FORMAT_A8R8G8B8:
	case FORMAT_A2R10G10B10:
		return true;
	default:
		return false;
	}
}

static u32 get_addr_offset(u32 id)
{
	u32 offset = 0;

	switch (id) {
	case PRIMARY_PLANE_1:
	case OVERLAY_PLANE_1:
		offset = 0x04;
		break;
	case OVERLAY_PLANE_2:
		offset = 0x08;
		break;
	case OVERLAY_PLANE_3:
		offset = 0x0C;
		break;
	default:
		break;
	}

	return offset;
}

int dc_hw_init(struct dc_hw *hw)
{
	u8 i, id, panel_num, layer_num;
	u32 offset;

	hw->func = (struct dc_hw_funcs *)&hw_func;

	layer_num = hw->info->layer_num;
	for (i = 0; i < layer_num; i++) {
		id = hw->info->planes[i].id;
		offset = get_addr_offset(id);
		if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
			hw->reg[i] = dc_plane_reg[0];
		else
			hw->reg[i] = dc_plane_reg[1];

		load_default_filter(hw, &hw->reg[i], offset);
		load_rgb_to_rgb(hw, &hw->reg[i], offset, RGB2RGB);
	}

	panel_num = hw->info->panel_num;
	for (i = 0; i < panel_num; i++) {
		offset = i << 2;

		load_rgb_to_yuv(hw, offset, RGB2YUV);
		dc_write(hw, DC_DISPLAY_PANEL_CONFIG + offset, 0x111);

		offset = i ? DC_CURSOR_OFFSET : 0;
		dc_write(hw, DC_CURSOR_BACKGROUND + offset, 0x00FFFFFF);
		dc_write(hw, DC_CURSOR_FOREGROUND + offset, 0x00AAAAAA);
	}

	return 0;
}

void dc_hw_deinit(struct dc_hw *hw)
{
	/* Nothing to do */
}

void dc_hw_update_plane(struct dc_hw *hw, u8 id,
			struct dc_hw_fb *fb, struct dc_hw_scale *scale,
			struct dc_hw_position *pos, struct dc_hw_blend *blend)
{
	struct dc_hw_plane *plane = &hw->plane[id];

	if (plane) {
		if (fb) {
			if (!fb->enable)
				plane->fb.enable = false;
			else
				memcpy(&plane->fb, fb,
				       sizeof(*fb) - sizeof(fb->dirty));
			plane->fb.dirty = true;
		}
		if (scale) {
			memcpy(&plane->scale, scale,
			       sizeof(*scale) - sizeof(scale->dirty));
			plane->scale.dirty = true;
		}
		if (pos) {
			memcpy(&plane->pos, pos,
			       sizeof(*pos) - sizeof(pos->dirty));
			plane->pos.dirty = true;
		}
		if (blend) {
			memcpy(&plane->blend, blend,
			       sizeof(*blend) - sizeof(blend->dirty));
			plane->blend.dirty = true;
		}
	}
}

void dc_hw_update_cursor(struct dc_hw *hw, u8 id, struct dc_hw_cursor *cursor)
{
	memcpy(&hw->cursor[id], cursor, sizeof(*cursor) - sizeof(cursor->dirty));
	hw->cursor[id].dirty = true;
}

void dc_hw_update_gamma(struct dc_hw *hw, u8 id, u16 index,
			u16 r, u16 g, u16 b)
{
	if (index >= hw->info->gamma_size)
		return;

	hw->gamma[id].gamma[index][0] = r;
	hw->gamma[id].gamma[index][1] = g;
	hw->gamma[id].gamma[index][2] = b;
	hw->gamma[id].dirty = true;
}

void dc_hw_enable_gamma(struct dc_hw *hw, u8 id, bool enable)
{
	hw->gamma[id].enable = enable;
	hw->gamma[id].dirty = true;
}

void dc_hw_setup_display(struct dc_hw *hw, struct dc_hw_display *display)
{
	u8 id = display->id;

	memcpy(&hw->display[id], display, sizeof(*display));

	hw->func->display(hw, display);
}

void dc_hw_enable_interrupt(struct dc_hw *hw, bool enable)
{
	if (enable)
		hi_write(hw, AQ_INTR_ENBL, 0xFFFFFFFF);
	else
		hi_write(hw, AQ_INTR_ENBL, 0);
}

u32 dc_hw_get_interrupt(struct dc_hw *hw)
{
	return hi_read(hw, AQ_INTR_ACKNOWLEDGE);
}

void dc_hw_enable_shadow_register(struct dc_hw *hw, bool enable)
{
	u32 i, offset;
	u8 id, layer_num = hw->info->layer_num;
	u8 panel_num = hw->info->panel_num;

	for (i = 0; i < layer_num; i++) {
		id = hw->info->planes[i].id;
		offset = get_addr_offset(id);
		if (enable) {
			if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
				dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX + offset,
					     PRIMARY_SHADOW_EN, 0);
			else
				dc_set_clear(hw, DC_OVERLAY_CONFIG + offset,
					     OVERLAY_SHADOW_EN, 0);
		} else {
			if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
				dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX + offset,
					     0, PRIMARY_SHADOW_EN);
			else
				dc_set_clear(hw, DC_OVERLAY_CONFIG + offset,
					     0, OVERLAY_SHADOW_EN);
		}
	}

	for (i = 0; i < panel_num; i++) {
		offset = i << 2;
		if (enable)
			dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG_EX + offset, 0, PANEL_SHADOW_EN);
		else
			dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG_EX + offset, PANEL_SHADOW_EN, 0);
	}
}

void dc_hw_set_out(struct dc_hw *hw, enum dc_hw_out out, u8 id)
{
	if (out <= OUT_DP)
		hw->out[id] = out;
}

static void gamma_ex_commit(struct dc_hw *hw)
{
	u8 panel_num = hw->info->panel_num;
	u16 i, j;
	u32 value;

	for (j = 0; j < panel_num; j++) {
		if (hw->gamma[j].dirty) {
			if (hw->gamma[j].enable) {
				dc_write(hw, DC_DISPLAY_GAMMA_EX_INDEX + (j << 2), 0x00);
				for (i = 0; i < GAMMA_EX_SIZE; i++) {
					value = hw->gamma[j].gamma[i][2] |
						(hw->gamma[j].gamma[i][1] << 12);
					dc_write(hw, DC_DISPLAY_GAMMA_EX_DATA + (j << 2), value);
					dc_write(hw, DC_DISPLAY_GAMMA_EX_ONE_DATA + (j << 2),
						 hw->gamma[j].gamma[i][0]);
				}
				dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + (j << 2),
					     PANEL_GAMMA_EN, 0);
			} else {
				dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + (j << 2),
					     0, PANEL_GAMMA_EN);
			}
			hw->gamma[j].dirty = false;
		}
	}
}

static void plane_ex_commit_primary(struct dc_hw *hw, struct dc_hw_plane *plane, u32 i, u32 offset)
{
	if (plane->fb.dirty) {
		if (is_rgb(plane->fb.format)) {
			dc_set_clear(hw,
				     DC_FRAMEBUFFER_CONFIG_EX + offset,
				     PRIMARY_RGB2RGB_EN, PRIMARY_YUVCLAMP_EN);
		} else {
			dc_set_clear(hw,
				     DC_FRAMEBUFFER_CONFIG_EX + offset,
				     PRIMARY_YUVCLAMP_EN, PRIMARY_RGB2RGB_EN);

			switch (plane->fb.yuv_color_space) {
			case COLOR_SPACE_601:
				load_yuv_to_rgb(hw, &hw->reg[i], offset, YUV601_2RGB);
				break;
			case COLOR_SPACE_709:
				load_yuv_to_rgb(hw, &hw->reg[i], offset, YUV709_2RGB);
				break;
			case COLOR_SPACE_2020:
				load_yuv_to_rgb(hw, &hw->reg[i], offset, YUV2020_2RGB);
				break;
			default:
				break;
			}
		}

		if (plane->fb.enable) {
			dc_write(hw, hw->reg[i].y_address + offset,
				 plane->fb.y_address);
			dc_write(hw, hw->reg[i].u_address + offset,
				 plane->fb.u_address);
			dc_write(hw, hw->reg[i].v_address + offset,
				 plane->fb.v_address);
			dc_write(hw, hw->reg[i].y_stride + offset,
				 plane->fb.y_stride);
			dc_write(hw, hw->reg[i].u_stride + offset,
				 plane->fb.u_stride);
			dc_write(hw, hw->reg[i].v_stride + offset,
				 plane->fb.v_stride);
			dc_write(hw, hw->reg[i].size + offset,
				 FB_SIZE(plane->fb.width, plane->fb.height));
			dc_write(hw, hw->reg[i].water_mark + offset,
				 plane->fb.water_mark);

			if (plane->fb.clear_enable)
				dc_write(hw, hw->reg[i].clear_value + offset,
					 plane->fb.clear_value);
		}

		dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG + offset,
			     PRIMARY_FORMAT(plane->fb.format) |
			     PRIMARY_UV_SWIZ(plane->fb.uv_swizzle) |
			     PRIMARY_SWIZ(plane->fb.swizzle) |
			     PRIMARY_TILE(plane->fb.tile_mode) |
			     PRIMARY_YUV_COLOR(plane->fb.yuv_color_space) |
			     PRIMARY_ROTATION(plane->fb.rotation) |
			     PRIMARY_CLEAR_EN(plane->fb.clear_enable),
			     PRIMARY_FORMAT_MASK |
			     PRIMARY_UV_SWIZ_MASK |
			     PRIMARY_SWIZ_MASK |
			     PRIMARY_TILE_MASK |
			     PRIMARY_YUV_COLOR_MASK |
			     PRIMARY_ROTATION_MASK |
			     PRIMARY_CLEAR_EN_MASK);
		dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX + offset,
			     PRIMARY_DECODER_EN(plane->fb.dec_enable) |
			     PRIMARY_EN(plane->fb.enable) |
			     PRIMARY_ZPOS(plane->fb.zpos) |
			     PRIMARY_CHANNEL(plane->fb.display_id),
			     PRIMARY_DECODER_EN_EN_MASK |
			     PRIMARY_EN_MASK |
			     PRIMARY_ZPOS_MASK |
			     PRIMARY_CHANNEL_MASK);

		plane->fb.dirty = false;
	}

	if (plane->scale.dirty) {
		if (plane->scale.enable) {
			dc_write(hw, hw->reg[i].scale_factor_x + offset,
				 plane->scale.scale_factor_x);
			dc_write(hw, hw->reg[i].scale_factor_y + offset,
				 plane->scale.scale_factor_y);
				dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG + offset,
					     PRIMARY_SCALE_EN, 0);
		} else {
			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG + offset,
				     0, PRIMARY_SCALE_EN);
		}
		plane->scale.dirty = false;
	}

	if (plane->pos.dirty) {
		dc_write(hw, hw->reg[i].top_left + offset,
			 X_POS(plane->pos.start_x) |
			 Y_POS(plane->pos.start_y));
		dc_write(hw, hw->reg[i].bottom_right + offset,
			 X_POS(plane->pos.end_x) |
			 Y_POS(plane->pos.end_y));
		plane->pos.dirty = false;
	}

	if (plane->blend.dirty) {
		dc_write(hw, hw->reg[i].src_global_color + offset,
			 PRIMARY_ALPHA_LEN(plane->blend.alpha));
		dc_write(hw, hw->reg[i].dst_global_color + offset,
			 PRIMARY_ALPHA_LEN(plane->blend.alpha));
		switch (plane->blend.blend_mode) {
		case DRM_MODE_BLEND_PREMULTI:
			dc_write(hw, hw->reg[i].blend_config + offset, BLEND_PREMULTI);
			break;
		case DRM_MODE_BLEND_COVERAGE:
			dc_write(hw, hw->reg[i].blend_config + offset, BLEND_COVERAGE);
			break;
		case DRM_MODE_BLEND_PIXEL_NONE:
			dc_write(hw, hw->reg[i].blend_config + offset, BLEND_PIXEL_NONE);
			break;
		default:
			break;
		}
		plane->blend.dirty = false;
	}
}

static void plane_ex_commit_overlay(struct dc_hw *hw, struct dc_hw_plane *plane,
				    u32 i, u32 offset)
{
	if (plane->fb.dirty) {
		if (is_rgb(plane->fb.format)) {
			dc_set_clear(hw, DC_OVERLAY_CONFIG + offset,
				     OVERLAY_RGB2RGB_EN, OVERLAY_CLAMP_EN);
		} else {
			dc_set_clear(hw, DC_OVERLAY_CONFIG + offset,
				     OVERLAY_CLAMP_EN, OVERLAY_RGB2RGB_EN);

			switch (plane->fb.yuv_color_space) {
			case COLOR_SPACE_601:
				load_yuv_to_rgb(hw, &hw->reg[i], offset, YUV601_2RGB);
				break;
			case COLOR_SPACE_709:
				load_yuv_to_rgb(hw, &hw->reg[i], offset, YUV709_2RGB);
				break;
			case COLOR_SPACE_2020:
				load_yuv_to_rgb(hw, &hw->reg[i], offset, YUV2020_2RGB);
				break;
			default:
				break;
			}
		}

		if (plane->fb.enable) {
			dc_write(hw, hw->reg[i].y_address + offset,
				 plane->fb.y_address);
			dc_write(hw, hw->reg[i].u_address + offset,
				 plane->fb.u_address);
			dc_write(hw, hw->reg[i].v_address + offset,
				 plane->fb.v_address);
			dc_write(hw, hw->reg[i].y_stride + offset,
				 plane->fb.y_stride);
			dc_write(hw, hw->reg[i].u_stride + offset,
				 plane->fb.u_stride);
			dc_write(hw, hw->reg[i].v_stride + offset,
				 plane->fb.v_stride);
			dc_write(hw, hw->reg[i].size + offset,
				 FB_SIZE(plane->fb.width, plane->fb.height));
			dc_write(hw, hw->reg[i].water_mark + offset,
				 plane->fb.water_mark);

			if (plane->fb.clear_enable)
				dc_write(hw, hw->reg[i].clear_value + offset,
					 plane->fb.clear_value);
		}

		dc_set_clear(hw, DC_OVERLAY_CONFIG + offset,
			     OVERLAY_DEC_EN(plane->fb.dec_enable) |
			     OVERLAY_CLEAR_EN(plane->fb.clear_enable) |
			     OVERLAY_FB_EN(plane->fb.enable) |
			     OVERLAY_FORMAT(plane->fb.format) |
			     OVERLAY_UV_SWIZ(plane->fb.uv_swizzle) |
			     OVERLAY_SWIZ(plane->fb.swizzle) |
			     OVERLAY_TILE(plane->fb.tile_mode) |
			     OVERLAY_YUV_COLOR(plane->fb.yuv_color_space) |
			     OVERLAY_ROTATION(plane->fb.rotation),
			     OVERLAY_DEC_EN_MASK |
			     OVERLAY_CLEAR_EN_MASK |
			     OVERLAY_FB_EN_MASK |
			     OVERLAY_FORMAT_MASK |
			     OVERLAY_UV_SWIZ_MASK |
			     OVERLAY_SWIZ_MASK |
			     OVERLAY_TILE_MASK |
			     OVERLAY_YUV_COLOR_MASK |
			     OVERLAY_ROTATION_MASK);

		dc_set_clear(hw, DC_OVERLAY_CONFIG_EX + offset,
			     OVERLAY_LAYER_SEL(plane->fb.zpos) |
			     OVERLAY_PANEL_SEL(plane->fb.display_id),
			     OVERLAY_LAYER_SEL_MASK |
			     OVERLAY_PANEL_SEL_MASK);

		plane->fb.dirty = false;
	}

	if (plane->scale.dirty) {
		if (plane->scale.enable) {
			dc_write(hw, hw->reg[i].scale_factor_x + offset,
				 plane->scale.scale_factor_x);
			dc_write(hw, hw->reg[i].scale_factor_y + offset,
				 plane->scale.scale_factor_y);
			dc_set_clear(hw, DC_OVERLAY_SCALE_CONFIG + offset,
				     OVERLAY_SCALE_EN, 0);
		} else {
			dc_set_clear(hw, DC_OVERLAY_SCALE_CONFIG + offset,
				     0, OVERLAY_SCALE_EN);
		}
		plane->scale.dirty = false;
	}

	if (plane->pos.dirty) {
		dc_write(hw, hw->reg[i].top_left + offset,
			 X_POS(plane->pos.start_x) |
			 Y_POS(plane->pos.start_y));
		dc_write(hw, hw->reg[i].bottom_right + offset,
			 X_POS(plane->pos.end_x) |
			 Y_POS(plane->pos.end_y));
		plane->pos.dirty = false;
	}

	if (plane->blend.dirty) {
		dc_write(hw, hw->reg[i].src_global_color + offset,
			 OVERLAY_ALPHA_LEN(plane->blend.alpha));
		dc_write(hw, hw->reg[i].dst_global_color + offset,
			 OVERLAY_ALPHA_LEN(plane->blend.alpha));
		switch (plane->blend.blend_mode) {
		case DRM_MODE_BLEND_PREMULTI:
			dc_write(hw, hw->reg[i].blend_config + offset, BLEND_PREMULTI);
			break;
		case DRM_MODE_BLEND_COVERAGE:
			dc_write(hw, hw->reg[i].blend_config + offset, BLEND_COVERAGE);
			break;
		case DRM_MODE_BLEND_PIXEL_NONE:
			dc_write(hw, hw->reg[i].blend_config + offset, BLEND_PIXEL_NONE);
			break;
		default:
			break;
		}
		plane->blend.dirty = false;
	}
}

static void plane_ex_commit(struct dc_hw *hw)
{
	struct dc_hw_plane *plane;
	u8 id, layer_num = hw->info->layer_num;
	u32 i, offset;

	for (i = 0; i < layer_num; i++) {
		plane = &hw->plane[i];
		id = hw->info->planes[i].id;
		offset = get_addr_offset(id);
		if (id == PRIMARY_PLANE_0 || id == PRIMARY_PLANE_1)
			plane_ex_commit_primary(hw, plane, i, offset);
		else
			plane_ex_commit_overlay(hw, plane, i, offset);
	}
}

static void setup_display(struct dc_hw *hw, struct dc_hw_display *display)
{
	u8 id = display->id;
	u32 dpi_cfg, offset = id << 2;

	if (hw->display[id].enable) {
		switch (display->bus_format) {
		case MEDIA_BUS_FMT_RGB565_1X16:
			dpi_cfg = 0;
			break;
		case MEDIA_BUS_FMT_RGB666_1X18:
			dpi_cfg = 3;
			break;
		case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
			dpi_cfg = 4;
			break;
		case MEDIA_BUS_FMT_RGB888_1X24:
			dpi_cfg = 5;
			break;
		case MEDIA_BUS_FMT_RGB101010_1X30:
			dpi_cfg = 6;
			break;
		default:
			dpi_cfg = 5;
			break;
		}
		dc_write(hw, DC_DISPLAY_DPI_CONFIG + offset, dpi_cfg);

		if (id == 0)
			dc_set_clear(hw, DC_DISPLAY_PANEL_START, 0, PANEL0_EN | TWO_PANEL_EN);
		else
			dc_set_clear(hw, DC_DISPLAY_PANEL_START, 0, PANEL1_EN | TWO_PANEL_EN);

		dc_write(hw, DC_DISPLAY_H + offset,
			 H_ACTIVE_LEN(hw->display[id].h_active) |
			 H_TOTAL_LEN(hw->display[id].h_total));

		dc_write(hw, DC_DISPLAY_H_SYNC + offset,
			 H_SYNC_START_LEN(hw->display[id].h_sync_start) |
			 H_SYNC_END_LEN(hw->display[id].h_sync_end) |
			 H_POLARITY_LEN(hw->display[id].h_sync_polarity ? 0 : 1) |
			 H_PLUS_LEN(1));

		dc_write(hw, DC_DISPLAY_V + offset,
			 V_ACTIVE_LEN(hw->display[id].v_active) |
			 V_TOTAL_LEN(hw->display[id].v_total));

		dc_write(hw, DC_DISPLAY_V_SYNC + offset,
			 V_SYNC_START_LEN(hw->display[id].v_sync_start) |
			 V_SYNC_END_LEN(hw->display[id].v_sync_end) |
			 V_POLARITY_LEN(hw->display[id].v_sync_polarity ? 0 : 1) |
			 V_PLUS_LEN(1));

		if (hw->info->pipe_sync)
			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX,
				     0, PRIMARY_SYNC0_EN | PRIMARY_SYNC1_EN);

		dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + offset, PANEL_OUTPUT_EN, 0);
		if (id == 0)
			dc_set_clear(hw, DC_DISPLAY_PANEL_START, PANEL0_EN, SYNC_EN);
		else
			dc_set_clear(hw, DC_DISPLAY_PANEL_START, PANEL1_EN, SYNC_EN);
	} else {
		dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + offset, 0, PANEL_OUTPUT_EN);

		if (id == 0)
			dc_set_clear(hw, DC_DISPLAY_PANEL_START, 0, PANEL0_EN | TWO_PANEL_EN);
		else
			dc_set_clear(hw, DC_DISPLAY_PANEL_START, 0, PANEL1_EN | TWO_PANEL_EN);
	}
}

static void setup_display_ex(struct dc_hw *hw, struct dc_hw_display *display)
{
	u8 id = display->id;
	u32 dp_cfg, offset = id << 2;
	bool is_yuv = false;

	if (hw->display[id].enable && hw->out[id] == OUT_DP) {
		switch (display->bus_format) {
		case MEDIA_BUS_FMT_RGB565_1X16:
			dp_cfg = 0;
			break;
		case MEDIA_BUS_FMT_RGB666_1X18:
			dp_cfg = 1;
			break;
		case MEDIA_BUS_FMT_RGB888_1X24:
			dp_cfg = 2;
			break;
		case MEDIA_BUS_FMT_RGB101010_1X30:
			dp_cfg = 3;
			break;
		case MEDIA_BUS_FMT_UYVY8_1X16:
			dp_cfg = 2 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_YUV8_1X24:
			dp_cfg = 4 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_UYVY10_1X20:
			dp_cfg = 8 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_YUV10_1X30:
			dp_cfg = 10 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
			dp_cfg = 12 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
			dp_cfg = 13 << 4;
			is_yuv = true;
			break;
		default:
			dp_cfg = 2;
			break;
		}
		if (is_yuv)
			dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + offset,
				     PANEL_RGB2YUV_EN, 0);
		else
			dc_set_clear(hw, DC_DISPLAY_PANEL_CONFIG + offset,
				     0, PANEL_RGB2YUV_EN);
		dc_write(hw, DC_DISPLAY_DP_CONFIG + offset, dp_cfg | DP_SELECT);
	}

	if (hw->out[id] == OUT_DPI)
		dc_set_clear(hw, DC_DISPLAY_DP_CONFIG + offset, 0, DP_SELECT);

	setup_display(hw, display);
}

static const struct dc_hw_funcs hw_func = {
	.gamma = &gamma_ex_commit,
	.plane = &plane_ex_commit,
	.display = setup_display_ex,
};

void dc_hw_commit(struct dc_hw *hw)
{
	u32 i, offset = 0;
	u8 plane_num = hw->info->plane_num;
	u8 layer_num = hw->info->layer_num;
	u8 cursor_num = plane_num - layer_num;

	hw->func->gamma(hw);
	hw->func->plane(hw);

	for (i = 0; i < cursor_num; i++) {
		if (hw->cursor[i].dirty) {
			offset = hw->cursor[i].display_id ? DC_CURSOR_OFFSET : 0;
			if (hw->cursor[i].enable) {
				dc_write(hw, DC_CURSOR_ADDRESS + offset,
					 hw->cursor[i].address);
				dc_write(hw, DC_CURSOR_LOCATION + offset,
					 X_LCOTION(hw->cursor[i].x) |
					 Y_LCOTION(hw->cursor[i].y));
				dc_set_clear(hw, DC_CURSOR_CONFIG + offset,
					     CURSOR_HOT_X(hw->cursor[i].hot_x) |
					     CURSOR_HOT_y(hw->cursor[i].hot_y) |
					     CURSOR_SIZE(hw->cursor[i].size) |
					     CURSOR_VALID(1) |
					     CURSOR_TRIG_FETCH(1) |
					     CURSOR_FORMAT(CURSOR_FORMAT_A8R8G8B8),
					     CURSOR_HOT_X_MASK |
					     CURSOR_HOT_y_MASK |
					     CURSOR_SIZE_MASK |
					     CURSOR_VALID_MASK |
					     CURSOR_TRIG_FETCH_MASK |
					     CURSOR_FORMAT_MASK);
			} else {
				dc_set_clear(hw, DC_CURSOR_CONFIG + offset,
					     CURSOR_VALID(1),
					     CURSOR_FORMAT_MASK);
			}
			hw->cursor[i].dirty = false;
		}
	}
}
