/* driver/video/starfive/starfive_mipi_tx.c
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License version 2 as
** published by the Free Software Foundation.
**
** Copyright (C) 2021 StarFive, Inc.
**
** PURPOSE:	This files contains the driver of LCD controller.
**
** CHANGE HISTORY:
**	Version		Date		Author		Description
**	0.1.0		2021-01-06	starfive		created
**
*/

#include <linux/module.h>
#include <stdarg.h>
#include <linux/delay.h>

#include "starfive_comm_regs.h"
#include "starfive_mipi_tx.h"

//#define SF_MIPITX_DEBUG	1
#ifdef SF_MIPITX_DEBUG
	#define MIPITX_PRT(format, args...)  printk(KERN_DEBUG "[MIPITX]: " format, ## args)
	#define MIPITX_INFO(format, args...) printk(KERN_INFO "[MIPITX]: " format, ## args)
	#define MIPITX_ERR(format, args...)	printk(KERN_ERR "[MIPITX]: " format, ## args)
#else
	#define MIPITX_PRT(x...)  do{} while(0)
	#define MIPITX_INFO(x...)  do{} while(0)
	#define MIPITX_ERR(x...)  do{} while(0)
#endif

static u32 sf_fb_dsitxread32(struct sf_fb_data *sf_dev, u32 reg)
{
	return ioread32(sf_dev->base_dsitx + reg);
}

static void sf_fb_dsitxwrite32(struct sf_fb_data *sf_dev, u32 reg, u32 val)
{
	iowrite32(val, sf_dev->base_dsitx + reg);
}

static void dcs_start(struct sf_fb_data *sf_dev, u32 cmd_head, u32 cmd_size, u32 cmd_nat)
{
	u32 main_settings;
    u32 cmd_long = (cmd_head == CMD_HEAD_WRITE_N);

	sf_fb_dsitxwrite32(sf_dev, DIRECT_CMD_STAT_CLR_ADDR, 0xffffffff);
	sf_fb_dsitxwrite32(sf_dev, DIRECT_CMD_STAT_CTRL_ADDR, 0xffffffff);

    main_settings = (0<<25)     //trigger_val
                        |(1<<24)    //cmd_lp_en
                        |((cmd_size&0xff)<<16)
                        |(0<<14)    //cmd_id
                        |((cmd_head&0x3f)<<8)
                        |((cmd_long&0x1)<<3)
                        |(cmd_nat&0x7);

	sf_fb_dsitxwrite32(sf_dev, DIRECT_CMD_MAINSET_ADDR, main_settings);
}

static void dcs_write_32(struct sf_fb_data *sf_dev, u32 val)
{
	sf_fb_dsitxwrite32(sf_dev, DIRECT_CMD_WRDAT_ADDR, val);
}

static void dcs_wait_finish(struct sf_fb_data *sf_dev, u32 exp_sts_mask, u32 exp_sts)
{
    u32 stat = 0;
    int timeout = 100;
	int stat_88;
	int stat_188;
	int stat_88_ack_val;

	sf_fb_dsitxwrite32(sf_dev, DIRECT_CMD_SEND_ADDR, 0);

    do {
        stat = sf_fb_dsitxread32(sf_dev, DIRECT_CMD_STAT_ADDR);
        if ((stat & exp_sts_mask) == exp_sts) {
            break;
        }
        mdelay(10);
    } while (--timeout);
    if (!timeout) {
        printk("timeout!\n");
    }

    stat_88 = sf_fb_dsitxread32(sf_dev, DIRECT_CMD_STAT_ADDR);
    stat_188 = sf_fb_dsitxread32(sf_dev, PHY_ERR_FLAG_ADDR);
    stat_88_ack_val = stat_88 >> 16;
    if (stat_188 || stat_88_ack_val) {
        MIPITX_PRT("stat: [188h] %08x, [88h] %08x\r\n", stat_188, stat_88);
    }
}

static void mipi_tx_lxn_set(struct sf_fb_data *sf_dev, u32 reg, u32 n_hstx, u32 p_hstx)
{
	u32 temp = 0;

	temp = n_hstx;
	temp |= p_hstx << 5;
	sf_fb_cfgwrite32(sf_dev, reg, temp);
}

static void dsi_csi2tx_sel(struct sf_fb_data *sf_dev, int sel)
{
  u32 temp = 0;

  temp = sf_fb_cfgread32(sf_dev, SCFG_DSI_CSI_SEL);
  temp &= ~(0x1);
  temp |= (sel & 0x1);
  sf_fb_cfgwrite32(sf_dev, SCFG_DSI_CSI_SEL, temp);
}

static void dphy_clane_hs_txready_sel(struct sf_fb_data *sf_dev, u32 ready_sel)
{
	sf_fb_cfgwrite32(sf_dev, SCFG_TXREADY_SRC_SEL_D, ready_sel);
	sf_fb_cfgwrite32(sf_dev, SCFG_TXREADY_SRC_SEL_C, ready_sel);
	sf_fb_cfgwrite32(sf_dev, SCFG_HS_PRE_ZERO_T_D, 0x30);
	sf_fb_cfgwrite32(sf_dev, SCFG_HS_PRE_ZERO_T_C, 0x30);
	MIPITX_PRT("DPHY ppi_c_hs_tx_ready from source %d\n", ready_sel);
}

static void dphy_config(struct sf_fb_data *sf_dev, int bit_rate)
{
	int pre_div,      fbk_int,       extd_cycle_sel;
	int dhs_pre_time, dhs_zero_time, dhs_trial_time;
	int chs_pre_time, chs_zero_time, chs_trial_time;
	int chs_clk_pre_time, chs_clk_post_time;
	u32 set_val = 0;

	mipi_tx_lxn_set(sf_dev, SCFG_L0N_L0P_HSTX, 0x10, 0x10);
	mipi_tx_lxn_set(sf_dev, SCFG_L1N_L1P_HSTX, 0x10, 0x10);
	mipi_tx_lxn_set(sf_dev, SCFG_L2N_L2P_HSTX, 0x10, 0x10);
	mipi_tx_lxn_set(sf_dev, SCFG_L3N_L3P_HSTX, 0x10, 0x10);
	mipi_tx_lxn_set(sf_dev, SCFG_L4N_L4P_HSTX, 0x10, 0x10);

	if(bit_rate == 80) {
		pre_div=0x1,		fbk_int=2*0x33,		extd_cycle_sel=0x4, 
		dhs_pre_time=0xe,	dhs_zero_time=0x1d,	dhs_trial_time=0x15,
		chs_pre_time=0x5,	chs_zero_time=0x2b,	chs_trial_time=0xd, 
		chs_clk_pre_time=0xf,
		chs_clk_post_time=0x71;
	} else if (bit_rate == 100) {
		pre_div=0x1,		fbk_int=2*0x40,		extd_cycle_sel=0x4,
		dhs_pre_time=0x10,	dhs_zero_time=0x21,	dhs_trial_time=0x17,
		chs_pre_time=0x7,	chs_zero_time=0x35,	chs_trial_time=0xf,
		chs_clk_pre_time=0xf,
		chs_clk_post_time=0x73;
	} else if (bit_rate == 200) {
		pre_div=0x1,		fbk_int=2*0x40,		extd_cycle_sel=0x3;
		dhs_pre_time=0xc,	dhs_zero_time=0x1b,	dhs_trial_time=0x13;
		chs_pre_time=0x7,	chs_zero_time=0x35,	chs_trial_time=0xf,
		chs_clk_pre_time=0x7,
		chs_clk_post_time=0x3f;
	} else if(bit_rate == 300) {
		pre_div=0x1,		fbk_int=2*0x60, 	extd_cycle_sel=0x3,
		dhs_pre_time=0x11,	dhs_zero_time=0x25, dhs_trial_time=0x19,
		chs_pre_time=0xa, 	chs_zero_time=0x50, chs_trial_time=0x15,
		chs_clk_pre_time=0x7,
		chs_clk_post_time=0x45;
    } else if(bit_rate == 400) {
		pre_div=0x1,      	fbk_int=2*0x40,		extd_cycle_sel=0x2,
		dhs_pre_time=0xa, 	dhs_zero_time=0x18,	dhs_trial_time=0x11,
		chs_pre_time=0x7, 	chs_zero_time=0x35, chs_trial_time=0xf,
		chs_clk_pre_time=0x3,
		chs_clk_post_time=0x25;
    } else if(bit_rate == 500 ) {
		pre_div=0x1,      fbk_int=2*0x50,       extd_cycle_sel=0x2,
		dhs_pre_time=0xc, dhs_zero_time=0x1d,	dhs_trial_time=0x14,
		chs_pre_time=0x9, chs_zero_time=0x42,	chs_trial_time=0x12,
		chs_clk_pre_time=0x3,
		chs_clk_post_time=0x28;
    } else if(bit_rate == 600 ) {
		pre_div=0x1,      fbk_int=2*0x60,       extd_cycle_sel=0x2,
		dhs_pre_time=0xe, dhs_zero_time=0x23,	dhs_trial_time=0x17,
		chs_pre_time=0xa, chs_zero_time=0x50,	chs_trial_time=0x15,
		chs_clk_pre_time=0x3,
		chs_clk_post_time=0x2b;
    } else if(bit_rate == 700) {
		pre_div=0x1,      fbk_int=2*0x38,       extd_cycle_sel=0x1,
		dhs_pre_time=0x8, dhs_zero_time=0x14,	dhs_trial_time=0xf,
		chs_pre_time=0x6, chs_zero_time=0x2f,	chs_trial_time=0xe,
		chs_clk_pre_time=0x1,
		chs_clk_post_time=0x16;
    } else if(bit_rate == 800 ) {
		pre_div=0x1,      fbk_int=2*0x40,       extd_cycle_sel=0x1,
		dhs_pre_time=0x9, dhs_zero_time=0x17,	dhs_trial_time=0x10,
		chs_pre_time=0x7, chs_zero_time=0x35,	chs_trial_time=0xf,
		chs_clk_pre_time=0x1,
		chs_clk_post_time=0x18;
    } else if(bit_rate == 900 ) {
		pre_div=0x1,      fbk_int=2*0x48,       extd_cycle_sel=0x1,
		dhs_pre_time=0xa, dhs_zero_time=0x19, 	dhs_trial_time=0x12,
		chs_pre_time=0x8, chs_zero_time=0x3c, 	chs_trial_time=0x10,
		chs_clk_pre_time=0x1,
		chs_clk_post_time=0x19;
    } else if(bit_rate == 1000) {
		pre_div=0x1,      fbk_int=2*0x50,       extd_cycle_sel=0x1,
		dhs_pre_time=0xb, dhs_zero_time=0x1c,	dhs_trial_time=0x13,
		chs_pre_time=0x9, chs_zero_time=0x42,	chs_trial_time=0x12,
		chs_clk_pre_time=0x1,
		chs_clk_post_time=0x1b;
    } else if(bit_rate == 1100) {
		pre_div=0x1,      fbk_int=2*0x58,       extd_cycle_sel=0x1,
		dhs_pre_time=0xc, dhs_zero_time=0x1e,	dhs_trial_time=0x15,
		chs_pre_time=0x9, chs_zero_time=0x4a,	chs_trial_time=0x14,
		chs_clk_pre_time=0x1,
		chs_clk_post_time=0x1d;
    } else if(bit_rate == 1200) {
		pre_div=0x1,      fbk_int=2*0x60,       extd_cycle_sel=0x1,
		dhs_pre_time=0xe, dhs_zero_time=0x20,	dhs_trial_time=0x16,
		chs_pre_time=0xa, chs_zero_time=0x50,	chs_trial_time=0x15,
		chs_clk_pre_time=0x1,
		chs_clk_post_time=0x1e;
    } else if(bit_rate == 1300) {
		pre_div=0x1,      fbk_int=2*0x34,       extd_cycle_sel=0x0,
		dhs_pre_time=0x7, dhs_zero_time=0x12,	dhs_trial_time=0xd,
		chs_pre_time=0x5, chs_zero_time=0x2c,	chs_trial_time=0xd,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0xf;
    } else if(bit_rate == 1400) {
		pre_div=0x1,      fbk_int=2*0x38,       extd_cycle_sel=0x0,
		dhs_pre_time=0x7, dhs_zero_time=0x14,	dhs_trial_time=0xe,
		chs_pre_time=0x6, chs_zero_time=0x2f,	chs_trial_time=0xe,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x10;
    } else if(bit_rate == 1500) {
		pre_div=0x1,      fbk_int=2*0x3c,       extd_cycle_sel=0x0,
		dhs_pre_time=0x8, dhs_zero_time=0x14,	dhs_trial_time=0xf,
		chs_pre_time=0x6, chs_zero_time=0x32,	chs_trial_time=0xe,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x11;
    } else if(bit_rate == 1600) {
		pre_div=0x1,      fbk_int=2*0x40,       extd_cycle_sel=0x0,
		dhs_pre_time=0x9, dhs_zero_time=0x15,	dhs_trial_time=0x10,
		chs_pre_time=0x7, chs_zero_time=0x35,	chs_trial_time=0xf,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x12;
    } else if(bit_rate == 1700) {
		pre_div=0x1,      fbk_int=2*0x44,       extd_cycle_sel=0x0,
		dhs_pre_time=0x9, dhs_zero_time=0x17,	dhs_trial_time=0x10,
		chs_pre_time=0x7, chs_zero_time=0x39,	chs_trial_time=0x10,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x12;
    } else if(bit_rate == 1800) {
		pre_div=0x1,      fbk_int=2*0x48,       extd_cycle_sel=0x0,
		dhs_pre_time=0xa, dhs_zero_time=0x18,	dhs_trial_time=0x11,
		chs_pre_time=0x8, chs_zero_time=0x3c,	chs_trial_time=0x10,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x13;
    } else if(bit_rate == 1900) {
		pre_div=0x1,      fbk_int=2*0x4c,       extd_cycle_sel=0x0,
		dhs_pre_time=0xa, dhs_zero_time=0x1a,	dhs_trial_time=0x12,
		chs_pre_time=0x8, chs_zero_time=0x3f,	chs_trial_time=0x11,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x14;
    } else if(bit_rate == 2000) {
		pre_div=0x1,      fbk_int=2*0x50,       extd_cycle_sel=0x0,
		dhs_pre_time=0xb, dhs_zero_time=0x1b,	dhs_trial_time=0x13,
		chs_pre_time=0x9, chs_zero_time=0x42,	chs_trial_time=0x12,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x15;
    } else if(bit_rate == 2100) {
		pre_div=0x1,      fbk_int=2*0x54,       extd_cycle_sel=0x0,
		dhs_pre_time=0xb, dhs_zero_time=0x1c,	dhs_trial_time=0x13,
		chs_pre_time=0x9, chs_zero_time=0x46,	chs_trial_time=0x13,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x15;
    } else if(bit_rate == 2200) {
		pre_div=0x1,      fbk_int=2*0x5b,       extd_cycle_sel=0x0,
		dhs_pre_time=0xc, dhs_zero_time=0x1d,	dhs_trial_time=0x14,
		chs_pre_time=0x9, chs_zero_time=0x4a,	chs_trial_time=0x14,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x16;
    } else if(bit_rate == 2300) {
		pre_div=0x1,      fbk_int=2*0x5c,       extd_cycle_sel=0x0,
		dhs_pre_time=0xc, dhs_zero_time=0x1f,	dhs_trial_time=0x15,
		chs_pre_time=0xa, chs_zero_time=0x4c,	chs_trial_time=0x14,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x17;
    } else if(bit_rate == 2400) {
		pre_div=0x1,      fbk_int=2*0x60,       extd_cycle_sel=0x0,
		dhs_pre_time=0xd, dhs_zero_time=0x20,	dhs_trial_time=0x16,
		chs_pre_time=0xa, chs_zero_time=0x50,	chs_trial_time=0x15,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x18;
    } else if(bit_rate == 2500) {
		pre_div=0x1,      fbk_int=2*0x64,       extd_cycle_sel=0x0,
		dhs_pre_time=0xe, dhs_zero_time=0x21,	dhs_trial_time=0x16,
		chs_pre_time=0xb, chs_zero_time=0x53,	chs_trial_time=0x16,
		chs_clk_pre_time=0x0,
		chs_clk_post_time=0x18;
    } else {
		//default bit_rate == 700
		pre_div=0x1,      fbk_int=2*0x38,       extd_cycle_sel=0x1,
		dhs_pre_time=0x8, dhs_zero_time=0x14,	dhs_trial_time=0xf,
		chs_pre_time=0x6, chs_zero_time=0x2f,	chs_trial_time=0xe,
		chs_clk_pre_time=0x1,
		chs_clk_post_time=0x16;
	 	MIPITX_ERR(" ERROR: invalid bit rate configuration!\n");
    }
	sf_fb_cfgwrite32(sf_dev, SCFG_REFCLK_SEL, 0x3);

	set_val = 0
			| (1 << OFFSET_CFG_L1_SWAP_SEL)
			| (4 << OFFSET_CFG_L2_SWAP_SEL)
			| (2 << OFFSET_CFG_L3_SWAP_SEL)
			| (3 << OFFSET_CFG_L4_SWAP_SEL);
	sf_fb_cfgwrite32(sf_dev, SCFG_LX_SWAP_SEL, set_val);

	set_val = 0
			| (0 << OFFSET_SCFG_PWRON_READY_N)
			| (1 << OFFSET_RG_CDTX_PLL_FM_EN)
			| (0 << OFFSET_SCFG_PLLSSC_EN)
			| (1 << OFFSET_RG_CDTX_PLL_LDO_STB_X2_EN);
	sf_fb_cfgwrite32(sf_dev, SCFG_DBUS_PW_PLL_SSC_LD0, set_val);

	set_val = fbk_int
			| (pre_div << 9);
	sf_fb_cfgwrite32(sf_dev, SCFG_RG_CDTX_PLL_FBK_PRE, set_val);

	sf_fb_cfgwrite32(sf_dev, SCFG_RG_EXTD_CYCLE_SEL, extd_cycle_sel);

	set_val = chs_zero_time
			| (dhs_pre_time << OFFSET_DHS_PRE_TIME)
			| (dhs_trial_time << OFFSET_DHS_TRIAL_TIME)
			| (dhs_zero_time << OFFSET_DHS_ZERO_TIME);
	sf_fb_cfgwrite32(sf_dev, SCFG_RG_CLANE_DLANE_TIME, set_val);

	set_val = chs_clk_post_time
			| (chs_clk_pre_time << OFFSET_CHS_PRE_TIME)
			| (chs_pre_time << OFFSET_CHS_TRIAL_TIME)
			| (chs_trial_time << OFFSET_CHS_ZERO_TIME);
	sf_fb_cfgwrite32(sf_dev, SCFG_RG_CLANE_HS_TIME, set_val);

}

void reset_dphy(struct sf_fb_data *sf_dev, int resetb)
{
	u32 cfg_link_enable = 0x01;//bit0
    u32 cfg_ck2_ck3_ck_enable = 0x07;//bit0-3
    u32 cfg_ck1_dat_enable = 0x1f<<3;//bit3-7
    u32 cfg_dsc_enable = 0x01;//bit0
	u32 precfg = sf_fb_dsitxread32(sf_dev, VID_MCTL_MAIN_EN) & ~cfg_ck1_dat_enable;
	sf_fb_dsitxwrite32(sf_dev, VID_MCTL_MAIN_EN, precfg|cfg_ck1_dat_enable);

	precfg = sf_fb_dsitxread32(sf_dev, VID_MCTL_MAIN_PHY_CTL) & ~cfg_ck2_ck3_ck_enable;
	sf_fb_dsitxwrite32(sf_dev, VID_MCTL_MAIN_PHY_CTL, precfg|cfg_ck2_ck3_ck_enable);

	precfg = sf_fb_dsitxread32(sf_dev, VID_MCTL_MAIN_DATA_CTL) & ~cfg_link_enable;
	sf_fb_dsitxwrite32(sf_dev, VID_MCTL_MAIN_DATA_CTL, precfg|cfg_link_enable);

	precfg = sf_fb_cfgread32(sf_dev, SCFG_PHY_RESETB); 
	precfg &= ~(cfg_dsc_enable);
	precfg |= (resetb&cfg_dsc_enable);
	sf_fb_cfgwrite32(sf_dev, SCFG_PHY_RESETB, precfg);
}

void polling_dphy_lock(struct sf_fb_data *sf_dev)
{
	int pll_unlock;

	udelay(10);

	do {
		pll_unlock = sf_fb_cfgread32(sf_dev, SCFG_GRS_CDTX_PLL) >> 3;
		pll_unlock &= 0x1;
		MIPITX_PRT("%s check\n",__func__, __LINE__);
	} while(pll_unlock == 0x1);
	//udelay(10);
}

static int dsitx_phy_config(struct sf_fb_data *sf_dev)
{
	uint32_t bit_rate = sf_dev->panel_info.dphy_bps/1000000UL;//(1920 * 1080 * bpp / dlanes * fps / 1000000 + 99) / 100 * 100;

	dphy_config(sf_dev, bit_rate);
	reset_dphy(sf_dev, 1);
	mdelay(10);
	polling_dphy_lock(sf_dev);

	return 0;
}

void release_txbyte_rst(struct sf_fb_data *sf_dev)
{
	u32 temp = sf_fb_rstread32(sf_dev, SRST_ASSERT0);
	temp &= ~(0x1<<18);
	temp |= (0x0&0x1)<<18;
	sf_fb_rstwrite32(sf_dev, SRST_ASSERT0, temp);

	do {
		temp = sf_fb_rstread32(sf_dev, SRST_STATUS0) >> 18;
		temp &= 0x1;
		MIPITX_PRT("%s check\n",__func__, __LINE__);
	} while (temp != 0x1 );
	//udelay(1);
	MIPITX_PRT("Tx byte reset released for csi2tx and dsitx\n");
}

void vid_size_cfg_update(struct sf_fb_data *sf_dev)
{
	int vcfg1 = sf_dev->panel_info.dsi_vsa | (sf_dev->panel_info.dsi_vbp<<6) | (sf_dev->panel_info.dsi_vfp<<12);

	int hsa_len = (sf_dev->panel_info.dsi_sync_pulse==0) ? 0 : sf_dev->panel_info.dsi_hsa-14;
	int hbp_len = (sf_dev->panel_info.dsi_sync_pulse==0) ?  sf_dev->panel_info.dsi_hsa+sf_dev->panel_info.dsi_hbp-12 : sf_dev->panel_info.dsi_hbp-12;

	int hact_len = sf_dev->panel_info.w * sf_dev->panel_info.bpp/8;
	int hfp_len =(sf_dev->panel_info.dsi_burst_mode) ? 0x0 : (sf_dev->panel_info.dsi_hfp-6);

	int hcfg1 = hsa_len|(hbp_len<<16);
	int hcfg2 = hact_len|(hfp_len<<16);

	hbp_len = sf_dev->panel_info.dsi_burst_mode ? (sf_dev->panel_info.dphy_lanes * (sf_dev->panel_info.dsi_hsa + sf_dev->panel_info.dsi_hbp)) - 12 + 300 : hbp_len;

	sf_fb_dsitxwrite32(sf_dev, VID_VSIZE1_ADDR, vcfg1);
	sf_fb_dsitxwrite32(sf_dev, VID_VSIZE2_ADDR, sf_dev->panel_info.h);
	MIPITX_PRT("DSI VSA: %d, VBP: %d, VFP: %d, VACT: %d\n", sf_dev->panel_info.dsi_vsa, sf_dev->panel_info.dsi_vbp, sf_dev->panel_info.dsi_vfp, sf_dev->panel_info.h);

	sf_fb_dsitxwrite32(sf_dev, VID_HSIZE1_ADDR, hcfg1);
	sf_fb_dsitxwrite32(sf_dev, VID_HSIZE2_ADDR, hcfg2);
	MIPITX_PRT("DSI HSA: %d, HBP: %d, HFP: %d, HACT: %d\n", hsa_len, hbp_len, hfp_len, hact_len);

	sf_fb_dsitxwrite32(sf_dev, VID_ERR_COLOR1_ADDR, (0xcc<<12)|0xaa);
	sf_fb_dsitxwrite32(sf_dev, VID_ERR_COLOR2_ADDR, (0xee<<12)|0x55);
}

static int div_roundup(int dived, int divsor)
{
	int q = ((dived-1)/divsor)+1;
	return q;
}

void vid_blktime_cfg_update(struct sf_fb_data *sf_dev)
{
     int hsa_dsi = (sf_dev->panel_info.dsi_sync_pulse==0) ? 0 : sf_dev->panel_info.dsi_hsa- 14;
     //int hbp_dsi = (pulse_event==0) ? (hsa+hbp)-12 : hbp - 12;
     //int hact_dsi = hact;
     //int hfp_dsi = (burst_en) ? 0 : hfp-6;
     int hline_bytes = (sf_dev->panel_info.dsi_hsa + sf_dev->panel_info.dsi_hbp + sf_dev->panel_info.w * sf_dev->panel_info.bpp/8 + sf_dev->panel_info.dsi_hfp);
     int total_line = div_roundup(hline_bytes, sf_dev->panel_info.dphy_lanes);

     int blkline_pulse_pck = hline_bytes - 20 - hsa_dsi;
     int pulse_reg_dura = total_line - div_roundup(hsa_dsi+14,sf_dev->panel_info.dphy_lanes);  

     int blkline_event_pck0 = hline_bytes - 10;
     int event_reg_dura = total_line - div_roundup(8, sf_dev->panel_info.dphy_lanes);

     int burst_blkline_pck = hline_bytes * 2 - 4;
     int burst_reg_dura = total_line * 2 - div_roundup(4, sf_dev->panel_info.dphy_lanes);

     //int burst_hbp = 2 * (hsa+hbp)- 12 + fifo_fill;
     int blkeol_pck = burst_blkline_pck - (sf_dev->panel_info.w * sf_dev->panel_info.bpp/8 + 6 + sf_dev->panel_info.dsi_hbp + 6);
     //int txbyte_cycles = div_roundup((burst_hbp + hact_dsi), nlane);
     int blkeol_dura = div_roundup(blkeol_pck + 6, sf_dev->panel_info.dphy_lanes);

     int blkline_event_pck = (sf_dev->panel_info.dsi_burst_mode) ? burst_blkline_pck : blkline_event_pck0;
     int reg_line_duration = (sf_dev->panel_info.dsi_burst_mode) ? burst_reg_dura :
                             (sf_dev->panel_info.dsi_sync_pulse==0) ? event_reg_dura : pulse_reg_dura;

	int reg_wakeup_time = DPHY_REG_WAKEUP_TIME<<17;
	int dphy_time = reg_line_duration | reg_wakeup_time;

	int max_line = (sf_dev->panel_info.dsi_sync_pulse) ? (blkline_pulse_pck-6)<<16 : (blkline_event_pck-6)<<16 ;
	int exact_burst = blkeol_pck;

	sf_fb_dsitxwrite32(sf_dev, VID_BLKSIZE1_ADDR, (blkeol_pck<<15)|blkline_event_pck);
	MIPITX_PRT("DSI blkeol_pck: %d, blkline_event_pck: %d\n", blkeol_pck, blkline_event_pck);

	sf_fb_dsitxwrite32(sf_dev, VID_BLKSIZE2_ADDR, blkline_pulse_pck);
	MIPITX_PRT("DSI blkline_pulse_pck: %d\n", blkline_pulse_pck);

	sf_fb_dsitxwrite32(sf_dev, VID_PCK_TIME_ADDR, blkeol_dura);
	MIPITX_PRT("DSI blkeol_duration: %d\n", blkeol_dura);

	sf_fb_dsitxwrite32(sf_dev, VID_DPHY_TIME_ADDR, dphy_time);
	MIPITX_PRT("DSI reg line duration: %d, wakeup time: %d\n", reg_line_duration, reg_wakeup_time>>17); 

	sf_fb_dsitxwrite32(sf_dev, VID_VCA_SET2_ADDR, max_line|exact_burst);
	MIPITX_PRT("DSI max_line: %d, max_burst: %d\n", max_line>>16, exact_burst);
}

void dsi_main_cfg(struct sf_fb_data *sf_dev)
{
	//PHY Main control
	int hs_continous = 0x01;
	int te = 0x00;
	int cmdEn = 0x00;
	int cont = hs_continous<<4;
	int lanen = 0xF>>(4-(sf_dev->panel_info.dphy_lanes-1));
	int write_burst = 0x3c<<8;
	int main_phy_cfg = cont | lanen | write_burst;
	int tvg_en = 0;

	int lane_en = (0x1F>>(4-sf_dev->panel_info.dphy_lanes))<<3;
	int dpi_en = 0x1<<14;
	int main_en_cfg = lane_en | dpi_en | 0x1;

	//main data ctrl, 0x4
	int te_en = (te<<12) | (te<<8) | (te<<24);
	int bta = cmdEn<<14;
	int rden = cmdEn<<13;
	int tvg = tvg_en<<6;
	int vid_en = 0x1<<5;
	int vid_if_sel = 0x1<<2;
	int sdi_mode = 0x1<<1;
	int link_en = 0x1;
	int interface = 0x0;//0x3<<1;
	int main_cfg = link_en | interface | sdi_mode | vid_if_sel| vid_en | tvg | rden | bta | te_en;

	//VID Main Ctrl, 0xb0
	int idle_miss_vsync=0x1<<31;
	int recovery_mode = 0x1<<25;
	int h_sync_pulse = sf_dev->panel_info.dsi_sync_pulse<<20;
	int sync_active = sf_dev->panel_info.dsi_sync_pulse<<19;
	int burst_mode = sf_dev->panel_info.dsi_burst_mode<<18;
	int pix_mode = 0x3<<14;
	int header = 0x3e<<8;
	int cfg = header|pix_mode|burst_mode|sync_active|h_sync_pulse|recovery_mode|idle_miss_vsync;

	//TVG main ctrl
	int strp_size = 0x7<<5;
	int tvg_md = TVG_MODE<<3;
	int tvg_stop = 0x1<<1;
	int start_tvg = tvg_en;
	int tvg_cfg = start_tvg | tvg_stop | tvg_md | strp_size;

	sf_fb_dsitxwrite32(sf_dev, PHY_TIMEOUT1_ADDR, 0xafffb);
	sf_fb_dsitxwrite32(sf_dev, PHY_TIMEOUT2_ADDR, 0x3ffff);
	sf_fb_dsitxwrite32(sf_dev, ULPOUT_TIME_ADDR, 0x3ab05);

	sf_fb_dsitxwrite32(sf_dev, MAIN_PHY_CTRL_ADDR, main_phy_cfg);
	sf_fb_dsitxwrite32(sf_dev, MAIN_EN_ADDR, main_en_cfg);

	sf_fb_dsitxwrite32(sf_dev, MAIN_DATA_CTRL_ADDR, main_cfg);
	sf_fb_dsitxwrite32(sf_dev, VID_MAIN_CTRL_ADDR, cfg);
	sf_fb_dsitxwrite32(sf_dev, DIRECT_CMD_STAT_CTRL_ADDR, 0x80);
	sf_fb_dsitxwrite32(sf_dev, TVG_CTRL_ADDR, tvg_cfg);
	MIPITX_PRT("DSI TVG main ctrl 0xfc: 0x%x\n", tvg_cfg);
}

int dsitx_dcs_write(struct sf_fb_data *sf_dev, int cmd_size, ...)
{
    int ret = 0;
	u32 exp_sts_mask = 0x2; // [1]write complete
    u32 exp_sts = 0x2;
    // transfer the sequence
    int i;
    struct dcs_buffer wbuf;
	va_list ap;

    // dcs cmd config
    int cmd_head = (cmd_size < 2 ? CMD_HEAD_WRITE_0 :
                        (cmd_size < 3 ? CMD_HEAD_WRITE_1 :
                            CMD_HEAD_WRITE_N));
    dcs_start(sf_dev, cmd_head, cmd_size, CMD_NAT_WRITE);

    wbuf.len = 0;
    wbuf.val32 = 0;
    va_start(ap, cmd_size);
    for (i = 0; i < cmd_size; i++) {
        wbuf.val8[wbuf.len++] = (char)va_arg(ap, int);
        if (((i + 1) & 0x3) == 0) {
            dcs_write_32(sf_dev, wbuf.val32);
            wbuf.len = 0;
            wbuf.val32 = 0;
        }
    }
    if (i & 0x3) {
        dcs_write_32(sf_dev, wbuf.val32);
        wbuf.len = 0;
        wbuf.val32 = 0;
    }
    va_end(ap);

    // wait transfer complete
    dcs_wait_finish(sf_dev, exp_sts_mask, exp_sts);

    return ret;
}


static int seeed_panel_send_cmd(struct sf_fb_data *sf_dev, u16 reg, u32 val)
{
    u8 msg[] = {
        reg,
        reg >> 8,
        val,
        val >> 8,
        val >> 16,
        val >> 24,
    };

    dsitx_dcs_write(sf_dev, 6, msg[0], msg[1], msg[2], msg[3], msg[4], msg[5]);

    return 0;
}

static int seeed_panel_enable(struct sf_fb_data *sf_dev)
{
    seeed_panel_send_cmd(sf_dev, DSI_LANEENABLE,
                  DSI_LANEENABLE_CLOCK |
                  DSI_LANEENABLE_D0);
    seeed_panel_send_cmd(sf_dev, PPI_D0S_CLRSIPOCOUNT, 0x05);
    seeed_panel_send_cmd(sf_dev, PPI_D1S_CLRSIPOCOUNT, 0x05);
    seeed_panel_send_cmd(sf_dev, PPI_D0S_ATMR, 0x00);
    seeed_panel_send_cmd(sf_dev, PPI_D1S_ATMR, 0x00);
    seeed_panel_send_cmd(sf_dev, PPI_LPTXTIMECNT, 0x03);

    seeed_panel_send_cmd(sf_dev, SPICMR, 0x00);
    seeed_panel_send_cmd(sf_dev, LCDCTRL, 0x00100150);
    seeed_panel_send_cmd(sf_dev, SYSCTRL, 0x040f);
    mdelay(100);

    seeed_panel_send_cmd(sf_dev, PPI_STARTPPI, 0x01);
    seeed_panel_send_cmd(sf_dev, DSI_STARTDSI, 0x01);
    mdelay(100);

    return 0;
}

void dpi_cfg(struct sf_fb_data *sf_dev, int int_en) {
  sf_fb_dsitxwrite32(sf_dev, DPI_IRQ_EN_ADDR, int_en);
}

int sf_mipi_init(struct sf_fb_data *sf_dev)
{
    int ret = 0;
	uint32_t dpi_fifo_int = 0;

    dsi_csi2tx_sel(sf_dev, DSI_CONN_LCDC);
    dphy_clane_hs_txready_sel(sf_dev, 0x1);

    dsitx_phy_config(sf_dev);
    release_txbyte_rst(sf_dev);
    mdelay(100);

	dpi_fifo_int = sf_fb_dsitxread32(sf_dev, DPI_IRQ_CLR_ADDR);
    if (dpi_fifo_int) {
		sf_fb_dsitxwrite32(sf_dev, DPI_IRQ_CLR_ADDR, 1);
    }

    vid_size_cfg_update(sf_dev);

    vid_blktime_cfg_update(sf_dev);

    dsi_main_cfg(sf_dev);

    mdelay(100);

	seeed_panel_enable(sf_dev);
    dpi_cfg(sf_dev, 1);

    return ret;
}

EXPORT_SYMBOL(sf_mipi_init);

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("loadable MIPI Tx driver for StarFive");
MODULE_LICENSE("GPL");
