/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Reset driver for the StarFive JH7110 SoC
 *
 * Copyright (C) 2022 samin <samin.guo@starfivetech.com>
 */

#ifndef __SOC_STARFIVE_JH7110_PMU_H__
#define __SOC_STARFIVE_JH7110_PMU_H__

#include <linux/bits.h>
#include <linux/types.h>

/* SW/HW Power domain id  */
enum PMU_POWER_DOMAIN {
	POWER_DOMAIN_SYSTOP	= BIT(0),
	POWER_DOMAIN_CPU	= BIT(1),
	POWER_DOMAIN_GPUA	= BIT(2),
	POWER_DOMAIN_VDEC	= BIT(3),
	POWER_DOMAIN_JPU	= POWER_DOMAIN_VDEC,
	POWER_DOMAIN_VOUT	= BIT(4),
	POWER_DOMAIN_ISP	= BIT(5),
	POWER_DOMAIN_VENC	= BIT(6),
	POWER_DOMAIN_GPUB	= BIT(7),
	POWER_DOMAIN_ALL	= GENMASK(7, 0),
};

enum PMU_HARD_EVENT {
	PMU_HW_EVENT_RTC	= BIT(0),
	PMU_HW_EVENT_GMAC	= BIT(1),
	PMU_HW_EVENT_RFU	= BIT(2),
	PMU_HW_EVENT_RGPIO0	= BIT(3),
	PMU_HW_EVENT_RGPIO1	= BIT(4),
	PMU_HW_EVENT_RGPIO2	= BIT(5),
	PMU_HW_EVENT_RGPIO3	= BIT(6),
	PMU_HW_EVENT_GPU	= BIT(7),
	PMU_HW_EVENT_ALL	= GENMASK(7, 0),
};

void starfive_pmu_hw_event_turn_off_mask(u32 mask);

#endif /* __SOC_STARFIVE_JH7110_PMU_H__ */
