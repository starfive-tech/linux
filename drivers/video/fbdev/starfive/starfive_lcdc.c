/* driver/video/starfive/starfive_lcdc.c
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License version 2 as
** published by the Free Software Foundation.
**
** Copyright (C) 2020 StarFive, Inc.
**
** PURPOSE:	This files contains the driver of LCD controller.
**
** CHANGE HISTORY:
**	Version		Date		Author		Description
**	0.1.0		2020-11-03	starfive		created
**
*/

#include <linux/module.h>

#include "starfive_fb.h"
#include "starfive_lcdc.h"
#include "starfive_vpp.h"
#include "starfive_comm_regs.h"

//#define SF_LCDC_DEBUG	1
#ifdef SF_LCDC_DEBUG
	#define LCDC_PRT(format, args...)  printk(KERN_DEBUG "[LCDC]: " format, ## args)
	#define LCDC_INFO(format, args...) printk(KERN_INFO "[LCDC]: " format, ## args)
	#define LCDC_ERR(format, args...)	printk(KERN_ERR "[LCDC]: " format, ## args)
#else
	#define LCDC_PRT(x...)  do{} while(0)
	#define LCDC_INFO(x...)  do{} while(0)
	#define LCDC_ERR(x...)  do{} while(0)
#endif

static u32 sf_fb_lcdcread32(struct sf_fb_data *sf_dev, u32 reg)
{
	return ioread32(sf_dev->base_lcdc + reg);
}

static void sf_fb_lcdcwrite32(struct sf_fb_data *sf_dev, u32 reg, u32 val)
{
	iowrite32(val, sf_dev->base_lcdc + reg);
}

void lcdc_mode_cfg(struct sf_fb_data *sf_dev, uint32_t workMode, int dotEdge, int syncEdge, int r2yBypass,
					int srcSel, int intSrc, int intFreq)
{
  u32 lcdcEn = 0x1;
  u32 cfg = lcdcEn | workMode << LCDC_WORK_MODE
				| dotEdge << LCDC_DOTCLK_P
				| syncEdge << LCDC_HSYNC_P
				| syncEdge << LCDC_VSYNC_P
				| 0x0 << LCDC_DITHER_EN
				| r2yBypass << LCDC_R2Y_BPS
				| srcSel << LCDC_TV_LCD_PATHSEL
				| intSrc << LCDC_INT_SEL
				| intFreq << LCDC_INT_FREQ;

  sf_fb_lcdcwrite32(sf_dev, LCDC_GCTRL, cfg);
  LCDC_PRT("LCDC WorkMode: 0x%x, LCDC Path: %d\n", workMode, srcSel);
}

//hbk, vbk=sa+bp, hpw?
void lcdc_timing_cfg(struct sf_fb_data *sf_dev, int vunit)
{
  int hpw = sf_dev->display_info.hsync_len - 1;
  int hbk = sf_dev->display_info.hsync_len + sf_dev->display_info.left_margin;
  int hfp = sf_dev->display_info.right_margin;
  int vpw = sf_dev->display_info.vsync_len - 1;
  int vbk = sf_dev->display_info.vsync_len + sf_dev->display_info.upper_margin;
  int vfp = sf_dev->display_info.lower_margin;

  int htiming = hbk | hfp << LCDC_RGB_HFP;
  int vtiming = vbk | vfp << LCDC_RGB_VFP;
  int hvwid = hpw | vpw << LCDC_RGB_VPW | vunit << LCDC_RGB_UNIT;

  sf_fb_lcdcwrite32(sf_dev, LCDC_RGB_H_TMG, htiming);
  sf_fb_lcdcwrite32(sf_dev, LCDC_RGB_V_TMG, vtiming);
  sf_fb_lcdcwrite32(sf_dev, LCDC_RGB_W_TMG, hvwid);
  LCDC_PRT("LCDC HPW: %d, HBK: %d, HFP: %d\n", hpw, hbk, hfp);
  LCDC_PRT("LCDC VPW: %d, VBK: %d, VFP: %d\n", vpw, vbk, vfp);
  LCDC_PRT("LCDC V-Unit: %d, 0-HSYNC and 1-dotClk period\n", vunit);
}

//? background size
//lcdc_desize_cfg(sf_dev, sf_dev->display_info.xres-1, sf_dev->display_info.yres-1);
void lcdc_desize_cfg(struct sf_fb_data *sf_dev)
{
  int hsize = sf_dev->display_info.xres - 1;
  int vsize = sf_dev->display_info.yres - 1;

  int sizecfg = hsize | vsize << LCDC_BG_VSIZE;
  sf_fb_lcdcwrite32(sf_dev, LCDC_BACKGROUD, sizecfg);
  LCDC_PRT("LCDC Dest H-Size: %d, V-Size: %d\n", hsize, vsize);
}

void lcdc_rgb_dclk_cfg(struct sf_fb_data *sf_dev, int dot_clk_sel)
{
  int cfg = dot_clk_sel << 16;

  sf_fb_lcdcwrite32(sf_dev, LCDC_RGB_DCLK, cfg);

  LCDC_PRT("LCDC Dot_clock_output_sel: 0x%x\n", cfg);
}


// color table
//win0, no lock transfer
//win3, no srcSel and addrMode, 0 assigned to them
//lcdc_win_cfgA(sf_dev, winNum, sf_dev->display_info.xres-1, sf_dev->display_info.yres-1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0);
void lcdc_win_cfgA(struct sf_fb_data *sf_dev, int winNum, int layEn, int clorTab,
			int colorEn, int addrMode, int lock)
{
   int cfg;
   int hsize = sf_dev->display_info.xres - 1;
   int vsize = sf_dev->display_info.yres - 1;
   int srcSel_v = 1;

   if(sf_dev->pp_conn_lcdc < 0)
		srcSel_v = 0;

   cfg = hsize | vsize << LCDC_WIN_VSIZE | layEn << LCDC_WIN_EN |
	 clorTab << LCDC_CC_EN | colorEn << LCDC_CK_EN |
	 srcSel_v << LCDC_WIN_ISSEL | addrMode << LCDC_WIN_PM |
	 lock << LCDC_WIN_CLK;

   sf_fb_lcdcwrite32(sf_dev, LCDC_WIN0_CFG_A + winNum * 0xC, cfg);
   LCDC_PRT("LCDC Win%d H-Size: %d, V-Size: %d, layEn: %d, Src: %d, AddrMode: %d\n",
		winNum, hsize, vsize, layEn, srcSel, addrMode);
}

static int ppfmt_to_lcdcfmt(enum COLOR_FORMAT ppfmt)
{
	int lcdcfmt = 0;

	if(COLOR_RGB888_ARGB == ppfmt) {
		lcdcfmt = WIN_FMT_xRGB8888;
	} else if (COLOR_RGB888_ABGR == ppfmt) {
		LCDC_PRT("COLOR_RGB888_ABGR(%d) not map\n", ppfmt);
	} else if (COLOR_RGB888_RGBA == ppfmt) {
		LCDC_PRT("COLOR_RGB888_RGBA(%d) not map\n", ppfmt);
	} else if (COLOR_RGB888_BGRA == ppfmt) {
		LCDC_PRT("COLOR_RGB888_BGRA(%d) not map\n", ppfmt);
	} else if (COLOR_RGB565 == ppfmt) {
		lcdcfmt = WIN_FMT_RGB565;
	}

	return lcdcfmt;
}

void lcdc_win_cfgB(struct sf_fb_data *sf_dev, int winNum, int xpos, int ypos, int argbOrd)
{
	int win_format = 0;
	int cfg = xpos | ypos << LCDC_WIN_VPOS;

	if(sf_dev->pp_conn_lcdc < 0) { //ddr -> lcdc
		win_format = sf_dev->ddr_format;
		LCDC_PRT("LCDC win_format: 0x%x\n",win_format);
	} else { //ddr -> pp -> lcdc
		win_format = ppfmt_to_lcdcfmt(sf_dev->pp[sf_dev->pp_conn_lcdc].dst.format);
    }

	if (!strcmp(sf_dev->dis_dev_name, "tda_998x_1080p"))
		argbOrd=0;
	if (!strcmp(sf_dev->dis_dev_name, "seeed_5_inch"))
		argbOrd=1;

	cfg |= win_format << LCDC_WIN_FMT | argbOrd << LCDC_WIN_ARGB_ORDER;

	sf_fb_lcdcwrite32(sf_dev, LCDC_WIN0_CFG_B + winNum * 0xC, cfg);
	LCDC_PRT("LCDC Win%d Xpos: %d, Ypos: %d, win_format: 0x%x, ARGB Order: 0x%x\n",
		 winNum, xpos, ypos, win_format, argbOrd);
}

//? Color key
void lcdc_win_cfgC(struct sf_fb_data *sf_dev, int winNum, int colorKey)
{
  sf_fb_lcdcwrite32(sf_dev, LCDC_WIN0_CFG_C + winNum * 0xC, colorKey);
  LCDC_PRT("LCDC Win%d Color Key: 0x%6x\n", winNum, colorKey);
}

//? hsize
//lcdc_win_srcSize(sf_dev, winNum, sf_dev->display_info.xres-1);
void lcdc_win_srcSize(struct sf_fb_data *sf_dev, int winNum)
{
	int addr, off, winsize, preCfg, cfg;
	int hsize = sf_dev->display_info.xres - 1;
	switch(winNum) {
	case 0 : {addr = LCDC_WIN01_HSIZE; off = 0xfffff000; winsize = hsize; break;}
	case 1 : {addr = LCDC_WIN01_HSIZE; off = 0xff000fff; winsize = hsize << LCDC_IMG_HSIZE; break;}
	case 2 : {addr = LCDC_WIN23_HSIZE; off = 0xfffff000; winsize = hsize; break;}
	case 3 : {addr = LCDC_WIN23_HSIZE; off = 0xff000fff; winsize = hsize << LCDC_IMG_HSIZE; break;}
	case 4 : {addr = LCDC_WIN45_HSIZE; off = 0xfffff000; winsize = hsize; break;}
	case 5 : {addr = LCDC_WIN45_HSIZE; off = 0xff000fff; winsize = hsize << LCDC_IMG_HSIZE; break;}
	case 6 : {addr = LCDC_WIN67_HSIZE; off = 0xfffff000; winsize = hsize; break;}
	case 7 : {addr = LCDC_WIN67_HSIZE; off = 0xff000fff; winsize = hsize << LCDC_IMG_HSIZE; break;}
	default: {addr = LCDC_WIN01_HSIZE; off = 0xfffff000; winsize = hsize; break;}
	}
	preCfg = sf_fb_lcdcread32(sf_dev, addr)  & off;
	cfg = winsize | preCfg;
	sf_fb_lcdcwrite32(sf_dev, addr, cfg);
	LCDC_PRT("LCDC Win%d Src Hsize: %d\n", winNum, hsize);
}

void lcdc_alphaVal_cfg(struct sf_fb_data *sf_dev, int val1, int val2, int val3, int val4, int sel)
{
	int val = val1 | val2 << LCDC_ALPHA2
			| val3 << LCDC_ALPHA3
			| val4 << LCDC_ALPHA4
			| sel << LCDC_01_ALPHA_SEL;

	int preVal = 0xfffb0000 & sf_fb_lcdcread32(sf_dev, LCDC_ALPHA_VALUE);
	sf_fb_lcdcwrite32(sf_dev, LCDC_ALPHA_VALUE, preVal | val);
	LCDC_PRT("LCDC Alpha 1: %x, 2: %x, 3: %x, 4: %x\n", val1, val2, val3, val4);
}

void lcdc_panel_cfg(struct sf_fb_data *sf_dev, int buswid, int depth, int txcycle, int pixpcycle,
		int rgb565sel, int rgb888sel)
{
	int cfg = buswid | depth << LCDC_COLOR_DEP
			  | txcycle << LCDC_TCYCLES
			  | pixpcycle << LCDC_PIXELS
			  | rgb565sel << LCDC_565RGB_SEL
			  | rgb888sel << LCDC_888RGB_SEL;

  sf_fb_lcdcwrite32(sf_dev, LCDC_PANELDATAFMT, cfg);
  LCDC_PRT("LCDC bus bit: :%d, pixDep: 0x%x, txCyle: %d, %dpix/cycle, RGB565 2cycle_%d, RGB888 3cycle_%d\n",
		buswid, depth, txcycle, pixpcycle, rgb565sel, rgb888sel);
}

//winNum: 0-2
void lcdc_win02Addr_cfg(struct sf_fb_data *sf_dev, int addr0, int addr1)
{
   sf_fb_lcdcwrite32(sf_dev, LCDC_WIN0STARTADDR0 + sf_dev->winNum * 0x8, addr0);
   sf_fb_lcdcwrite32(sf_dev, LCDC_WIN0STARTADDR1 + sf_dev->winNum * 0x8, addr1);
   LCDC_PRT("LCDC Win%d Start Addr0: 0x%8x, Addr1: 0x%8x\n", sf_dev->winNum, addr0, addr1);
}

void lcdc_enable_intr(struct sf_fb_data *sf_dev)
{
	int cfg;
	cfg = ~(0x1 << LCDC_OUT_FRAME_END);

	sf_fb_lcdcwrite32(sf_dev, LCDC_INT_MSK, cfg);
}
EXPORT_SYMBOL(lcdc_enable_intr);

void lcdc_disable_intr(struct sf_fb_data *sf_dev)
{
	sf_fb_lcdcwrite32(sf_dev, LCDC_INT_MSK, 0xff);
	sf_fb_lcdcwrite32(sf_dev, LCDC_INT_CLR, 0xff);
}
EXPORT_SYMBOL(lcdc_disable_intr);

int lcdc_win_sel(struct sf_fb_data *sf_dev, enum lcdc_in_mode sel)
{
	int winNum = 2;

	switch(sel)
	{
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
		mapconv_pp0_sel(sf_dev, 0x0);
		break;
	case LCDC_IN_MAPCONVERT:
		winNum = LCDC_WIN_1;
		mapconv_pp0_sel(sf_dev, 0x1);
		break;
	}

	return winNum;
}
EXPORT_SYMBOL(lcdc_win_sel);

void lcdc_dsi_sel(struct sf_fb_data *sf_dev)
{
  int temp;
  u32 lcdcEn = 0x1;
  u32 workMode = 0x1;
  u32 cfg = lcdcEn | workMode << LCDC_WORK_MODE;

  sf_fb_lcdcwrite32(sf_dev, LCDC_GCTRL, cfg);

  temp = sf_fb_rstread32(sf_dev, SRST_ASSERT0);
  temp &= ~(0x1<<BIT_RST_DSI_DPI_PIX);
  sf_fb_rstwrite32(sf_dev, SRST_ASSERT0, temp);
}
EXPORT_SYMBOL(lcdc_dsi_sel);

irqreturn_t lcdc_isr_handler(int this_irq, void *dev_id)
{
	struct sf_fb_data *sf_dev = (struct sf_fb_data *)dev_id;
	static int count = 0;
	u32 intr_status = 0;

	intr_status = sf_fb_lcdcread32(sf_dev, LCDC_INT_STATUS);
	sf_fb_lcdcwrite32(sf_dev, LCDC_INT_CLR, 0xffffffff);

	count ++;
	//if(0 == count % 100)
	//LCDC_PRT("++++\n");
		//printk("+ count = %d, intr_status = 0x%x\n", count, intr_status);
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(lcdc_isr_handler);

void lcdc_int_cfg(struct sf_fb_data *sf_dev, int mask)
{
	int cfg;

	if(mask==0x1)
		cfg = 0xffffffff;
	else
		cfg = ~(0x1 << LCDC_OUT_FRAME_END); //only frame end interrupt mask
	sf_fb_lcdcwrite32(sf_dev, LCDC_INT_MSK, cfg);
}
EXPORT_SYMBOL(lcdc_int_cfg);

void lcdc_config(struct sf_fb_data *sf_dev, int winNum)
{
	lcdc_mode_cfg(sf_dev, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0);
	lcdc_timing_cfg(sf_dev, 0);
	lcdc_desize_cfg(sf_dev);
	lcdc_rgb_dclk_cfg(sf_dev, 0x1);

	if(sf_dev->pp_conn_lcdc < 0) { //ddr->lcdc
		if (sf_dev->fb.fix.smem_start)
			lcdc_win02Addr_cfg(sf_dev, sf_dev->fb.fix.smem_start, 0x0);
	    else {
			lcdc_win02Addr_cfg(sf_dev, 0xfb000000, 0x0);
			dev_err(sf_dev->dev, "smem_start is not RIGHT\n");
	    }
	}

	lcdc_win_cfgA(sf_dev, winNum, 0x1, 0x0, 0x0, 0x0, 0x0);
	lcdc_win_cfgB(sf_dev, winNum, 0x0, 0x0, 0x0);
	lcdc_win_cfgC(sf_dev, winNum, 0xffffff);

	lcdc_win_srcSize(sf_dev, winNum);
	lcdc_alphaVal_cfg(sf_dev, 0xf, 0xf, 0xf, 0xf, 0x0);
	lcdc_panel_cfg(sf_dev, 0x3, 0x4, 0x0, 0x0, 0x0, 0x1);  //rgb888sel?
}
EXPORT_SYMBOL(lcdc_config);

void lcdc_run(struct sf_fb_data *sf_dev, uint32_t winMode, uint32_t lcdTrig)
{
	uint32_t runcfg = winMode << LCDC_EN_CFG_MODE | lcdTrig;
	sf_fb_lcdcwrite32(sf_dev, LCDC_SWITCH, runcfg);
	LCDC_PRT("Start run LCDC\n");
}
EXPORT_SYMBOL(lcdc_run);

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("loadable LCDC driver for StarFive");
MODULE_LICENSE("GPL");
