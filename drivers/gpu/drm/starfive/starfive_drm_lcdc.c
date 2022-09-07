// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/units.h>
#include <drm/drm_crtc.h>
#include "starfive_drm_lcdc.h"
#include "starfive_drm_vpp.h"

static u32 sf_fb_clkread32(struct starfive_crtc *sf_crtc, u32 reg)
{
	return ioread32(sf_crtc->base_clk + reg);
}

static void sf_fb_clkwrite32(struct starfive_crtc *sf_crtc, u32 reg, u32 val)
{
	iowrite32(val, sf_crtc->base_clk + reg);
}

static u32 sf_fb_lcdcread32(struct starfive_crtc *sf_crtc, u32 reg)
{
	return ioread32(sf_crtc->base_lcdc + reg);
}

static void sf_fb_lcdcwrite32(struct starfive_crtc *sf_crtc, u32 reg, u32 val)
{
	iowrite32(val, sf_crtc->base_lcdc + reg);
}

static u32 starfive_lcdc_rstread32(struct starfive_crtc *sf_crtc, u32 reg)
{
	return ioread32(sf_crtc->base_rst + reg);
}

static void starfive_lcdc_rstwrite32(struct starfive_crtc *sf_crtc, u32 reg, u32 val)
{
	iowrite32(val, sf_crtc->base_rst + reg);
}

static void lcdc_mode_cfg(struct starfive_crtc *sf_crtc, uint32_t workMode, int dotEdge,
			  int syncEdge, int r2yBypass, int srcSel, int intSrc, int intFreq)
{
	u32 lcdcEn = 0x1;
	u32 cfg = lcdcEn |
		workMode << LCDC_WORK_MODE |
		dotEdge << LCDC_DOTCLK_P |
		syncEdge << LCDC_HSYNC_P |
		syncEdge << LCDC_VSYNC_P |
		0x0 << LCDC_DITHER_EN |
		r2yBypass << LCDC_R2Y_BPS |
		srcSel << LCDC_TV_LCD_PATHSEL |
		intSrc << LCDC_INT_SEL |
		intFreq << LCDC_INT_FREQ;

	sf_fb_lcdcwrite32(sf_crtc, LCDC_GCTRL, cfg);
	dev_dbg(sf_crtc->dev, "LCDC WorkMode: 0x%x, LCDC Path: %d\n", workMode, srcSel);
}

static void lcdc_timing_cfg(struct starfive_crtc *sf_crtc,
			    struct drm_crtc_state *state, int vunit)
{
	int hpw, hbk, hfp, vpw, vbk, vfp;
	u32 htiming, vtiming, hvwid;

	//h-sync
	int hsync_len = state->adjusted_mode.crtc_hsync_end -
		state->adjusted_mode.crtc_hsync_start;
	//h-bp
	int left_margin = state->adjusted_mode.crtc_htotal -
		state->adjusted_mode.crtc_hsync_end;
	//h-fp
	int right_margin = state->adjusted_mode.crtc_hsync_start -
		state->adjusted_mode.crtc_hdisplay;
	//v-sync
	int vsync_len = state->adjusted_mode.crtc_vsync_end -
		state->adjusted_mode.crtc_vsync_start;
	//v-bp
	int upper_margin = state->adjusted_mode.crtc_vtotal -
		state->adjusted_mode.crtc_vsync_end;
	//v-fp
	int lower_margin = state->adjusted_mode.crtc_vsync_start -
		state->adjusted_mode.crtc_vdisplay;

	hpw = hsync_len - 1;
	hbk = hsync_len + left_margin;
	hfp = right_margin;
	vpw = vsync_len - 1;
	vbk = vsync_len + upper_margin;
	vfp = lower_margin;

	dev_dbg(sf_crtc->dev, "%s: h-sync = %d, h-bp = %d, h-fp = %d", __func__,
		hsync_len, left_margin, right_margin);
	dev_dbg(sf_crtc->dev, "%s: v-sync = %d, v-bp = %d, v-fp = %d", __func__,
		vsync_len, upper_margin, lower_margin);

	htiming = hbk | hfp << LCDC_RGB_HFP;
	vtiming = vbk | vfp << LCDC_RGB_VFP;
	hvwid = hpw | vpw << LCDC_RGB_VPW | vunit << LCDC_RGB_UNIT;

	sf_fb_lcdcwrite32(sf_crtc, LCDC_RGB_H_TMG, htiming);
	sf_fb_lcdcwrite32(sf_crtc, LCDC_RGB_V_TMG, vtiming);
	sf_fb_lcdcwrite32(sf_crtc, LCDC_RGB_W_TMG, hvwid);
	dev_dbg(sf_crtc->dev, "LCDC HPW: %d, HBK: %d, HFP: %d\n", hpw, hbk, hfp);
	dev_dbg(sf_crtc->dev, "LCDC VPW: %d, VBK: %d, VFP: %d\n", vpw, vbk, vfp);
	dev_dbg(sf_crtc->dev, "LCDC V-Unit: %d, 0-HSYNC and 1-dotClk period\n", vunit);
}

//? background size
//lcdc_desize_cfg(sf_dev, sf_dev->display_info.xres-1, sf_dev->display_info.yres-1);
static void lcdc_desize_cfg(struct starfive_crtc *sf_crtc, struct drm_crtc_state *state)
{
	int hsize = state->adjusted_mode.crtc_hdisplay - 1;
	int vsize = state->adjusted_mode.crtc_vdisplay - 1;
	u32 sizecfg = hsize | vsize << LCDC_BG_VSIZE;

	sf_fb_lcdcwrite32(sf_crtc, LCDC_BACKGROUND, sizecfg);
	dev_dbg(sf_crtc->dev, "LCDC Dest H-Size: %d, V-Size: %d\n", hsize, vsize);
}

static void lcdc_rgb_dclk_cfg(struct starfive_crtc *sf_crtc, int dot_clk_sel)
{
	u32 cfg = dot_clk_sel << 16;

	sf_fb_lcdcwrite32(sf_crtc, LCDC_RGB_DCLK, cfg);
	dev_dbg(sf_crtc->dev, "LCDC Dot_clock_output_sel: 0x%x\n", cfg);
}

// color table
//win0, no lock transfer
//win3, no srcSel and addrMode, 0 assigned to them
//lcdc_win_cfgA(sf_dev, winNum, sf_dev->display_info.xres-1, sf_dev->display_info.yres-1,
//		0x1, 0x0, 0x0, 0x1, 0x0, 0x0);
static void lcdc_win_cfgA(struct starfive_crtc *sf_crtc, struct drm_crtc_state *state,
			  int winNum, int layEn, int clorTab,
			  int colorEn, int addrMode, int lock)
{
	int hsize = state->adjusted_mode.crtc_hdisplay - 1;
	int vsize = state->adjusted_mode.crtc_vdisplay - 1;
	int srcSel_v = 1;
	u32 cfg;

	if (sf_crtc->pp_conn_lcdc < 0)
		srcSel_v = 0;

	cfg = hsize | vsize << LCDC_WIN_VSIZE | layEn << LCDC_WIN_EN |
		clorTab << LCDC_CC_EN | colorEn << LCDC_CK_EN |
		srcSel_v << LCDC_WIN_ISSEL | addrMode << LCDC_WIN_PM |
		lock << LCDC_WIN_CLK;

	sf_fb_lcdcwrite32(sf_crtc, LCDC_WIN0_CFG_A + winNum * 0xC, cfg);
	dev_dbg(sf_crtc->dev,
		"LCDC Win%d H-Size: %d, V-Size: %d, layEn: %d, Src: %d, AddrMode: %d\n",
		winNum, hsize, vsize, layEn, srcSel_v, addrMode);
}

static void lcdc_win_cfgB(struct starfive_crtc *sf_crtc,
			  int winNum, int xpos, int ypos, int argbOrd)
{
	int win_format = sf_crtc->lcdcfmt;
	u32 cfg;

#ifdef CONFIG_DRM_STARFIVE_MIPI_DSI
	argbOrd = 0;
#else
	argbOrd = 1;
#endif

	cfg = xpos |
		ypos << LCDC_WIN_VPOS |
		win_format << LCDC_WIN_FMT |
		argbOrd << LCDC_WIN_ARGB_ORDER;

	sf_fb_lcdcwrite32(sf_crtc, LCDC_WIN0_CFG_B + winNum * 0xC, cfg);
	dev_dbg(sf_crtc->dev,
		"LCDC Win%d Xpos: %d, Ypos: %d, win_format: 0x%x, ARGB Order: 0x%x\n",
		winNum, xpos, ypos, win_format, argbOrd);
}

//? Color key
static void lcdc_win_cfgC(struct starfive_crtc *sf_crtc, int winNum, int colorKey)
{
	sf_fb_lcdcwrite32(sf_crtc, LCDC_WIN0_CFG_C + winNum * 0xC, colorKey);
	dev_dbg(sf_crtc->dev, "LCDC Win%d Color Key: 0x%6x\n", winNum, colorKey);
}

//? hsize
//lcdc_win_srcSize(sf_dev, winNum, sf_dev->display_info.xres-1);
static void lcdc_win_srcSize(struct starfive_crtc *sf_crtc,
			     struct drm_crtc_state *state, int winNum)
{
	int addr, off, winsize, preCfg, cfg;
	int hsize = state->adjusted_mode.crtc_hdisplay - 1;

	switch (winNum) {
	case 0:
		addr = LCDC_WIN01_HSIZE;
		off = 0xfffff000;
		winsize = hsize;
		break;
	case 1:
		addr = LCDC_WIN01_HSIZE;
		off = 0xff000fff;
		winsize = hsize << LCDC_IMG_HSIZE;
		break;
	case 2:
		addr = LCDC_WIN23_HSIZE;
		off = 0xfffff000;
		winsize = hsize;
		break;
	case 3:
		addr = LCDC_WIN23_HSIZE;
		off = 0xff000fff;
		winsize = hsize << LCDC_IMG_HSIZE;
		break;
	case 4:
		addr = LCDC_WIN45_HSIZE;
		off = 0xfffff000;
		winsize = hsize;
		break;
	case 5:
		addr = LCDC_WIN45_HSIZE;
		off = 0xff000fff;
		winsize = hsize << LCDC_IMG_HSIZE;
		break;
	case 6:
		addr = LCDC_WIN67_HSIZE;
		off = 0xfffff000;
		winsize = hsize;
		break;
	case 7:
		addr = LCDC_WIN67_HSIZE;
		off = 0xff000fff;
		winsize = hsize << LCDC_IMG_HSIZE;
		break;
	default:
		addr = LCDC_WIN01_HSIZE;
		off = 0xfffff000;
		winsize = hsize;
		break;
	}
	preCfg = sf_fb_lcdcread32(sf_crtc, addr) & off;
	cfg = winsize | preCfg;
	sf_fb_lcdcwrite32(sf_crtc, addr, cfg);
	dev_dbg(sf_crtc->dev, "LCDC Win%d Src Hsize: %d\n", winNum, hsize);
}

static void lcdc_alphaVal_cfg(struct starfive_crtc *sf_crtc,
			      int val1, int val2, int val3, int val4, int sel)
{
	u32 val = val1 |
		val2 << LCDC_ALPHA2 |
		val3 << LCDC_ALPHA3 |
		val4 << LCDC_ALPHA4 |
		sel << LCDC_01_ALPHA_SEL;
	u32 preVal = sf_fb_lcdcread32(sf_crtc, LCDC_ALPHA_VALUE) & 0xfffb0000U;

	sf_fb_lcdcwrite32(sf_crtc, LCDC_ALPHA_VALUE, preVal | val);
	dev_dbg(sf_crtc->dev, "LCDC Alpha 1: %x, 2: %x, 3: %x, 4: %x\n", val1, val2, val3, val4);
}

static void lcdc_panel_cfg(struct starfive_crtc *sf_crtc,
			   int buswid, int depth, int txcycle, int pixpcycle,
			   int rgb565sel, int rgb888sel)
{
	u32 cfg = buswid |
		depth << LCDC_COLOR_DEP |
		txcycle << LCDC_TCYCLES |
		pixpcycle << LCDC_PIXELS |
		rgb565sel << LCDC_565RGB_SEL |
		rgb888sel << LCDC_888RGB_SEL;

	sf_fb_lcdcwrite32(sf_crtc, LCDC_PANELDATAFMT, cfg);
	dev_dbg(sf_crtc->dev, "LCDC bus bit: :%d, pixDep: 0x%x, txCyle: %d, %dpix/cycle, RGB565 2cycle_%d, RGB888 3cycle_%d\n",
		buswid, depth, txcycle, pixpcycle, rgb565sel, rgb888sel);
}

//winNum: 0-2
static void lcdc_win02Addr_cfg(struct starfive_crtc *sf_crtc, int addr0, int addr1)
{
	sf_fb_lcdcwrite32(sf_crtc, LCDC_WIN0STARTADDR0 + sf_crtc->winNum * 0x8, addr0);
	sf_fb_lcdcwrite32(sf_crtc, LCDC_WIN0STARTADDR1 + sf_crtc->winNum * 0x8, addr1);
	dev_dbg(sf_crtc->dev, "LCDC Win%d Start Addr0: 0x%8x, Addr1: 0x%8x\n",
		sf_crtc->winNum, addr0, addr1);
}

void starfive_set_win_addr(struct starfive_crtc *sf_crtc, int addr)
{
	lcdc_win02Addr_cfg(sf_crtc, addr, 0x0);
}

void lcdc_enable_intr(struct starfive_crtc *sf_crtc)
{
	u32 cfg = ~(1U << LCDC_OUT_FRAME_END);

	sf_fb_lcdcwrite32(sf_crtc, LCDC_INT_MSK, cfg);
}

void lcdc_disable_intr(struct starfive_crtc *sf_crtc)
{
	sf_fb_lcdcwrite32(sf_crtc, LCDC_INT_MSK, 0xff);
	sf_fb_lcdcwrite32(sf_crtc, LCDC_INT_CLR, 0xff);
}

int lcdc_win_sel(struct starfive_crtc *sf_crtc, enum lcdc_in_mode sel)
{
	int winNum;

	switch (sel) {
	case LCDC_IN_LCD_AXI:
		winNum = LCDC_WIN_0;
		break;
	case LCDC_IN_VPP2:
		winNum = LCDC_WIN_0;
		break;
	case LCDC_IN_VPP1:
		winNum = LCDC_WIN_2;
		break;
	case LCDC_IN_VPP0:
		winNum = LCDC_WIN_1;
		//mapconv_pp0_sel(sf_dev, 0x0);
		break;
	case LCDC_IN_MAPCONVERT:
		winNum = LCDC_WIN_1;
		//mapconv_pp0_sel(sf_dev, 0x1);
		break;
	default:
		winNum = 2;
	}

	return winNum;
}

void lcdc_dsi_sel(struct starfive_crtc *sf_crtc)
{
	int temp;
	u32 lcdcEn = 0x1;
	u32 workMode = 0x1;
	u32 cfg = lcdcEn | workMode << LCDC_WORK_MODE;

	sf_fb_lcdcwrite32(sf_crtc, LCDC_GCTRL, cfg);
	temp = starfive_lcdc_rstread32(sf_crtc, SRST_ASSERT0);
	temp &= ~(0x1<<BIT_RST_DSI_DPI_PIX);
	starfive_lcdc_rstwrite32(sf_crtc, SRST_ASSERT0, temp);
}

irqreturn_t lcdc_isr_handler(int this_irq, void *dev_id)
{
	struct starfive_crtc *sf_crtc = dev_id;
	//u32 intr_status = sf_fb_lcdcread32(sf_crtc, LCDC_INT_STATUS);

	sf_fb_lcdcwrite32(sf_crtc, LCDC_INT_CLR, 0xffffffff);

	return IRQ_HANDLED;
}

void lcdc_int_cfg(struct starfive_crtc *sf_crtc, int mask)
{
	u32 cfg;

	if (mask == 0x1)
		cfg = 0xffffffff;
	else
		cfg = ~(1U << LCDC_OUT_FRAME_END); //only frame end interrupt mask

	sf_fb_lcdcwrite32(sf_crtc, LCDC_INT_MSK, cfg);
}

void lcdc_config(struct starfive_crtc *sf_crtc, struct drm_crtc_state *state, int winNum)
{
	lcdc_mode_cfg(sf_crtc, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0);
	lcdc_timing_cfg(sf_crtc, state, 0);
	lcdc_desize_cfg(sf_crtc, state);
	lcdc_rgb_dclk_cfg(sf_crtc, 0x1);

	if (sf_crtc->pp_conn_lcdc < 0) //ddr->lcdc
		lcdc_win02Addr_cfg(sf_crtc, sf_crtc->dma_addr, 0x0);

	lcdc_win_cfgA(sf_crtc, state, winNum, 0x1, 0x0, 0x0, 0x0, 0x0);
	lcdc_win_cfgB(sf_crtc, winNum, 0x0, 0x0, 0x0);
	lcdc_win_cfgC(sf_crtc, winNum, 0xffffff);

	lcdc_win_srcSize(sf_crtc, state, winNum);
	lcdc_alphaVal_cfg(sf_crtc, 0xf, 0xf, 0xf, 0xf, 0x0);
	lcdc_panel_cfg(sf_crtc, 0x3, 0x4, 0x0, 0x0, 0x0, 0x1);  //rgb888sel?
}

void lcdc_run(struct starfive_crtc *sf_crtc, uint32_t winMode, uint32_t lcdTrig)
{
	u32 runcfg = winMode << LCDC_EN_CFG_MODE | lcdTrig;

	sf_fb_lcdcwrite32(sf_crtc, LCDC_SWITCH, runcfg);
	dev_dbg(sf_crtc->dev, "Start run LCDC\n");
}

static int sf_fb_lcdc_clk_cfg(struct starfive_crtc *sf_crtc, struct drm_crtc_state *state)
{
	u32 reg_val = clk_get_rate(sf_crtc->clk_vout_src) / (state->mode.clock * HZ_PER_KHZ);
	u32 tmp_val;

	dev_dbg(sf_crtc->dev, "%s: reg_val = %u\n", __func__, reg_val);

	switch (state->adjusted_mode.crtc_hdisplay) {
	case 640:
		tmp_val = sf_fb_clkread32(sf_crtc, CLK_LCDC_OCLK_CTRL);
		tmp_val &= ~(0x3F);
		tmp_val |= (59 & 0x3F);
		sf_fb_clkwrite32(sf_crtc, CLK_LCDC_OCLK_CTRL, tmp_val);
		break;
	case 840:
		tmp_val = sf_fb_clkread32(sf_crtc, CLK_LCDC_OCLK_CTRL);
		tmp_val &= ~(0x3F);
		tmp_val |= (54 & 0x3F);
		sf_fb_clkwrite32(sf_crtc, CLK_LCDC_OCLK_CTRL, tmp_val);
		break;
	case 1024:
		tmp_val = sf_fb_clkread32(sf_crtc, CLK_LCDC_OCLK_CTRL);
		tmp_val &= ~(0x3F);
		tmp_val |= (30 & 0x3F);
		sf_fb_clkwrite32(sf_crtc, CLK_LCDC_OCLK_CTRL, tmp_val);
		break;
	case 1280:
		tmp_val = sf_fb_clkread32(sf_crtc, CLK_LCDC_OCLK_CTRL);
		tmp_val &= ~(0x3F);
		tmp_val |= (30 & 0x3F);
		sf_fb_clkwrite32(sf_crtc, CLK_LCDC_OCLK_CTRL, tmp_val);
		break;
	case 1440:
		tmp_val = sf_fb_clkread32(sf_crtc, CLK_LCDC_OCLK_CTRL);
		tmp_val &= ~(0x3F);
		tmp_val |= (30 & 0x3F);
		sf_fb_clkwrite32(sf_crtc, CLK_LCDC_OCLK_CTRL, tmp_val);
		break;
	case 1680:
		tmp_val = sf_fb_clkread32(sf_crtc, CLK_LCDC_OCLK_CTRL);
		tmp_val &= ~(0x3F);
		tmp_val |= (24 & 0x3F);	//24 30MHZ
		sf_fb_clkwrite32(sf_crtc, CLK_LCDC_OCLK_CTRL, tmp_val);
		break;
	case 1920:
		tmp_val = sf_fb_clkread32(sf_crtc, CLK_LCDC_OCLK_CTRL);
		tmp_val &= ~(0x3F);
		tmp_val |= (10 & 0x3F); //20 30MHz , 15  40Mhz, 10 60Mhz
		sf_fb_clkwrite32(sf_crtc, CLK_LCDC_OCLK_CTRL, tmp_val);
		break;
	case 2048:
		tmp_val = sf_fb_clkread32(sf_crtc, CLK_LCDC_OCLK_CTRL);
		tmp_val &= ~(0x3F);
		tmp_val |= (10 & 0x3F);
		sf_fb_clkwrite32(sf_crtc, CLK_LCDC_OCLK_CTRL, tmp_val);
		break;
	default:
		tmp_val = sf_fb_clkread32(sf_crtc, CLK_LCDC_OCLK_CTRL);
		tmp_val &= ~(0x3F);
		tmp_val |= (reg_val & 0x3F);
		sf_fb_clkwrite32(sf_crtc, CLK_LCDC_OCLK_CTRL, tmp_val);
	}

	return 0;
}

static int sf_fb_lcdc_init(struct starfive_crtc *sf_crtc, struct drm_crtc_state *state)
{
	int pp_id;
	int lcd_in_pp;
	int winNum;

	pp_id = sf_crtc->pp_conn_lcdc;
	if (pp_id < 0) {
		dev_dbg(sf_crtc->dev, "DDR to LCDC\n");
		lcd_in_pp = LCDC_IN_LCD_AXI;
		winNum = lcdc_win_sel(sf_crtc, lcd_in_pp);
		sf_crtc->winNum = winNum;
		lcdc_config(sf_crtc, state, winNum);
	} else {
		dev_dbg(sf_crtc->dev, "DDR to VPP to LCDC\n");
		lcd_in_pp = (pp_id == 0) ? LCDC_IN_VPP0 :
			((pp_id == 1) ? LCDC_IN_VPP1 : LCDC_IN_VPP2);
		winNum = lcdc_win_sel(sf_crtc, lcd_in_pp);
		sf_crtc->winNum = winNum;
		lcdc_config(sf_crtc, state, winNum);
	}

	return 0;
}

int starfive_lcdc_enable(struct starfive_crtc *sf_crtc)
{
	struct drm_crtc_state *state = sf_crtc->crtc.state;

	lcdc_disable_intr(sf_crtc);

	if (sf_fb_lcdc_clk_cfg(sf_crtc, state)) {
		dev_err(sf_crtc->dev, "lcdc clock configure fail\n");
		return -EINVAL;
	}

	if (sf_fb_lcdc_init(sf_crtc, state)) {
		dev_err(sf_crtc->dev, "lcdc init fail\n");
		return -EINVAL;
	}

	lcdc_run(sf_crtc, sf_crtc->winNum, LCDC_RUN);
	lcdc_enable_intr(sf_crtc);

	return 0;
}

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("loadable LCDC driver for StarFive");
MODULE_LICENSE("GPL");
