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
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <video/stf-vin.h>
#include "stf_isp.h"

static DEFINE_MUTEX(vin_mutex);
static void __iomem *isp0_base;

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
	mutex_lock(&vin_mutex);
	
	struct stf_vin_dev *dev;
	dev=container_of(inode->i_cdev, struct stf_vin_dev, vin_cdev);

	file->private_data = dev;
out:
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

/*-------------------------------------------------------*/
/*
 * NOTE:
 * 	vic clk driver hasn't complete, using follow functions
 * 	to reset
 * TODO: vic clk driver
 *
 */
static int vin_clk_reset(void)
{
	void __iomem *vin_clk_gen_base
	    = ioremap(VIN_CLKGEN_BASE_ADDR, 0x1000);
	//disable clk
    reg_clear_highest_bit(vin_clk_gen_base,CLK_VIN_SRC_CTRL);
    reg_clear_highest_bit(vin_clk_gen_base,CLK_ISP0_AXI_CTRL);
    reg_clear_highest_bit(vin_clk_gen_base,CLK_ISP0NOC_AXI_CTRL);
    reg_clear_highest_bit(vin_clk_gen_base,CLK_ISPSLV_AXI_CTRL);
    reg_clear_highest_bit(vin_clk_gen_base,CLK_ISP1_AXI_CTRL);
	reg_clear_highest_bit(vin_clk_gen_base,CLK_ISP1NOC_AXI_CTRL);
    reg_clear_highest_bit(vin_clk_gen_base,CLK_VIN_AXI);
    reg_clear_highest_bit(vin_clk_gen_base,CLK_VINNOC_AXI);

	//enable clk
    reg_set_highest_bit(vin_clk_gen_base,CLK_VIN_SRC_CTRL);
    reg_set_highest_bit(vin_clk_gen_base,CLK_ISP0_AXI_CTRL);
    reg_set_highest_bit(vin_clk_gen_base,CLK_ISP0NOC_AXI_CTRL);
    reg_set_highest_bit(vin_clk_gen_base,CLK_ISPSLV_AXI_CTRL);
    reg_set_highest_bit(vin_clk_gen_base,CLK_ISP1_AXI_CTRL);
	reg_set_highest_bit(vin_clk_gen_base,CLK_ISP1NOC_AXI_CTRL);
    reg_set_highest_bit(vin_clk_gen_base,CLK_VIN_AXI);
    reg_set_highest_bit(vin_clk_gen_base,CLK_VINNOC_AXI);

	return 0;
}

static int vin_rstgen_assert_reset(void)
{
	u32 val;
	void __iomem *vin_rst_gen_base
	    = ioremap(VIN_RSTGEN_BASE_ADDR, 0x1000);
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

	val = ioread32(vin_rst_gen_base + SOFTWARE_RESET_ASSERT1);
	val |= val_reg_reset_config;
	iowrite32(val, vin_rst_gen_base + SOFTWARE_RESET_ASSERT1);

	val = ioread32(vin_rst_gen_base + SOFTWARE_RESET_ASSERT1);
	val &= ~(val_reg_reset_config);

	iowrite32(val, vin_rst_gen_base + SOFTWARE_RESET_ASSERT1);

	return 0;
}

/*
 * TODO: should replace with DTS pinctrl
 *
 */

static void dvp_io_pad_func_shared_config(void)
{
	void __iomem *vin_rst_gen_base
	    = ioremap(VIN_IOPAD_BASE_ADDR, 0x1000);
	/*
	 * pin: 49 ~ 57
	 * offset: 0x144 ~ 0x164
	 * SCFG_funcshare_pad_ctrl
	 */
	u32 val_scfg_funcshare_config = 0x800080;
	iowrite32(val_scfg_funcshare_config, vin_rst_gen_base + IOPAD_REG81);
	iowrite32(val_scfg_funcshare_config, vin_rst_gen_base + IOPAD_REG82);
	iowrite32(val_scfg_funcshare_config, vin_rst_gen_base + IOPAD_REG83);
	iowrite32(val_scfg_funcshare_config, vin_rst_gen_base + IOPAD_REG84);
	iowrite32(val_scfg_funcshare_config, vin_rst_gen_base + IOPAD_REG85);
	iowrite32(val_scfg_funcshare_config, vin_rst_gen_base + IOPAD_REG86);
	iowrite32(val_scfg_funcshare_config, vin_rst_gen_base + IOPAD_REG87);
	iowrite32(val_scfg_funcshare_config, vin_rst_gen_base + IOPAD_REG88);
	iowrite32(val_scfg_funcshare_config, vin_rst_gen_base + IOPAD_REG89);
}

static int vin_rstgen_clkgen(struct stf_vin_dev *vin)
{
	void __iomem *rstgen_base = vin->base + VIN_RSTGEN_OFFSET;
	void __iomem *clkgen_base = vin->base + VIN_CLKGEN_OFFSET;
	u32 val_vin_axi_wr_ctrl_reg = 0x02000000;

	/* rst disable */
	reg_write(rstgen_base, SOFTWARE_RESET_ASSERT0, 0xFFFFFFFF);

	/* rst enable */
	reg_write(rstgen_base, SOFTWARE_RESET_ASSERT0, 0x0);

	switch (vin->format.format) {
	case SRC_DVP_SENSOR_VIN_OV5640:
	case SRC_DVP_SENSOR_VIN:
		reg_write(clkgen_base, CLK_VIN_AXI_WR_CTRL, val_vin_axi_wr_ctrl_reg);
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
	void __iomem *sysctrl_base = vin->base + VIN_SYSCONTROLLER_OFFSET;
	u32 value;
	int cnfg_axiwr_pix_ct = 0;

	value = reg_read(sysctrl_base, SYSCTRL_REG14) & 0x3;
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
	void __iomem *sysctrl_base = vin->base + VIN_SYSCONTROLLER_OFFSET;
	int val, pix_ct;
	pix_ct = get_vin_axiwr_pix_ct(vin);
	val = (vin->frame.width / pix_ct) - 1;
	reg_write(sysctrl_base, SYSCTRL_REG12, val);
}

static int stf_vin_clk_init(struct stf_vin_dev *vin)
{
	int ret=0;
	ret = vin_clk_reset();
	ret |= vin_rstgen_assert_reset();
	ret |= vin_rstgen_clkgen(vin);
	return ret;
}

static void dvp_vin_ddr_addr_config(struct stf_vin_dev *vin)
{
	void __iomem *sysctrl_base = vin->base + VIN_SYSCONTROLLER_OFFSET;
	u32 val_vin_axi_ctrl_reg = 0x00000003;
	dev_dbg(vin->dev, "%d: addr: 0x%lx, size: 0x%lx\n", __LINE__,
		(long)vin->buf.paddr, (long)(vin->buf.paddr + vin->buf.size));
	/* set the start address of vin and enable */
	reg_write(sysctrl_base, SYSCTRL_REG10, (long)vin->buf.paddr);
	reg_write(sysctrl_base, SYSCTRL_REG11, (long)(vin->buf.paddr + vin->buf.size));
	reg_write(sysctrl_base, SYSCTRL_REG6, val_vin_axi_ctrl_reg);
}

static int stf_vin_config_set(struct stf_vin_dev *vin)
{
	void __iomem *sysctrl_base = vin->base + VIN_SYSCONTROLLER_OFFSET;
	u32 val_vin_rd_pix_total_reg ;
	u32 val_vin_rd_vblank_reg ;
	u32 val_vin_rd_vend_reg ;
	u32 val_vin_rd_hblank_reg ;
	u32 val_vin_rd_hend_reg ;
	u32 val_vin_rw_ctrl_reg ;
	u32 val_vin_src_channel_reg ;
	u32 val_vin_axi_ctrl_reg ;
	u32 val_vin_rw_start_addr_reg ;
	u32 val_vin_rd_end_addr_reg ;
	u32 val_vin_wr_pix_reg ;

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
		
		reg_write(sysctrl_base, SYSCTRL_REG13, val_vin_rd_pix_total_reg);
		reg_write(sysctrl_base, SYSCTRL_REG17, val_vin_rd_vblank_reg);
		reg_write(sysctrl_base, SYSCTRL_REG18, val_vin_rd_vend_reg);
		reg_write(sysctrl_base, SYSCTRL_REG19, val_vin_rd_hblank_reg);
		reg_write(sysctrl_base, SYSCTRL_REG20, val_vin_rd_hend_reg);
		reg_write(sysctrl_base, SYSCTRL_REG14, val_vin_rw_ctrl_reg);
		reg_write(sysctrl_base, SYSCTRL_REG15, val_vin_src_channel_reg);
		reg_write(sysctrl_base, SYSCTRL_REG6, val_vin_axi_ctrl_reg);
		isp_base_addr_config(vin);
		if(vin->isp0||vin->isp1)
			isp_ddr_config(vin);
		break;

	case SRC_DVP_SENSOR_VIN:
		dvp_io_pad_func_shared_config();
		/*
		 * NOTE:
		 * 0x38: [13:12]
		 * 00: pix_data[7:0]
		 * 01: pix_data[9:2]
		 */
	    val_vin_rw_ctrl_reg = 0x00010301;
		reg_write(sysctrl_base, SYSCTRL_REG14, val_vin_rw_ctrl_reg);

		set_vin_wr_pix_total(vin);
	break;

	case SRC_DVP_SENSOR_VIN_OV5640:
		dvp_io_pad_func_shared_config();
	    val_vin_rw_start_addr_reg = FB_FIRST_ADDR;
		val_vin_rd_end_addr_reg = FB_FIRST_ADDR+0x3f4800;
		val_vin_wr_pix_reg = 0x000001df;
		val_vin_rw_ctrl_reg = 0x00001202;
		val_vin_axi_ctrl_reg = 0x00000003;
		reg_write(sysctrl_base, SYSCTRL_REG10, val_vin_rw_start_addr_reg);	// First frame address
		reg_write(sysctrl_base, SYSCTRL_REG11, val_vin_rd_end_addr_reg);	//4bytes 0x887e9000 2bytes 0x883f4800, 1byte 0x881FA400
		reg_write(sysctrl_base, SYSCTRL_REG12, val_vin_wr_pix_reg);	//       0x000003bf        0x000001df        0x000000ef
		reg_write(sysctrl_base, SYSCTRL_REG14, val_vin_rw_ctrl_reg);
		reg_write(sysctrl_base, SYSCTRL_REG6,  val_vin_axi_ctrl_reg);
		break;

	case SRC_DVP_SENSOR_VIN_ISP:
		dvp_io_pad_func_shared_config();
	    val_vin_rw_ctrl_reg = 0x00011300;
	    val_vin_src_channel_reg = 0x00001100;
		reg_write(sysctrl_base, SYSCTRL_REG14, val_vin_rw_ctrl_reg);
		reg_write(sysctrl_base, SYSCTRL_REG15, val_vin_src_channel_reg);
		isp_base_addr_config(vin);
		if(vin->isp0||vin->isp1)
			isp_ddr_config(vin);
	break;

	case SRC_CSI2RX_VIN_ISP:
	break;

	default:
		pr_err("unknown format\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * vin_get_pixel_size:
 *
 * size = (width * height * 4)/(cnfg_axiwr_pix_ct / 2)
 */
static int vin_get_axiwr_pixel_size(struct stf_vin_dev *vin)
{
	void __iomem *sysctrl_base = vin->base + VIN_SYSCONTROLLER_OFFSET;
	u32 value;
	int cnfg_axiwr_pix_ct;

	value = reg_read(sysctrl_base, SYSCTRL_REG14) & 0x3;
	
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

static int vin_alloc_buf(struct stf_vin_dev *vin)
{
	struct vin_buf *buf = &vin->buf;
	u32 size;
	if (vin->format.format == SRC_DVP_SENSOR_VIN_OV5640)
		size = vin->frame.height*vin->frame.width*2;
	else
		size = vin_get_axiwr_pixel_size(vin);

	if (size == 0) {
		dev_err(vin->dev, "size is 0");
		return -EINVAL;
	}

	buf->vaddr = dma_alloc_coherent(vin->dev, 2 * size, &buf->paddr,
					GFP_KERNEL);
	if (!buf->vaddr) {
		dev_err(vin->dev,
			"Failed to allocate buffer of size 0x%x\n", size);
		return -ENOMEM;
	}

	buf->size = size;

	return 0;
}

static void vin_free_buf(struct stf_vin_dev *vin)
{
	struct vin_buf *buf = &vin->buf;

	if (buf->vaddr) {
		dma_free_coherent(vin->dev, buf->size, buf->vaddr, buf->paddr);
		buf->vaddr = NULL;
		buf->size = 0;
	}
}

static void vin_intr_clear(void __iomem * sysctrl_base)
{
	reg_write(sysctrl_base, SYSCTRL_REG21, 0x1);
	reg_write(sysctrl_base, SYSCTRL_REG21, 0x0);
}

static irqreturn_t vin_wr_irq_handler(int irq, void *priv)
{
	static struct vin_params params;
	static int wtimes = 0;
	struct stf_vin_dev *vin = priv;

	void __iomem *sysctrl_base = vin->base + VIN_SYSCONTROLLER_OFFSET;

	/*clear interrupt */
	vin_intr_clear(sysctrl_base);

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

void vin_isp0_intr_clear(void)
{
	u32 value;
	value = reg_read(isp0_base, 0xA00);
	reg_write(isp0_base, 0xA00, value);
}

static irqreturn_t vin_isp0_irq_handler(int irq, void *priv)
{
	static struct vin_params params;
	static int wtimes = 0;
	struct stf_vin_dev *vin = priv;

	/*clear interrupt */
	vin_isp0_intr_clear();

	wtimes = wtimes % 2;
	if (wtimes == 0) {
		params.paddr = (void *)FB_FIRST_ADDR;
		reg_write(isp0_base, ISP_REG_Y_PLANE_START_ADDR,  FB_FIRST_ADDR);	// Unscaled Output Image Y Plane Start Address Register
		reg_write(isp0_base, ISP_REG_UV_PLANE_START_ADDR, FB_FIRST_ADDR+(vin->frame.width*vin->frame.height));	// Unscaled Output Image UV Plane Start Address Register
		reg_write(isp0_base, ISP_REG_STRIDE, vin->frame.width);	// Unscaled Output Image Stride Register
	} else {
		params.paddr = (void *)FB_SECOND_ADDR;
		reg_write(isp0_base, ISP_REG_Y_PLANE_START_ADDR,  FB_SECOND_ADDR);	// Unscaled Output Image Y Plane Start Address Register
		reg_write(isp0_base, ISP_REG_UV_PLANE_START_ADDR, FB_SECOND_ADDR+(vin->frame.width*vin->frame.height));	// Unscaled Output Image UV Plane Start Address Register
		reg_write(isp0_base, ISP_REG_STRIDE, vin->frame.width);	// Unscaled Output Image Stride Register
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

	void __iomem *sysctrl_base = vin->base + VIN_SYSCONTROLLER_OFFSET;

	/* mask and clear vin interrupt */
	mask_value = (0x1 << 4) | (0x1 << 20);

	value = 0x1 | (0x1 << 0x16) | mask_value;
	reg_write(sysctrl_base, SYSCTRL_REG21, value);

	value = mask_value;
	reg_write(sysctrl_base, SYSCTRL_REG21, value);
}

static void vin_irq_enable(struct stf_vin_dev *vin)
{
	unsigned int value = 0;

	void __iomem *sysctrl_base = vin->base + VIN_SYSCONTROLLER_OFFSET;

	value = ~((0x1 << 4) | (0x1 << 20));

	reg_write(sysctrl_base, SYSCTRL_REG21, value);
}

static int stf_vin_probe(struct platform_device *pdev)
{
	struct stf_vin_dev *vin;
	struct resource *mem;
	int irq;
	dev_t devid;
	struct class *vin_cls;
	int major = 0;
	int ret = 0;
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

	isp0_base = ioremap(VIN_ISP0_BASE_ADDR, 0x30000);
	if (IS_ERR(isp0_base)) {
		dev_err(&pdev->dev, "Could not map registers\n");
		return PTR_ERR(isp0_base);
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "Could not get resource\n");
		return -ENODEV;
	}

	vin->base = devm_ioremap(&pdev->dev, mem->start,
					 resource_size(mem));
	if (IS_ERR(vin->base)) {
		dev_err(&pdev->dev, "Could not map registers\n");
		return PTR_ERR(vin->base);
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

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Could not get reserved memory\n");
		goto out;
	}

	/* default config */
	vin->format.format = SRC_DVP_SENSOR_VIN_OV5640;
	vin->frame.height = VD_HEIGHT_1080P;
	vin->frame.width = VD_WIDTH_1080P;
	vin->isp0 = false;
	vin->isp1 = false;

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

	ret = vin_alloc_buf(vin);
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

static int __init stf_vin_init(void)
{
	return platform_driver_register(&stf_vin_driver);
}

static void __exit stf_vin_cleanup(void)
{	
	platform_driver_unregister(&stf_vin_driver);
}

fs_initcall(stf_vin_init);
module_exit(stf_vin_cleanup);

MODULE_AUTHOR("StarFive Technology Co., Ltd.");
MODULE_DESCRIPTION("Starfive VIC video in driver");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("video");
