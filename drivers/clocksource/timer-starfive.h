/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 StarFive Technology Co., Ltd.
 */

#ifndef __STARFIVE_TIMER_H__
#define __STARFIVE_TIMER_H__

/* Bias: Ch0-0x0, Ch1-0x40, Ch2-0x80, and so on. */
#define STARFIVE_TIMER_CH_LEN			0x40
#define STARFIVE_TIMER_CH_BASE(x)		((STARFIVE_TIMER_CH_##x) * STARFIVE_TIMER_CH_LEN)

#define STARFIVE_CLOCK_SOURCE_RATING		200
#define STARFIVE_VALID_BITS			32
#define STARFIVE_DELAY_US			0
#define STARFIVE_TIMEOUT_US			10000
#define STARFIVE_CLOCKEVENT_RATING		300
#define STARFIVE_TIMER_MAX_TICKS		0xffffffff
#define STARFIVE_TIMER_MIN_TICKS		0xf

#define STARFIVE_TIMER_JH7110_INT_STATUS	0x00 /* RO[0:4]: Interrupt Status for channel0~4 */
#define STARFIVE_TIMER_JH7110_CTL		0x04 /* RW[0]: 0-continuous run, 1-single run */
#define STARFIVE_TIMER_JH7110_LOAD		0x08 /* RW: load value to counter */
#define STARFIVE_TIMER_JH7110_ENABLE		0x10 /* RW[0]: timer enable register */
#define STARFIVE_TIMER_JH7110_RELOAD		0x14 /* RW: write 1 or 0 both reload counter */
#define STARFIVE_TIMER_JH7110_VALUE		0x18 /* RO: timer value register */
#define STARFIVE_TIMER_JH7110_INT_CLR		0x20 /* RW: timer interrupt clear register */
#define STARFIVE_TIMER_JH7110_INT_MASK		0x24 /* RW[0]: timer interrupt mask register */
#define STARFIVE_TIMER_JH7110_INT_CLR_AVA_MASK	BIT(1)

enum STARFIVE_TIMER_CH {
	STARFIVE_TIMER_CH_0 = 0,
	STARFIVE_TIMER_CH_1,
	STARFIVE_TIMER_CH_2,
	STARFIVE_TIMER_CH_3,
	STARFIVE_TIMER_CH_4,
	STARFIVE_TIMER_CH_5,
	STARFIVE_TIMER_CH_6,
	STARFIVE_TIMER_CH_7,
	STARFIVE_TIMER_CH_MAX
};

enum STARFIVE_TIMER_INTMASK {
	STARFIVE_TIMER_INTMASK_DIS = 0,
	STARFIVE_TIMER_INTMASK_ENA = 1
};

enum STARFIVE_TIMER_MOD {
	STARFIVE_TIMER_MOD_CONTIN = 0,
	STARFIVE_TIMER_MOD_SINGLE = 1
};

enum STARFIVE_TIMER_CTL_EN {
	STARFIVE_TIMER_DIS = 0,
	STARFIVE_TIMER_ENA = 1
};

struct starfive_timer_chan_base {
	/* Resgister */
	unsigned int ctrl;
	unsigned int load;
	unsigned int enable;
	unsigned int reload;
	unsigned int value;
	unsigned int intclr;
	unsigned int intmask;

	unsigned int channel_num;	/* timer channel numbers */
	unsigned int channel_base[];
};

struct starfive_clkevt {
	struct clock_event_device evt;
	struct clk *clk;
	char name[20];
	int irq;
	u32 periodic;
	u32 rate;
	u32 reload_val;
	void __iomem *base;
	void __iomem *ctrl;
	void __iomem *load;
	void __iomem *enable;
	void __iomem *reload;
	void __iomem *value;
	void __iomem *intclr;
	void __iomem *intmask;
};

struct starfive_timer_priv {
	struct device *dev;
	void __iomem *base;
	struct starfive_clkevt clkevt[];
};

#endif /* __STARFIVE_TIMER_H__ */
