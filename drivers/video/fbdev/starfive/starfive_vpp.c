/* driver/video/starfive/starfive_vpp.c
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License version 2 as
** published by the Free Software Foundation.
**
** Copyright (C) 2020 StarFive, Inc.
**
** PURPOSE:	This files contains the driver of VPP.
**
** CHANGE HISTORY:
**	Version		Date		Author		Description
**	0.1.0		2020-10-09	starfive		created
**
*/

#include <linux/module.h>

#include "starfive_fb.h"
#include "starfive_vpp.h"

//#define SF_PP_DEBUG	1
#ifdef SF_PP_DEBUG
	#define PP_PRT(format, args...)  printk(KERN_DEBUG "[pp]: " format, ## args)
	#define PP_INFO(format, args...) printk(KERN_INFO "[pp]: " format, ## args)
	#define PP_ERR(format, args...) printk(KERN_ERR "[pp]: " format, ## args)
#else
	#define PP_PRT(x...)  do{} while(0)
	#define PP_INFO(x...)  do{} while(0)
	#define PP_ERR(x...)  do{} while(0)
#endif

static u32 sf_fb_sysread32(struct sf_fb_data *sf_dev, u32 reg)
{
	return ioread32(sf_dev->base_syscfg + reg);
}

static void sf_fb_syswrite32(struct sf_fb_data *sf_dev, u32 reg, u32 val)
{
	iowrite32(val, sf_dev->base_syscfg + reg);
}

static u32 sf_fb_vppread32(struct sf_fb_data *sf_dev, int ppNum, u32 reg)
{
	void __iomem	*base_vpp = 0;
	switch(ppNum) {
	    case 0 : {base_vpp = sf_dev->base_vpp0; break;}
	    case 1 : {base_vpp = sf_dev->base_vpp1; break;}
	    case 2 : {base_vpp = sf_dev->base_vpp2; break;}
	    default: {PP_ERR("Err：invalid vpp Number!\n"); break;}
	}
	return ioread32(base_vpp + reg);
}

static void sf_fb_vppwrite32(struct sf_fb_data *sf_dev, int ppNum, u32 reg, u32 val)
{
	void __iomem	*base_vpp = 0;
	switch(ppNum) {
	case 0 : {base_vpp = sf_dev->base_vpp0; break;}
	case 1 : {base_vpp = sf_dev->base_vpp1; break;}
	case 2 : {base_vpp = sf_dev->base_vpp2; break;}
	default: {PP_ERR("Err：invalid vpp Number!\n"); break;}
	}
	iowrite32(val, base_vpp + reg);
}

void mapconv_pp0_sel(struct sf_fb_data *sf_dev, int sel)
{
	u32 temp;
	temp = sf_fb_sysread32(sf_dev, SYS_MAP_CONV);
	temp &= ~(0x1);
	temp |= (sel & 0x1);
	sf_fb_syswrite32(sf_dev, SYS_MAP_CONV, temp);
}
EXPORT_SYMBOL(mapconv_pp0_sel);

void pp_output_cfg(struct sf_fb_data *sf_dev, int ppNum, int outSel, int progInter, int desformat, int ptMode)
{
	int cfg = outSel | progInter << PP_INTERLACE
			  | desformat << PP_DES_FORMAT
			  | ptMode << PP_POINTER_MODE;

	int preCfg = 0xffff8f0 & sf_fb_vppread32(sf_dev, ppNum, PP_CTRL1);
	sf_fb_vppwrite32(sf_dev, ppNum, PP_CTRL1, cfg | preCfg);
	PP_PRT("PP%d outSel: %d, outFormat: 0x%x, Out Interlace: %d, ptMode: %d\n",
		ppNum, outSel, desformat, progInter, ptMode);
}

void pp_srcfmt_cfg(struct sf_fb_data *sf_dev, int ppNum, int srcformat, int yuv420Inter, int yuv422_mode,
		int yuv420_mode, int argbOrd)
{
	int cfg = srcformat << PP_SRC_FORMAT_N | yuv420Inter << PP_420_ITLC
						| yuv422_mode << PP_SRC_422_YUV_POS
						| yuv420_mode << PP_SRC_420_YUV_POS
						| argbOrd << PP_SRC_ARGB_ORDER;

	int preCfg = 0x83ffff0f & sf_fb_vppread32(sf_dev, ppNum, PP_CTRL1);
	sf_fb_vppwrite32(sf_dev, ppNum, PP_CTRL1, cfg | preCfg);
	PP_PRT("PP%d Src Format: 0x%x, YUV420 Interlace: %d, YUV422: %d, YUV420: %d, ARGB Order: %d\n",
		ppNum, srcformat,yuv420Inter,yuv422_mode,yuv420_mode, argbOrd);
}

void pp_r2yscal_bypass(struct sf_fb_data *sf_dev, int ppNum, int r2yByp, int scalByp, int y2rByp)
{
	int bypass = (r2yByp | scalByp<<1 | y2rByp<<2) << PP_R2Y_BPS;
	int preCfg = 0xffff8fff & sf_fb_vppread32(sf_dev, ppNum, PP_CTRL1);
	sf_fb_vppwrite32(sf_dev, ppNum, PP_CTRL1, bypass | preCfg);
	PP_PRT("PP%d Bypass R2Y: %d, Y2R: %d, MainSacle: %d\n", ppNum, r2yByp, y2rByp, scalByp);
}

void pp_argb_alpha(struct sf_fb_data *sf_dev, int ppNum, int alpha)
{
	int preCfg = 0xff00ffff & sf_fb_vppread32(sf_dev, ppNum, PP_CTRL1);
	sf_fb_vppwrite32(sf_dev, ppNum, PP_CTRL1, alpha << PP_ARGB_ALPHA | preCfg);
	PP_PRT("PP%d Alpha: 0x%4x\n", ppNum, alpha);
}

//rgbNum: 1-3
void pp_r2y_coeff(struct sf_fb_data *sf_dev, int ppNum, int coefNum, int rcoef, int gcoef, int bcoef, int off)
{
	int rgcoeff = rcoef | gcoef << PP_COEF_G1;
	int bcoefoff = bcoef| off << PP_OFFSET_1;
	u32 addr1 = (coefNum - 1) * 0x8 + PP_R2Y_COEF1;
	u32 addr2 = (coefNum - 1) * 0x8 + PP_R2Y_COEF2;
	sf_fb_vppwrite32(sf_dev, ppNum, addr1, rgcoeff);
	sf_fb_vppwrite32(sf_dev, ppNum, addr2, bcoefoff);
	PP_PRT("PP%d coefNum: %d, rCoef: 0x%4x, gCoef: 0x%4x, bCoef: 0x%4x, off: 0x%4x\n",
		ppNum, coefNum, rcoef, gcoef, bcoef, off);
}

void pp_output_fmt_cfg(struct sf_fb_data *sf_dev, int ppNum, int yuv420Inter, int yuv422_mode)
{
	int preCfg = 0xfffffffe & sf_fb_vppread32(sf_dev, ppNum, PP_CTRL2);
	preCfg = preCfg | yuv420Inter << PP_DES_420_ORDER
			| yuv422_mode << PP_DES_422_ORDER;
	sf_fb_vppwrite32(sf_dev, ppNum, PP_CTRL2, preCfg);
	PP_PRT("PP%d Lock Transfer: %d\n", ppNum, yuv422_mode);
}

void pp_lockTrans_cfg(struct sf_fb_data *sf_dev, int ppNum, int lockTrans)
{
	int preCfg = 0xfffffffe & sf_fb_vppread32(sf_dev, ppNum, PP_CTRL2);
	sf_fb_vppwrite32(sf_dev, ppNum, PP_CTRL2, lockTrans | preCfg);
	PP_PRT("PP%d Lock Transfer: %d\n", ppNum, lockTrans);
}

void pp_int_interval_cfg(struct sf_fb_data *sf_dev, int ppNum, int interval)
{
	int preCfg = 0xffff00ff & sf_fb_vppread32(sf_dev, ppNum, PP_CTRL2);
	sf_fb_vppwrite32(sf_dev, ppNum, PP_CTRL2, interval << PP_INT_INTERVAL | preCfg);
	PP_PRT("PP%d Frame Interrupt interval: %d Frames\n", ppNum, interval);
}

void pp_srcSize_cfg(struct sf_fb_data *sf_dev, int ppNum, int hsize, int vsize)
{
  int size = hsize | vsize << PP_SRC_VSIZE;
  sf_fb_vppwrite32(sf_dev, ppNum, PP_SRC_SIZE, size);
  PP_PRT("PP%d HSize: %d, VSize: %d\n", ppNum, hsize, vsize);
}

//0-no drop, 1-1/2, 2-1/4, down to 1/32
void pp_drop_cfg(struct sf_fb_data *sf_dev, int ppNum, int hdrop, int vdrop)
{
	int drop = hdrop | vdrop << PP_DROP_VRATION;
	sf_fb_vppwrite32(sf_dev, ppNum, PP_DROP_CTRL, drop);
	PP_PRT("PP%d HDrop: %d, VDrop: %d\n", ppNum, hdrop, vdrop);
}


void pp_desSize_cfg(struct sf_fb_data *sf_dev, int ppNum, int hsize, int vsize)
{
  int size = hsize | vsize << PP_DES_VSIZE;
  sf_fb_vppwrite32(sf_dev, ppNum, PP_DES_SIZE, size);
  PP_PRT("PP%d HSize: %d, VSize: %d\n", ppNum, hsize, vsize);
}

void pp_desAddr_cfg(struct sf_fb_data *sf_dev, int ppNum, int yaddr, int uaddr, int vaddr)
{
   sf_fb_vppwrite32(sf_dev, ppNum, PP_DES_Y_SA, yaddr);
   sf_fb_vppwrite32(sf_dev, ppNum, PP_DES_U_SA, uaddr);
   sf_fb_vppwrite32(sf_dev, ppNum, PP_DES_V_SA, vaddr);
   PP_PRT("PP%d des-Addr Y: 0x%8x, U: 0x%8x, V: 0x%8x\n", ppNum, yaddr, uaddr, vaddr);
}

void pp_desOffset_cfg(struct sf_fb_data *sf_dev, int ppNum, int yoff, int uoff, int voff)
{
   sf_fb_vppwrite32(sf_dev, ppNum, PP_DES_Y_OFS, yoff);
   sf_fb_vppwrite32(sf_dev, ppNum, PP_DES_U_OFS, uoff);
   sf_fb_vppwrite32(sf_dev, ppNum, PP_DES_V_OFS, voff);
   PP_PRT("PP%d des-Offset Y: 0x%4x, U: 0x%4x, V: 0x%4x\n", ppNum, yoff, uoff, voff);
}


void pp_intcfg(struct sf_fb_data *sf_dev, int ppNum, int intMask)
{
   int intcfg = ~(0x1<<0);

   if(intMask)
       intcfg = 0xf;
   sf_fb_vppwrite32(sf_dev, ppNum, PP_INT_MASK, intcfg);
}
EXPORT_SYMBOL(pp_intcfg);

//next source frame Y/RGB start address, ?
void pp_srcAddr_next(struct sf_fb_data *sf_dev, int ppNum, int ysa, int usa, int vsa)
{
  sf_fb_vppwrite32(sf_dev, ppNum, PP_SRC_Y_SA_NXT, ysa);
  sf_fb_vppwrite32(sf_dev, ppNum, PP_SRC_U_SA_NXT, usa);
  sf_fb_vppwrite32(sf_dev, ppNum, PP_SRC_V_SA_NXT, vsa);
  PP_PRT("PP%d next Y startAddr: 0x%8x, U startAddr: 0x%8x, V startAddr: 0x%8x\n", ppNum, ysa, usa, vsa);
}
EXPORT_SYMBOL(pp_srcAddr_next);

void pp_srcOffset_cfg(struct sf_fb_data *sf_dev, int ppNum, int yoff, int uoff, int voff)
{
   sf_fb_vppwrite32(sf_dev, ppNum, PP_SRC_Y_OFS, yoff);
   sf_fb_vppwrite32(sf_dev, ppNum, PP_SRC_U_OFS, uoff);
   sf_fb_vppwrite32(sf_dev, ppNum, PP_SRC_V_OFS, voff);
   PP_PRT("PP%d src-Offset Y: 0x%4x, U: 0x%4x, V: 0x%4x\n", ppNum, yoff, uoff, voff);
}
EXPORT_SYMBOL(pp_srcOffset_cfg);

void pp_nxtAddr_load(struct sf_fb_data *sf_dev, int ppNum, int nxtPar, int nxtPos)
{
  sf_fb_vppwrite32(sf_dev, ppNum, PP_LOAD_NXT_PAR, nxtPar | nxtPos);
  PP_PRT("PP%d next addrPointer: %d, %d set Regs\n", ppNum, nxtPar, nxtPos);
}
EXPORT_SYMBOL(pp_nxtAddr_load);

void pp_run(struct sf_fb_data *sf_dev, int ppNum, int start)
{
   sf_fb_vppwrite32(sf_dev, ppNum, PP_SWITCH, start);
   //if(start)
   //  PP_PRT("Now start the PP%d\n\n", ppNum);
}
EXPORT_SYMBOL(pp_run);

void pp1_enable_intr(struct sf_fb_data *sf_dev)
{
	sf_fb_vppwrite32(sf_dev, 1, PP_INT_MASK, 0x0);
}
EXPORT_SYMBOL(pp1_enable_intr);

void pp_enable_intr(struct sf_fb_data *sf_dev, int ppNum)
{
	u32 cfg = 0xfffe;

	sf_fb_vppwrite32(sf_dev, ppNum, PP_INT_MASK, cfg);
}
EXPORT_SYMBOL(pp_enable_intr);

void pp_disable_intr(struct sf_fb_data *sf_dev, int ppNum)
{
	sf_fb_vppwrite32(sf_dev, ppNum, PP_INT_MASK, 0xf);
	sf_fb_vppwrite32(sf_dev, ppNum, PP_INT_CLR, 0xf);
}
EXPORT_SYMBOL(pp_disable_intr);

static void pp_srcfmt_set(struct sf_fb_data *sf_dev, int ppNum, struct pp_video_mode *src)
{
    switch(src->format)
    {
        case COLOR_YUV422_YVYU:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_YUV422, 0x0, COLOR_YUV422_YVYU, 0x0, 0x0);
            break;
        case COLOR_YUV422_VYUY:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_YUV422, 0x0, COLOR_YUV422_VYUY, 0x0, 0x0);
            break;
        case COLOR_YUV422_YUYV:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_YUV422, 0x0, COLOR_YUV422_YUYV, 0x0, 0x0);
            break;
        case COLOR_YUV422_UYVY:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_YUV422, 0x0, COLOR_YUV422_UYVY, 0x0, 0x0);
            break;
        case COLOR_YUV420P:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_YUV420P, 0x0, 0, 0x0, 0x0);
            break;
        case COLOR_YUV420_NV21:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_YUV420I, 0x1, 0, COLOR_YUV420_NV21-COLOR_YUV420_NV21, 0x0);
            break;
        case COLOR_YUV420_NV12:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_YUV420I, 0x1, 0, COLOR_YUV420_NV12-COLOR_YUV420_NV21, 0x0);
            break;
        case COLOR_RGB888_ARGB:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_GRB888, 0x0, 0x0, 0x0, COLOR_RGB888_ARGB-COLOR_RGB888_ARGB);//0x0);
            break;
        case COLOR_RGB888_ABGR:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_GRB888, 0x0, 0x0, 0x0, COLOR_RGB888_ABGR-COLOR_RGB888_ARGB);//0x1);
            break;
        case COLOR_RGB888_RGBA:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_GRB888, 0x0, 0x0, 0x0, COLOR_RGB888_RGBA-COLOR_RGB888_ARGB);//0x2);
            break;
        case COLOR_RGB888_BGRA:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_GRB888, 0x0, 0x0, 0x0, COLOR_RGB888_BGRA-COLOR_RGB888_ARGB);//0x3);
            break;
        case COLOR_RGB565:
            pp_srcfmt_cfg(sf_dev, ppNum, PP_SRC_RGB565, 0x0, 0x0, 0x0, 0x0);
            break;
    }
}

static void pp_dstfmt_set(struct sf_fb_data *sf_dev, int ppNum, struct pp_video_mode *dst)
{
    unsigned int outsel = 1;
    if(dst->addr)
    {
        outsel = 0;
    }

    switch(dst->format) {
        case COLOR_YUV422_YVYU:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_YUV422, 0x0);
            pp_output_fmt_cfg(sf_dev, ppNum, 0, COLOR_YUV422_UYVY - COLOR_YUV422_YVYU);
            break;
        case COLOR_YUV422_VYUY:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_YUV422, 0x0);
            pp_output_fmt_cfg(sf_dev, ppNum, 0, COLOR_YUV422_UYVY - COLOR_YUV422_VYUY);
            break;
        case COLOR_YUV422_YUYV:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_YUV422, 0x0);
            pp_output_fmt_cfg(sf_dev, ppNum, 0, COLOR_YUV422_UYVY - COLOR_YUV422_YUYV);
            break;
        case COLOR_YUV422_UYVY:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_YUV422, 0x0);
            pp_output_fmt_cfg(sf_dev, ppNum, 0, COLOR_YUV422_UYVY - COLOR_YUV422_YVYU);
            break;
        case COLOR_YUV420P:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_YUV420P, 0x0);
            pp_output_fmt_cfg(sf_dev, ppNum, 0, 0);
            break;
        case COLOR_YUV420_NV21:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_YUV420I, 0x0);
            pp_output_fmt_cfg(sf_dev, ppNum, COLOR_YUV420_NV21 - COLOR_YUV420_NV21, 0);
            break;
        case COLOR_YUV420_NV12:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_YUV420I, 0x0);///0x2, 0x0);
            //pp_output_fmt_cfg(ppNum, COLOR_YUV420_NV12 - COLOR_YUV420_NV21, 0);
            break;
        case COLOR_RGB888_ARGB:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_ARGB888, 0x0);
            //pp_output_fmt_cfg(ppNum, 0, 0);
            break;
        case COLOR_RGB888_ABGR:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_ABGR888, 0x0);
            pp_output_fmt_cfg(sf_dev, ppNum, 0, 0);
            break;
        case COLOR_RGB888_RGBA:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_RGBA888, 0x0);
            pp_output_fmt_cfg(sf_dev, ppNum, 0, 0);
            break;
        case COLOR_RGB888_BGRA:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_BGRA888, 0x0);
            pp_output_fmt_cfg(sf_dev, ppNum, 0, 0);
            break;
        case COLOR_RGB565:
            pp_output_cfg(sf_dev, ppNum, outsel, 0x0, PP_DST_RGB565, 0x0);
            pp_output_fmt_cfg(sf_dev, ppNum, 0, 0);
            break;
    }
}


void pp_format_set(struct sf_fb_data *sf_dev, int ppNum, struct pp_video_mode *src, struct pp_video_mode *dst)
{

    /* 1:bypass, 0:not bypass */
    unsigned int scale_byp = 1;

    pp_srcfmt_set(sf_dev, ppNum, src);
    pp_dstfmt_set(sf_dev, ppNum, dst);

    if((src->height != dst->height) || (src->width != dst->width)) {
        scale_byp = 0;
    }

    if((src->format >= COLOR_RGB888_ARGB) && (dst->format <= COLOR_YUV420_NV12)) {
        /* rgb -> yuv-420 */
        pp_r2yscal_bypass(sf_dev, ppNum, NOT_BYPASS, scale_byp, BYPASS);
        pp_r2y_coeff(sf_dev, ppNum, 1, R2Y_COEF_R1, R2Y_COEF_G1, R2Y_COEF_B1, R2Y_OFFSET1);
        pp_r2y_coeff(sf_dev, ppNum, 2, R2Y_COEF_R2, R2Y_COEF_G2, R2Y_COEF_B2, R2Y_OFFSET2);
        pp_r2y_coeff(sf_dev, ppNum, 3, R2Y_COEF_R3, R2Y_COEF_G3, R2Y_COEF_B3, R2Y_OFFSET3);
    } else if ((src->format <= COLOR_YUV420_NV12) && (dst->format >= COLOR_RGB888_ARGB)) {
        /* yuv-420 -> rgb */
        pp_r2yscal_bypass(sf_dev, ppNum, BYPASS, scale_byp, NOT_BYPASS);
    } else if ((src->format <= COLOR_YUV422_YVYU) && (dst->format <= COLOR_YUV420_NV12)) {
        /* yuv422 -> yuv420 */
        pp_r2yscal_bypass(sf_dev, ppNum, BYPASS, scale_byp, BYPASS);
    } else {
        /* rgb565->argb888 */
        pp_r2yscal_bypass(sf_dev, ppNum, BYPASS, scale_byp, BYPASS);
    } //else if((src->format >= COLOR_RGB888_ARGB) && (dst->format >= COLOR_RGB888_ARGB))
    {
        /* rgb -> rgb */
       // pp_r2yscal_bypass(ppNum, BYPASS, scale_byp, BYPASS);
    }
    pp_argb_alpha(sf_dev, ppNum, 0xff);

    if(dst->addr) {
        pp_lockTrans_cfg(sf_dev, ppNum, SYS_BUS_OUTPUT);
    } else {
        pp_lockTrans_cfg(sf_dev, ppNum, FIFO_OUTPUT);
    }

    pp_int_interval_cfg(sf_dev, ppNum, 0x1);

}

void pp_size_set(struct sf_fb_data *sf_dev, int ppNum, struct pp_video_mode *src, struct pp_video_mode *dst)
{
    uint32_t srcAddr, dstaddr;
    unsigned int size, y_rgb_ofst, uofst;
	unsigned int v_uvofst = 0, next_y_rgb_addr = 0, next_u_addr = 0, next_v_addr = 0;
    unsigned int i = 0;

    pp_srcSize_cfg(sf_dev, ppNum, src->width - 1, src->height - 1);
    pp_drop_cfg(sf_dev, ppNum, 0x0, 0x0);///0:no drop
    pp_desSize_cfg(sf_dev, ppNum, dst->width - 1, dst->height - 1);

    srcAddr = src->addr + (i<<30);///PP_SRC_BASE_ADDR + (i<<30);
    size = src->width * src->height;

    if(src->format >= COLOR_RGB888_ARGB) {
        next_y_rgb_addr = srcAddr;
        next_u_addr = 0;
        next_v_addr = 0;

        y_rgb_ofst = 0;
        uofst = 0;
        v_uvofst = 0;

        //pp_srcAddr_next(ppNum, srcAddr, 0, 0);
        //pp_srcOffset_cfg(ppNum, 0x0, 0x0, 0x0);

    } else {
        //if((src->format == COLOR_YUV420_NV21) || (src->format == COLOR_YUV420_NV12)){
        if(src->format == COLOR_YUV420_NV21) {    //ok
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
        } else if(src->format == COLOR_YUV422_YUYV) {   //ok
            next_y_rgb_addr = srcAddr;
            next_u_addr = srcAddr+1;
            next_v_addr = srcAddr+2;
            y_rgb_ofst = 0;
            uofst = 0;
            v_uvofst = 0;
        } else if(src->format == COLOR_YUV422_UYVY) {  //ok
            next_y_rgb_addr = srcAddr+1;
            next_u_addr = srcAddr;
            next_v_addr = srcAddr+2;
            y_rgb_ofst = 0;
            uofst = 0;
            v_uvofst = 0;
        }
    }
    pp_srcAddr_next(sf_dev, ppNum, next_y_rgb_addr, next_u_addr, next_v_addr);
    pp_srcOffset_cfg(sf_dev, ppNum, y_rgb_ofst, uofst, v_uvofst);
    /* source addr not change */
    pp_nxtAddr_load(sf_dev, ppNum, 0x1, (i & 0x1));

    if(dst->addr) {
        dstaddr = dst->addr;
        size = dst->height*dst->width;
        if(dst->format >= COLOR_RGB888_ARGB) {
            next_y_rgb_addr = dstaddr;
            next_u_addr = 0;
            next_v_addr = 0;
            y_rgb_ofst = 0;
            uofst = 0;
            v_uvofst = 0;
        } else {
            if(dst->format == COLOR_YUV420_NV21) {
				/* yyyyvuvuvu */
                next_y_rgb_addr = dstaddr;
                next_u_addr = dstaddr+size;
                next_v_addr = 0;//dstaddr+size;
                y_rgb_ofst = 0;
                uofst = 0;
                v_uvofst = 0;
            } else if (dst->format == COLOR_YUV420_NV12){
				/* yyyyuvuvuv */
                next_y_rgb_addr = dstaddr;
                next_u_addr = dstaddr+size;
                next_v_addr = dstaddr+size+1;
                y_rgb_ofst = 0;
                uofst = size;
                v_uvofst = 0;
            } else if(dst->format == COLOR_YUV420P) {
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
            } else if(dst->format == COLOR_YUV422_VYUY) {
                next_y_rgb_addr = dstaddr+1;
                next_u_addr = dstaddr+2;
                next_v_addr = dstaddr;
                y_rgb_ofst = 0;
                uofst = 0;
                v_uvofst = 0;
            } else if(dst->format == COLOR_YUV422_YUYV) {
                next_y_rgb_addr = dstaddr;
                next_u_addr = dstaddr+1;
                next_v_addr = dstaddr+2;
                y_rgb_ofst = 0;
                uofst = 0;
                v_uvofst = 0;
            } else if(dst->format == COLOR_YUV422_UYVY) {
                next_y_rgb_addr = dstaddr+1;
                next_u_addr = dstaddr;
                next_v_addr = dstaddr+2;
                y_rgb_ofst = 0;
                uofst = 0;
                v_uvofst = 0;
            }
        }
        pp_desAddr_cfg(sf_dev, ppNum, next_y_rgb_addr, next_u_addr, next_v_addr);
        pp_desOffset_cfg(sf_dev, ppNum, y_rgb_ofst, uofst, v_uvofst);
    }

}


void pp_config(struct sf_fb_data *sf_dev, int ppNum, struct pp_video_mode *src, struct pp_video_mode *dst)
{
	//pp_disable_intr(sf_dev, ppNum);
	pp_format_set(sf_dev, ppNum, src, dst);
	pp_size_set(sf_dev, ppNum, src, dst);
}
EXPORT_SYMBOL(pp_config);

irqreturn_t vpp1_isr_handler(int this_irq, void *dev_id)
{
	struct sf_fb_data *sf_dev = (struct sf_fb_data *)dev_id;
	static int count = 0;
	sf_fb_vppwrite32(sf_dev, 1, PP_INT_CLR, 0xf);

	count ++;
	if(0 == count % 60)
		PP_PRT("=");
		//printk("=");

	return IRQ_HANDLED;
}
EXPORT_SYMBOL(vpp1_isr_handler);

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("loadable VPP driver for StarFive");
MODULE_LICENSE("GPL");
