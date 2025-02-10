// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#ifndef __SND_SOC_STARFIVE_PWMDAC_H
#define __SND_SOC_STARFIVE_PWMDAC_H

#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/device.h>
#include <linux/types.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm.h>

#define PWMDAC_WDATA	0	// PWMDAC_BASE_ADDR
#define PWMDAC_CTRL	0x04	// PWMDAC_BASE_ADDR + 0x04
#define PWMDAC_SATAE	0x08	// PWMDAC_BASE_ADDR + 0x08
#define PWMDAC_RESERVED	0x0C	// PWMDAC_BASE_ADDR + 0x0C

#define SFC_PWMDAC_SHIFT	BIT(1)
#define SFC_PWMDAC_DUTY_CYCLE	BIT(2)
#define SFC_PWMDAC_CNT_N	BIT(4)

#define SFC_PWMDAC_LEFT_RIGHT_DATA_CHANGE	BIT(13)
#define SFC_PWMDAC_DATA_MODE			BIT(14)

#define FIFO_UN_FULL	0
#define FIFO_FULL	1

enum pwmdac_lr_change{
	NO_CHANGE = 0,
	CHANGE,
};

enum pwmdac_d_mode{
	UNSINGED_DATA = 0,
	INVERTER_DATA_MSB,
};

enum pwmdac_shift_bit{
	PWMDAC_SHIFT_8 = 8,	/* pwmdac shift 8 bit */
	PWMDAC_SHIFT_10 = 10,	/* pwmdac shift 10 bit */
};

enum pwmdac_duty_cycle{
	PWMDAC_CYCLE_LEFT = 0,		/* pwmdac duty cycle left */
	PWMDAC_CYCLE_RIGHT = 1,		/* pwmdac duty cycle right */
	PWMDAC_CYCLE_CENTER = 2,	/* pwmdac duty cycle center */
};

/*sample count [12:4] <511*/
enum pwmdac_sample_count{
	PWMDAC_SAMPLE_CNT_1 = 1,
	PWMDAC_SAMPLE_CNT_2,
	PWMDAC_SAMPLE_CNT_3,
	PWMDAC_SAMPLE_CNT_4,
	PWMDAC_SAMPLE_CNT_5,
	PWMDAC_SAMPLE_CNT_6,
	PWMDAC_SAMPLE_CNT_7,
	PWMDAC_SAMPLE_CNT_8 = 1,	//(32.468/8) == (12.288/3) == 4.096
	PWMDAC_SAMPLE_CNT_9,
	PWMDAC_SAMPLE_CNT_10,
	PWMDAC_SAMPLE_CNT_11,
	PWMDAC_SAMPLE_CNT_12,
	PWMDAC_SAMPLE_CNT_13,
	PWMDAC_SAMPLE_CNT_14,
	PWMDAC_SAMPLE_CNT_15,
	PWMDAC_SAMPLE_CNT_16,
	PWMDAC_SAMPLE_CNT_17,
	PWMDAC_SAMPLE_CNT_18,
	PWMDAC_SAMPLE_CNT_19,
	PWMDAC_SAMPLE_CNT_20 = 20,
	PWMDAC_SAMPLE_CNT_30 = 30,
	PWMDAC_SAMPLE_CNT_511 = 511,
};


enum data_shift{
	PWMDAC_DATA_LEFT_SHIFT_BIT_0 = 0,
	PWMDAC_DATA_LEFT_SHIFT_BIT_1,
	PWMDAC_DATA_LEFT_SHIFT_BIT_2,
	PWMDAC_DATA_LEFT_SHIFT_BIT_3,
	PWMDAC_DATA_LEFT_SHIFT_BIT_4,
	PWMDAC_DATA_LEFT_SHIFT_BIT_5,
	PWMDAC_DATA_LEFT_SHIFT_BIT_6,
	PWMDAC_DATA_LEFT_SHIFT_BIT_7,
	PWMDAC_DATA_LEFT_SHIFT_BIT_ALL,
};

enum pwmdac_config_list{
	shift_8Bit_unsigned = 0,
	shift_8Bit_unsigned_dataShift,
	shift_10Bit_unsigned,
	shift_10Bit_unsigned_dataShift,

	shift_8Bit_inverter,
	shift_8Bit_inverter_dataShift,
	shift_10Bit_inverter,
	shift_10Bit_inverter_dataShift,
};

enum pwmdac_clocks {
	PWMDAC_CLK_AUDIO_ROOT,
	PWMDAC_CLK_AUDIO_SRC,
	PWMDAC_CLK_AUDIO_12288,
	PWMDAC_CLK_DMA1P_AHB,
	PWMDAC_CLK_PWMDAC_APB,
	PWMDAC_CLK_DAC_MCLK,
	PWMDAC_CLK_NUM,
};

enum pwmdac_resets {
	PWMDAC_RST_APB_BUS,
	PWMDAC_RST_DMA1P_AHB,
	PWMDAC_RST_APB_PWMDAC,
	PWMDAC_RST_NUM,
};

struct sf_pwmdac_dev {
	void __iomem *pwmdac_base;
	resource_size_t	mapbase;
	u8  mode;
	u8 shift_bit;
	u8 duty_cycle;
	u8 datan;
	u8 data_mode;
	u8 lr_change;
	u8 shift;
	u8 fifo_th;
	bool use_pio;
	spinlock_t lock;
	int active;

	struct clk_bulk_data clk[PWMDAC_CLK_NUM];
	struct reset_control_bulk_data rst[PWMDAC_RST_NUM];

	struct device *dev;
	struct snd_dmaengine_dai_dma_data play_dma_data;
	struct snd_pcm_substream __rcu *tx_substream;
	unsigned int (*tx_fn)(struct sf_pwmdac_dev *dev,
			struct snd_pcm_runtime *runtime, unsigned int tx_ptr,
			bool *period_elapsed);
	unsigned int tx_ptr;
	struct task_struct *tx_thread;
	bool tx_thread_exit;
};



#if IS_ENABLED(CONFIG_SND_STARFIVE_PWMDAC_PCM)
void sf_pwmdac_pcm_push_tx(struct sf_pwmdac_dev *dev);
int sf_pwmdac_pcm_register(struct platform_device *pdev);
#else
static void sf_pwmdac_pcm_push_tx(struct sf_pwmdac_dev *dev) { }
static int sf_pwmdac_pcm_register(struct platform_device *pdev)
{
	return -EINVAL;
}
#endif

#endif
