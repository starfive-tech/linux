// SPDX-License-Identifier: GPL-2.0
/*
 * stf_vin_hw_ops.c
 *
 * Register interface file for StarFive VIN module driver
 *
 * Copyright (C) 2021-2023 StarFive Technology Co., Ltd.
 */
#include "stf_camss.h"

static void vin_intr_clear(void __iomem *syscon_base)
{
	reg_set_bit(syscon_base, SYSCONSAIF_SYSCFG(28),
		    U0_VIN_CNFG_AXIWR0_INTR_CLEAN, 0x1);
	reg_set_bit(syscon_base, SYSCONSAIF_SYSCFG(28),
		    U0_VIN_CNFG_AXIWR0_INTR_CLEAN, 0x0);
}

static irqreturn_t stf_vin_wr_irq_handler(int irq, void *priv)
{
	struct stf_vin_dev *vin_dev = priv;
	struct stfcamss *stfcamss = vin_dev->stfcamss;
	struct dummy_buffer *dummy_buffer =
			&vin_dev->dummy_buffer[STF_DUMMY_VIN];

	if (atomic_dec_if_positive(&dummy_buffer->frame_skip) < 0) {
		vin_dev->isr_ops->isr_change_buffer(&vin_dev->line[VIN_LINE_WR]);
		vin_dev->isr_ops->isr_buffer_done(&vin_dev->line[VIN_LINE_WR]);
	}

	vin_intr_clear(stfcamss->syscon_base);

	return IRQ_HANDLED;
}

static void __iomem *stf_vin_get_ispbase(struct stf_vin_dev *vin_dev)
{
	void __iomem *base = vin_dev->stfcamss->isp_base;

	return base;
}

static irqreturn_t stf_vin_isp_irq_handler(int irq, void *priv)
{
	struct stf_vin_dev *vin_dev = priv;
	void __iomem *ispbase = stf_vin_get_ispbase(vin_dev);
	u32 int_status;

	int_status = reg_read(ispbase, ISP_REG_ISP_CTRL_0);

	if (int_status & BIT(24)) {
		if ((int_status & BIT(11)))
			vin_dev->isr_ops->isr_buffer_done(
				&vin_dev->line[VIN_LINE_ISP_SS0]);

		if ((int_status & BIT(12)))
			vin_dev->isr_ops->isr_buffer_done(
				&vin_dev->line[VIN_LINE_ISP_SS1]);

		if ((int_status & BIT(20)))
			vin_dev->isr_ops->isr_buffer_done(
				&vin_dev->line[VIN_LINE_ISP]);

		if (int_status & BIT(25))
			vin_dev->isr_ops->isr_buffer_done(
				&vin_dev->line[VIN_LINE_ISP_RAW]);

		/* clear interrupt */
		reg_write(ispbase,
			  ISP_REG_ISP_CTRL_0,
			  (int_status & ~EN_INT_ALL) |
			  EN_INT_ISP_DONE |
			  EN_INT_CSI_DONE |
			  EN_INT_SC_DONE);

	} else {
		st_debug(ST_VIN, "%s, Unknown interrupt!\n", __func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t stf_vin_isp_irq_csi_handler(int irq, void *priv)
{
	struct stf_vin_dev *vin_dev = priv;
	void __iomem *ispbase = stf_vin_get_ispbase(vin_dev);
	u32 int_status;

	int_status = reg_read(ispbase, ISP_REG_ISP_CTRL_0);

	if (int_status & BIT(25)) {
		vin_dev->isr_ops->isr_buffer_done(
			&vin_dev->line[VIN_LINE_ISP_RAW]);

		/* clear interrupt */
		reg_write(ispbase,
			  ISP_REG_ISP_CTRL_0,
			  (int_status & ~EN_INT_ALL) | EN_INT_CSI_DONE);
	} else {
		st_debug(ST_VIN, "%s, Unknown interrupt!!!\n", __func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t stf_vin_isp_irq_csiline_handler(int irq, void *priv)
{
	struct stf_vin_dev *vin_dev = priv;
	struct stf_isp_dev *isp_dev;
	void __iomem *ispbase = stf_vin_get_ispbase(vin_dev);
	u32 int_status, value;

	isp_dev = vin_dev->stfcamss->isp_dev;

	int_status = reg_read(ispbase, ISP_REG_ISP_CTRL_0);
	if (int_status & BIT(27)) {
		struct dummy_buffer *dummy_buffer =
			&vin_dev->dummy_buffer[STF_DUMMY_ISP];

		if (!atomic_read(&isp_dev->shadow_count)) {
			if (atomic_dec_if_positive(&dummy_buffer->frame_skip) < 0) {
				if ((int_status & BIT(11)))
					vin_dev->isr_ops->isr_change_buffer(
						&vin_dev->line[VIN_LINE_ISP_SS0]);
				if ((int_status & BIT(12)))
					vin_dev->isr_ops->isr_change_buffer(
						&vin_dev->line[VIN_LINE_ISP_SS1]);
				if ((int_status & BIT(20)))
					vin_dev->isr_ops->isr_change_buffer(
						&vin_dev->line[VIN_LINE_ISP]);
				value = reg_read(ispbase,
						 ISP_REG_CSI_MODULE_CFG);
				if ((value & BIT(19)))
					vin_dev->isr_ops->isr_change_buffer(
						&vin_dev->line[VIN_LINE_ISP_RAW]);
			}

			/* shadow update */
			reg_set_bit(ispbase, ISP_REG_CSIINTS_ADDR,
				    0x30000, 0x30000);
			reg_set_bit(ispbase, ISP_REG_IESHD_ADDR,
				    BIT(1) | BIT(0), 0x3);
		} else {
			st_err_ratelimited(ST_VIN, "skip this frame\n");
		}

		/* clear interrupt */
		reg_write(ispbase, ISP_REG_ISP_CTRL_0,
			  (int_status & ~EN_INT_ALL) | EN_INT_LINE_INT);
	} else {
		st_debug(ST_VIN, "%s, Unknown interrupt!!!\n", __func__);
	}

	return IRQ_HANDLED;
}

static int stf_vin_clk_enable(struct stf_vin_dev *vin_dev, enum link link)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	clk_set_rate(stfcamss->sys_clk[STF_CLK_APB_FUNC].clk, 49500000);

	switch (link) {
	case LINK_DVP_TO_WR:
		break;
	case LINK_DVP_TO_ISP:
		break;
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
	default:
		return -EINVAL;
	}

	return 0;
}

static int stf_vin_clk_disable(struct stf_vin_dev *vin_dev, enum link link)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	switch (link) {
	case LINK_DVP_TO_WR:
		break;
	case LINK_DVP_TO_ISP:
		break;
	case LINK_CSI_TO_WR:
		reset_control_assert(stfcamss->sys_rst[STF_RST_AXIWR].rstc);
		break;
	case LINK_CSI_TO_ISP:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int stf_vin_wr_stream_set(struct stf_vin_dev *vin_dev, int on)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	/* make the axiwr alway on */
	if (on)
		reg_set(stfcamss->syscon_base,
			SYSCONSAIF_SYSCFG(20), U0_VIN_CNFG_AXIWR0_EN);

	return 0;
}

static int stf_vin_stream_set(struct stf_vin_dev *vin_dev, int on,
			      struct v4l2_subdev_format sensor_fmt,
			      enum link link)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	switch (link) {
	case LINK_DVP_TO_WR:
		break;
	case LINK_DVP_TO_ISP:
		break;
	case LINK_CSI_TO_WR:
		reg_set_bit(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(20),
			    U0_VIN_CNFG_AXIWR0_CHANNEL_SEL, 0 << 0);
		reg_set_bit(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(28),
			    U0_VIN_CNFG_AXIWR0_PIX_CT, 1 << 13);
		reg_set_bit(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(28),
			    U0_VIN_CNFG_AXIWR0_PIXEL_HEIGH_BIT_SEL, 0 << 15);
		reg_set_bit(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(28),
			    U0_VIN_CNFG_AXIWR0_PIX_CNT_END,
			    (sensor_fmt.format.width / 4 - 1) << 2);
		break;
	case LINK_CSI_TO_ISP:
		reg_set_bit(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(36),
			    U0_VIN_CNFG_MIPI_BYTE_EN_ISP0, 0 << 6);
		reg_set_bit(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(36),
			    U0_VIN_CNFG_MIPI_CHANNEL_SEL0, 0 << 8);
		reg_set_bit(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(36),
			    U0_VIN_CNFG_PIX_NUM, 0 << 13);

		if (sensor_fmt.format.code == MEDIA_BUS_FMT_SRGGB10_1X10 ||
		    sensor_fmt.format.code == MEDIA_BUS_FMT_SGRBG10_1X10 ||
		    sensor_fmt.format.code == MEDIA_BUS_FMT_SGBRG10_1X10 ||
		    sensor_fmt.format.code == MEDIA_BUS_FMT_SBGGR10_1X10)
			reg_set_bit(stfcamss->syscon_base,
				    SYSCONSAIF_SYSCFG(36),
				    U0_VIN_CNFG_P_I_MIPI_HAEDER_EN0,
				    1 << 12);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void stf_vin_wr_irq_enable(struct stf_vin_dev *vin_dev, int enable)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;
	unsigned int value = 0;

	if (enable) {
		value = ~(0x1 << 1);
		reg_set_bit(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(28),
			    U0_VIN_CNFG_AXIWR0_INTR_MASK, value);
	} else {
		/* clear vin interrupt */
		value = 0x1 << 1;
		reg_set_bit(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(28),
			    U0_VIN_CNFG_AXIWR0_INTR_CLEAN, 0x1);
		reg_set_bit(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(28),
			    U0_VIN_CNFG_AXIWR0_INTR_CLEAN, 0x0);
		reg_set_bit(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(28),
			    U0_VIN_CNFG_AXIWR0_INTR_MASK, value);
	}
}

static void stf_vin_wr_set_ping_addr(struct stf_vin_dev *vin_dev,
				     dma_addr_t addr)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	/* set the start address */
	reg_write(stfcamss->syscon_base,  SYSCONSAIF_SYSCFG(32), (long)addr);
}

static void stf_vin_wr_set_pong_addr(struct stf_vin_dev *vin_dev,
				     dma_addr_t addr)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

	/* set the start address */
	reg_write(stfcamss->syscon_base, SYSCONSAIF_SYSCFG(24), (long)addr);
}

static void stf_vin_isp_set_yuv_addr(struct stf_vin_dev *vin_dev,
				     dma_addr_t y_addr, dma_addr_t uv_addr)
{
	void __iomem *ispbase = stf_vin_get_ispbase(vin_dev);

	reg_write(ispbase, ISP_REG_Y_PLANE_START_ADDR, y_addr);
	reg_write(ispbase, ISP_REG_UV_PLANE_START_ADDR, uv_addr);
}

static void stf_vin_isp_set_raw_addr(struct stf_vin_dev *vin_dev,
				     dma_addr_t raw_addr)
{
	void __iomem *ispbase = stf_vin_get_ispbase(vin_dev);

	reg_write(ispbase, ISP_REG_DUMP_CFG_0, raw_addr);
}

static void stf_vin_isp_set_ss0_addr(struct stf_vin_dev *vin_dev,
				     dma_addr_t y_addr, dma_addr_t uv_addr)
{
	void __iomem *ispbase = stf_vin_get_ispbase(vin_dev);

	reg_write(ispbase, ISP_REG_SS0AY, y_addr);
	reg_write(ispbase, ISP_REG_SS0AUV, uv_addr);
}

static void stf_vin_isp_set_ss1_addr(struct stf_vin_dev *vin_dev,
				     dma_addr_t y_addr, dma_addr_t uv_addr)
{
	void __iomem *ispbase = stf_vin_get_ispbase(vin_dev);

	reg_write(ispbase, ISP_REG_SS1AY, y_addr);
	reg_write(ispbase, ISP_REG_SS1AUV, uv_addr);
}

const struct vin_hw_ops vin_ops = {
	.vin_clk_enable        = stf_vin_clk_enable,
	.vin_clk_disable       = stf_vin_clk_disable,
	.vin_wr_stream_set     = stf_vin_wr_stream_set,
	.vin_wr_irq_enable     = stf_vin_wr_irq_enable,
	.vin_stream_set        = stf_vin_stream_set,
	.vin_wr_set_ping_addr  = stf_vin_wr_set_ping_addr,
	.vin_wr_set_pong_addr  = stf_vin_wr_set_pong_addr,
	.vin_isp_set_yuv_addr  = stf_vin_isp_set_yuv_addr,
	.vin_isp_set_raw_addr  = stf_vin_isp_set_raw_addr,
	.vin_isp_set_ss0_addr  = stf_vin_isp_set_ss0_addr,
	.vin_isp_set_ss1_addr  = stf_vin_isp_set_ss1_addr,
	.vin_wr_irq_handler    = stf_vin_wr_irq_handler,
	.vin_isp_irq_handler   = stf_vin_isp_irq_handler,
	.vin_isp_irq_csi_handler   = stf_vin_isp_irq_csi_handler,
	.vin_isp_irq_csiline_handler   = stf_vin_isp_irq_csiline_handler,
};
