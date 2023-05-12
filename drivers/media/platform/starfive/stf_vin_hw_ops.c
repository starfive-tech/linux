// SPDX-License-Identifier: GPL-2.0
/*
 * stf_vin_hw_ops.c
 *
 * Register interface file for StarFive VIN module driver
 *
 * Copyright (C) 2021-2023 StarFive Technology Co., Ltd.
 */
#include "stf_camss.h"

static void vin_intr_clear(struct stfcamss *stfcamss)
{
	stf_syscon_reg_set_bit(stfcamss, SYSCONSAIF_SYSCFG(28),
			       U0_VIN_INTR_CLEAN);
	stf_syscon_reg_clear_bit(stfcamss, SYSCONSAIF_SYSCFG(28),
				 U0_VIN_INTR_CLEAN);
}

irqreturn_t stf_vin_wr_irq_handler(int irq, void *priv)
{
	struct stf_vin_dev *vin_dev = priv;
	struct stfcamss *stfcamss = vin_dev->stfcamss;
	struct dummy_buffer *dummy_buffer =
			&vin_dev->dummy_buffer[STF_DUMMY_VIN];

	if (atomic_dec_if_positive(&dummy_buffer->frame_skip) < 0) {
		vin_dev->isr_ops->isr_change_buffer(&vin_dev->line[VIN_LINE_WR]);
		vin_dev->isr_ops->isr_buffer_done(&vin_dev->line[VIN_LINE_WR]);
	}

	vin_intr_clear(stfcamss);

	return IRQ_HANDLED;
}

irqreturn_t stf_vin_isp_irq_handler(int irq, void *priv)
{
	struct stf_vin_dev *vin_dev = priv;
	u32 int_status;

	int_status = stf_isp_reg_read(vin_dev->stfcamss, ISP_REG_ISP_CTRL_0);

	if (int_status & ISPC_INTS) {
		if ((int_status & ISPC_ENUO))
			vin_dev->isr_ops->isr_buffer_done(
				&vin_dev->line[VIN_LINE_ISP]);

		/* clear interrupt */
		stf_isp_reg_write(vin_dev->stfcamss,
				  ISP_REG_ISP_CTRL_0,
				  (int_status & ~EN_INT_ALL) |
				  EN_INT_ISP_DONE |
				  EN_INT_CSI_DONE |
				  EN_INT_SC_DONE);
	}

	return IRQ_HANDLED;
}

irqreturn_t stf_vin_isp_irq_csiline_handler(int irq, void *priv)
{
	struct stf_vin_dev *vin_dev = priv;
	struct stf_isp_dev *isp_dev;
	u32 int_status;

	isp_dev = &vin_dev->stfcamss->isp_dev;

	int_status = stf_isp_reg_read(vin_dev->stfcamss, ISP_REG_ISP_CTRL_0);
	if (int_status & ISPC_SCFEINT) {
		struct dummy_buffer *dummy_buffer =
			&vin_dev->dummy_buffer[STF_DUMMY_ISP];

		if (atomic_dec_if_positive(&dummy_buffer->frame_skip) < 0) {
			if ((int_status & ISPC_ENUO))
				vin_dev->isr_ops->isr_change_buffer(
					&vin_dev->line[VIN_LINE_ISP]);
		}

		stf_isp_reg_set_bit(isp_dev->stfcamss, ISP_REG_CSIINTS,
				    CSI_INTS_MASK, CSI_INTS(0x3));
		stf_isp_reg_set_bit(isp_dev->stfcamss, ISP_REG_IESHD,
				    SHAD_UP_M | SHAD_UP_EN, 0x3);

		/* clear interrupt */
		stf_isp_reg_write(vin_dev->stfcamss, ISP_REG_ISP_CTRL_0,
				  (int_status & ~EN_INT_ALL) | EN_INT_LINE_INT);
	}

	return IRQ_HANDLED;
}

int stf_vin_clk_enable(struct stf_vin_dev *vin_dev, enum link link)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	clk_set_rate(stfcamss->sys_clk[STF_CLK_APB_FUNC].clk, 49500000);

	switch (link) {
	case LINK_CSI_TO_WR:
		clk_set_rate(stfcamss->sys_clk[STF_CLK_MIPI_RX0_PXL].clk,
			     198000000);
		reset_control_deassert(stfcamss->sys_rst[STF_RST_AXIWR].rstc);
		clk_set_parent(stfcamss->sys_clk[STF_CLK_AXIWR].clk,
			       stfcamss->sys_clk[STF_CLK_MIPI_RX0_PXL].clk);
		break;
	case LINK_CSI_TO_ISP:
		clk_set_rate(stfcamss->sys_clk[STF_CLK_MIPI_RX0_PXL].clk,
			     198000000);
		clk_set_parent(stfcamss->sys_clk[STF_CLK_WRAPPER_CLK_C].clk,
			       stfcamss->sys_clk[STF_CLK_MIPI_RX0_PXL].clk);
		break;
	case LINK_DVP_TO_WR:
	case LINK_DVP_TO_ISP:
	default:
		return -EINVAL;
	}

	return 0;
}

int stf_vin_clk_disable(struct stf_vin_dev *vin_dev, enum link link)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	switch (link) {
	case LINK_CSI_TO_WR:
		reset_control_assert(stfcamss->sys_rst[STF_RST_AXIWR].rstc);
		break;
	case LINK_CSI_TO_ISP:
		break;
	case LINK_DVP_TO_WR:
	case LINK_DVP_TO_ISP:
	default:
		return -EINVAL;
	}

	return 0;
}

int stf_vin_wr_stream_set(struct stf_vin_dev *vin_dev)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	/* make the axiwr alway on */
	stf_syscon_reg_set_bit(stfcamss, SYSCONSAIF_SYSCFG(20),
			       U0_VIN_AXIWR0_EN);

	return 0;
}

int stf_vin_stream_set(struct stf_vin_dev *vin_dev, enum link link)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;
	u32 val;

	switch (link) {
	case LINK_CSI_TO_WR:
		val = stf_syscon_reg_read(stfcamss, SYSCONSAIF_SYSCFG(20));
		val &= ~U0_VIN_CHANNEL_SEL_MASK;
		val |= CHANNEL(0);
		stf_syscon_reg_write(stfcamss, SYSCONSAIF_SYSCFG(20), val);

		val = stf_syscon_reg_read(stfcamss, SYSCONSAIF_SYSCFG(28));
		val &= ~U0_VIN_PIX_CT_MASK;
		val |= PIX_CT(1);

		val &= ~U0_VIN_PIXEL_HEIGH_BIT_SEL_MAKS;
		val |= PIXEL_HEIGH_BIT_SEL(0);

		val &= ~U0_VIN_PIX_CNT_END_MASK;
		val |= PIX_CNT_END(IMAGE_MAX_WIDTH / 4 - 1);

		stf_syscon_reg_write(stfcamss, SYSCONSAIF_SYSCFG(28), val);
		break;
	case LINK_CSI_TO_ISP:
		val = stf_syscon_reg_read(stfcamss, SYSCONSAIF_SYSCFG(36));
		val &= ~U0_VIN_MIPI_BYTE_EN_ISP0_MASK;
		val |= U0_VIN_MIPI_BYTE_EN_ISP0(0);

		val &= ~U0_VIN_MIPI_CHANNEL_SEL0_MASK;
		val |= U0_VIN_MIPI_CHANNEL_SEL0(0);

		val &= ~U0_VIN_PIX_NUM_MASK;
		val |= U0_VIN_PIX_NUM(0);

		val &= ~U0_VIN_P_I_MIPI_HAEDER_EN0_MASK;
		val |= U0_VIN_P_I_MIPI_HAEDER_EN0(1);

		stf_syscon_reg_write(stfcamss, SYSCONSAIF_SYSCFG(36), val);
		break;
	case LINK_DVP_TO_WR:
	case LINK_DVP_TO_ISP:
	default:
		return -EINVAL;
	}

	return 0;
}

void stf_vin_wr_irq_enable(struct stf_vin_dev *vin_dev, int enable)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	if (enable) {
		stf_syscon_reg_clear_bit(stfcamss, SYSCONSAIF_SYSCFG(28),
					 U0_VIN_INTR_M);
	} else {
		/* clear vin interrupt */
		stf_syscon_reg_set_bit(stfcamss, SYSCONSAIF_SYSCFG(28),
				       U0_VIN_INTR_CLEAN);
		stf_syscon_reg_clear_bit(stfcamss, SYSCONSAIF_SYSCFG(28),
					 U0_VIN_INTR_CLEAN);
		stf_syscon_reg_set_bit(stfcamss, SYSCONSAIF_SYSCFG(28),
				       U0_VIN_INTR_M);
	}
}

void stf_vin_wr_set_ping_addr(struct stf_vin_dev *vin_dev, dma_addr_t addr)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	/* set the start address */
	stf_syscon_reg_write(stfcamss, SYSCONSAIF_SYSCFG(32), (long)addr);
}

void stf_vin_wr_set_pong_addr(struct stf_vin_dev *vin_dev, dma_addr_t addr)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	/* set the start address */
	stf_syscon_reg_write(stfcamss, SYSCONSAIF_SYSCFG(24), (long)addr);
}

void stf_vin_isp_set_yuv_addr(struct stf_vin_dev *vin_dev,
			      dma_addr_t y_addr, dma_addr_t uv_addr)
{
	stf_isp_reg_write(vin_dev->stfcamss,
			  ISP_REG_Y_PLANE_START_ADDR, y_addr);
	stf_isp_reg_write(vin_dev->stfcamss,
			  ISP_REG_UV_PLANE_START_ADDR, uv_addr);
}
