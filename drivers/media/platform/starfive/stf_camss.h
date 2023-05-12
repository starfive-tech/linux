/* SPDX-License-Identifier: GPL-2.0 */
/*
 * stf_camss.h
 *
 * Starfive Camera Subsystem driver
 *
 * Copyright (C) 2021-2023 StarFive Technology Co., Ltd.
 */

#ifndef STF_CAMSS_H
#define STF_CAMSS_H

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <media/v4l2-device.h>

#include "stf_common.h"
#include "stf_isp.h"

#define DRV_NAME     "starfive-camss"
#define STF_DVP_NAME "stf_dvp"
#define STF_CSI_NAME "cdns_csi2rx"
#define STF_ISP_NAME "stf_isp"
#define STF_VIN_NAME "stf_vin"

#define STF_PAD_SINK   0
#define STF_PAD_SRC    1
#define STF_PADS_NUM   2

enum port_num {
	PORT_NUMBER_DVP_SENSOR = 0,
	PORT_NUMBER_CSI2RX
};

enum stf_clk {
	STF_CLK_APB_FUNC = 0,
	STF_CLK_WRAPPER_CLK_C,
	STF_CLK_DVP_INV,
	STF_CLK_AXIWR,
	STF_CLK_MIPI_RX0_PXL,
	STF_CLK_ISPCORE_2X,
	STF_CLK_ISP_AXI,
	STF_CLK_NUM
};

enum stf_rst {
	STF_RST_WRAPPER_P = 0,
	STF_RST_WRAPPER_C,
	STF_RST_AXIRD,
	STF_RST_AXIWR,
	STF_RST_ISP_TOP_N,
	STF_RST_ISP_TOP_AXI,
	STF_RST_NUM
};

enum stf_irq {
	STF_IRQ_VINWR = 0,
	STF_IRQ_ISP,
	STF_IRQ_ISPCSIL,
	STF_IRQ_NUM
};

#define HOST_ENTITY_MAX		2

struct host_data {
	struct media_entity *host_entity[HOST_ENTITY_MAX];
};

struct stfcamss {
	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct media_pipeline pipe;
	struct device *dev;
	struct stf_isp_dev isp_dev;
	struct v4l2_async_notifier notifier;
	struct host_data host_data;
	void __iomem *syscon_base;
	void __iomem *isp_base;
	int irq[STF_IRQ_NUM];
	struct clk_bulk_data sys_clk[STF_CLK_NUM];
	int nclks;
	struct reset_control_bulk_data sys_rst[STF_RST_NUM];
	int nrsts;
};

struct stfcamss_async_subdev {
	struct v4l2_async_subdev asd;  /* must be first */
	enum port_num port;
};

static inline u32 stf_isp_reg_read(struct stfcamss *stfcamss, u32 reg)
{
	return ioread32(stfcamss->isp_base + reg);
}

static inline void stf_isp_reg_write(struct stfcamss *stfcamss,
				     u32 reg, u32 val)
{
	iowrite32(val, stfcamss->isp_base + reg);
}

static inline void stf_isp_reg_write_delay(struct stfcamss *stfcamss,
					   u32 reg, u32 val, u32 delay)
{
	iowrite32(val, stfcamss->isp_base + reg);
	usleep_range(1000 * delay, 1000 * delay + 100);
}

static inline void stf_isp_reg_set_bit(struct stfcamss *stfcamss,
				       u32 reg, u32 mask, u32 val)
{
	u32 value;

	value = ioread32(stfcamss->isp_base + reg) & ~mask;
	val &= mask;
	val |= value;
	iowrite32(val, stfcamss->isp_base + reg);
}

static inline void stf_isp_reg_set(struct stfcamss *stfcamss, u32 reg, u32 mask)
{
	iowrite32(ioread32(stfcamss->isp_base + reg) | mask,
		  stfcamss->isp_base + reg);
}

static inline u32 stf_syscon_reg_read(struct stfcamss *stfcamss, u32 reg)
{
	return ioread32(stfcamss->syscon_base + reg);
}

static inline void stf_syscon_reg_write(struct stfcamss *stfcamss,
					u32 reg, u32 val)
{
	iowrite32(val, stfcamss->syscon_base + reg);
}

static inline void stf_syscon_reg_set_bit(struct stfcamss *stfcamss,
					  u32 reg, u32 bit_mask)
{
	u32 value;

	value = ioread32(stfcamss->syscon_base + reg);
	iowrite32(value | bit_mask, stfcamss->syscon_base + reg);
}

static inline void stf_syscon_reg_clear_bit(struct stfcamss *stfcamss,
					    u32 reg, u32 bit_mask)
{
	u32 value;

	value = ioread32(stfcamss->syscon_base + reg);
	iowrite32(value & ~bit_mask, stfcamss->syscon_base + reg);
}
#endif /* STF_CAMSS_H */
