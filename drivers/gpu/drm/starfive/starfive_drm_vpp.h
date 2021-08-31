/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#ifndef __SF_FB_VPP_H__
#define __SF_FB_VPP_H__

#include "starfive_drm_crtc.h"

#define PP_ID_0	0
#define PP_ID_1	1
#define PP_ID_2	2

#define PP_NUM	3

#define PP_STOP	0
#define PP_RUN	1

#define PP_INTR_ENABLE	1
#define PP_INTR_DISABLE	0
//PP coefficients
#define R2Y_COEF_R1	77
#define R2Y_COEF_G1	150
#define R2Y_COEF_B1	29
#define R2Y_OFFSET1	0
#define R2Y_COEF_R2	(0x400|43)
#define R2Y_COEF_G2	(0x400|85)
#define R2Y_COEF_B2	128
#define R2Y_OFFSET2	128
#define R2Y_COEF_R3	128
#define R2Y_COEF_G3	(0x400|107)
#define R2Y_COEF_B3	(0x400|21)
#define R2Y_OFFSET3	128

//sys registers
#define SYS_CONF_LCDC		0x00
#define SYS_CONF_PP		0x04
#define SYS_MAP_CONV		0x08

//vout clk registers
#define CLK_LCDC_OCLK_CTRL	0x14

enum PP_LCD_PATH {
	SYS_BUS_OUTPUT = 0,
	FIFO_OUTPUT = 1,
};

enum PP_COLOR_CONVERT_SCALE {
	NOT_BYPASS = 0,
	BYPASS,
};

enum PP_SRC_FORMAT {
	PP_SRC_YUV420P = 0,
	PP_SRC_YUV422,
	PP_SRC_YUV420I,
	PP_RESERVED,
	PP_SRC_GRB888,
	PP_SRC_RGB565,
};

enum PP_DST_FORMAT {
	PP_DST_YUV420P = 0,
	PP_DST_YUV422,
	PP_DST_YUV420I,
	PP_DST_RGBA888,
	PP_DST_ARGB888,
	PP_DST_RGB565,
	PP_DST_ABGR888,
	PP_DST_BGRA888,
};


struct pp_video_mode {
	enum COLOR_FORMAT format;
	unsigned int height;
	unsigned int width;
	unsigned int addr;
};

struct pp_mode {
	char pp_id;
	bool bus_out;	/*out to ddr*/
	bool fifo_out;	/*out to lcdc*/
	bool inited;
	struct pp_video_mode src;
	struct pp_video_mode dst;
};

//vpp registers
#define PP_SWITCH		0x0000
#define PP_CTRL1		0x0004
#define PP_CTRL2		0x0008
#define PP_SRC_SIZE		0x000C
#define PP_DROP_CTRL		0x0010
#define PP_DES_SIZE		0x0014
#define PP_Scale_Hratio		0x0018
#define PP_Scale_Vratio		0x001C
#define PP_Scale_limit		0x0020
#define PP_SRC_Y_SA_NXT		0x0024
#define PP_SRC_U_SA_NXT		0x0028
#define PP_SRC_V_SA_NXT		0x002c
#define PP_LOAD_NXT_PAR		0x0030
#define PP_SRC_Y_SA0		0x0034
#define PP_SRC_U_SA0		0x0038
#define PP_SRC_V_SA0		0x003c
#define PP_SRC_Y_OFS		0x0040
#define PP_SRC_U_OFS		0x0044
#define PP_SRC_V_OFS		0x0048
#define PP_SRC_Y_SA1		0x004C
#define PP_SRC_U_SA1		0x0050
#define PP_SRC_V_SA1		0x0054
#define PP_DES_Y_SA		0x0058
#define PP_DES_U_SA		0x005C
#define PP_DES_V_SA		0x0060
#define PP_DES_Y_OFS		0x0064
#define PP_DES_U_OFS		0x0068
#define PP_DES_V_OFS		0x006C
#define PP_INT_STATUS		0x0070
#define PP_INT_MASK		0x0074
#define PP_INT_CLR		0x0078
#define PP_R2Y_COEF1		0x007C
#define PP_R2Y_COEF2		0x0080

/* Definition controller bit for LCDC registers */
//for PP_SWITCH
#define PP_TRIG			0
//for PP_CTRL1
#define PP_LCDPATH_EN		0
#define PP_INTERLACE		1
#define PP_POINTER_MODE		2
#define PP_SRC_FORMAT_N		4
#define PP_420_ITLC		7
#define PP_DES_FORMAT		8
#define PP_R2Y_BPS		12
#define PP_MSCALE_BPS		13
#define PP_Y2R_BPS		14
#define PP_ARGB_ALPHA		16
#define PP_UV_IN_ADD_128	24
#define PP_UV_OUT_ADD_128	25
#define PP_SRC_422_YUV_POS	26
#define PP_SRC_420_YUV_POS	28
#define PP_SRC_ARGB_ORDER	29
//for PP_CTRL2
#define PP_LOCK_EN		0
#define PP_INT_INTERVAL		8
#define PP_DES_422_ORDER	16
#define PP_DES_420_ORDER	18
//for PP_SRC_SIZE
#define PP_SRC_HSIZE		0
#define PP_SRC_VSIZE		16
//for PP_DROP_CTRL
#define PP_DROP_HRATION		0
#define PP_DROP_VRATION		4
//for PP_DES_SIZE
#define PP_DES_HSIZE		0
#define PP_DES_VSIZE		16
//for PP_R2Y_COEF1
#define PP_COEF_R1		0
#define PP_COEF_G1		16
//for PP_R2Y_COEF2
#define PP_COEF_B1		0
#define PP_OFFSET_1		16

#define vout_rstgen_assert0_REG			0x0
#define vout_rstgen_status0_REG			0x4
#define clk_vout_apb_ctrl_REG			0x0
#define clk_mapconv_apb_ctrl_REG		0x4
#define clk_mapconv_axi_ctrl_REG		0x8
#define clk_disp0_axi_ctrl_REG			0xC
#define clk_disp1_axi_ctrl_REG			0x10
#define clk_lcdc_oclk_ctrl_REG			0x14
#define clk_lcdc_axi_ctrl_REG			0x18
#define clk_vpp0_axi_ctrl_REG			0x1C
#define clk_vpp1_axi_ctrl_REG			0x20
#define clk_vpp2_axi_ctrl_REG			0x24
#define clk_pixrawout_apb_ctrl_REG		0x28
#define clk_pixrawout_axi_ctrl_REG		0x2C
#define clk_csi2tx_strm0_pixclk_ctrl_REG	0x30
#define clk_csi2tx_strm0_apb_ctrl_REG		0x34
#define clk_dsi_sys_clk_ctrl_REG		0x38
#define clk_dsi_apb_ctrl_REG			0x3C
#define clk_ppi_tx_esc_clk_ctrl_REG		0x40

void mapconv_pp0_sel(struct starfive_crtc *sf_crtc, int sel);
void pp_srcAddr_next(struct starfive_crtc *sf_crtc, int ppNum, int ysa, int usa, int vsa);
void pp_srcOffset_cfg(struct starfive_crtc *sf_crtc, int ppNum, int yoff, int uoff, int voff);
void pp_nxtAddr_load(struct starfive_crtc *sf_crtc, int ppNum, int nxtPar, int nxtPos);
void pp_intcfg(struct starfive_crtc *sf_crtc, int ppNum, int intMask);
irqreturn_t vpp1_isr_handler(int this_irq, void *dev_id);
void pp1_enable_intr(struct starfive_crtc *sf_crtc);
void pp_enable_intr(struct starfive_crtc *sf_crtc, int ppNum);
void pp_disable_intr(struct starfive_crtc *sf_crtc, int ppNum);
void pp_run(struct starfive_crtc *sf_crtc, int ppNum, int start);
int starfive_pp_enable(struct starfive_crtc *sf_crtc);
int starfive_pp_get_2lcdc_id(struct starfive_crtc *sf_crtc);
int starfive_pp_update(struct starfive_crtc *sf_crtc);
void vout_disable(struct starfive_crtc *sf_crtc);
void vout_reset(struct starfive_crtc *sf_crtc);
void dsitx_vout_init(struct starfive_crtc *sf_crtc);

#endif
