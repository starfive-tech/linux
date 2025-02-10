// SPDX-License-Identifier: GPL-2.0
/*
 * PWMDAC driver for the StarFive JH7100 SoC
 *
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
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>
#include "pwmdac.h"
#include <linux/kthread.h>

struct ct_pwmdac {
	char *name;
	unsigned int vals;
};

static const struct ct_pwmdac pwmdac_ct_shift_bit[] = {
	{ .name = "8bit", .vals = PWMDAC_SHIFT_8 },
	{ .name = "10bit", .vals = PWMDAC_SHIFT_10 }
};

static const struct ct_pwmdac pwmdac_ct_duty_cycle[] = {
	{ .name = "left", .vals = PWMDAC_CYCLE_LEFT },
	{ .name = "right", .vals = PWMDAC_CYCLE_RIGHT },
	{ .name = "center", .vals = PWMDAC_CYCLE_CENTER }
};

static const struct ct_pwmdac pwmdac_ct_data_mode[] = {
	{ .name = "unsinged", .vals = UNSINGED_DATA },
	{ .name = "inverter", .vals = INVERTER_DATA_MSB }
};

static const struct ct_pwmdac pwmdac_ct_lr_change[] = {
	{ .name = "no_change", .vals = NO_CHANGE },
	{ .name = "change", .vals = CHANGE }
};

static const struct ct_pwmdac pwmdac_ct_shift[] = {
	{ .name = "left 0 bit", .vals = PWMDAC_DATA_LEFT_SHIFT_BIT_0 },
	{ .name = "left 1 bit", .vals = PWMDAC_DATA_LEFT_SHIFT_BIT_1 },
	{ .name = "left 2 bit", .vals = PWMDAC_DATA_LEFT_SHIFT_BIT_2 },
	{ .name = "left 3 bit", .vals = PWMDAC_DATA_LEFT_SHIFT_BIT_3 },
	{ .name = "left 4 bit", .vals = PWMDAC_DATA_LEFT_SHIFT_BIT_4 },
	{ .name = "left 5 bit", .vals = PWMDAC_DATA_LEFT_SHIFT_BIT_5 },
	{ .name = "left 6 bit", .vals = PWMDAC_DATA_LEFT_SHIFT_BIT_6 }
};

static int pwmdac_shift_bit_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	unsigned int items = ARRAY_SIZE(pwmdac_ct_shift_bit);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = items;
	if (uinfo->value.enumerated.item >= items) {
		uinfo->value.enumerated.item = items - 1;
	}
	strcpy(uinfo->value.enumerated.name,
			pwmdac_ct_shift_bit[uinfo->value.enumerated.item].name);

	return 0;
}
static int pwmdac_shift_bit_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);
	unsigned int item;

	if (dev->shift_bit == pwmdac_ct_shift_bit[0].vals)
		item = 0;
	else
		item = 1;

	ucontrol->value.enumerated.item[0] = item;

	return 0;
}

static int pwmdac_shift_bit_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);
	int sel = ucontrol->value.enumerated.item[0];
	unsigned int items = ARRAY_SIZE(pwmdac_ct_shift_bit);

	if (sel > items)
		return 0;

	switch (sel) {
	case 1:
		dev->shift_bit = pwmdac_ct_shift_bit[1].vals;
		break;
	default:
		dev->shift_bit = pwmdac_ct_shift_bit[0].vals;
		break;
	}

	return 0;
}

static int pwmdac_duty_cycle_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	unsigned int items = ARRAY_SIZE(pwmdac_ct_duty_cycle);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = items;
	if (uinfo->value.enumerated.item >= items)
		uinfo->value.enumerated.item = items - 1;
	strcpy(uinfo->value.enumerated.name,
		pwmdac_ct_duty_cycle[uinfo->value.enumerated.item].name);

	return 0;
}

static int pwmdac_duty_cycle_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = dev->duty_cycle;
	return 0;
}

static int pwmdac_duty_cycle_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);
	int sel = ucontrol->value.enumerated.item[0];
	unsigned int items = ARRAY_SIZE(pwmdac_ct_duty_cycle);

	if (sel > items)
		return 0;

	dev->duty_cycle = pwmdac_ct_duty_cycle[sel].vals;
	return 0;
}

/*
static int pwmdac_datan_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 1;
	uinfo->value.integer.max = PWMDAC_SAMPLE_CNT_511;
	uinfo->value.integer.step = 1;
	return 0;
}

static int pwmdac_datan_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = dev->datan;

	return 0;
}

static int pwmdac_datan_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);
	int sel = ucontrol->value.integer.value[0];

	if (sel > PWMDAC_SAMPLE_CNT_511)
		return 0;

	dev->datan = sel;

	return 0;
}
*/

static int pwmdac_data_mode_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	unsigned int items = ARRAY_SIZE(pwmdac_ct_data_mode);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = items;
	if (uinfo->value.enumerated.item >= items)
		uinfo->value.enumerated.item = items - 1;
	strcpy(uinfo->value.enumerated.name,
		pwmdac_ct_data_mode[uinfo->value.enumerated.item].name);

	return 0;
}

static int pwmdac_data_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = dev->data_mode;
	return 0;
}

static int pwmdac_data_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);
	int sel = ucontrol->value.enumerated.item[0];
	unsigned int items = ARRAY_SIZE(pwmdac_ct_data_mode);

	if (sel > items)
		return 0;

	dev->data_mode = pwmdac_ct_data_mode[sel].vals;
	return 0;
}

static int pwmdac_shift_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	unsigned int items = ARRAY_SIZE(pwmdac_ct_shift);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = items;
	if (uinfo->value.enumerated.item >= items)
		uinfo->value.enumerated.item = items - 1;
	strcpy(uinfo->value.enumerated.name,
		pwmdac_ct_shift[uinfo->value.enumerated.item].name);

	return 0;
}

static int pwmdac_shift_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);
	unsigned int item = dev->shift;

	ucontrol->value.enumerated.item[0] = pwmdac_ct_shift[item].vals;
	return 0;
}

static int pwmdac_shift_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);
	int sel = ucontrol->value.enumerated.item[0];
	unsigned int items = ARRAY_SIZE(pwmdac_ct_shift);

	if (sel > items)
		return 0;

	dev->shift = pwmdac_ct_shift[sel].vals;
	return 0;
}

static int pwmdac_lr_change_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	unsigned int items = ARRAY_SIZE(pwmdac_ct_lr_change);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = items;
	if (uinfo->value.enumerated.item >= items)
		uinfo->value.enumerated.item = items - 1;
	strcpy(uinfo->value.enumerated.name,
		pwmdac_ct_lr_change[uinfo->value.enumerated.item].name);

	return 0;
}

static int pwmdac_lr_change_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = dev->lr_change;
	return 0;
}

static int pwmdac_lr_change_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sf_pwmdac_dev *dev = snd_soc_component_get_drvdata(component);
	int sel = ucontrol->value.enumerated.item[0];
	unsigned int items = ARRAY_SIZE(pwmdac_ct_lr_change);

	if (sel > items)
		return 0;

	dev->lr_change = pwmdac_ct_lr_change[sel].vals;
	return 0;
}

static inline void pwmdc_write_reg(void __iomem *io_base, int reg, u32 val)
{
	writel(val, io_base + reg);
}

static inline u32 pwmdc_read_reg(void __iomem *io_base, int reg)
{
	return readl(io_base + reg);
}

/*
 * 32bit-4byte
*/
static void pwmdac_set_ctrl_enable(struct sf_pwmdac_dev *dev)
{
	u32 date;
	date = pwmdc_read_reg(dev->pwmdac_base, PWMDAC_CTRL);
	pwmdc_write_reg(dev->pwmdac_base, PWMDAC_CTRL, date | BIT(0) );
}

/*
 * 32bit-4byte
*/
static void pwmdac_set_ctrl_disable(struct sf_pwmdac_dev *dev)
{
	u32 date;
	date = pwmdc_read_reg(dev->pwmdac_base, PWMDAC_CTRL);
	pwmdc_write_reg(dev->pwmdac_base, PWMDAC_CTRL, date & ~ BIT(0));
}

/*
 * 8:8-bit
 * 10:10-bit
*/
static void pwmdac_set_ctrl_shift(struct sf_pwmdac_dev *dev, u8 data)
{
	u32 value = 0;

	if (data == 8) {
		value = (~((~value) | 0x02));
		pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
	}
	else if(data == 10){
		value |= 0x02;
		pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
	}
}

/*
 * 00:left
 * 01:right
 * 10:center
*/
static void pwmdac_set_ctrl_dutyCycle(struct sf_pwmdac_dev *dev, u8 data)
{
	u32 value = 0;

	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_CTRL);
	if (data == 0) { //left
		value = (~((~value) | (0x03<<2)));
		pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
	}
	else if (data == 1) { //right
		value = (~((~value) | (0x01<<3))) | (0x01<<2);
		pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
	}
	else if (data == 2) { //center
		value = (~((~value) | (0x01<<2))) | (0x01<<3);
		pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
	}
}


static void pwmdac_set_ctrl_N(struct sf_pwmdac_dev *dev, u16 data)
{
	u32 value = 0;

	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_CTRL);
	pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, (value & 0xF) | ((data - 1) << 4));
}


static void pwmdac_LR_data_change(struct sf_pwmdac_dev *dev, u8 data)
{
	u32 value = 0;

	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_CTRL);
	switch (data) {
	case NO_CHANGE:
		value &= (~SFC_PWMDAC_LEFT_RIGHT_DATA_CHANGE);
		break;
	case CHANGE:
		value |= SFC_PWMDAC_LEFT_RIGHT_DATA_CHANGE;
		break;
	}
	pwmdc_write_reg(dev->pwmdac_base, PWMDAC_CTRL, value);
}

static void pwmdac_data_mode(struct sf_pwmdac_dev *dev, u8 data)
{
	u32 value = 0;

	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_CTRL);
	if (data == UNSINGED_DATA) {
		value &= (~SFC_PWMDAC_DATA_MODE);
	}
	else if (data == INVERTER_DATA_MSB) {
		value |= SFC_PWMDAC_DATA_MODE;
	}
	pwmdc_write_reg(dev->pwmdac_base,PWMDAC_CTRL, value);
}

static int pwmdac_data_shift(struct sf_pwmdac_dev *dev,u8 data)
{
	u32 value = 0;

	if ((data < PWMDAC_DATA_LEFT_SHIFT_BIT_0) || (data > PWMDAC_DATA_LEFT_SHIFT_BIT_7)) {
		return -1;
	}

	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_CTRL);
	value &= ( ~ ( PWMDAC_DATA_LEFT_SHIFT_BIT_ALL << 15 ) );
	value |= (data<<15);
	pwmdc_write_reg(dev->pwmdac_base , PWMDAC_CTRL, value);
	return 0;
}

static int get_pwmdac_fifo_state(struct sf_pwmdac_dev *dev)
{
	u32 value;

	value = pwmdc_read_reg(dev->pwmdac_base , PWMDAC_SATAE);
	if ((value & 0x02) == 0)
		return FIFO_UN_FULL;

	return FIFO_FULL;
}


static void pwmdac_set(struct sf_pwmdac_dev *dev)
{
	///8-bit + left + N=16
	pwmdac_set_ctrl_shift(dev, dev->shift_bit);
	pwmdac_set_ctrl_dutyCycle(dev, dev->duty_cycle);
	pwmdac_set_ctrl_N(dev, dev->datan);
	pwmdac_set_ctrl_enable(dev);

	pwmdac_LR_data_change(dev, dev->lr_change);
	pwmdac_data_mode(dev, dev->data_mode);
	if (dev->shift) {
		pwmdac_data_shift(dev, dev->shift);
	}
}

static void pwmdac_stop(struct sf_pwmdac_dev *dev)
{
	pwmdac_set_ctrl_disable(dev);
}

static int pwmdac_config(struct sf_pwmdac_dev *dev)
{
	switch (dev->mode) {
	case shift_8Bit_unsigned:
	case shift_8Bit_unsigned_dataShift:
		/* 8 bit, unsigned */
		dev->shift_bit	= PWMDAC_SHIFT_8;
		dev->duty_cycle	= PWMDAC_CYCLE_CENTER;
		dev->datan		= PWMDAC_SAMPLE_CNT_8;
		dev->data_mode	= UNSINGED_DATA;
		break;

	case shift_8Bit_inverter:
	case shift_8Bit_inverter_dataShift:
		/* 8 bit, invert */
		dev->shift_bit	= PWMDAC_SHIFT_8;
		dev->duty_cycle	= PWMDAC_CYCLE_CENTER;
		dev->datan		= PWMDAC_SAMPLE_CNT_8;
		dev->data_mode	= INVERTER_DATA_MSB;
		break;

	case shift_10Bit_unsigned:
	case shift_10Bit_unsigned_dataShift:
		/* 10 bit, unsigend */
		dev->shift_bit	= PWMDAC_SHIFT_10;
		dev->duty_cycle	= PWMDAC_CYCLE_CENTER;
		dev->datan		= PWMDAC_SAMPLE_CNT_8;
		dev->data_mode	= UNSINGED_DATA;
		break;

	case shift_10Bit_inverter:
	case shift_10Bit_inverter_dataShift:
		/* 10 bit, invert */
		dev->shift_bit	= PWMDAC_SHIFT_10;
		dev->duty_cycle	= PWMDAC_CYCLE_CENTER;
		dev->datan		= PWMDAC_SAMPLE_CNT_8;
		dev->data_mode	= INVERTER_DATA_MSB;
		break;

	default:
		return -1;
	}

	if ((dev->mode == shift_8Bit_unsigned_dataShift) || (dev->mode == shift_8Bit_inverter_dataShift)
		|| (dev->mode == shift_10Bit_unsigned_dataShift) || (dev->mode == shift_10Bit_inverter_dataShift)) {
		dev->shift = 4; /*0~7*/
	} else {
		dev->shift = 0;
	}
	dev->lr_change = NO_CHANGE;
	return 0;
}

static int sf_pwmdac_prepare(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	//struct sf_pwmdac_dev *dev = snd_soc_dai_get_drvdata(dai);
	//pwmdac_set(dev);
	return 0;
}

static int pwmdac_tx_thread(void *dev)
{
	struct sf_pwmdac_dev *pwmdac_dev = (struct sf_pwmdac_dev *)dev;

	set_current_state(TASK_INTERRUPTIBLE);
	while (!schedule_timeout(usecs_to_jiffies(50))) {
		if (pwmdac_dev->tx_thread_exit)
			break;
		if (get_pwmdac_fifo_state(pwmdac_dev)==0) {
			sf_pwmdac_pcm_push_tx(pwmdac_dev);
		}

		set_current_state(TASK_INTERRUPTIBLE);
	}

	pwmdac_dev->tx_thread = NULL;
	return 0;
}

static int sf_pwmdac_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	struct sf_pwmdac_dev *dev = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dev->active++;
		pwmdac_set(dev);
		if (dev->use_pio) {
			dev->tx_thread = kthread_create(pwmdac_tx_thread, (void *)dev, "pwmdac");
			if (IS_ERR(dev->tx_thread)) {
				return PTR_ERR(dev->tx_thread);
			}
			wake_up_process(dev->tx_thread);
			dev->tx_thread_exit = 0;
		}
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		dev->active--;
		pwmdac_stop(dev);
		if (dev->use_pio) {
			if(dev->tx_thread) {
				dev->tx_thread_exit = 1;
			}
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;

	return 0;
}

static int sf_pwmdac_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct sf_pwmdac_dev *dev = dev_get_drvdata(dai->dev);

	dev->play_dma_data.addr = dev->mapbase + PWMDAC_WDATA;

	switch (params_channels(params)) {
	case 2:
		dev->play_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		break;
	case 1:
		dev->play_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		break;
	default:
		dev_err(dai->dev, "%d channels not supported\n",
				params_channels(params));
		return -EINVAL;
	}

	dev->play_dma_data.fifo_size = 1;
	dev->play_dma_data.maxburst = 16;

	snd_soc_dai_init_dma_data(dai, &dev->play_dma_data, NULL);
	snd_soc_dai_set_drvdata(dai, dev);

	return 0;
}

static int sf_pwmdac_clks_get(struct platform_device *pdev,
			      struct sf_pwmdac_dev *dev)
{
	static const char *const clock_names[PWMDAC_CLK_NUM] = {
		[PWMDAC_CLK_AUDIO_ROOT]  = "audio_root",
		[PWMDAC_CLK_AUDIO_SRC]   = "audio_src",
		[PWMDAC_CLK_AUDIO_12288] = "audio_12288",
		[PWMDAC_CLK_DMA1P_AHB]   = "dma1p_ahb",
		[PWMDAC_CLK_PWMDAC_APB]  = "pwmdac_apb",
		[PWMDAC_CLK_DAC_MCLK]    = "dac_mclk",
	};
	int i;

	for (i = 0; i < PWMDAC_CLK_NUM; i++)
		dev->clk[i].id = clock_names[i];

	return devm_clk_bulk_get(&pdev->dev, ARRAY_SIZE(dev->clk), dev->clk);
}

static int sf_pwmdac_resets_get(struct platform_device *pdev,
				struct sf_pwmdac_dev *dev)
{
	static const char *const reset_names[PWMDAC_RST_NUM] = {
		[PWMDAC_RST_APB_BUS]    = "apb_bus",
		[PWMDAC_RST_DMA1P_AHB]  = "dma1p_ahb",
		[PWMDAC_RST_APB_PWMDAC] = "apb_pwmdac",
	};
	int i;

	for (i = 0; i < PWMDAC_RST_NUM; i++)
		dev->rst[i].id = reset_names[i];

	return devm_reset_control_bulk_get_exclusive(&pdev->dev, ARRAY_SIZE(dev->rst), dev->rst);
}

static int sf_pwmdac_clk_init(struct platform_device *pdev,
				struct sf_pwmdac_dev *dev)
{
	int ret;
	int i;

	for (i = 0; i <= PWMDAC_CLK_DMA1P_AHB; i++) {
		ret = clk_prepare_enable(dev->clk[i].clk);
		if (ret)
			return dev_err_probe(&pdev->dev, ret,
					     "failed to enable %s\n", dev->clk[i].id);
	}

	for (i = 0; i <= PWMDAC_RST_DMA1P_AHB; i++) {
		ret = reset_control_deassert(dev->rst[i].rstc);
		if (ret)
			return dev_err_probe(&pdev->dev, ret,
					     "failed to deassert %s\n", dev->rst[i].id);
	}

	ret = clk_set_rate(dev->clk[PWMDAC_CLK_AUDIO_SRC].clk, 12288000);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "failed to set 12.288 MHz rate for clk_audio_src\n");

	ret = reset_control_assert(dev->rst[PWMDAC_RST_APB_PWMDAC].rstc);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to assert apb_pwmdac\n");

	ret = clk_prepare_enable(dev->clk[PWMDAC_CLK_DAC_MCLK].clk);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to prepare enable clk_dac_mclk\n");

	/* we want 4096kHz but the clock driver always rounds down so add a little slack */
	ret = clk_set_rate(dev->clk[PWMDAC_CLK_DAC_MCLK].clk, 4096000 + 64);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to set 4096kHz rate for clk_dac_mclk\n");

	ret = clk_prepare_enable(dev->clk[PWMDAC_CLK_PWMDAC_APB].clk);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to prepare enable clk_pwmdac_apb\n");

	ret = reset_control_deassert(dev->rst[PWMDAC_RST_APB_PWMDAC].rstc);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to deassert apb_pwmdac\n");

	return 0;
}

#define SOC_PWMDAC_ENUM_DECL(xname, xinfo, xget, xput) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = xinfo, .get = xget, \
	.put = xput,}
static const struct snd_kcontrol_new pwmdac_snd_controls[] = {
	SOC_PWMDAC_ENUM_DECL("shift_bit", pwmdac_shift_bit_info,
		pwmdac_shift_bit_get, pwmdac_shift_bit_put),
	SOC_PWMDAC_ENUM_DECL("duty_cycle", pwmdac_duty_cycle_info,
		pwmdac_duty_cycle_get, pwmdac_duty_cycle_put),
	SOC_PWMDAC_ENUM_DECL("data_mode", pwmdac_data_mode_info,
		pwmdac_data_mode_get, pwmdac_data_mode_put),
	SOC_PWMDAC_ENUM_DECL("shift", pwmdac_shift_info,
		pwmdac_shift_get, pwmdac_shift_put),
	SOC_PWMDAC_ENUM_DECL("lr_change", pwmdac_lr_change_info,
		pwmdac_lr_change_get, pwmdac_lr_change_put),
};

static int pwmdac_probe(struct snd_soc_component *component)
{
//	struct sf_pwmdac_dev *priv = snd_soc_component_get_drvdata(component);
	snd_soc_add_component_controls(component, pwmdac_snd_controls,
				ARRAY_SIZE(pwmdac_snd_controls));
	return 0;
}

static const struct snd_soc_component_driver sf_pwmdac_component = {
	.name		= "sf-pwmdac",
	.probe		= pwmdac_probe,
};

static int sf_pwmdac_dai_probe(struct snd_soc_dai *dai)
{
	struct sf_pwmdac_dev *dev = dev_get_drvdata(dai->dev);

	dev->play_dma_data.addr = dev->mapbase + PWMDAC_WDATA;
	dev->play_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dev->play_dma_data.fifo_size = 1;
	dev->play_dma_data.maxburst = 16;

	snd_soc_dai_init_dma_data(dai, &dev->play_dma_data, NULL);
	snd_soc_dai_set_drvdata(dai, dev);

	return 0;
}

static const struct snd_soc_dai_ops sf_pwmdac_dai_ops = {
	.probe		= sf_pwmdac_dai_probe,
	.hw_params	= sf_pwmdac_hw_params,
	.prepare	= sf_pwmdac_prepare,
	.trigger	= sf_pwmdac_trigger,
};

static struct snd_soc_dai_driver pwmdac_dai = {
	.name = "pwmdac",
	.id = 0,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &sf_pwmdac_dai_ops,
};

static int sf_pwmdac_probe(struct platform_device *pdev)
{
	struct sf_pwmdac_dev *dev;
	struct resource *res;
	int ret;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dev->mapbase = res->start;
	dev->pwmdac_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dev->pwmdac_base))
		return PTR_ERR(dev->pwmdac_base);

	ret = sf_pwmdac_clks_get(pdev, dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to get audio clock\n");
		return ret;
	}

	ret = sf_pwmdac_resets_get(pdev, dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to get audio reset controls\n");
		return ret;
        }

	ret = sf_pwmdac_clk_init(pdev, dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable audio clock\n");
		return ret;
	}

	dev->dev = &pdev->dev;
	dev->mode = shift_8Bit_inverter;
	dev->fifo_th = 1;//8byte
	pwmdac_config(dev);

	dev->use_pio = false;
	dev_set_drvdata(&pdev->dev, dev);
	ret = devm_snd_soc_register_component(&pdev->dev, &sf_pwmdac_component,
					 &pwmdac_dai, 1);
	if (ret != 0) {
		dev_err(&pdev->dev, "not able to register dai\n");
		return ret;
	}

	if (dev->use_pio) {
		ret = sf_pwmdac_pcm_register(pdev);
	} else {
		ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL,
				0);
	}
	return 0;
}


static void sf_pwmdac_remove(struct platform_device *pdev)
{
}

#ifdef CONFIG_OF
static const struct of_device_id sf_pwmdac_of_match[] = {
	{ .compatible = "starfive,pwmdac",	 },
	{},
};

MODULE_DEVICE_TABLE(of, sf_pwmdac_of_match);
#endif


static struct platform_driver sf_pwmdac_driver = {
	.probe		= sf_pwmdac_probe,
	.remove		= sf_pwmdac_remove,
	.driver		= {
		.name	= "sf-pwmdac",
		.of_match_table = of_match_ptr(sf_pwmdac_of_match),
	},
};

module_platform_driver(sf_pwmdac_driver);

MODULE_AUTHOR("jenny.zhang <jenny.zhang@starfivetech.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("starfive pwmdac SoC Interface");
MODULE_ALIAS("platform:starfive-pwmdac");
