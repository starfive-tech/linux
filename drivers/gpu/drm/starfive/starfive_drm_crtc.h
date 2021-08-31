/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#ifndef _STARFIVE_DRM_CRTC_H
#define _STARFIVE_DRM_CRTC_H
#include <drm/drm_crtc.h>

enum COLOR_FORMAT {
	COLOR_YUV422_UYVY = 0,  //00={Y1,V0,Y0,U0}
	COLOR_YUV422_VYUY = 1,  //01={Y1,U0,Y0,V0}
	COLOR_YUV422_YUYV = 2,  //10={V0,Y1,U0,Y0}
	COLOR_YUV422_YVYU = 3,  //11={U0,Y1,V0,Y0}

	COLOR_YUV420P,
	COLOR_YUV420_NV21,
	COLOR_YUV420_NV12,

	COLOR_RGB888_ARGB,
	COLOR_RGB888_ABGR,
	COLOR_RGB888_RGBA,
	COLOR_RGB888_BGRA,
	COLOR_RGB565,
};

struct starfive_crtc_state {
	struct drm_crtc_state base;
};

#define to_starfive_crtc_state(s) \
		container_of(s, struct starfive_crtc_state, base)

struct starfive_crtc {
	struct drm_crtc		crtc;
	struct device		*dev;
	struct drm_device	*drm_dev;
	bool is_enabled;

	void __iomem	*base_clk;	// 0x12240000
	void __iomem	*base_rst;	// 0x12250000
	void __iomem	*base_syscfg;	// 0x12260000
	void __iomem	*base_vpp0;	// 0x12040000
	void __iomem	*base_vpp1;	// 0x12080000
	void __iomem	*base_vpp2;	// 0x120c0000
	void __iomem	*base_lcdc;	// 0x12000000

	struct clk *clk_disp_axi;
	struct clk *clk_vout_src;

	struct reset_control *rst_disp_axi;
	struct reset_control *rst_vout_src;

	int		lcdc_irq;
	int		vpp0_irq;
	int		vpp1_irq;
	int		vpp2_irq;

	struct pp_mode	*pp;

	int		winNum;
	int		pp_conn_lcdc;
	unsigned int	ddr_format;
	bool		ddr_format_change;
	enum		COLOR_FORMAT vpp_format;
	int		lcdcfmt;

	/* one time only one process allowed to config the register */
	spinlock_t	reg_lock;

	struct drm_plane	*planes;

	u8		lut_r[256];
	u8		lut_g[256];
	u8		lut_b[256];

	bool		gamma_lut;
	dma_addr_t	dma_addr;
	bool		dma_addr_change;
	size_t		size;
};

#define to_starfive_crtc(x) container_of(x, struct starfive_crtc, crtc)

void starfive_crtc_hw_config_simple(struct starfive_crtc *starfive_crtc);

#endif /* _STARFIVE_DRM_CRTC_H */
