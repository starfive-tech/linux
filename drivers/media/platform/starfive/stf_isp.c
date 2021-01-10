/* /drivers/media/platform/starfive/stf_isp.c
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
**	0.1.0		2020-12-09	starfive		created
**
*/
#include <asm/io.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <video/stf-vin.h>
#include <linux/delay.h>

#define SF_ISP_DEBUG
#ifdef SF_ISP_DEBUG
	#define ISP_PRT(format, args...)    printk(KERN_DEBUG "[ISP]: " format, ## args)
	#define ISP_INFO(format, args...)   printk(KERN_INFO "[ISP]: " format, ## args)
	#define ISP_ERR(format, args...)	printk(KERN_ERR "[ISP]: " format, ## args)
#else
	#define ISP_PRT(x...)  do{} while(0)
	#define ISP_INFO(x...)  do{} while(0)
	#define ISP_ERR(x...)  do{} while(0)
#endif


static inline u32 reg_read(void __iomem * base, u32 reg)
{
	return ioread32(base + reg);
}

static inline void reg_write(void __iomem * base, u32 reg, u32 val)
{
	iowrite32(val, base + reg);
}

void isp_ddr_format_config(struct stf_vin_dev *vin)
{
	void __iomem *ispbase;
    
	if(vin->isp0)
		ispbase = vin->isp_isp0_base;
	else if(vin->isp1)
	    ispbase = vin->isp_isp1_base;
	else
		return;
	
	switch (vin->format.format) {
	case SRC_COLORBAR_VIN_ISP:
	    reg_write(ispbase, ISP_REG_DVP_POLARITY_CFG, 0xd);
		break;

	case SRC_DVP_SENSOR_VIN_ISP:
	    reg_write(ispbase, ISP_REG_DVP_POLARITY_CFG, 0x08);
		break;

	default:
		pr_err("unknown format\n");
		return;
	}

	reg_write(ispbase, ISP_REG_RAW_FORMAT_CFG, 0x000011BB);	// sym_order = [0+:16]
	reg_write(ispbase, ISP_REG_CFA_MODE, 0x00000030);
	reg_write(ispbase, ISP_REG_PIC_CAPTURE_START_CFG, 0x00000000); // cro_hstart = [0+:16], cro_vstart = [16+:16]
}

void isp_ddr_resolution_config(struct stf_vin_dev *vin)
{
	u32 val = 0;
	void __iomem *ispbase;
    
	if(vin->isp0)
		ispbase = vin->isp_isp0_base;
	else if(vin->isp1)
	    ispbase = vin->isp_isp1_base;
	else
		return;
	val = (vin->frame.width-1) + ((vin->frame.height-1)<<16);

	reg_write(ispbase, ISP_REG_PIC_CAPTURE_END_CFG, val);	// cro_hend = [0+:16], cro_vend = [16+:16]
	val = (vin->frame.width) + ((vin->frame.height)<<16);
	reg_write(ispbase, ISP_REG_PIPELINE_XY_SIZE, val);	// ISP pipeline width[0+:16], height[16+:16]

	reg_write(ispbase, ISP_REG_Y_PLANE_START_ADDR, vin->buf.paddr);	// Unscaled Output Image Y Plane Start Address Register
	reg_write(ispbase, ISP_REG_UV_PLANE_START_ADDR,
		  vin->buf.paddr + (vin->frame.width * vin->frame.height));	// Unscaled Output Image UV Plane Start Address Register

	reg_write(ispbase, ISP_REG_STRIDE, vin->frame.width);	// Unscaled Output Image Stride Register

	reg_write(ispbase, ISP_REG_PIXEL_COORDINATE_GEN, 0x00000010);	// Unscaled Output Pixel Coordinate Generator Mode Register
	reg_write(ispbase, ISP_REG_PIXEL_AXI_CONTROL, 0x00000000);
	reg_write(ispbase, ISP_REG_SS_AXI_CONTROL, 0x00000000);

	reg_write(ispbase, ISP_REG_RGB_TO_YUV_COVERSION0, 0x0000004D);	// ICCONV_0
	reg_write(ispbase, ISP_REG_RGB_TO_YUV_COVERSION1, 0x00000096);	// ICCONV_1
	reg_write(ispbase, ISP_REG_RGB_TO_YUV_COVERSION2, 0x0000001D);	// ICCONV_2
	reg_write(ispbase, ISP_REG_RGB_TO_YUV_COVERSION3, 0x000001DA);	// ICCONV_3
	reg_write(ispbase, ISP_REG_RGB_TO_YUV_COVERSION4, 0x000001B6);	// ICCONV_4
	reg_write(ispbase, ISP_REG_RGB_TO_YUV_COVERSION5, 0x00000070);	// ICCONV_5
	reg_write(ispbase, ISP_REG_RGB_TO_YUV_COVERSION6, 0x0000009D);	// ICCONV_6
	reg_write(ispbase, ISP_REG_RGB_TO_YUV_COVERSION7, 0x0000017C);	// ICCONV_7
	reg_write(ispbase, ISP_REG_RGB_TO_YUV_COVERSION8, 0x000001E6);	// ICCONV_8

	reg_write(ispbase, ISP_REG_CIS_MODULE_CFG, 0x00000000);
	reg_write(ispbase, ISP_REG_ISP_CTRL_1, 0x10000022);	//0x30000022);//	
	reg_write(ispbase, ISP_REG_DC_AXI_ID, 0x00000000);
	reg_write(ispbase, 0x00000008, 0x00010005);//this reg can not be found in document
}

void isp_reset(struct stf_vin_dev *vin)
{
	void __iomem *ispbase;
	u32 isp_enable = ISP_NO_SCALE_ENABLE | ISP_MULTI_FRAME_ENABLE;
	if(vin->isp0)
		ispbase = vin->isp_isp0_base;
	else if(vin->isp1)
	    ispbase = vin->isp_isp1_base;
	else
		return;

	reg_write(ispbase, ISP_REG_ISP_CTRL_0, (isp_enable | ISP_RESET) /*0x00120002 */ );	// isp_rst = [1]
	reg_write(ispbase, ISP_REG_ISP_CTRL_0, isp_enable /*0x00120000 */ );	// isp_rst = [1]
}

void isp_enable(struct stf_vin_dev *vin)
{
	u32 isp_enable = ISP_NO_SCALE_ENABLE | ISP_MULTI_FRAME_ENABLE;
	void __iomem *ispbase;
    
	if(vin->isp0)
		ispbase = vin->isp_isp0_base;
	else if(vin->isp1)
	    ispbase = vin->isp_isp1_base;
	else
		return;

	reg_write(ispbase, ISP_REG_ISP_CTRL_0, (isp_enable | ISP_ENBALE) /*0x00120001 */ );	// isp_en = [0]
	reg_write(ispbase, 0x00000008, 0x00010004);	// CSI immed shadow update
	reg_write(ispbase, ISP_REG_CSI_INPUT_EN_AND_STATUS, 0x00000001);	// csi_en = [0]
}

void isp_dvp_2ndframe_config(struct stf_vin_dev *vin)
{
	void __iomem *ispbase;
	if(vin->isp0)
		ispbase = vin->isp_isp0_base;
	else if(vin->isp1)
	    ispbase = vin->isp_isp1_base;
	else
		return;

	reg_write(ispbase, ISP_REG_Y_PLANE_START_ADDR, FB_SECOND_ADDR);	// Unscaled Output Image Y Plane Start Address Register
	reg_write(ispbase, ISP_REG_UV_PLANE_START_ADDR, FB_SECOND_ADDR+vin->frame.width*vin->frame.height);	// Unscaled Output Image UV Plane Start Address Register
	reg_write(ispbase, ISP_REG_STRIDE, vin->frame.width);	// Unscaled Output Image Stride Register
}

void isp_clk_set(struct stf_vin_dev *vin)
{
	if (vin->isp0) {
		/* enable isp0 clk */
		reg_write(vin->clkgen_base, CLK_ISP0_CTRL, 0x80000002);	//ISP0-CLK
		reg_write(vin->clkgen_base, CLK_ISP0_2X_CTRL, 0x80000000);
		reg_write(vin->clkgen_base, CLK_ISP0_MIPI_CTRL, 0x80000000);
		reg_write(vin->clkgen_base, CLK_MIPI_RX0_PXL_CTRL, 0x00000008);	//0x00000010);colorbar

		if (vin->format.format == SRC_COLORBAR_VIN_ISP)
			reg_write(vin->clkgen_base, CLK_C_ISP0_CTRL, 0x00000000);
		else
			reg_write(vin->clkgen_base, CLK_C_ISP0_CTRL, 0x02000000);
	}

	if (vin->isp1) {
		/* enable isp1 clk */
		reg_write(vin->clkgen_base, CLK_ISP1_CTRL, 0x80000002);	//ISP1-CLK
		reg_write(vin->clkgen_base, CLK_ISP1_2X_CTRL, 0x80000000);
		reg_write(vin->clkgen_base, CLK_ISP1_MIPI_CTRL, 0x80000000);
		reg_write(vin->clkgen_base, CLK_MIPI_RX1_PXL_CTRL, 0x00000008);	//0x00000010);colorbar

		if (vin->format.format == SRC_COLORBAR_VIN_ISP)
			reg_write(vin->clkgen_base, CLK_C_ISP1_CTRL, 0x00000000);
		else
			reg_write(vin->clkgen_base, CLK_C_ISP1_CTRL, 0x02000000);
	}
}
EXPORT_SYMBOL(isp_clk_set);

void isp_ddr_config(struct stf_vin_dev *vin)
{
	isp_ddr_format_config(vin);
	isp_ddr_resolution_config(vin);
	/* reset isp */
	isp_reset(vin);
	/* enable isp */
	isp_enable(vin);
	if(SRC_DVP_SENSOR_VIN_ISP == vin->format.format)
		isp_dvp_2ndframe_config(vin);
}
EXPORT_SYMBOL(isp_ddr_config);

#define REGVALLIST(list) (list), sizeof(list)/sizeof((list)[0])

typedef struct
{
	u32 addr;
	u32 val;
} regval_t;

static const regval_t isp_800_480_reg_config_list[] = {
	{0x00000014, 0x0000000D},
	{0x00000018, 0x000011BB},
	{0x00000A1C, 0x00000032},
	{0x0000001C, 0x00000000},
	{0x00000020, 0x01df031f},
	{0x00000A0C, 0x01e00320},
	{0x00000A80, 0xF9000000},
	{0x00000A84, 0xF905DC00},
	{0x00000A88, 0x00000320},
	{0x00000A8C, 0x00000000},
	{0x00000A90, 0x00000000},
	{0x00000E40, 0x0000004C},
	{0x00000E44, 0x00000097},
	{0x00000E48, 0x0000001D},
	{0x00000E4C, 0x000001D5},
	{0x00000E50, 0x000001AC},
	{0x00000E54, 0x00000080},
	{0x00000E58, 0x00000080},
	{0x00000E5C, 0x00000194},
	{0x00000E60, 0x000001EC},
	{0x00000280, 0x00000000},
	{0x00000284, 0x00000000},
	{0x00000288, 0x00000000},
	{0x0000028C, 0x00000000},
	{0x00000290, 0x00000000},
	{0x00000294, 0x00000000},
	{0x00000298, 0x00000000},
	{0x0000029C, 0x00000000},
	{0x000002A0, 0x00000000},
	{0x000002A4, 0x00000000},
	{0x000002A8, 0x00000000},
	{0x000002AC, 0x00000000},
	{0x000002B0, 0x00000000},
	{0x000002B4, 0x00000000},
	{0x000002B8, 0x00000000},
	{0x000002BC, 0x00000000},
	{0x000002C0, 0x00F000F0},
	{0x000002C4, 0x00F000F0},
	{0x000002C8, 0x00800080},
	{0x000002CC, 0x00800080},
	{0x000002D0, 0x00800080},
	{0x000002D4, 0x00800080},
	{0x000002D8, 0x00B000B0},
	{0x000002DC, 0x00B000B0},
	{0x00000E00, 0x24000000},
	{0x00000E04, 0x159500A5},
	{0x00000E08, 0x0F9900EE},
	{0x00000E0C, 0x0CE40127},
	{0x00000E10, 0x0B410157},
	{0x00000E14, 0x0A210181},
	{0x00000E18, 0x094B01A8},
	{0x00000E1C, 0x08A401CC},
	{0x00000E20, 0x081D01EE},
	{0x00000E24, 0x06B20263},
	{0x00000E28, 0x05D802C7},
	{0x00000E2C, 0x05420320},
	{0x00000E30, 0x04D30370},
	{0x00000E34, 0x047C03BB},
	{0x00000E38, 0x043703FF},
	{0x00000010, 0x00000080},
	{0x00000A08, 0x10000032},
	{0x00000A00, 0x00120002},
	{0x00000A00, 0x00120000},
	{0x00000A50, 0x00000002},
	{0x00000A00, 0x00120001},
	{0x00000008, 0x00010000},
	{0x00000008, 0x0002000A},
	{0x00000000, 0x00000001},
};

static const regval_t isp_1080p_reg_config_list[] = {
	{0x00000014, 0x0000000D},
	{0x00000018, 0x000011BB},
	{0x00000A1C, 0x00000032},
	{0x0000001C, 0x00000000},
	{0x00000020, 0x0437077F},
	{0x00000A0C, 0x04380780},
	{0x00000A80, 0xF9000000},
	{0x00000A84, 0xF91FA400},
	{0x00000A88, 0x00000780},
	{0x00000A8C, 0x00000000},
	{0x00000A90, 0x00000000},
	{0x00000E40, 0x0000004C},
	{0x00000E44, 0x00000097},
	{0x00000E48, 0x0000001D},
	{0x00000E4C, 0x000001D5},
	{0x00000E50, 0x000001AC},
	{0x00000E54, 0x00000080},
	{0x00000E58, 0x00000080},
	{0x00000E5C, 0x00000194},
	{0x00000E60, 0x000001EC},
	{0x00000280, 0x00000000},
	{0x00000284, 0x00000000},
	{0x00000288, 0x00000000},
	{0x0000028C, 0x00000000},
	{0x00000290, 0x00000000},
	{0x00000294, 0x00000000},
	{0x00000298, 0x00000000},
	{0x0000029C, 0x00000000},
	{0x000002A0, 0x00000000},
	{0x000002A4, 0x00000000},
	{0x000002A8, 0x00000000},
	{0x000002AC, 0x00000000},
	{0x000002B0, 0x00000000},
	{0x000002B4, 0x00000000},
	{0x000002B8, 0x00000000},
	{0x000002BC, 0x00000000},
	{0x000002C0, 0x00F000F0},
	{0x000002C4, 0x00F000F0},
	{0x000002C8, 0x00800080},
	{0x000002CC, 0x00800080},
	{0x000002D0, 0x00800080},
	{0x000002D4, 0x00800080},
	{0x000002D8, 0x00B000B0},
	{0x000002DC, 0x00B000B0},
	{0x00000E00, 0x24000000},
	{0x00000E04, 0x159500A5},
	{0x00000E08, 0x0F9900EE},
	{0x00000E0C, 0x0CE40127},
	{0x00000E10, 0x0B410157},
	{0x00000E14, 0x0A210181},
	{0x00000E18, 0x094B01A8},
	{0x00000E1C, 0x08A401CC},
	{0x00000E20, 0x081D01EE},
	{0x00000E24, 0x06B20263},
	{0x00000E28, 0x05D802C7},
	{0x00000E2C, 0x05420320},
	{0x00000E30, 0x04D30370},
	{0x00000E34, 0x047C03BB},
	{0x00000E38, 0x043703FF},
	{0x00000010, 0x00000080},
	{0x00000A08, 0x10000032},
	{0x00000A00, 0x00120002},
	{0x00000A00, 0x00120000},
	{0x00000A50, 0x00000002},
	{0x00000A00, 0x00120001},
	{0x00000008, 0x00010000},
	{0x00000008, 0x0002000A},
	{0x00000000, 0x00000001},
};

const struct {
	const regval_t * regval;
	int regval_num;
} isp_1920_1080_settings[] = {
	{REGVALLIST(isp_1080p_reg_config_list)},
};

const struct {
	const regval_t * regval;
	int regval_num;
} isp_800_480_settings[] = {
	{REGVALLIST(isp_800_480_reg_config_list)},
};

void isp_config(struct stf_vin_dev *vin,int isp_id)
{
	void __iomem *ispbase;
	int j;
    u32 mipi_vc = 0;
	u32 mipi_channel_sel,vin_src_chan_sel;
	u32 y_start = vin->buf.paddr;
	u32 uv_start = y_start + vin->frame.width*vin->frame.height;
    ISP_INFO("  y_start 0x%08x, uv_start 0x%08x\n", y_start, uv_start); 
	ISP_INFO("  config isp %d <-- mipi %d:\n", isp_id, vin->csi_fmt.mipi_id);
    ISP_INFO("  h_size %d, v_size %d\n", vin->frame.width, vin->frame.height);
	if(vin->isp0)
		ispbase = vin->isp_isp0_base;
	else if(vin->isp1)
	    ispbase = vin->isp_isp1_base;
	else
		return;

    if(vin->csi_fmt.mipi_id==0)
    	reg_write(vin->clkgen_base, CLK_MIPI_RX0_PXL_CTRL, 0x3);
	else
        reg_write(vin->clkgen_base, CLK_MIPI_RX1_PXL_CTRL, 0x3);
	
    if(isp_id==0){
	    reg_write(vin->clkgen_base, CLK_ISP0_MIPI_CTRL, 0x80000000|(vin->csi_fmt.mipi_id<<24));
	    reg_write(vin->clkgen_base, CLK_C_ISP0_CTRL, vin->csi_fmt.mipi_id<<24);
    }
	else{
	    reg_write(vin->clkgen_base, CLK_ISP1_MIPI_CTRL, 0x80000000|(vin->csi_fmt.mipi_id<<24));
	    reg_write(vin->clkgen_base, CLK_C_ISP1_CTRL, vin->csi_fmt.mipi_id<<24);
    }
    mipi_channel_sel = vin->csi_fmt.mipi_id*4+mipi_vc;
    vin_src_chan_sel = reg_read(vin->sysctrl_base, SYSCTRL_VIN_SRC_CHAN_SEL);
	
    if (isp_id == 0) {
        vin_src_chan_sel &= ~0xf;
        vin_src_chan_sel |= mipi_channel_sel & 0xf;
    } else {
        vin_src_chan_sel &= ~0xf0;
        vin_src_chan_sel |= (mipi_channel_sel & 0xf) << 4;
    }
    reg_write(vin->sysctrl_base, SYSCTRL_VIN_SRC_CHAN_SEL, vin_src_chan_sel);

    // vin padding mipi output from raw10 to raw12
    if (vin->csi_fmt.dt == DT_RAW10) {
        uint32_t vin_src_dw_sel = reg_read(vin->sysctrl_base, SYSCTRL_VIN_SRC_DW_SEL);
        vin_src_dw_sel |= 1<<(isp_id==0?4:5);
        reg_write(vin->sysctrl_base, SYSCTRL_VIN_SRC_DW_SEL, vin_src_dw_sel);
    }

	if (VD_WIDTH_1080P == vin->frame.width) {
		for (j = 0; j < isp_1920_1080_settings[0].regval_num; j++) {
			reg_write(ispbase,
				  isp_1920_1080_settings[0].regval[j].addr,
				  isp_1920_1080_settings[0].regval[j].val);
			mdelay(5);
		}
	} else if (SEEED_WIDTH_800 == vin->frame.width) {
		for (j = 0; j < isp_800_480_settings[0].regval_num; j++) {
			reg_write(ispbase,
				  isp_800_480_settings[0].regval[j].addr,
				  isp_800_480_settings[0].regval[j].val);
			mdelay(5);
		}
	}
}
EXPORT_SYMBOL(isp_config);

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("loadable ISP driver for StarFive");
MODULE_LICENSE("GPL");
