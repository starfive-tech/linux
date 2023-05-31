/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * StarFive JH7110 PLL Clock Generator Driver
 *
 * Copyright (C) 2023 StarFive Technology Co., Ltd.
 */

#ifndef _CLK_STARFIVE_JH7110_PLL_H_
#define _CLK_STARFIVE_JH7110_PLL_H_

#include <linux/bits.h>

/* The decimal places are counted by expanding them by a factor of STARFIVE_PLL_FRAC_PATR_SIZE */
#define STARFIVE_PLL_FRAC_PATR_SIZE		1000

#define STARFIVE_JH7110_CLK_PLL0_OUT_DACPD_OFFSET	0x18
#define STARFIVE_JH7110_CLK_PLL0_OUT_DACPD_SHIFT	24
#define STARFIVE_JH7110_CLK_PLL0_OUT_DACPD_MASK		BIT(24)
#define STARFIVE_JH7110_CLK_PLL0_OUT_DSMPD_OFFSET	0x18
#define STARFIVE_JH7110_CLK_PLL0_OUT_DSMPD_SHIFT	25
#define STARFIVE_JH7110_CLK_PLL0_OUT_DSMPD_MASK		BIT(25)
#define STARFIVE_JH7110_CLK_PLL0_OUT_FBDIV_OFFSET	0x1c
#define STARFIVE_JH7110_CLK_PLL0_OUT_FBDIV_SHIFT	0
#define STARFIVE_JH7110_CLK_PLL0_OUT_FBDIV_MASK		GENMASK(11, 0)
#define STARFIVE_JH7110_CLK_PLL0_OUT_FRAC_OFFSET	0x20
#define STARFIVE_JH7110_CLK_PLL0_OUT_FRAC_SHIFT		0
#define STARFIVE_JH7110_CLK_PLL0_OUT_FRAC_MASK		GENMASK(23, 0)
#define STARFIVE_JH7110_CLK_PLL0_OUT_POSTDIV1_OFFSET	0x20
#define STARFIVE_JH7110_CLK_PLL0_OUT_POSTDIV1_SHIFT	28
#define STARFIVE_JH7110_CLK_PLL0_OUT_POSTDIV1_MASK	GENMASK(29, 28)
#define STARFIVE_JH7110_CLK_PLL0_OUT_PREDIV_OFFSET	0x24
#define STARFIVE_JH7110_CLK_PLL0_OUT_PREDIV_SHIFT	0
#define STARFIVE_JH7110_CLK_PLL0_OUT_PREDIV_MASK	GENMASK(5, 0)

#define STARFIVE_JH7110_CLK_PLL1_OUT_DACPD_OFFSET	0x24
#define STARFIVE_JH7110_CLK_PLL1_OUT_DACPD_SHIFT	15
#define STARFIVE_JH7110_CLK_PLL1_OUT_DACPD_MASK		BIT(15)
#define STARFIVE_JH7110_CLK_PLL1_OUT_DSMPD_OFFSET	0x24
#define STARFIVE_JH7110_CLK_PLL1_OUT_DSMPD_SHIFT	16
#define STARFIVE_JH7110_CLK_PLL1_OUT_DSMPD_MASK		BIT(16)
#define STARFIVE_JH7110_CLK_PLL1_OUT_FBDIV_OFFSET	0x24
#define STARFIVE_JH7110_CLK_PLL1_OUT_FBDIV_SHIFT	17
#define STARFIVE_JH7110_CLK_PLL1_OUT_FBDIV_MASK		GENMASK(28, 17)
#define STARFIVE_JH7110_CLK_PLL1_OUT_FRAC_OFFSET	0x28
#define STARFIVE_JH7110_CLK_PLL1_OUT_FRAC_SHIFT		0
#define STARFIVE_JH7110_CLK_PLL1_OUT_FRAC_MASK		GENMASK(23, 0)
#define STARFIVE_JH7110_CLK_PLL1_OUT_POSTDIV1_OFFSET	0x28
#define STARFIVE_JH7110_CLK_PLL1_OUT_POSTDIV1_SHIFT	28
#define STARFIVE_JH7110_CLK_PLL1_OUT_POSTDIV1_MASK	GENMASK(29, 28)
#define STARFIVE_JH7110_CLK_PLL1_OUT_PREDIV_OFFSET	0x2c
#define STARFIVE_JH7110_CLK_PLL1_OUT_PREDIV_SHIFT	0
#define STARFIVE_JH7110_CLK_PLL1_OUT_PREDIV_MASK	GENMASK(5, 0)

#define STARFIVE_JH7110_CLK_PLL2_OUT_DACPD_OFFSET	0x2c
#define STARFIVE_JH7110_CLK_PLL2_OUT_DACPD_SHIFT	15
#define STARFIVE_JH7110_CLK_PLL2_OUT_DACPD_MASK		BIT(15)
#define STARFIVE_JH7110_CLK_PLL2_OUT_DSMPD_OFFSET	0x2c
#define STARFIVE_JH7110_CLK_PLL2_OUT_DSMPD_SHIFT	16
#define STARFIVE_JH7110_CLK_PLL2_OUT_DSMPD_MASK		BIT(16)
#define STARFIVE_JH7110_CLK_PLL2_OUT_FBDIV_OFFSET	0x2c
#define STARFIVE_JH7110_CLK_PLL2_OUT_FBDIV_SHIFT	17
#define STARFIVE_JH7110_CLK_PLL2_OUT_FBDIV_MASK		GENMASK(28, 17)
#define STARFIVE_JH7110_CLK_PLL2_OUT_FRAC_OFFSET	0x30
#define STARFIVE_JH7110_CLK_PLL2_OUT_FRAC_SHIFT		0
#define STARFIVE_JH7110_CLK_PLL2_OUT_FRAC_MASK		GENMASK(23, 0)
#define STARFIVE_JH7110_CLK_PLL2_OUT_POSTDIV1_OFFSET	0x30
#define STARFIVE_JH7110_CLK_PLL2_OUT_POSTDIV1_SHIFT	28
#define STARFIVE_JH7110_CLK_PLL2_OUT_POSTDIV1_MASK	GENMASK(29, 28)
#define STARFIVE_JH7110_CLK_PLL2_OUT_PREDIV_OFFSET	0x34
#define STARFIVE_JH7110_CLK_PLL2_OUT_PREDIV_SHIFT	0
#define STARFIVE_JH7110_CLK_PLL2_OUT_PREDIV_MASK	GENMASK(5, 0)

#define JH7110_PLL(_idx, _name, _nums, _val)			\
[_idx] = {							\
	.name = _name,						\
	.offsets = {						\
		.dacpd = STARFIVE_##_idx##_DACPD_OFFSET,	\
		.dsmpd = STARFIVE_##_idx##_DSMPD_OFFSET,	\
		.fbdiv = STARFIVE_##_idx##_FBDIV_OFFSET,	\
		.frac = STARFIVE_##_idx##_FRAC_OFFSET,		\
		.prediv = STARFIVE_##_idx##_PREDIV_OFFSET,	\
		.postdiv1 = STARFIVE_##_idx##_POSTDIV1_OFFSET,	\
	},							\
	.masks = {						\
		.dacpd = STARFIVE_##_idx##_DACPD_MASK,		\
		.dsmpd = STARFIVE_##_idx##_DSMPD_MASK,		\
		.fbdiv = STARFIVE_##_idx##_FBDIV_MASK,		\
		.frac = STARFIVE_##_idx##_FRAC_MASK,		\
		.prediv = STARFIVE_##_idx##_PREDIV_MASK,	\
		.postdiv1 = STARFIVE_##_idx##_POSTDIV1_MASK,	\
	},							\
	.shifts = {						\
		.dacpd = STARFIVE_##_idx##_DACPD_SHIFT,		\
		.dsmpd = STARFIVE_##_idx##_DSMPD_SHIFT,		\
		.fbdiv = STARFIVE_##_idx##_FBDIV_SHIFT,		\
		.frac = STARFIVE_##_idx##_FRAC_SHIFT,		\
		.prediv = STARFIVE_##_idx##_PREDIV_SHIFT,	\
		.postdiv1 = STARFIVE_##_idx##_POSTDIV1_SHIFT,	\
	},							\
	.preset_val_nums = _nums,				\
	.preset_val = _val,					\
}

struct jh7110_pll_syscon_offset {
	unsigned int dacpd;
	unsigned int dsmpd;
	unsigned int fbdiv;
	unsigned int frac;
	unsigned int prediv;
	unsigned int postdiv1;
};

struct jh7110_pll_syscon_mask {
	u32 dacpd;
	u32 dsmpd;
	u32 fbdiv;
	u32 frac;
	u32 prediv;
	u32 postdiv1;
};

struct jh7110_pll_syscon_shift {
	char dacpd;
	char dsmpd;
	char fbdiv;
	char frac;
	char prediv;
	char postdiv1;
};

struct jh7110_pll_syscon_val {
	unsigned long freq;
	u32 prediv;
	u32 fbdiv;
	u32 postdiv1;
/* Both daxpd and dsmpd set 1 while integer mode */
/* Both daxpd and dsmpd set 0 while fraction mode */
	u32 dacpd;
	u32 dsmpd;
/* frac value should be decimals multiplied by 2^24 */
	u32 frac;
};

struct jh7110_pll_syscon_conf {
	char *name;
	struct jh7110_pll_syscon_offset offsets;
	struct jh7110_pll_syscon_mask masks;
	struct jh7110_pll_syscon_shift shifts;
	unsigned int preset_val_nums;
	const struct jh7110_pll_syscon_val *preset_val;
};

struct jh7110_clk_pll_data {
	struct clk_hw hw;
	unsigned int idx;
	unsigned int freq_select_idx;
	struct jh7110_pll_syscon_conf conf;
};

struct jh7110_clk_pll_priv {
	unsigned int pll_nums;
	struct device *dev;
	struct regmap *syscon_regmap;
	struct jh7110_clk_pll_data data[];
};

enum jh7110_pll0_freq_index {
	JH7110_PLL0_FREQ_375 = 0,
	JH7110_PLL0_FREQ_500,
	JH7110_PLL0_FREQ_625,
	JH7110_PLL0_FREQ_750,
	JH7110_PLL0_FREQ_875,
	JH7110_PLL0_FREQ_1000,
	JH7110_PLL0_FREQ_1250,
	JH7110_PLL0_FREQ_1375,
	JH7110_PLL0_FREQ_1500,
	JH7110_PLL0_FREQ_MAX
};

enum jh7110_pll1_freq_index {
	JH7110_PLL1_FREQ_1066 = 0,
	JH7110_PLL1_FREQ_1200,
	JH7110_PLL1_FREQ_1400,
	JH7110_PLL1_FREQ_1600,
	JH7110_PLL1_FREQ_MAX
};

enum jh7110_pll2_freq_index {
	JH7110_PLL2_FREQ_1188 = 0,
	JH7110_PLL2_FREQ_12288,
	JH7110_PLL2_FREQ_MAX
};

/*
 * Because the pll frequency is relatively fixed,
 * it cannot be set arbitrarily, so it needs a specific configuration.
 * PLL0 frequency should be multiple of 125MHz (USB frequency).
 */
static const struct jh7110_pll_syscon_val
	jh7110_pll0_syscon_val_preset[] = {
	[JH7110_PLL0_FREQ_375] = {
		.freq = 375000000,
		.prediv = 8,
		.fbdiv = 125,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL0_FREQ_500] = {
		.freq = 500000000,
		.prediv = 6,
		.fbdiv = 125,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL0_FREQ_625] = {
		.freq = 625000000,
		.prediv = 24,
		.fbdiv = 625,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL0_FREQ_750] = {
		.freq = 750000000,
		.prediv = 4,
		.fbdiv = 125,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL0_FREQ_875] = {
		.freq = 875000000,
		.prediv = 24,
		.fbdiv = 875,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL0_FREQ_1000] = {
		.freq = 1000000000,
		.prediv = 3,
		.fbdiv = 125,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL0_FREQ_1250] = {
		.freq = 1250000000,
		.prediv = 12,
		.fbdiv = 625,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL0_FREQ_1375] = {
		.freq = 1375000000,
		.prediv = 24,
		.fbdiv = 1375,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL0_FREQ_1500] = {
		.freq = 1500000000,
		.prediv = 2,
		.fbdiv = 125,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
};

static const struct jh7110_pll_syscon_val
	jh7110_pll1_syscon_val_preset[] = {
	[JH7110_PLL1_FREQ_1066] = {
		.freq = 1066000000,
		.prediv = 12,
		.fbdiv = 533,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL1_FREQ_1200] = {
		.freq = 1200000000,
		.prediv = 1,
		.fbdiv = 50,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL1_FREQ_1400] = {
		.freq = 1400000000,
		.prediv = 6,
		.fbdiv = 350,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL1_FREQ_1600] = {
		.freq = 1600000000,
		.prediv = 3,
		.fbdiv = 200,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
};

static const struct jh7110_pll_syscon_val
	jh7110_pll2_syscon_val_preset[] = {
	[JH7110_PLL2_FREQ_1188] = {
		.freq = 1188000000,
		.prediv = 2,
		.fbdiv = 99,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
	[JH7110_PLL2_FREQ_12288] = {
		.freq = 1228800000,
		.prediv = 5,
		.fbdiv = 256,
		.postdiv1 = 1,
		.dacpd = 1,
		.dsmpd = 1,
	},
};

#endif
