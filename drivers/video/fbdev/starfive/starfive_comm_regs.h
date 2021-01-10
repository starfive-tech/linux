/*
 * StarFive Vout driver
 *
 * Copyright 2020 StarFive Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef __SF_COMM_REGS_H__
#define __SF_COMM_REGS_H__

#include "starfive_fb.h"

//syscfg registers
#define SCFG_DSI_CSI_SEL	        0x2c
#define SCFG_PHY_RESETB	            0x30
#define SCFG_REFCLK_SEL	            0x34
#define SCFG_DBUS_PW_PLL_SSC_LD0	0x38
#define SCFG_GRS_CDTX_PLL       	0x3c

#define SCFG_RG_CDTX_PLL_FBK_PRE	0x44
#define SCFG_RG_CLANE_DLANE_TIME   	0x58
#define SCFG_RG_CLANE_HS_TIME   	0x58

#define SCFG_RG_EXTD_CYCLE_SEL   	0x5c

#define SCFG_L0N_L0P_HSTX	        0x60
#define SCFG_L1N_L1P_HSTX	        0x64
#define SCFG_L2N_L2P_HSTX	        0x68
#define SCFG_L3N_L3P_HSTX	        0x6c
#define SCFG_L4N_L4P_HSTX	        0x70
#define SCFG_LX_SWAP_SEL	        0x78

#define SCFG_HS_PRE_ZERO_T_D	    0xc4
#define SCFG_TXREADY_SRC_SEL_D	    0xc8
#define SCFG_HS_PRE_ZERO_T_C	    0xd4
#define SCFG_TXREADY_SRC_SEL_C	    0xd8

//reg SCFG_LX_SWAP_SEL 
#define	OFFSET_CFG_L0_SWAP_SEL 	0
#define	OFFSET_CFG_L1_SWAP_SEL 	3
#define	OFFSET_CFG_L2_SWAP_SEL 	6
#define	OFFSET_CFG_L3_SWAP_SEL 	9 
#define OFFSET_CFG_L4_SWAP_SEL 	12

//reg SCFG_DBUS_PW_PLL_SSC_LD0
#define OFFSET_SCFG_CFG_DATABUD16_SEL    0  
#define OFFSET_SCFG_PWRON_READY_N        1
#define OFFSET_RG_CDTX_PLL_FM_EN         2
#define OFFSET_SCFG_PLLSSC_EN            3
#define OFFSET_RG_CDTX_PLL_LDO_STB_X2_EN 4

//reg SCFG_RG_CLANE_DLANE_TIME
#define OFFSET_DHS_PRE_TIME          8  
#define OFFSET_DHS_TRIAL_TIME        16
#define OFFSET_DHS_ZERO_TIME         24

//reg SCFG_RG_CLANE_HS_TIME
#define OFFSET_CHS_PRE_TIME          8  
#define OFFSET_CHS_TRIAL_TIME        16
#define OFFSET_CHS_ZERO_TIME         24



//sysrst registers
#define SRST_ASSERT0	    0x00
#define SRST_STATUS0    	0x04
/* Definition controller bit for syd rst registers */
#define BIT_RST_DSI_DPI_PIX		17


static u32 sf_fb_cfgread32(struct sf_fb_data *sf_dev, u32 reg)
{
	return ioread32(sf_dev->base_syscfg + reg);
}

static inline void sf_fb_cfgwrite32(struct sf_fb_data *sf_dev, u32 reg, u32 val)
{
	iowrite32(val, sf_dev->base_syscfg + reg);
}

static inline u32 sf_fb_rstread32(struct sf_fb_data *sf_dev, u32 reg)
{
	return ioread32(sf_dev->base_rst + reg);
}

static void sf_fb_rstwrite32(struct sf_fb_data *sf_dev, u32 reg, u32 val)
{
	iowrite32(val, sf_dev->base_rst + reg);
}

#endif

