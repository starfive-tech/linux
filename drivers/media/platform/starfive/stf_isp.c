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
	switch (vin->format.format) {
	case SRC_COLORBAR_VIN_ISP:
	    reg_write(vin->base_isp, ISP_REG_DVP_POLARITY_CFG, 0xd);
		break;

	case SRC_DVP_SENSOR_VIN_ISP:
	    reg_write(vin->base_isp, ISP_REG_DVP_POLARITY_CFG, 0x08);
		break;

	default:
		pr_err("unknown format\n");
		return;
	}
	
	reg_write(vin->base_isp, ISP_REG_RAW_FORMAT_CFG, 0x000011BB);	// sym_order = [0+:16]
	reg_write(vin->base_isp, ISP_REG_CFA_MODE, 0x00000030);
	reg_write(vin->base_isp, ISP_REG_PIC_CAPTURE_START_CFG, 0x00000000); // cro_hstart = [0+:16], cro_vstart = [16+:16]
}

void isp_ddr_resolution_config(struct stf_vin_dev *vin)
{
	u32 val = 0;
	val = (vin->frame.width-1) + ((vin->frame.height-1)<<16);
	
	reg_write(vin->base_isp, ISP_REG_PIC_CAPTURE_END_CFG, val);	// cro_hend = [0+:16], cro_vend = [16+:16]
	val = (vin->frame.width) + ((vin->frame.height)<<16);
	reg_write(vin->base_isp, ISP_REG_PIPELINE_XY_SIZE, val);	// ISP pipeline width[0+:16], height[16+:16]

	reg_write(vin->base_isp, ISP_REG_Y_PLANE_START_ADDR, FB_FIRST_ADDR);	// Unscaled Output Image Y Plane Start Address Register
	reg_write(vin->base_isp, ISP_REG_UV_PLANE_START_ADDR, FB_FIRST_ADDR+(vin->frame.width*vin->frame.height));	// Unscaled Output Image UV Plane Start Address Register

	reg_write(vin->base_isp, ISP_REG_STRIDE, vin->frame.width);	// Unscaled Output Image Stride Register

	reg_write(vin->base_isp, ISP_REG_PIXEL_COORDINATE_GEN, 0x00000010);	// Unscaled Output Pixel Coordinate Generator Mode Register
	reg_write(vin->base_isp, ISP_REG_PIXEL_AXI_CONTROL, 0x00000000);	
	reg_write(vin->base_isp, ISP_REG_SS_AXI_CONTROL, 0x00000000);
	
	reg_write(vin->base_isp, ISP_REG_RGB_TO_YUV_COVERSION0, 0x0000004D);	// ICCONV_0
	reg_write(vin->base_isp, ISP_REG_RGB_TO_YUV_COVERSION1, 0x00000096);	// ICCONV_1
	reg_write(vin->base_isp, ISP_REG_RGB_TO_YUV_COVERSION2, 0x0000001D);	// ICCONV_2
	reg_write(vin->base_isp, ISP_REG_RGB_TO_YUV_COVERSION3, 0x000001DA);	// ICCONV_3
	reg_write(vin->base_isp, ISP_REG_RGB_TO_YUV_COVERSION4, 0x000001B6);	// ICCONV_4
	reg_write(vin->base_isp, ISP_REG_RGB_TO_YUV_COVERSION5, 0x00000070);	// ICCONV_5
	reg_write(vin->base_isp, ISP_REG_RGB_TO_YUV_COVERSION6, 0x0000009D);	// ICCONV_6
	reg_write(vin->base_isp, ISP_REG_RGB_TO_YUV_COVERSION7, 0x0000017C);	// ICCONV_7
	reg_write(vin->base_isp, ISP_REG_RGB_TO_YUV_COVERSION8, 0x000001E6);	// ICCONV_8
	
	reg_write(vin->base_isp, ISP_REG_CIS_MODULE_CFG, 0x00000000);	
	reg_write(vin->base_isp, ISP_REG_ISP_CTRL_1, 0x10000022);	//0x30000022);//	
	reg_write(vin->base_isp, ISP_REG_DC_AXI_ID, 0x00000000);
	reg_write(vin->base_isp, 0x00000008, 0x00010005);//this reg can not be found in document
}

void isp_reset(struct stf_vin_dev *vin)
{
	u32 isp_enable = ISP_NO_SCALE_ENABLE | ISP_MULTI_FRAME_ENABLE;
	reg_write(vin->base_isp, ISP_REG_ISP_CTRL_0, (isp_enable | ISP_RESET) /*0x00120002 */ );	// isp_rst = [1]
	reg_write(vin->base_isp, ISP_REG_ISP_CTRL_0, isp_enable /*0x00120000 */ );	// isp_rst = [1]
}

void isp_enable(struct stf_vin_dev *vin)
{
	u32 isp_enable = ISP_NO_SCALE_ENABLE | ISP_MULTI_FRAME_ENABLE;

	reg_write(vin->base_isp, ISP_REG_ISP_CTRL_0, (isp_enable | ISP_ENBALE) /*0x00120001 */ );	// isp_en = [0]
	reg_write(vin->base_isp, 0x00000008, 0x00010004);	// CSI immed shadow update
	reg_write(vin->base_isp, ISP_REG_CSI_INPUT_EN_AND_STATUS, 0x00000001);	// csi_en = [0]
}

void isp_dvp_2ndframe_config(struct stf_vin_dev *vin)
{
	reg_write(vin->base_isp, ISP_REG_Y_PLANE_START_ADDR, FB_SECOND_ADDR);	// Unscaled Output Image Y Plane Start Address Register
	reg_write(vin->base_isp, ISP_REG_UV_PLANE_START_ADDR, FB_SECOND_ADDR+vin->frame.width*vin->frame.height);	// Unscaled Output Image UV Plane Start Address Register
	reg_write(vin->base_isp, ISP_REG_STRIDE, vin->frame.width);	// Unscaled Output Image Stride Register
}

void isp_clk_set(struct stf_vin_dev *vin)
{
	void __iomem *clkgen_base = vin->base + VIN_CLKGEN_OFFSET;

	if (vin->isp0) {
		/* enable isp0 clk */
		reg_write(clkgen_base, CLK_ISP0_CTRL, 0x80000002);	//ISP0-CLK
		reg_write(clkgen_base, CLK_ISP0_2X_CTRL, 0x80000000);
		reg_write(clkgen_base, CLK_ISP0_MIPI_CTRL, 0x80000000);

		reg_write(clkgen_base, CLK_MIPI_RX0_PXL_CTRL, 0x00000008);	//0x00000010);colorbar

		if (vin->format.format == SRC_COLORBAR_VIN_ISP)
			reg_write(clkgen_base, CLK_C_ISP0_CTRL, 0x00000000);
		else
			reg_write(clkgen_base, CLK_C_ISP0_CTRL, 0x02000000);
	}

	if (vin->isp1) {
		/* enable isp1 clk */
		reg_write(clkgen_base, CLK_ISP1_CTRL, 0x80000002);	//ISP1-CLK
		reg_write(clkgen_base, CLK_ISP1_2X_CTRL, 0x80000000);
		reg_write(clkgen_base, CLK_ISP1_MIPI_CTRL, 0x80000000);

		reg_write(clkgen_base, CLK_MIPI_RX1_PXL_CTRL, 0x00000008);	//0x00000010);colorbar

		if (vin->format.format == SRC_COLORBAR_VIN_ISP)
			reg_write(clkgen_base, CLK_C_ISP1_CTRL, 0x00000000);
		else
			reg_write(clkgen_base, CLK_C_ISP1_CTRL, 0x02000000);
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

void isp_base_addr_config(struct stf_vin_dev *vin)
{
	if(vin->isp0)
		vin->base_isp = ioremap(VIN_ISP0_BASE_ADDR, 0x30000);
	else if(vin->isp1)
		vin->base_isp = ioremap(VIN_ISP1_BASE_ADDR, 0x30000);
    else{
		return;
    }
}
EXPORT_SYMBOL(isp_base_addr_config);

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("loadable ISP driver for StarFive");
MODULE_LICENSE("GPL");
