// SPDX-License-Identifier: GPL-2.0
/*
 * Starfive JH7110 Timer driver
 *
 * Copyright (C) 2022-2023 StarFive Technology Co., Ltd.
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

/* Bias: Ch0-0x0, Ch1-0x40, Ch2-0x80, and so on. */
#define JH7110_TIMER_CH_LEN		0x40
#define JH7110_TIMER_CH_BASE(x)		((x) * JH7110_TIMER_CH_LEN)
#define JH7110_TIMER_CH_MAX		4

#define JH7110_CLOCK_SOURCE_RATING	200
#define JH7110_VALID_BITS		32
#define JH7110_DELAY_US			0
#define JH7110_TIMEOUT_US		10000
#define JH7110_CLOCKEVENT_RATING	300
#define JH7110_TIMER_MAX_TICKS		0xffffffff
#define JH7110_TIMER_MIN_TICKS		0xf
#define JH7110_TIMER_NAME_NUM		20

#define JH7110_TIMER_INT_STATUS		0x00 /* RO[0:4]: Interrupt Status for channel0~4 */
#define JH7110_TIMER_CTL		0x04 /* RW[0]: 0-continuous run, 1-single run */
#define JH7110_TIMER_LOAD		0x08 /* RW: load value to counter */
#define JH7110_TIMER_ENABLE		0x10 /* RW[0]: timer enable register */
#define JH7110_TIMER_RELOAD		0x14 /* RW: write 1 or 0 both reload counter */
#define JH7110_TIMER_VALUE		0x18 /* RO: timer value register */
#define JH7110_TIMER_INT_CLR		0x20 /* RW: timer interrupt clear register */
#define JH7110_TIMER_INT_MASK		0x24 /* RW[0]: timer interrupt mask register */
#define JH7110_TIMER_INT_CLR_AVA_MASK	BIT(1)

enum JH7110_TIMER_INTMASK {
	JH7110_TIMER_INTMASK_DIS = 0,
	JH7110_TIMER_INTMASK_ENA = 1
};

enum JH7110_TIMER_MOD {
	JH7110_TIMER_MOD_CONTIN = 0,
	JH7110_TIMER_MOD_SINGLE = 1
};

enum JH7110_TIMER_CTL_EN {
	JH7110_TIMER_DIS = 0,
	JH7110_TIMER_ENA = 1
};

struct jh7110_clkevt {
	struct clock_event_device evt;
	struct clocksource cs;
	struct clk *clk;
	char name[JH7110_TIMER_NAME_NUM];
	int irq;
	u32 periodic;
	u32 rate;
	u32 reload_val;
	void __iomem *base;
};

struct jh7110_timer_priv {
	struct device *dev;
	void __iomem *base;
	struct jh7110_clkevt clkevt[JH7110_TIMER_CH_MAX];
};

static inline struct jh7110_clkevt *to_jh7110_clkevt(struct clock_event_device *evt)
{
	return container_of(evt, struct jh7110_clkevt, evt);
}

/* 0:continuous-run mode, 1:single-run mode */
static inline void jh7110_timer_set_continuous_mod(struct jh7110_clkevt *clkevt)
{
	writel(JH7110_TIMER_MOD_CONTIN, clkevt->base + JH7110_TIMER_CTL);
}

static inline void jh7110_timer_set_single_mod(struct jh7110_clkevt *clkevt)
{
	writel(JH7110_TIMER_MOD_SINGLE, clkevt->base + JH7110_TIMER_CTL);
}

/* Interrupt Mask Register, 0:Unmask, 1:Mask */
static inline void jh7110_timer_int_enable(struct jh7110_clkevt *clkevt)
{
	writel(JH7110_TIMER_INTMASK_DIS, clkevt->base + JH7110_TIMER_INT_MASK);
}

static inline void jh7110_timer_int_disable(struct jh7110_clkevt *clkevt)
{
	writel(JH7110_TIMER_INTMASK_ENA, clkevt->base + JH7110_TIMER_INT_MASK);
}

/*
 * BIT(0): Read value represent channel intr status.
 * Write 1 to this bit to clear interrupt. Write 0 has no effects.
 * BIT(1): "1" means that it is clearing interrupt. BIT(0) can not be written.
 */
static inline int jh7110_timer_int_clear(struct jh7110_clkevt *clkevt)
{
	u32 value;
	int ret;

	/* waiting interrupt can be to clearing */
	ret = readl_poll_timeout_atomic(clkevt->base + JH7110_TIMER_INT_CLR, value,
					!(value & JH7110_TIMER_INT_CLR_AVA_MASK),
					JH7110_DELAY_US, JH7110_TIMEOUT_US);
	if (!ret)
		writel(0x1, clkevt->base + JH7110_TIMER_INT_CLR);

	return ret;
}

/*
 * The initial value to be loaded into the
 * counter and is also used as the reload value.
 * val = clock rate --> 1s
 */
static inline void jh7110_timer_set_load(struct jh7110_clkevt *clkevt, u32 val)
{
	writel(val, clkevt->base + JH7110_TIMER_LOAD);
}

static inline u32 jh7110_timer_get_val(struct jh7110_clkevt *clkevt)
{
	return readl(clkevt->base + JH7110_TIMER_VALUE);
}

/*
 * Write RELOAD register to reload preset value to counter.
 * Write 0 and write 1 are both ok.
 */
static inline void jh7110_timer_set_reload(struct jh7110_clkevt *clkevt)
{
	writel(0, clkevt->base + JH7110_TIMER_RELOAD);
}

static inline void jh7110_timer_enable(struct jh7110_clkevt *clkevt)
{
	writel(JH7110_TIMER_ENA, clkevt->base + JH7110_TIMER_ENABLE);
}

static inline void jh7110_timer_disable(struct jh7110_clkevt *clkevt)
{
	writel(JH7110_TIMER_DIS, clkevt->base + JH7110_TIMER_ENABLE);
}

static int jh7110_timer_int_init_enable(struct jh7110_clkevt *clkevt)
{
	int ret;

	jh7110_timer_int_disable(clkevt);
	ret = jh7110_timer_int_clear(clkevt);
	if (ret)
		return ret;

	jh7110_timer_int_enable(clkevt);
	jh7110_timer_enable(clkevt);

	return 0;
}

static int jh7110_timer_shutdown(struct clock_event_device *evt)
{
	struct jh7110_clkevt *clkevt = to_jh7110_clkevt(evt);

	jh7110_timer_disable(clkevt);
	return jh7110_timer_int_clear(clkevt);
}

static void jh7110_timer_suspend(struct clock_event_device *evt)
{
	struct jh7110_clkevt *clkevt = to_jh7110_clkevt(evt);

	clkevt->reload_val = jh7110_timer_get_val(clkevt);
	jh7110_timer_shutdown(evt);
}

static void jh7110_timer_resume(struct clock_event_device *evt)
{
	struct jh7110_clkevt *clkevt = to_jh7110_clkevt(evt);

	jh7110_timer_set_load(clkevt, clkevt->reload_val);
	jh7110_timer_set_reload(clkevt);
	jh7110_timer_int_enable(clkevt);
	jh7110_timer_enable(clkevt);
}

static int jh7110_timer_tick_resume(struct clock_event_device *evt)
{
	jh7110_timer_resume(evt);

	return 0;
}

/* IRQ handler for the timer */
static irqreturn_t jh7110_timer_interrupt(int irq, void *priv)
{
	struct clock_event_device *evt = (struct clock_event_device *)priv;
	struct jh7110_clkevt *clkevt = to_jh7110_clkevt(evt);

	if (jh7110_timer_int_clear(clkevt))
		return IRQ_NONE;

	if (evt->event_handler)
		evt->event_handler(evt);

	return IRQ_HANDLED;
}

static int jh7110_timer_set_periodic(struct clock_event_device *evt)
{
	struct jh7110_clkevt *clkevt = to_jh7110_clkevt(evt);

	clkevt->periodic = DIV_ROUND_CLOSEST(clkevt->rate, HZ);
	jh7110_timer_disable(clkevt);
	jh7110_timer_set_continuous_mod(clkevt);
	jh7110_timer_set_load(clkevt, clkevt->periodic);

	return jh7110_timer_int_init_enable(clkevt);
}

static int jh7110_timer_set_oneshot(struct clock_event_device *evt)
{
	struct jh7110_clkevt *clkevt = to_jh7110_clkevt(evt);

	jh7110_timer_disable(clkevt);
	jh7110_timer_set_single_mod(clkevt);
	jh7110_timer_set_load(clkevt, JH7110_TIMER_MAX_TICKS);

	return jh7110_timer_int_init_enable(clkevt);
}

static int jh7110_timer_set_next_event(unsigned long next,
				       struct clock_event_device *evt)
{
	struct jh7110_clkevt *clkevt = to_jh7110_clkevt(evt);

	jh7110_timer_disable(clkevt);
	jh7110_timer_set_single_mod(clkevt);
	jh7110_timer_set_load(clkevt, next);
	jh7110_timer_enable(clkevt);

	return 0;
}

static void jh7110_set_clockevent(struct clock_event_device *evt)
{
	evt->features = CLOCK_EVT_FEAT_PERIODIC |
			CLOCK_EVT_FEAT_ONESHOT |
			CLOCK_EVT_FEAT_DYNIRQ;
	evt->set_state_shutdown = jh7110_timer_shutdown;
	evt->set_state_periodic = jh7110_timer_set_periodic;
	evt->set_state_oneshot = jh7110_timer_set_oneshot;
	evt->set_state_oneshot_stopped = jh7110_timer_shutdown;
	evt->tick_resume = jh7110_timer_tick_resume;
	evt->set_next_event = jh7110_timer_set_next_event;
	evt->suspend = jh7110_timer_suspend;
	evt->resume = jh7110_timer_resume;
	evt->rating = JH7110_CLOCKEVENT_RATING;
}

static u64 jh7110_timer_clocksource_read(struct clocksource *cs)
{
	struct jh7110_clkevt *clkevt = container_of(cs, struct jh7110_clkevt, cs);

	return (u64)jh7110_timer_get_val(clkevt);
}

static int jh7110_clocksource_init(struct jh7110_clkevt *clkevt)
{
	int ret;

	jh7110_timer_set_continuous_mod(clkevt);
	jh7110_timer_set_load(clkevt, JH7110_TIMER_MAX_TICKS);

	ret = jh7110_timer_int_init_enable(clkevt);
	if (ret)
		return ret;

	clkevt->cs.name = clkevt->name;
	clkevt->cs.rating = JH7110_CLOCK_SOURCE_RATING;
	clkevt->cs.read = jh7110_timer_clocksource_read;
	clkevt->cs.mask = CLOCKSOURCE_MASK(JH7110_VALID_BITS);
	clkevt->cs.flags = CLOCK_SOURCE_IS_CONTINUOUS;

	return clocksource_register_hz(&clkevt->cs, clkevt->rate);
}

static void jh7110_clockevents_register(struct jh7110_clkevt *clkevt)
{
	clkevt->rate = clk_get_rate(clkevt->clk);

	jh7110_set_clockevent(&clkevt->evt);
	clkevt->evt.name = clkevt->name;
	clkevt->evt.irq = clkevt->irq;
	clkevt->evt.cpumask = cpu_possible_mask;

	clockevents_config_and_register(&clkevt->evt, clkevt->rate,
					JH7110_TIMER_MIN_TICKS, JH7110_TIMER_MAX_TICKS);
}

static int jh7110_timer_probe(struct platform_device *pdev)
{
	char name[JH7110_TIMER_NAME_NUM];
	struct jh7110_timer_priv *priv;
	struct jh7110_clkevt *clkevt;
	struct clk *pclk;
	struct reset_control *rst;
	int ch;
	int ret;

	priv = devm_kzalloc(&pdev->dev, struct_size(priv, clkevt, JH7110_TIMER_CH_MAX),
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
		return dev_err_probe(&pdev->dev, ret, "failed to deassert apb reset\n");

	priv->dev = &pdev->dev;
	platform_set_drvdata(pdev, priv);

	for (ch = 0; ch < JH7110_TIMER_CH_MAX; ch++) {
		clkevt = &priv->clkevt[ch];
		snprintf(name, sizeof(name), "ch%d", ch);

		clkevt->base = priv->base + JH7110_TIMER_CH_BASE(ch);
		/* Ensure timers are disabled */
		jh7110_timer_disable(clkevt);

		rst = devm_reset_control_get_exclusive(&pdev->dev, name);
		if (IS_ERR(rst))
			return PTR_ERR(rst);

		clkevt->clk = devm_clk_get_enabled(&pdev->dev, name);
		if (IS_ERR(clkevt->clk))
			return PTR_ERR(clkevt->clk);

		ret = reset_control_deassert(rst);
		if (ret)
			return ret;

		clkevt->irq = platform_get_irq(pdev, ch);
		if (clkevt->irq < 0)
			return clkevt->irq;

		snprintf(clkevt->name, sizeof(clkevt->name), "%s.ch%d", pdev->name, ch);
		jh7110_clockevents_register(clkevt);

		ret = devm_request_irq(&pdev->dev, clkevt->irq, jh7110_timer_interrupt,
				       IRQF_TIMER | IRQF_IRQPOLL,
				       clkevt->name, &clkevt->evt);
		if (ret)
			return ret;

		ret = jh7110_clocksource_init(clkevt);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct of_device_id jh7110_timer_match[] = {
	{ .compatible = "starfive,jh7110-timer", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, jh7110_timer_match);

static struct platform_driver jh7110_timer_driver = {
	.probe = jh7110_timer_probe,
	.driver = {
		.name = "jh7110-timer",
		.of_match_table = jh7110_timer_match,
	},
};
module_platform_driver(jh7110_timer_driver);

MODULE_AUTHOR("Xingyu Wu <xingyu.wu@starfivetech.com>");
MODULE_DESCRIPTION("StarFive JH7110 timer driver");
MODULE_LICENSE("GPL");
