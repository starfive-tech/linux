// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include "starfive_drm_vpp.h"
#include "starfive_drm_crtc.h"
#include <soc/sifive/sifive_l2_cache.h>

static inline void sf_set_clear(void __iomem *addr, u32 reg, u32 set, u32 clear)
{
	u32 value = ioread32(addr + reg);

	value &= ~clear;
	value |= set;
	iowrite32(value, addr + reg);
}

static u32 sf_fb_sysread32(struct starfive_crtc *sf_crtc, u32 reg)
{
	return ioread32(sf_crtc->base_syscfg + reg);
}

static void sf_fb_syswrite32(struct starfive_crtc *sf_crtc, u32 reg, u32 val)
{
	iowrite32(val, sf_crtc->base_syscfg + reg);
}

static u32 sf_fb_vppread32(struct starfive_crtc *sf_crtc, int ppNum, u32 reg)
{
	void __iomem *base_vpp;

	switch (ppNum) {
	case 0:
		base_vpp = sf_crtc->base_vpp0;
		break;
	case 1:
		base_vpp = sf_crtc->base_vpp1;
		break;
	case 2:
		base_vpp = sf_crtc->base_vpp2;
		break;
	default:
		dev_err(sf_crtc->dev, "Err：invalid vpp Number!\n");
		return 0;
	}

	return ioread32(base_vpp + reg);
}

static void sf_fb_vppwrite32(struct starfive_crtc *sf_crtc, int ppNum, u32 reg, u32 val)
{
	void __iomem *base_vpp;

	switch (ppNum) {
	case 0:
		base_vpp = sf_crtc->base_vpp0;
		break;
	case 1:
		base_vpp = sf_crtc->base_vpp1;
		break;
	case 2:
		base_vpp = sf_crtc->base_vpp2;
		break;
	default:
		dev_err(sf_crtc->dev, "Err：invalid vpp Number!\n");
		return;
	}
	iowrite32(val, base_vpp + reg);
}

void mapconv_pp0_sel(struct starfive_crtc *sf_crtc, int sel)
{
	u32 temp;

	temp = sf_fb_sysread32(sf_crtc, SYS_MAP_CONV);
	temp &= ~(0x1);
	temp |= (sel & 0x1);
	sf_fb_syswrite32(sf_crtc, SYS_MAP_CONV, temp);
}

static void pp_output_cfg(struct starfive_crtc *sf_crtc,
			  int ppNum, int outSel, int progInter, int desformat, int ptMode)
{
	int cfg = outSel | progInter << PP_INTERLACE |
		desformat << PP_DES_FORMAT |
		ptMode << PP_POINTER_MODE;
	int preCfg = sf_fb_vppread32(sf_crtc, ppNum, PP_CTRL1) & 0xffff8f0U;

	sf_fb_vppwrite32(sf_crtc, ppNum, PP_CTRL1, cfg | preCfg);
	dev_dbg(sf_crtc->dev, "PP%d outSel: %d, outFormat: 0x%x, Out Interlace: %d, ptMode: %d\n",
		ppNum, outSel, desformat, progInter, ptMode);
}

static void pp_srcfmt_cfg(struct starfive_crtc *sf_crtc, int ppNum, int srcformat,
			  int yuv420Inter, int yuv422_mode, int yuv420_mode, int argbOrd)
{
	int cfg = srcformat << PP_SRC_FORMAT_N |
		yuv420Inter << PP_420_ITLC |
		yuv422_mode << PP_SRC_422_YUV_POS |
		yuv420_mode << PP_SRC_420_YUV_POS |
		argbOrd << PP_SRC_ARGB_ORDER;
	int preCfg = sf_fb_vppread32(sf_crtc, ppNum, PP_CTRL1) & 0x83ffff0fU;

	sf_fb_vppwrite32(sf_crtc, ppNum, PP_CTRL1, cfg | preCfg);
	dev_dbg(sf_crtc->dev, "PP%d Src Format: 0x%x, YUV420 Interlace: %d, YUV422: %d, YUV420: %d, ARGB Order: %d\n",
		ppNum, srcformat, yuv420Inter, yuv422_mode, yuv420_mode, argbOrd);
}

static void pp_r2yscal_bypass(struct starfive_crtc *sf_crtc,
			      int ppNum, int r2yByp, int scalByp, int y2rByp)
{
	int bypass = (r2yByp | scalByp << 1 | y2rByp << 2) << PP_R2Y_BPS;
	int preCfg = sf_fb_vppread32(sf_crtc, ppNum, PP_CTRL1) & 0xffff8fffU;

	sf_fb_vppwrite32(sf_crtc, ppNum, PP_CTRL1, bypass | preCfg);
	dev_dbg(sf_crtc->dev, "PP%d Bypass R2Y: %d, Y2R: %d, MainSacle: %d\n",
		ppNum, r2yByp, y2rByp, scalByp);
}

static void pp_argb_alpha(struct starfive_crtc *sf_crtc, int ppNum, int alpha)
{
	int preCfg = sf_fb_vppread32(sf_crtc, ppNum, PP_CTRL1) & 0xff00ffffU;

	sf_fb_vppwrite32(sf_crtc, ppNum, PP_CTRL1, alpha << PP_ARGB_ALPHA | preCfg);
	dev_dbg(sf_crtc->dev, "PP%d Alpha: 0x%4x\n", ppNum, alpha);
}

//rgbNum: 1-3
static void pp_r2y_coeff(struct starfive_crtc *sf_crtc,
			 int ppNum, int coefNum, int rcoef, int gcoef, int bcoef, int off)
{
	int rgcoeff = rcoef | gcoef << PP_COEF_G1;
	int bcoefoff = bcoef | off << PP_OFFSET_1;
	u32 addr1 = (coefNum - 1) * 0x8 + PP_R2Y_COEF1;
	u32 addr2 = (coefNum - 1) * 0x8 + PP_R2Y_COEF2;

	sf_fb_vppwrite32(sf_crtc, ppNum, addr1, rgcoeff);
	sf_fb_vppwrite32(sf_crtc, ppNum, addr2, bcoefoff);
	dev_dbg(sf_crtc->dev, "PP%d coefNum: %d, rCoef: 0x%4x, gCoef: 0x%4x, bCoef: 0x%4x, off: 0x%4x\n",
		ppNum, coefNum, rcoef, gcoef, bcoef, off);
}

static void pp_output_fmt_cfg(struct starfive_crtc *sf_crtc,
			      int ppNum, int yuv420Inter, int yuv422_mode)
{
	int preCfg = sf_fb_vppread32(sf_crtc, ppNum, PP_CTRL2) & 0xfffffffeU;

	preCfg = preCfg |
		yuv420Inter << PP_DES_420_ORDER |
		yuv422_mode << PP_DES_422_ORDER;
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_CTRL2, preCfg);
	dev_dbg(sf_crtc->dev, "PP%d Lock Transfer: %d\n", ppNum, yuv422_mode);
}

static void pp_lockTrans_cfg(struct starfive_crtc *sf_crtc, int ppNum, int lockTrans)
{
	int preCfg = sf_fb_vppread32(sf_crtc, ppNum, PP_CTRL2) & 0xfffffffeU;

	sf_fb_vppwrite32(sf_crtc, ppNum, PP_CTRL2, lockTrans | preCfg);
	dev_dbg(sf_crtc->dev, "PP%d Lock Transfer: %d\n", ppNum, lockTrans);
}

static void pp_int_interval_cfg(struct starfive_crtc *sf_crtc, int ppNum, int interval)
{
	int preCfg = sf_fb_vppread32(sf_crtc, ppNum, PP_CTRL2) & 0xffff00ffU;

	sf_fb_vppwrite32(sf_crtc, ppNum, PP_CTRL2, interval << PP_INT_INTERVAL | preCfg);
	dev_dbg(sf_crtc->dev, "PP%d Frame Interrupt interval: %d Frames\n", ppNum, interval);
}

static void pp_srcSize_cfg(struct starfive_crtc *sf_crtc, int ppNum, int hsize, int vsize)
{
	int size = hsize | vsize << PP_SRC_VSIZE;

	sf_fb_vppwrite32(sf_crtc, ppNum, PP_SRC_SIZE, size);
	dev_dbg(sf_crtc->dev, "PP%d HSize: %d, VSize: %d\n", ppNum, hsize, vsize);
}

//0-no drop, 1-1/2, 2-1/4, down to 1/32
static void pp_drop_cfg(struct starfive_crtc *sf_crtc, int ppNum, int hdrop, int vdrop)
{
	int drop = hdrop | vdrop << PP_DROP_VRATION;

	sf_fb_vppwrite32(sf_crtc, ppNum, PP_DROP_CTRL, drop);
	dev_dbg(sf_crtc->dev, "PP%d HDrop: %d, VDrop: %d\n", ppNum, hdrop, vdrop);
}

static void pp_desSize_cfg(struct starfive_crtc *sf_crtc, int ppNum, int hsize, int vsize)
{
	int size = hsize | vsize << PP_DES_VSIZE;

	sf_fb_vppwrite32(sf_crtc, ppNum, PP_DES_SIZE, size);
	dev_dbg(sf_crtc->dev, "PP%d HSize: %d, VSize: %d\n", ppNum, hsize, vsize);
}

static void pp_desAddr_cfg(struct starfive_crtc *sf_crtc,
			   int ppNum, int yaddr, int uaddr, int vaddr)
{
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_DES_Y_SA, yaddr);
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_DES_U_SA, uaddr);
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_DES_V_SA, vaddr);
	dev_dbg(sf_crtc->dev, "PP%d des-Addr Y: 0x%8x, U: 0x%8x, V: 0x%8x\n",
		ppNum, yaddr, uaddr, vaddr);
}

static void pp_desOffset_cfg(struct starfive_crtc *sf_crtc,
			     int ppNum, int yoff, int uoff, int voff)
{
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_DES_Y_OFS, yoff);
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_DES_U_OFS, uoff);
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_DES_V_OFS, voff);
	dev_dbg(sf_crtc->dev, "PP%d des-Offset Y: 0x%4x, U: 0x%4x, V: 0x%4x\n",
		ppNum, yoff, uoff, voff);
}

void pp_intcfg(struct starfive_crtc *sf_crtc, int ppNum, int intMask)
{
	int intcfg = ~(0x1<<0);

	if (intMask)
		intcfg = 0xf;
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_INT_MASK, intcfg);
}

//next source frame Y/RGB start address, ?
void pp_srcAddr_next(struct starfive_crtc *sf_crtc, int ppNum, int ysa, int usa, int vsa)
{
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_SRC_Y_SA_NXT, ysa);
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_SRC_U_SA_NXT, usa);
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_SRC_V_SA_NXT, vsa);
	dev_dbg(sf_crtc->dev,
		"PP%d next Y startAddr: 0x%8x, U startAddr: 0x%8x, V startAddr: 0x%8x\n",
		ppNum, ysa, usa, vsa);
}

void pp_srcOffset_cfg(struct starfive_crtc *sf_crtc, int ppNum, int yoff, int uoff, int voff)
{
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_SRC_Y_OFS, yoff);
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_SRC_U_OFS, uoff);
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_SRC_V_OFS, voff);
	dev_dbg(sf_crtc->dev, "PP%d src-Offset Y: 0x%4x, U: 0x%4x, V: 0x%4x\n",
		ppNum, yoff, uoff, voff);
}

void pp_nxtAddr_load(struct starfive_crtc *sf_crtc, int ppNum, int nxtPar, int nxtPos)
{
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_LOAD_NXT_PAR, nxtPar | nxtPos);
	dev_dbg(sf_crtc->dev, "PP%d next addrPointer: %d, %d set Regs\n", ppNum, nxtPar, nxtPos);
}

void pp_run(struct starfive_crtc *sf_crtc, int ppNum, int start)
{
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_SWITCH, start);
	//if (start)
	//	dev_dbg(sf_crtc->dev, "Now start the PP%d\n\n", ppNum);
}

void pp1_enable_intr(struct starfive_crtc *sf_crtc)
{
	sf_fb_vppwrite32(sf_crtc, 1, PP_INT_MASK, 0x0);
}

void pp_enable_intr(struct starfive_crtc *sf_crtc, int ppNum)
{
	u32 cfg = 0xfffe;

	sf_fb_vppwrite32(sf_crtc, ppNum, PP_INT_MASK, cfg);
}

void pp_disable_intr(struct starfive_crtc *sf_crtc, int ppNum)
{
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_INT_MASK, 0xf);
	sf_fb_vppwrite32(sf_crtc, ppNum, PP_INT_CLR, 0xf);
}

static void pp_srcfmt_set(struct starfive_crtc *sf_crtc, int ppNum, struct pp_video_mode *src)
{
	switch (src->format) {
	case COLOR_YUV422_YVYU:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_YUV422, 0x0, COLOR_YUV422_YVYU, 0x0, 0x0);
		break;
	case COLOR_YUV422_VYUY:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_YUV422, 0x0, COLOR_YUV422_VYUY, 0x0, 0x0);
		break;
	case COLOR_YUV422_YUYV:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_YUV422, 0x0, COLOR_YUV422_YUYV, 0x0, 0x0);
		break;
	case COLOR_YUV422_UYVY:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_YUV422, 0x0, COLOR_YUV422_UYVY, 0x0, 0x0);
		break;
	case COLOR_YUV420P:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_YUV420P, 0x0, 0, 0x0, 0x0);
		break;
	case COLOR_YUV420_NV21:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_YUV420I, 0x1, 0,
			      COLOR_YUV420_NV21 - COLOR_YUV420_NV21, 0x0);
		break;
	case COLOR_YUV420_NV12:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_YUV420I, 0x1, 0,
			      COLOR_YUV420_NV12 - COLOR_YUV420_NV21, 0x0);
		break;
	case COLOR_RGB888_ARGB:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_GRB888, 0x0, 0x0,
			      0x0, COLOR_RGB888_ARGB - COLOR_RGB888_ARGB);
		break;
	case COLOR_RGB888_ABGR:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_GRB888, 0x0, 0x0,
			      0x0, COLOR_RGB888_ABGR-COLOR_RGB888_ARGB);
		break;
	case COLOR_RGB888_RGBA:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_GRB888, 0x0, 0x0,
			      0x0, COLOR_RGB888_RGBA-COLOR_RGB888_ARGB);
		break;
	case COLOR_RGB888_BGRA:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_GRB888, 0x0, 0x0,
			      0x0, COLOR_RGB888_BGRA-COLOR_RGB888_ARGB);
		break;
	case COLOR_RGB565:
		pp_srcfmt_cfg(sf_crtc, ppNum, PP_SRC_RGB565, 0x0, 0x0, 0x0, 0x0);
		break;
	}
}

static void pp_dstfmt_set(struct starfive_crtc *sf_crtc, int ppNum, struct pp_video_mode *dst)
{
	unsigned int outsel = 1;

	if (dst->addr)
		outsel = 0;

	switch (dst->format) {
	case COLOR_YUV422_YVYU:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_YUV422, 0x0);
		pp_output_fmt_cfg(sf_crtc, ppNum, 0, COLOR_YUV422_UYVY - COLOR_YUV422_YVYU);
		break;
	case COLOR_YUV422_VYUY:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_YUV422, 0x0);
		pp_output_fmt_cfg(sf_crtc, ppNum, 0, COLOR_YUV422_UYVY - COLOR_YUV422_VYUY);
		break;
	case COLOR_YUV422_YUYV:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_YUV422, 0x0);
		pp_output_fmt_cfg(sf_crtc, ppNum, 0, COLOR_YUV422_UYVY - COLOR_YUV422_YUYV);
		break;
	case COLOR_YUV422_UYVY:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_YUV422, 0x0);
		pp_output_fmt_cfg(sf_crtc, ppNum, 0, COLOR_YUV422_UYVY - COLOR_YUV422_YVYU);
		break;
	case COLOR_YUV420P:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_YUV420P, 0x0);
		pp_output_fmt_cfg(sf_crtc, ppNum, 0, 0);
		break;
	case COLOR_YUV420_NV21:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_YUV420I, 0x0);
		pp_output_fmt_cfg(sf_crtc, ppNum, COLOR_YUV420_NV21 - COLOR_YUV420_NV21, 0);
		break;
	case COLOR_YUV420_NV12:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_YUV420I, 0x0);///0x2, 0x0);
		//pp_output_fmt_cfg(ppNum, COLOR_YUV420_NV12 - COLOR_YUV420_NV21, 0);
		break;
	case COLOR_RGB888_ARGB:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_ARGB888, 0x0);
		//pp_output_fmt_cfg(ppNum, 0, 0);
		break;
	case COLOR_RGB888_ABGR:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_ABGR888, 0x0);
		pp_output_fmt_cfg(sf_crtc, ppNum, 0, 0);
		break;
	case COLOR_RGB888_RGBA:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_RGBA888, 0x0);
		pp_output_fmt_cfg(sf_crtc, ppNum, 0, 0);
		break;
	case COLOR_RGB888_BGRA:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_BGRA888, 0x0);
		pp_output_fmt_cfg(sf_crtc, ppNum, 0, 0);
		break;
	case COLOR_RGB565:
		pp_output_cfg(sf_crtc, ppNum, outsel, 0x0, PP_DST_RGB565, 0x0);
		pp_output_fmt_cfg(sf_crtc, ppNum, 0, 0);
		break;
	}
}

static void pp_format_set(struct starfive_crtc *sf_crtc, int ppNum,
			  struct pp_video_mode *src, struct pp_video_mode *dst)
{
	/* 1:bypass, 0:not bypass */
	unsigned int scale_byp = 1;

	pp_srcfmt_set(sf_crtc, ppNum, src);
	pp_dstfmt_set(sf_crtc, ppNum, dst);

	if (src->height != dst->height || src->width != dst->width)
		scale_byp = 0;

	if (src->format >= COLOR_RGB888_ARGB && dst->format <= COLOR_YUV420_NV12) {
		/* rgb -> yuv-420 */
		pp_r2yscal_bypass(sf_crtc, ppNum, NOT_BYPASS, scale_byp, BYPASS);
		pp_r2y_coeff(sf_crtc, ppNum, 1, R2Y_COEF_R1, R2Y_COEF_G1, R2Y_COEF_B1, R2Y_OFFSET1);
		pp_r2y_coeff(sf_crtc, ppNum, 2, R2Y_COEF_R2, R2Y_COEF_G2, R2Y_COEF_B2, R2Y_OFFSET2);
		pp_r2y_coeff(sf_crtc, ppNum, 3, R2Y_COEF_R3, R2Y_COEF_G3, R2Y_COEF_B3, R2Y_OFFSET3);
	} else if (src->format <= COLOR_YUV420_NV12 && dst->format >= COLOR_RGB888_ARGB) {
		/* yuv-420 -> rgb */
		pp_r2yscal_bypass(sf_crtc, ppNum, BYPASS, scale_byp, NOT_BYPASS);
	} else if (src->format <= COLOR_YUV422_YVYU && dst->format <= COLOR_YUV420_NV12) {
		/* yuv422 -> yuv420 */
		pp_r2yscal_bypass(sf_crtc, ppNum, BYPASS, scale_byp, BYPASS);
	} else {
		/* rgb565->argb888 */
		pp_r2yscal_bypass(sf_crtc, ppNum, BYPASS, scale_byp, BYPASS);
	} //else if ((src->format >= COLOR_RGB888_ARGB) && (dst->format >= COLOR_RGB888_ARGB)) {
		/* rgb -> rgb */
		// pp_r2yscal_bypass(ppNum, BYPASS, scale_byp, BYPASS);
	//}
	pp_argb_alpha(sf_crtc, ppNum, 0xff);

	if (dst->addr)
		pp_lockTrans_cfg(sf_crtc, ppNum, SYS_BUS_OUTPUT);
	else
		pp_lockTrans_cfg(sf_crtc, ppNum, FIFO_OUTPUT);

	pp_int_interval_cfg(sf_crtc, ppNum, 0x1);
}

static void pp_size_set(struct starfive_crtc *sf_crtc, int ppNum,
			struct pp_video_mode *src, struct pp_video_mode *dst)
{
	u32 srcAddr, dstaddr;
	unsigned int size, y_rgb_ofst, uofst;
	unsigned int v_uvofst = 0, next_y_rgb_addr = 0, next_u_addr = 0, next_v_addr = 0;
	unsigned int i = 0;

	pp_srcSize_cfg(sf_crtc, ppNum, src->width - 1, src->height - 1);
	pp_drop_cfg(sf_crtc, ppNum, 0x0, 0x0);///0:no drop
	pp_desSize_cfg(sf_crtc, ppNum, dst->width - 1, dst->height - 1);

	srcAddr = src->addr + (i<<30); //PP_SRC_BASE_ADDR + (i<<30);
	size = src->width * src->height;

	if (src->format >= COLOR_RGB888_ARGB) {
		next_y_rgb_addr = srcAddr;
		next_u_addr = 0;
		next_v_addr = 0;

		y_rgb_ofst = 0;
		uofst = 0;
		v_uvofst = 0;
		//pp_srcAddr_next(ppNum, srcAddr, 0, 0);
		//pp_srcOffset_cfg(ppNum, 0x0, 0x0, 0x0);
	} else {
		if (src->format == COLOR_YUV420_NV21) {    //ok
			next_y_rgb_addr = srcAddr;
			next_u_addr = srcAddr+size+1;
			next_v_addr = srcAddr+size;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = size;
		} else if (src->format == COLOR_YUV420_NV12) {
			next_y_rgb_addr = srcAddr;
			next_u_addr = srcAddr+size;
			next_v_addr = srcAddr+size+1;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = size;
		} else if (src->format == COLOR_YUV420P) {
			next_y_rgb_addr = srcAddr;
			next_u_addr = srcAddr+size;
			next_v_addr = srcAddr+size*5/4;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = 0;
		} else if (src->format == COLOR_YUV422_YVYU) {   //ok
			next_y_rgb_addr = srcAddr;
			next_u_addr = srcAddr+1;
			next_v_addr = srcAddr+3;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = 0;
		} else if (src->format == COLOR_YUV422_VYUY) {   //ok
			next_y_rgb_addr = srcAddr+1;
			next_u_addr = srcAddr+2;
			next_v_addr = srcAddr;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = 0;
		} else if (src->format == COLOR_YUV422_YUYV) {   //ok
			next_y_rgb_addr = srcAddr;
			next_u_addr = srcAddr+1;
			next_v_addr = srcAddr+2;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = 0;
		} else if (src->format == COLOR_YUV422_UYVY) {  //ok
			next_y_rgb_addr = srcAddr+1;
			next_u_addr = srcAddr;
			next_v_addr = srcAddr+2;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = 0;
		}
	}
	pp_srcAddr_next(sf_crtc, ppNum, next_y_rgb_addr, next_u_addr, next_v_addr);
	pp_srcOffset_cfg(sf_crtc, ppNum, y_rgb_ofst, uofst, v_uvofst);
	/* source addr not change */
	pp_nxtAddr_load(sf_crtc, ppNum, 0x1, (i & 0x1));

	if (dst->addr) {
		dstaddr = dst->addr;
		size = dst->height*dst->width;
		if (dst->format >= COLOR_RGB888_ARGB) {
			next_y_rgb_addr = dstaddr;
			next_u_addr = 0;
			next_v_addr = 0;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = 0;
		} else {
			if (dst->format == COLOR_YUV420_NV21) {
				/* yyyyvuvuvu */
				next_y_rgb_addr = dstaddr;
				next_u_addr = dstaddr+size;
				next_v_addr = 0;//dstaddr+size;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV420_NV12) {
				/* yyyyuvuvuv */
				next_y_rgb_addr = dstaddr;
				next_u_addr = dstaddr+size;
				next_v_addr = dstaddr+size+1;
				y_rgb_ofst = 0;
				uofst = size;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV420P) {
				next_y_rgb_addr = dstaddr;
				next_u_addr = dstaddr+size;
				next_v_addr = dstaddr+size*5/4;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV422_YVYU) {
				next_y_rgb_addr = dstaddr;
				next_u_addr = dstaddr+1;
				next_v_addr = dstaddr+3;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV422_VYUY) {
				next_y_rgb_addr = dstaddr+1;
				next_u_addr = dstaddr+2;
				next_v_addr = dstaddr;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV422_YUYV) {
				next_y_rgb_addr = dstaddr;
				next_u_addr = dstaddr+1;
				next_v_addr = dstaddr+2;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV422_UYVY) {
				next_y_rgb_addr = dstaddr+1;
				next_u_addr = dstaddr;
				next_v_addr = dstaddr+2;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			}
		}
		pp_desAddr_cfg(sf_crtc, ppNum, next_y_rgb_addr, next_u_addr, next_v_addr);
		pp_desOffset_cfg(sf_crtc, ppNum, y_rgb_ofst, uofst, v_uvofst);
	}
}

static void pp_config(struct starfive_crtc *sf_crtc, int ppNum,
		      struct pp_video_mode *src, struct pp_video_mode *dst)
{
	//pp_disable_intr(sf_dev, ppNum);
	pp_format_set(sf_crtc, ppNum, src, dst);
	pp_size_set(sf_crtc, ppNum, src, dst);
}

irqreturn_t vpp1_isr_handler(int this_irq, void *dev_id)
{
	struct starfive_crtc *sf_crtc = dev_id;

	sf_fb_vppread32(sf_crtc, 1, PP_INT_STATUS);
	sf_fb_vppwrite32(sf_crtc, 1, PP_INT_CLR, 0xf);
	sifive_l2_flush_range(sf_crtc->dma_addr, sf_crtc->size);

	return IRQ_HANDLED;
}

static void starfive_pp_enable_intr(struct starfive_crtc *sf_crtc, int enable)
{
	int pp_id;

	for (pp_id = 0; pp_id < PP_NUM; pp_id++) {
		if (sf_crtc->pp[pp_id].inited == 1) {
			if (enable)
				pp_enable_intr(sf_crtc, pp_id);
			else
				pp_disable_intr(sf_crtc, pp_id);
		}
	}
}

static int starfive_pp_video_mode_init(struct starfive_crtc *sf_crtc,
				       struct pp_video_mode *src,
				       struct pp_video_mode *dst,
				       int pp_id)
{
	if (!src || !dst) {
		dev_err(sf_crtc->dev, "Invalid argument!\n");
		return -EINVAL;
	}

	if (pp_id < PP_NUM && pp_id >= 0) {
		src->format = sf_crtc->vpp_format;
		src->width = sf_crtc->crtc.state->adjusted_mode.hdisplay;
		src->height = sf_crtc->crtc.state->adjusted_mode.vdisplay;
		src->addr = sf_crtc->dma_addr;
		//src->addr = 0xa0000000;
		dst->format = sf_crtc->pp[pp_id].dst.format;
		dst->width = sf_crtc->crtc.state->adjusted_mode.hdisplay;
		dst->height = sf_crtc->crtc.state->adjusted_mode.vdisplay;
		if (sf_crtc->pp[pp_id].bus_out) /*out to ddr*/
			dst->addr = 0xfc000000;
		else if (sf_crtc->pp[pp_id].fifo_out) /*out to lcdc*/
			dst->addr = 0;
	} else {
		dev_err(sf_crtc->dev, "pp_id %d is not support\n", pp_id);
		return -EINVAL;
	}

	return 0;
}

static int starfive_pp_init(struct starfive_crtc *sf_crtc)
{
	int pp_id;
	int ret = 0;
	struct pp_video_mode src, dst;

	for (pp_id = 0; pp_id < PP_NUM; pp_id++) {
		if (sf_crtc->pp[pp_id].inited == 1) {
			ret = starfive_pp_video_mode_init(sf_crtc, &src, &dst, pp_id);
			if (!ret)
				pp_config(sf_crtc, pp_id, &src, &dst);
		}
	}

	return ret;
}

static int starfive_pp_run(struct starfive_crtc *sf_crtc)
{
	int pp_id;
	int ret = 0;

	for (pp_id = 0; pp_id < PP_NUM; pp_id++) {
		if (sf_crtc->pp[pp_id].inited == 1)
			pp_run(sf_crtc, pp_id, PP_RUN);
	}

	return ret;
}

int starfive_pp_enable(struct starfive_crtc *sf_crtc)
{
	starfive_pp_enable_intr(sf_crtc, PP_INTR_DISABLE);

	if (starfive_pp_init(sf_crtc))
		return -ENODEV;

	starfive_pp_run(sf_crtc);
	starfive_pp_enable_intr(sf_crtc, PP_INTR_ENABLE);

	return 0;
}

int starfive_pp_update(struct starfive_crtc *sf_crtc)
{
	int pp_id;
	int ret = 0;
	struct pp_video_mode src, dst;

	for (pp_id = 0; pp_id < PP_NUM; pp_id++) {
		if (sf_crtc->pp[pp_id].inited == 1) {
			ret = starfive_pp_video_mode_init(sf_crtc, &src, &dst, pp_id);
			if (!ret) {
				if (sf_crtc->ddr_format_change)
					pp_format_set(sf_crtc, pp_id, &src, &dst);

				if (sf_crtc->dma_addr_change)
					pp_size_set(sf_crtc, pp_id, &src, &dst);
			}
		}
	}

	return 0;
}

int starfive_pp_get_2lcdc_id(struct starfive_crtc *sf_crtc)
{
	int pp_id;

	for (pp_id = 0; pp_id < PP_NUM; pp_id++) {
		if (sf_crtc->pp[pp_id].inited == 1) {
			if (sf_crtc->pp[pp_id].fifo_out == 1 && !sf_crtc->pp[pp_id].bus_out)
				return pp_id;
		}
	}

	if (pp_id == PP_NUM - 1)
		dev_warn(sf_crtc->dev, "NO pp connect to LCDC\n");

	return -ENODEV;
}

void dsitx_vout_init(struct starfive_crtc *sf_crtc)
{
	u32 temp;

	reset_control_assert(sf_crtc->rst_vout_src);
	reset_control_assert(sf_crtc->rst_disp_axi);
	clk_prepare_enable(sf_crtc->clk_disp_axi);
	clk_prepare_enable(sf_crtc->clk_vout_src);
	reset_control_deassert(sf_crtc->rst_vout_src);
	reset_control_deassert(sf_crtc->rst_disp_axi);

	sf_set_clear(sf_crtc->base_clk, clk_disp0_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_disp1_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_lcdc_oclk_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_lcdc_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_vpp0_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_vpp1_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_vpp2_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_ppi_tx_esc_clk_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_dsi_apb_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_dsi_sys_clk_ctrl_REG, BIT(31), BIT(31));

	sf_set_clear(sf_crtc->base_rst, vout_rstgen_assert0_REG, ~0x1981ec, 0x1981ec);

	do {
		temp = ioread32(sf_crtc->base_rst + vout_rstgen_status0_REG);
		temp &= 0x1981ec;
	} while (temp != 0x1981ec);
}

void vout_reset(struct starfive_crtc *sf_crtc)
{
	u32 temp;

	iowrite32(0xFFFFFFFF, sf_crtc->base_rst);

	clk_prepare_enable(sf_crtc->clk_disp_axi);
	clk_prepare_enable(sf_crtc->clk_vout_src);
	reset_control_deassert(sf_crtc->rst_vout_src);
	reset_control_deassert(sf_crtc->rst_disp_axi);

	sf_set_clear(sf_crtc->base_clk, clk_disp0_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_disp1_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_lcdc_oclk_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_lcdc_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_vpp0_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_vpp1_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_vpp2_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_mapconv_apb_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_mapconv_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_pixrawout_apb_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_pixrawout_axi_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_csi2tx_strm0_apb_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_csi2tx_strm0_pixclk_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_ppi_tx_esc_clk_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_dsi_apb_ctrl_REG, BIT(31), BIT(31));
	sf_set_clear(sf_crtc->base_clk, clk_dsi_sys_clk_ctrl_REG, BIT(31), BIT(31));

	sf_set_clear(sf_crtc->base_rst, vout_rstgen_assert0_REG, ~0x19bfff, 0x19bfff);
	do {
		temp = ioread32(sf_crtc->base_rst + vout_rstgen_status0_REG);
		temp &= 0x19bfff;
	} while (temp != 0x19bfff);
}

void vout_disable(struct starfive_crtc *sf_crtc)
{
	iowrite32(0xFFFFFFFF, sf_crtc->base_rst);

	clk_disable_unprepare(sf_crtc->clk_disp_axi);
	clk_disable_unprepare(sf_crtc->clk_vout_src);
	reset_control_assert(sf_crtc->rst_vout_src);
	reset_control_assert(sf_crtc->rst_disp_axi);
}

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("loadable VPP driver for StarFive");
MODULE_LICENSE("GPL");
