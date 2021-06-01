// SPDX-License-Identifier: GPL-2.0
/*
 * StarFive JH7100 Clock Generator Driver
 *
 * Copyright 2021 Ahmad Fatoum, Pengutronix
 * Copyright (C) 2021 Glider bv
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/overflow.h>
#include <linux/platform_device.h>

#include <dt-bindings/clock/starfive-jh7100.h>

#define STARFIVE_CLK_ENABLE_SHIFT	31
#define STARFIVE_CLK_INVERT_SHIFT	30
#define STARFIVE_CLK_MUX_SHIFT		24


static const char *cpundbus_root_sels[4] = {
	[0] = "osc_sys",
	[1] = "pll0_out",
	[2] = "pll1_out",
	[3] = "pll2_out",
};

static const char *dla_root_sels[4] = {
	[0] = "osc_sys",
	[1] = "pll1_out",
	[2] = "pll2_out",
	[3] = "dummy",
};

static const char *dsp_root_sels[4] = {
	[0] = "osc_sys",
	[1] = "pll0_out",
	[2] = "pll1_out",
	[3] = "pll2_out",
};

static const char *gmacusb_root_sels[4] = {
	[0] = "osc_sys",
	[1] = "pll0_out",
	[2] = "pll2_out",
	[3] = "dummy",
};

static const char *perh0_root_sels[2] = {
	[0] = "osc_sys",
	[1] = "pll0_out",
};

static const char *perh1_root_sels[2] = {
	[0] = "osc_sys",
	[1] = "pll2_out",
};

static const char *vin_root_sels[4] = {
	[0] = "osc_sys",
	[1] = "pll1_out",
	[2] = "pll2_out",
	[3] = "dummy",
};

static const char *vout_root_sels[4] = {
	[0] = "osc_aud",
	[1] = "pll0_out",
	[2] = "pll2_out",
	[3] = "dummy",
};

static const char *cdechifi4_root_sels[4] = {
	[0] = "osc_sys",
	[1] = "pll1_out",
	[2] = "pll2_out",
	[3] = "dummy",
};

static const char *cdec_root_sels[4] = {
	[0] = "osc_sys",
	[1] = "pll0_out",
	[2] = "pll1_out",
	[3] = "dummy",
};

static const char *voutbus_root_sels[4] = {
	[0] = "osc_aud",
	[1] = "pll0_out",
	[2] = "pll2_out",
	[3] = "dummy",
};

static const char *pll2_refclk_sels[2] = {
	[0] = "osc_sys",
	[1] = "osc_aud",
};

static const char *ddrc0_sels[4] = {
	[0] = "ddrosc_div2",
	[1] = "ddrpll_div2",
	[2] = "ddrpll_div4",
	[3] = "ddrpll_div8",
};

static const char *ddrc1_sels[4] = {
	[0] = "ddrosc_div2",
	[1] = "ddrpll_div2",
	[2] = "ddrpll_div4",
	[3] = "ddrpll_div8",
};

static const char *nne_bus_sels[2] = {
	[0] = "cpu_axi",
	[1] = "nnebus_src1",
};

static const char *usbphy_25m_sels[2] = {
	[0] = "osc_sys",
	[1] = "usbphy_plldiv25m",
};

static const char *gmac_tx_sels[4] = {
	[0] = "gmac_gtxclk",
	[1] = "gmac_mii_txclk",
	[2] = "gmac_rmii_txclk",
	[3] = "dummy",
};

static const char *gmac_rx_pre_sels[2] = {
	[0] = "gmac_gr_mii_rxclk",
	[1] = "gmac_rmii_rxclk",
};

struct clk_starfive_jh7100_priv {
	spinlock_t rmw_lock;
	struct device *dev;
	void __iomem *base;
	struct clk_hw_onecell_data clk_hws;
};

/* assume osc_sys as direct parent for clocks of yet unknown lineage */
#define UNKNOWN "osc_sys"

static struct clk_hw * __init starfive_clk_underspecifid(struct clk_starfive_jh7100_priv *priv,
							 const char *name,
							 const char *parent)
{
	/*
	 * TODO With documentation available, all users of this functions can be
	 * migrated to one of the above or to a clk_fixed_factor with
	 * appropriate factor
	 */
	return devm_clk_hw_register_fixed_factor(priv->dev, name, parent, 0, 1,
						 1);
}

static struct clk_hw * __init starfive_clk_pll_mult(struct clk_starfive_jh7100_priv *priv,
						    const char *name,
						    const char *parent,
						    unsigned int mult)
{
	/*
	 * TODO With documentation available, all users of this functions can be
	 * migrated to one of the above or to a clk_fixed_factor with
	 * appropriate factor
	 */
	return devm_clk_hw_register_fixed_factor(priv->dev, name, parent, 0,
						 mult, 1);
}

static struct clk_hw * __init starfive_clk_divider(struct clk_starfive_jh7100_priv *priv,
						   const char *name,
						   const char *parent,
						   unsigned int offset,
						   unsigned int width)
{
	return starfive_clk_underspecifid(priv, name, parent);
}

static struct clk_hw * __init starfive_clk_gate(struct clk_starfive_jh7100_priv *priv,
						const char *name,
						const char *parent,
						unsigned int offset)
{
	// FIXME devm
	return clk_hw_register_gate(priv->dev, name, parent,
				    CLK_SET_RATE_PARENT, priv->base + offset,
				    STARFIVE_CLK_ENABLE_SHIFT, 0,
				    &priv->rmw_lock);
}

static struct clk_hw * __init starfive_clk_gated_divider(struct clk_starfive_jh7100_priv *priv,
							 const char *name,
							 const char *parent,
							 unsigned int offset,
							 unsigned int width)
{
	/* TODO divider part */
	return starfive_clk_gate(priv, name, parent, offset);
}

static struct clk_hw * __init starfive_clk_gate_dis(struct clk_starfive_jh7100_priv *priv,
						    const char *name,
						    const char *parent,
						    unsigned int offset)
{
	// FIXME devm
	return clk_hw_register_gate(priv->dev, name, parent,
				    CLK_SET_RATE_PARENT, priv->base + offset,
				    STARFIVE_CLK_INVERT_SHIFT,
				    CLK_GATE_SET_TO_DISABLE, &priv->rmw_lock);
}

static struct clk_hw * __init starfive_clk_mux(struct clk_starfive_jh7100_priv *priv,
					       const char *name,
					       unsigned int offset,
					       unsigned int width,
					       const char * const *parents,
					       unsigned int num_parents)
{
	return devm_clk_hw_register_mux(priv->dev, name, parents, num_parents,
					0, priv->base + offset,
					STARFIVE_CLK_MUX_SHIFT, width, 0,
					&priv->rmw_lock);
}

static int __init starfive_clkgen_init(struct clk_starfive_jh7100_priv *priv)
{
	struct clk_hw **hws = priv->clk_hws.hws;
	struct clk *osc_sys, *osc_aud;

	osc_sys = devm_clk_get(priv->dev, "osc_sys");
	if (IS_ERR(osc_sys))
		return PTR_ERR(osc_sys);

	osc_aud = devm_clk_get(priv->dev, "osc_aud");
	if (IS_ERR(osc_aud))
		return PTR_ERR(osc_aud);

	hws[JH7100_CLK_OSC_SYS]			= __clk_get_hw(osc_sys);
	hws[JH7100_CLK_OSC_AUD]			= __clk_get_hw(osc_aud);

	hws[JH7100_CLK_PLL0_OUT]		= starfive_clk_pll_mult(priv, "pll0_out", "osc_sys", 40);
	hws[JH7100_CLK_PLL1_OUT]		= starfive_clk_pll_mult(priv, "pll1_out", "osc_sys", 64);
	hws[JH7100_CLK_PLL2_OUT]		= starfive_clk_pll_mult(priv, "pll2_out", "pll2_refclk", 55);
	hws[JH7100_CLK_CPUNDBUS_ROOT]		= starfive_clk_mux(priv, "cpundbus_root", 0x0, 2, cpundbus_root_sels, ARRAY_SIZE(cpundbus_root_sels));
	hws[JH7100_CLK_DLA_ROOT]		= starfive_clk_mux(priv, "dla_root",	0x4, 2, dla_root_sels, ARRAY_SIZE(dla_root_sels));
	hws[JH7100_CLK_DSP_ROOT]		= starfive_clk_mux(priv, "dsp_root",	0x8, 2, dsp_root_sels, ARRAY_SIZE(dsp_root_sels));
	hws[JH7100_CLK_GMACUSB_ROOT]		= starfive_clk_mux(priv, "gmacusb_root",	0xc, 2, gmacusb_root_sels, ARRAY_SIZE(gmacusb_root_sels));
	hws[JH7100_CLK_PERH0_ROOT]		= starfive_clk_mux(priv, "perh0_root",	0x10, 1, perh0_root_sels, ARRAY_SIZE(perh0_root_sels));
	hws[JH7100_CLK_PERH1_ROOT]		= starfive_clk_mux(priv, "perh1_root",	0x14, 1, perh1_root_sels, ARRAY_SIZE(perh1_root_sels));
	hws[JH7100_CLK_VIN_ROOT]		= starfive_clk_mux(priv, "vin_root",	0x18, 2, vin_root_sels, ARRAY_SIZE(vin_root_sels));
	hws[JH7100_CLK_VOUT_ROOT]		= starfive_clk_mux(priv, "vout_root",	0x1c, 2, vout_root_sels, ARRAY_SIZE(vout_root_sels));
	hws[JH7100_CLK_AUDIO_ROOT]		= starfive_clk_gated_divider(priv, "audio_root",		UNKNOWN,	0x20, 4);
	hws[JH7100_CLK_CDECHIFI4_ROOT]		= starfive_clk_mux(priv, "cdechifi4_root",	0x24, 2, cdechifi4_root_sels, ARRAY_SIZE(cdechifi4_root_sels));
	hws[JH7100_CLK_CDEC_ROOT]		= starfive_clk_mux(priv, "cdec_root",	0x28, 2, cdec_root_sels, ARRAY_SIZE(cdec_root_sels));
	hws[JH7100_CLK_VOUTBUS_ROOT]		= starfive_clk_mux(priv, "voutbus_root",	0x2c, 2, voutbus_root_sels, ARRAY_SIZE(voutbus_root_sels));
	hws[JH7100_CLK_CPUNBUS_ROOT_DIV]	= starfive_clk_divider(priv, "cpunbus_root_div",		"cpunbus_root",	0x30, 2);
	hws[JH7100_CLK_DSP_ROOT_DIV]		= starfive_clk_divider(priv, "dsp_root_div",		"dsp_root",	0x34, 3);
	hws[JH7100_CLK_PERH0_SRC]		= starfive_clk_divider(priv, "perh0_src",		"perh0_root",	0x38, 3);
	hws[JH7100_CLK_PERH1_SRC]		= starfive_clk_divider(priv, "perh1_src",		"perh1_root",	0x3c, 3);
	hws[JH7100_CLK_PLL0_TESTOUT]		= starfive_clk_gated_divider(priv, "pll0_testout",		"pll0_out",	0x40, 5);
	hws[JH7100_CLK_PLL1_TESTOUT]		= starfive_clk_gated_divider(priv, "pll1_testout",		"pll1_out",	0x44, 5);
	hws[JH7100_CLK_PLL2_TESTOUT]		= starfive_clk_gated_divider(priv, "pll2_testout",		"pll2_out",	0x48, 5);
	hws[JH7100_CLK_PLL2_REF]		= starfive_clk_mux(priv, "pll2_refclk",	0x4c, 1, pll2_refclk_sels, ARRAY_SIZE(pll2_refclk_sels));
	hws[JH7100_CLK_CPU_CORE]		= starfive_clk_divider(priv, "cpu_core",		UNKNOWN,	0x50, 4);
	hws[JH7100_CLK_CPU_AXI]			= starfive_clk_divider(priv, "cpu_axi",		UNKNOWN,	0x54, 4);
	hws[JH7100_CLK_AHB_BUS]			= starfive_clk_divider(priv, "ahb_bus",		UNKNOWN,	0x58, 4);
	hws[JH7100_CLK_APB1_BUS]		= starfive_clk_divider(priv, "apb1_bus",		UNKNOWN,	0x5c, 4);
	hws[JH7100_CLK_APB2_BUS]		= starfive_clk_divider(priv, "apb2_bus",		UNKNOWN,	0x60, 4);
	hws[JH7100_CLK_DOM3AHB_BUS]		= starfive_clk_gate(priv, "dom3ahb_bus",		UNKNOWN,	0x64);
	hws[JH7100_CLK_DOM7AHB_BUS]		= starfive_clk_gate(priv, "dom7ahb_bus",		UNKNOWN,	0x68);
	hws[JH7100_CLK_U74_CORE0]		= starfive_clk_gate(priv, "u74_core0",		UNKNOWN,	0x6c);
	hws[JH7100_CLK_U74_CORE1]		= starfive_clk_gated_divider(priv, "u74_core1",		UNKNOWN,	0x70, 4);
	hws[JH7100_CLK_U74_AXI]			= starfive_clk_gate(priv, "u74_axi",		UNKNOWN,	0x74);
	hws[JH7100_CLK_U74RTC_TOGGLE]		= starfive_clk_gate(priv, "u74rtc_toggle",		UNKNOWN,	0x78);
	hws[JH7100_CLK_SGDMA2P_AXI]		= starfive_clk_gate(priv, "sgdma2p_axi",		UNKNOWN,	0x7c);
	hws[JH7100_CLK_DMA2PNOC_AXI]		= starfive_clk_gate(priv, "dma2pnoc_axi",		UNKNOWN,	0x80);
	hws[JH7100_CLK_SGDMA2P_AHB]		= starfive_clk_gate(priv, "sgdma2p_ahb",		UNKNOWN,	0x84);
	hws[JH7100_CLK_DLA_BUS]			= starfive_clk_divider(priv, "dla_bus",		UNKNOWN,	0x88, 3);
	hws[JH7100_CLK_DLA_AXI]			= starfive_clk_gate(priv, "dla_axi",		UNKNOWN,	0x8c);
	hws[JH7100_CLK_DLANOC_AXI]		= starfive_clk_gate(priv, "dlanoc_axi",		UNKNOWN,	0x90);
	hws[JH7100_CLK_DLA_APB]			= starfive_clk_gate(priv, "dla_apb",		UNKNOWN,	0x94);
	hws[JH7100_CLK_VP6_CORE]		= starfive_clk_gated_divider(priv, "vp6_core",		UNKNOWN,	0x98, 3);
	hws[JH7100_CLK_VP6BUS_SRC]		= starfive_clk_divider(priv, "vp6bus_src",		UNKNOWN,	0x9c, 3);
	hws[JH7100_CLK_VP6_AXI]			= starfive_clk_gated_divider(priv, "vp6_axi",		UNKNOWN,	0xa0, 3);
	hws[JH7100_CLK_VCDECBUS_SRC]		= starfive_clk_divider(priv, "vcdecbus_src",		UNKNOWN,	0xa4, 3);
	hws[JH7100_CLK_VDEC_BUS]		= starfive_clk_divider(priv, "vdec_bus",		UNKNOWN,	0xa8, 4);
	hws[JH7100_CLK_VDEC_AXI]		= starfive_clk_gate(priv, "vdec_axi",		UNKNOWN,	0xac);
	hws[JH7100_CLK_VDECBRG_MAIN]		= starfive_clk_gate(priv, "vdecbrg_mainclk",		UNKNOWN,	0xb0);
	hws[JH7100_CLK_VDEC_BCLK]		= starfive_clk_gated_divider(priv, "vdec_bclk",		UNKNOWN,	0xb4, 4);
	hws[JH7100_CLK_VDEC_CCLK]		= starfive_clk_gated_divider(priv, "vdec_cclk",		UNKNOWN,	0xb8, 4);
	hws[JH7100_CLK_VDEC_APB]		= starfive_clk_gate(priv, "vdec_apb",		UNKNOWN,	0xbc);
	hws[JH7100_CLK_JPEG_AXI]		= starfive_clk_gated_divider(priv, "jpeg_axi",		UNKNOWN,	0xc0, 4);
	hws[JH7100_CLK_JPEG_CCLK]		= starfive_clk_gated_divider(priv, "jpeg_cclk",		UNKNOWN,	0xc4, 4);
	hws[JH7100_CLK_JPEG_APB]		= starfive_clk_gate(priv, "jpeg_apb",		UNKNOWN,	0xc8);
	hws[JH7100_CLK_GC300_2X]		= starfive_clk_gated_divider(priv, "gc300_2x",		UNKNOWN,	0xcc, 4);
	hws[JH7100_CLK_GC300_AHB]		= starfive_clk_gate(priv, "gc300_ahb",		UNKNOWN,	0xd0);
	hws[JH7100_CLK_JPCGC300_AXIBUS]		= starfive_clk_divider(priv, "jpcgc300_axibus",		UNKNOWN,	0xd4, 4);
	hws[JH7100_CLK_GC300_AXI]		= starfive_clk_gate(priv, "gc300_axi",		UNKNOWN,	0xd8);
	hws[JH7100_CLK_JPCGC300_MAIN]		= starfive_clk_gate(priv, "jpcgc300_mainclk",		UNKNOWN,	0xdc);
	hws[JH7100_CLK_VENC_BUS]		= starfive_clk_divider(priv, "venc_bus",		UNKNOWN,	0xe0, 4);
	hws[JH7100_CLK_VENC_AXI]		= starfive_clk_gate(priv, "venc_axi",		UNKNOWN,	0xe4);
	hws[JH7100_CLK_VENCBRG_MAIN]		= starfive_clk_gate(priv, "vencbrg_mainclk",		UNKNOWN,	0xe8);
	hws[JH7100_CLK_VENC_BCLK]		= starfive_clk_gated_divider(priv, "venc_bclk",		UNKNOWN,	0xec, 4);
	hws[JH7100_CLK_VENC_CCLK]		= starfive_clk_gated_divider(priv, "venc_cclk",		UNKNOWN,	0xf0, 4);
	hws[JH7100_CLK_VENC_APB]		= starfive_clk_gate(priv, "venc_apb",		UNKNOWN,	0xf4);
	hws[JH7100_CLK_DDRPLL_DIV2]		= starfive_clk_gated_divider(priv, "ddrpll_div2",		UNKNOWN,	0xf8, 2);
	hws[JH7100_CLK_DDRPLL_DIV4]		= starfive_clk_gated_divider(priv, "ddrpll_div4",		UNKNOWN,	0xfc, 2);
	hws[JH7100_CLK_DDRPLL_DIV8]		= starfive_clk_gated_divider(priv, "ddrpll_div8",		UNKNOWN,	0x100, 2);
	hws[JH7100_CLK_DDROSC_DIV2]		= starfive_clk_gated_divider(priv, "ddrosc_div2",		UNKNOWN,	0x104, 2);
	hws[JH7100_CLK_DDRC0]			= starfive_clk_mux(priv, "ddrc0",	0x108, 2, ddrc0_sels, ARRAY_SIZE(ddrc0_sels));
	hws[JH7100_CLK_DDRC1]			= starfive_clk_mux(priv, "ddrc1",	0x10c, 2, ddrc1_sels, ARRAY_SIZE(ddrc1_sels));
	hws[JH7100_CLK_DDRPHY_APB]		= starfive_clk_gate(priv, "ddrphy_apb",		UNKNOWN,	0x110);
	hws[JH7100_CLK_NOC_ROB]			= starfive_clk_divider(priv, "noc_rob",		UNKNOWN,	0x114, 4);
	hws[JH7100_CLK_NOC_COG]			= starfive_clk_divider(priv, "noc_cog",		UNKNOWN,	0x118, 4);
	hws[JH7100_CLK_NNE_AHB]			= starfive_clk_gate(priv, "nne_ahb",		UNKNOWN,	0x11c);
	hws[JH7100_CLK_NNEBUS_SRC1]		= starfive_clk_divider(priv, "nnebus_src1",		UNKNOWN,	0x120, 3);
	hws[JH7100_CLK_NNE_BUS]			= starfive_clk_mux(priv, "nne_bus",	0x124, 2, nne_bus_sels, ARRAY_SIZE(nne_bus_sels));
	hws[JH7100_CLK_NNE_AXI]			= starfive_clk_gate(priv, "nne_axi",	UNKNOWN, 0x128);
	hws[JH7100_CLK_NNENOC_AXI]		= starfive_clk_gate(priv, "nnenoc_axi",		UNKNOWN,	0x12c);
	hws[JH7100_CLK_DLASLV_AXI]		= starfive_clk_gate(priv, "dlaslv_axi",		UNKNOWN,	0x130);
	hws[JH7100_CLK_DSPX2C_AXI]		= starfive_clk_gate(priv, "dspx2c_axi",		UNKNOWN,	0x134);
	hws[JH7100_CLK_HIFI4_SRC]		= starfive_clk_divider(priv, "hifi4_src",		UNKNOWN,	0x138, 3);
	hws[JH7100_CLK_HIFI4_COREFREE]		= starfive_clk_divider(priv, "hifi4_corefree",		UNKNOWN,	0x13c, 4);
	hws[JH7100_CLK_HIFI4_CORE]		= starfive_clk_gate(priv, "hifi4_core",		UNKNOWN,	0x140);
	hws[JH7100_CLK_HIFI4_BUS]		= starfive_clk_divider(priv, "hifi4_bus",		UNKNOWN,	0x144, 4);
	hws[JH7100_CLK_HIFI4_AXI]		= starfive_clk_gate(priv, "hifi4_axi",		UNKNOWN,	0x148);
	hws[JH7100_CLK_HIFI4NOC_AXI]		= starfive_clk_gate(priv, "hifi4noc_axi",		UNKNOWN,	0x14c);
	hws[JH7100_CLK_SGDMA1P_BUS]		= starfive_clk_divider(priv, "sgdma1p_bus",		UNKNOWN,	0x150, 4);
	hws[JH7100_CLK_SGDMA1P_AXI]		= starfive_clk_gate(priv, "sgdma1p_axi",		UNKNOWN,	0x154);
	hws[JH7100_CLK_DMA1P_AXI]		= starfive_clk_gate(priv, "dma1p_axi",		UNKNOWN,	0x158);
	hws[JH7100_CLK_X2C_AXI]			= starfive_clk_gated_divider(priv, "x2c_axi",		UNKNOWN,	0x15c, 4);
	hws[JH7100_CLK_USB_BUS]			= starfive_clk_divider(priv, "usb_bus",		UNKNOWN,	0x160, 4);
	hws[JH7100_CLK_USB_AXI]			= starfive_clk_gate(priv, "usb_axi",		UNKNOWN,	0x164);
	hws[JH7100_CLK_USBNOC_AXI]		= starfive_clk_gate(priv, "usbnoc_axi",		UNKNOWN,	0x168);
	hws[JH7100_CLK_USBPHY_ROOTDIV]		= starfive_clk_divider(priv, "usbphy_rootdiv",		UNKNOWN,	0x16c, 3);
	hws[JH7100_CLK_USBPHY_125M]		= starfive_clk_gated_divider(priv, "usbphy_125m",		UNKNOWN,	0x170, 4);
	hws[JH7100_CLK_USBPHY_PLLDIV25M]	= starfive_clk_gated_divider(priv, "usbphy_plldiv25m",		UNKNOWN,	0x174, 6);
	hws[JH7100_CLK_USBPHY_25M]		= starfive_clk_mux(priv, "usbphy_25m",	0x178, 1, usbphy_25m_sels, ARRAY_SIZE(usbphy_25m_sels));
	hws[JH7100_CLK_AUDIO_DIV]		= starfive_clk_divider(priv, "audio_div",		UNKNOWN,	0x17c, 18);
	hws[JH7100_CLK_AUDIO_SRC]		= starfive_clk_gate(priv, "audio_src",		UNKNOWN,	0x180);
	hws[JH7100_CLK_AUDIO_12288]		= starfive_clk_gate(priv, "audio_12288",		UNKNOWN,	0x184);
	hws[JH7100_CLK_VIN_SRC]			= starfive_clk_gated_divider(priv, "vin_src",		UNKNOWN,	0x188, 3);
	hws[JH7100_CLK_ISP0_BUS]		= starfive_clk_divider(priv, "isp0_bus",		UNKNOWN,	0x18c, 4);
	hws[JH7100_CLK_ISP0_AXI]		= starfive_clk_gate(priv, "isp0_axi",		UNKNOWN,	0x190);
	hws[JH7100_CLK_ISP0NOC_AXI]		= starfive_clk_gate(priv, "isp0noc_axi",		UNKNOWN,	0x194);
	hws[JH7100_CLK_ISPSLV_AXI]		= starfive_clk_gate(priv, "ispslv_axi",		UNKNOWN,	0x198);
	hws[JH7100_CLK_ISP1_BUS]		= starfive_clk_divider(priv, "isp1_bus",		UNKNOWN,	0x19c, 4);
	hws[JH7100_CLK_ISP1_AXI]		= starfive_clk_gate(priv, "isp1_axi",		UNKNOWN,	0x1a0);
	hws[JH7100_CLK_ISP1NOC_AXI]		= starfive_clk_gate(priv, "isp1noc_axi",		UNKNOWN,	0x1a4);
	hws[JH7100_CLK_VIN_BUS]			= starfive_clk_divider(priv, "vin_bus",		UNKNOWN,	0x1a8, 4);
	hws[JH7100_CLK_VIN_AXI]			= starfive_clk_gate(priv, "vin_axi",		UNKNOWN,	0x1ac);
	hws[JH7100_CLK_VINNOC_AXI]		= starfive_clk_gate(priv, "vinnoc_axi",		UNKNOWN,	0x1b0);
	hws[JH7100_CLK_VOUT_SRC]		= starfive_clk_gated_divider(priv, "vout_src",		UNKNOWN,	0x1b4, 3);
	hws[JH7100_CLK_DISPBUS_SRC]		= starfive_clk_divider(priv, "dispbus_src",		UNKNOWN,	0x1b8, 3);
	hws[JH7100_CLK_DISP_BUS]		= starfive_clk_divider(priv, "disp_bus",		UNKNOWN,	0x1bc, 3);
	hws[JH7100_CLK_DISP_AXI]		= starfive_clk_gate(priv, "disp_axi",		UNKNOWN,	0x1c0);
	hws[JH7100_CLK_DISPNOC_AXI]		= starfive_clk_gate(priv, "dispnoc_axi",		UNKNOWN,	0x1c4);
	hws[JH7100_CLK_SDIO0_AHB]		= starfive_clk_gate(priv, "sdio0_ahb",		UNKNOWN,	0x1c8);
	hws[JH7100_CLK_SDIO0_CCLKINT]		= starfive_clk_gated_divider(priv, "sdio0_cclkint",		UNKNOWN,	0x1cc, 5);
	hws[JH7100_CLK_SDIO0_CCLKINT_INV]	= starfive_clk_gate_dis(priv, "sdio0_cclkint_inv",		UNKNOWN,	0x1d0);
	hws[JH7100_CLK_SDIO1_AHB]		= starfive_clk_gate(priv, "sdio1_ahb",		UNKNOWN,	0x1d4);
	hws[JH7100_CLK_SDIO1_CCLKINT]		= starfive_clk_gated_divider(priv, "sdio1_cclkint",		UNKNOWN,	0x1d8, 5);
	hws[JH7100_CLK_SDIO1_CCLKINT_INV]	= starfive_clk_gate_dis(priv, "sdio1_cclkint_inv",		UNKNOWN,	0x1dc);
	hws[JH7100_CLK_GMAC_AHB]		= starfive_clk_gate(priv, "gmac_ahb",		UNKNOWN,	0x1e0);
	hws[JH7100_CLK_GMAC_ROOT_DIV]		= starfive_clk_divider(priv, "gmac_root_div",		UNKNOWN,	0x1e4, 4);
	hws[JH7100_CLK_GMAC_PTP_REF]		= starfive_clk_gated_divider(priv, "gmac_ptp_refclk",		UNKNOWN,	0x1e8, 5);
	hws[JH7100_CLK_GMAC_GTX]		= starfive_clk_gated_divider(priv, "gmac_gtxclk",		UNKNOWN,	0x1ec, 8);
	hws[JH7100_CLK_GMAC_RMII_TX]		= starfive_clk_gated_divider(priv, "gmac_rmii_txclk",		UNKNOWN,	0x1f0, 4);
	hws[JH7100_CLK_GMAC_RMII_RX]		= starfive_clk_gated_divider(priv, "gmac_rmii_rxclk",		UNKNOWN,	0x1f4, 4);
	hws[JH7100_CLK_GMAC_TX]			= starfive_clk_mux(priv, "gmac_tx",	0x1f8, 2, gmac_tx_sels, ARRAY_SIZE(gmac_tx_sels));
	hws[JH7100_CLK_GMAC_TX_INV]		= starfive_clk_gate_dis(priv, "gmac_tx_inv",		UNKNOWN,	0x1fc);
	hws[JH7100_CLK_GMAC_RX_PRE]		= starfive_clk_mux(priv, "gmac_rx_pre",	0x200, 1, gmac_rx_pre_sels, ARRAY_SIZE(gmac_rx_pre_sels));
	hws[JH7100_CLK_GMAC_RX_INV]		= starfive_clk_gate_dis(priv, "gmac_rx_inv",		UNKNOWN,	0x204);
	hws[JH7100_CLK_GMAC_RMII]		= starfive_clk_gate(priv, "gmac_rmii",		UNKNOWN,	0x208);
	hws[JH7100_CLK_GMAC_TOPHYREF]		= starfive_clk_gated_divider(priv, "gmac_tophyref",		UNKNOWN,	0x20c, 7);
	hws[JH7100_CLK_SPI2AHB_AHB]		= starfive_clk_gate(priv, "spi2ahb_ahb",		UNKNOWN,	0x210);
	hws[JH7100_CLK_SPI2AHB_CORE]		= starfive_clk_gated_divider(priv, "spi2ahb_core",		UNKNOWN,	0x214, 5);
	hws[JH7100_CLK_EZMASTER_AHB]		= starfive_clk_gate(priv, "ezmaster_ahb",		UNKNOWN,	0x218);
	hws[JH7100_CLK_E24_AHB]			= starfive_clk_gate(priv, "e24_ahb",		UNKNOWN,	0x21c);
	hws[JH7100_CLK_E24RTC_TOGGLE]		= starfive_clk_gate(priv, "e24rtc_toggle",		UNKNOWN,	0x220);
	hws[JH7100_CLK_QSPI_AHB]		= starfive_clk_gate(priv, "qspi_ahb",		UNKNOWN,	0x224);
	hws[JH7100_CLK_QSPI_APB]		= starfive_clk_gate(priv, "qspi_apb",		UNKNOWN,	0x228);
	hws[JH7100_CLK_QSPI_REF]		= starfive_clk_gated_divider(priv, "qspi_refclk",		UNKNOWN,	0x22c, 5);
	hws[JH7100_CLK_SEC_AHB]			= starfive_clk_gate(priv, "sec_ahb",		UNKNOWN,	0x230);
	hws[JH7100_CLK_AES]			= starfive_clk_gate(priv, "aes_clk",		UNKNOWN,	0x234);
	hws[JH7100_CLK_SHA]			= starfive_clk_gate(priv, "sha_clk",		UNKNOWN,	0x238);
	hws[JH7100_CLK_PKA]			= starfive_clk_gate(priv, "pka_clk",		UNKNOWN,	0x23c);
	hws[JH7100_CLK_TRNG_APB]		= starfive_clk_gate(priv, "trng_apb",		UNKNOWN,	0x240);
	hws[JH7100_CLK_OTP_APB]			= starfive_clk_gate(priv, "otp_apb",		UNKNOWN,	0x244);
	hws[JH7100_CLK_UART0_APB]		= starfive_clk_gate(priv, "uart0_apb",		UNKNOWN,	0x248);
	hws[JH7100_CLK_UART0_CORE]		= starfive_clk_gated_divider(priv, "uart0_core",		UNKNOWN,	0x24c, 6);
	hws[JH7100_CLK_UART1_APB]		= starfive_clk_gate(priv, "uart1_apb",		UNKNOWN,	0x250);
	hws[JH7100_CLK_UART1_CORE]		= starfive_clk_gated_divider(priv, "uart1_core",		UNKNOWN,	0x254, 6);
	hws[JH7100_CLK_SPI0_APB]		= starfive_clk_gate(priv, "spi0_apb",		UNKNOWN,	0x258);
	hws[JH7100_CLK_SPI0_CORE]		= starfive_clk_gated_divider(priv, "spi0_core",		UNKNOWN,	0x25c, 6);
	hws[JH7100_CLK_SPI1_APB]		= starfive_clk_gate(priv, "spi1_apb",		UNKNOWN,	0x260);
	hws[JH7100_CLK_SPI1_CORE]		= starfive_clk_gated_divider(priv, "spi1_core",		UNKNOWN,	0x264, 6);
	hws[JH7100_CLK_I2C0_APB]		= starfive_clk_gate(priv, "i2c0_apb",		UNKNOWN,	0x268);
	hws[JH7100_CLK_I2C0_CORE]		= starfive_clk_gated_divider(priv, "i2c0_core",		UNKNOWN,	0x26c, 6);
	hws[JH7100_CLK_I2C1_APB]		= starfive_clk_gate(priv, "i2c1_apb",		UNKNOWN,	0x270);
	hws[JH7100_CLK_I2C1_CORE]		= starfive_clk_gated_divider(priv, "i2c1_core",		UNKNOWN,	0x274, 6);
	hws[JH7100_CLK_GPIO_APB]		= starfive_clk_gate(priv, "gpio_apb",		UNKNOWN,	0x278);
	hws[JH7100_CLK_UART2_APB]		= starfive_clk_gate(priv, "uart2_apb",		UNKNOWN,	0x27c);
	hws[JH7100_CLK_UART2_CORE]		= starfive_clk_gated_divider(priv, "uart2_core",		UNKNOWN,	0x280, 6);
	hws[JH7100_CLK_UART3_APB]		= starfive_clk_gate(priv, "uart3_apb",		UNKNOWN,	0x284);
	hws[JH7100_CLK_UART3_CORE]		= starfive_clk_gated_divider(priv, "uart3_core",		UNKNOWN,	0x288, 6);
	hws[JH7100_CLK_SPI2_APB]		= starfive_clk_gate(priv, "spi2_apb",		UNKNOWN,	0x28c);
	hws[JH7100_CLK_SPI2_CORE]		= starfive_clk_gated_divider(priv, "spi2_core",		UNKNOWN,	0x290, 6);
	hws[JH7100_CLK_SPI3_APB]		= starfive_clk_gate(priv, "spi3_apb",		UNKNOWN,	0x294);
	hws[JH7100_CLK_SPI3_CORE]		= starfive_clk_gated_divider(priv, "spi3_core",		UNKNOWN,	0x298, 6);
	hws[JH7100_CLK_I2C2_APB]		= starfive_clk_gate(priv, "i2c2_apb",		UNKNOWN,	0x29c);
	hws[JH7100_CLK_I2C2_CORE]		= starfive_clk_gated_divider(priv, "i2c2_core",		UNKNOWN,	0x2a0, 6);
	hws[JH7100_CLK_I2C3_APB]		= starfive_clk_gate(priv, "i2c3_apb",		UNKNOWN,	0x2a4);
	hws[JH7100_CLK_I2C3_CORE]		= starfive_clk_gated_divider(priv, "i2c3_core",		UNKNOWN,	0x2a8, 6);
	hws[JH7100_CLK_WDTIMER_APB]		= starfive_clk_gate(priv, "wdtimer_apb",		UNKNOWN,	0x2ac);
	hws[JH7100_CLK_WDT_CORE]		= starfive_clk_gated_divider(priv, "wdt_coreclk",		UNKNOWN,	0x2b0, 6);
	hws[JH7100_CLK_TIMER0_CORE]		= starfive_clk_gated_divider(priv, "timer0_coreclk",		UNKNOWN,	0x2b4, 6);
	hws[JH7100_CLK_TIMER1_CORE]		= starfive_clk_gated_divider(priv, "timer1_coreclk",		UNKNOWN,	0x2b8, 6);
	hws[JH7100_CLK_TIMER2_CORE]		= starfive_clk_gated_divider(priv, "timer2_coreclk",		UNKNOWN,	0x2bc, 6);
	hws[JH7100_CLK_TIMER3_CORE]		= starfive_clk_gated_divider(priv, "timer3_coreclk",		UNKNOWN,	0x2c0, 6);
	hws[JH7100_CLK_TIMER4_CORE]		= starfive_clk_gated_divider(priv, "timer4_coreclk",		UNKNOWN,	0x2c4, 6);
	hws[JH7100_CLK_TIMER5_CORE]		= starfive_clk_gated_divider(priv, "timer5_coreclk",		UNKNOWN,	0x2c8, 6);
	hws[JH7100_CLK_TIMER6_CORE]		= starfive_clk_gated_divider(priv, "timer6_coreclk",		UNKNOWN,	0x2cc, 6);
	hws[JH7100_CLK_VP6INTC_APB]		= starfive_clk_gate(priv, "vp6intc_apb",		UNKNOWN,	0x2d0);
	hws[JH7100_CLK_PWM_APB]			= starfive_clk_gate(priv, "pwm_apb",		UNKNOWN,	0x2d4);
	hws[JH7100_CLK_MSI_APB]			= starfive_clk_gate(priv, "msi_apb",		UNKNOWN,	0x2d8);
	hws[JH7100_CLK_TEMP_APB]		= starfive_clk_gate(priv, "temp_apb",		UNKNOWN,	0x2dc);
	hws[JH7100_CLK_TEMP_SENSE]		= starfive_clk_gated_divider(priv, "temp_sense",		UNKNOWN,	0x2e0, 5);
	hws[JH7100_CLK_SYSERR_APB]		= starfive_clk_gate(priv, "syserr_apb",		UNKNOWN,	0x2e4);

{
	// FIXME Temporary overrides until we get the clock tree right
	#define CLK_AXI		0
	#define CLK_AHB0	1
	#define CLK_AHB2	2
	#define CLK_APB1	3
	#define CLK_APB2	4
	#define CLK_VPU		5
	#define CLK_JPU		6
	#define CLK_PWM		7
	#define CLK_DWMMC_BIU	8
	#define CLK_DWMMC_CIU	9
	#define CLK_UART	10
	#define CLK_HS_UART	11
	#define CLK_I2C0	12
	#define CLK_I2C2	13
	#define CLK_QSPI	14
	#define CLK_SPI		15
	#define CLK_GMAC	16
	#define CLK_HF		17
	#define CLK_RTC		18

	struct clk_hw **hws = priv->clk_hws.hws;
	extern bool clk_ignore_unused;
	static struct tmp_clk {
		const char *name;
		unsigned int mult;
		unsigned int div;
		struct clk_hw *hw;
	} tmp_clks[] = {
		[CLK_AXI] =       { "axi",       .mult =  20, .div =   1 },
		[CLK_AHB0] =      { "ahb0",      .mult =  10, .div =   1 },
		[CLK_AHB2] =      { "ahb2",      .mult =   5, .div =   1 },
		[CLK_APB1] =      { "apb1",      .mult =   5, .div =   1 },
		[CLK_APB2] =      { "apb2",      .mult =   5, .div =   1 },
		[CLK_VPU] =       { "vpu",       .mult =  16, .div =   1 },
		[CLK_JPU] =       { "jpu",       .mult =  40, .div =   3 },
		[CLK_PWM] =       { "pwm",       .mult =   5, .div =   1 },
		[CLK_DWMMC_BIU] = { "dwmmc-biu", .mult =   4, .div =   1 },
		[CLK_DWMMC_CIU] = { "dwmmc-ciu", .mult =   4, .div =   1 },
		[CLK_UART] =      { "uart",      .mult =   4, .div =   1 },
		[CLK_HS_UART] =   { "hs_uart",   .mult = 297, .div = 100 },
		[CLK_I2C0] =      { "i2c0",      .mult =  99, .div =  50 },
		[CLK_I2C2] =      { "i2c2",      .mult =   2, .div =   1 },
		[CLK_QSPI] =      { "qspi",      .mult =   2, .div =   1 },
		[CLK_SPI] =       { "spi",       .mult =   2, .div =   1 },
		[CLK_GMAC] =      { "gmac",      .mult =   1, .div =   1 },
		[CLK_HF] =        { "hf",        .mult =   1, .div =   1 },
		[CLK_RTC] =       { "rtc",       .mult =   1, .div =   4 }
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(tmp_clks); i++) {
		struct tmp_clk *clk = &tmp_clks[i];
		clk->hw = devm_clk_hw_register_fixed_factor(priv->dev,
				clk->name, "osc_sys", 0, clk->mult, clk->div);
	}

	hws[JH7100_CLK_UART0_APB]	= tmp_clks[CLK_APB2].hw;
	hws[JH7100_CLK_UART0_CORE]	= tmp_clks[CLK_HS_UART].hw;
//	hws[JH7100_CLK_UART1_APB]	= tmp_clks[CLK_APB2].hw;
//	hws[JH7100_CLK_UART1_CORE]	= tmp_clks[CLK_HS_UART].hw;
//	hws[JH7100_CLK_UART2_APB]	= tmp_clks[CLK_APB2].hw;
//	hws[JH7100_CLK_UART2_CORE]	= tmp_clks[CLK_UART].hw;
	hws[JH7100_CLK_UART3_APB]	= tmp_clks[CLK_APB2].hw;
	hws[JH7100_CLK_UART3_CORE]	= tmp_clks[CLK_UART].hw;

	hws[JH7100_CLK_I2C0_APB]	= tmp_clks[CLK_APB2].hw;
	hws[JH7100_CLK_I2C0_CORE]	= tmp_clks[CLK_I2C0].hw;
	hws[JH7100_CLK_I2C1_APB]	= tmp_clks[CLK_APB2].hw;
	hws[JH7100_CLK_I2C1_CORE]	= tmp_clks[CLK_I2C0].hw;
	hws[JH7100_CLK_I2C2_APB]	= tmp_clks[CLK_APB2].hw;
	hws[JH7100_CLK_I2C2_CORE]	= tmp_clks[CLK_I2C2].hw;
//	hws[JH7100_CLK_I2C3_APB]	= tmp_clks[CLK_APB2].hw;
//	hws[JH7100_CLK_I2C3_CORE]	= tmp_clks[CLK_I2C2].hw;

//	hws[JH7100_CLK_SPI0_CORE]	= tmp_clks[CLK_SPI].hw;
//	hws[JH7100_CLK_SPI1_CORE]	= tmp_clks[CLK_SPI].hw;
	hws[JH7100_CLK_SPI2_CORE]	= tmp_clks[CLK_SPI].hw;
//	hws[JH7100_CLK_SPI3_CORE]	= tmp_clks[CLK_SPI].hw;

	clk_ignore_unused = true;
}

	return 0;
}

static int __init clk_starfive_jh7100_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct clk_starfive_jh7100_priv *priv;
	int error;

	priv = devm_kzalloc(dev, struct_size(priv, clk_hws.hws, JH7100_CLK_END), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	priv->clk_hws.num = JH7100_CLK_END;
	spin_lock_init(&priv->rmw_lock);

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	error = starfive_clkgen_init(priv);
	if (error)
		goto cleanup;

	error = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					    &priv->clk_hws);
	if (error)
		goto cleanup;

	return 0;

cleanup:
	// FIXME unregister gate clocks on failure
	return error;
}

static const struct of_device_id clk_starfive_jh7100_match[] = {
	{
		.compatible = "starfive,jh7100-clkgen",
	},
	{ /* sentinel */ }
};

static struct platform_driver clk_starfive_jh7100_driver = {
	.driver		= {
		.name	= "clk-starfive-jh7100",
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
