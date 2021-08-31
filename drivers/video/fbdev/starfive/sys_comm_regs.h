/*
 * StarFive sys regs
 *
 * Copyright 2020 StarFive Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef __SYS_COMM_REGS_H__
#define __SYS_COMM_REGS_H__

#define MA_OUTW( io, val )  ({void __iomem * vir; vir = ioremap(io, 4); iowrite32((u32)val, vir);})
#define MA_INW( io )  ({void __iomem * vir; vir = ioremap(io, 4); ioread32(vir);})

#define WDT_BASE_ADDR					0x12480000

#define DSITX_BASE_ADDR                 0x12100000
#define CSI2TX_BASE_ADDR                0x12220000
#define ISP_MIPI_CONTROLLER0_BASE_ADDR  0x19800000
#define ISP_MIPI_CONTROLLER1_BASE_ADDR  0x19830000

#define VOUT_SYS_CLKGEN_BASE_ADDR  		0x12240000
#define VOUT_SYS_RSTGEN_BASE_ADDR  		0x12250000
#define VOUT_SYS_SYSCON_BASE_ADDR  		0x12260000

#define ISP_CLKGEN_BASE_ADDR        	0x19810000
#define ISP_RSTGEN_BASE_ADDR       		0x19820000
#define ISP_SYSCONTROLLER_BASE_ADDR 	0x19840000

#define ISP0_AXI_SLV_BASE_ADDR    	    0x19870000
#define ISP1_AXI_SLV_BASE_ADDR    	    0x198A0000

#define CLKGEN_BASE_ADDR                0x11800000      
#define RSTGEN_BASE_ADDR                0x11840000       
#define ISP_CLKGEN_BASE_ADDR        	0x19810000
#define ISP_RSTGEN_BASE_ADDR        	0x19820000
#define ISP_SYSCONTROLLER_BASE_ADDR 	0x19840000

#define GPIO_BASE_ADDR		            0x11910000
#define EZGPIO_FULLMUX_BASE_ADDR		0x11910000
#define SYSCON_IOPAD_CTRL_BASE_ADDR     0x11858000 

#define vout_sys_rstgen_Software_RESET_assert0_REG_ADDR  VOUT_SYS_RSTGEN_BASE_ADDR + 0x0
#define vout_sys_rstgen_Software_RESET_status0_REG_ADDR  VOUT_SYS_RSTGEN_BASE_ADDR + 0x4
#define rstgen_Software_RESET_assert1_REG_ADDR           RSTGEN_BASE_ADDR + 0x4
#define rstgen_Software_RESET_status1_REG_ADDR           RSTGEN_BASE_ADDR + 0x14

//#define VOUT_SYS_CLKGEN_BASE_ADDR 0x0
#define clk_vout_apb_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x0
#define clk_mapconv_apb_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x4
#define clk_mapconv_axi_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x8
#define clk_disp0_axi_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0xC
#define clk_disp1_axi_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x10
#define clk_lcdc_oclk_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x14
#define clk_lcdc_axi_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x18
#define clk_vpp0_axi_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x1C
#define clk_vpp1_axi_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x20
#define clk_vpp2_axi_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x24
#define clk_pixrawout_apb_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x28
#define clk_pixrawout_axi_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x2C
#define clk_csi2tx_strm0_pixclk_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x30
#define clk_csi2tx_strm0_apb_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x34
#define clk_dsi_sys_clk_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x38
#define clk_dsi_apb_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x3C
#define clk_ppi_tx_esc_clk_ctrl_REG_ADDR  VOUT_SYS_CLKGEN_BASE_ADDR + 0x40

//#define CLKGEN_BASE_ADDR 0x0
#define clk_cpundbus_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x0
#define clk_dla_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x4
#define clk_dsp_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x8
#define clk_gmacusb_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xC
#define clk_perh0_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x10
#define clk_perh1_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x14
#define clk_vin_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x18
#define clk_vout_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1C
#define clk_audio_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x20
#define clk_cdechifi4_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x24
#define clk_cdec_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x28
#define clk_voutbus_root_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2C
#define clk_cpunbus_root_div_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x30
#define clk_dsp_root_div_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x34
#define clk_perh0_src_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x38
#define clk_perh1_src_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x3C
#define clk_pll0_testout_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x40
#define clk_pll1_testout_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x44
#define clk_pll2_testout_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x48
#define clk_pll2_refclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x4C
#define clk_cpu_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x50
#define clk_cpu_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x54
#define clk_ahb_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x58
#define clk_apb1_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x5C
#define clk_apb2_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x60
#define clk_dom3ahb_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x64
#define clk_dom7ahb_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x68
#define clk_u74_core0_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x6C
#define clk_u74_core1_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x70
#define clk_u74_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x74
#define clk_u74rtc_toggle_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x78
#define clk_sgdma2p_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x7C
#define clk_dma2pnoc_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x80
#define clk_sgdma2p_ahb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x84
#define clk_dla_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x88
#define clk_dla_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x8C
#define clk_dlanoc_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x90
#define clk_dla_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x94
#define clk_vp6_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x98
#define clk_vp6bus_src_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x9C
#define clk_vp6_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xA0
#define clk_vcdecbus_src_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xA4
#define clk_vdec_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xA8
#define clk_vdec_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xAC
#define clk_vdecbrg_mainclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xB0
#define clk_vdec_bclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xB4
#define clk_vdec_cclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xB8
#define clk_vdec_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xBC
#define clk_jpeg_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xC0
#define clk_jpeg_cclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xC4
#define clk_jpeg_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xC8
#define clk_gc300_2x_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xCC
#define clk_gc300_ahb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xD0
#define clk_jpcgc300_axibus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xD4
#define clk_gc300_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xD8
#define clk_jpcgc300_mainclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xDC
#define clk_venc_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xE0
#define clk_venc_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xE4
#define clk_vencbrg_mainclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xE8
#define clk_venc_bclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xEC
#define clk_venc_cclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xF0
#define clk_venc_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xF4
#define clk_ddrpll_div2_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xF8
#define clk_ddrpll_div4_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0xFC
#define clk_ddrpll_div8_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x100
#define clk_ddrosc_div2_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x104
#define clk_ddrc0_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x108
#define clk_ddrc1_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x10C
#define clk_ddrphy_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x110
#define clk_noc_rob_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x114
#define clk_noc_cog_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x118
#define clk_nne_ahb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x11C
#define clk_nnebus_src1_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x120
#define clk_nne_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x124
#define clk_nne_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x128
#define clk_nnenoc_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x12C
#define clk_dlaslv_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x130
#define clk_dspx2c_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x134
#define clk_hifi4_src_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x138
#define clk_hifi4_corefree_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x13C
#define clk_hifi4_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x140
#define clk_hifi4_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x144
#define clk_hifi4_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x148
#define clk_hifi4noc_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x14C
#define clk_sgdma1p_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x150
#define clk_sgdma1p_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x154
#define clk_dma1p_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x158
#define clk_x2c_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x15C
#define clk_usb_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x160
#define clk_usb_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x164
#define clk_usbnoc_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x168
#define clk_usbphy_rootdiv_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x16C
#define clk_usbphy_125m_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x170
#define clk_usbphy_plldiv25m_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x174
#define clk_usbphy_25m_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x178
#define clk_audio_div_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x17C
#define clk_audio_src_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x180
#define clk_audio_12288_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x184
#define clk_vin_src_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x188
#define clk_isp0_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x18C
#define clk_isp0_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x190
#define clk_isp0noc_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x194
#define clk_ispslv_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x198
#define clk_isp1_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x19C
#define clk_isp1_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1A0
#define clk_isp1noc_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1A4
#define clk_vin_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1A8
#define clk_vin_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1AC
#define clk_vinnoc_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1B0
#define clk_vout_src_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1B4
#define clk_dispbus_src_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1B8
#define clk_disp_bus_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1BC
#define clk_disp_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1C0
#define clk_dispnoc_axi_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1C4
#define clk_sdio0_ahb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1C8
#define clk_sdio0_cclkint_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1CC
#define clk_sdio0_cclkint_inv_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1D0
#define clk_sdio1_ahb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1D4
#define clk_sdio1_cclkint_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1D8
#define clk_sdio1_cclkint_inv_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1DC
#define clk_gmac_ahb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1E0
#define clk_gmac_root_div_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1E4
#define clk_gmac_ptp_refclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1E8
#define clk_gmac_gtxclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1EC
#define clk_gmac_rmii_txclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1F0
#define clk_gmac_rmii_rxclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1F4
#define clk_gmac_tx_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1F8
#define clk_gmac_tx_inv_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x1FC
#define clk_gmac_rx_pre_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x200
#define clk_gmac_rx_inv_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x204
#define clk_gmac_rmii_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x208
#define clk_gmac_tophyref_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x20C
#define clk_spi2ahb_ahb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x210
#define clk_spi2ahb_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x214
#define clk_ezmaster_ahb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x218
#define clk_e24_ahb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x21C
#define clk_e24rtc_toggle_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x220
#define clk_qspi_ahb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x224
#define clk_qspi_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x228
#define clk_qspi_refclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x22C
#define clk_sec_ahb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x230
#define clk_aes_clk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x234
#define clk_sha_clk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x238
#define clk_pka_clk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x23C
#define clk_trng_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x240
#define clk_otp_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x244
#define clk_uart0_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x248
#define clk_uart0_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x24C
#define clk_uart1_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x250
#define clk_uart1_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x254
#define clk_spi0_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x258
#define clk_spi0_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x25C
#define clk_spi1_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x260
#define clk_spi1_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x264
#define clk_i2c0_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x268
#define clk_i2c0_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x26C
#define clk_i2c1_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x270
#define clk_i2c1_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x274
#define clk_gpio_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x278
#define clk_uart2_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x27C
#define clk_uart2_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x280
#define clk_uart3_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x284
#define clk_uart3_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x288
#define clk_spi2_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x28C
#define clk_spi2_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x290
#define clk_spi3_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x294
#define clk_spi3_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x298
#define clk_i2c2_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x29C
#define clk_i2c2_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2A0
#define clk_i2c3_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2A4
#define clk_i2c3_core_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2A8
#define clk_wdtimer_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2AC
#define clk_wdt_coreclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2B0
#define clk_timer0_coreclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2B4
#define clk_timer1_coreclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2B8
#define clk_timer2_coreclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2BC
#define clk_timer3_coreclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2C0
#define clk_timer4_coreclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2C4
#define clk_timer5_coreclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2C8
#define clk_timer6_coreclk_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2CC
#define clk_vp6intc_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2D0
#define clk_pwm_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2D4
#define clk_msi_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2D8
#define clk_temp_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2DC
#define clk_temp_sense_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2E0
#define clk_syserr_apb_ctrl_REG_ADDR  CLKGEN_BASE_ADDR + 0x2E4


#define _ENABLE_CLOCK_clk_disp0_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_disp0_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_disp0_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_disp1_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_disp1_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_disp1_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_lcdc_oclk_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_lcdc_oclk_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_lcdc_oclk_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_lcdc_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_lcdc_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_lcdc_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_vpp0_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_vpp0_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_vpp0_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}


#define _ENABLE_CLOCK_clk_vpp1_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_vpp1_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_vpp1_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_vpp2_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_vpp2_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_vpp2_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_vpp2_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_vpp2_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_vpp2_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_mapconv_apb_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_mapconv_apb_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_mapconv_apb_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_mapconv_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_mapconv_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_mapconv_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_pixrawout_apb_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_pixrawout_apb_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_pixrawout_apb_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_pixrawout_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_pixrawout_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_pixrawout_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_csi2tx_strm0_apb_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_csi2tx_strm0_apb_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_csi2tx_strm0_apb_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_csi2tx_strm0_pixclk_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_csi2tx_strm0_pixclk_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_csi2tx_strm0_pixclk_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_ppi_tx_esc_clk_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_ppi_tx_esc_clk_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_ppi_tx_esc_clk_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_dsi_apb_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_dsi_apb_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_dsi_apb_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_dsi_sys_clk_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_dsi_sys_clk_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_dsi_sys_clk_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_disp0_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<2); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<2; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>2; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_disp1_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<3); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<3; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>3; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_lcdc_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<5); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<5; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>5; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_vpp0_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<6); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<6; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>6; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_vpp1_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<7); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<7; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>7; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_vpp2_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<8); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<8; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>8; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_mapconv_apb_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1); \
	_ezchip_macro_read_value_ |= (0x0&0x1); \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR); \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_mapconv_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<1); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<1; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>1; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_pixrawout_apb_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<9); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<9; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>9; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_pixrawout_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<10); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<10; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>10; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_apb_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<15); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<15; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>15; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_sys_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<16); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<16; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>16; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_ppi_tx_esc_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<19); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<19; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>19; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_ppi_rx_esc_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<20); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<20; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>20; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_csi2tx_strm0_apb_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<11); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<11; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>11; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_csi2tx_strm0_pix_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<12); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<12; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>12; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_vout_sys_rstgen_rstn_csi2tx_ppi_tx_esc_ { \
	u32 _ezchip_macro_read_value_=MA_INW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<13); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<13; \
	MA_OUTW(vout_sys_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(vout_sys_rstgen_Software_RESET_status0_REG_ADDR)>>13; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _ASSERT_RESET_rstgen_rstn_vout_src_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<23); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<23; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>23; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x0); \
}

#define _ASSERT_RESET_rstgen_rstn_disp_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<24); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<24; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>24; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x0); \
}

#define _ENABLE_CLOCK_clk_disp_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_disp_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_disp_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_vout_src_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_vout_src_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_vout_src_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _CLEAR_RESET_rstgen_rstn_vout_src_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<23); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<23; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>23; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_rstgen_rstn_disp_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<24); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<24; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>24; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _ENABLE_CLOCK_clk_vin_src_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_vin_src_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_vin_src_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_vin_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_vin_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_vin_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_vinnoc_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_vinnoc_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_vinnoc_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_ispslv_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_ispslv_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_ispslv_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_isp0_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_isp0_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_isp0_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_isp1_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_isp1_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_isp1_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_isp0noc_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_isp0noc_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_isp0noc_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_isp1noc_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_isp1noc_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_isp1noc_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _CLEAR_RESET_rstgen_rstn_vin_src_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<15); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<15; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>15; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_rstgen_rstn_vin_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<17); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<17; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>17; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_rstgen_rstn_vinnoc_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<18); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<18; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>18; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_rstgen_rstn_ispslv_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<16); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<16; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>16; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_rstgen_rstn_isp0_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<19); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<19; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>19; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_rstgen_rstn_isp0noc_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<20); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<20; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>20; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_rstgen_rstn_isp1_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<21); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<21; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>21; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_rstgen_rstn_isp1noc_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<22); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<22; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>22; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _ENABLE_CLOCK_clk_c_isp0_ {}
#define clk_isp0_ctrl_REG_ADDR  ISP_CLKGEN_BASE_ADDR + 0x3C
#define clk_isp0_2x_ctrl_REG_ADDR  ISP_CLKGEN_BASE_ADDR + 0x40
#define clk_isp0_mipi_ctrl_REG_ADDR  ISP_CLKGEN_BASE_ADDR + 0x44
#define clk_isp1_ctrl_REG_ADDR  ISP_CLKGEN_BASE_ADDR + 0x48
#define clk_isp1_2x_ctrl_REG_ADDR  ISP_CLKGEN_BASE_ADDR + 0x4C
#define clk_isp1_mipi_ctrl_REG_ADDR  ISP_CLKGEN_BASE_ADDR + 0x50

#define _ENABLE_CLOCK_clk_isp0_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_isp0_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_isp0_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_isp0_2x_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_isp0_2x_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_isp0_2x_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_isp0_mipi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_isp0_mipi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_isp0_mipi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_c_isp1_ {}

#define _ENABLE_CLOCK_clk_isp1_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_isp1_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_isp1_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_isp1_2x_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_isp1_2x_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_isp1_2x_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_isp1_mipi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_isp1_mipi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_isp1_mipi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define clk_mipi_rx0_sys0_ctrl_REG_ADDR  			ISP_CLKGEN_BASE_ADDR + 0x24
#define clk_mipi_rx1_sys1_ctrl_REG_ADDR  			ISP_CLKGEN_BASE_ADDR + 0x38
#define isp_rstgen_Software_RESET_assert0_REG_ADDR  ISP_RSTGEN_BASE_ADDR + 0x0
#define isp_rstgen_Software_RESET_status0_REG_ADDR  ISP_RSTGEN_BASE_ADDR + 0x4

#define _ENABLE_CLOCK_clk_mipi_rx0_sys0_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_mipi_rx0_sys0_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_mipi_rx0_sys0_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ENABLE_CLOCK_clk_mipi_rx1_sys1_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_mipi_rx1_sys1_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<31; \
	MA_OUTW(clk_mipi_rx1_sys1_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _CLEAR_RESET_isp_rstgen_rst_n_pclk_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<1); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<1; \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR)>>1; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_isp_rstgen_rst_n_sys_clk_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1); \
	_ezchip_macro_read_value_ |= (0x0&0x1); \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR); \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_isp_rstgen_rst_c_isp0_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<19); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<19; \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR)>>19; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x0); \
}

#define _CLEAR_RESET_isp_rstgen_rst_isp_0_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<11); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<11; \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR)>>11; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_isp_rstgen_rst_p_isp0_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<15); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<15; \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR)>>15; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x0); \
}

#define _CLEAR_RESET_isp_rstgen_rst_n_sys_clk_1_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<2); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<2; \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR)>>2; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_isp_rstgen_rst_c_isp1_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<20); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<20; \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR)>>20; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x0); \
}

#define _CLEAR_RESET_isp_rstgen_rst_c_isp1_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<20); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<20; \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR)>>20; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x0); \
}

#define _CLEAR_RESET_isp_rstgen_rst_p_isp1_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<16); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<16; \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR)>>16; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x0); \
}

#define _CLEAR_RESET_isp_rstgen_rst_p_axiwr_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<14); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<14; \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR)>>14; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_isp_rstgen_rst_p_axird_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<13); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<13; \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR)>>13; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _CLEAR_RESET_isp_rstgen_rst_isp_1_ { \
	u32 _ezchip_macro_read_value_=MA_INW(isp_rstgen_Software_RESET_assert0_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<12); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<12; \
	MA_OUTW(isp_rstgen_Software_RESET_assert0_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(isp_rstgen_Software_RESET_status0_REG_ADDR)>>12; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x1); \
}

#define _DISABLE_CLOCK_clk_disp_axi_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_disp_axi_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<31; \
	MA_OUTW(clk_disp_axi_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _DISABLE_CLOCK_clk_vout_src_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_vout_src_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<31; \
	MA_OUTW(clk_vout_src_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

#define _ASSERT_RESET_rstgen_rstn_vin_src_ { \
	u32 _ezchip_macro_read_value_=MA_INW(rstgen_Software_RESET_assert1_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<15); \
	_ezchip_macro_read_value_ |= (0x1&0x1)<<15; \
	MA_OUTW(rstgen_Software_RESET_assert1_REG_ADDR,_ezchip_macro_read_value_); \
	do { \
		_ezchip_macro_read_value_ = MA_INW(rstgen_Software_RESET_status1_REG_ADDR)>>15; \
		_ezchip_macro_read_value_ &= 0x1;\
	} while(_ezchip_macro_read_value_!=0x0); \
}

#define _DISABLE_CLOCK_clk_vin_src_ { \
	u32 _ezchip_macro_read_value_=MA_INW(clk_vin_src_ctrl_REG_ADDR); \
	_ezchip_macro_read_value_ &= ~(0x1<<31); \
	_ezchip_macro_read_value_ |= (0x0&0x1)<<31; \
	MA_OUTW(clk_vin_src_ctrl_REG_ADDR,_ezchip_macro_read_value_); \
}

void delay(int cycles)
{
    int i;

    for(i = 0; i < cycles; i++);
}

void vout_sys_clkrstsrc_init(int open) 
{
	if(open==0x0) {
		_DISABLE_CLOCK_clk_disp_axi_
		_DISABLE_CLOCK_clk_vout_src_
		_ASSERT_RESET_rstgen_rstn_vout_src_
		_ASSERT_RESET_rstgen_rstn_disp_axi_
	} else {
		_ENABLE_CLOCK_clk_disp_axi_
		_ENABLE_CLOCK_clk_vout_src_
		_CLEAR_RESET_rstgen_rstn_vout_src_
		_CLEAR_RESET_rstgen_rstn_disp_axi_
	}

	delay(1000);
	printk("Config the clk and reset source for vout domain Finish\n");
}

void vout_sys_clkrst_init(int open){
	if(open==0x1){
	 _ENABLE_CLOCK_clk_disp0_axi_;
	 _ENABLE_CLOCK_clk_disp1_axi_;
	 _ENABLE_CLOCK_clk_lcdc_oclk_;
	 _ENABLE_CLOCK_clk_lcdc_axi_;
	 _ENABLE_CLOCK_clk_vpp0_axi_;
	 _ENABLE_CLOCK_clk_vpp1_axi_;
	 _ENABLE_CLOCK_clk_vpp2_axi_;
	 _ENABLE_CLOCK_clk_mapconv_apb_;
	 _ENABLE_CLOCK_clk_mapconv_axi_;
	 _ENABLE_CLOCK_clk_pixrawout_apb_;
	 _ENABLE_CLOCK_clk_pixrawout_axi_;
	 _ENABLE_CLOCK_clk_csi2tx_strm0_apb_;
	 _ENABLE_CLOCK_clk_csi2tx_strm0_pixclk_;
	 _ENABLE_CLOCK_clk_ppi_tx_esc_clk_;
	 _ENABLE_CLOCK_clk_dsi_apb_;
	 _ENABLE_CLOCK_clk_dsi_sys_clk_;


	 _CLEAR_RESET_vout_sys_rstgen_rstn_disp0_axi_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_disp1_axi_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_lcdc_axi_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_vpp0_axi_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_vpp1_axi_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_vpp2_axi_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_mapconv_apb_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_mapconv_axi_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_pixrawout_apb_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_pixrawout_axi_;

	 _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_apb_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_sys_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_ppi_tx_esc_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_ppi_rx_esc_;

	 _CLEAR_RESET_vout_sys_rstgen_rstn_csi2tx_strm0_apb_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_csi2tx_strm0_pix_;
	 _CLEAR_RESET_vout_sys_rstgen_rstn_csi2tx_ppi_tx_esc_;
	}
	printk("Config the clk and reset for vout domain, Finish\n");
	delay(100);
}

#endif

