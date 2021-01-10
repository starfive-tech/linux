/*
 * StarFive Vout driver
 *
 * Copyright 2020 StarFive Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef __SF_FRAMEBUFFER_H__
#define __SF_FRAMEBUFFER_H__

#include <linux/fb.h>
#include <linux/miscdevice.h>

#define FB_MEM_SIZE  0x1000000

#define  H_SIZE	1920//352//1920//1280
#define  V_SIZE	1080//288//1080//720
#define  H_SIZE_DST	H_SIZE
#define  V_SIZE_DST	V_SIZE

#define  H_WID	40
#define  H_BP		220
#define  H_FP		110

#define  V_WID	5
#define  V_BP		20
#define  V_FP		5

#define VD_1080P	1080
#define VD_720P	720
#define VD_PAL	480

#define VD_HEIGHT_1080P	VD_1080P
#define VD_WIDTH_1080P	1920

#define  RGB_OFFSET_ADDR	0//H_SIZE*(PIX_BPP+1)

#define MIN_XRES		64
#define MIN_YRES		64

#define STARFIVEFB_RGB_IF	(0x00000002)
#define STARFIVEFB_MIPI_IF	(0x00000003)
#define STARFIVEFB_HDMI_IF	(0x00000004)
#define STARFIVE_NAME		"starfive"
#define BUFFER_NUMS		2

//sys registers
#define SYS_CONF_LCDC		0x00
#define SYS_CONF_PP		0x04
#define SYS_MAP_CONV		0x08

//vout clk registers
#define CLK_LCDC_OCLK_CTRL	0x14

struct res_name {
	char name[10];
};

struct dis_panel_info {
	const char *name;

	// supported pixel format
	int w;
	int h;
	int bpp;
	int fps;

	/* dpi parameters */
	u32 dpi_pclk;
	// pixels
	int dpi_hsa;
	int dpi_hbp;
	int dpi_hfp;
	// lines
	int dpi_vsa;
	int dpi_vbp;
	int dpi_vfp;

	/* dsi parameters */
	int dphy_lanes;
	u32 dphy_bps;
	int dsi_burst_mode;
	int dsi_sync_pulse;
	// bytes
	int dsi_hsa;
	int dsi_hbp;
	int dsi_hfp;
	// lines
	int dsi_vsa;
	int dsi_vbp;
	int dsi_vfp;
};


struct sf_fb_data {
	struct device		*dev;
	char			*dis_dev_name;
	struct miscdevice	stfbcdev;
	int 			lcdc_irq;
	int 			vpp0_irq;
	int 			vpp1_irq;
	int 			vpp2_irq;
	void __iomem		*base_clk;
	void __iomem		*base_rst;
	void __iomem		*base_syscfg;
	void __iomem		*base_vpp0;
	void __iomem		*base_vpp1;
	void __iomem		*base_vpp2;
	void __iomem		*base_dsitx;
	void __iomem		*base_lcdc;
	struct notifier_block	vin;
	struct clk *mclk;

	/*
	 * Hardware control information
	 */
	struct fb_info		fb;
	struct fb_videomode display_info;    /* reparent video mode source*/
	struct dis_panel_info	panel_info;		/* mipi parameters for panel */
	unsigned int		refresh_en;
	unsigned int		pixclock;	/*lcdc_mclk*/
	unsigned int		buf_num;	/* frame buffer number. */
	unsigned int		buf_size;	/* frame buffer size. */
	int		cmap_inverse;
	int		cmap_static;
	/* keep these registers in case we need to re-write palette */
	u32			palette_buffer[256];
	u32			pseudo_pal[16];

	struct sf_fb_display_dev *display_dev;
	struct pp_mode *pp;
	int winNum;
	int pp_conn_lcdc;
	int ddr_format;
};

#endif
