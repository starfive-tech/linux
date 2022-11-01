// SPDX-License-Identifier: GPL-2.0
/*
 * Starfive Timer driver
 *
 * Copyright (C) 2022 StarFive Technology Co., Ltd.
 *
 * Author:
 * Xingyu Wu <xingyu.wu@starfivetech.com>
 * Samin Guo <samin.guo@starfivetech.com>
 */

#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/sched_clock.h>

#include "timer-starfive.h"

static const struct starfive_timer_chan_base starfive_timer_jh7110_base = {
	.ctrl		= STARFIVE_TIMER_JH7110_CTL,
	.load		= STARFIVE_TIMER_JH7110_LOAD,
	.enable		= STARFIVE_TIMER_JH7110_ENABLE,
	.reload		= STARFIVE_TIMER_JH7110_RELOAD,
	.value		= STARFIVE_TIMER_JH7110_VALUE,
	.intclr		= STARFIVE_TIMER_JH7110_INT_CLR,
	.intmask	= STARFIVE_TIMER_JH7110_INT_MASK,
	.channel_num	= STARFIVE_TIMER_CH_4,
	.channel_base	= {STARFIVE_TIMER_CH_BASE(0), STARFIVE_TIMER_CH_BASE(1),
			   STARFIVE_TIMER_CH_BASE(2), STARFIVE_TIMER_CH_BASE(3)},
};

static inline struct starfive_clkevt *to_starfive_clkevt(struct clock_event_device *evt)
{
	return container_of(evt, struct starfive_clkevt, evt);
}

/* 0:continuous-run mode, 1:single-run mode */
static inline void starfive_timer_set_mod(struct starfive_clkevt *clkevt, int mod)
{
	writel(mod, clkevt->ctrl);
}

/* Interrupt Mask Register, 0:Unmask, 1:Mask */
static inline void starfive_timer_int_enable(struct starfive_clkevt *clkevt)
{
	writel(STARFIVE_TIMER_INTMASK_DIS, clkevt->intmask);
}

static inline void starfive_timer_int_disable(struct starfive_clkevt *clkevt)
{
	writel(STARFIVE_TIMER_INTMASK_ENA, clkevt->intmask);
}

/*
 * BIT(0): Read value represent channel intr status.
 * Write 1 to this bit to clear interrupt. Write 0 has no effects.
 * BIT(1): "1" means that it is clearing interrupt. BIT(0) can not be written.
 */
static inline int starfive_timer_int_clear(struct starfive_clkevt *clkevt)
{
	u32 value;
	int ret;

	/* waiting interrupt can be to clearing */
	ret = readl_poll_timeout_atomic(clkevt->intclr, value,
					!(value & STARFIVE_TIMER_JH7110_INT_CLR_AVA_MASK),
					STARFIVE_DELAY_US, STARFIVE_TIMEOUT_US);
	if (!ret)
		writel(0x1, clkevt->intclr);

	return ret;
}

/*
 * The initial value to be loaded into the
 * counter and is also used as the reload value.
 * val = clock rate --> 1s
 */
static inline void starfive_timer_set_load(struct starfive_clkevt *clkevt, u32 val)
{
	writel(val, clkevt->load);
}

static inline u32 starfive_timer_get_val(struct starfive_clkevt *clkevt)
{
	return readl(clkevt->value);
}

/*
 * Write RELOAD register to reload preset value to counter.
 * (Write 0 and write 1 are both ok)
 */
static inline void starfive_timer_set_reload(struct starfive_clkevt *clkevt)
{
	writel(0, clkevt->reload);
}

static inline void starfive_timer_enable(struct starfive_clkevt *clkevt)
{
	writel(STARFIVE_TIMER_ENA, clkevt->enable);
}

static inline void starfive_timer_disable(struct starfive_clkevt *clkevt)
{
	writel(STARFIVE_TIMER_DIS, clkevt->enable);
}

static int starfive_timer_int_init_enable(struct starfive_clkevt *clkevt)
{
	int ret;

	starfive_timer_int_disable(clkevt);
	ret = starfive_timer_int_clear(clkevt);
	if (ret)
		return ret;

	starfive_timer_int_enable(clkevt);
	starfive_timer_enable(clkevt);

	return 0;
}

static int starfive_timer_shutdown(struct clock_event_device *evt)
{
	struct starfive_clkevt *clkevt = to_starfive_clkevt(evt);

	starfive_timer_disable(clkevt);
	return starfive_timer_int_clear(clkevt);
}

static void starfive_timer_suspend(struct clock_event_device *evt)
{
	struct starfive_clkevt *clkevt = to_starfive_clkevt(evt);

	clkevt->reload_val = starfive_timer_get_val(clkevt);
	starfive_timer_shutdown(evt);
}

static void starfive_timer_resume(struct clock_event_device *evt)
{
	struct starfive_clkevt *clkevt = to_starfive_clkevt(evt);

	starfive_timer_set_load(clkevt, clkevt->reload_val);
	starfive_timer_set_reload(clkevt);
	starfive_timer_int_enable(clkevt);
	starfive_timer_enable(clkevt);
}

static int starfive_timer_tick_resume(struct clock_event_device *evt)
{
	starfive_timer_resume(evt);

	return 0;
}

static int starfive_clocksource_init(struct starfive_clkevt *clkevt)
{
	int ret;

	starfive_timer_set_mod(clkevt, STARFIVE_TIMER_MOD_CONTIN);
	starfive_timer_set_load(clkevt, STARFIVE_TIMER_MAX_TICKS);
	ret = starfive_timer_int_init_enable(clkevt);
	if (ret)
		return ret;

	return clocksource_mmio_init(clkevt->value, clkevt->name, clkevt->rate,
				     STARFIVE_CLOCK_SOURCE_RATING, STARFIVE_VALID_BITS,
				     clocksource_mmio_readl_down);
}

/* IRQ handler for the timer */
static irqreturn_t starfive_timer_interrupt(int irq, void *priv)
{
	struct clock_event_device *evt = (struct clock_event_device *)priv;
	struct starfive_clkevt *clkevt = to_starfive_clkevt(evt);

	if (starfive_timer_int_clear(clkevt))
		return IRQ_NONE;

	if (evt->event_handler)
		evt->event_handler(evt);

	return IRQ_HANDLED;
}

static int starfive_timer_set_periodic(struct clock_event_device *evt)
{
	struct starfive_clkevt *clkevt = to_starfive_clkevt(evt);

	starfive_timer_disable(clkevt);
	starfive_timer_set_mod(clkevt, STARFIVE_TIMER_MOD_CONTIN);
	starfive_timer_set_load(clkevt, clkevt->periodic);

	return starfive_timer_int_init_enable(clkevt);
}

static int starfive_timer_set_oneshot(struct clock_event_device *evt)
{
	struct starfive_clkevt *clkevt = to_starfive_clkevt(evt);

	starfive_timer_disable(clkevt);
	starfive_timer_set_mod(clkevt, STARFIVE_TIMER_MOD_SINGLE);
	starfive_timer_set_load(clkevt, STARFIVE_TIMER_MAX_TICKS);

	return starfive_timer_int_init_enable(clkevt);
}

static int starfive_timer_set_next_event(unsigned long next,
					 struct clock_event_device *evt)
{
	struct starfive_clkevt *clkevt = to_starfive_clkevt(evt);

	starfive_timer_disable(clkevt);
	starfive_timer_set_mod(clkevt, STARFIVE_TIMER_MOD_SINGLE);
	starfive_timer_set_load(clkevt, next);
	starfive_timer_enable(clkevt);

	return 0;
}

static void starfive_set_clockevent(struct clock_event_device *evt)
{
	evt->features = CLOCK_EVT_FEAT_PERIODIC |
			CLOCK_EVT_FEAT_ONESHOT |
			CLOCK_EVT_FEAT_DYNIRQ;
	evt->set_state_shutdown = starfive_timer_shutdown;
	evt->set_state_periodic = starfive_timer_set_periodic;
	evt->set_state_oneshot = starfive_timer_set_oneshot;
	evt->set_state_oneshot_stopped = starfive_timer_shutdown;
	evt->tick_resume = starfive_timer_tick_resume;
	evt->set_next_event = starfive_timer_set_next_event;
	evt->suspend = starfive_timer_suspend;
	evt->resume = starfive_timer_resume;
	evt->rating = STARFIVE_CLOCKEVENT_RATING;
}

static void starfive_clockevents_register(struct starfive_clkevt *clkevt)
{
	clkevt->rate = clk_get_rate(clkevt->clk);
	clkevt->periodic = DIV_ROUND_CLOSEST(clkevt->rate, HZ);

	starfive_set_clockevent(&clkevt->evt);
	clkevt->evt.name = clkevt->name;
	clkevt->evt.irq = clkevt->irq;
	clkevt->evt.cpumask = cpu_possible_mask;

	clockevents_config_and_register(&clkevt->evt, clkevt->rate,
					STARFIVE_TIMER_MIN_TICKS, STARFIVE_TIMER_MAX_TICKS);
}

static void __init starfive_clkevt_base_init(const struct starfive_timer_chan_base *timer,
					     struct starfive_clkevt *clkevt,
					     void __iomem *base, int ch)
{
	void __iomem *channel_base;

	channel_base = base + timer->channel_base[ch];
	clkevt->base = channel_base;
	clkevt->ctrl = channel_base + timer->ctrl;
	clkevt->load = channel_base + timer->load;
	clkevt->enable = channel_base + timer->enable;
	clkevt->reload = channel_base + timer->reload;
	clkevt->value = channel_base + timer->value;
	clkevt->intclr = channel_base + timer->intclr;
	clkevt->intmask = channel_base + timer->intmask;
}

static int __init starfive_timer_probe(struct platform_device *pdev)
{
	const struct starfive_timer_chan_base *timer_base = of_device_get_match_data(&pdev->dev);
	char name[10];
	struct starfive_timer_priv *priv;
	struct starfive_clkevt *clkevt;
	struct clk *pclk;
	struct reset_control *rst;
	int ch;
	int ret;

	priv = devm_kzalloc(&pdev->dev, struct_size(priv, clkevt, timer_base->channel_num),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return dev_err_probe(&pdev->dev, PTR_ERR(priv->base),
				     "failed to map registers\n");

	rst = devm_reset_control_get_exclusive(&pdev->dev, "apb");
	if (IS_ERR(rst))
		return dev_err_probe(&pdev->dev, PTR_ERR(rst), "failed to get apb reset\n");

	pclk = devm_clk_get_enabled(&pdev->dev, "apb");
	if (IS_ERR(pclk))
		return dev_err_probe(&pdev->dev, PTR_ERR(pclk),
				     "failed to get & enable apb clock\n");

	ret = reset_control_deassert(rst);
	if (ret)
		goto err;

	priv->dev = &pdev->dev;
	platform_set_drvdata(pdev, priv);

	for (ch = 0; ch < timer_base->channel_num; ch++) {
		clkevt = &priv->clkevt[ch];
		snprintf(name, sizeof(name), "ch%d", ch);

		starfive_clkevt_base_init(timer_base, clkevt, priv->base, ch);
		/* Ensure timers are disabled */
		starfive_timer_disable(clkevt);

		rst = devm_reset_control_get_exclusive(&pdev->dev, name);
		if (IS_ERR(rst)) {
			ret = PTR_ERR(rst);
			goto err;
		}

		clkevt->clk = devm_clk_get_enabled(&pdev->dev, name);
		if (IS_ERR(clkevt->clk)) {
			ret = PTR_ERR(clkevt->clk);
			goto err;
		}

		ret = reset_control_deassert(rst);
		if (ret)
			goto ch_err;

		clkevt->irq = platform_get_irq(pdev, ch);
		if (clkevt->irq < 0) {
			ret = clkevt->irq;
			goto ch_err;
		}

		snprintf(clkevt->name, sizeof(clkevt->name), "%s.ch%d", pdev->name, ch);
		starfive_clockevents_register(clkevt);

		ret = devm_request_irq(&pdev->dev, clkevt->irq, starfive_timer_interrupt,
				       IRQF_TIMER | IRQF_IRQPOLL,
				       clkevt->name, &clkevt->evt);
		if (ret)
			goto ch_err;

		ret = starfive_clocksource_init(clkevt);
		if (ret)
			goto ch_err;
	}

	return 0;

ch_err:
	/* Only unregister the failed channel and the rest timer channels continue to work. */
	clk_disable_unprepare(clkevt->clk);
err:
	/* If no other channel successfully registers, pclk should be disabled. */
	if (!ch)
		clk_disable_unprepare(pclk);

	return ret;
}

static const struct of_device_id starfive_timer_match[] = {
	{ .compatible = "starfive,jh7110-timer", .data = &starfive_timer_jh7110_base },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, starfive_timer_match);

static struct platform_driver starfive_timer_driver = {
	.probe = starfive_timer_probe,
	.driver = {
		.name = "starfive-timer",
		.of_match_table = starfive_timer_match,
	},
};
module_platform_driver(starfive_timer_driver);

MODULE_AUTHOR("Xingyu Wu <xingyu.wu@starfivetech.com>");
MODULE_DESCRIPTION("StarFive timer driver");
MODULE_LICENSE("GPL");
