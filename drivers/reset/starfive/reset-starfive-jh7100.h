// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2021 Emil Renner Berthing <kernel@esmil.dk>
 */

#ifndef _RESET_STARFIVE_JH7100_H_
#define _RESET_STARFIVE_JH7100_H_

#include <linux/platform_device.h>

int reset_starfive_jh7100_generic_probe(struct platform_device *pdev,
					const u32 *asserted,
					unsigned int status_offset,
					unsigned int nr_resets);

#endif
