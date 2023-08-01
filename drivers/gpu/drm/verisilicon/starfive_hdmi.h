/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 StarFive Technology Co., Ltd.
 */

#ifndef __STARFIVE_HDMI_H__
#define __STARFIVE_HDMI_H__

#include <drm/bridge/dw_hdmi.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <linux/bitfield.h>
#include <linux/bits.h>

#define DDC_SEGMENT_ADDR		0x30

#define HDMI_SCL_RATE			(100 * 1000)
#define DDC_BUS_FREQ_L			0x4b
#define DDC_BUS_FREQ_H			0x4c

#define HDMI_SYS_CTRL			0x00
#define m_RST_ANALOG			BIT(6)
#define v_RST_ANALOG			0
#define v_NOT_RST_ANALOG		BIT(6)
#define m_RST_DIGITAL			BIT(5)
#define v_RST_DIGITAL			0
#define v_NOT_RST_DIGITAL		BIT(5)
#define m_REG_CLK_INV			BIT(4)
#define v_REG_CLK_NOT_INV		0
#define v_REG_CLK_INV			BIT(4)
#define m_VCLK_INV			BIT(3)
#define v_VCLK_NOT_INV			0
#define v_VCLK_INV			BIT(3)
#define m_REG_CLK_SOURCE		BIT(2)
#define v_REG_CLK_SOURCE_TMDS		0
#define v_REG_CLK_SOURCE_SYS		BIT(2)
#define m_POWER				BIT(1)
#define v_PWR_ON			0
#define v_PWR_OFF			BIT(1)
#define m_INT_POL			BIT(0)
#define v_INT_POL_HIGH			1
#define v_INT_POL_LOW			0

#define HDMI_AV_MUTE			0x05
#define m_AVMUTE_CLEAR			BIT(7)
#define m_AVMUTE_ENABLE			BIT(6)
#define m_AUDIO_MUTE			BIT(1)
#define m_VIDEO_BLACK			BIT(0)
#define v_AVMUTE_CLEAR(n)		((n) << 7)
#define v_AVMUTE_ENABLE(n)		((n) << 6)
#define v_AUDIO_MUTE(n)			((n) << 1)
#define v_VIDEO_MUTE(n)			((n) << 0)

#define HDMI_VIDEO_TIMING_CTL		0x08
#define v_VSYNC_POLARITY(n)		((n) << 3)
#define v_HSYNC_POLARITY(n)		((n) << 2)
#define v_INETLACE(n)			((n) << 1)
#define v_EXTERANL_VIDEO(n)		((n) << 0)

#define HDMI_VIDEO_EXT_HTOTAL_L		0x09
#define HDMI_VIDEO_EXT_HTOTAL_H		0x0a
#define HDMI_VIDEO_EXT_HBLANK_L		0x0b
#define HDMI_VIDEO_EXT_HBLANK_H		0x0c
#define HDMI_VIDEO_EXT_HDELAY_L		0x0d
#define HDMI_VIDEO_EXT_HDELAY_H		0x0e
#define HDMI_VIDEO_EXT_HDURATION_L	0x0f
#define HDMI_VIDEO_EXT_HDURATION_H	0x10
#define HDMI_VIDEO_EXT_VTOTAL_L		0x11
#define HDMI_VIDEO_EXT_VTOTAL_H		0x12
#define HDMI_VIDEO_EXT_VBLANK		0x13
#define HDMI_VIDEO_EXT_VDELAY		0x14
#define HDMI_VIDEO_EXT_VDURATION	0x15

#define HDMI_EDID_SEGMENT_POINTER	0x4d
#define HDMI_EDID_WORD_ADDR		0x4e
#define HDMI_EDID_FIFO_OFFSET		0x4f
#define HDMI_EDID_FIFO_ADDR		0x50

#define HDMI_INTERRUPT_MASK1		0xc0
#define HDMI_INTERRUPT_STATUS1		0xc1
#define	m_INT_ACTIVE_VSYNC		BIT(5)
#define m_INT_EDID_READY		BIT(2)

#define HDMI_STATUS			0xc8
#define m_HOTPLUG			BIT(7)
#define m_MASK_INT_HOTPLUG		BIT(5)
#define m_INT_HOTPLUG			BIT(1)
#define v_MASK_INT_HOTPLUG(n)		(((n) & 0x1) << 5)

#define HDMI_SYNC					0xce

#define UPDATE(x, h, l)					FIELD_PREP(GENMASK(h, l), x)

/* REG: 0x1a0 */
#define STARFIVE_PRE_PLL_CONTROL			0x1a0
#define STARFIVE_PCLK_VCO_DIV_5_MASK			BIT(1)
#define STARFIVE_PCLK_VCO_DIV_5(x)			UPDATE(x, 1, 1)
#define STARFIVE_PRE_PLL_POWER_DOWN			BIT(0)

/* REG: 0x1a1 */
#define STARFIVE_PRE_PLL_DIV_1				0x1a1
#define STARFIVE_PRE_PLL_PRE_DIV_MASK			GENMASK(5, 0)
#define STARFIVE_PRE_PLL_PRE_DIV(x)			UPDATE(x, 5, 0)

/* REG: 0x1a2 */
#define STARFIVE_PRE_PLL_DIV_2				0x1a2
#define STARFIVE_SPREAD_SPECTRUM_MOD_DOWN		BIT(7)
#define STARFIVE_SPREAD_SPECTRUM_MOD_DISABLE		BIT(6)
#define STARFIVE_PRE_PLL_FRAC_DIV_DISABLE		UPDATE(3, 5, 4)
#define STARFIVE_PRE_PLL_FB_DIV_11_8_MASK		GENMASK(3, 0)
#define STARFIVE_PRE_PLL_FB_DIV_11_8(x)			UPDATE((x) >> 8, 3, 0)

/* REG: 0x1a3 */
#define STARFIVE_PRE_PLL_DIV_3				0x1a3
#define STARFIVE_PRE_PLL_FB_DIV_7_0(x)			UPDATE(x, 7, 0)

/* REG: 0x1a4*/
#define STARFIVE_PRE_PLL_DIV_4				0x1a4
#define STARFIVE_PRE_PLL_TMDSCLK_DIV_C_MASK		GENMASK(1, 0)
#define STARFIVE_PRE_PLL_TMDSCLK_DIV_C(x)		UPDATE(x, 1, 0)
#define STARFIVE_PRE_PLL_TMDSCLK_DIV_B_MASK		GENMASK(3, 2)
#define STARFIVE_PRE_PLL_TMDSCLK_DIV_B(x)		UPDATE(x, 3, 2)
#define STARFIVE_PRE_PLL_TMDSCLK_DIV_A_MASK		GENMASK(5, 4)
#define STARFIVE_PRE_PLL_TMDSCLK_DIV_A(x)		UPDATE(x, 5, 4)

/* REG: 0x1a5 */
#define STARFIVE_PRE_PLL_DIV_5				0x1a5
#define STARFIVE_PRE_PLL_PCLK_DIV_B_SHIFT		5
#define STARFIVE_PRE_PLL_PCLK_DIV_B_MASK		GENMASK(6, 5)
#define STARFIVE_PRE_PLL_PCLK_DIV_B(x)			UPDATE(x, 6, 5)
#define STARFIVE_PRE_PLL_PCLK_DIV_A_MASK		GENMASK(4, 0)
#define STARFIVE_PRE_PLL_PCLK_DIV_A(x)			UPDATE(x, 4, 0)

/* REG: 0x1a6 */
#define STARFIVE_PRE_PLL_DIV_6				0x1a6
#define STARFIVE_PRE_PLL_PCLK_DIV_C_SHIFT		5
#define STARFIVE_PRE_PLL_PCLK_DIV_C_MASK		GENMASK(6, 5)
#define STARFIVE_PRE_PLL_PCLK_DIV_C(x)			UPDATE(x, 6, 5)
#define STARFIVE_PRE_PLL_PCLK_DIV_D_MASK		GENMASK(4, 0)
#define STARFIVE_PRE_PLL_PCLK_DIV_D(x)			UPDATE(x, 4, 0)

/* REG: 0x1a9 */
#define STARFIVE_PRE_PLL_LOCK_STATUS			0x1a9

/* REG: 0x1aa */
#define STARFIVE_POST_PLL_DIV_1				0x1aa
#define STARFIVE_POST_PLL_POST_DIV_ENABLE		GENMASK(3, 2)
#define STARFIVE_POST_PLL_REFCLK_SEL_TMDS		BIT(1)
#define STARFIVE_POST_PLL_POWER_DOWN			BIT(0)
#define STARFIVE_POST_PLL_FB_DIV_8(x)			UPDATE(((x) >> 8) << 4, 4, 4)

/* REG:0x1ab */
#define STARFIVE_POST_PLL_DIV_2				0x1ab
#define STARFIVE_POST_PLL_Pre_DIV_MASK			GENMASK(5, 0)
#define STARFIVE_POST_PLL_PRE_DIV(x)			UPDATE(x, 5, 0)

/* REG: 0x1ac */
#define STARFIVE_POST_PLL_DIV_3				0x1ac
#define STARFIVE_POST_PLL_FB_DIV_7_0(x)			UPDATE(x, 7, 0)

/* REG: 0x1ad */
#define STARFIVE_POST_PLL_DIV_4				0x1ad
#define STARFIVE_POST_PLL_POST_DIV_MASK			GENMASK(2, 0)
#define STARFIVE_POST_PLL_POST_DIV_2			0x0
#define STARFIVE_POST_PLL_POST_DIV_4			0x1
#define STARFIVE_POST_PLL_POST_DIV_8			0x3

/* REG: 0x1af */
#define STARFIVE_POST_PLL_LOCK_STATUS			0x1af

/* REG: 0x1b0 */
#define STARFIVE_BIAS_CONTROL				0x1b0
#define STARFIVE_BIAS_ENABLE				BIT(2)

/* REG: 0x1b2 */
#define STARFIVE_TMDS_CONTROL				0x1b2
#define STARFIVE_TMDS_CLK_DRIVER_EN			BIT(3)
#define STARFIVE_TMDS_D2_DRIVER_EN			BIT(2)
#define STARFIVE_TMDS_D1_DRIVER_EN			BIT(1)
#define STARFIVE_TMDS_D0_DRIVER_EN			BIT(0)
#define STARFIVE_TMDS_DRIVER_ENABLE			(STARFIVE_TMDS_CLK_DRIVER_EN | \
							 STARFIVE_TMDS_D2_DRIVER_EN | \
							 STARFIVE_TMDS_D1_DRIVER_EN | \
							 STARFIVE_TMDS_D0_DRIVER_EN)

/* REG: 0x1b4 */
#define STARFIVE_LDO_CONTROL				0x1b4
#define STARFIVE_LDO_D2_EN				BIT(2)
#define STARFIVE_LDO_D1_EN				BIT(1)
#define STARFIVE_LDO_D0_EN				BIT(0)
#define STARFIVE_LDO_ENABLE				(STARFIVE_LDO_D2_EN | \
							 STARFIVE_LDO_D1_EN | \
							 STARFIVE_LDO_D0_EN)

/* REG: 0x1be */
#define STARFIVE_SERIALIER_CONTROL			0x1be
#define STARFIVE_SERIALIER_D2_EN			BIT(6)
#define STARFIVE_SERIALIER_D1_EN			BIT(5)
#define STARFIVE_SERIALIER_D0_EN			BIT(4)
#define STARFIVE_SERIALIER_EN				BIT(0)

#define STARFIVE_SERIALIER_ENABLE			(STARFIVE_SERIALIER_D2_EN | \
							 STARFIVE_SERIALIER_D1_EN | \
							 STARFIVE_SERIALIER_D0_EN | \
							 STARFIVE_SERIALIER_EN)

/* REG: 0x1cc */
#define STARFIVE_RX_CONTROL				0x1cc
#define STARFIVE_RX_EN					BIT(3)
#define STARFIVE_RX_CHANNEL_2_EN			BIT(2)
#define STARFIVE_RX_CHANNEL_1_EN			BIT(1)
#define STARFIVE_RX_CHANNEL_0_EN			BIT(0)
#define STARFIVE_RX_ENABLE				(STARFIVE_RX_EN | \
							 STARFIVE_RX_CHANNEL_2_EN | \
							 STARFIVE_RX_CHANNEL_1_EN | \
							 STARFIVE_RX_CHANNEL_0_EN)

/* REG: 0x1d1 */
#define STARFIVE_PRE_PLL_FRAC_DIV_H			0x1d1
#define STARFIVE_PRE_PLL_FRAC_DIV_23_16(x)		UPDATE((x) >> 16, 7, 0)
/* REG: 0x1d2 */
#define STARFIVE_PRE_PLL_FRAC_DIV_M			0x1d2
#define STARFIVE_PRE_PLL_FRAC_DIV_15_8(x)		UPDATE((x) >> 8, 7, 0)
/* REG: 0x1d3 */
#define STARFIVE_PRE_PLL_FRAC_DIV_L			0x1d3
#define STARFIVE_PRE_PLL_FRAC_DIV_7_0(x)		UPDATE(x, 7, 0)

struct pre_pll_config {
	unsigned long pixclock;
	unsigned long tmdsclock;
	u8 prediv;
	u16 fbdiv;
	u8 tmds_div_a;
	u8 tmds_div_b;
	u8 tmds_div_c;
	u8 pclk_div_a;
	u8 pclk_div_b;
	u8 pclk_div_c;
	u8 pclk_div_d;
	u8 vco_div_5_en;
	u32 fracdiv;
};

struct post_pll_config {
	unsigned long tmdsclock;
	u8 prediv;
	u16 fbdiv;
	u8 postdiv;
	u8 post_div_en;
	u8 version;
};

struct phy_config {
	unsigned long	tmdsclock;
	u8		regs[14];
};

struct hdmi_data_info {
	int vic;
	bool sink_is_hdmi;
	bool sink_has_audio;
	unsigned int enc_in_format;
	unsigned int enc_out_format;
	unsigned int colorimetry;
};

struct starfive_hdmi {
	struct device *dev;
	struct drm_device *drm_dev;

	int irq;
	struct clk *sys_clk;
	struct clk *mclk;
	struct clk *bclk;
	struct reset_control *tx_rst;
	void __iomem *regs;

	struct drm_connector	connector;
	struct drm_encoder	encoder;

	struct starfive_hdmi_i2c *i2c;
	struct i2c_adapter *ddc;

	unsigned long tmds_rate;

	struct hdmi_data_info	hdmi_data;
	struct drm_display_mode previous_mode;
	const struct pre_pll_config	*pre_cfg;
	const struct post_pll_config	*post_cfg;
};

#endif /* __STARFIVE_HDMI_H__ */
