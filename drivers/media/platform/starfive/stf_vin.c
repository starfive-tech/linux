// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for VIC Video In
 *
 * Copyright (C) starfivetech.Inc
 * Authors: Xing Tang <eric.tang@starfivetech.com>
 *
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_graph.h>
#include <linux/of_address.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <video/stf-vin.h>
#include "stf_isp.h"
#include "stf_csi.h"


static const struct reg_name mem_reg_name[] = {
	{"mipi0"},
	{"vclk"},
	{"vrst"},
	{"mipi1"},
	{"sctrl"},
	{"isp0"},
	{"isp1"},
	{"tclk"},
	{"trst"},
	{"iopad"}
};

static DEFINE_MUTEX(vin_mutex);



static inline u32 reg_read(void __iomem * base, u32 reg)
{
	return ioread32(base + reg);
}

static inline void reg_write(void __iomem * base, u32 reg, u32 val)
{
	iowrite32(val, base + reg);
}

static inline void reg_set(void __iomem * base, u32 reg, u32 mask)
{
	reg_write(base, reg, reg_read(base, reg) | mask);
}

static inline void reg_clear(void __iomem * base, u32 reg, u32 mask)
{
	reg_write(base, reg, reg_read(base, reg) & ~mask);
}

static void reg_set_highest_bit(void __iomem * base, u32 reg)
{
    u32 val;
	val = ioread32(base + reg);
	val &= ~(0x1 << 31);
	val |= (0x1 & 0x1) << 31;
	iowrite32(val, base + reg);
}

static void reg_clear_highest_bit(void __iomem * base, u32 reg)
{
    u32 val;
	val = ioread32(base + reg);
	val &= ~(0x1 << 31);
	val |= (0x0 & 0x1) << 31;
	iowrite32(val, base + reg);
}

static int vin_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct stf_vin_dev *dev;

	mutex_lock(&vin_mutex);

	dev=container_of(inode->i_cdev, struct stf_vin_dev, vin_cdev);

	file->private_data = dev;
//out:
	mutex_unlock(&vin_mutex);
	return ret;
}

static ssize_t vin_read(struct file *file, char __user * buf,
			size_t count, loff_t * ppos)
{
	int ret;
	int data[2];
	struct stf_vin_dev *vin = file->private_data;
	if (vin->condition == false) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(vin->wq, vin->condition != false))
			return -ERESTARTSYS;
	}
	data[0] = vin->odd;
	data[1] = vin->buf.size;

	mutex_lock(&vin_mutex);
	ret = copy_to_user(buf, data, count);
	if (ret != 0) {
		pr_err("Failed to copy data\n");
		return -EINVAL;
	}
	mutex_unlock(&vin_mutex);

	vin->condition = false;

	return count;
}

static int vin_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct vm_operations_struct mmap_mem_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys
#endif
};

static int vin_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct stf_vin_dev *vin = file->private_data;
	size_t size = vma->vm_end - vma->vm_start;

	vma->vm_ops = &mmap_mem_ops;

	/* Remap-pfn-range will mark the range VM_IO */
	if (remap_pfn_range(vma,
			    vma->vm_start,
			    vin->buf.paddr >> PAGE_SHIFT,
			    size, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	return 0;
}

static long int vin_ioctl(struct file *file, unsigned int cmd,
			  long unsigned int arg)
{
	return 0;
}

static const struct file_operations vin_fops = {
	.owner = THIS_MODULE,
	.open = vin_open,
	.read = vin_read,
	.release = vin_release,
	.unlocked_ioctl = vin_ioctl,
	.mmap = vin_mmap,
};

static int vin_get_mem_res(struct platform_device *pdev, struct stf_vin_dev *vin)
{
	struct device *dev = &pdev->dev;
	struct resource	*res;
	void __iomem *regs;
	char *name;
	int i;

	for (i = 0; i < sizeof(mem_reg_name)/sizeof(struct reg_name); i++) {
	    name = (char *)(& mem_reg_name[i]);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
		regs = devm_ioremap_resource(dev, res);
		if (IS_ERR(regs))
			return PTR_ERR(regs);	

		if(!strcmp(name, "mipi0")) {
			vin->mipi0_base = regs;
		} else if (!strcmp(name, "vclk")) {
			vin->clkgen_base = regs;
		} else if (!strcmp(name, "vrst")) {
			vin->rstgen_base = regs;
		} else if (!strcmp(name, "mipi1")) {
			vin->mipi1_base = regs;
		} else if (!strcmp(name, "sctrl")) {
			vin->sysctrl_base = regs;
		} else if (!strcmp(name, "isp0")) {
			vin->isp_isp0_base = regs;
		} else if (!strcmp(name, "isp1")) {
			vin->isp_isp1_base = regs;
		} else if (!strcmp(name, "tclk")) {
			vin->vin_top_clkgen_base = regs;
		} else if (!strcmp(name, "trst")) {
			vin->vin_top_rstgen_base = regs;
		}else if (!strcmp(name, "iopad")) {
			vin->vin_top_iopad_base = regs;
		}else {
			dev_err(&pdev->dev, "Could not match resource name\n");
		}
	}

	return 0;
}

/*-------------------------------------------------------*/
/*
 * NOTE:
 * 	vic clk driver hasn't complete, using follow functions
 * 	to reset
 * TODO: vic clk driver
 *
 */
static int vin_clk_reset(struct stf_vin_dev *vin)
{
	//disable clk
    reg_clear_highest_bit(vin->vin_top_clkgen_base,CLK_VIN_SRC_CTRL);
    reg_clear_highest_bit(vin->vin_top_clkgen_base,CLK_ISP0_AXI_CTRL);
    reg_clear_highest_bit(vin->vin_top_clkgen_base,CLK_ISP0NOC_AXI_CTRL);
    reg_clear_highest_bit(vin->vin_top_clkgen_base,CLK_ISPSLV_AXI_CTRL);
    reg_clear_highest_bit(vin->vin_top_clkgen_base,CLK_ISP1_AXI_CTRL);
	reg_clear_highest_bit(vin->vin_top_clkgen_base,CLK_ISP1NOC_AXI_CTRL);
    reg_clear_highest_bit(vin->vin_top_clkgen_base,CLK_VIN_AXI);
    reg_clear_highest_bit(vin->vin_top_clkgen_base,CLK_VINNOC_AXI);

	//enable clk
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_VIN_SRC_CTRL);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_ISP0_AXI_CTRL);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_ISP0NOC_AXI_CTRL);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_ISPSLV_AXI_CTRL);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_ISP1_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_ISP1NOC_AXI_CTRL);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_VIN_AXI);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_VINNOC_AXI);
	return 0;
}

static int isp_rstgen_assert_reset(struct stf_vin_dev *vin)
{
	 u32 val;	 
	 u32 val_reg_reset_config = 0x19f807;
		 
	 val = ioread32(vin->rstgen_base + SOFTWARE_RESET_ASSERT0);
	 val |= val_reg_reset_config;
	 iowrite32(val, vin->rstgen_base + SOFTWARE_RESET_ASSERT0);
 
	 val = ioread32(vin->rstgen_base + SOFTWARE_RESET_ASSERT0);
	 val &= ~(val_reg_reset_config);
 
	 iowrite32(val, vin->rstgen_base + SOFTWARE_RESET_ASSERT0);
		 	
	return 0;
}

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

/*
 * vin_get_pixel_size:
 *
 * size = (width * height * 4)/(cnfg_axiwr_pix_ct / 2)
 */
static int vin_get_axiwr_pixel_size(struct stf_vin_dev *vin)
{
	u32 value;
	int cnfg_axiwr_pix_ct;

	value = reg_read(vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL) & 0x3;
	if (value == 0)
		cnfg_axiwr_pix_ct = 2;
	else if (value == 1)
		cnfg_axiwr_pix_ct = 4;
	else if (value == 2)
		cnfg_axiwr_pix_ct = 8;
	else
		return 0;

	return (vin->frame.height * vin->frame.width * 4) / (cnfg_axiwr_pix_ct /
							     2);
}

static int vin_update_buf_size(struct stf_vin_dev *vin)
{
	u32 size;
	if (vin->format.format == SRC_DVP_SENSOR_VIN_OV5640) //ov5640 out rgb565
		size = vin->frame.height*vin->frame.width*2;
	else
		size = vin_get_axiwr_pixel_size(vin); //out raw data

	if (size == 0) {
		dev_err(vin->dev, "size is 0");
		return -EINVAL;
	}

	vin->buf.size = size;

	return 0;
}


static void dvp_io_pad_func_shared_config(struct stf_vin_dev *vin)
{
	/*
	 * pin: 49 ~ 57
	 * offset: 0x144 ~ 0x164
	 * SCFG_funcshare_pad_ctrl
	 */
	u32 val_scfg_funcshare_config = 0x800080;
	iowrite32(val_scfg_funcshare_config, vin->vin_top_iopad_base + IOPAD_REG81);
	iowrite32(val_scfg_funcshare_config, vin->vin_top_iopad_base + IOPAD_REG82);
	iowrite32(val_scfg_funcshare_config, vin->vin_top_iopad_base + IOPAD_REG83);
	iowrite32(val_scfg_funcshare_config, vin->vin_top_iopad_base + IOPAD_REG84);
	iowrite32(val_scfg_funcshare_config, vin->vin_top_iopad_base + IOPAD_REG85);
	iowrite32(val_scfg_funcshare_config, vin->vin_top_iopad_base + IOPAD_REG86);
	iowrite32(val_scfg_funcshare_config, vin->vin_top_iopad_base + IOPAD_REG87);
	iowrite32(val_scfg_funcshare_config, vin->vin_top_iopad_base + IOPAD_REG88);
	iowrite32(val_scfg_funcshare_config, vin->vin_top_iopad_base + IOPAD_REG89);
}

static int vin_rstgen_clkgen(struct stf_vin_dev *vin)
{
	u32 val_vin_axi_wr_ctrl_reg = 0x02000000;

	/* rst disable */
	reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 0xFFFFFFFF);

	/* rst enable */
	reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 0x0);

	switch (vin->format.format) {
	case SRC_DVP_SENSOR_VIN_OV5640:
	case SRC_DVP_SENSOR_VIN:
		reg_write(vin->clkgen_base, CLK_VIN_AXI_WR_CTRL, val_vin_axi_wr_ctrl_reg);
		break;

	case SRC_COLORBAR_VIN_ISP:
	case SRC_DVP_SENSOR_VIN_ISP:
		isp_clk_set(vin);
		break;

	case SRC_CSI2RX_VIN_ISP:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int get_vin_axiwr_pix_ct(struct stf_vin_dev *vin)
{
	u32 value;
	int cnfg_axiwr_pix_ct = 0;

	value = reg_read(vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL) & 0x3;
	if (value == 0)
		cnfg_axiwr_pix_ct = 2;
	else if (value == 1)
		cnfg_axiwr_pix_ct = 4;
	else if (value == 2)
		cnfg_axiwr_pix_ct = 8;
	else
		return 0;

	return cnfg_axiwr_pix_ct;
}

static void set_vin_wr_pix_total(struct stf_vin_dev *vin)
{
	int val, pix_ct;
	pix_ct = get_vin_axiwr_pix_ct(vin);
	val = (vin->frame.width / pix_ct) - 1;
	reg_write(vin->sysctrl_base, SYSCTRL_VIN_WR_PIX_TOTAL, val);
}



static void dvp_vin_ddr_addr_config(struct stf_vin_dev *vin)
{
	u32 val_vin_axi_ctrl_reg = 0x00000003;
	dev_dbg(vin->dev, "%d: addr: 0x%lx, size: 0x%lx\n", __LINE__,
		(long)vin->buf.paddr, (long)(vin->buf.paddr + vin->buf.size));
	/* set the start address of vin and enable */
	reg_write(vin->sysctrl_base, SYSCTRL_VIN_WR_START_ADDR, (long)vin->buf.paddr);
	reg_write(vin->sysctrl_base, SYSCTRL_VIN_RD_END_ADDR, (long)(vin->buf.paddr + vin->buf.size));
	reg_write(vin->sysctrl_base, SYSCTRL_VIN_AXI_CTRL, val_vin_axi_ctrl_reg);
}

static int stf_vin_config_set(struct stf_vin_dev *vin)
{
	u32  val_vin_rd_pix_total_reg,val_vin_rd_vblank_reg,val_vin_rd_vend_reg;
	u32  val_vin_rd_hblank_reg,val_vin_rd_hend_reg,val_vin_rw_ctrl_reg;
	u32  val_vin_src_channel_reg,val_vin_axi_ctrl_reg,val_vin_rw_start_addr_reg;
	u32  val_vin_rd_end_addr_reg,val_vin_wr_pix_reg,reset_val;
//	int ret;
	csi2rx_dphy_cfg_t dphy_rx_cfg = {
			.dlane_nb		= vin->csi_fmt.lane,
			.dlane_map		= {vin->csi_fmt.dlane_swap[0],	  vin->csi_fmt.dlane_swap[1],	 vin->csi_fmt.dlane_swap[2],	vin->csi_fmt.dlane_swap[3]},
			.dlane_pn_swap	= {vin->csi_fmt.dlane_pn_swap[0], vin->csi_fmt.dlane_pn_swap[1], vin->csi_fmt.dlane_pn_swap[2], vin->csi_fmt.dlane_pn_swap[3]},
			.dlane_en		= {vin->csi_fmt.lane>0,   vin->csi_fmt.lane>1,   vin->csi_fmt.lane>2, 	vin->csi_fmt.lane>3},
			.clane_nb		= 1,
			.clane_map		= {vin->csi_fmt.clane_swap, 	5},
			.clane_pn_swap	= {vin->csi_fmt.clane_pn_swap,	0},
	};
	csi2rx_cfg_t csi2rx_cfg = {
				.lane_nb	= vin->csi_fmt.lane,//lane count
				.dlane_map	= {1,2,3,4},
				.dt 		= vin->csi_fmt.dt,
				.hsize		= vin->csi_fmt.w,
				.vsize		= vin->csi_fmt.h,
	};

	switch (vin->format.format) {
	case SRC_COLORBAR_VIN_ISP:
		/*vin */
		val_vin_rd_pix_total_reg = 0x000003BF;
		val_vin_rd_vblank_reg = 0x0000002D;
		val_vin_rd_vend_reg = 0x00000464;
		val_vin_rd_hblank_reg = 0x00000117;
		val_vin_rd_hend_reg = 0x00000897;
		val_vin_rw_ctrl_reg = 0x00010300;
		val_vin_src_channel_reg = 0x00000088;
		val_vin_axi_ctrl_reg = 0x00000004;

		reg_write(vin->sysctrl_base, SYSCTRL_VIN_RD_PIX_TOTAL, val_vin_rd_pix_total_reg);
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_RD_VBLANK, val_vin_rd_vblank_reg);
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_RD_VEND, val_vin_rd_vend_reg);
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_RD_HBLANK, val_vin_rd_hblank_reg);
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_RD_HEND, val_vin_rd_hend_reg);
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL, val_vin_rw_ctrl_reg);
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_SRC_CHAN_SEL, val_vin_src_channel_reg);
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_AXI_CTRL, val_vin_axi_ctrl_reg);

		if(vin->isp0||vin->isp1)
			isp_ddr_config(vin);
		break;

	case SRC_DVP_SENSOR_VIN:
		dvp_io_pad_func_shared_config(vin);
		/*
		 * NOTE:
		 * 0x38: [13:12]
		 * 00: pix_data[7:0]
		 * 01: pix_data[9:2]
		 */
	    val_vin_rw_ctrl_reg = 0x00010301;
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL, val_vin_rw_ctrl_reg);

		set_vin_wr_pix_total(vin);
	break;

	case SRC_DVP_SENSOR_VIN_OV5640:
		dvp_io_pad_func_shared_config(vin);
	    val_vin_rw_start_addr_reg = vin->buf.paddr;
		val_vin_rd_end_addr_reg = vin->buf.paddr+vin->frame.width*vin->frame.height*2;
		val_vin_wr_pix_reg = 0x000001df;
		val_vin_rw_ctrl_reg = 0x00001202;
		val_vin_axi_ctrl_reg = 0x00000003;
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_WR_START_ADDR, val_vin_rw_start_addr_reg);	// First frame address
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_RD_END_ADDR, val_vin_rd_end_addr_reg);	//4bytes 0x887e9000 2bytes 0x883f4800, 1byte 0x881FA400
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_WR_PIX_TOTAL, val_vin_wr_pix_reg);	// 0x000003bf        0x000001df        0x000000ef
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL, val_vin_rw_ctrl_reg);
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_AXI_CTRL,  val_vin_axi_ctrl_reg);
	break;

	case SRC_DVP_SENSOR_VIN_ISP:
		dvp_io_pad_func_shared_config(vin);
	    val_vin_rw_ctrl_reg = 0x00011300;
	    val_vin_src_channel_reg = 0x00001100;
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_RW_CTRL, val_vin_rw_ctrl_reg);
		reg_write(vin->sysctrl_base, SYSCTRL_VIN_SRC_CHAN_SEL, val_vin_src_channel_reg);
		if(vin->isp0||vin->isp1)
			isp_ddr_config(vin);
	break;

	case SRC_CSI2RX_VIN_ISP:
		isp_config(vin, 0);	
	    csi2rx_dphy_config(vin,&dphy_rx_cfg);
		csi2rx_config(vin,0, &csi2rx_cfg);
	    reset_val = reg_read(vin->rstgen_base, 0x00);
		reset_val &= ~(1<<17);
		reg_write(vin->rstgen_base, 0x00, reset_val);
	break;

	default:
		pr_err("unknown format\n");
		return -EINVAL;
	}

	return 0;
}


static void vin_free_buf(struct stf_vin_dev *vin)
{
	struct vin_buf *buf = &vin->buf;

	if (buf->vaddr) {
		buf->vaddr = NULL;
		buf->size = 0;
	}
}

static void vin_intr_clear(void __iomem * sysctrl_base)
{
	reg_write(sysctrl_base, SYSCTRL_VIN_INTP_CTRL, 0x1);
	reg_write(sysctrl_base, SYSCTRL_VIN_INTP_CTRL, 0x0);
}

static irqreturn_t vin_wr_irq_handler(int irq, void *priv)
{
	static struct vin_params params;
	static int wtimes = 0;
	struct stf_vin_dev *vin = priv;

	vin_intr_clear(vin->sysctrl_base);

	wtimes = wtimes % 2;
	if (wtimes == 0)
		params.paddr = (void *)vin->buf.paddr;
	else
		params.paddr = (void *)(vin->buf.paddr + vin->buf.size);

	params.size = vin->buf.size;

	vin_notifier_call(1, &params);

	vin->condition = true;
	vin->odd = wtimes;
	wake_up_interruptible(&vin->wq);
	wtimes++;

	return IRQ_HANDLED;
}

void vin_isp0_intr_clear(struct stf_vin_dev *vin)
{
	u32 value;
	value = reg_read(vin->isp_isp0_base, 0xA00);
	reg_write(vin->isp_isp0_base, 0xA00, value);
}

static irqreturn_t vin_isp0_irq_handler(int irq, void *priv)
{
	static struct vin_params params;
	static int wtimes = 0;
	struct stf_vin_dev *vin = priv;

	/*clear interrupt */
	vin_isp0_intr_clear(vin);

	wtimes = wtimes % 2;
	if (wtimes == 0) {
		params.paddr = (void *)vin->buf.paddr;
		reg_write(vin->isp_isp0_base, ISP_REG_Y_PLANE_START_ADDR,  vin->buf.paddr);	// Unscaled Output Image Y Plane Start Address Register
		reg_write(vin->isp_isp0_base, ISP_REG_UV_PLANE_START_ADDR, vin->buf.paddr+(vin->frame.width*vin->frame.height));	// Unscaled Output Image UV Plane Start Address Register
		reg_write(vin->isp_isp0_base, ISP_REG_STRIDE, vin->frame.width);	// Unscaled Output Image Stride Register
	} else {
		params.paddr = (void *)vin->buf.paddr;
		reg_write(vin->isp_isp0_base, ISP_REG_Y_PLANE_START_ADDR,  vin->buf.paddr+vin->buf.size);	// Unscaled Output Image Y Plane Start Address Register
		reg_write(vin->isp_isp0_base, ISP_REG_UV_PLANE_START_ADDR, vin->buf.paddr+vin->buf.size+(vin->frame.width*vin->frame.height));	// Unscaled Output Image UV Plane Start Address Register
		reg_write(vin->isp_isp0_base, ISP_REG_STRIDE, vin->frame.width);	// Unscaled Output Image Stride Register
	}

	params.size = vin->buf.size;

	vin->condition = true;
	vin->odd = wtimes;
	wake_up_interruptible(&vin->wq);

	vin_notifier_call(1, &params);

	wtimes++;
	return IRQ_HANDLED;
}

static void vin_irq_disable(struct stf_vin_dev *vin)
{
	unsigned int mask_value = 0, value = 0;

	/* mask and clear vin interrupt */
	mask_value = (0x1 << 4) | (0x1 << 20);

	value = 0x1 | (0x1 << 16) | mask_value;
	reg_write(vin->sysctrl_base, SYSCTRL_VIN_INTP_CTRL, value);

	value = mask_value;
	reg_write(vin->sysctrl_base, SYSCTRL_VIN_INTP_CTRL, value);
}

static void vin_irq_enable(struct stf_vin_dev *vin)
{
	unsigned int value = 0;

	value = ~((0x1 << 4) | (0x1 << 20));

	reg_write(vin->sysctrl_base, SYSCTRL_VIN_INTP_CTRL, value);
}

static int vin_sys_init(struct stf_vin_dev *vin)
{
	u32 val;

	val = ioread32(vin->vin_top_clkgen_base + 0x124)>>24;
	val &= 0x1;
	if (val != 0) {
        val = ioread32(vin->vin_top_clkgen_base + 0x124)>>24;
	    val &= ~(0x1<<24); 
	    val |= (0x0&0x1)<<24; 
		iowrite32(val, vin->vin_top_clkgen_base + 0x124);
    } else {
        printk("nne bus clk src is already clk_cpu_axi\n");
    }
	//enable clk
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_VIN_SRC_CTRL);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_ISP0_AXI_CTRL);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_ISP0NOC_AXI_CTRL);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_ISPSLV_AXI_CTRL);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_ISP1_AXI_CTRL);
	reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_ISP1NOC_AXI_CTRL);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_VIN_AXI);
    reg_set_highest_bit(vin->vin_top_clkgen_base,CLK_VINNOC_AXI);

    vin_rstgen_assert_reset(vin);

	reg_set_highest_bit(vin->clkgen_base,CLK_ISP0_CTRL);
	reg_set_highest_bit(vin->clkgen_base,CLK_ISP0_2X_CTRL);
	reg_set_highest_bit(vin->clkgen_base,CLK_ISP0_MIPI_CTRL);
	
	reg_set_highest_bit(vin->clkgen_base,CLK_ISP1_CTRL);
	reg_set_highest_bit(vin->clkgen_base,CLK_ISP1_2X_CTRL);
	reg_set_highest_bit(vin->clkgen_base,CLK_ISP1_MIPI_CTRL);
	reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX0_SYS0_CTRL);
	reg_set_highest_bit(vin->clkgen_base,CLK_MIPI_RX1_SYS1_CTRL);

    isp_rstgen_assert_reset(vin);

    // hold vin resets for sub modules before csi2rx controller get configed
    reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 0xffffffff);
    mdelay(10);

    // clear reset for all vin submodules except dphy-rx (follow lunhai's advice)
    reg_write(vin->rstgen_base, SOFTWARE_RESET_ASSERT0, 1<<17);
    return 0;
}

static int vin_parse_dt(struct device *dev, struct stf_vin_dev *vin) 
{
	int ret=0;
	struct device_node *np = dev->of_node;

	if(!np)
		return -EINVAL;

	if (of_property_read_u32(np, "format", &vin->format.format)) {
		dev_err(dev,"Missing format property in the DT.\n");
		ret = -EINVAL;
	}
	dev_info(dev,"vin->format.format = %d ,in the DT.\n",vin->format.format);
	
	if (of_property_read_u32(np, "frame-width", &vin->frame.width)) {
		dev_err(dev,"Missing frame-width property in the DT.\n");
		ret = -EINVAL;
	}

	if (of_property_read_u32(np, "frame-height", &vin->frame.height)) {
		dev_err(dev,"Missing frame.height property in the DT.\n");
		ret = -EINVAL;
	}

	vin->isp0 = of_property_read_bool(np, "isp0_enable");
	vin->isp1 = of_property_read_bool(np, "isp1_enable");

	of_property_read_u8_array(np, "csi-dlane-swaps", vin->csi_fmt.dlane_swap, 4);

	of_property_read_u8_array(np, "csi-dlane-pn-swaps", vin->csi_fmt.dlane_pn_swap, 4);

	of_property_read_u8(np, "csi-clane-swap", &vin->csi_fmt.clane_swap);

	of_property_read_u8(np, "csi-clane-pn-swap", &vin->csi_fmt.clane_pn_swap);

	of_property_read_u32(np, "csi-mipiID", &vin->csi_fmt.mipi_id);
	
	of_property_read_u32(np, "csi-width", &vin->csi_fmt.w); 
	
	of_property_read_u32(np, "csi-height", &vin->csi_fmt.h); 
	
	of_property_read_u32(np, "csi-dt", &vin->csi_fmt.dt);
	
	of_property_read_u32(np, "csi-lane", &vin->csi_fmt.lane);	

	return ret;
}

static int stf_vin_clk_init(struct stf_vin_dev *vin)
{
	int ret=0;
	switch (vin->format.format) {
	case SRC_COLORBAR_VIN_ISP:
	case SRC_DVP_SENSOR_VIN:
	case SRC_DVP_SENSOR_VIN_OV5640:
	case SRC_DVP_SENSOR_VIN_ISP:
		 ret =  vin_clk_reset(vin);
	     ret |= vin_rstgen_assert_reset(vin);
	     ret |= vin_rstgen_clkgen(vin);
	break;

	case SRC_CSI2RX_VIN_ISP:
		 ret =vin_sys_init(vin);
	break;

	default:
		pr_err("unknown format\n");
		return -EINVAL;
	}
	
	return ret;
}

static int stf_vin_probe(struct platform_device *pdev)
{
	struct stf_vin_dev *vin;
	int irq;
	dev_t devid;
	struct class *vin_cls;
	int major = 0;
	int ret = 0;
	struct device_node *node;
	struct resource res_mem;

	dev_info(&pdev->dev, "vin probe enter!\n");

	vin = devm_kzalloc(&pdev->dev, sizeof(struct stf_vin_dev), GFP_KERNEL);
	if (!vin)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev, "Could not get irq\n");
		return -ENODEV;
	}

	vin->isp0_irq = platform_get_irq(pdev, 1);
	if (vin->isp0_irq <= 0) {
		dev_err(&pdev->dev, "Could not get irq\n");
		return -ENODEV;
	}

	ret = vin_get_mem_res(pdev,vin);
	if (ret) {
			dev_err(&pdev->dev, "Could not map registers\n");
			goto out;
	}

	spin_lock_init(&vin->irqlock);

	vin->dev = &pdev->dev;
	vin->irq = irq;

	vin_irq_disable(vin);

	ret = devm_request_irq(&pdev->dev, vin->irq, vin_wr_irq_handler, 0,
			       "vin_axiwr_irq", vin);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		goto out;
	}

	ret = devm_request_irq(&pdev->dev, vin->isp0_irq, vin_isp0_irq_handler, 0,
			       "vin_isp0_irq", vin);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		goto out;
	}

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (node) {
		of_address_to_resource(node, 0, &res_mem);
		vin->buf.paddr = res_mem.start;
	} else {
		dev_err(&pdev->dev, "Could not get reserved memory\n");
		return -ENOMEM;
	}

	vin->buf.vaddr = devm_ioremap_resource(&pdev->dev, &res_mem);
	memset(vin->buf.vaddr, 0, RESERVED_MEM_SIZE);

	ret = vin_parse_dt(&pdev->dev, vin);
	if (ret)
		goto out;

	vin->condition = false;
	init_waitqueue_head(&vin->wq);

	spin_lock_init(&vin->irqlock);
	platform_set_drvdata(pdev, vin);

	/* Reset device */
	ret = stf_vin_clk_init(vin);
	if (ret) {
		dev_err(&pdev->dev, "Failed to reset device \n");
		goto out;
	}

	/* set the sysctl config */
	ret = stf_vin_config_set(vin);
	if (ret) {
		dev_err(&pdev->dev, "Failed to config device\n");
		goto out;
	}
	
	ret = vin_update_buf_size(vin);
	if (ret)
		goto out;

	vin_irq_enable(vin);
	if (vin->format.format == SRC_DVP_SENSOR_VIN)
		dvp_vin_ddr_addr_config(vin);

	usleep_range(3000, 5000);

	ret = alloc_chrdev_region(&devid, 0, 1, "vin");

	major = MAJOR(devid);
	if (major < 0) {
		dev_err(&pdev->dev, "Failed register chrdev\n");
		ret = major;
	}
	vin->major = major;

	cdev_init(&vin->vin_cdev, &vin_fops);
	cdev_add(&vin->vin_cdev, devid, 1);

	vin_cls = class_create(THIS_MODULE, "vin");
	device_create(vin_cls, NULL, MKDEV(major, 0), NULL, "vin");

	return 0;
out:
	return ret;
}

static int stf_vin_remove(struct platform_device *pdev)
{
	struct stf_vin_dev *vin = platform_get_drvdata(pdev);
	vin_free_buf(vin);

	cdev_del(&vin->vin_cdev);
	unregister_chrdev_region(MKDEV(vin->major, 0), 1);
	dev_info(&pdev->dev, "remove done\n");

	return 0;
}

static const struct of_device_id stf_vin_of_match[] = {
	{.compatible = "starfive,stf-vin"},
	{ /* end node */ },
};

MODULE_DEVICE_TABLE(of, stf_vin_of_match);

static struct platform_driver stf_vin_driver = {
	.probe = stf_vin_probe,
	.remove = stf_vin_remove,
	.driver = {
		   .name = DRV_NAME,
		   .of_match_table = of_match_ptr(stf_vin_of_match),
		   },
};

#if defined(CONFIG_VIDEO_STARFIVE_VIN_SENSOR_SC2235) || defined(CONFIG_VIDEO_STARFIVE_VIN_SENSOR_OV5640) || \
    defined(CONFIG_VIDEO_STARFIVE_VIN_SENSOR_OV4689) || defined(CONFIG_VIDEO_STARFIVE_VIN_SENSOR_IMX219)
static int __init stf_vin_init(void)
{
	return platform_driver_register(&stf_vin_driver);
}
#endif

static void __exit stf_vin_cleanup(void)
{
	platform_driver_unregister(&stf_vin_driver);
}

#if defined(CONFIG_VIDEO_STARFIVE_VIN_SENSOR_SC2235) || defined(CONFIG_VIDEO_STARFIVE_VIN_SENSOR_OV5640)
fs_initcall(stf_vin_init);
#elif defined(CONFIG_VIDEO_STARFIVE_VIN_SENSOR_OV4689) || defined(CONFIG_VIDEO_STARFIVE_VIN_SENSOR_IMX219)
subsys_initcall(stf_vin_init);
#endif

module_exit(stf_vin_cleanup); 

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("Starfive VIC video in driver");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("video");
