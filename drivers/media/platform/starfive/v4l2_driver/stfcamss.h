// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#ifndef STFCAMSS_H
#define STFCAMSS_H

#include <linux/io.h>
#include <linux/delay.h>

enum sensor_type {
	SENSOR_VIN,
	SENSOR_ISP0,  // need replace sensor
	SENSOR_ISP1,  // need replace sensor
};

enum subdev_type {
	VIN_DEV_TYPE,
	ISP0_DEV_TYPE,
	ISP1_DEV_TYPE,
};

#include "stf_common.h"
#include "stf_dvp.h"
#include "stf_csi.h"
#include "stf_csiphy.h"
#include "stf_isp.h"
#include "stf_vin.h"

#define STF_PAD_SINK   0
#define STF_PAD_SRC    1
#define STF_PADS_NUM   2

enum port_num {
	CSI2RX0_PORT_NUMBER = 0,
	CSI2RX1_PORT_NUMBER,
	DVP_SENSOR_PORT_NUMBER,
	CSI2RX0_SENSOR_PORT_NUMBER,
	CSI2RX1_SENSOR_PORT_NUMBER
};

enum stf_clk_num {
	STFCLK_VIN_SRC = 0,
	STFCLK_ISP0_AXI,
	STFCLK_ISP0NOC_AXI,
	STFCLK_ISPSLV_AXI,
	STFCLK_ISP1_AXI,
	STFCLK_ISP1NOC_AXI,
	STFCLK_VIN_AXI,
	STFCLK_VINNOC_AXI,
	STFCLK_CSI_2RX_APH_CLK,
	STFCLK_MIPIRX0_PIXEL0,
	STFCLK_MIPIRX0_PIXEL1,
	STFCLK_MIPIRX0_PIXEL2,
	STFCLK_MIPIRX0_PIXEL3,
	STFCLK_MIPIRX0_SYS,
	STFCLK_MIPIRX1_PIXEL0,
	STFCLK_MIPIRX1_PIXEL1,
	STFCLK_MIPIRX1_PIXEL2,
	STFCLK_MIPIRX1_PIXEL3,
	STFCLK_MIPIRX1_SYS,
	STFCLK_CSIDPHY_CFGCLK,
	STFCLK_CSIDPHY_REFCLK,
	STFCLK_CSIDPHY_TXCLKESC,
	STFCLK_ISP0_CTRL,
	STFCLK_ISP0_2X_CTRL,
	STFCLK_ISP0_MIPI_CTRL,
	STFCLK_ISP1_CTRL,
	STFCLK_ISP1_2X_CTRL,
	STFCLK_ISP1_MIPI_CTRL,
	STFCLK_NUM
};

struct stfcamss_clk {
	struct clk *clk;
	const char *name;
};

struct stfcamss {
	struct stf_vin_dev *vin;  // stfcamss phy res
	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct device *dev;
	struct stf_vin2_dev *vin_dev;  // subdev
	struct stf_dvp_dev *dvp_dev;   // subdev
	int csi_num;
	struct stf_csi_dev *csi_dev;   // subdev
	int csiphy_num;
	struct stf_csiphy_dev *csiphy_dev;   // subdev
	int isp_num;
	struct stf_isp_dev *isp_dev;   // subdev
	struct v4l2_async_notifier notifier;
	struct stfcamss_clk *sys_clk;
	int nclks;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_entry;
	struct dentry *vin_debugfs;
#endif
};

struct stfcamss_async_subdev {
	struct v4l2_async_subdev asd;  // must be first
	enum port_num port;
	struct {
		struct dvp_cfg dvp;
		struct csi2phy_cfg csiphy;
	} interface;
};

extern struct media_entity *stfcamss_find_sensor(struct media_entity *entity);
extern int stfcamss_enable_clocks(int nclocks, struct stfcamss_clk *clock,
			struct device *dev);
extern void stfcamss_disable_clocks(int nclocks, struct stfcamss_clk *clock);

#endif /* STFCAMSS_H */
