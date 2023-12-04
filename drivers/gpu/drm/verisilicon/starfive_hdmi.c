// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 StarFive Technology Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/hdmi.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/media-bus-format.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include <drm/bridge/dw_hdmi.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_managed.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>

#include "starfive_hdmi.h"
#include "vs_drv.h"
#include "vs_crtc.h"

static const char * const hdmi_clocks[] = {
	"sysclk",
	"mclk",
	"bclk"
};

static struct starfive_hdmi_encoder *encoder_to_hdmi(struct drm_encoder *encoder)
{
	return container_of(encoder, struct starfive_hdmi_encoder, encoder);
}

static struct starfive_hdmi *connector_to_hdmi(struct drm_connector *connector)
{
	return container_of(connector, struct starfive_hdmi, connector);
}

static const struct post_pll_config post_pll_cfg_table[] = {
	{25200000,	1, 80, 13, 3, 1},
	{27000000,	1, 40, 11, 3, 1},
	{33750000,	1, 40, 11, 3, 1},
	{49000000,	1, 20, 1, 3, 3},
	{241700000, 1, 20, 1, 3, 3},
	{297000000, 4, 20, 0, 0, 3},
	{594000000, 4, 20, 0, 0, 0},
	{ /* sentinel */ }
};

inline u8 hdmi_readb(struct starfive_hdmi *hdmi, u16 offset)
{
	return readl_relaxed(hdmi->regs + (offset) * 0x04);
}

inline void hdmi_writeb(struct starfive_hdmi *hdmi, u16 offset, u32 val)
{
	writel_relaxed(val, hdmi->regs + (offset) * 0x04);
}

inline void hdmi_writew(struct starfive_hdmi *hdmi, u16 offset, u32 val)
{
	writew_relaxed(val & 0xFF, hdmi->regs + (offset) * 0x04);
	writew_relaxed((val >> 8) & 0xFF, hdmi->regs + (offset + 1) * 0x04);
}

inline void hdmi_modb(struct starfive_hdmi *hdmi, u16 offset,
			     u32 msk, u32 val)
{
	u8 temp = hdmi_readb(hdmi, offset) & ~msk;

	temp |= val & msk;
	hdmi_writeb(hdmi, offset, temp);
}

static int starfive_hdmi_enable_clk_deassert_rst(struct device *dev, struct starfive_hdmi *hdmi)
{
	int ret;

	ret = clk_bulk_prepare_enable(hdmi->nclks, hdmi->clk_hdmi);
	if (ret) {
		dev_err(dev, "failed to enable clocks\n");
		return ret;
	}

	ret = reset_control_deassert(hdmi->tx_rst);
	if (ret < 0) {
		dev_err(dev, "failed to deassert tx_rst\n");
		return ret;
	}
	return 0;
}

static void starfive_hdmi_disable_clk_assert_rst(struct device *dev, struct starfive_hdmi *hdmi)
{
	int ret;

	ret = reset_control_assert(hdmi->tx_rst);
	if (ret < 0)
		dev_err(dev, "failed to assert tx_rst\n");

	clk_bulk_disable_unprepare(hdmi->nclks, hdmi->clk_hdmi);
}

#ifdef CONFIG_PM_SLEEP
static int hdmi_system_pm_suspend(struct device *dev)
{
	return pm_runtime_force_suspend(dev);
}

static int hdmi_system_pm_resume(struct device *dev)
{
	return pm_runtime_force_resume(dev);
}
#endif

#ifdef CONFIG_PM
static int hdmi_runtime_suspend(struct device *dev)
{
	struct starfive_hdmi *hdmi = dev_get_drvdata(dev);

	starfive_hdmi_disable_clk_assert_rst(dev, hdmi);

	return 0;
}

static int hdmi_runtime_resume(struct device *dev)
{
	struct starfive_hdmi *hdmi = dev_get_drvdata(dev);

	return starfive_hdmi_enable_clk_deassert_rst(dev, hdmi);
}
#endif

static void starfive_hdmi_tx_phy_power_down(struct starfive_hdmi *hdmi)
{
	hdmi_modb(hdmi, HDMI_SYS_CTRL, m_POWER, v_PWR_OFF);
}

static void starfive_hdmi_tx_phy_power_on(struct starfive_hdmi *hdmi)
{
	hdmi_modb(hdmi, HDMI_SYS_CTRL, m_POWER, v_PWR_ON);
}

static void starfive_hdmi_config_pll(struct starfive_hdmi *hdmi)
{
	u32 val;
	u8 reg_1ad_value = hdmi->post_cfg->post_div_en ?
		 hdmi->post_cfg->postdiv : 0x00;
	u8 reg_1aa_value = hdmi->post_cfg->post_div_en ?
		 0x0e : 0x02;

	hdmi_writeb(hdmi, STARFIVE_PRE_PLL_CONTROL, STARFIVE_PRE_PLL_POWER_DOWN);
	hdmi_writeb(hdmi, STARFIVE_POST_PLL_DIV_1,
		    STARFIVE_POST_PLL_POST_DIV_ENABLE |
		    STARFIVE_POST_PLL_REFCLK_SEL_TMDS |
		    STARFIVE_POST_PLL_POWER_DOWN);
	hdmi_writeb(hdmi, STARFIVE_PRE_PLL_DIV_1, STARFIVE_PRE_PLL_PRE_DIV(hdmi->pre_cfg.prediv));

	val = STARFIVE_SPREAD_SPECTRUM_MOD_DISABLE | STARFIVE_SPREAD_SPECTRUM_MOD_DOWN;
	if (!hdmi->pre_cfg.fracdiv)
		val |= STARFIVE_PRE_PLL_FRAC_DIV_DISABLE;
	hdmi_writeb(hdmi, STARFIVE_PRE_PLL_DIV_2,
		    STARFIVE_PRE_PLL_FB_DIV_11_8(hdmi->pre_cfg.fbdiv) | val);
	hdmi_writeb(hdmi, STARFIVE_PRE_PLL_DIV_3,
		    STARFIVE_PRE_PLL_FB_DIV_7_0(hdmi->pre_cfg.fbdiv));
	hdmi_writeb(hdmi, STARFIVE_PRE_PLL_DIV_4,
		    STARFIVE_PRE_PLL_TMDSCLK_DIV_C(hdmi->pre_cfg.tmds_div_c) |
		    STARFIVE_PRE_PLL_TMDSCLK_DIV_A(hdmi->pre_cfg.tmds_div_a) |
		    STARFIVE_PRE_PLL_TMDSCLK_DIV_B(hdmi->pre_cfg.tmds_div_b));

	if (hdmi->pre_cfg.fracdiv) {
		hdmi_writeb(hdmi, STARFIVE_PRE_PLL_FRAC_DIV_L,
			    STARFIVE_PRE_PLL_FRAC_DIV_7_0(hdmi->pre_cfg.fracdiv));
		hdmi_writeb(hdmi, STARFIVE_PRE_PLL_FRAC_DIV_M,
			    STARFIVE_PRE_PLL_FRAC_DIV_15_8(hdmi->pre_cfg.fracdiv));
		hdmi_writeb(hdmi, STARFIVE_PRE_PLL_FRAC_DIV_H,
			    STARFIVE_PRE_PLL_FRAC_DIV_23_16(hdmi->pre_cfg.fracdiv));
	}

	hdmi_writeb(hdmi, STARFIVE_PRE_PLL_DIV_5,
		    STARFIVE_PRE_PLL_PCLK_DIV_A(hdmi->pre_cfg.pclk_div_a) |
		    STARFIVE_PRE_PLL_PCLK_DIV_B(hdmi->pre_cfg.pclk_div_b));
	hdmi_writeb(hdmi, STARFIVE_PRE_PLL_DIV_6,
		    STARFIVE_PRE_PLL_PCLK_DIV_C(hdmi->pre_cfg.pclk_div_c) |
		    STARFIVE_PRE_PLL_PCLK_DIV_D(hdmi->pre_cfg.pclk_div_d));

	/*pre-pll power down*/
	hdmi_modb(hdmi, STARFIVE_PRE_PLL_CONTROL, STARFIVE_PRE_PLL_POWER_DOWN, 0);

	hdmi_modb(hdmi, STARFIVE_POST_PLL_DIV_2, STARFIVE_POST_PLL_Pre_DIV_MASK,
		  STARFIVE_POST_PLL_PRE_DIV(hdmi->post_cfg->prediv));
	hdmi_writeb(hdmi, STARFIVE_POST_PLL_DIV_3, hdmi->post_cfg->fbdiv & 0xff);
	hdmi_writeb(hdmi, STARFIVE_POST_PLL_DIV_4, reg_1ad_value);
	hdmi_writeb(hdmi, STARFIVE_POST_PLL_DIV_1, reg_1aa_value);
}

static void starfive_hdmi_tmds_driver_on(struct starfive_hdmi *hdmi)
{
	hdmi_modb(hdmi, STARFIVE_TMDS_CONTROL,
		  STARFIVE_TMDS_DRIVER_ENABLE, STARFIVE_TMDS_DRIVER_ENABLE);
}

static void starfive_hdmi_sync_tmds(struct starfive_hdmi *hdmi)
{
	/*first send 0 to this bit, then send 1 and keep 1 into this bit*/
	hdmi_writeb(hdmi, HDMI_SYNC, 0x0);
	hdmi_writeb(hdmi, HDMI_SYNC, 0x1);
}

static void starfive_hdmi_i2c_init(struct starfive_hdmi *hdmi)
{
	int ddc_bus_freq;

	ddc_bus_freq = (clk_get_rate(hdmi->clk_hdmi[CLK_SYS].clk) >> 2) / HDMI_SCL_RATE;

	hdmi_writeb(hdmi, DDC_BUS_FREQ_L, ddc_bus_freq & 0xFF);
	hdmi_writeb(hdmi, DDC_BUS_FREQ_H, (ddc_bus_freq >> 8) & 0xFF);

	/* Clear the EDID interrupt flag and mute the interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, 0);
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, m_INT_EDID_READY);
}

static void starfive_hdmi_phy_get_pre_pll_cfg(struct starfive_hdmi *hdmi)
{
	if (hdmi->tmds_rate > 30000000) {
		hdmi->pre_cfg.pixclock = hdmi->tmds_rate;
		hdmi->pre_cfg.tmdsclock = hdmi->tmds_rate;
		hdmi->pre_cfg.prediv = 1;
		hdmi->pre_cfg.fbdiv = hdmi->tmds_rate / 3000000;
		hdmi->pre_cfg.tmds_div_a = 0;
		hdmi->pre_cfg.tmds_div_b = 1;
		hdmi->pre_cfg.tmds_div_c = 1;
		hdmi->pre_cfg.pclk_div_a = 1;
		hdmi->pre_cfg.pclk_div_b = 0;
		hdmi->pre_cfg.pclk_div_c = 2;
		hdmi->pre_cfg.pclk_div_d = 2;
		hdmi->pre_cfg.vco_div_5_en = hdmi->tmds_rate % 3000000 ? 1 : 0;

		if (hdmi->pre_cfg.vco_div_5_en) {
			hdmi->pre_cfg.fracdiv = (hdmi->tmds_rate % 3000000) *
						 0xffffff / 1000000;
		}
	} else {
		hdmi->pre_cfg.pixclock = hdmi->tmds_rate;
		hdmi->pre_cfg.tmdsclock = hdmi->tmds_rate;
		hdmi->pre_cfg.prediv = 1;
		hdmi->pre_cfg.fbdiv = hdmi->tmds_rate / 1000000;
		hdmi->pre_cfg.tmds_div_a = 2;
		hdmi->pre_cfg.tmds_div_b = 1;
		hdmi->pre_cfg.tmds_div_c = 1;
		hdmi->pre_cfg.pclk_div_a = 3;
		hdmi->pre_cfg.pclk_div_b = 0;
		hdmi->pre_cfg.pclk_div_c = 3;
		hdmi->pre_cfg.pclk_div_d = 4;
		hdmi->pre_cfg.vco_div_5_en = hdmi->tmds_rate % 1000000 ? 1 : 0;

		if (hdmi->pre_cfg.vco_div_5_en) {
			hdmi->pre_cfg.fracdiv = (hdmi->tmds_rate % 1000000) *
						 0xffffff / 1000000;
		}
	}
}

static int starfive_hdmi_phy_clk_set_rate(struct starfive_hdmi *hdmi)
{
	hdmi->post_cfg = post_pll_cfg_table;

	starfive_hdmi_phy_get_pre_pll_cfg(hdmi);

	for (; hdmi->post_cfg->tmdsclock != 0; hdmi->post_cfg++)
		if (hdmi->tmds_rate <= hdmi->post_cfg->tmdsclock)
			break;

	starfive_hdmi_config_pll(hdmi);

	return 0;
}

static int starfive_hdmi_config_video_timing(struct starfive_hdmi *hdmi,
					     struct drm_display_mode *mode)
{
	int value;
	/* Set detail external video timing */
	value = mode->htotal;
	hdmi_writew(hdmi, HDMI_VIDEO_EXT_HTOTAL_L, value);

	value = mode->htotal - mode->hdisplay;
	hdmi_writew(hdmi, HDMI_VIDEO_EXT_HBLANK_L, value);

	value = mode->htotal - mode->hsync_start;
	hdmi_writew(hdmi, HDMI_VIDEO_EXT_HDELAY_L, value);

	value = mode->hsync_end - mode->hsync_start;
	hdmi_writew(hdmi, HDMI_VIDEO_EXT_HDURATION_L, value);

	value = mode->vtotal;
	hdmi_writew(hdmi, HDMI_VIDEO_EXT_VTOTAL_L, value);

	value = mode->vtotal - mode->vdisplay;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VBLANK, value & 0xFF);

	value = mode->vtotal - mode->vsync_start;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VDELAY, value & 0xFF);

	value = mode->vsync_end - mode->vsync_start;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VDURATION, value & 0xFF);

	/* Set detail external video timing polarity and interlace mode */
	value = v_EXTERANL_VIDEO(1);
	value |= mode->flags & DRM_MODE_FLAG_PHSYNC ?
		v_HSYNC_POLARITY(1) : v_HSYNC_POLARITY(0);
	value |= mode->flags & DRM_MODE_FLAG_PVSYNC ?
		v_VSYNC_POLARITY(1) : v_VSYNC_POLARITY(0);
	value |= mode->flags & DRM_MODE_FLAG_INTERLACE ?
		v_INETLACE(1) : v_INETLACE(0);

	hdmi_writeb(hdmi, HDMI_VIDEO_TIMING_CTL, value);
	return 0;
}

static int starfive_hdmi_setup(struct starfive_hdmi *hdmi,
			       struct drm_display_mode *mode)
{
	int ret;
	u32 val;

	hdmi_modb(hdmi, STARFIVE_BIAS_CONTROL, STARFIVE_BIAS_ENABLE, STARFIVE_BIAS_ENABLE);
	hdmi_writeb(hdmi, STARFIVE_RX_CONTROL, STARFIVE_RX_ENABLE);

	hdmi->tmds_rate = mode->clock * 1000;
	starfive_hdmi_phy_clk_set_rate(hdmi);

	ret = readx_poll_timeout(readl_relaxed,
				 hdmi->regs + (STARFIVE_PRE_PLL_LOCK_STATUS) * 0x04,
				 val, val & 0x1, 1000, 100000);
	if (ret < 0) {
		dev_err(hdmi->dev, "failed to wait pre-pll lock\n");
		return ret;
	}

	ret = readx_poll_timeout(readl_relaxed,
				 hdmi->regs + (STARFIVE_POST_PLL_LOCK_STATUS) * 0x04,
				 val, val & 0x1, 1000, 100000);
	if (ret < 0) {
		dev_err(hdmi->dev, "failed to wait post-pll lock\n");
		return ret;
	}

	/*turn on LDO*/
	hdmi_writeb(hdmi, STARFIVE_LDO_CONTROL, STARFIVE_LDO_ENABLE);
	/*turn on serializer*/
	hdmi_writeb(hdmi, STARFIVE_SERIALIER_CONTROL, STARFIVE_SERIALIER_ENABLE);

	starfive_hdmi_tx_phy_power_down(hdmi);
	starfive_hdmi_config_video_timing(hdmi, mode);
	starfive_hdmi_tx_phy_power_on(hdmi);

	starfive_hdmi_tmds_driver_on(hdmi);
	starfive_hdmi_sync_tmds(hdmi);

	return 0;
}

static void starfive_hdmi_encoder_enable(struct drm_encoder *encoder)
{
	struct starfive_hdmi_encoder *hdmi_encoder = encoder_to_hdmi(encoder);
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	int ret, idx;
	struct drm_device *drm = hdmi_encoder->hdmi->connector.dev;

	if (drm && !drm_dev_enter(drm, &idx))
		return;

	ret = pm_runtime_get_sync(hdmi_encoder->hdmi->dev);
	if (ret < 0)
		return;
	starfive_hdmi_setup(hdmi_encoder->hdmi, mode);

	if (drm)
		drm_dev_exit(idx);
}

static void starfive_hdmi_encoder_disable(struct drm_encoder *encoder)
{
	struct starfive_hdmi_encoder *hdmi_encoder = encoder_to_hdmi(encoder);

	pm_runtime_put(hdmi_encoder->hdmi->dev);
}

static int
starfive_hdmi_encoder_atomic_check(struct drm_encoder *encoder,
				   struct drm_crtc_state *crtc_state,
				   struct drm_connector_state *conn_state)
{
	bool valid = false;
	struct drm_display_mode *mode = &crtc_state->adjusted_mode;
	struct vs_crtc_state *vs_crtc_state = to_vs_crtc_state(crtc_state);

	vs_crtc_state->encoder_type = encoder->encoder_type;
	vs_crtc_state->output_fmt = MEDIA_BUS_FMT_RGB888_1X24;

	int pclk = mode->clock * 1000;

	if (pclk <= PIXCLOCK_4K_30FPS)
		valid = true;

	return (valid) ? 0 : -EINVAL;
}

static const struct drm_encoder_helper_funcs starfive_hdmi_encoder_helper_funcs = {
	.enable     = starfive_hdmi_encoder_enable,
	.disable    = starfive_hdmi_encoder_disable,
	.atomic_check = starfive_hdmi_encoder_atomic_check,
};

static enum drm_connector_status
starfive_hdmi_connector_detect(struct drm_connector *connector, bool force)
{
	struct starfive_hdmi *hdmi = connector_to_hdmi(connector);
	struct drm_device *drm = hdmi->connector.dev;
	int ret;
	int idx;

	if (drm && !drm_dev_enter(drm, &idx))
		return connector_status_disconnected;

	ret = pm_runtime_get_sync(hdmi->dev);
	if (ret < 0)
		return ret;

	ret = (hdmi_readb(hdmi, HDMI_STATUS) & m_HOTPLUG) ?
		connector_status_connected : connector_status_disconnected;
	pm_runtime_put(hdmi->dev);

	if (drm)
		drm_dev_exit(idx);

	return ret;
}

static int starfive_hdmi_connector_get_modes(struct drm_connector *connector)
{
	struct starfive_hdmi *hdmi = connector_to_hdmi(connector);
	int ret = 0;

	if (!hdmi->ddc)
		return 0;
	ret = pm_runtime_get_sync(hdmi->dev);
	if (ret < 0)
		return ret;

	ret = drm_connector_helper_get_modes_from_ddc(connector);
	pm_runtime_put(hdmi->dev);

	return ret;
}

static enum drm_mode_status
starfive_hdmi_connector_mode_valid(struct drm_connector *connector,
				   struct drm_display_mode *mode)
{
	int pclk = mode->clock * 1000;
	bool valid = false;

	if (pclk <= PIXCLOCK_4K_30FPS)
		valid = true;

	return (valid) ? MODE_OK : MODE_BAD;
}

static int
starfive_hdmi_probe_single_connector_modes(struct drm_connector *connector,
					   u32 maxX, u32 maxY)
{
	return drm_helper_probe_single_connector_modes(connector, 3840, 2160);
}

static const struct drm_connector_funcs starfive_hdmi_connector_funcs = {
	.fill_modes = starfive_hdmi_probe_single_connector_modes,
	.detect = starfive_hdmi_connector_detect,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static struct drm_connector_helper_funcs starfive_hdmi_connector_helper_funcs = {
	.get_modes = starfive_hdmi_connector_get_modes,
	.mode_valid = starfive_hdmi_connector_mode_valid,
};

static int starfive_hdmi_register(struct drm_device *drm,
				  struct starfive_hdmi_encoder *hdmi_encoder)
{
	struct drm_encoder *encoder = &hdmi_encoder->encoder;
	struct device *dev = hdmi_encoder->hdmi->dev;

	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);

	/*
	 * If we failed to find the CRTC(s) which this encoder is
	 * supposed to be connected to, it's because the CRTC has
	 * not been registered yet.  Defer probing, and hope that
	 * the required CRTC is added later.
	 */
	if (encoder->possible_crtcs == 0)
		return -EPROBE_DEFER;

	drm_encoder_helper_add(encoder, &starfive_hdmi_encoder_helper_funcs);

	hdmi_encoder->hdmi->connector.polled = DRM_CONNECTOR_POLL_HPD;

	drm_connector_helper_add(&hdmi_encoder->hdmi->connector,
				 &starfive_hdmi_connector_helper_funcs);
	drmm_connector_init(drm, &hdmi_encoder->hdmi->connector,
			    &starfive_hdmi_connector_funcs,
			    DRM_MODE_CONNECTOR_HDMIA,
			    hdmi_encoder->hdmi->ddc);

	drm_connector_attach_encoder(&hdmi_encoder->hdmi->connector, encoder);

	return 0;
}

static irqreturn_t starfive_hdmi_i2c_irq(struct starfive_hdmi *hdmi)
{
	struct starfive_hdmi_i2c *i2c = hdmi->i2c;
	u8 stat;

	stat = hdmi_readb(hdmi, HDMI_INTERRUPT_STATUS1);
	if (!(stat & m_INT_EDID_READY))
		return IRQ_NONE;

	/* Clear HDMI EDID interrupt flag */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, m_INT_EDID_READY);

	complete(&i2c->cmp);

	return IRQ_HANDLED;
}

static irqreturn_t starfive_hdmi_hardirq(int irq, void *dev_id)
{
	struct starfive_hdmi *hdmi = dev_id;
	irqreturn_t ret = IRQ_NONE;
	u8 interrupt;

	if (hdmi->i2c)
		ret = starfive_hdmi_i2c_irq(hdmi);

	interrupt = hdmi_readb(hdmi, HDMI_STATUS);
	if (interrupt & m_INT_HOTPLUG) {
		hdmi_modb(hdmi, HDMI_STATUS, m_INT_HOTPLUG, m_INT_HOTPLUG);
		ret = IRQ_WAKE_THREAD;
	}

	return ret;
}

static irqreturn_t starfive_hdmi_irq(int irq, void *dev_id)
{
	struct starfive_hdmi *hdmi = dev_id;

	drm_connector_helper_hpd_irq_event(&hdmi->connector);

	return IRQ_HANDLED;
}

static int starfive_hdmi_i2c_read(struct starfive_hdmi *hdmi, struct i2c_msg *msgs)
{
	int length = msgs->len;
	u8 *buf = msgs->buf;
	int ret;

	ret = wait_for_completion_timeout(&hdmi->i2c->cmp, HZ / 10);
	if (!ret)
		return -EAGAIN;

	while (length--)
		*buf++ = hdmi_readb(hdmi, HDMI_EDID_FIFO_ADDR);

	return 0;
}

static int starfive_hdmi_i2c_write(struct starfive_hdmi *hdmi, struct i2c_msg *msgs)
{
	/*
	 * The DDC module only support read EDID message, so
	 * we assume that each word write to this i2c adapter
	 * should be the offset of EDID word address.
	 */
	if (msgs->len != 1 ||
	    (msgs->addr != DDC_ADDR && msgs->addr != DDC_SEGMENT_ADDR))
		return -EINVAL;

	reinit_completion(&hdmi->i2c->cmp);

	if (msgs->addr == DDC_SEGMENT_ADDR)
		hdmi->i2c->segment_addr = msgs->buf[0];
	if (msgs->addr == DDC_ADDR)
		hdmi->i2c->ddc_addr = msgs->buf[0];

	/* Set edid fifo first addr */
	hdmi_writeb(hdmi, HDMI_EDID_FIFO_OFFSET, 0x00);

	/* Set edid word address 0x00/0x80 */
	hdmi_writeb(hdmi, HDMI_EDID_WORD_ADDR, hdmi->i2c->ddc_addr);

	/* Set edid segment pointer */
	hdmi_writeb(hdmi, HDMI_EDID_SEGMENT_POINTER, hdmi->i2c->segment_addr);

	return 0;
}

static int starfive_hdmi_i2c_xfer(struct i2c_adapter *adap,
				  struct i2c_msg *msgs, int num)
{
	struct starfive_hdmi *hdmi = i2c_get_adapdata(adap);
	struct starfive_hdmi_i2c *i2c = hdmi->i2c;
	int i, ret = 0;

	mutex_lock(&i2c->lock);

	/* Clear the EDID interrupt flag and unmute the interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, m_INT_EDID_READY);
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, m_INT_EDID_READY);

	for (i = 0; i < num; i++) {
		DRM_DEV_DEBUG(hdmi->dev,
			      "xfer: num: %d/%d, len: %d, flags: %#x\n",
			      i + 1, num, msgs[i].len, msgs[i].flags);

		if (msgs[i].flags & I2C_M_RD)
			ret = starfive_hdmi_i2c_read(hdmi, &msgs[i]);
		else
			ret = starfive_hdmi_i2c_write(hdmi, &msgs[i]);

		if (ret < 0)
			break;
	}

	if (!ret)
		ret = num;

	/* Mute HDMI EDID interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, 0);

	mutex_unlock(&i2c->lock);

	return ret;
}

static u32 starfive_hdmi_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm starfive_hdmi_algorithm = {
	.master_xfer	= starfive_hdmi_i2c_xfer,
	.functionality	= starfive_hdmi_i2c_func,
};

static struct i2c_adapter *starfive_hdmi_i2c_adapter(struct starfive_hdmi *hdmi)
{
	struct i2c_adapter *adap;
	struct starfive_hdmi_i2c *i2c;
	int ret;

	i2c = devm_kzalloc(hdmi->dev, sizeof(*i2c), GFP_KERNEL);
		if (!i2c)
			return ERR_PTR(-ENOMEM);

	mutex_init(&i2c->lock);
	init_completion(&i2c->cmp);

	adap = &i2c->adap;
	adap->class = I2C_CLASS_DDC;
	adap->owner = THIS_MODULE;
	adap->dev.parent = hdmi->dev;
	adap->algo = &starfive_hdmi_algorithm;
	strscpy(adap->name, "Starfive HDMI", sizeof(adap->name));
	i2c_set_adapdata(adap, hdmi);

	ret = devm_i2c_add_adapter(hdmi->dev, adap);
	if (ret) {
		dev_warn(hdmi->dev, "cannot add %s I2C adapter\n", adap->name);
		devm_kfree(hdmi->dev, i2c);
		return ERR_PTR(ret);
	}

	hdmi->i2c = i2c;

	DRM_DEV_INFO(hdmi->dev, "registered %s I2C bus driver success\n", adap->name);

	return adap;
}

static int starfive_hdmi_get_clk_rst(struct device *dev, struct starfive_hdmi *hdmi)
{
	int ret;

	hdmi->nclks = ARRAY_SIZE(hdmi->clk_hdmi);
	for (int i = 0; i < hdmi->nclks; ++i)
		hdmi->clk_hdmi[i].id = hdmi_clocks[i];

	ret = devm_clk_bulk_get(dev, hdmi->nclks, hdmi->clk_hdmi);
	if (ret) {
		dev_err(dev, "Failed to get clk controls\n");
		return ret;
	}

	hdmi->tx_rst = devm_reset_control_get_by_index(dev, 0);
	if (IS_ERR(hdmi->tx_rst)) {
		dev_err(dev, "failed to get tx_rst reset\n");
		return PTR_ERR(hdmi->tx_rst);
	}

	return 0;
}

static int starfive_hdmi_bind(struct device *dev, struct device *master,
			      void *data)
{
	struct drm_device *drm = dev_get_drvdata(master);
	struct starfive_hdmi_encoder *hdmi_encoder;
	int ret;

	hdmi_encoder = drmm_simple_encoder_alloc(drm, struct starfive_hdmi_encoder,
						 encoder, DRM_MODE_ENCODER_TMDS);
	if (IS_ERR(hdmi_encoder))
		return PTR_ERR(hdmi_encoder);

	hdmi_encoder->hdmi = dev_get_drvdata(dev);
	hdmi_encoder->hdmi->drm_dev = drm;

	ret = pm_runtime_resume_and_get(dev);
	if (ret)
		return ret;

	starfive_hdmi_i2c_init(hdmi_encoder->hdmi);

	ret = starfive_hdmi_register(drm, hdmi_encoder);
	if (ret)
		goto err_put_adapter;

	/* Unmute hotplug interrupt */
	hdmi_modb(hdmi_encoder->hdmi, HDMI_STATUS, m_MASK_INT_HOTPLUG, v_MASK_INT_HOTPLUG(1));

	ret = devm_request_threaded_irq(dev, hdmi_encoder->hdmi->irq, starfive_hdmi_hardirq,
					starfive_hdmi_irq, IRQF_SHARED,
					dev_name(dev), hdmi_encoder->hdmi);
	if (ret < 0)
		goto err_put_adapter;

	pm_runtime_put_sync(dev);

	return 0;

err_put_adapter:
	i2c_put_adapter(hdmi_encoder->hdmi->ddc);
	return ret;
}

static const struct component_ops starfive_hdmi_ops = {
	.bind	= starfive_hdmi_bind,
};

static int starfive_hdmi_probe(struct platform_device *pdev)
{
	int ret;
	struct starfive_hdmi *hdmi;
	struct resource *iores;

	hdmi = devm_kzalloc(&pdev->dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, hdmi);
	hdmi->dev = &pdev->dev;

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hdmi->regs = devm_ioremap_resource(hdmi->dev, iores);
	if (IS_ERR(hdmi->regs))
		return PTR_ERR(hdmi->regs);

	ret = starfive_hdmi_get_clk_rst(hdmi->dev, hdmi);
	if (ret < 0)
		return ret;

	ret = devm_pm_runtime_enable(hdmi->dev);
	if (ret)
		return ret;

	hdmi->irq = platform_get_irq(pdev, 0);
	if (hdmi->irq < 0) {
		ret = hdmi->irq;
		return ret;
	}

	hdmi->ddc = starfive_hdmi_i2c_adapter(hdmi);
	if (IS_ERR(hdmi->ddc)) {
		ret = PTR_ERR(hdmi->ddc);
		hdmi->ddc = NULL;
		return ret;
	}

	return component_add(&pdev->dev, &starfive_hdmi_ops);
}

static int starfive_hdmi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &starfive_hdmi_ops);

	return 0;
}

static const struct dev_pm_ops hdmi_pm_ops = {
	SET_RUNTIME_PM_OPS(hdmi_runtime_suspend, hdmi_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(hdmi_system_pm_suspend, hdmi_system_pm_resume)
};

static const struct of_device_id starfive_hdmi_dt_ids[] = {
	{ .compatible = "starfive,jh7110-inno-hdmi",},
	{},
};
MODULE_DEVICE_TABLE(of, starfive_hdmi_dt_ids);

struct platform_driver starfive_hdmi_driver = {
	.probe  = starfive_hdmi_probe,
	.remove = starfive_hdmi_remove,
	.driver = {
		.name = "starfive-hdmi",
		.of_match_table = starfive_hdmi_dt_ids,
		.pm = &hdmi_pm_ops,
	},
};

MODULE_AUTHOR("StarFive Corporation");
MODULE_DESCRIPTION("Starfive HDMI Driver");
