/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright 2022 Emil Renner Berthing <kernel@esmil.dk>
 */

#ifndef __DT_BINDINGS_CLOCK_STARFIVE_JH7110_AON_H__
#define __DT_BINDINGS_CLOCK_STARFIVE_JH7110_AON_H__

#define JH7110_AONCLK_OSC_DIV4			 0 /* clk_osc_div4 */
#define JH7110_AONCLK_APB_FUNC			 1 /* clk_aon_apb_func */
#define JH7110_AONCLK_GMAC0_AHB			 2 /* clk_u0_dw_gmac5_axi64_clk_ahb */
#define JH7110_AONCLK_GMAC0_AXI			 3 /* clk_u0_dw_gmac5_axi64_clk_axi */
#define JH7110_AONCLK_GMAC0_RMII_RTX		 4 /* clk_gmac0_rmii_rtx */
#define JH7110_AONCLK_GMAC0_TX			 5 /* clk_u0_dw_gmac5_axi64_clk_tx */
#define JH7110_AONCLK_GMAC0_TX_INV		 6 /* clk_u0_dw_gmac5_axi64_clk_tx_inv */
#define JH7110_AONCLK_GMAC0_RX			 7 /* clk_u0_dw_gmac5_axi64_clk_rx */
#define JH7110_AONCLK_GMAC0_RX_INV		 8 /* clk_u0_dw_gmac5_axi64_clk_rx_inv */
#define JH7110_AONCLK_OTPC_APB			 9 /* clk_u0_otpc_clk_apb */
#define JH7110_AONCLK_RTC_APB			10 /* clk_u0_rtc_hms_clk_apb */
#define JH7110_AONCLK_RTC_INTERNAL		11 /* clk_rtc_internal */
#define JH7110_AONCLK_RTC_32K			12 /* clk_u0_rtc_hms_clk_osc32k */
#define JH7110_AONCLK_RTC_CAL			13 /* clk_u0_rtc_hms_clk_cal */

#define JH7110_AONCLK_END			14

#if 0
/* aon other */
#define JH7110_U0_GMAC5_CLK_PTP			15
#define JH7110_U0_GMAC5_CLK_RMII		16
#define JH7110_AON_SYSCON_PCLK			17
#define JH7110_AON_IOMUX_PCLK			18
#define JH7110_AON_CRG_PCLK			19
#define JH7110_PMU_CLK_APB			20
#define JH7110_PMU_CLK_WKUP			21
#define JH7110_RTC_HMS_CLK_OSC32K_G		22
#define JH7110_32K_OUT				23
#define JH7110_RESET0_CTRL_CLK_SRC		24
/* aon other and source */
#define JH7110_PCLK_MUX_FUNC_PCLK		25
#define JH7110_PCLK_MUX_BIST_PCLK		26

#endif

#endif /* __DT_BINDINGS_CLOCK_STARFIVE_JH7110_H__ */
