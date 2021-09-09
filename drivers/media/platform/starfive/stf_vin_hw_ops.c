// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 StarFive Technology Co., Ltd.
 */
#include "stfcamss.h"

static int vin_rstgen_assert_reset(struct stf_vin_dev *vin)
{
	u32 val;
	/*
	 *      Software_RESET_assert1 (0x11840004)
	 *      ------------------------------------
	 *      bit[15]         rstn_vin_src
	 *      bit[16]         rstn_ispslv_axi
	 *      bit[17]         rstn_vin_axi
	 *      bit[18]         rstn_vinnoc_axi
	 *      bit[19]         rstn_isp0_axi
	 *      bit[20]         rstn_isp0noc_axi
	 *      bit[21]         rstn_isp1_axi
	 *      bit[22]         rstn_isp1noc_axi
	 *
	 */
	u32 val_reg_reset_config = 0x7f8000;

	val = ioread32(vin->vin_top_rstgen_base + SOFTWARE_RESET_ASSERT1);
	val |= val_reg_reset_config;
	iowrite32(val, vin->vin_top_rstgen_base + SOFTWARE_RESET_ASSERT1);

	val = ioread32(vin->vin_top_rstgen_base + SOFTWARE_RESET_ASSERT1);
	val &= ~(val_reg_reset_config);

	iowrite32(val, vin->vin_top_rstgen_base + SOFTWARE_RESET_ASSERT1);

	return 0;
}

static void vin_intr_clear(void __iomem * sysctrl_base)
{
	reg_set_bit(sysctrl_base, SYSCTRL_VIN_INTP_CTRL, BIT(0), 0x1);
	reg_set_bit(sysctrl_base, SYSCTRL_VIN_INTP_CTRL, BIT(0), 0x0);
}

static irqreturn_t stf_vin_wr_irq_handler(int irq, void *priv)
{
	static struct vin_params params;
	struct stf_vin2_dev *vin_dev = priv;
	struct stf_vin_dev *vin = vin_dev->stfcamss->vin;

	vin_dev->hw_ops->isr_buffer_done(&vin_dev->line[VIN_LINE_WR], &params);

	vin_intr_clear(vin->sysctrl_base);

	return IRQ_HANDLED;
}

static irqreturn_t stf_vin_isp_irq_handler(int irq, void *priv)
{
	static struct vin_params params;
	struct stf_vin2_dev *vin_dev = priv;
	struct stf_vin_dev *vin = vin_dev->stfcamss->vin;
	void __iomem *ispbase;
	u32 int_status, value;
	int isp_id = irq == vin->isp0_irq ? 0 : 1;

	if (isp_id == 0)
		ispbase = vin->isp_isp0_base;
	else
		ispbase = vin->isp_isp1_base;

	int_status = reg_read(ispbase, ISP_REG_ISP_CTRL_0);

	// if (int_status & BIT(24))
	vin_dev->hw_ops->isr_buffer_done(
		&vin_dev->line[VIN_LINE_ISP0 + isp_id], &params);

	value = reg_read(ispbase, ISP_REG_CIS_MODULE_CFG);
	if ((value & BIT(19)) && (int_status & BIT(25)))
		vin_dev->hw_ops->isr_buffer_done(
			&vin_dev->line[VIN_LINE_ISP0_RAW + isp_id], &params);

	/* clear interrupt */
	reg_write(ispbase, ISP_REG_ISP_CTRL_0, int_status);

	return IRQ_HANDLED;
}

static int stf_vin_clk_init(struct stf_vin2_dev *vin_dev)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;
	struct stf_vin_dev *vin = stfcamss->vin;
	int ret = 0;

#ifdef USE_CLK_TREE
	// enable clk
	ret = stfcamss_enable_clocks(8, &stfcamss->sys_clk[STFCLK_VIN_SRC],
			stfcamss->dev);
	if (ret < 0) {
		st_err(ST_VIN, "%s enable clk failed\n", __func__);
		return ret;
	}

	vin_rstgen_assert_reset(vin);

	// hold vin resets for sub modules before csi2rx controller get configed
	reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 0xffffffff);
	mdelay(10);

	// clear reset for all vin submodules
	// except dphy-rx (follow lunhai's advice)
	reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 1 << 17);
	mdelay(10);

	// disable clk
	stfcamss_disable_clocks(8, &stfcamss->sys_clk[STFCLK_VIN_SRC]);
	return ret;
#else
	val = ioread32(vin->vin_top_clkgen_base + 0x124) >> 24;
	val &= 0x1;
	if (val != 0) {
		val = ioread32(vin->vin_top_clkgen_base + 0x124) >> 24;
		val &= ~(0x1 << 24);
		val |= (0x0 & 0x1) << 24;
		iowrite32(val, vin->vin_top_clkgen_base + 0x124);
	} else {
		st_debug(ST_VIN, "nne bus clk src is already clk_cpu_axi\n");
	}

	// enable clk
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_VIN_SRC_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP0_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP0NOC_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISPSLV_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP1_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP1NOC_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_VIN_AXI);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_VINNOC_AXI);

	vin_rstgen_assert_reset(vin);

	// hold vin resets for sub modules before csi2rx controller get configed
	reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 0xffffffff);
	mdelay(10);

	// clear reset for all vin submodules
	// except dphy-rx (follow lunhai's advice)
	reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 1 << 17);
	mdelay(10);

	// disable clk
	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_VIN_SRC_CTRL);
	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_ISPSLV_AXI_CTRL);
	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_VIN_AXI);
	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_VINNOC_AXI);

	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP0_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP0NOC_AXI_CTRL);

	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP1_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP1NOC_AXI_CTRL);
	return 0;
#endif
}

static int stf_vin_clk_enable(struct stf_vin2_dev *vin_dev)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;
	struct stf_vin_dev *vin = stfcamss->vin;
	int ret = 0;

#ifdef USE_CLK_TREE
	// enable clk
	ret = stfcamss_enable_clocks(8, &stfcamss->sys_clk[STFCLK_VIN_SRC],
			stfcamss->dev);
	if (ret < 0) {
		st_err(ST_VIN, "%s enable clk failed\n", __func__);
		return ret;
	}

	/* rst disable */
	reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 0xFFFFFFFF);

	/* rst enable */
	reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 0x0);

	return ret;
#else
	val = ioread32(vin->vin_top_clkgen_base + 0x124) >> 24;
	val &= 0x1;
	if (val != 0) {
		val = ioread32(vin->vin_top_clkgen_base + 0x124) >> 24;
		val &= ~(0x1 << 24);
		val |= (0x0 & 0x1) << 24;
		iowrite32(val, vin->vin_top_clkgen_base + 0x124);
	} else {
		st_debug(ST_VIN, "nne bus clk src is already clk_cpu_axi\n");
	}

	// enable clk
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_VIN_SRC_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISPSLV_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_VIN_AXI);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_VINNOC_AXI);

	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP0_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP0NOC_AXI_CTRL);

	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP1_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base, CLK_ISP1NOC_AXI_CTRL);

	/* rst disable */
	reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 0xFFFFFFFF);

	/* rst enable */
	reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 0x0);

	return 0;
#endif
}

static int stf_vin_clk_disable(struct stf_vin2_dev *vin_dev)
{
	struct stfcamss *stfcamss = vin_dev->stfcamss;

#ifdef USE_CLK_TREE
	stfcamss_disable_clocks(8, &stfcamss->sys_clk[STFCLK_VIN_SRC]);
#else
	// disable clk
	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_VIN_SRC_CTRL);
	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_ISPSLV_AXI_CTRL);
	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_VIN_AXI);
	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_VINNOC_AXI);

	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_ISP0_AXI_CTRL);
	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_ISP0NOC_AXI_CTRL);

	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_ISP1_AXI_CTRL);
	reg_clr_highest_bit(vin->vin_top_clkgen_base, CLK_ISP1NOC_AXI_CTRL);
#endif
	return 0;
}

static int stf_vin_config_set(struct stf_vin2_dev *vin_dev)
{

	return 0;
}

static int stf_vin_wr_stream_set(struct stf_vin2_dev *vin_dev, int on)
{
	struct stf_vin_dev *vin = vin_dev->stfcamss->vin;

	print_reg(ST_VIN, vin->sysctrl_base, SYSCTRL_VIN_AXI_CTRL);
	if (on) {
		reg_set(vin->sysctrl_base,
				SYSCTRL_VIN_AXI_CTRL, BIT(1));
	} else {
		reg_clear(vin->sysctrl_base,
				SYSCTRL_VIN_AXI_CTRL, BIT(1));
	}
	print_reg(ST_VIN, vin->sysctrl_base, SYSCTRL_VIN_AXI_CTRL);
	return 0;
}

static void stf_vin_wr_irq_enable(struct stf_vin2_dev *vin_dev,
		int enable)
{
	struct stf_vin_dev *vin = vin_dev->stfcamss->vin;
	unsigned int value = 0;

	if (enable) {
		// value = ~((0x1 << 4) | (0x1 << 20));
		value = ~(0x1 << 4);

		reg_set_bit(vin->sysctrl_base, SYSCTRL_VIN_INTP_CTRL, BIT(4), value);
	} else {
		/* mask and clear vin interrupt */
		// mask_value = (0x1 << 4) | (0x1 << 20);
		// value = 0x1 | (0x1 << 16) | mask_value;
		reg_set_bit(vin->sysctrl_base, SYSCTRL_VIN_INTP_CTRL, BIT(0), 0x1);
		reg_set_bit(vin->sysctrl_base, SYSCTRL_VIN_INTP_CTRL, BIT(0), 0x0);

		value = 0x1 << 4;
		reg_set_bit(vin->sysctrl_base, SYSCTRL_VIN_INTP_CTRL, BIT(4), value);
	}
}

static void stf_vin_wr_rd_set_addr(struct stf_vin2_dev *vin_dev,
		dma_addr_t wr_addr, dma_addr_t rd_addr)
{
	struct stf_vin_dev *vin = vin_dev->stfcamss->vin;

	/* set the start address*/
	reg_write(vin->sysctrl_base,
			SYSCTRL_VIN_WR_START_ADDR, (long)wr_addr);
	reg_write(vin->sysctrl_base,
			SYSCTRL_VIN_RD_END_ADDR, (long)rd_addr);
}

void stf_vin_wr_set_ping_addr(struct stf_vin2_dev *vin_dev,
		dma_addr_t addr)
{
	struct stf_vin_dev *vin = vin_dev->stfcamss->vin;

	/* set the start address */
	reg_write(vin->sysctrl_base, SYSCTRL_VIN_WR_START_ADDR, (long)addr);
}

void stf_vin_wr_set_pong_addr(struct stf_vin2_dev *vin_dev, dma_addr_t addr)
{
	struct stf_vin_dev *vin = vin_dev->stfcamss->vin;

	/* set the start address */
	reg_write(vin->sysctrl_base, SYSCTRL_VIN_RD_END_ADDR, (long)addr);
}

void stf_vin_isp_set_yuv_addr(struct stf_vin2_dev *vin_dev, int isp_id,
				dma_addr_t y_addr, dma_addr_t uv_addr)
{
	struct stf_vin_dev *vin = vin_dev->stfcamss->vin;
	void __iomem *ispbase =
		isp_id ? vin->isp_isp1_base : vin->isp_isp0_base;

	reg_set_bit(ispbase, ISP_REG_ISP_CTRL_0, BIT(0), 0);
	reg_write(ispbase, ISP_REG_Y_PLANE_START_ADDR, y_addr);
	reg_write(ispbase, ISP_REG_UV_PLANE_START_ADDR, uv_addr);
	// shadow update
	reg_set_bit(ispbase, ISP_REG_IESHD_ADDR, BIT(1) | BIT(0), 0x3);
	reg_set_bit(ispbase, ISP_REG_ISP_CTRL_0, BIT(0), 1);
}

void stf_vin_isp_set_raw_addr(struct stf_vin2_dev *vin_dev, int isp_id,
				dma_addr_t raw_addr)
{
	struct stf_vin_dev *vin = vin_dev->stfcamss->vin;
	void __iomem *ispbase =
		isp_id ? vin->isp_isp1_base : vin->isp_isp0_base;

	reg_write(ispbase, ISP_REG_DUMP_CFG_0, raw_addr);
	reg_set_bit(ispbase, ISP_REG_CSIINTS_ADDR, 0x3FFFF, 0x3000a);
}

void dump_vin_reg(void *__iomem regbase)
{
	st_debug(ST_VIN, "DUMP VIN register:\n");
	print_reg(ST_VIN, regbase, 0x00);
	print_reg(ST_VIN, regbase, 0x04);
	print_reg(ST_VIN, regbase, 0x08);
	print_reg(ST_VIN, regbase, 0x0c);
	print_reg(ST_VIN, regbase, 0x10);
	print_reg(ST_VIN, regbase, 0x14);
	print_reg(ST_VIN, regbase, 0x18);
	print_reg(ST_VIN, regbase, 0x1c);
	print_reg(ST_VIN, regbase, 0x20);
	print_reg(ST_VIN, regbase, 0x24);
	print_reg(ST_VIN, regbase, 0x28);
	print_reg(ST_VIN, regbase, 0x2c);
	print_reg(ST_VIN, regbase, 0x30);
	print_reg(ST_VIN, regbase, 0x34);
	print_reg(ST_VIN, regbase, 0x38);
	print_reg(ST_VIN, regbase, 0x3c);
	print_reg(ST_VIN, regbase, 0x40);
	print_reg(ST_VIN, regbase, 0x44);
	print_reg(ST_VIN, regbase, 0x48);
	print_reg(ST_VIN, regbase, 0x4c);
	print_reg(ST_VIN, regbase, 0x50);
	print_reg(ST_VIN, regbase, 0x54);
	print_reg(ST_VIN, regbase, 0x58);
	print_reg(ST_VIN, regbase, 0x5c);
}

struct vin_hw_ops vin_ops = {
	.vin_clk_init          = stf_vin_clk_init,
	.vin_clk_enable        = stf_vin_clk_enable,
	.vin_clk_disable       = stf_vin_clk_disable,
	.vin_config_set        = stf_vin_config_set,
	.vin_wr_stream_set     = stf_vin_wr_stream_set,
	.vin_wr_irq_enable     = stf_vin_wr_irq_enable,
	.wr_rd_set_addr        = stf_vin_wr_rd_set_addr,
	.vin_wr_set_ping_addr  = stf_vin_wr_set_ping_addr,
	.vin_wr_set_pong_addr  = stf_vin_wr_set_pong_addr,
	.vin_isp_set_yuv_addr  = stf_vin_isp_set_yuv_addr,
	.vin_isp_set_raw_addr  = stf_vin_isp_set_raw_addr,
	.vin_wr_irq_handler    = stf_vin_wr_irq_handler,
	.vin_isp_irq_handler   = stf_vin_isp_irq_handler,
};
