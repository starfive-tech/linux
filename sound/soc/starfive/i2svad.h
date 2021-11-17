// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#ifndef __SND_SOC_STARFIVE_I2SVAD_H
#define __SND_SOC_STARFIVE_I2SVAD_H

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/reset.h>
#include <linux/types.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm.h>
#include <sound/designware_i2s.h>

/* common register for all channel */
#define IER		0x000
#define IRER		0x004
#define ITER		0x008
#define CER		0x00C
#define CCR		0x010
#define RXFFR		0x014
#define TXFFR		0x018

/* Interrupt status register fields */
#define ISR_TXFO	BIT(5)
#define ISR_TXFE	BIT(4)
#define ISR_RXFO	BIT(1)
#define ISR_RXDA	BIT(0)

/* I2STxRxRegisters for all channels */
#define LRBR_LTHR(x)	(0x40 * x + 0x020)
#define RRBR_RTHR(x)	(0x40 * x + 0x024)
#define RER(x)		(0x40 * x + 0x028)
#define TER(x)		(0x40 * x + 0x02C)
#define RCR(x)		(0x40 * x + 0x030)
#define TCR(x)		(0x40 * x + 0x034)
#define ISR(x)		(0x40 * x + 0x038)
#define IMR(x)		(0x40 * x + 0x03C)
#define ROR(x)		(0x40 * x + 0x040)
#define TOR(x)		(0x40 * x + 0x044)
#define RFCR(x)		(0x40 * x + 0x048)
#define TFCR(x)		(0x40 * x + 0x04C)
#define RFF(x)		(0x40 * x + 0x050)
#define TFF(x)		(0x40 * x + 0x054)

/* I2SCOMPRegisters */
#define I2S_COMP_PARAM_2	0x01F0
#define I2S_COMP_PARAM_1	0x01F4
#define I2S_COMP_VERSION	0x01F8
#define I2S_COMP_TYPE		0x01FC

/* VAD Registers */
#define VAD_LEFT_MARGIN				0x800  /* left_margin */
#define VAD_RIGHT_MARGIN			0x804  /* right_margin */
#define VAD_N_LOW_CONT_FRAMES			0x808  /* low-energy transition range threshold ——NL*/
#define VAD_N_LOW_SEEK_FRAMES			0x80C  /* low-energy transition range */
#define VAD_N_HIGH_CONT_FRAMES			0x810  /* high-energy transition range threshold——NH */
#define VAD_N_HIGH_SEEK_FRAMES			0x814  /* high-energy transition range */
#define VAD_N_SPEECH_LOW_HIGH_FRAMES		0x818  /* low-energy voice range threshold——NVL*/
#define VAD_N_SPEECH_LOW_SEEK_FRAMES		0x81C  /* low-energy voice range*/
#define VAD_MEAN_SIL_FRAMES			0x820  /* mean silence frame range*/
#define VAD_N_ALPHA				0x824  /* low-energy threshold scaling factor,12bit(0~0xFFF)*/
#define VAD_N_BETA				0x828  /* high-energy threshold scaling factor,12bit(0~0xFFF)*/
#define VAD_FIFO_DEPTH				0x82C  /* status register for VAD */
#define VAD_LR_SEL				0x840  /* L/R channel data selection for processing */
#define VAD_SW					0x844  /* push enable signal*/
#define VAD_LEFT_WD				0x848  /* select left channel*/
#define VAD_RIGHT_WD				0x84C  /* select right channel*/
#define VAD_STOP_DELAY				0x850  /* delay stop for 0-3 samples*/
#define VAD_ADDR_START				0x854  /* vad memory start address, align with 64bit*/
#define VAD_ADDR_WRAP				0x858  /* vad memory highest address for Push, align with 64bit,(addr_wrap-1) is the max physical address*/
#define VAD_MEM_SW				0x85C  /* xmem switch */
#define VAD_SPINT_CLR				0x860  /* clear vad_spint interrup status*/
#define VAD_SPINT_EN				0x864  /* disable/enable vad_spint from vad_flag rising edge*/
#define VAD_SLINT_CLR				0x868  /* clear vad_slint interrup status*/
#define VAD_SLINT_EN				0x86C  /* disable/enable  vad_slint from vad_flag falling edge*/
#define VAD_RAW_SPINT				0x870  /* status of spint before vad_spint_en*/
#define VAD_RAW_SLINT				0x874  /* status of slint before vad_slint_en*/
#define VAD_SPINT				0x878  /* status of spint after vad_spint_en*/
#define VAD_SLINT				0x87C  /* status of slint before vad_slint_en*/
#define VAD_XMEM_ADDR				0x880  /* next xmem address ,align to 16bi*/
#define VAD_I2S_CTRL_REG_ADDR			0x884

/*
 * vad parameter register fields
 */
#define VAD_LEFT_MARGIN_MASK			GENMASK(4, 0)
#define VAD_RIGHT_MARGIN_MASK			GENMASK(4, 0)
#define VAD_N_LOW_CONT_FRAMES_MASK		GENMASK(4, 0)
#define VAD_N_LOW_SEEK_FRAMES_MASK		GENMASK(4, 0)
#define VAD_N_HIGH_CONT_FRAMES_MASK		GENMASK(4, 0)
#define VAD_N_HIGH_SEEK_FRAMES_MASK		GENMASK(4, 0)
#define VAD_N_SPEECH_LOW_HIGH_FRAMES_MASK	GENMASK(4, 0)
#define VAD_N_SPEECH_LOW_SEEK_FRAMES_MASK	GENMASK(4, 0)
#define VAD_MEAN_SIL_FRAMES_MASK		GENMASK(4, 0)
#define VAD_N_ALPHA_MASK			GENMASK(11, 0)
#define VAD_N_BETA_MASK				GENMASK(11, 0)
#define VAD_LR_SEL_MASK				GENMASK(0, 0)
#define VAD_LR_SEL_L				(0 << 0)
#define VAD_LR_SEL_R				(1 << 0)

#define VAD_SW_MASK				GENMASK(1, 0)
#define VAD_SW_VAD_XMEM_ENABLE			(1 << 0)
#define VAD_SW_VAD_XMEM_DISABLE			(0 << 0)
#define VAD_SW_ADC_ENABLE			(1 << 1)
#define VAD_SW_ADC_DISABLE			(0 << 1)


#define VAD_LEFT_WD_MASK			GENMASK(0, 0)
#define VAD_LEFT_WD_BIT_31_16			(1 << 1)
#define VAD_LEFT_WD_BIT_15_0			(0 << 1)


#define VAD_RIGHT_WD_MASK			GENMASK(0, 0)
#define VAD_RIGHT_WD_BIT_31_16			(1 << 1)
#define VAD_RIGHT_WD_BIT_15_0			(0 << 1)


#define VAD_STOP_DELAY_MASK			GENMASK(1, 0)
#define VAD_STOP_DELAY_0_SAMPLE			0
#define VAD_STOP_DELAY_1_SAMPLE			1
#define VAD_STOP_DELAY_2_SAMPLE			2
#define VAD_STOP_DELAY_3_SAMPLE			3

#define VAD_ADDR_START_MASK			GENMASK(12, 0)
#define VAD_ADDR_WRAP_MASK			GENMASK(13, 0)
#define VAD_MEM_SW_MASK				GENMASK(0, 0)
#define VAD_SPINT_CLR_MASK			GENMASK(0, 0)
#define VAD_SPINT_EN_MASK			GENMASK(0, 0)
#define VAD_SLINT_CLR_MASK			GENMASK(0, 0)
#define VAD_SLINT_EN_MASK			GENMASK(0, 0)
#define VAD_I2S_CTRL_REG_ADDR_MASK		GENMASK(0, 0)

#define VAD_MEM_SW_TO_VAD			(1 << 0)
#define VAD_MEM_SW_TO_AXI			(0 << 0)

#define VAD_SPINT_CLR_VAD_SPINT			(1 << 0)

#define VAD_SPINT_EN_ENABLE			(1 << 0)
#define VAD_SPINT_EN_DISABLE			(0 << 0)

#define VAD_SLINT_CLR_VAD_SLINT			(1 << 0)

#define VAD_SLINT_EN_ENABLE			(1 << 0)
#define VAD_SLINT_EN_DISABLE			(0 << 0)

#define VAD_STATUS_NORMAL			0
#define VAD_STATUS_SPINT			1
#define VAD_STATUS_SLINT			2

/*
 * Component parameter register fields - define the I2S block's
 * configuration.
 */
#define COMP1_TX_WORDSIZE_3(r)		(((r) & GENMASK(27, 25)) >> 25)
#define COMP1_TX_WORDSIZE_2(r)		(((r) & GENMASK(24, 22)) >> 22)
#define COMP1_TX_WORDSIZE_1(r)		(((r) & GENMASK(21, 19)) >> 19)
#define COMP1_TX_WORDSIZE_0(r)		(((r) & GENMASK(18, 16)) >> 16)
#define COMP1_TX_CHANNELS(r)		(((r) & GENMASK(10, 9)) >> 9)
#define COMP1_RX_CHANNELS(r)		(((r) & GENMASK(8, 7)) >> 7)
#define COMP1_RX_ENABLED(r)		(((r) & BIT(6)) >> 6)
#define COMP1_TX_ENABLED(r)		(((r) & BIT(5)) >> 5)
#define COMP1_MODE_EN(r)		(((r) & BIT(4)) >> 4)
#define COMP1_FIFO_DEPTH_GLOBAL(r)	(((r) & GENMASK(3, 2)) >> 2)
#define COMP1_APB_DATA_WIDTH(r)		(((r) & GENMASK(1, 0)) >> 0)

#define COMP2_RX_WORDSIZE_3(r)		(((r) & GENMASK(12, 10)) >> 10)
#define COMP2_RX_WORDSIZE_2(r)		(((r) & GENMASK(9, 7)) >> 7)
#define COMP2_RX_WORDSIZE_1(r)		(((r) & GENMASK(5, 3)) >> 3)
#define COMP2_RX_WORDSIZE_0(r)		(((r) & GENMASK(2, 0)) >> 0)

/* Number of entries in WORDSIZE and DATA_WIDTH parameter registers */
#define COMP_MAX_WORDSIZE		(1 << 3)
#define COMP_MAX_DATA_WIDTH		(1 << 2)

#define MAX_CHANNEL_NUM		8
#define MIN_CHANNEL_NUM		2
#define ALL_CHANNEL_NUM		4


union dw_i2s_snd_dma_data {
	struct i2s_dma_data pd;
	struct snd_dmaengine_dai_dma_data dt;
};

struct vad_params {
	void __iomem *vad_base;
	struct regmap *vad_map;
	unsigned int vswitch;
	unsigned int vstatus; /*vad detect status: 1:SPINT 2:SLINT 0:normal*/
};

struct i2svad_dev {
	void __iomem *i2s_base;
	struct clk *clk;
	int active;
	unsigned int capability;
	unsigned int quirks;
	unsigned int i2s_reg_comp1;
	unsigned int i2s_reg_comp2;
	struct device *dev;
	u32 ccr;
	u32 xfer_resolution;
	u32 fifo_th;

	struct clk *clk_apb_i2svad;
	struct reset_control *rst_apb_i2svad;
	struct reset_control *rst_i2svad_srst;

	/* data related to DMA transfers b/w i2s and DMAC */
	union dw_i2s_snd_dma_data play_dma_data;
	union dw_i2s_snd_dma_data capture_dma_data;
	struct i2s_clk_config_data config;
	int (*i2s_clk_cfg)(struct i2s_clk_config_data *config);

	/* data related to PIO transfers */
	bool use_pio;
	struct snd_pcm_substream __rcu *tx_substream;
	struct snd_pcm_substream __rcu *rx_substream;
	unsigned int (*tx_fn)(struct i2svad_dev *dev,
			struct snd_pcm_runtime *runtime, unsigned int tx_ptr,
			bool *period_elapsed);
	unsigned int (*rx_fn)(struct i2svad_dev *dev,
			struct snd_pcm_runtime *runtime, unsigned int rx_ptr,
			bool *period_elapsed);
	unsigned int tx_ptr;
	unsigned int rx_ptr;

	struct vad_params vad;
};

#if IS_ENABLED(CONFIG_SND_STARFIVE_I2SVAD_PCM)
void i2svad_pcm_push_tx(struct i2svad_dev *dev);
void i2svad_pcm_pop_rx(struct i2svad_dev *dev);
int i2svad_pcm_register(struct platform_device *pdev);
#else
static inline void i2svad_pcm_push_tx(struct i2svad_dev *dev) { }
static inline void i2svad_pcm_pop_rx(struct i2svad_dev *dev) { }
static inline int i2svad_pcm_register(struct platform_device *pdev)
{
	return -EINVAL;
}
#endif

#endif
