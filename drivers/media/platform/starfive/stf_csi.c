/* /drivers/media/platform/starfive/stf_csi.c
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
#include "stf_csi.h"

static inline u32 reg_read(void __iomem * base, u32 reg)
{
	return ioread32(base + reg);
}

static inline void reg_write(void __iomem * base, u32 reg, u32 val)
{
	iowrite32(val, base + reg);
}

static void reg_set_highest_bit(void __iomem * base, u32 reg)
{
    u32 val;
	val = ioread32(base + reg);
	val &= ~(0x1 << 31);
	val |= (0x1 & 0x1) << 31;
	iowrite32(val, base + reg);
}

/*
static void reg_clear_highest_bit(void __iomem * base, u32 reg)
{
    u32 val;
	val = ioread32(base + reg);
	val &= ~(0x1 << 31);
	val |= (0x0 & 0x1) << 31;
	iowrite32(val, base + reg);
}
*/
int csi2rx_dphy_config(struct stf_vin_dev *vin,const csi2rx_dphy_cfg_t *cfg)
{
    union dphy_lane_swap dphy_lane_swap = {
        .bits = {
            .rx_1c2c_sel        = cfg->clane_nb - 1, // 0 - 1clk, 1 - 2clks
            .lane_swap_clk      = cfg->clane_map[0], //mipi-rx0 bind to clk0
            .lane_swap_clk1     = cfg->clane_map[1], //mipi-rx1 bind to clk1
            .lane_swap_lan0     = cfg->dlane_map[0],
            .lane_swap_lan1     = cfg->dlane_map[1],
            .lane_swap_lan2     = cfg->dlane_map[2],
            .lane_swap_lan3     = cfg->dlane_map[3],
            .dpdn_swap_clk      = cfg->clane_pn_swap[0],
            .dpdn_swap_clk1     = cfg->clane_pn_swap[1],
            .dpdn_swap_lan0     = cfg->dlane_pn_swap[0],
            .dpdn_swap_lan1     = cfg->dlane_pn_swap[1],
            .dpdn_swap_lan2     = cfg->dlane_pn_swap[2],
            .dpdn_swap_lan3     = cfg->dlane_pn_swap[3],
            .hs_freq_chang_clk0 = 0,
            .hs_freq_chang_clk1 = 0,
            .reserved           = 0,
        }
    };
		
	union dphy_lane_en dphy_lane_en = {
        .bits = {
            .gpio_en          = 0,
            .mp_test_mode_sel = 0,
            .mp_test_en       = 0,
            .dphy_enable_lan0 = cfg->dlane_en[0],
            .dphy_enable_lan1 = cfg->dlane_en[1],
            .dphy_enable_lan2 = cfg->dlane_en[2],
            .dphy_enable_lan3 = cfg->dlane_en[3],
            .rsvd_0           = 0,
        }
    };
    // evb, ov4689, c0-0,d0-1,d1-2,d2-3,d3-4,c1-5
    //write_reg(ISP_SYSCONTROLLER_BASE_ADDR, 0x10, 0x000468d0);
    reg_write(vin->sysctrl_base, SYSCTRL_REG4, dphy_lane_swap.raw);    
    reg_write(vin->sysctrl_base, SYSCTRL_DPHY_CTRL, dphy_lane_en.raw); 
    reg_write(vin->clkgen_base, CLK_DPHY_CFGCLK_ISPCORE_2X_CTRL, 0x80000008);
    reg_write(vin->clkgen_base, CLK_DPHY_REFCLK_ISPCORE_2X_CTRL, 0x80000010);
    reg_write(vin->clkgen_base, CLK_DPHY_TXCLKESC_IN_CTRL, 0x80000028);

    return 0;
}
EXPORT_SYMBOL(csi2rx_dphy_config);

static int csi2rx_reset(struct stf_vin_dev *vin,int id)
{
	reg_set_highest_bit(vin->clkgen_base,CLK_CSI2RX0_APB_CTRL);

    if (id == 0) {
		reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX0_PXL_0_CTRL);
		reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX0_PXL_1_CTRL);
	    reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX0_PXL_2_CTRL);
	    reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX0_PXL_3_CTRL);
	    reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX0_SYS0_CTRL);
    } else {
		reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX1_PXL_0_CTRL);
		reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX1_PXL_1_CTRL);
	    reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX1_PXL_2_CTRL);
	    reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX1_PXL_3_CTRL);
	    reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX1_SYS1_CTRL);
    }

    return 0;
}

static void csi2rx_debug_config(void *reg_base, u32 frame_lines)
{
    // data_id, ecc, crc error to irq
    union error_bypass_cfg err_bypass_cfg = {
        .data_id = 0,
        .ecc     = 0,
        .crc     = 0,
    };
    union stream_monitor_ctrl stream0_monitor_ctrl = {
        .frame_length = frame_lines,
        .frame_mon_en = 1,
        .frame_mon_vc = 0,
        .lb_en = 1,
        .lb_vc = 0,
    };
    reg_write(reg_base, ERROR_BYPASS_CFG     , err_bypass_cfg.value);
    reg_write(reg_base, MONITOR_IRQS_MASK_CFG, 0xffffffff);
    reg_write(reg_base, INFO_IRQS_MASK_CFG   , 0xffffffff);
    reg_write(reg_base, ERROR_IRQS_MASK_CFG  , 0xffffffff);
    reg_write(reg_base, DPHY_ERR_IRQ_MASK_CFG, 0xffffffff);


    reg_write(reg_base, STREAM0_MONITOR_CTRL, stream0_monitor_ctrl.value);
    reg_write(reg_base, STREAM0_FCC_CTRL, (0x0<<1)|0x1);  //Frame Capture Counter enable, vc0
}

int csi2rx_config(struct stf_vin_dev *vin,int id, const csi2rx_cfg_t *cfg)
{
    int s_stream0_fifo_mode = 0;
    int s_stream0_fifo_fill = 0;
	
    union static_config config;
    union dphy_lane_ctrl dphy_lane_ctrl;
    union stream_cfg stream0_cfg = {
        .fifo_fill      = 0, // fifo depth is 2048 pixels
        .bpp_bypass     = 0,
        .fifo_mode      = 0, // 0 full line; 1 large buffer
        .num_pixels     = 0, // 0 - 1 pixel per clock (default), 1 - 2 pixel
        .ls_le_mode     = 0,
        .interface_mode = 0,
    };

    void *reg_base = NULL;
    if(id == 0)
        reg_base= vin->mipi0_base;
    else if (id == 1) {
        reg_base= vin->mipi1_base;
    }else {
        return 0;
    }
    
    csi2rx_reset(vin,id);

    // 0x08 STATIC_CFG
    config.raw = 0;
    config.bits.lane_nb = cfg->lane_nb;
    config.bits.dl0_map = cfg->dlane_map[0];
    config.bits.dl1_map = cfg->dlane_map[1];
    config.bits.dl2_map = cfg->dlane_map[2];
    config.bits.dl3_map = cfg->dlane_map[3];
	
    reg_write(reg_base, STATIC_CFG, config.raw);

    // 0x40 DPHY_LANE_CONTROL
    dphy_lane_ctrl.raw = 0;
    dphy_lane_ctrl.bits.dl0_en = dphy_lane_ctrl.bits.dl0_reset = (cfg->lane_nb > 0);
    dphy_lane_ctrl.bits.dl1_en = dphy_lane_ctrl.bits.dl1_reset = (cfg->lane_nb > 1);
    dphy_lane_ctrl.bits.dl2_en = dphy_lane_ctrl.bits.dl2_reset = (cfg->lane_nb > 2);
    dphy_lane_ctrl.bits.dl3_en = dphy_lane_ctrl.bits.dl3_reset = (cfg->lane_nb > 3);
    dphy_lane_ctrl.bits.cl_en = dphy_lane_ctrl.bits.cl_reset = 1;
	
    reg_write(reg_base, DPHY_LANE_CONTROL, dphy_lane_ctrl.raw);

    csi2rx_debug_config(reg_base, cfg->vsize);

    reg_write(reg_base, STREAM0_DATA_CFG, 0x00000080|(cfg->dt&0x3f));  // all vc, dt0 enabled

    stream0_cfg.fifo_mode = s_stream0_fifo_mode & 0x3;
    stream0_cfg.fifo_fill = (stream0_cfg.fifo_mode == 1) ? s_stream0_fifo_fill : 0;
    reg_write(reg_base, STREAM0_CFG, stream0_cfg.value);
    reg_write(reg_base, STREAM0_CTRL, 0x00000010);      // soft_rst
    mdelay(100);
    reg_write(reg_base, STREAM0_CTRL, 0x00000001);      // start

    return 0;
}
EXPORT_SYMBOL(csi2rx_config);

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("loadable CSI driver for StarFive");
MODULE_LICENSE("GPL");
