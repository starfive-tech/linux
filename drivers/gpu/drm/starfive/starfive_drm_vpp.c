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
#include <soc/sifive/sifive_ccache.h>

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

static u32 sf_fb_vppread32(struct starfive_crtc *sf_crtc, int pp_num, u32 reg)
{
	void __iomem *base_vpp;

	switch (pp_num) {
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

static void sf_fb_vppwrite32(struct starfive_crtc *sf_crtc, int pp_num, u32 reg, u32 val)
{
	void __iomem *base_vpp;

	switch (pp_num) {
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
			  int pp_num, int out_sel, int prog_inter, int desformat,
			  int pt_mode)
{
	int cfg = out_sel | prog_inter << PP_INTERLACE |
		desformat << PP_DES_FORMAT |
		pt_mode << PP_POINTER_MODE;
	int pre_cfg = sf_fb_vppread32(sf_crtc, pp_num, PP_CTRL1) & 0xffff8f0U;

	sf_fb_vppwrite32(sf_crtc, pp_num, PP_CTRL1, cfg | pre_cfg);
	dev_dbg(sf_crtc->dev, "PP%d out_sel: %d, outFormat: 0x%x, Out Interlace: %d, pt_mode: %d\n",
		pp_num, out_sel, desformat, prog_inter, pt_mode);
}

static void pp_srcfmt_cfg(struct starfive_crtc *sf_crtc, int pp_num, int srcformat,
			  int yuv420_inter, int yuv422_mode, int yuv420_mode, int argb_ord)
{
	int cfg = srcformat << PP_SRC_FORMAT_N |
		yuv420_inter << PP_420_ITLC |
		yuv422_mode << PP_SRC_422_YUV_POS |
		yuv420_mode << PP_SRC_420_YUV_POS |
		argb_ord << PP_SRC_ARGB_ORDER;
	int pre_cfg = sf_fb_vppread32(sf_crtc, pp_num, PP_CTRL1) & 0x83ffff0fU;

	sf_fb_vppwrite32(sf_crtc, pp_num, PP_CTRL1, cfg | pre_cfg);
	dev_dbg(sf_crtc->dev, "PP%d Src Format: 0x%x, YUV420 Interlace: %d, YUV422: %d, YUV420: %d, ARGB Order: %d\n",
		pp_num, srcformat, yuv420_inter, yuv422_mode, yuv420_mode, argb_ord);
}

static void pp_r2yscal_bypass(struct starfive_crtc *sf_crtc,
			      int pp_num, int r2y_byp, int scal_byp, int y2r_byp)
{
	int bypass = (r2y_byp | scal_byp << 1 | y2r_byp << 2) << PP_R2Y_BPS;
	int pre_cfg = sf_fb_vppread32(sf_crtc, pp_num, PP_CTRL1) & 0xffff8fffU;

	sf_fb_vppwrite32(sf_crtc, pp_num, PP_CTRL1, bypass | pre_cfg);
	dev_dbg(sf_crtc->dev, "PP%d Bypass R2Y: %d, Y2R: %d, MainSacle: %d\n",
		pp_num, r2y_byp, y2r_byp, scal_byp);
}

static void pp_argb_alpha(struct starfive_crtc *sf_crtc, int pp_num, int alpha)
{
	int pre_cfg = sf_fb_vppread32(sf_crtc, pp_num, PP_CTRL1) & 0xff00ffffU;

	sf_fb_vppwrite32(sf_crtc, pp_num, PP_CTRL1, alpha << PP_ARGB_ALPHA | pre_cfg);
	dev_dbg(sf_crtc->dev, "PP%d Alpha: 0x%4x\n", pp_num, alpha);
}

//rgbNum: 1-3
static void pp_r2y_coeff(struct starfive_crtc *sf_crtc,
			 int pp_num, int coef_num, int rcoef, int gcoef, int bcoef, int off)
{
	int rgcoeff = rcoef | gcoef << PP_COEF_G1;
	int bcoefoff = bcoef | off << PP_OFFSET_1;
	u32 addr1 = (coef_num - 1) * 0x8 + PP_R2Y_COEF1;
	u32 addr2 = (coef_num - 1) * 0x8 + PP_R2Y_COEF2;

	sf_fb_vppwrite32(sf_crtc, pp_num, addr1, rgcoeff);
	sf_fb_vppwrite32(sf_crtc, pp_num, addr2, bcoefoff);
	dev_dbg(sf_crtc->dev, "PP%d coef_num: %d, rCoef: 0x%4x, gCoef: 0x%4x, bCoef: 0x%4x, off: 0x%4x\n",
		pp_num, coef_num, rcoef, gcoef, bcoef, off);
}

static void pp_output_fmt_cfg(struct starfive_crtc *sf_crtc,
			      int pp_num, int yuv420_inter, int yuv422_mode)
{
	int pre_cfg = sf_fb_vppread32(sf_crtc, pp_num, PP_CTRL2) & 0xfffffffeU;

	pre_cfg = pre_cfg |
		yuv420_inter << PP_DES_420_ORDER |
		yuv422_mode << PP_DES_422_ORDER;
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_CTRL2, pre_cfg);
	dev_dbg(sf_crtc->dev, "PP%d Lock Transfer: %d\n", pp_num, yuv422_mode);
}

static void pp_lock_trans_cfg(struct starfive_crtc *sf_crtc, int pp_num, int lock_trans)
{
	int pre_cfg = sf_fb_vppread32(sf_crtc, pp_num, PP_CTRL2) & 0xfffffffeU;

	sf_fb_vppwrite32(sf_crtc, pp_num, PP_CTRL2, lock_trans | pre_cfg);
	dev_dbg(sf_crtc->dev, "PP%d Lock Transfer: %d\n", pp_num, lock_trans);
}

static void pp_int_interval_cfg(struct starfive_crtc *sf_crtc, int pp_num, int interval)
{
	int pre_cfg = sf_fb_vppread32(sf_crtc, pp_num, PP_CTRL2) & 0xffff00ffU;

	sf_fb_vppwrite32(sf_crtc, pp_num, PP_CTRL2, interval << PP_INT_INTERVAL | pre_cfg);
	dev_dbg(sf_crtc->dev, "PP%d Frame Interrupt interval: %d Frames\n", pp_num, interval);
}

static void pp_src_size_cfg(struct starfive_crtc *sf_crtc, int pp_num, int hsize, int vsize)
{
	int size = hsize | vsize << PP_SRC_VSIZE;

	sf_fb_vppwrite32(sf_crtc, pp_num, PP_SRC_SIZE, size);
	dev_dbg(sf_crtc->dev, "PP%d HSize: %d, VSize: %d\n", pp_num, hsize, vsize);
}

//0-no drop, 1-1/2, 2-1/4, down to 1/32
static void pp_drop_cfg(struct starfive_crtc *sf_crtc, int pp_num, int hdrop, int vdrop)
{
	int drop = hdrop | vdrop << PP_DROP_VRATION;

	sf_fb_vppwrite32(sf_crtc, pp_num, PP_DROP_CTRL, drop);
	dev_dbg(sf_crtc->dev, "PP%d HDrop: %d, VDrop: %d\n", pp_num, hdrop, vdrop);
}

static void pp_des_size_cfg(struct starfive_crtc *sf_crtc, int pp_num, int hsize, int vsize)
{
	int size = hsize | vsize << PP_DES_VSIZE;

	sf_fb_vppwrite32(sf_crtc, pp_num, PP_DES_SIZE, size);
	dev_dbg(sf_crtc->dev, "PP%d HSize: %d, VSize: %d\n", pp_num, hsize, vsize);
}

static void pp_des_addr_cfg(struct starfive_crtc *sf_crtc,
			    int pp_num, int yaddr, int uaddr, int vaddr)
{
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_DES_Y_SA, yaddr);
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_DES_U_SA, uaddr);
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_DES_V_SA, vaddr);
	dev_dbg(sf_crtc->dev, "PP%d des-Addr Y: 0x%8x, U: 0x%8x, V: 0x%8x\n",
		pp_num, yaddr, uaddr, vaddr);
}

static void pp_des_offset_cfg(struct starfive_crtc *sf_crtc,
			      int pp_num, int yoff, int uoff, int voff)
{
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_DES_Y_OFS, yoff);
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_DES_U_OFS, uoff);
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_DES_V_OFS, voff);
	dev_dbg(sf_crtc->dev, "PP%d des-Offset Y: 0x%4x, U: 0x%4x, V: 0x%4x\n",
		pp_num, yoff, uoff, voff);
}

void pp_intcfg(struct starfive_crtc *sf_crtc, int pp_num, int int_mask)
{
	int intcfg = ~(0x1 << 0);

	if (int_mask)
		intcfg = 0xf;
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_INT_MASK, intcfg);
}

//next source frame Y/RGB start address, ?
void pp_src_addr_next(struct starfive_crtc *sf_crtc, int pp_num, int ysa, int usa, int vsa)
{
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_SRC_Y_SA_NXT, ysa);
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_SRC_U_SA_NXT, usa);
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_SRC_V_SA_NXT, vsa);
	dev_dbg(sf_crtc->dev,
		"PP%d next Y startAddr: 0x%8x, U startAddr: 0x%8x, V startAddr: 0x%8x\n",
		pp_num, ysa, usa, vsa);
}

void pp_src_offset_cfg(struct starfive_crtc *sf_crtc, int pp_num, int yoff, int uoff, int voff)
{
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_SRC_Y_OFS, yoff);
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_SRC_U_OFS, uoff);
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_SRC_V_OFS, voff);
	dev_dbg(sf_crtc->dev, "PP%d src-Offset Y: 0x%4x, U: 0x%4x, V: 0x%4x\n",
		pp_num, yoff, uoff, voff);
}

void pp_nxt_addr_load(struct starfive_crtc *sf_crtc, int pp_num, int nxt_par, int nxt_pos)
{
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_LOAD_NXT_PAR, nxt_par | nxt_pos);
	dev_dbg(sf_crtc->dev, "PP%d next addrPointer: %d, %d set Regs\n", pp_num, nxt_par, nxt_pos);
}

void pp_run(struct starfive_crtc *sf_crtc, int pp_num, int start)
{
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_SWITCH, start);
	//if (start)
	//	dev_dbg(sf_crtc->dev, "Now start the PP%d\n\n", pp_num);
}

void pp1_enable_intr(struct starfive_crtc *sf_crtc)
{
	sf_fb_vppwrite32(sf_crtc, 1, PP_INT_MASK, 0x0);
}

void pp_enable_intr(struct starfive_crtc *sf_crtc, int pp_num)
{
	u32 cfg = 0xfffe;

	sf_fb_vppwrite32(sf_crtc, pp_num, PP_INT_MASK, cfg);
}

void pp_disable_intr(struct starfive_crtc *sf_crtc, int pp_num)
{
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_INT_MASK, 0xf);
	sf_fb_vppwrite32(sf_crtc, pp_num, PP_INT_CLR, 0xf);
}

static void pp_srcfmt_set(struct starfive_crtc *sf_crtc, int pp_num, struct pp_video_mode *src)
{
	switch (src->format) {
	case COLOR_YUV422_YVYU:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_YUV422, 0x0, COLOR_YUV422_YVYU, 0x0, 0x0);
		break;
	case COLOR_YUV422_VYUY:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_YUV422, 0x0, COLOR_YUV422_VYUY, 0x0, 0x0);
		break;
	case COLOR_YUV422_YUYV:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_YUV422, 0x0, COLOR_YUV422_YUYV, 0x0, 0x0);
		break;
	case COLOR_YUV422_UYVY:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_YUV422, 0x0, COLOR_YUV422_UYVY, 0x0, 0x0);
		break;
	case COLOR_YUV420P:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_YUV420P, 0x0, 0, 0x0, 0x0);
		break;
	case COLOR_YUV420_NV21:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_YUV420I, 0x1, 0,
			      COLOR_YUV420_NV21 - COLOR_YUV420_NV21, 0x0);
		break;
	case COLOR_YUV420_NV12:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_YUV420I, 0x1, 0,
			      COLOR_YUV420_NV12 - COLOR_YUV420_NV21, 0x0);
		break;
	case COLOR_RGB888_ARGB:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_GRB888, 0x0, 0x0,
			      0x0, COLOR_RGB888_ARGB - COLOR_RGB888_ARGB);
		break;
	case COLOR_RGB888_ABGR:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_GRB888, 0x0, 0x0,
			      0x0, COLOR_RGB888_ABGR - COLOR_RGB888_ARGB);
		break;
	case COLOR_RGB888_RGBA:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_GRB888, 0x0, 0x0,
			      0x0, COLOR_RGB888_RGBA - COLOR_RGB888_ARGB);
		break;
	case COLOR_RGB888_BGRA:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_GRB888, 0x0, 0x0,
			      0x0, COLOR_RGB888_BGRA - COLOR_RGB888_ARGB);
		break;
	case COLOR_RGB565:
		pp_srcfmt_cfg(sf_crtc, pp_num, PP_SRC_RGB565, 0x0, 0x0, 0x0, 0x0);
		break;
	}
}

static void pp_dstfmt_set(struct starfive_crtc *sf_crtc, int pp_num, struct pp_video_mode *dst)
{
	unsigned int outsel = 1;

	if (dst->addr)
		outsel = 0;

	switch (dst->format) {
	case COLOR_YUV422_YVYU:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_YUV422, 0x0);
		pp_output_fmt_cfg(sf_crtc, pp_num, 0, COLOR_YUV422_UYVY - COLOR_YUV422_YVYU);
		break;
	case COLOR_YUV422_VYUY:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_YUV422, 0x0);
		pp_output_fmt_cfg(sf_crtc, pp_num, 0, COLOR_YUV422_UYVY - COLOR_YUV422_VYUY);
		break;
	case COLOR_YUV422_YUYV:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_YUV422, 0x0);
		pp_output_fmt_cfg(sf_crtc, pp_num, 0, COLOR_YUV422_UYVY - COLOR_YUV422_YUYV);
		break;
	case COLOR_YUV422_UYVY:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_YUV422, 0x0);
		pp_output_fmt_cfg(sf_crtc, pp_num, 0, COLOR_YUV422_UYVY - COLOR_YUV422_YVYU);
		break;
	case COLOR_YUV420P:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_YUV420P, 0x0);
		pp_output_fmt_cfg(sf_crtc, pp_num, 0, 0);
		break;
	case COLOR_YUV420_NV21:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_YUV420I, 0x0);
		pp_output_fmt_cfg(sf_crtc, pp_num, COLOR_YUV420_NV21 - COLOR_YUV420_NV21, 0);
		break;
	case COLOR_YUV420_NV12:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_YUV420I, 0x0);///0x2, 0x0);
		//pp_output_fmt_cfg(pp_num, COLOR_YUV420_NV12 - COLOR_YUV420_NV21, 0);
		break;
	case COLOR_RGB888_ARGB:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_ARGB888, 0x0);
		//pp_output_fmt_cfg(pp_num, 0, 0);
		break;
	case COLOR_RGB888_ABGR:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_ABGR888, 0x0);
		pp_output_fmt_cfg(sf_crtc, pp_num, 0, 0);
		break;
	case COLOR_RGB888_RGBA:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_RGBA888, 0x0);
		pp_output_fmt_cfg(sf_crtc, pp_num, 0, 0);
		break;
	case COLOR_RGB888_BGRA:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_BGRA888, 0x0);
		pp_output_fmt_cfg(sf_crtc, pp_num, 0, 0);
		break;
	case COLOR_RGB565:
		pp_output_cfg(sf_crtc, pp_num, outsel, 0x0, PP_DST_RGB565, 0x0);
		pp_output_fmt_cfg(sf_crtc, pp_num, 0, 0);
		break;
	}
}

static void pp_format_set(struct starfive_crtc *sf_crtc, int pp_num,
			  struct pp_video_mode *src, struct pp_video_mode *dst)
{
	/* 1:bypass, 0:not bypass */
	unsigned int scale_byp = 1;

	pp_srcfmt_set(sf_crtc, pp_num, src);
	pp_dstfmt_set(sf_crtc, pp_num, dst);

	if (src->height != dst->height || src->width != dst->width)
		scale_byp = 0;

	if (src->format >= COLOR_RGB888_ARGB && dst->format <= COLOR_YUV420_NV12) {
		/* rgb -> yuv-420 */
		pp_r2yscal_bypass(sf_crtc, pp_num, NOT_BYPASS, scale_byp, BYPASS);
		pp_r2y_coeff(sf_crtc, pp_num, 1, R2Y_COEF_R1, R2Y_COEF_G1,
			     R2Y_COEF_B1, R2Y_OFFSET1);
		pp_r2y_coeff(sf_crtc, pp_num, 2, R2Y_COEF_R2, R2Y_COEF_G2,
			     R2Y_COEF_B2, R2Y_OFFSET2);
		pp_r2y_coeff(sf_crtc, pp_num, 3, R2Y_COEF_R3, R2Y_COEF_G3,
			     R2Y_COEF_B3, R2Y_OFFSET3);
	} else if (src->format <= COLOR_YUV420_NV12 && dst->format >= COLOR_RGB888_ARGB) {
		/* yuv-420 -> rgb */
		pp_r2yscal_bypass(sf_crtc, pp_num, BYPASS, scale_byp, NOT_BYPASS);
	} else if (src->format <= COLOR_YUV422_YVYU && dst->format <= COLOR_YUV420_NV12) {
		/* yuv422 -> yuv420 */
		pp_r2yscal_bypass(sf_crtc, pp_num, BYPASS, scale_byp, BYPASS);
	} else {
		/* rgb565->argb888 */
		pp_r2yscal_bypass(sf_crtc, pp_num, BYPASS, scale_byp, BYPASS);
	} //else if ((src->format >= COLOR_RGB888_ARGB) && (dst->format >= COLOR_RGB888_ARGB)) {
		/* rgb -> rgb */
		// pp_r2yscal_bypass(pp_num, BYPASS, scale_byp, BYPASS);
	//}
	pp_argb_alpha(sf_crtc, pp_num, 0xff);

	if (dst->addr)
		pp_lock_trans_cfg(sf_crtc, pp_num, SYS_BUS_OUTPUT);
	else
		pp_lock_trans_cfg(sf_crtc, pp_num, FIFO_OUTPUT);

	pp_int_interval_cfg(sf_crtc, pp_num, 0x1);
}

static void pp_size_set(struct starfive_crtc *sf_crtc, int pp_num,
			struct pp_video_mode *src, struct pp_video_mode *dst)
{
	u32 src_addr, dstaddr;
	unsigned int size, y_rgb_ofst, uofst;
	unsigned int v_uvofst = 0, next_y_rgb_addr = 0, next_u_addr = 0, next_v_addr = 0;
	unsigned int i = 0;

	pp_src_size_cfg(sf_crtc, pp_num, src->width - 1, src->height - 1);
	pp_drop_cfg(sf_crtc, pp_num, 0x0, 0x0);///0:no drop
	pp_des_size_cfg(sf_crtc, pp_num, dst->width - 1, dst->height - 1);

	src_addr = src->addr + (i << 30); //PP_SRC_BASE_ADDR + (i << 30);
	size = src->width * src->height;

	if (src->format >= COLOR_RGB888_ARGB) {
		next_y_rgb_addr = src_addr;
		next_u_addr = 0;
		next_v_addr = 0;

		y_rgb_ofst = 0;
		uofst = 0;
		v_uvofst = 0;
		//pp_src_addr_next(pp_num, src_addr, 0, 0);
		//pp_src_offset_cfg(pp_num, 0x0, 0x0, 0x0);
	} else {
		if (src->format == COLOR_YUV420_NV21) {    //ok
			next_y_rgb_addr = src_addr;
			next_u_addr = src_addr + size + 1;
			next_v_addr = src_addr + size;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = size;
		} else if (src->format == COLOR_YUV420_NV12) {
			next_y_rgb_addr = src_addr;
			next_u_addr = src_addr + size;
			next_v_addr = src_addr + size + 1;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = size;
		} else if (src->format == COLOR_YUV420P) {
			next_y_rgb_addr = src_addr;
			next_u_addr = src_addr + size;
			next_v_addr = src_addr + size * 5 / 4;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = 0;
		} else if (src->format == COLOR_YUV422_YVYU) {   //ok
			next_y_rgb_addr = src_addr;
			next_u_addr = src_addr + 1;
			next_v_addr = src_addr + 3;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = 0;
		} else if (src->format == COLOR_YUV422_VYUY) {   //ok
			next_y_rgb_addr = src_addr + 1;
			next_u_addr = src_addr + 2;
			next_v_addr = src_addr;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = 0;
		} else if (src->format == COLOR_YUV422_YUYV) {   //ok
			next_y_rgb_addr = src_addr;
			next_u_addr = src_addr + 1;
			next_v_addr = src_addr + 2;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = 0;
		} else if (src->format == COLOR_YUV422_UYVY) {  //ok
			next_y_rgb_addr = src_addr + 1;
			next_u_addr = src_addr;
			next_v_addr = src_addr + 2;
			y_rgb_ofst = 0;
			uofst = 0;
			v_uvofst = 0;
		}
	}
	pp_src_addr_next(sf_crtc, pp_num, next_y_rgb_addr, next_u_addr, next_v_addr);
	pp_src_offset_cfg(sf_crtc, pp_num, y_rgb_ofst, uofst, v_uvofst);
	/* source addr not change */
	pp_nxt_addr_load(sf_crtc, pp_num, 0x1, (i & 0x1));

	if (dst->addr) {
		dstaddr = dst->addr;
		size = dst->height * dst->width;
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
				next_u_addr = dstaddr + size;
				next_v_addr = 0;//dstaddr + size;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV420_NV12) {
				/* yyyyuvuvuv */
				next_y_rgb_addr = dstaddr;
				next_u_addr = dstaddr + size;
				next_v_addr = dstaddr + size + 1;
				y_rgb_ofst = 0;
				uofst = size;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV420P) {
				next_y_rgb_addr = dstaddr;
				next_u_addr = dstaddr + size;
				next_v_addr = dstaddr + size * 5 / 4;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV422_YVYU) {
				next_y_rgb_addr = dstaddr;
				next_u_addr = dstaddr + 1;
				next_v_addr = dstaddr + 3;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV422_VYUY) {
				next_y_rgb_addr = dstaddr + 1;
				next_u_addr = dstaddr + 2;
				next_v_addr = dstaddr;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV422_YUYV) {
				next_y_rgb_addr = dstaddr;
				next_u_addr = dstaddr + 1;
				next_v_addr = dstaddr + 2;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			} else if (dst->format == COLOR_YUV422_UYVY) {
				next_y_rgb_addr = dstaddr + 1;
				next_u_addr = dstaddr;
				next_v_addr = dstaddr + 2;
				y_rgb_ofst = 0;
				uofst = 0;
				v_uvofst = 0;
			}
		}
		pp_des_addr_cfg(sf_crtc, pp_num, next_y_rgb_addr, next_u_addr, next_v_addr);
		pp_des_offset_cfg(sf_crtc, pp_num, y_rgb_ofst, uofst, v_uvofst);
	}
}

static void pp_config(struct starfive_crtc *sf_crtc, int pp_num,
		      struct pp_video_mode *src, struct pp_video_mode *dst)
{
	//pp_disable_intr(sf_dev, pp_num);
	pp_format_set(sf_crtc, pp_num, src, dst);
	pp_size_set(sf_crtc, pp_num, src, dst);
}

irqreturn_t vpp1_isr_handler(int this_irq, void *dev_id)
{
	struct starfive_crtc *sf_crtc = dev_id;

	sf_fb_vppread32(sf_crtc, 1, PP_INT_STATUS);
	sf_fb_vppwrite32(sf_crtc, 1, PP_INT_CLR, 0xf);
	sifive_ccache_flush_range(sf_crtc->dma_addr, sf_crtc->size);

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
