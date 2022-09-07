/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#ifndef __SF_FB_LCDC_H__
#define __SF_FB_LCDC_H__

#include "starfive_drm_crtc.h"

enum lcdc_in_mode {
	LCDC_IN_LCD_AXI = 0,
	LCDC_IN_VPP2,
	LCDC_IN_VPP1,
	LCDC_IN_VPP0,
	LCDC_IN_MAPCONVERT,
};

enum lcdc_win_num {
	LCDC_WIN_0 = 0,
	LCDC_WIN_1,
	LCDC_WIN_2,
	LCDC_WIN_3,
	LCDC_WIN_4,
	LCDC_WIN_5,
};

enum WIN_FMT {
	WIN_FMT_RGB565 = 4,
	WIN_FMT_xRGB1555,
	WIN_FMT_xRGB4444,
	WIN_FMT_xRGB8888,
};

#define LCDC_STOP	0
#define LCDC_RUN	1

//lcdc registers
#define LCDC_SWITCH		0x0000
#define LCDC_GCTRL		0x0004
#define LCDC_INT_STATUS		0x0008
#define LCDC_INT_MSK		0x000C
#define LCDC_INT_CLR		0x0010
#define LCDC_RGB_H_TMG		0x0014
#define LCDC_RGB_V_TMG		0x0018
#define LCDC_RGB_W_TMG		0x001C
#define LCDC_RGB_DCLK		0x0020
#define LCDC_M_CS_CTRL		0x0024
#define LCDC_DeltaRGB_CFG	0x0028
#define LCDC_BACKGROUND		0x002C
#define LCDC_WIN0_CFG_A		0x0030
#define LCDC_WIN0_CFG_B		0x0034
#define LCDC_WIN0_CFG_C		0x0038
#define LCDC_WIN1_CFG_A		0x003C
#define LCDC_WIN1_CFG_B		0x0040
#define LCDC_WIN1_CFG_C		0x0044
#define LCDC_WIN2_CFG_A		0x0048
#define LCDC_WIN2_CFG_B		0x004C
#define LCDC_WIN2_CFG_C		0x0050
#define LCDC_WIN3_CFG_A		0x0054
#define LCDC_WIN3_CFG_B		0x0058
#define LCDC_WIN3_CFG_C		0x005C
#define LCDC_WIN01_HSIZE	0x0090
#define LCDC_WIN23_HSIZE	0x0094
#define LCDC_WIN45_HSIZE	0x0098
#define LCDC_WIN67_HSIZE	0x009C
#define LCDC_ALPHA_VALUE	0x00A0
#define LCDC_PANELDATAFMT	0x00A4
#define LCDC_WIN0STARTADDR0	0x00B8
#define LCDC_WIN0STARTADDR1	0x00BC

/* Definition controller bit for LCDC registers */
//for LCDC_SWITCH
#define LCDC_DTRANS_SWITCH		0
#define LCDC_MPU_START			1
#define LCDC_EN_CFG_MODE		2
//for LCDC_GCTRL
#define LCDC_EN				0
#define LCDC_WORK_MODE			1
#define LCDC_A0_P			4
#define LCDC_ENABLE_P			5
#define LCDC_DOTCLK_P			6
#define LCDC_HSYNC_P			7
#define LCDC_VSYNC_P			8
#define LCDC_DITHER_EN			9
#define LCDC_R2Y_BPS			10
#define LCDC_MS_SEL			11
#define LCDC_TV_LCD_PATHSEL		12
#define LCDC_INTERLACE			13
#define LCDC_CBCR_ORDER			14
#define LCDC_INT_SEL			15
#define LCDC_INT_FREQ			24
//for LCDC_INT_MSK
#define LCDC_OUT_FRAME_END		5
//for RGB_H_TMG,RGB_V_TMG,RGB_W_TMG
#define LCDC_RGB_HBK			0
#define LCDC_RGB_HFP			16
#define LCDC_RGB_VBK			0
#define LCDC_RGB_VFP			16
#define LCDC_RGB_HPW			0
#define LCDC_RGB_VPW			8
#define LCDC_RGB_UNIT			16
//for BACKGROUND
#define LCDC_BG_HSIZE			0
#define LCDC_BG_VSIZE			12
//for WINx_CFG_A/B/C
#define LCDC_WIN_HSIZE			0
#define LCDC_WIN_VSIZE			12
#define LCDC_WIN_EN			24
#define LCDC_CC_EN			25
#define LCDC_CK_EN			26
#define LCDC_WIN_ISSEL			27
#define LCDC_WIN_PM			28
#define LCDC_WIN_CLK			30
#define LCDC_WIN_HPOS			0
#define LCDC_WIN_VPOS			12
#define LCDC_WIN_FMT			24
#define LCDC_WIN_ARGB_ORDER		27
#define LCDC_WIN_CC			0
//for WINxx_HSIZE
#define LCDC_IMG_HSIZE			12
//for LCDC_ALPHA_VALUE
#define LCDC_ALPHA1			0
#define LCDC_ALPHA2			4
#define LCDC_ALPHA3			8
#define LCDC_ALPHA4			12
#define LCDC_A_GLBL_ALPHA		16
#define LCDC_B_GLBL_ALPHA		17
#define LCDC_01_ALPHA_SEL		18
//for LCDC_PANELDATAFMT
#define LCDC_BUS_W			0
#define LCDC_TCYCLES			2
#define LCDC_COLOR_DEP			4
#define LCDC_PIXELS			7
#define LCDC_332RGB_SEL			8
#define LCDC_444RGB_SEL			9
#define LCDC_666RGB_SEL			12
#define LCDC_565RGB_SEL			16
#define LCDC_888RGB_SEL			18

//sysrst registers
#define SRST_ASSERT0		0x00
#define SRST_STATUS0		0x04
/* Definition controller bit for syd rst registers */
#define BIT_RST_DSI_DPI_PIX	17

void lcdc_enable_intr(struct starfive_crtc *sf_crtc);
void lcdc_disable_intr(struct starfive_crtc *sf_crtc);
irqreturn_t lcdc_isr_handler(int this_irq, void *dev_id);
void lcdc_int_cfg(struct starfive_crtc *sf_crtc, int mask);
void lcdc_config(struct starfive_crtc *sf_crtc,
		 struct drm_crtc_state *old_state,
		 int winNum);
int lcdc_win_sel(struct starfive_crtc *sf_crtc, enum lcdc_in_mode sel);
void lcdc_dsi_sel(struct starfive_crtc *sf_crtc);
void lcdc_run(struct starfive_crtc *sf_crtc,
	      uint32_t winMode, uint32_t lcdTrig);
void starfive_set_win_addr(struct starfive_crtc *sf_crtc, int addr);
int starfive_lcdc_enable(struct starfive_crtc *sf_crtc);

#endif
