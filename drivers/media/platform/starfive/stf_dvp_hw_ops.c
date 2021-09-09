// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#include "stfcamss.h"

static int stf_dvp_clk_init(struct stf_dvp_dev *dvp_dev)
{
	return 0;
}

static void stf_dvp_io_pad_config(struct stf_vin_dev *vin)
{
	/*
	 * pin: 49 ~ 57
	 * offset: 0x144 ~ 0x164
	 * SCFG_funcshare_pad_ctrl
	 */
	u32 val_scfg_funcshare_config = 0x800080;

	iowrite32(val_scfg_funcshare_config,
			vin->vin_top_iopad_base + IOPAD_REG81);
	iowrite32(val_scfg_funcshare_config,
			vin->vin_top_iopad_base + IOPAD_REG82);
	iowrite32(val_scfg_funcshare_config,
			vin->vin_top_iopad_base + IOPAD_REG83);
	iowrite32(val_scfg_funcshare_config,
			vin->vin_top_iopad_base + IOPAD_REG84);
	iowrite32(val_scfg_funcshare_config,
			vin->vin_top_iopad_base + IOPAD_REG85);
	iowrite32(val_scfg_funcshare_config,
			vin->vin_top_iopad_base + IOPAD_REG86);
	iowrite32(val_scfg_funcshare_config,
			vin->vin_top_iopad_base + IOPAD_REG87);
	iowrite32(val_scfg_funcshare_config,
			vin->vin_top_iopad_base + IOPAD_REG88);
	iowrite32(val_scfg_funcshare_config,
			vin->vin_top_iopad_base + IOPAD_REG89);
}

static int stf_dvp_config_set(struct stf_dvp_dev *dvp_dev)
{
	struct stf_vin_dev *vin = dvp_dev->stfcamss->vin;
	unsigned int flags = 0;
	unsigned char data_shift = 0;
	u32 polarities = 0;

	if (!dvp_dev->dvp)
		return -EINVAL;

	flags = dvp_dev->dvp->flags;
	data_shift = dvp_dev->dvp->data_shift;
	st_info(ST_DVP, "%s, polarities = 0x%x, flags = 0x%x\n",
			__func__, polarities, flags);

	// stf_dvp_io_pad_config(vin);

	if (flags & V4L2_MBUS_HSYNC_ACTIVE_HIGH)
		polarities |= BIT(9);

	if (flags & V4L2_MBUS_VSYNC_ACTIVE_HIGH)
		polarities |= BIT(8);

	print_reg(ST_DVP, vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL);
	reg_set_bit(vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL,
			BIT(9) | BIT(8), polarities);
	print_reg(ST_DVP, vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL);

	switch (data_shift) {
	case 0:
		data_shift = 0;
		break;
	case 2:
		data_shift = 1;
		break;
	case 4:
		data_shift = 2;
		break;
	case 6:
		data_shift = 3;
		break;
	default:
		data_shift = 0;
		break;
	};
	print_reg(ST_DVP, vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL);
	reg_set_bit(vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL,
			BIT(13) | BIT(12), data_shift << 12);
	print_reg(ST_DVP, vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL);

	return 0;
}

static int set_vin_axiwr_pix_ct(struct stf_vin_dev *vin, u8 bpp)
{
	u32 value = 0;
	int cnfg_axiwr_pix_ct = 64 / bpp;

	// need check
	if (cnfg_axiwr_pix_ct == 2)
		value = 1;
	else if (cnfg_axiwr_pix_ct == 4)
		value = 2;
	else if (cnfg_axiwr_pix_ct == 8)
		value = 0;
	else
		return 0;

	print_reg(ST_DVP, vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL);
	reg_set_bit(vin->sysctrl_base,
			SYSCTRL_VIN_RW_CTRL, BIT(1) | BIT(0), value);
	print_reg(ST_DVP, vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL);

	return cnfg_axiwr_pix_ct;
}

static int stf_dvp_set_format(struct stf_dvp_dev *dvp_dev,
		u32 pix_width, u8 bpp)
{
	struct stf_vin_dev *vin = dvp_dev->stfcamss->vin;
	int val, pix_ct;

	if (dvp_dev->s_type == SENSOR_VIN) {
		pix_ct = set_vin_axiwr_pix_ct(vin, bpp);
		val = (pix_width / pix_ct) - 1;
		print_reg(ST_DVP, vin->sysctrl_base, SYSCTRL_VIN_WR_PIX_TOTAL);
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_WR_PIX_TOTAL, val);
		print_reg(ST_DVP, vin->sysctrl_base, SYSCTRL_VIN_WR_PIX_TOTAL);
	}
	return 0;
}

static int stf_dvp_stream_set(struct stf_dvp_dev *dvp_dev, int on)
{
	struct stf_vin_dev *vin = dvp_dev->stfcamss->vin;

	switch (dvp_dev->s_type) {
	case SENSOR_VIN:
		reg_set_bit(vin->sysctrl_base, SYSCTRL_VIN_AXI_CTRL, BIT(0), on);
		reg_set_bit(vin->clkgen_base, CLK_VIN_AXI_WR_CTRL,
				BIT(25) | BIT(24), 2 << 24);
		break;
	case SENSOR_ISP0:
		reg_set_bit(vin->sysctrl_base,
				SYSCTRL_VIN_SRC_CHAN_SEL, BIT(8), !!on << 8);
		break;
	case SENSOR_ISP1:
		reg_set_bit(vin->sysctrl_base,
				SYSCTRL_VIN_SRC_CHAN_SEL, BIT(12), !!on << 12);
		break;
	default:
		break;
	}
	return 0;
}

struct dvp_hw_ops dvp_ops = {
	.dvp_clk_init          = stf_dvp_clk_init,
	.dvp_config_set        = stf_dvp_config_set,
	.dvp_set_format        = stf_dvp_set_format,
	.dvp_stream_set        = stf_dvp_stream_set,
};
