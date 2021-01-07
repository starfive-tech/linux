/*
 * Copyright 2020 StarFive, Inc <samin.guo@starfivetech.com>
 *
 * DW AXI dma driver for StarFive SoC VIC7100.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <asm/uaccess.h>
#include <linux/dmaengine.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <soc/starfive/vic7100.h>

#define DRIVER_NAME			"dwaxidma"
#define AXIDMA_IOC_MAGIC		'A'
#define AXIDMA_IOCGETCHN		_IO(AXIDMA_IOC_MAGIC, 0)
#define AXIDMA_IOCCFGANDSTART		_IO(AXIDMA_IOC_MAGIC, 1)
#define AXIDMA_IOCGETSTATUS		_IO(AXIDMA_IOC_MAGIC, 2)
#define AXIDMA_IOCRELEASECHN		_IO(AXIDMA_IOC_MAGIC, 3)

#define AXI_DMA_MAX_CHANS		20

#define DMA_CHN_UNUSED			0
#define DMA_CHN_USED			1
#define DMA_STATUS_UNFINISHED		0
#define DMA_STATUS_FINISHED		1

/* for DEBUG*/
//#define DW_DMA_CHECK_RESULTS
//#define DW_DMA_PRINT_MEM
//#define DW_DMA_FLUSH_DESC

struct axidma_chncfg {
	unsigned long src_addr;	/*dma addr*/
	unsigned long dst_addr;	/*dma addr*/
	unsigned long virt_src;	/*mmap src addr*/
	unsigned long virt_dst;	/*mmap dst addr*/
	unsigned long phys;	/*desc phys addr*/
	unsigned int len;	/*transport lenth*/
	int mem_fd;		/*fd*/
	unsigned char chn_num;	/*dma channels number*/
	unsigned char status;	/*dma transport status*/
};

struct axidma_chns {
	struct dma_chan *dma_chan;
	unsigned char used;
	unsigned char status;
	unsigned char reserve[2];
};

struct axidma_chns channels[AXI_DMA_MAX_CHANS];
#ifdef DW_DMA_PRINT_MEM
void print_in_line_u64(u8 *p_name, u64 *p_buf, u32 len)
{
	u32 i, j;
	u32 line;
	u32* ptmp;
	u32 len_tmp;
	u32 rest = len / 4;

	printk("%s: 0x%#llx, 0x%x\n",
		p_name, dw_virt_to_phys((void *)p_buf), len);

	if(len >= 0x1000)
		len_tmp = 0x1000 / 32;	//print 128 size of memory.
	else
		len_tmp = len / 8;	//print real 100% size of memory.

	rest = len / 4;			//one line print 8 u32

	for (i = 0; i < len_tmp; i += 4, rest -= line) {
		if (!(i % 4))
			printk(KERN_CONT KERN_INFO" %#llx: ",
				dw_virt_to_phys((void *)(p_buf + i)));

		ptmp = (u32*)(p_buf + i);
		line = (rest > 8) ? 8 : rest;

		for (j = 0; j < line; j++)
			printk(KERN_CONT KERN_INFO "%08x ", *(ptmp + j));

		printk(KERN_CONT KERN_INFO"\n");
	}
}
#endif

static int axidma_open(struct inode *inode, struct file *file)
{
	/*Open: do nothing*/
	return 0;
}

static int axidma_release(struct inode *inode, struct file *file)
{
	/* Release: do nothing */
	return 0;
}

static ssize_t axidma_write(struct file *file, const char __user *data,
			size_t len, loff_t *ppos)
{
	/* Write: do nothing */
	return 0;
}

static void dma_complete_func(void *status)
{
	*(char *)status = DMA_STATUS_FINISHED;
}

static long axidma_unlocked_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	int i, ret;
	dma_cap_mask_t mask;
	dma_cookie_t cookie;
	struct dma_device *dma_dev;
	struct axidma_chncfg chncfg;
	struct dma_async_tx_descriptor *tx;

#ifdef DW_DMA_FLUSH_DESC
	void *des_chncfg = &chncfg;
	chncfg.phys = dw_virt_to_phys(des_chncfg);
#endif
	memset(&chncfg, 0, sizeof(struct axidma_chncfg));

	switch(cmd) {
	case AXIDMA_IOCGETCHN:
		for(i = 0; i < AXI_DMA_MAX_CHANS; i++) {
			if(DMA_CHN_UNUSED == channels[i].used)
				break;
		}
		if(AXI_DMA_MAX_CHANS == i) {
			printk("Get dma chn failed, because no idle channel\n");
			goto error;
		} else {
			channels[i].used = DMA_CHN_USED;
			channels[i].status = DMA_STATUS_UNFINISHED;
			chncfg.status = DMA_STATUS_UNFINISHED;
			chncfg.chn_num = i;
		}
		dma_cap_zero(mask);
		dma_cap_set(DMA_MEMCPY, mask);
		channels[i].dma_chan = dma_request_channel(mask, NULL, NULL);
		if(!channels[i].dma_chan) {
			printk("dma request channel failed\n");
			channels[i].used = DMA_CHN_UNUSED;
			goto error;
		}
		ret = copy_to_user((void __user *)arg, &chncfg,
				sizeof(struct axidma_chncfg));
		if(ret) {
			printk("Copy to user failed\n");
			goto error;
		}
		break;
	case AXIDMA_IOCCFGANDSTART:
#ifdef DW_DMA_CHECK_RESULTS
		void *src,*dst;
#endif
		ret = copy_from_user(&chncfg, (void __user *)arg,
				     sizeof(struct axidma_chncfg));
		if(ret) {
			printk("Copy from user failed\n");
			goto error;
		}

		if((chncfg.chn_num >= AXI_DMA_MAX_CHANS) ||
		   (!channels[chncfg.chn_num].dma_chan)) {
			printk("chn_num[%d] is invalid\n", chncfg.chn_num);
			goto error;
		}
		dma_dev = channels[chncfg.chn_num].dma_chan->device;
#ifdef DW_DMA_FLUSH_DESC
		starfive_flush_dcache(chncfg.phys,sizeof(chncfg));
#endif
#ifdef DW_DMA_CHECK_RESULTS
		src = dw_phys_to_virt(chncfg.src_addr);
		dst = dw_phys_to_virt(chncfg.dst_addr);
#endif
		starfive_flush_dcache(chncfg.src_addr, chncfg.len);

		tx = dma_dev->device_prep_dma_memcpy(
			channels[chncfg.chn_num].dma_chan,
			chncfg.dst_addr, chncfg.src_addr, chncfg.len,
			DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
		if(!tx){
			printk("Failed to prepare DMA memcpy\n");
			goto error;
		}
		channels[chncfg.chn_num].status = DMA_STATUS_UNFINISHED;
		tx->callback_param = &channels[chncfg.chn_num].status;
		tx->callback = dma_complete_func;
		cookie = tx->tx_submit(tx);
		if(dma_submit_error(cookie)) {
			printk("Failed to dma tx_submit\n");
			goto error;
		}
		dma_async_issue_pending(channels[chncfg.chn_num].dma_chan);
		/*flush dcache*/
		starfive_flush_dcache(chncfg.dst_addr, chncfg.len);
#ifdef DW_DMA_PRINT_MEM
		print_in_line_u64((u8 *)"src", (u64 *)src, chncfg.len);
		print_in_line_u64((u8 *)"dst", (u64 *)dst, chncfg.len);
#endif
#ifdef DW_DMA_CHECK_RESULTS
		if(memcmp(src, dst, chncfg.len))
			printk("check data faild.\n");
		else
			printk("check data ok.\n");
#endif
		break;

	case AXIDMA_IOCGETSTATUS:
		ret = copy_from_user(&chncfg, (void __user *)arg,
			sizeof(struct axidma_chncfg));
		if(ret) {
			printk("Copy from user failed\n");
			goto error;
		}

		if(chncfg.chn_num >= AXI_DMA_MAX_CHANS) {
			printk("chn_num[%d] is invalid\n", chncfg.chn_num);
			goto error;
		}

		chncfg.status = channels[chncfg.chn_num].status;

		ret = copy_to_user((void __user *)arg, &chncfg,
				   sizeof(struct axidma_chncfg));
		if(ret) {
			printk("Copy to user failed\n");
			goto error;
		}
		break;

	case AXIDMA_IOCRELEASECHN:
		ret = copy_from_user(&chncfg, (void __user *)arg,
				     sizeof(struct axidma_chncfg));
		if(ret) {
			printk("Copy from user failed\n");
			goto error;
		}

		if((chncfg.chn_num >= AXI_DMA_MAX_CHANS) ||
		   (!channels[chncfg.chn_num].dma_chan)) {
			printk("chn_num[%d] is invalid\n", chncfg.chn_num);
			goto error;
		}

		dma_release_channel(channels[chncfg.chn_num].dma_chan);
		channels[chncfg.chn_num].used = DMA_CHN_UNUSED;
		channels[chncfg.chn_num].status = DMA_STATUS_UNFINISHED;
		break;

	default:
		printk("Don't support cmd [%d]\n", cmd);
		break;
	}
	return 0;

error:
	return -EFAULT;
}

/*
 *	Kernel Interfaces
 */
static struct file_operations axidma_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= axidma_write,
	.unlocked_ioctl	= axidma_unlocked_ioctl,
	.open		= axidma_open,
	.release	= axidma_release,
};

static struct miscdevice axidma_miscdev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= DRIVER_NAME,
	.fops		= &axidma_fops,
};

static int __init axidma_init(void)
{
	int ret = misc_register(&axidma_miscdev);
	if(ret) {
		printk (KERN_ERR "cannot register miscdev (err=%d)\n", ret);
		return ret;
	}

	memset(&channels, 0, sizeof(channels));

	return 0;
}

static void __exit axidma_exit(void)
{
	misc_deregister(&axidma_miscdev);
}

module_init(axidma_init);
module_exit(axidma_exit);

MODULE_AUTHOR("samin.guo");
MODULE_DESCRIPTION("DW Axi Dmac Driver");
MODULE_LICENSE("GPL");
