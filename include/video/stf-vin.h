/* include/video/stf-vin.h
 *
 * Copyright 2020 starfive tech.
 *	Eric Tang <eric.tang@starfivetech.com>
 *
 * Generic vin notifier interface
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/
#ifndef _VIDEO_VIN_H
#define _VIDEO_VIN_H

#include <linux/cdev.h>

#define DRV_NAME "stf-vin"
#define FB_FIRST_ADDR      0xf9000000
#define FB_SECOND_ADDR     0xf97e9000

#define VIN_MIPI_CONTROLLER0_OFFSET 0x00000
#define VIN_CLKGEN_OFFSET           0x10000
#define VIN_RSTGEN_OFFSET           0x20000
#define VIN_MIPI_CONTROLLER1_OFFSET 0x30000
#define VIN_SYSCONTROLLER_OFFSET    0x40000

#define VD_1080P    1080
#define VD_720P     720
#define VD_PAL      480

#define VD_HEIGHT_1080P     VD_1080P
#define VD_WIDTH_1080P      1920

#define VD_HEIGHT_720P      VD_720P
#define VD_WIDTH_720P       1080

#define VD_HEIGHT_480       480
#define VD_WIDTH_640        640

#define VIN_CLKGEN_BASE_ADDR	0x11800000
#define VIN_RSTGEN_BASE_ADDR	0x11840000
#define VIN_IOPAD_BASE_ADDR	    0x11858000

//vin clk registers
#define CLK_VIN_SRC_CTRL		    0x188
#define CLK_ISP0_AXI_CTRL		    0x190
#define CLK_ISP0NOC_AXI_CTRL	    0x194
#define CLK_ISPSLV_AXI_CTRL		    0x198
#define CLK_ISP1_AXI_CTRL		    0x1A0
#define CLK_ISP1NOC_AXI_CTRL	    0x1A4
#define CLK_VIN_AXI		            0x1AC
#define CLK_VINNOC_AXI		        0x1B0

//isp clk registers
#define CLK_MIPI_RX0_PXL_CTRL       0x0c
#define CLK_ISP0_CTRL               0x3c
#define CLK_ISP0_2X_CTRL            0x40
#define CLK_ISP0_MIPI_CTRL          0x44
#define CLK_C_ISP0_CTRL             0x64
#define CLK_ISP1_CTRL               0x48
#define CLK_ISP1_2X_CTRL            0x4C
#define CLK_ISP1_MIPI_CTRL          0x50
#define CLK_MIPI_RX1_PXL_CTRL       0x10
#define CLK_C_ISP1_CTRL             0x68
#define CLK_VIN_AXI_WR_CTRL         0x5C

#define SOFTWARE_RESET_ASSERT0		0x0
#define SOFTWARE_RESET_ASSERT1		0x4

#define IOPAD_REG81		        0x144
#define IOPAD_REG82		        0x148
#define IOPAD_REG83		        0x14C
#define IOPAD_REG84		        0x150
#define IOPAD_REG85		        0x154
#define IOPAD_REG86		        0x158
#define IOPAD_REG87	            0x15C
#define IOPAD_REG88	            0x160
#define IOPAD_REG89	            0x164

//sys control REG DEFINE
#define SYSCTRL_REG6	            0x18
#define SYSCTRL_REG10	            0x28
#define SYSCTRL_REG11	            0x2C
#define SYSCTRL_REG12	            0x30
#define SYSCTRL_REG13	            0x34
#define SYSCTRL_REG14	            0x38
#define SYSCTRL_REG15	            0x3C
#define SYSCTRL_REG17	            0x44
#define SYSCTRL_REG18	            0x48
#define SYSCTRL_REG19	            0x4C
#define SYSCTRL_REG20	            0x50
#define SYSCTRL_REG21	            0x54

#define ISP_NO_SCALE_ENABLE     (0x1<<20)
#define ISP_MULTI_FRAME_ENABLE  (0x1<<17)
#define ISP_SS0_ENABLE          (0x1<<11)
#define ISP_SS1_ENABLE          (0x1<<12)
#define ISP_RESET               (0x1<<1)
#define ISP_ENBALE              (0x1)

#define VIN_ISP0_BASE_ADDR	0x19870000
#define VIN_ISP1_BASE_ADDR	0x198a0000

 //ISP REG DEFINE
#define ISP_REG_DVP_POLARITY_CFG            0x00000014
#define ISP_REG_RAW_FORMAT_CFG              0x00000018
#define ISP_REG_CFA_MODE                    0x00000A1C
#define ISP_REG_PIC_CAPTURE_START_CFG       0x0000001C
#define ISP_REG_PIC_CAPTURE_END_CFG         0x00000020
#define ISP_REG_PIPELINE_XY_SIZE            0x00000A0C
#define ISP_REG_Y_PLANE_START_ADDR          0x00000A80
#define ISP_REG_UV_PLANE_START_ADDR         0x00000A84
#define ISP_REG_STRIDE                      0x00000A88
#define ISP_REG_PIXEL_COORDINATE_GEN        0x00000A8C
#define ISP_REG_PIXEL_AXI_CONTROL           0x00000A90
#define ISP_REG_SS_AXI_CONTROL              0x00000AC4
#define ISP_REG_RGB_TO_YUV_COVERSION0       0x00000E40
#define ISP_REG_RGB_TO_YUV_COVERSION1       0x00000E44
#define ISP_REG_RGB_TO_YUV_COVERSION2       0x00000E48
#define ISP_REG_RGB_TO_YUV_COVERSION3       0x00000E4C
#define ISP_REG_RGB_TO_YUV_COVERSION4       0x00000E50
#define ISP_REG_RGB_TO_YUV_COVERSION5       0x00000E54
#define ISP_REG_RGB_TO_YUV_COVERSION6       0x00000E58
#define ISP_REG_RGB_TO_YUV_COVERSION7       0x00000E5C
#define ISP_REG_RGB_TO_YUV_COVERSION8       0x00000E60
#define ISP_REG_CIS_MODULE_CFG              0x00000010
#define ISP_REG_ISP_CTRL_1                  0x00000A08
#define ISP_REG_ISP_CTRL_0                  0x00000A00
#define ISP_REG_DC_AXI_ID                   0x00000044
#define ISP_REG_CSI_INPUT_EN_AND_STATUS     0x00000000

enum VIN_SOURCE_FORMAT {
	SRC_COLORBAR_VIN_ISP = 0,
	SRC_DVP_SENSOR_VIN,
	SRC_DVP_SENSOR_VIN_ISP,//need replace sensor
	SRC_CSI2RX_VIN_ISP,
	SRC_DVP_SENSOR_VIN_OV5640,
};

struct vin_params {
	void *paddr;
	unsigned long size;
};

struct vin_buf {
	void *vaddr;
	dma_addr_t paddr;
	u32 size;
};

struct vin_framesize {
	u32 width;
	u32 height;
};

struct vin_format {
	enum VIN_SOURCE_FORMAT format;
	u8 fps;
};

struct stf_vin_dev {
	/* Protects the access of variables shared within the interrupt */
	spinlock_t irqlock;
	int irq;
	struct device *dev;
	struct cdev vin_cdev;
	void __iomem *base;
	void __iomem *base_isp;
	struct vin_framesize frame;
	struct vin_format format;
	bool isp0;
	bool isp1;
	int isp0_irq;
	int isp1_irq;
	u32 major;
	struct vin_buf buf;

	wait_queue_head_t wq;
	bool condition;
	int odd;
};

extern int vin_notifier_register(struct notifier_block *nb);
extern void vin_notifier_unregister(struct notifier_block *nb);
extern int vin_notifier_call(unsigned long e, void *v);
#endif
