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
	STFCLK_CSIDPHY_CFGCLK,
	STFCLK_CSIDPHY_REFCLK,
	STFCLK_CSIDPHY_TXCLKESC,
	STFCLK_MIPIRX0_PIXEL,
	STFCLK_MIPIRX1_PIXEL,
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
	STFCLK_ISP0_CTRL,
	STFCLK_ISP0_2X_CTRL,
	STFCLK_ISP0_MIPI_CTRL,
	STFCLK_ISP1_CTRL,
	STFCLK_ISP1_2X_CTRL,
	STFCLK_ISP1_MIPI_CTRL,
	STFCLK_DOM4_APB_CLK,
	STFCLK_CSI_2RX_APB_CLK,
	STFCLK_VIN_AXI_WR_CTRL,
	STFCLK_VIN_AXI_RD_CTRL,
	STFCLK_C_ISP0_CTRL,
	STFCLK_C_ISP1_CTRL,
	STFCLK_NUM
};

enum stf_rst_num {
	STFRST_VIN_SRC = 0,
	STFRST_ISPSLV_AXI,
	STFRST_VIN_AXI,
	STFRST_VINNOC_AXI,
	STFRST_ISP0_AXI,
	STFRST_ISP0NOC_AXI,
	STFRST_ISP1_AXI,
	STFRST_ISP1NOC_AXI,
	STFRST_SYS_CLK,
	STFRST_PCLK,
	STFRST_SYS_CLK_1,
	STFRST_PIXEL_CLK_IF0,
	STFRST_PIXEL_CLK_IF1,
	STFRST_PIXEL_CLK_IF2,
	STFRST_PIXEL_CLK_IF3,
	STFRST_PIXEL_CLK_IF10,
	STFRST_PIXEL_CLK_IF11,
	STFRST_PIXEL_CLK_IF12,
	STFRST_PIXEL_CLK_IF13,
	STFRST_ISP_0,
	STFRST_ISP_1,
	STFRST_P_AXIRD,
	STFRST_P_AXIWR,
	STFRST_P_ISP0,
	STFRST_P_ISP1,
	STFRST_DPHY_HW_RSTN,
	STFRST_DPHY_RST09_ALWY_ON,
	STFRST_C_ISP0,
	STFRST_C_ISP1,
	STFRST_NUM
};

struct stfcamss_clk {
	struct clk *clk;
	const char *name;
};

struct stfcamss_rst {
	struct reset_control *rst;
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
	struct stfcamss_rst *sys_rst;
	int nrsts;
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
