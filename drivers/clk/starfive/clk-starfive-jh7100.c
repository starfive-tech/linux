// SPDX-License-Identifier: GPL-2.0
/*
 * StarFive JH7100 Clock Generator Driver
 *
 * Copyright 2021 Ahmad Fatoum, Pengutronix
 * Copyright (C) 2021 Glider bv
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/overflow.h>
#include <linux/platform_device.h>

#include <dt-bindings/clock/starfive-jh7100.h>

#define STARFIVE_CLK_ENABLE_SHIFT	31
#define STARFIVE_CLK_INVERT_SHIFT	30
#define STARFIVE_CLK_MUX_SHIFT		24

#define STARFIVE_CLK_ENABLE	BIT(31)
#define STARFIVE_CLK_INVERT	BIT(30)
#define STARFIVE_CLK_MUX_MASK	GENMASK(27, 24)
#define STARFIVE_CLK_DIV_MASK	GENMASK(23, 0)

static const char *const cpundbus_root_sels[] __initconst = {
	[0] = "osc_sys",
	[1] = "pll0_out",
	[2] = "pll1_out",
	[3] = "pll2_out",
};

static const char *const dla_root_sels[] __initconst = {
	[0] = "osc_sys",
	[1] = "pll1_out",
	[2] = "pll2_out",
};

static const char *const dsp_root_sels[] __initconst = {
	[0] = "osc_sys",
	[1] = "pll0_out",
	[2] = "pll1_out",
	[3] = "pll2_out",
};

static const char *const gmacusb_root_sels[] __initconst = {
	[0] = "osc_sys",
	[1] = "pll0_out",
	[2] = "pll2_out",
};

static const char *const perh0_root_sels[] __initconst = {
	[0] = "osc_sys",
	[1] = "pll0_out",
};

static const char *const perh1_root_sels[] __initconst = {
	[0] = "osc_sys",
	[1] = "pll2_out",
};

static const char *const vin_root_sels[] __initconst = {
	[0] = "osc_sys",
	[1] = "pll1_out",
	[2] = "pll2_out",
};

static const char *const vout_root_sels[] __initconst = {
	[0] = "osc_aud",
	[1] = "pll0_out",
	[2] = "pll2_out",
};

static const char *const cdechifi4_root_sels[] __initconst = {
	[0] = "osc_sys",
	[1] = "pll1_out",
	[2] = "pll2_out",
};

static const char *const cdec_root_sels[] __initconst = {
	[0] = "osc_sys",
	[1] = "pll0_out",
	[2] = "pll1_out",
};

static const char *const voutbus_root_sels[] __initconst = {
	[0] = "osc_aud",
	[1] = "pll0_out",
	[2] = "pll2_out",
};

static const char *const pll2_refclk_sels[] __initconst = {
	[0] = "osc_sys",
	[1] = "osc_aud",
};

static const char *const ddrc0_sels[] __initconst = {
	[0] = "ddrosc_div2",
	[1] = "ddrpll_div2",
	[2] = "ddrpll_div4",
	[3] = "ddrpll_div8",
};

static const char *const ddrc1_sels[] __initconst = {
	[0] = "ddrosc_div2",
	[1] = "ddrpll_div2",
	[2] = "ddrpll_div4",
	[3] = "ddrpll_div8",
};

static const char *const nne_bus_sels[] __initconst = {
	[0] = "cpu_axi",
	[1] = "nnebus_src1",
};

static const char *const usbphy_25m_sels[] __initconst = {
	[0] = "osc_sys",
	[1] = "usbphy_plldiv25m",
};

static const char *const gmac_tx_sels[] __initconst = {
	[0] = "gmac_gtxclk",
	[1] = "gmac_mii_txclk",
	[2] = "gmac_rmii_txclk",
};

static const char *const gmac_rx_pre_sels[] __initconst = {
	[0] = "gmac_gr_mii_rxclk",
	[1] = "gmac_rmii_rxclk",
};

struct clk_starfive_jh7100_priv {
	spinlock_t rmw_lock;
	struct device *dev;
	void __iomem *base;
	struct clk_hw_onecell_data clk_hws;
};

struct starfive_clk {
	struct clk_hw hw;
	struct clk_starfive_jh7100_priv *priv;
	unsigned int nr;
	u32 max;
};

static struct starfive_clk *starfive_clk_from(struct clk_hw *hw)
{
	return container_of(hw, struct starfive_clk, hw);
}

static u32 starfive_clk_reg_get(struct starfive_clk *clk)
{
	void __iomem *reg = clk->priv->base + 4 * clk->nr;

	return readl_relaxed(reg);
}

static void starfive_clk_reg_rmw(struct starfive_clk *clk, u32 mask, u32 value)
{
	void __iomem *reg = clk->priv->base + 4 * clk->nr;
	unsigned long flags;

	spin_lock_irqsave(&clk->priv->rmw_lock, flags);
	value |= readl_relaxed(reg) & ~mask;
	writel_relaxed(value, reg);
	spin_unlock_irqrestore(&clk->priv->rmw_lock, flags);
}

static int starfive_clk_enable(struct clk_hw *hw)
{
	struct starfive_clk *clk = starfive_clk_from(hw);

	dev_dbg(clk->priv->dev, "enable(%s)\n", __clk_get_name(clk->hw.clk));

	starfive_clk_reg_rmw(clk, STARFIVE_CLK_ENABLE, STARFIVE_CLK_ENABLE);
	return 0;
}

static void starfive_clk_disable(struct clk_hw *hw)
{
	struct starfive_clk *clk = starfive_clk_from(hw);

	dev_dbg(clk->priv->dev, "disable(%s)\n", __clk_get_name(clk->hw.clk));

	starfive_clk_reg_rmw(clk, STARFIVE_CLK_ENABLE, 0);
}

static int starfive_clk_is_enabled(struct clk_hw *hw)
{
	struct starfive_clk *clk = starfive_clk_from(hw);

	return !!(starfive_clk_reg_get(clk) & STARFIVE_CLK_ENABLE);
}

static unsigned long starfive_clk_recalc_rate(struct clk_hw *hw,
					      unsigned long parent_rate)
{
	struct starfive_clk *clk = starfive_clk_from(hw);
	u32 value = starfive_clk_reg_get(clk) & STARFIVE_CLK_DIV_MASK;
	unsigned long rate;

	if (value)
		rate = parent_rate / value;
	else
		rate = 0;

	dev_dbg(clk->priv->dev, "recalc_rate(%s, %lu) = %lu (div %u)\n",
		__clk_get_name(clk->hw.clk), parent_rate, rate, value);

	return rate;
}

static u8 starfive_clk_get_parent(struct clk_hw *hw)
{
	struct starfive_clk *clk = starfive_clk_from(hw);
	u32 value = starfive_clk_reg_get(clk);

	return (value & STARFIVE_CLK_MUX_MASK) >> STARFIVE_CLK_MUX_SHIFT;
}

static int starfive_clk_set_parent(struct clk_hw *hw, u8 index)
{
	struct starfive_clk *clk = starfive_clk_from(hw);
	u32 value = (u32)index << STARFIVE_CLK_MUX_SHIFT;

	dev_dbg(clk->priv->dev, "set_parent(%s, %u)\n",
		__clk_get_name(clk->hw.clk), index);

	starfive_clk_reg_rmw(clk, STARFIVE_CLK_MUX_MASK, value);
	return 0;
}

static int starfive_clk_mux_determine_rate(struct clk_hw *hw,
					   struct clk_rate_request *req)
{
	struct starfive_clk *clk = starfive_clk_from(hw);
	int ret = clk_mux_determine_rate_flags(&clk->hw, req, 0);

	dev_dbg(clk->priv->dev, "determine_rate(%s) = %d\n",
		__clk_get_name(clk->hw.clk), ret);

	return ret;
}

static int starfive_clk_get_phase(struct clk_hw *hw)
{
	struct starfive_clk *clk = starfive_clk_from(hw);
	u32 value = starfive_clk_reg_get(clk);

	return (value & STARFIVE_CLK_INVERT) ? 180 : 0;
}

static int starfive_clk_set_phase(struct clk_hw *hw, int degrees)
{
	struct starfive_clk *clk = starfive_clk_from(hw);
	u32 value;

	dev_dbg(clk->priv->dev, "set_phase(%s, %d)\n",
		__clk_get_name(clk->hw.clk), degrees);

	if (degrees == 0)
		value = 0;
	else if (degrees == 180)
		value = STARFIVE_CLK_INVERT;
	else
		return -EINVAL;

	starfive_clk_reg_rmw(clk, STARFIVE_CLK_INVERT, value);
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static void starfive_clk_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	static const struct debugfs_reg32 starfive_clk_reg = {
		.name = "CTRL",
		.offset = 0,
	};
	struct starfive_clk *clk = starfive_clk_from(hw);
	struct debugfs_regset32 *regset;

	regset = devm_kzalloc(clk->priv->dev, sizeof(*regset), GFP_KERNEL);
	if (!regset)
		return;

	regset->regs = &starfive_clk_reg;
	regset->nregs = 1;
	regset->base = clk->priv->base + 4 * clk->nr;

	debugfs_create_regset32("registers", 0400, dentry, regset);
}
#else
#define starfive_clk_debug_init NULL
#endif

static const struct clk_ops starfive_clk_gate_ops = {
	.enable = starfive_clk_enable,
	.disable = starfive_clk_disable,
	.is_enabled = starfive_clk_is_enabled,
	.debug_init = starfive_clk_debug_init,
};

static const struct clk_ops starfive_clk_div_ops = {
	.recalc_rate = starfive_clk_recalc_rate,
	.debug_init = starfive_clk_debug_init,
};

static const struct clk_ops starfive_clk_gdiv_ops = {
	.enable = starfive_clk_enable,
	.disable = starfive_clk_disable,
	.is_enabled = starfive_clk_is_enabled,
	.recalc_rate = starfive_clk_recalc_rate,
	.debug_init = starfive_clk_debug_init,
};

static const struct clk_ops starfive_clk_mux_ops = {
	.get_parent = starfive_clk_get_parent,
	.set_parent = starfive_clk_set_parent,
	.determine_rate = starfive_clk_mux_determine_rate,
	.debug_init = starfive_clk_debug_init,
};

static const struct clk_ops starfive_clk_gmux_ops = {
	.enable = starfive_clk_enable,
	.disable = starfive_clk_disable,
	.is_enabled = starfive_clk_is_enabled,
	.get_parent = starfive_clk_get_parent,
	.set_parent = starfive_clk_set_parent,
	.determine_rate = starfive_clk_mux_determine_rate,
	.debug_init = starfive_clk_debug_init,
};

static const struct clk_ops starfive_clk_inv_ops = {
	.get_phase = starfive_clk_get_phase,
	.set_phase = starfive_clk_set_phase,
	.debug_init = starfive_clk_debug_init,
};

#define STARFIVE_GATE(_nr, _name, _parent) [_nr] = { \
	.name = _name, \
	.ops = &starfive_clk_gate_ops, \
	.parent = _parent, \
	.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, \
	.max = 0, \
}
#define STARFIVE__DIV(_nr, _name, _parent, _max) [_nr] = { \
	.name = _name, \
	.ops = &starfive_clk_div_ops, \
	.parent = _parent, \
	.flags = 0, \
	.max = _max, \
}
#define STARFIVE_GDIV(_nr, _name, _parent, _max) [_nr] = { \
	.name = _name, \
	.ops = &starfive_clk_gdiv_ops, \
	.parent = _parent, \
	.flags = CLK_IGNORE_UNUSED, \
	.max = _max, \
}
#define STARFIVE__MUX(_nr, _name, _parents) [_nr] = { \
	.name = _name, \
	.ops = &starfive_clk_mux_ops, \
	.parents = _parents, \
	.flags = 0, \
	.max = (ARRAY_SIZE(_parents) - 1) << STARFIVE_CLK_MUX_SHIFT, \
}
#define STARFIVE_GMUX(_nr, _name, _parents) [_nr] = { \
	.name = _name, \
	.ops = &starfive_clk_gmux_ops, \
	.parents = _parents, \
	.flags = CLK_IGNORE_UNUSED, \
	.max = (ARRAY_SIZE(_parents) - 1) << STARFIVE_CLK_MUX_SHIFT, \
}
#define STARFIVE__INV(_nr, _name, _parent) [_nr] = { \
	.name = _name, \
	.ops = &starfive_clk_inv_ops, \
	.parent = _parent, \
	.flags = CLK_SET_RATE_PARENT, \
	.max = 0, \
}

static const struct {
	const char *name;
	const struct clk_ops *ops;
	union {
		const char *parent;
		const char *const *parents;
	};
	unsigned long flags;
	u32 max;
} starfive_ctrl[] __initconst = {
	STARFIVE__MUX(JH7100_CLK_CPUNDBUS_ROOT, "cpundbus_root", cpundbus_root_sels),
	STARFIVE__MUX(JH7100_CLK_DLA_ROOT, "dla_root", dla_root_sels),
	STARFIVE__MUX(JH7100_CLK_DSP_ROOT, "dsp_root", dsp_root_sels),
	STARFIVE__MUX(JH7100_CLK_GMACUSB_ROOT, "gmacusb_root", gmacusb_root_sels),
	STARFIVE__MUX(JH7100_CLK_PERH0_ROOT, "perh0_root", perh0_root_sels),
	STARFIVE__MUX(JH7100_CLK_PERH1_ROOT, "perh1_root", perh1_root_sels),
	STARFIVE__MUX(JH7100_CLK_VIN_ROOT, "vin_root", vin_root_sels),
	STARFIVE__MUX(JH7100_CLK_VOUT_ROOT, "vout_root", vout_root_sels),
	STARFIVE_GDIV(JH7100_CLK_AUDIO_ROOT, "audio_root", "pll0_out", 8),
	STARFIVE__MUX(JH7100_CLK_CDECHIFI4_ROOT, "cdechifi4_root", cdechifi4_root_sels),
	STARFIVE__MUX(JH7100_CLK_CDEC_ROOT, "cdec_root", cdec_root_sels),
	STARFIVE__MUX(JH7100_CLK_VOUTBUS_ROOT, "voutbus_root", voutbus_root_sels),
	STARFIVE__DIV(JH7100_CLK_CPUNBUS_ROOT_DIV, "cpunbus_root_div", "cpundbus_root", 2),
	STARFIVE__DIV(JH7100_CLK_DSP_ROOT_DIV, "dsp_root_div", "dsp_root", 4),
	STARFIVE__DIV(JH7100_CLK_PERH0_SRC, "perh0_src", "perh0_root", 4),
	STARFIVE__DIV(JH7100_CLK_PERH1_SRC, "perh1_src", "perh1_root", 4),
	STARFIVE_GDIV(JH7100_CLK_PLL0_TESTOUT, "pll0_testout", "perh0_src", 31),
	STARFIVE_GDIV(JH7100_CLK_PLL1_TESTOUT, "pll1_testout", "dla_root", 31),
	STARFIVE_GDIV(JH7100_CLK_PLL2_TESTOUT, "pll2_testout", "perh1_src", 31),
	STARFIVE__MUX(JH7100_CLK_PLL2_REF, "pll2_refclk", pll2_refclk_sels),
	STARFIVE__DIV(JH7100_CLK_CPU_CORE, "cpu_core", "cpunbus_root_div", 8),
	STARFIVE__DIV(JH7100_CLK_CPU_AXI, "cpu_axi", "cpu_core", 8),
	STARFIVE__DIV(JH7100_CLK_AHB_BUS, "ahb_bus", "cpunbus_root_div", 8),
	STARFIVE__DIV(JH7100_CLK_APB1_BUS, "apb1_bus", "ahb_bus", 8),
	STARFIVE__DIV(JH7100_CLK_APB2_BUS, "apb2_bus", "ahb_bus", 8),
	STARFIVE_GATE(JH7100_CLK_DOM3AHB_BUS, "dom3ahb_bus", "ahb_bus"),
	STARFIVE_GATE(JH7100_CLK_DOM7AHB_BUS, "dom7ahb_bus", "ahb_bus"),
	STARFIVE_GATE(JH7100_CLK_U74_CORE0, "u74_core0", "cpu_core"),
	STARFIVE_GDIV(JH7100_CLK_U74_CORE1, "u74_core1", "cpu_core", 8),
	STARFIVE_GATE(JH7100_CLK_U74_AXI, "u74_axi", "cpu_axi"),
	STARFIVE_GATE(JH7100_CLK_U74RTC_TOGGLE, "u74rtc_toggle", "osc_sys"),
	STARFIVE_GATE(JH7100_CLK_SGDMA2P_AXI, "sgdma2p_axi", "cpu_axi"),
	STARFIVE_GATE(JH7100_CLK_DMA2PNOC_AXI, "dma2pnoc_axi", "cpu_axi"),
	STARFIVE_GATE(JH7100_CLK_SGDMA2P_AHB, "sgdma2p_ahb", "ahb_bus"),
	STARFIVE__DIV(JH7100_CLK_DLA_BUS, "dla_bus", "dla_root", 4),
	STARFIVE_GATE(JH7100_CLK_DLA_AXI, "dla_axi", "dla_bus"),
	STARFIVE_GATE(JH7100_CLK_DLANOC_AXI, "dlanoc_axi", "dla_bus"),
	STARFIVE_GATE(JH7100_CLK_DLA_APB, "dla_apb", "apb1_bus"),
	STARFIVE_GDIV(JH7100_CLK_VP6_CORE, "vp6_core", "dsp_root_div", 4),
	STARFIVE__DIV(JH7100_CLK_VP6BUS_SRC, "vp6bus_src", "dsp_root", 4),
	STARFIVE_GDIV(JH7100_CLK_VP6_AXI, "vp6_axi", "vp6bus_src", 4),
	STARFIVE__DIV(JH7100_CLK_VCDECBUS_SRC, "vcdecbus_src", "cdechifi4_root", 4),
	STARFIVE__DIV(JH7100_CLK_VDEC_BUS, "vdec_bus", "vcdecbus_src", 8),
	STARFIVE_GATE(JH7100_CLK_VDEC_AXI, "vdec_axi", "vdec_bus"),
	STARFIVE_GATE(JH7100_CLK_VDECBRG_MAIN, "vdecbrg_mainclk", "vdec_bus"),
	STARFIVE_GDIV(JH7100_CLK_VDEC_BCLK, "vdec_bclk", "vcdecbus_src", 8),
	STARFIVE_GDIV(JH7100_CLK_VDEC_CCLK, "vdec_cclk", "cdec_root", 8),
	STARFIVE_GATE(JH7100_CLK_VDEC_APB, "vdec_apb", "apb1_bus"),
	STARFIVE_GDIV(JH7100_CLK_JPEG_AXI, "jpeg_axi", "cpunbus_root_div", 8),
	STARFIVE_GDIV(JH7100_CLK_JPEG_CCLK, "jpeg_cclk", "cpunbus_root_div", 8),
	STARFIVE_GATE(JH7100_CLK_JPEG_APB, "jpeg_apb", "apb1_bus"),
	STARFIVE_GDIV(JH7100_CLK_GC300_2X, "gc300_2x", "cdechifi4_root", 8),
	STARFIVE_GATE(JH7100_CLK_GC300_AHB, "gc300_ahb", "ahb_bus"),
	STARFIVE__DIV(JH7100_CLK_JPCGC300_AXIBUS, "jpcgc300_axibus", "vcdecbus_src", 8),
	STARFIVE_GATE(JH7100_CLK_GC300_AXI, "gc300_axi", "jpcgc300_axibus"),
	STARFIVE_GATE(JH7100_CLK_JPCGC300_MAIN, "jpcgc300_mainclk", "jpcgc300_axibus"),
	STARFIVE__DIV(JH7100_CLK_VENC_BUS, "venc_bus", "vcdecbus_src", 8),
	STARFIVE_GATE(JH7100_CLK_VENC_AXI, "venc_axi", "venc_bus"),
	STARFIVE_GATE(JH7100_CLK_VENCBRG_MAIN, "vencbrg_mainclk", "venc_bus"),
	STARFIVE_GDIV(JH7100_CLK_VENC_BCLK, "venc_bclk", "vcdecbus_src", 8),
	STARFIVE_GDIV(JH7100_CLK_VENC_CCLK, "venc_cclk", "cdec_root", 8),
	STARFIVE_GATE(JH7100_CLK_VENC_APB, "venc_apb", "apb1_bus"),
	STARFIVE_GDIV(JH7100_CLK_DDRPLL_DIV2, "ddrpll_div2", "pll1_out", 2),
	STARFIVE_GDIV(JH7100_CLK_DDRPLL_DIV4, "ddrpll_div4", "ddrpll_div2", 2),
	STARFIVE_GDIV(JH7100_CLK_DDRPLL_DIV8, "ddrpll_div8", "ddrpll_div4", 2),
	STARFIVE_GDIV(JH7100_CLK_DDROSC_DIV2, "ddrosc_div2", "osc_sys", 2),
	STARFIVE_GMUX(JH7100_CLK_DDRC0, "ddrc0", ddrc0_sels),
	STARFIVE_GMUX(JH7100_CLK_DDRC1, "ddrc1", ddrc1_sels),
	STARFIVE_GATE(JH7100_CLK_DDRPHY_APB, "ddrphy_apb", "apb1_bus"),
	STARFIVE__DIV(JH7100_CLK_NOC_ROB, "noc_rob", "cpunbus_root_div", 8),
	STARFIVE__DIV(JH7100_CLK_NOC_COG, "noc_cog", "dla_root", 8),
	STARFIVE_GATE(JH7100_CLK_NNE_AHB, "nne_ahb", "ahb_bus"),
	STARFIVE__DIV(JH7100_CLK_NNEBUS_SRC1, "nnebus_src1", "dsp_root", 4),
	STARFIVE__MUX(JH7100_CLK_NNE_BUS, "nne_bus", nne_bus_sels),
	STARFIVE_GATE(JH7100_CLK_NNE_AXI, "nne_axi", "nne_bus"),
	STARFIVE_GATE(JH7100_CLK_NNENOC_AXI, "nnenoc_axi", "nne_bus"),
	STARFIVE_GATE(JH7100_CLK_DLASLV_AXI, "dlaslv_axi", "nne_bus"),
	STARFIVE_GATE(JH7100_CLK_DSPX2C_AXI, "dspx2c_axi", "nne_bus"),
	STARFIVE__DIV(JH7100_CLK_HIFI4_SRC, "hifi4_src", "cdechifi4_root", 4),
	STARFIVE__DIV(JH7100_CLK_HIFI4_COREFREE, "hifi4_corefree", "hifi4_src", 8),
	STARFIVE_GATE(JH7100_CLK_HIFI4_CORE, "hifi4_core", "hifi4_corefree"),
	STARFIVE__DIV(JH7100_CLK_HIFI4_BUS, "hifi4_bus", "hifi4_corefree", 8),
	STARFIVE_GATE(JH7100_CLK_HIFI4_AXI, "hifi4_axi", "hifi4_bus"),
	STARFIVE_GATE(JH7100_CLK_HIFI4NOC_AXI, "hifi4noc_axi", "hifi4_bus"),
	STARFIVE__DIV(JH7100_CLK_SGDMA1P_BUS, "sgdma1p_bus", "cpunbus_root_div", 8),
	STARFIVE_GATE(JH7100_CLK_SGDMA1P_AXI, "sgdma1p_axi", "sgdma1p_bus"),
	STARFIVE_GATE(JH7100_CLK_DMA1P_AXI, "dma1p_axi", "sgdma1p_bus"),
	STARFIVE_GDIV(JH7100_CLK_X2C_AXI, "x2c_axi", "cpunbus_root_div", 8),
	STARFIVE__DIV(JH7100_CLK_USB_BUS, "usb_bus", "cpunbus_root_div", 8),
	STARFIVE_GATE(JH7100_CLK_USB_AXI, "usb_axi", "usb_bus"),
	STARFIVE_GATE(JH7100_CLK_USBNOC_AXI, "usbnoc_axi", "usb_bus"),
	STARFIVE__DIV(JH7100_CLK_USBPHY_ROOTDIV, "usbphy_rootdiv", "gmacusb_root", 4),
	STARFIVE_GDIV(JH7100_CLK_USBPHY_125M, "usbphy_125m", "usbphy_rootdiv", 8),
	STARFIVE_GDIV(JH7100_CLK_USBPHY_PLLDIV25M, "usbphy_plldiv25m", "usbphy_rootdiv", 32),
	STARFIVE__MUX(JH7100_CLK_USBPHY_25M, "usbphy_25m", usbphy_25m_sels),
	STARFIVE__DIV(JH7100_CLK_AUDIO_DIV, "audio_div", "audio_root", 131072),
	STARFIVE_GATE(JH7100_CLK_AUDIO_SRC, "audio_src", "audio_div"),
	STARFIVE_GATE(JH7100_CLK_AUDIO_12288, "audio_12288", "osc_aud"),
	STARFIVE_GDIV(JH7100_CLK_VIN_SRC, "vin_src", "vin_root", 4),
	STARFIVE__DIV(JH7100_CLK_ISP0_BUS, "isp0_bus", "vin_src", 8),
	STARFIVE_GATE(JH7100_CLK_ISP0_AXI, "isp0_axi", "isp0_bus"),
	STARFIVE_GATE(JH7100_CLK_ISP0NOC_AXI, "isp0noc_axi", "isp0_bus"),
	STARFIVE_GATE(JH7100_CLK_ISPSLV_AXI, "ispslv_axi", "isp0_bus"),
	STARFIVE__DIV(JH7100_CLK_ISP1_BUS, "isp1_bus", "vin_src", 8),
	STARFIVE_GATE(JH7100_CLK_ISP1_AXI, "isp1_axi", "isp1_bus"),
	STARFIVE_GATE(JH7100_CLK_ISP1NOC_AXI, "isp1noc_axi", "isp1_bus"),
	STARFIVE__DIV(JH7100_CLK_VIN_BUS, "vin_bus", "vin_src", 8),
	STARFIVE_GATE(JH7100_CLK_VIN_AXI, "vin_axi", "vin_bus"),
	STARFIVE_GATE(JH7100_CLK_VINNOC_AXI, "vinnoc_axi", "vin_bus"),
	STARFIVE_GDIV(JH7100_CLK_VOUT_SRC, "vout_src", "vout_root", 4),
	STARFIVE__DIV(JH7100_CLK_DISPBUS_SRC, "dispbus_src", "voutbus_root", 4),
	STARFIVE__DIV(JH7100_CLK_DISP_BUS, "disp_bus", "dispbus_src", 4),
	STARFIVE_GATE(JH7100_CLK_DISP_AXI, "disp_axi", "disp_bus"),
	STARFIVE_GATE(JH7100_CLK_DISPNOC_AXI, "dispnoc_axi", "disp_bus"),
	STARFIVE_GATE(JH7100_CLK_SDIO0_AHB, "sdio0_ahb", "ahb_bus"),
	STARFIVE_GDIV(JH7100_CLK_SDIO0_CCLKINT, "sdio0_cclkint", "perh0_src", 24),
	STARFIVE__INV(JH7100_CLK_SDIO0_CCLKINT_INV, "sdio0_cclkint_inv", "sdio0_cclkint"),
	STARFIVE_GATE(JH7100_CLK_SDIO1_AHB, "sdio1_ahb", "ahb_bus"),
	STARFIVE_GDIV(JH7100_CLK_SDIO1_CCLKINT, "sdio1_cclkint", "perh1_src", 24),
	STARFIVE__INV(JH7100_CLK_SDIO1_CCLKINT_INV, "sdio1_cclkint_inv", "sdio1_cclkint"),
	STARFIVE_GATE(JH7100_CLK_GMAC_AHB, "gmac_ahb", "ahb_bus"),
	STARFIVE__DIV(JH7100_CLK_GMAC_ROOT_DIV, "gmac_root_div", "gmacusb_root", 8),
	STARFIVE_GDIV(JH7100_CLK_GMAC_PTP_REF, "gmac_ptp_refclk", "gmac_root_div", 31),
	STARFIVE_GDIV(JH7100_CLK_GMAC_GTX, "gmac_gtxclk", "gmac_root_div", 255),
	STARFIVE_GDIV(JH7100_CLK_GMAC_RMII_TX, "gmac_rmii_txclk", "gmac_rmii_ref", 8),
	STARFIVE_GDIV(JH7100_CLK_GMAC_RMII_RX, "gmac_rmii_rxclk", "gmac_rmii_ref", 8),
	STARFIVE__MUX(JH7100_CLK_GMAC_TX, "gmac_tx", gmac_tx_sels),
	STARFIVE__INV(JH7100_CLK_GMAC_TX_INV, "gmac_tx_inv", "gmac_tx"),
	STARFIVE__MUX(JH7100_CLK_GMAC_RX_PRE, "gmac_rx_pre", gmac_rx_pre_sels),
	STARFIVE__INV(JH7100_CLK_GMAC_RX_INV, "gmac_rx_inv", "gmac_rx_dly"),
	STARFIVE_GATE(JH7100_CLK_GMAC_RMII, "gmac_rmii", "gmac_rmii_ref"),
	STARFIVE_GDIV(JH7100_CLK_GMAC_TOPHYREF, "gmac_tophyref", "gmac_root_div", 127),
	STARFIVE_GATE(JH7100_CLK_SPI2AHB_AHB, "spi2ahb_ahb", "ahb_bus"),
	STARFIVE_GDIV(JH7100_CLK_SPI2AHB_CORE, "spi2ahb_core", "perh0_src", 31),
	STARFIVE_GATE(JH7100_CLK_EZMASTER_AHB, "ezmaster_ahb", "ahb_bus"),
	STARFIVE_GATE(JH7100_CLK_E24_AHB, "e24_ahb", "ahb_bus"),
	STARFIVE_GATE(JH7100_CLK_E24RTC_TOGGLE, "e24rtc_toggle", "osc_sys"),
	STARFIVE_GATE(JH7100_CLK_QSPI_AHB, "qspi_ahb", "ahb_bus"),
	STARFIVE_GATE(JH7100_CLK_QSPI_APB, "qspi_apb", "apb1_bus"),
	STARFIVE_GDIV(JH7100_CLK_QSPI_REF, "qspi_refclk", "perh0_src", 31),
	STARFIVE_GATE(JH7100_CLK_SEC_AHB, "sec_ahb", "ahb_bus"),
	STARFIVE_GATE(JH7100_CLK_AES, "aes_clk", "sec_ahb"),
	STARFIVE_GATE(JH7100_CLK_SHA, "sha_clk", "sec_ahb"),
	STARFIVE_GATE(JH7100_CLK_PKA, "pka_clk", "sec_ahb"),
	STARFIVE_GATE(JH7100_CLK_TRNG_APB, "trng_apb", "apb1_bus"),
	STARFIVE_GATE(JH7100_CLK_OTP_APB, "otp_apb", "apb1_bus"),
	STARFIVE_GATE(JH7100_CLK_UART0_APB, "uart0_apb", "apb1_bus"),
	STARFIVE_GDIV(JH7100_CLK_UART0_CORE, "uart0_core", "perh1_src", 63),
	STARFIVE_GATE(JH7100_CLK_UART1_APB, "uart1_apb", "apb1_bus"),
	STARFIVE_GDIV(JH7100_CLK_UART1_CORE, "uart1_core", "perh1_src", 63),
	STARFIVE_GATE(JH7100_CLK_SPI0_APB, "spi0_apb", "apb1_bus"),
	STARFIVE_GDIV(JH7100_CLK_SPI0_CORE, "spi0_core", "perh1_src", 63),
	STARFIVE_GATE(JH7100_CLK_SPI1_APB, "spi1_apb", "apb1_bus"),
	STARFIVE_GDIV(JH7100_CLK_SPI1_CORE, "spi1_core", "perh1_src", 63),
	STARFIVE_GATE(JH7100_CLK_I2C0_APB, "i2c0_apb", "apb1_bus"),
	STARFIVE_GDIV(JH7100_CLK_I2C0_CORE, "i2c0_core", "perh1_src", 63),
	STARFIVE_GATE(JH7100_CLK_I2C1_APB, "i2c1_apb", "apb1_bus"),
	STARFIVE_GDIV(JH7100_CLK_I2C1_CORE, "i2c1_core", "perh1_src", 63),
	STARFIVE_GATE(JH7100_CLK_GPIO_APB, "gpio_apb", "apb1_bus"),
	STARFIVE_GATE(JH7100_CLK_UART2_APB, "uart2_apb", "apb2_bus"),
	STARFIVE_GDIV(JH7100_CLK_UART2_CORE, "uart2_core", "perh0_src", 63),
	STARFIVE_GATE(JH7100_CLK_UART3_APB, "uart3_apb", "apb2_bus"),
	STARFIVE_GDIV(JH7100_CLK_UART3_CORE, "uart3_core", "perh0_src", 63),
	STARFIVE_GATE(JH7100_CLK_SPI2_APB, "spi2_apb", "apb2_bus"),
	STARFIVE_GDIV(JH7100_CLK_SPI2_CORE, "spi2_core", "perh0_src", 63),
	STARFIVE_GATE(JH7100_CLK_SPI3_APB, "spi3_apb", "apb2_bus"),
	STARFIVE_GDIV(JH7100_CLK_SPI3_CORE, "spi3_core", "perh0_src", 63),
	STARFIVE_GATE(JH7100_CLK_I2C2_APB, "i2c2_apb", "apb2_bus"),
	STARFIVE_GDIV(JH7100_CLK_I2C2_CORE, "i2c2_core", "perh0_src", 63),
	STARFIVE_GATE(JH7100_CLK_I2C3_APB, "i2c3_apb", "apb2_bus"),
	STARFIVE_GDIV(JH7100_CLK_I2C3_CORE, "i2c3_core", "perh0_src", 63),
	STARFIVE_GATE(JH7100_CLK_WDTIMER_APB, "wdtimer_apb", "apb2_bus"),
	STARFIVE_GDIV(JH7100_CLK_WDT_CORE, "wdt_coreclk", "perh0_src", 63),
	STARFIVE_GDIV(JH7100_CLK_TIMER0_CORE, "timer0_coreclk", "perh0_src", 63),
	STARFIVE_GDIV(JH7100_CLK_TIMER1_CORE, "timer1_coreclk", "perh0_src", 63),
	STARFIVE_GDIV(JH7100_CLK_TIMER2_CORE, "timer2_coreclk", "perh0_src", 63),
	STARFIVE_GDIV(JH7100_CLK_TIMER3_CORE, "timer3_coreclk", "perh0_src", 63),
	STARFIVE_GDIV(JH7100_CLK_TIMER4_CORE, "timer4_coreclk", "perh0_src", 63),
	STARFIVE_GDIV(JH7100_CLK_TIMER5_CORE, "timer5_coreclk", "perh0_src", 63),
	STARFIVE_GDIV(JH7100_CLK_TIMER6_CORE, "timer6_coreclk", "perh0_src", 63),
	STARFIVE_GATE(JH7100_CLK_VP6INTC_APB, "vp6intc_apb", "apb2_bus"),
	STARFIVE_GATE(JH7100_CLK_PWM_APB, "pwm_apb", "apb2_bus"),
	STARFIVE_GATE(JH7100_CLK_MSI_APB, "msi_apb", "apb2_bus"),
	STARFIVE_GATE(JH7100_CLK_TEMP_APB, "temp_apb", "apb2_bus"),
	STARFIVE_GDIV(JH7100_CLK_TEMP_SENSE, "temp_sense", "osc_sys", 31),
	STARFIVE_GATE(JH7100_CLK_SYSERR_APB, "syserr_apb", "apb2_bus"),
};

static void __init starfive_clkgen_destroy(struct clk_starfive_jh7100_priv *priv,
					   unsigned int nr)
{
	while (nr > 0) {
		struct starfive_clk *clk = starfive_clk_from(priv->clk_hws.hws[--nr]);

		clk_hw_unregister(&clk->hw);
		devm_kfree(priv->dev, clk);
	}
}

static int __init starfive_clkgen_init(struct clk_starfive_jh7100_priv *priv)
{
	unsigned int nr = 0;
	int ret;

	priv->clk_hws.hws[JH7100_CLK_PLL0_OUT] =
		devm_clk_hw_register_fixed_factor(priv->dev, "pll0_out", "osc_sys",
						  0, 40, 1);
	priv->clk_hws.hws[JH7100_CLK_PLL1_OUT] =
		devm_clk_hw_register_fixed_factor(priv->dev, "pll1_out", "osc_sys",
						  0, 64, 1);
	priv->clk_hws.hws[JH7100_CLK_PLL2_OUT] =
		devm_clk_hw_register_fixed_factor(priv->dev, "pll2_out", "pll2_refclk",
						  0, 55, 1);

	for (; nr < ARRAY_SIZE(starfive_ctrl); nr++) {
		struct clk_init_data init = {
			.name = starfive_ctrl[nr].name,
			.ops = starfive_ctrl[nr].ops,
			.num_parents = (starfive_ctrl[nr].max >> STARFIVE_CLK_MUX_SHIFT) + 1,
			.flags = starfive_ctrl[nr].flags,
		};
		struct starfive_clk *clk;

		if (init.num_parents > 1)
			init.parent_names = starfive_ctrl[nr].parents;
		else
			init.parent_names = &starfive_ctrl[nr].parent;

		clk = devm_kzalloc(priv->dev, sizeof(*clk), GFP_KERNEL);
		if (!clk) {
			ret = -ENOMEM;
			goto err;
		}

		clk->hw.init = &init;
		clk->priv = priv;
		clk->nr = nr;
		clk->max = starfive_ctrl[nr].max;

		ret = clk_hw_register(priv->dev, &clk->hw);
		if (ret) {
			devm_kfree(priv->dev, clk);
			goto err;
		}

		priv->clk_hws.hws[nr] = &clk->hw;
	}

	return 0;
err:
	starfive_clkgen_destroy(priv, nr);
	return ret;
}

static int __init clk_starfive_jh7100_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct clk_starfive_jh7100_priv *priv;
	int ret;

	priv = devm_kzalloc(dev, struct_size(priv, clk_hws.hws, JH7100_CLK_END), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	priv->clk_hws.num = JH7100_CLK_END;
	spin_lock_init(&priv->rmw_lock);

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	ret = starfive_clkgen_init(priv);
	if (ret)
		return ret;

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					    &priv->clk_hws);
	if (ret) {
		starfive_clkgen_destroy(priv, ARRAY_SIZE(starfive_ctrl));
		return ret;
	}

	return 0;
}

static const struct of_device_id clk_starfive_jh7100_match[] = {
	{ .compatible = "starfive,jh7100-clkgen" },
	{ /* sentinel */ }
};

static struct platform_driver clk_starfive_jh7100_driver = {
	.driver = {
		.name = "clk-starfive-jh7100",
		.of_match_table = clk_starfive_jh7100_match,
	},
};

static int __init clk_starfive_jh7100_init(void)
{
	return platform_driver_probe(&clk_starfive_jh7100_driver,
				     clk_starfive_jh7100_probe);
}

subsys_initcall(clk_starfive_jh7100_init);

MODULE_DESCRIPTION("StarFive JH7100 Clock Generator Driver");
MODULE_AUTHOR("Geert Uytterhoeven <geert@linux-m68k.org>");
MODULE_LICENSE("GPL v2");
