// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 YanHong Wang <yanhong.wang@starfivetech.com>
 */

#ifndef __SOC_STARFIVE_JH71100_H
#define __SOC_STARFIVE_JH71100_H
#include <linux/io.h>
#include <soc/sifive/sifive_l2_cache.h>


#define starfive_flush_dcache(start, len) \
	sifive_l2_flush64_range(start, len)

#endif /*__SOC_STARFIVE_JH71100_H*/
