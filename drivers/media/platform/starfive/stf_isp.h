/* SPDX-License-Identifier: GPL-2.0 */
/*
 * stf_isp.h
 *
 * StarFive Camera Subsystem - ISP Module
 *
 * Copyright (C) 2021-2023 StarFive Technology Co., Ltd.
 */

#ifndef STF_ISP_H
#define STF_ISP_H

#include <media/media-entity.h>
#include <media/v4l2-subdev.h>

#define STF_ISP_SETFILE     "stf_isp0_fw.bin"

#define ISP_RAW_DATA_BITS       12
#define SCALER_RATIO_MAX        1
#define STF_ISP_REG_OFFSET_MAX  0x0FFF
#define STF_ISP_REG_DELAY_MAX   100

/* isp registers */
#define ISP_REG_CSI_INPUT_EN_AND_STATUS   0x00000000
#define ISP_REG_CSIINTS_ADDR              0x00000008
#define ISP_REG_CSI_MODULE_CFG            0x00000010
#define ISP_REG_SENSOR                    0x00000014
#define ISP_REG_RAW_FORMAT_CFG            0x00000018
#define ISP_REG_PIC_CAPTURE_START_CFG     0x0000001C
#define ISP_REG_PIC_CAPTURE_END_CFG       0x00000020
#define ISP_REG_DUMP_CFG_0                0x00000024
#define ISP_REG_DUMP_CFG_1                0x00000028
#define ISP_REG_SCD_CFG_0                 0x00000098
#define ISP_REG_SCD_CFG_1                 0x0000009C
#define ISP_REG_SC_CFG_1                  0x000000BC
#define ISP_REG_ISP_CTRL_0                0x00000A00
#define ISP_REG_ISP_CTRL_1                0x00000A08
#define ISP_REG_PIPELINE_XY_SIZE          0x00000A0C
#define ISP_REG_IESHD_ADDR                0x00000A50
#define ISP_REG_Y_PLANE_START_ADDR        0x00000A80
#define ISP_REG_UV_PLANE_START_ADDR       0x00000A84
#define ISP_REG_STRIDE                    0x00000A88
#define ISP_REG_PIXEL_COORDINATE_GEN      0x00000A8C
#define ISP_REG_SS0AY                     0x00000A94
#define ISP_REG_SS0AUV                    0x00000A98
#define ISP_REG_SS0S                      0x00000A9C
#define ISP_REG_SS0IW                     0x00000AA8
#define ISP_REG_SS1AY                     0x00000AAC
#define ISP_REG_SS1AUV                    0x00000AB0
#define ISP_REG_SS1S                      0x00000AB4
#define ISP_REG_SS1IW                     0x00000AC0
#define ISP_REG_YHIST_CFG_4               0x00000CD8
#define ISP_REG_ITIIWSR                   0x00000B20
#define ISP_REG_ITIDWLSR                  0x00000B24
#define ISP_REG_ITIDWYSAR                 0x00000B28
#define ISP_REG_ITIDWUSAR                 0x00000B2C
#define ISP_REG_ITIDRYSAR                 0x00000B30
#define ISP_REG_ITIDRUSAR                 0x00000B34
#define ISP_REG_ITIPDFR                   0x00000B38
#define ISP_REG_ITIDRLSR                  0x00000B3C
#define ISP_REG_ITIBSR                    0x00000B40
#define ISP_REG_ITIAIR                    0x00000B44
#define ISP_REG_ITIDPSR                   0x00000B48

/* The output line of ISP */
enum isp_line_id {
	STF_ISP_LINE_INVALID = -1,
	STF_ISP_LINE_SRC = 1,
	STF_ISP_LINE_SRC_SS0,
	STF_ISP_LINE_SRC_SS1,
	STF_ISP_LINE_SRC_RAW,
	STF_ISP_LINE_MAX = STF_ISP_LINE_SRC_RAW
};

/* pad id for media framework */
enum isp_pad_id {
	STF_ISP_PAD_SINK = 0,
	STF_ISP_PAD_SRC,
	STF_ISP_PAD_SRC_SS0,
	STF_ISP_PAD_SRC_SS1,
	STF_ISP_PAD_SRC_RAW,
	STF_ISP_PAD_MAX
};

enum {
	EN_INT_NONE                 = 0,
	EN_INT_ISP_DONE             = (0x1 << 24),
	EN_INT_CSI_DONE             = (0x1 << 25),
	EN_INT_SC_DONE              = (0x1 << 26),
	EN_INT_LINE_INT             = (0x1 << 27),
	EN_INT_ALL                  = (0xF << 24),
};

enum {
	INTERFACE_DVP = 0,
	INTERFACE_CSI,
};

struct isp_format {
	u32 code;
	u8 bpp;
};

struct isp_format_table {
	const struct isp_format *fmts;
	int nfmts;
};

struct regval_t {
	u32 addr;
	u32 val;
	u32 mask;
	u32 delay_ms;
};

struct reg_table {
	const struct regval_t *regval;
	int regval_num;
};

struct isp_stream_format {
	struct v4l2_rect rect;
	u32 bpp;
};

struct isp_setfile {
	struct reg_table settings;
	const u8 *data;
	unsigned int size;
	unsigned int state;
};

enum {
	ISP_CROP = 0,
	ISP_COMPOSE,
	ISP_SCALE_SS0,
	ISP_SCALE_SS1,
	ISP_RECT_MAX
};

struct stf_isp_dev {
	enum subdev_type sdev_type;  /* This member must be first */
	struct stfcamss *stfcamss;
	struct v4l2_subdev subdev;
	struct media_pad pads[STF_ISP_PAD_MAX];
	struct v4l2_mbus_framefmt fmt[STF_ISP_PAD_MAX];
	struct isp_stream_format rect[ISP_RECT_MAX];
	const struct isp_format_table *formats;
	unsigned int nformats;
	const struct isp_hw_ops *hw_ops;
	struct mutex power_lock;	/* serialize power control*/
	int power_count;
	struct mutex stream_lock;	/* serialize stream control */
	int stream_count;
	atomic_t shadow_count;

	struct mutex setfile_lock;	/* protects setting files */
	struct isp_setfile setfile;

	union reg_buf *reg_buf;
};

struct isp_hw_ops {
	int (*isp_clk_enable)(struct stf_isp_dev *isp_dev);
	int (*isp_clk_disable)(struct stf_isp_dev *isp_dev);
	int (*isp_reset)(struct stf_isp_dev *isp_dev);
	int (*isp_config_set)(struct stf_isp_dev *isp_dev);
	int (*isp_set_format)(struct stf_isp_dev *isp_dev,
			      struct isp_stream_format *crop,
			      u32 mcode, int type);
	int (*isp_stream_set)(struct stf_isp_dev *isp_dev, int on);
	int (*isp_reg_read)(struct stf_isp_dev *isp_dev, void *arg);
	int (*isp_reg_write)(struct stf_isp_dev *isp_dev, void *arg);
	int (*isp_shadow_trigger)(struct stf_isp_dev *isp_dev);
};

extern const struct isp_hw_ops isp_ops;

int stf_isp_subdev_init(struct stfcamss *stfcamss);
int stf_isp_register(struct stf_isp_dev *isp_dev, struct v4l2_device *v4l2_dev);
int stf_isp_unregister(struct stf_isp_dev *isp_dev);

#endif /* STF_ISP_H */
