// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <sound/designware_i2s.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>
#include <linux/kthread.h>

#include "i2svad.h"

/* vad control function*/
static void vad_start(struct vad_params *vad)
{
	regmap_update_bits(vad->vad_map, VAD_MEM_SW,
			VAD_MEM_SW_MASK, VAD_MEM_SW_TO_VAD);
	regmap_update_bits(vad->vad_map, VAD_SW,
			VAD_SW_MASK, VAD_SW_VAD_XMEM_ENABLE|VAD_SW_ADC_ENABLE);
	regmap_update_bits(vad->vad_map, VAD_SPINT_EN,
			VAD_SPINT_EN_MASK, VAD_SPINT_EN_ENABLE);
	regmap_update_bits(vad->vad_map, VAD_SLINT_EN,
			VAD_SLINT_EN_MASK, VAD_SLINT_EN_ENABLE);
}

static void vad_stop(struct vad_params *vad)
{
	regmap_update_bits(vad->vad_map, VAD_SPINT_EN,
			VAD_SPINT_EN_MASK, VAD_SLINT_EN_DISABLE);
	regmap_update_bits(vad->vad_map, VAD_SLINT_EN,
			VAD_SLINT_EN_MASK, VAD_SLINT_EN_DISABLE);
	regmap_update_bits(vad->vad_map, VAD_SW,
			VAD_SW_MASK, VAD_SW_VAD_XMEM_DISABLE|VAD_SW_ADC_DISABLE);
	regmap_update_bits(vad->vad_map, VAD_MEM_SW,
			VAD_MEM_SW_MASK, VAD_MEM_SW_TO_AXI);
}

static void vad_status(struct vad_params *vad)
{
	u32 sp_value,sp_en;
	u32 sl_value,sl_en;

	regmap_read(vad->vad_map, VAD_SPINT,&sp_value);
	regmap_read(vad->vad_map, VAD_SPINT_EN,&sp_en);
	if (sp_value&sp_en){
		regmap_update_bits(vad->vad_map, VAD_SPINT_CLR,
				VAD_SPINT_CLR_MASK, VAD_SPINT_CLR_VAD_SPINT);
		vad->vstatus = VAD_STATUS_SPINT;
		vad_stop(vad);
		vad_start(vad);
	}

	regmap_read(vad->vad_map, VAD_SLINT,&sl_value);
	regmap_read(vad->vad_map, VAD_SLINT_EN,&sl_en);
	if (sl_value&sl_en){
		regmap_update_bits(vad->vad_map, VAD_SLINT_CLR,
				VAD_SLINT_CLR_MASK, VAD_SLINT_CLR_VAD_SLINT);
		vad->vstatus = VAD_STATUS_SLINT;
	}
}

static int vad_trigger(struct vad_params *vad,int cmd)
{
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if(vad->vswitch)
		{
			vad_start(vad);
		}
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		vad_stop(vad);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static void vad_init(struct vad_params *vad)
{
	/* left_margin */
	regmap_update_bits(vad->vad_map, VAD_LEFT_MARGIN,
			VAD_LEFT_MARGIN_MASK, 0x0);
	/* right_margin */
	regmap_update_bits(vad->vad_map, VAD_RIGHT_MARGIN,
			VAD_RIGHT_MARGIN_MASK, 0x0);
	/*low-energy transition range threshold ——NL*/
	regmap_update_bits(vad->vad_map, VAD_N_LOW_CONT_FRAMES,
			VAD_N_LOW_CONT_FRAMES_MASK, 0x3);
	/* low-energy transition range */
	regmap_update_bits(vad->vad_map, VAD_N_LOW_SEEK_FRAMES,
			VAD_N_LOW_SEEK_FRAMES_MASK, 0x8);
	/* high-energy transition range threshold——NH */
	regmap_update_bits(vad->vad_map, VAD_N_HIGH_CONT_FRAMES,
			VAD_N_HIGH_CONT_FRAMES_MASK, 0x5);
	/* high-energy transition range */
	regmap_update_bits(vad->vad_map, VAD_N_HIGH_SEEK_FRAMES,
			VAD_N_HIGH_SEEK_FRAMES_MASK, 0x1E);
	/*low-energy voice range threshold——NVL*/
	regmap_update_bits(vad->vad_map, VAD_N_SPEECH_LOW_HIGH_FRAMES,
			VAD_N_SPEECH_LOW_HIGH_FRAMES_MASK, 0x2);
	/*low-energy voice range*/
	regmap_update_bits(vad->vad_map, VAD_N_SPEECH_LOW_SEEK_FRAMES,
			VAD_N_SPEECH_LOW_SEEK_FRAMES_MASK, 0x12);
	/*mean silence frame range*/
	regmap_update_bits(vad->vad_map, VAD_MEAN_SIL_FRAMES,
			VAD_MEAN_SIL_FRAMES_MASK, 0xA);
	/*low-energy threshold scaling factor,12bit(0~0xFFF)*/
	regmap_update_bits(vad->vad_map, VAD_N_ALPHA,
			VAD_N_ALPHA_MASK, 0x1A);
	/*high-energy threshold scaling factor,12bit(0~0xFFF)*/
	regmap_update_bits(vad->vad_map, VAD_N_BETA,
			VAD_N_BETA_MASK, 0x34);
	regmap_update_bits(vad->vad_map, VAD_LEFT_WD,
			VAD_LEFT_WD_MASK, VAD_LEFT_WD_BIT_15_0);
	regmap_update_bits(vad->vad_map, VAD_RIGHT_WD,
			VAD_RIGHT_WD_MASK, VAD_RIGHT_WD_BIT_15_0);
	regmap_update_bits(vad->vad_map, VAD_LR_SEL,
			VAD_LR_SEL_MASK, VAD_LR_SEL_L);
	regmap_update_bits(vad->vad_map, VAD_STOP_DELAY,
			VAD_STOP_DELAY_MASK, VAD_STOP_DELAY_0_SAMPLE);
	regmap_update_bits(vad->vad_map, VAD_ADDR_START,
			VAD_ADDR_START_MASK, 0x0);
	regmap_update_bits(vad->vad_map, VAD_ADDR_WRAP,
			VAD_ADDR_WRAP_MASK, 0x2000);
	regmap_update_bits(vad->vad_map, VAD_MEM_SW,
			VAD_MEM_SW_MASK, VAD_MEM_SW_TO_AXI);
	regmap_update_bits(vad->vad_map, VAD_SPINT_CLR,
			VAD_SPINT_CLR_MASK, VAD_SPINT_CLR_VAD_SPINT);
	regmap_update_bits(vad->vad_map, VAD_SLINT_CLR,
			VAD_SLINT_CLR_MASK, VAD_SLINT_CLR_VAD_SLINT);
}


static int vad_switch_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int vad_switch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct i2svad_dev *dev = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = dev->vad.vswitch;

	return 0;
}

static int vad_switch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct i2svad_dev *dev = snd_soc_component_get_drvdata(component);
	int val;

	val = ucontrol->value.integer.value[0];
	if (val && !dev->vad.vswitch) {
		dev->vad.vswitch = true;
	} else if (!val && dev->vad.vswitch) {
		dev->vad.vswitch = false;
		vad_stop(&(dev->vad));
	}

	return 0;
}


static int vad_status_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 2;

	return 0;
}

static int vad_status_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct i2svad_dev *dev = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = dev->vad.vstatus;
	dev->vad.vstatus = VAD_STATUS_NORMAL;

	return 0;
}


#define SOC_VAD_SWITCH_DECL(xname) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = vad_switch_info, .get = vad_switch_get, \
	.put = vad_switch_put, }

#define SOC_VAD_STATUS_DECL(xname) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = vad_status_info, .get = vad_status_get, }


static const struct snd_kcontrol_new vad_snd_controls[] = {
	SOC_VAD_SWITCH_DECL("vad switch"),
	SOC_VAD_STATUS_DECL("vad status"),
};

static int vad_probe(struct snd_soc_component *component)
{
	struct i2svad_dev *priv = snd_soc_component_get_drvdata(component);

	snd_soc_component_init_regmap(component, priv->vad.vad_map);
	snd_soc_add_component_controls(component, vad_snd_controls,
				ARRAY_SIZE(vad_snd_controls));

	return 0;
}

/* i2s control function*/
static inline void i2s_write_reg(void __iomem *io_base, int reg, u32 val)
{
	writel(val, io_base + reg);
}

static inline u32 i2s_read_reg(void __iomem *io_base, int reg)
{
	return readl(io_base + reg);
}

static inline void i2s_disable_channels(struct i2svad_dev *dev, u32 stream)
{
	u32 i = 0;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < ALL_CHANNEL_NUM; i++)
			i2s_write_reg(dev->i2s_base, TER(i), 0);
	} else {
		for (i = 0; i < ALL_CHANNEL_NUM; i++)
			i2s_write_reg(dev->i2s_base, RER(i), 0);
	}
}

static inline void i2s_clear_irqs(struct i2svad_dev *dev, u32 stream)
{
	u32 i = 0;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < ALL_CHANNEL_NUM; i++)
			i2s_read_reg(dev->i2s_base, TOR(i));
	} else {
		for (i = 0; i < ALL_CHANNEL_NUM; i++)
			i2s_read_reg(dev->i2s_base, ROR(i));
	}
}

static inline void i2s_disable_irqs(struct i2svad_dev *dev, u32 stream,
				int chan_nr)
{
	u32 i, irq;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < (chan_nr / 2); i++) {
			irq = i2s_read_reg(dev->i2s_base, IMR(i));
			i2s_write_reg(dev->i2s_base, IMR(i), irq | 0x30);
		}
	} else {
		for (i = 0; i < (chan_nr / 2); i++) {
			irq = i2s_read_reg(dev->i2s_base, IMR(i));
			i2s_write_reg(dev->i2s_base, IMR(i), irq | 0x03);
		}
	}
}

static inline void i2s_enable_irqs(struct i2svad_dev *dev, u32 stream,
				int chan_nr)
{
	u32 i, irq;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < (chan_nr / 2); i++) {
			irq = i2s_read_reg(dev->i2s_base, IMR(i));
			i2s_write_reg(dev->i2s_base, IMR(i), irq & ~0x30);
		}
	} else {
		for (i = 0; i < (chan_nr / 2); i++) {
			irq = i2s_read_reg(dev->i2s_base, IMR(i));
			i2s_write_reg(dev->i2s_base, IMR(i), irq & ~0x03);
		}
	}
}

static irqreturn_t i2s_irq_handler(int irq, void *dev_id)
{
	struct i2svad_dev *dev = dev_id;
	bool irq_valid = false;
	u32 isr[4];
	int i;

	for (i = 0; i < ALL_CHANNEL_NUM; i++)
		isr[i] = i2s_read_reg(dev->i2s_base, ISR(i));

	i2s_clear_irqs(dev, SNDRV_PCM_STREAM_PLAYBACK);
	i2s_clear_irqs(dev, SNDRV_PCM_STREAM_CAPTURE);

	for (i = 0; i < 4; i++) {
		/*
		 * Check if TX fifo is empty. If empty fill FIFO with samples
		 * NOTE: Only two channels supported
		 */
		if ((isr[i] & ISR_TXFE) && (i == 0) && dev->use_pio) {
			i2svad_pcm_push_tx(dev);
			irq_valid = true;
		}

		/*
		 * Data available. Retrieve samples from FIFO
		 * NOTE: Only two channels supported
		 */
		if ((isr[i] & ISR_RXDA) && (i == 0) && dev->use_pio) {
			i2svad_pcm_pop_rx(dev);
			irq_valid = true;
		}

		/* Error Handling: TX */
		if (isr[i] & ISR_TXFO) {
			//dev_err(dev->dev, "TX overrun (ch_id=%d)\n", i);
			irq_valid = true;
		}

		/* Error Handling: TX */
		if (isr[i] & ISR_RXFO) {
			//dev_err(dev->dev, "RX overrun (ch_id=%d)\n", i);
			irq_valid = true;
		}
	}

	vad_status(&(dev->vad));

	if (irq_valid)
		return IRQ_HANDLED;
	else
		return IRQ_NONE;
}

static void i2s_start(struct i2svad_dev *dev,
			struct snd_pcm_substream *substream)
{
	struct i2s_clk_config_data *config = &dev->config;

	i2s_write_reg(dev->i2s_base, IER, 1);
	i2s_enable_irqs(dev, substream->stream, config->chan_nr);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		i2s_write_reg(dev->i2s_base, ITER, 1);
	else
		i2s_write_reg(dev->i2s_base, IRER, 1);

	i2s_write_reg(dev->i2s_base, CER, 1);
}

static void i2s_stop(struct i2svad_dev *dev,
		struct snd_pcm_substream *substream)
{

	i2s_clear_irqs(dev, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		i2s_write_reg(dev->i2s_base, ITER, 0);
	else
		i2s_write_reg(dev->i2s_base, IRER, 0);

	i2s_disable_irqs(dev, substream->stream, 8);

	if (!dev->active) {
		i2s_write_reg(dev->i2s_base, CER, 0);
		i2s_write_reg(dev->i2s_base, IER, 0);
	}
}

static int dw_i2s_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *cpu_dai)
{
	struct i2svad_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	union dw_i2s_snd_dma_data *dma_data = NULL;


	if (!(dev->capability & DWC_I2S_RECORD) &&
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE))
		return -EINVAL;

	if (!(dev->capability & DWC_I2S_PLAY) &&
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK))
		return -EINVAL;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = &dev->play_dma_data;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		dma_data = &dev->capture_dma_data;

	snd_soc_dai_set_dma_data(cpu_dai, substream, (void *)dma_data);

	return 0;
}

static void dw_i2s_config(struct i2svad_dev *dev, int stream)
{
	u32 ch_reg;
	struct i2s_clk_config_data *config = &dev->config;


	i2s_disable_channels(dev, stream);

	for (ch_reg = 0; ch_reg < (config->chan_nr / 2); ch_reg++) {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			i2s_write_reg(dev->i2s_base, TCR(ch_reg),
					dev->xfer_resolution);
			i2s_write_reg(dev->i2s_base, TFCR(ch_reg),
					dev->fifo_th - 1);
			i2s_write_reg(dev->i2s_base, TER(ch_reg), 1);
		} else {
			i2s_write_reg(dev->i2s_base, RCR(ch_reg),
					dev->xfer_resolution);
			i2s_write_reg(dev->i2s_base, RFCR(ch_reg),
					dev->fifo_th - 1);
			i2s_write_reg(dev->i2s_base, RER(ch_reg), 1);
		}

	}
}

static int dw_i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct i2svad_dev *dev = snd_soc_dai_get_drvdata(dai);
	struct i2s_clk_config_data *config = &dev->config;
	int ret;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		config->data_width = 16;
		dev->ccr = 0x00;
		dev->xfer_resolution = 0x02;
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		config->data_width = 24;
		dev->ccr = 0x08;
		dev->xfer_resolution = 0x04;
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		config->data_width = 32;
		dev->ccr = 0x10;
		dev->xfer_resolution = 0x05;
		break;

	default:
		dev_err(dev->dev, "designware-i2s: unsupported PCM fmt");
		return -EINVAL;
	}

	config->chan_nr = params_channels(params);

	switch (config->chan_nr) {
	case EIGHT_CHANNEL_SUPPORT:
	case SIX_CHANNEL_SUPPORT:
	case FOUR_CHANNEL_SUPPORT:
	case TWO_CHANNEL_SUPPORT:
		break;
	default:
		dev_err(dev->dev, "channel not supported\n");
		return -EINVAL;
	}

	dw_i2s_config(dev, substream->stream);

	i2s_write_reg(dev->i2s_base, CCR, dev->ccr);

	config->sample_rate = params_rate(params);

	if (dev->capability & DW_I2S_MASTER) {
		if (dev->i2s_clk_cfg) {
			ret = dev->i2s_clk_cfg(config);
			if (ret < 0) {
				dev_err(dev->dev, "runtime audio clk config fail\n");
				return ret;
			}
		} else {
			u32 bitclk = config->sample_rate *
					config->data_width * 2;

			ret = clk_set_rate(dev->clk, bitclk);
			if (ret) {
				dev_err(dev->dev, "Can't set I2S clock rate: %d\n",
					ret);
				return ret;
			}
		}
	}
	return 0;
}

static void dw_i2s_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	snd_soc_dai_set_dma_data(dai, substream, NULL);
}

static int dw_i2s_prepare(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct i2svad_dev *dev = snd_soc_dai_get_drvdata(dai);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		i2s_write_reg(dev->i2s_base, TXFFR, 1);
	else
		i2s_write_reg(dev->i2s_base, RXFFR, 1);

	return 0;
}

static int dw_i2s_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	struct i2svad_dev *dev = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dev->active++;
		i2s_start(dev, substream);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		dev->active--;
		i2s_stop(dev, substream);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
	{
		vad_trigger(&(dev->vad),cmd);
	}
	return ret;
}

static int dw_i2s_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct i2svad_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	int ret = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		if (dev->capability & DW_I2S_SLAVE)
			ret = 0;
		else
			ret = -EINVAL;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		if (dev->capability & DW_I2S_MASTER)
			ret = 0;
		else
			ret = -EINVAL;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
	case SND_SOC_DAIFMT_CBS_CFM:
		ret = -EINVAL;
		break;
	default:
		dev_dbg(dev->dev, "dwc : Invalid master/slave format\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static const struct snd_soc_dai_ops dw_i2s_dai_ops = {
	.startup	= dw_i2s_startup,
	.shutdown	= dw_i2s_shutdown,
	.hw_params	= dw_i2s_hw_params,
	.prepare	= dw_i2s_prepare,
	.trigger	= dw_i2s_trigger,
	.set_fmt	= dw_i2s_set_fmt,
};

#ifdef CONFIG_PM
static int dw_i2s_runtime_suspend(struct device *dev)
{
	struct i2svad_dev *dw_dev = dev_get_drvdata(dev);

	if (dw_dev->capability & DW_I2S_MASTER)
		clk_disable(dw_dev->clk);
	return 0;
}

static int dw_i2s_runtime_resume(struct device *dev)
{
	struct i2svad_dev *dw_dev = dev_get_drvdata(dev);

	if (dw_dev->capability & DW_I2S_MASTER)
		clk_enable(dw_dev->clk);
	return 0;
}

static int dw_i2s_suspend(struct snd_soc_component *component)
{
	struct i2svad_dev *dev = snd_soc_component_get_drvdata(component);

	if (dev->capability & DW_I2S_MASTER)
		clk_disable(dev->clk);
	return 0;
}

static int dw_i2s_resume(struct snd_soc_component *component)
{
	struct i2svad_dev *dev = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *dai;
	int stream;

	if (dev->capability & DW_I2S_MASTER)
		clk_enable(dev->clk);

	for_each_component_dais(component, dai) {
		for_each_pcm_streams(stream)
			if (snd_soc_dai_stream_active(dai, stream))
				dw_i2s_config(dev, stream);
	}

	return 0;
}

#else
#define dw_i2s_suspend	NULL
#define dw_i2s_resume	NULL
#endif

static int dw_i2svad_probe(struct snd_soc_component *component)
{
	vad_probe(component);
	return 0;
}

static const struct snd_soc_component_driver dw_i2s_component = {
	.name		= "dw-i2s",
	.probe		= dw_i2svad_probe,
	.suspend	= dw_i2s_suspend,
	.resume		= dw_i2s_resume,
};

/*
 * The following tables allow a direct lookup of various parameters
 * defined in the I2S block's configuration in terms of sound system
 * parameters.  Each table is sized to the number of entries possible
 * according to the number of configuration bits describing an I2S
 * block parameter.
 */

/* Maximum bit resolution of a channel - not uniformly spaced */
static const u32 fifo_width[COMP_MAX_WORDSIZE] = {
	12, 16, 20, 24, 32, 0, 0, 0
};

/* Width of (DMA) bus */
static const u32 bus_widths[COMP_MAX_DATA_WIDTH] = {
	DMA_SLAVE_BUSWIDTH_1_BYTE,
	DMA_SLAVE_BUSWIDTH_2_BYTES,
	DMA_SLAVE_BUSWIDTH_4_BYTES,
	DMA_SLAVE_BUSWIDTH_UNDEFINED
};

/* PCM format to support channel resolution */
static const u32 formats[COMP_MAX_WORDSIZE] = {
	SNDRV_PCM_FMTBIT_S16_LE,
	SNDRV_PCM_FMTBIT_S16_LE,
	SNDRV_PCM_FMTBIT_S24_LE,
	SNDRV_PCM_FMTBIT_S24_LE,
	SNDRV_PCM_FMTBIT_S32_LE,
	0,
	0,
	0
};

static const struct regmap_config sf_i2s_regmap_cfg = {
	.reg_bits	= 32,
	.val_bits	= 32,
	.reg_stride	= 4,
	.max_register	= 0x1000,
};

static int dw_configure_dai(struct i2svad_dev *dev,
				struct snd_soc_dai_driver *dw_i2s_dai,
				unsigned int rates)
{
	/*
	 * Read component parameter registers to extract
	 * the I2S block's configuration.
	 */
	u32 comp1 = i2s_read_reg(dev->i2s_base, dev->i2s_reg_comp1);
	u32 comp2 = i2s_read_reg(dev->i2s_base, dev->i2s_reg_comp2);
	u32 fifo_depth = 1 << (1 + COMP1_FIFO_DEPTH_GLOBAL(comp1));
	u32 idx;

	if (dev->capability & DWC_I2S_RECORD &&
			dev->quirks & DW_I2S_QUIRK_COMP_PARAM1)
		comp1 = comp1 & ~BIT(5);

	if (dev->capability & DWC_I2S_PLAY &&
			dev->quirks & DW_I2S_QUIRK_COMP_PARAM1)
		comp1 = comp1 & ~BIT(6);

	if (COMP1_TX_ENABLED(comp1)) {
		dev_dbg(dev->dev, " designware: play supported\n");
		idx = COMP1_TX_WORDSIZE_0(comp1);
		if (WARN_ON(idx >= ARRAY_SIZE(formats)))
			return -EINVAL;
		if (dev->quirks & DW_I2S_QUIRK_16BIT_IDX_OVERRIDE)
			idx = 1;
		dw_i2s_dai->playback.channels_min = MIN_CHANNEL_NUM;
		dw_i2s_dai->playback.channels_max =
				1 << (COMP1_TX_CHANNELS(comp1) + 1);
		//dw_i2s_dai->playback.formats = formats[idx];
		dw_i2s_dai->playback.formats = SNDRV_PCM_FMTBIT_S16_LE;
		dw_i2s_dai->playback.rates = rates;
	}

	if (COMP1_RX_ENABLED(comp1)) {
		dev_dbg(dev->dev, "designware: record supported\n");
		idx = COMP2_RX_WORDSIZE_0(comp2);
		if (WARN_ON(idx >= ARRAY_SIZE(formats)))
			return -EINVAL;
		if (dev->quirks & DW_I2S_QUIRK_16BIT_IDX_OVERRIDE)
			idx = 1;
		dw_i2s_dai->capture.channels_min = MIN_CHANNEL_NUM;
		dw_i2s_dai->capture.channels_max =
				1 << (COMP1_RX_CHANNELS(comp1) + 1);
		//dw_i2s_dai->capture.formats = formats[idx];
		dw_i2s_dai->capture.formats = SNDRV_PCM_FMTBIT_S16_LE;
		dw_i2s_dai->capture.rates = rates;
	}

	if (COMP1_MODE_EN(comp1)) {
		dev_dbg(dev->dev, "designware: i2s master mode supported\n");
		dev->capability |= DW_I2S_MASTER;
	} else {
		dev_dbg(dev->dev, "designware: i2s slave mode supported\n");
		dev->capability |= DW_I2S_SLAVE;
	}

	dev->fifo_th = fifo_depth / 2;
	return 0;
}

static int dw_configure_dai_by_pd(struct i2svad_dev *dev,
				struct snd_soc_dai_driver *dw_i2s_dai,
				struct resource *res,
				const struct i2s_platform_data *pdata)
{
	u32 comp1 = i2s_read_reg(dev->i2s_base, dev->i2s_reg_comp1);
	u32 idx = COMP1_APB_DATA_WIDTH(comp1);
	int ret;

	if (WARN_ON(idx >= ARRAY_SIZE(bus_widths)))
		return -EINVAL;

	ret = dw_configure_dai(dev, dw_i2s_dai, pdata->snd_rates);
	if (ret < 0)
		return ret;

	if (dev->quirks & DW_I2S_QUIRK_16BIT_IDX_OVERRIDE)
		idx = 1;
	/* Set DMA slaves info */
	dev->play_dma_data.pd.data = pdata->play_dma_data;
	dev->capture_dma_data.pd.data = pdata->capture_dma_data;
	dev->play_dma_data.pd.addr = res->start + I2S_TXDMA;
	dev->capture_dma_data.pd.addr = res->start + I2S_RXDMA;
	dev->play_dma_data.pd.max_burst = 16;
	dev->capture_dma_data.pd.max_burst = 16;
	dev->play_dma_data.pd.addr_width = bus_widths[idx];
	dev->capture_dma_data.pd.addr_width = bus_widths[idx];
	dev->play_dma_data.pd.filter = pdata->filter;
	dev->capture_dma_data.pd.filter = pdata->filter;

	return 0;
}

static int dw_configure_dai_by_dt(struct i2svad_dev *dev,
				struct snd_soc_dai_driver *dw_i2s_dai,
				struct resource *res)
{
	u32 comp1 = i2s_read_reg(dev->i2s_base, I2S_COMP_PARAM_1);
	u32 comp2 = i2s_read_reg(dev->i2s_base, I2S_COMP_PARAM_2);
	u32 fifo_depth = 1 << (1 + COMP1_FIFO_DEPTH_GLOBAL(comp1));
	u32 idx = COMP1_APB_DATA_WIDTH(comp1);
	u32 idx2;
	int ret;

	if (WARN_ON(idx >= ARRAY_SIZE(bus_widths)))
		return -EINVAL;

	ret = dw_configure_dai(dev, dw_i2s_dai, SNDRV_PCM_RATE_8000_192000);
	if (ret < 0)
		return ret;

	if (COMP1_TX_ENABLED(comp1)) {
		idx2 = COMP1_TX_WORDSIZE_0(comp1);

		dev->capability |= DWC_I2S_PLAY;
		dev->play_dma_data.dt.addr = res->start + I2S_TXDMA;
		dev->play_dma_data.dt.addr_width = bus_widths[idx];
		dev->play_dma_data.dt.fifo_size = fifo_depth *
			(fifo_width[idx2]) >> 8;
		dev->play_dma_data.dt.maxburst = 16;
	}
	if (COMP1_RX_ENABLED(comp1)) {
		idx2 = COMP2_RX_WORDSIZE_0(comp2);

		dev->capability |= DWC_I2S_RECORD;
		dev->capture_dma_data.dt.addr = res->start + I2S_RXDMA;
		dev->capture_dma_data.dt.addr_width = bus_widths[idx];
		dev->capture_dma_data.dt.fifo_size = fifo_depth *
			(fifo_width[idx2] >> 8);
		dev->capture_dma_data.dt.maxburst = 16;
	}

	return 0;

}

static int dw_i2s_probe(struct platform_device *pdev)
{
	const struct i2s_platform_data *pdata = pdev->dev.platform_data;
	struct i2svad_dev *dev;
	struct resource *res;
	int ret, irq;
	struct snd_soc_dai_driver *dw_i2s_dai;
	const char *clk_id;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dw_i2s_dai = devm_kzalloc(&pdev->dev, sizeof(*dw_i2s_dai), GFP_KERNEL);
	if (!dw_i2s_dai)
		return -ENOMEM;

	dw_i2s_dai->ops = &dw_i2s_dai_ops;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dev->i2s_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dev->i2s_base))
		return PTR_ERR(dev->i2s_base);

	dev->vad.vad_base = dev->i2s_base;
	dev->vad.vad_map = devm_regmap_init_mmio(&pdev->dev, dev->i2s_base, &sf_i2s_regmap_cfg);
	if (IS_ERR(dev->vad.vad_map)) {
		dev_err(&pdev->dev, "failed to init regmap: %ld\n",
			PTR_ERR(dev->vad.vad_map));
		return PTR_ERR(dev->vad.vad_map);
	}

	dev->dev = &pdev->dev;

	dev->clk_apb_i2svad = devm_clk_get(&pdev->dev, "i2svad_apb");
	if (IS_ERR(dev->clk_apb_i2svad))
		return dev_err_probe(&pdev->dev, PTR_ERR(dev->clk_apb_i2svad),
				     "failed to get apb clock\n");

	dev->rst_apb_i2svad = devm_reset_control_get_exclusive(&pdev->dev, "apb_i2svad");
	if (IS_ERR(dev->rst_apb_i2svad))
		return dev_err_probe(&pdev->dev, PTR_ERR(dev->rst_apb_i2svad),
				     "failed to get apb reset\n");

	dev->rst_i2svad_srst = devm_reset_control_get_exclusive(&pdev->dev, "i2svad_srst");
	if (IS_ERR(dev->rst_i2svad_srst))
		return dev_err_probe(&pdev->dev, PTR_ERR(dev->rst_i2svad_srst),
				     "failed to get source reset\n");

	ret = clk_prepare_enable(dev->clk_apb_i2svad);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to enable apb clock\n");

	ret = reset_control_deassert(dev->rst_apb_i2svad);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to deassert apb reset\n");

	ret = reset_control_deassert(dev->rst_i2svad_srst);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to deassert source reset\n");

	irq = platform_get_irq(pdev, 0);
	if (irq >= 0) {
		ret = devm_request_irq(&pdev->dev, irq, i2s_irq_handler, 0,
				pdev->name, dev);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to request irq\n");
			return ret;
		}
	}

	dev->i2s_reg_comp1 = I2S_COMP_PARAM_1;
	dev->i2s_reg_comp2 = I2S_COMP_PARAM_2;
	if (pdata) {
		dev->capability = pdata->cap;
		clk_id = NULL;
		dev->quirks = pdata->quirks;
		if (dev->quirks & DW_I2S_QUIRK_COMP_REG_OFFSET) {
			dev->i2s_reg_comp1 = pdata->i2s_reg_comp1;
			dev->i2s_reg_comp2 = pdata->i2s_reg_comp2;
		}
		ret = dw_configure_dai_by_pd(dev, dw_i2s_dai, res, pdata);
	} else {
		clk_id = "i2sclk";
		ret = dw_configure_dai_by_dt(dev, dw_i2s_dai, res);
	}
	if (ret < 0)
		return ret;

	if (dev->capability & DW_I2S_MASTER) {
		if (pdata) {
			dev->i2s_clk_cfg = pdata->i2s_clk_cfg;
			if (!dev->i2s_clk_cfg) {
				dev_err(&pdev->dev, "no clock configure method\n");
				return -ENODEV;
			}
		}
		dev->clk = devm_clk_get(&pdev->dev, clk_id);

		if (IS_ERR(dev->clk))
			return PTR_ERR(dev->clk);

		ret = clk_prepare_enable(dev->clk);
		if (ret < 0)
			return ret;
	}

	dev_set_drvdata(&pdev->dev, dev);
	ret = devm_snd_soc_register_component(&pdev->dev, &dw_i2s_component,
					 dw_i2s_dai, 1);
	if (ret != 0) {
		dev_err(&pdev->dev, "not able to register dai\n");
		goto err_clk_disable;
	}

	if (!pdata) {
		if (irq >= 0) {
			ret = i2svad_pcm_register(pdev);
			dev->use_pio = true;
		} else {
			ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL,
					0);
			dev->use_pio = false;
		}

		if (ret) {
			dev_err(&pdev->dev, "could not register pcm: %d\n",
					ret);
			goto err_clk_disable;
		}
	}

	vad_init(&(dev->vad));
	pm_runtime_enable(&pdev->dev);

	return 0;

err_clk_disable:
	if (dev->capability & DW_I2S_MASTER)
		clk_disable_unprepare(dev->clk);
	return ret;
}

static int dw_i2s_remove(struct platform_device *pdev)
{
	struct i2svad_dev *dev = dev_get_drvdata(&pdev->dev);

	if (dev->capability & DW_I2S_MASTER)
		clk_disable_unprepare(dev->clk);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dw_i2s_of_match[] = {
	{ .compatible = "starfive,sf-i2svad", },
	{},
};

MODULE_DEVICE_TABLE(of, dw_i2s_of_match);
#endif

static const struct dev_pm_ops dwc_pm_ops = {
	SET_RUNTIME_PM_OPS(dw_i2s_runtime_suspend, dw_i2s_runtime_resume, NULL)
};

static struct platform_driver dw_i2s_driver = {
	.probe		= dw_i2s_probe,
	.remove		= dw_i2s_remove,
	.driver		= {
		.name	= "sf-i2svad",
		.of_match_table = of_match_ptr(dw_i2s_of_match),
		.pm = &dwc_pm_ops,
	},
};

module_platform_driver(dw_i2s_driver);

MODULE_AUTHOR("jenny zhang <jenny.zhang@starfivetech.com>");
MODULE_DESCRIPTION("starfive I2SVAD SoC Interface");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sf-i2svad");
