// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/idr.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include "starfive_e24.h"
#include "starfive_e24_hw.h"

#define RET_E24_VECTOR_ADDR	0xc0000000

static void halt(void *hw_arg)
{
	struct e24_hw_arg *mail_arg = hw_arg;

	reset_control_assert(mail_arg->rst_core);
	pr_debug("e24 halt.\n");
}

static void release(void *hw_arg)
{
	struct e24_hw_arg *mail_arg = hw_arg;

	reset_control_deassert(mail_arg->rst_core);
	pr_debug("e24 begin run.\n");
}

static void reset(void *hw_arg)
{
	struct e24_hw_arg *mail_arg = hw_arg;

	regmap_update_bits(mail_arg->reg_syscon, 0x24, 0xFFFFFFFF, RET_E24_VECTOR_ADDR);
	pr_debug("e24 reset vector.\n");
}

static void disable(void *hw_arg)
{
	struct e24_hw_arg *mail_arg = hw_arg;

	clk_disable_unprepare(mail_arg->clk_core);
	clk_disable_unprepare(mail_arg->clk_dbg);
	clk_disable_unprepare(mail_arg->clk_rtc);

	pr_debug("e24 disable ...\n");

}

static int enable(void *hw_arg)
{
	struct e24_hw_arg *mail_arg = hw_arg;
	int ret = 0;

	ret = clk_prepare_enable(mail_arg->clk_core);
	if (ret)
		return -EAGAIN;

	ret = clk_prepare_enable(mail_arg->clk_dbg);
	if (ret) {
		clk_disable_unprepare(mail_arg->clk_core);
		return -EAGAIN;
	}

	ret = clk_prepare_enable(mail_arg->clk_rtc);
	if (ret) {
		clk_disable_unprepare(mail_arg->clk_core);
		clk_disable_unprepare(mail_arg->clk_dbg);
		return -EAGAIN;
	}

	pr_debug("e24_enable clk ...\n");
	return 0;
}


static int init(void *hw_arg)
{
	struct e24_hw_arg *mail_arg = hw_arg;

	mail_arg->reg_syscon = syscon_regmap_lookup_by_phandle(
					mail_arg->e24->dev->of_node,
					"starfive,stg-syscon");
	if (IS_ERR(mail_arg->reg_syscon)) {
		dev_err(mail_arg->e24->dev, "No starfive,stg-syscon\n");
		return PTR_ERR(mail_arg->reg_syscon);
	}

	mail_arg->clk_core = devm_clk_get_optional(mail_arg->e24->dev, "clk_core");
	if (IS_ERR(mail_arg->clk_core)) {
		dev_err(mail_arg->e24->dev, "failed to get e24 clk core\n");
		return -ENOMEM;
	}

	mail_arg->clk_dbg = devm_clk_get_optional(mail_arg->e24->dev, "clk_dbg");
	if (IS_ERR(mail_arg->clk_dbg)) {
		dev_err(mail_arg->e24->dev, "failed to get e24 clk dbg\n");
		return -ENOMEM;
	}

	mail_arg->clk_rtc = devm_clk_get_optional(mail_arg->e24->dev, "clk_rtc");
	if (IS_ERR(mail_arg->clk_rtc)) {
		dev_err(mail_arg->e24->dev, "failed to get e24 clk rtc\n");
		return -ENOMEM;
	}

	mail_arg->rst_core = devm_reset_control_get_exclusive(mail_arg->e24->dev, "e24_core");
	if (IS_ERR(mail_arg->rst_core)) {
		dev_err(mail_arg->e24->dev, "failed to get e24 reset\n");
		return -ENOMEM;
	}

	enable(hw_arg);

	return 0;
}

static struct e24_hw_ops e24_hw_ops = {
	.init = init,
	.enable = enable,
	.reset = reset,
	.halt = halt,
	.release = release,
	.disable = disable,
};

struct e24_hw_ops *e24_get_hw_ops(void)
{
	return &e24_hw_ops;
}
