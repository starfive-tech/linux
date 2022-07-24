/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2021 Emil Renner Berthing <kernel@esmil.dk>
 */

#ifndef __DT_BINDINGS_RESET_STARFIVE_JH7110_AON_H__
#define __DT_BINDINGS_RESET_STARFIVE_JH7110_AON_H__

#define JH7110_AONRST_GMAC0_AXI		0 /* rstn_u0_u0_dw_gmac5_axi64_rstn_axi */
#define JH7110_AONRST_GMAC0_AHB		1 /* rstn_u0_u0_dw_gmac5_axi64_rstn_ahb */
#define JH7110_AONRST_AON_IOMUX		2 /* rstn_u0_aon_iomux_presetn */
#define JH7110_AONRST_PMU_APB		3 /* rstn_u0_pmu_rstn_apb */
#define JH7110_AONRST_PMU_WKUP		4 /* rstn_u0_pmu_rstn_wkup */
#define JH7110_AONRST_RTC_APB		5 /* rstn_u0_rtc_hms_rstn_apb */
#define JH7110_AONRST_RTC_CAL		6 /* rstn_u0_rtc_hms_rstn_cal */
#define JH7110_AONRST_RTC_32K		7 /* rstn_u0_u7mc_sft7110_rst_core2 */

#define	JH7110_AONRST_END		8

#endif /* __DT_BINDINGS_RESET_STARFIVE_JH7110_SYS_H__ */
