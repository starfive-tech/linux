// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Isp reset driver for the StarFive JH7100 SoC
 *
 * Copyright (C) 2021 Emil Renner Berthing <kernel@esmil.dk>
 * Copyright (C) 2021 Hal Feng <hal.feng@starfivetech.com>
 */

#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>

#include <dt-bindings/reset/starfive-jh7100-isp.h>

#include "reset-starfive-jh7100.h"

/* register offsets */
#define JH7100_ISPRST_ASSERT0	0x00
#define JH7100_ISPRST_STATUS0	0x04

/*
 * Writing a 1 to the n'th bit of the ASSERT register asserts
 * line n, and writing a 0 deasserts the same line.
 * Most reset lines have their status inverted so a 0 bit in the STATUS
 * register means the line is asserted and a 1 means it's deasserted. A few
 * lines don't though, so store the expected value of the status registers when
 * all lines are asserted.
 */
static const u32 jh7100_isprst_asserted[1] = {
	0,
};

static int jh7100_isprst_probe(struct platform_device *pdev)
{
	return reset_starfive_jh7100_generic_probe(pdev, jh7100_isprst_asserted,
						   JH7100_ISPRST_STATUS0, JH7100_ISPRSTN_END);
}

static const struct of_device_id jh7100_isprst_dt_ids[] = {
	{ .compatible = "starfive,jh7100-isprst" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, jh7100_isprst_dt_ids);

static struct platform_driver jh7100_isprst_driver = {
	.probe = jh7100_isprst_probe,
	.driver = {
		.name = "jh7100-reset-isp",
		.of_match_table = jh7100_isprst_dt_ids,
	},
};
module_platform_driver(jh7100_isprst_driver);

MODULE_AUTHOR("Emil Renner Berthing");
MODULE_AUTHOR("Hal Feng");
MODULE_DESCRIPTION("StarFive JH7100 isp reset driver");
MODULE_LICENSE("GPL");
