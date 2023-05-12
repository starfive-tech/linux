/* SPDX-License-Identifier: GPL-2.0 */
/*
 * stf_vin.h
 *
 * StarFive Camera Subsystem - VIN Module
 *
 * Copyright (C) 2021-2023 StarFive Technology Co., Ltd.
 */

#ifndef STF_VIN_H
#define STF_VIN_H

#include <linux/interrupt.h>
#include <linux/spinlock_types.h>
#include <media/v4l2-subdev.h>

#include "stf_video.h"

#define SYSCONSAIF_SYSCFG(x)	(x)

/* syscon offset 0 */
#define U0_VIN_CNFG_AXI_DVP_EN	BIT(2)

/* syscon offset 20 */
#define U0_VIN_CHANNEL_SEL_MASK	GENMASK(3, 0)
#define U0_VIN_AXIWR0_EN	BIT(4)
#define CHANNEL(x)	((x) << 0)

/* syscon offset 32 */
#define U0_VIN_INTR_CLEAN	BIT(0)
#define U0_VIN_INTR_M	BIT(1)
#define U0_VIN_PIX_CNT_END_MASK	GENMASK(12, 2)
#define U0_VIN_PIX_CT_MASK	GENMASK(14, 13)
#define U0_VIN_PIXEL_HEIGH_BIT_SEL_MAKS	GENMASK(16, 15)

#define PIX_CNT_END(x)	((x) << 2)
#define PIX_CT(x)	((x) << 13)
#define PIXEL_HEIGH_BIT_SEL(x)	((x) << 15)

/* syscon offset 36 */
#define U0_VIN_CNFG_DVP_HS_POS	BIT(1)
#define U0_VIN_CNFG_DVP_SWAP_EN	BIT(2)
#define U0_VIN_CNFG_DVP_VS_POS	BIT(3)
#define U0_VIN_CNFG_GEN_EN_AXIRD	BIT(4)
#define U0_VIN_CNFG_ISP_DVP_EN0		BIT(5)
#define U0_VIN_MIPI_BYTE_EN_ISP0(n)	((n) << 6)
#define U0_VIN_MIPI_CHANNEL_SEL0(n)	((n) << 8)
#define U0_VIN_P_I_MIPI_HAEDER_EN0(n)	((n) << 12)
#define U0_VIN_PIX_NUM(n)	((n) << 13)
#define U0_VIN_MIPI_BYTE_EN_ISP0_MASK	GENMASK(7, 6)
#define U0_VIN_MIPI_CHANNEL_SEL0_MASK	GENMASK(11, 8)
#define U0_VIN_P_I_MIPI_HAEDER_EN0_MASK	BIT(12)
#define U0_VIN_PIX_NUM_MASK	GENMASK(16, 13)

#define STF_VIN_PAD_SINK   0
#define STF_VIN_PAD_SRC    1
#define STF_VIN_PADS_NUM   2

#define ISP_DUMMY_BUFFER_NUMS  STF_ISP_PAD_MAX
#define VIN_DUMMY_BUFFER_NUMS  1

enum {
	STF_DUMMY_VIN,
	STF_DUMMY_ISP,
	STF_DUMMY_MODULE_NUMS,
};

enum link {
	LINK_ERROR = -1,
	LINK_DVP_TO_WR,
	LINK_DVP_TO_ISP,
	LINK_CSI_TO_WR,
	LINK_CSI_TO_ISP,
};

struct vin_format {
	u32 code;
	u8 bpp;
};

struct vin_format_table {
	const struct vin_format *fmts;
	int nfmts;
};

enum vin_output_state {
	VIN_OUTPUT_OFF,
	VIN_OUTPUT_RESERVED,
	VIN_OUTPUT_SINGLE,
	VIN_OUTPUT_CONTINUOUS,
	VIN_OUTPUT_IDLE,
	VIN_OUTPUT_STOPPING
};

struct vin_output {
	int active_buf;
	struct stfcamss_buffer *buf[2];
	struct stfcamss_buffer *last_buffer;
	struct list_head pending_bufs;
	struct list_head ready_bufs;
	enum vin_output_state state;
	unsigned int sequence;
	unsigned int frame_skip;
};

/* The vin output lines */
enum vin_line_id {
	VIN_LINE_NONE = -1,
	VIN_LINE_WR = 0,
	VIN_LINE_ISP,
	VIN_LINE_MAX,
};

struct vin_line {
	enum stf_subdev_type sdev_type;  /* must be frist */
	enum vin_line_id id;
	struct v4l2_subdev subdev;
	struct media_pad pads[STF_VIN_PADS_NUM];
	struct v4l2_mbus_framefmt fmt[STF_VIN_PADS_NUM];
	struct stfcamss_video video_out;
	struct mutex stream_lock;	/* serialize stream control */
	int stream_count;
	struct mutex power_lock; /* serialize pipeline control in power process*/
	int power_count;
	struct vin_output output;	/* pipeline and stream states */
	spinlock_t output_lock;
	const struct vin_format *formats;
	unsigned int nformats;
};

struct vin_dummy_buffer {
	dma_addr_t paddr[3];
	void *vaddr;
	u32 buffer_size;
	u32 width;
	u32 height;
	u32 mcode;
};

struct dummy_buffer {
	struct vin_dummy_buffer *buffer;
	u32 nums;
	struct mutex stream_lock;	/* protects buffer data */
	int stream_count;
	atomic_t frame_skip;
};

struct vin_isr_ops {
	void (*isr_buffer_done)(struct vin_line *line);
	void (*isr_change_buffer)(struct vin_line *line);
};

struct stf_vin_dev {
	struct stfcamss *stfcamss;
	struct vin_line line[VIN_LINE_MAX];
	struct dummy_buffer dummy_buffer[STF_DUMMY_MODULE_NUMS];
	struct vin_isr_ops *isr_ops;
	atomic_t ref_count;
	struct mutex power_lock;	/* serialize power control*/
	int power_count;
};

int stf_vin_clk_enable(struct stf_vin_dev *vin_dev, enum link link);
int stf_vin_clk_disable(struct stf_vin_dev *vin_dev, enum link link);
int stf_vin_wr_stream_set(struct stf_vin_dev *vin_dev);
int stf_vin_stream_set(struct stf_vin_dev *vin_dev, enum link link);
void stf_vin_wr_irq_enable(struct stf_vin_dev *vin_dev, int enable);
void stf_vin_wr_set_ping_addr(struct stf_vin_dev *vin_dev, dma_addr_t addr);
void stf_vin_wr_set_pong_addr(struct stf_vin_dev *vin_dev, dma_addr_t addr);
void stf_vin_isp_set_yuv_addr(struct stf_vin_dev *vin_dev,
			      dma_addr_t y_addr, dma_addr_t uv_addr);
irqreturn_t stf_vin_wr_irq_handler(int irq, void *priv);
irqreturn_t stf_vin_isp_irq_handler(int irq, void *priv);
irqreturn_t stf_vin_isp_irq_csiline_handler(int irq, void *priv);
int stf_vin_subdev_init(struct stfcamss *stfcamss);
int stf_vin_register(struct stf_vin_dev *vin_dev, struct v4l2_device *v4l2_dev);
int stf_vin_unregister(struct stf_vin_dev *vin_dev);
enum isp_pad_id stf_vin_map_isp_pad(enum vin_line_id line, enum isp_pad_id def);

#endif /* STF_VIN_H */
