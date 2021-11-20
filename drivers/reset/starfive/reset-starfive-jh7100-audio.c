// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Audio reset driver for the StarFive JH7100 SoC
 *
 * Copyright (C) 2021 Emil Renner Berthing <kernel@esmil.dk>
 */

#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "reset-starfive-jh71x0.h"

#include <dt-bindings/reset/starfive-jh7100-audio.h>

/* register offsets */
#define JH7100_AUDRST_ASSERT0	0x00
#define JH7100_AUDRST_STATUS0	0x04

/*
 * Writing a 1 to the n'th bit of the ASSERT register asserts
 * line n, and writing a 0 deasserts the same line.
 * Most reset lines have their status inverted so a 0 bit in the STATUS
 * register means the line is asserted and a 1 means it's deasserted. A few
 * lines don't though, so store the expected value of the status registers when
 * all lines are asserted.
 */
static const u32 jh7100_audrst_asserted[1] = {
	BIT(JH7100_AUDRST_USB_AXI) |
	BIT(JH7100_AUDRST_USB_PWRUP_RST_N) |
	BIT(JH7100_AUDRST_USB_PONRST)
};

static int jh7100_audrst_probe(struct platform_device *pdev)
{
	void __iomem *base = devm_platform_ioremap_resource(pdev, 0);

	if (IS_ERR(base))
		return PTR_ERR(base);

	return reset_starfive_jh71x0_register(&pdev->dev, pdev->dev.of_node,
					      base + JH7100_AUDRST_ASSERT0,
					      base + JH7100_AUDRST_STATUS0,
					      jh7100_audrst_asserted,
					      JH7100_AUDRSTN_END,
					      THIS_MODULE);
}

static const struct of_device_id jh7100_audrst_dt_ids[] = {
	{ .compatible = "starfive,jh7100-audrst" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, jh7100_audrst_dt_ids);

static struct platform_driver jh7100_audrst_driver = {
	.probe = jh7100_audrst_probe,
	.driver = {
		.name = "jh7100-reset-audio",
		.of_match_table = jh7100_audrst_dt_ids,
	},
};
module_platform_driver(jh7100_audrst_driver);

MODULE_AUTHOR("Emil Renner Berthing");
MODULE_DESCRIPTION("StarFive JH7100 audio reset driver");
MODULE_LICENSE("GPL");
