/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * PMU driver for the StarFive JH7110 SoC
 *
 * Copyright (C) 2022 samin <samin.guo@starfivetech.com>
 */

#ifndef __SOC_STARFIVE_JH7110_PMU_H__
#define __SOC_STARFIVE_JH7110_PMU_H__

#include <linux/bits.h>
#include <linux/types.h>

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

