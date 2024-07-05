// SPDX-License-Identifier: GPL-2.0
/*
 * e24 driver for StarFive JH7110 SoC
 *
 * Copyright (c) 2021 StarFive Technology Co., Ltd.
 * Author: Shanlong Li <shanlong.li@starfivetech.com>
 */
#include <linux/version.h>
#include <linux/atomic.h>
#include <linux/acpi.h>
#include <linux/completion.h>
#include <linux/delay.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
#include <linux/dma-mapping.h>
#else
#include <linux/dma-direct.h>
#endif
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/highmem.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/mman.h>
#include <linux/uaccess.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox_client.h>

#include <linux/bsearch.h>

#include "e24_alloc.h"
#include "starfive_e24.h"
#include "starfive_e24_hw.h"

#define EMBOX_MAX_MSG_LEN	4

static DEFINE_IDA(e24_nodeid);

struct e24_dsp_cmd {
	__u32 flags;
	__u32 in_data_size;
	__u32 out_data_size;
	union {
		__u32 in_data_addr;
		__u8 in_data[E24_DSP_CMD_INLINE_DATA_SIZE];
	};
	union {
		__u32 out_data_addr;
		__u8 out_data[E24_DSP_CMD_INLINE_DATA_SIZE];
	};
};

struct e24_ioctl_user {
	u32 flags;
	u32 in_data_size;
	u32 out_data_size;
	u64 in_data_addr;
	u64 out_data_addr;
};

struct e24_ioctl_request {
	struct e24_ioctl_user ioctl_data;
	phys_addr_t in_data_phys;
	phys_addr_t out_data_phys;
	struct e24_mapping *buffer_mapping;

	union {
		struct e24_mapping in_data_mapping;
		u8 in_data[E24_DSP_CMD_INLINE_DATA_SIZE];
	};
	union {
		struct e24_mapping out_data_mapping;
		u8 out_data[E24_DSP_CMD_INLINE_DATA_SIZE];
	};
};

static int firmware_command_timeout = 10;

static inline void e24_comm_read(volatile void __iomem *addr, void *p,
				  size_t sz)
{
	size_t sz32 = sz & ~3;
	u32 v;

	while (sz32) {
		v = __raw_readl(addr);
		memcpy(p, &v, sizeof(v));
		p += 4;
		addr += 4;
		sz32 -= 4;
	}
	sz &= 3;
	if (sz) {
		v = __raw_readl(addr);
		memcpy(p, &v, sz);
	}
}

static inline void e24_comm_write(volatile void __iomem *addr, const void *p,
				  size_t sz)
{
	size_t sz32 = sz & ~3;
	u32 v;

	while (sz32) {
		memcpy(&v, p, sizeof(v));
		__raw_writel(v, addr);
		p += 4;
		addr += 4;
		sz32 -= 4;
	}
	sz &= 3;
	if (sz) {
		v = 0;
		memcpy(&v, p, sz);
		__raw_writel(v, addr);
	}
}

static bool e24_cacheable(struct e24_device *e24_dat, unsigned long pfn,
			  unsigned long n_pages)
{
	if (e24_dat->hw_ops->cacheable) {
		return e24_dat->hw_ops->cacheable(e24_dat->hw_arg, pfn, n_pages);
	} else {
		unsigned long i;

		for (i = 0; i < n_pages; ++i)
			if (!pfn_valid(pfn + i))
				return false;
		return true;
	}
}

static int e24_compare_address_sort(const void *a, const void *b)
{
	const struct e24_address_map_entry *pa = a;
	const struct e24_address_map_entry *pb = b;

	if (pa->src_addr < pb->src_addr &&
		pb->src_addr - pa->src_addr >= pa->size)
		return -1;
	if (pa->src_addr > pb->src_addr &&
		pa->src_addr - pb->src_addr >= pb->size)
		return 1;

	return 0;
}

static int e24_compare_address_search(const void *a, const void *b)
{
	const phys_addr_t *pa = a;

	return e24_compare_address(*pa, b);
}

struct e24_address_map_entry *
e24_get_address_mapping(const struct e24_address_map *map, phys_addr_t addr)
{
	return bsearch(&addr, map->entry, map->n, sizeof(*map->entry),
			e24_compare_address_search);
}

u32 e24_translate_to_dsp(const struct e24_address_map *map, phys_addr_t addr)
{
#ifdef E24_MEM_MAP
	return addr;
#else
	struct e24_address_map_entry *entry = e24_get_address_mapping(map, addr);

	if (!entry)
		return E24_NO_TRANSLATION;
	return entry->dst_addr + addr - entry->src_addr;
#endif
}

static int e24_dma_direction(unsigned int flags)
{
	static const enum dma_data_direction e24_dma_direction[] = {
		[0] = DMA_NONE,
		[E24_FLAG_READ] = DMA_TO_DEVICE,
		[E24_FLAG_WRITE] = DMA_FROM_DEVICE,
		[E24_FLAG_READ_WRITE] = DMA_BIDIRECTIONAL,
	};
	return e24_dma_direction[flags & E24_FLAG_READ_WRITE];
}

static void e24_dma_sync_for_cpu(struct e24_device *e24_dat,
				 unsigned long virt,
				 phys_addr_t phys,
				 unsigned long size,
				 unsigned long flags)
{
	if (e24_dat->hw_ops->dma_sync_for_cpu)
		e24_dat->hw_ops->dma_sync_for_cpu(e24_dat->hw_arg,
					      (void *)virt, phys, size,
					      flags);
	else
		dma_sync_single_for_cpu(e24_dat->dev, phys_to_dma(e24_dat->dev, phys), size,
			e24_dma_direction(flags));
}

static void starfive_mbox_receive_message(struct mbox_client *client, void *message)
{
	struct e24_device *e24_dat = dev_get_drvdata(client->dev);

	complete(&e24_dat->tx_channel->tx_complete);
}

static struct mbox_chan *
starfive_mbox_request_channel(struct device *dev, const char *name)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev		= dev;
	client->rx_callback	= starfive_mbox_receive_message;
	client->tx_prepare	= NULL;
	client->tx_done		= NULL;
	client->tx_block	= true;
	client->knows_txdone	= false;
	client->tx_tout		= 3000;

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		dev_warn(dev, "Failed to request %s channel\n", name);
		return NULL;
	}

	return channel;
}

static void e24_vm_open(struct vm_area_struct *vma)
{
	struct e24_allocation *cur = vma->vm_private_data;

	atomic_inc(&cur->ref);
}

static void e24_vm_close(struct vm_area_struct *vma)
{
	e24_allocation_put(vma->vm_private_data);
}

static const struct vm_operations_struct e24_vm_ops = {
	.open = e24_vm_open,
	.close = e24_vm_close,
};

static long e24_synchronize(struct e24_device *dev)
{

	struct e24_comm *queue = dev->queue;
	struct e24_dsp_cmd __iomem *cmd = queue->comm;
	u32 flags;
	unsigned long deadline = jiffies + 10 * HZ;

	do {
		flags = __raw_readl(&cmd->flags);
		/* memory barrier */
		rmb();
		if (flags == 0x104)
			return 0;

		schedule();
	} while (time_before(jiffies, deadline));

	return -1;
}

static int e24_open(struct inode *inode, struct file *filp)
{
	struct e24_device *e24_dev = container_of(filp->private_data,
					struct e24_device, miscdev);
	int rc = 0;

	rc = pm_runtime_get_sync(e24_dev->dev);
	if (rc < 0)
		return rc;

	spin_lock_init(&e24_dev->busy_list_lock);
	filp->private_data = e24_dev;
	mdelay(1);

	return 0;
}

int e24_release(struct inode *inode, struct file *filp)
{
	struct e24_device *e24_dev = (struct e24_device *)filp->private_data;
	int rc = 0;

	rc = pm_runtime_put_sync(e24_dev->dev);
	if (rc < 0)
		return rc;

	return 0;
}

static ssize_t mbox_e24_message_write(struct file *filp,
				       const char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	struct e24_device *edev = filp->private_data;
	void *data;
	int ret;

	if (!edev->tx_channel) {
		dev_err(edev->dev, "Channel cannot do Tx\n");
		return -EINVAL;
	}

	if (count > EMBOX_MAX_MSG_LEN) {
		dev_err(edev->dev,
			"Message length %zd greater than max allowed %d\n",
			count, EMBOX_MAX_MSG_LEN);
		return -EINVAL;
	}

	edev->message = kzalloc(EMBOX_MAX_MSG_LEN, GFP_KERNEL);
	if (!edev->message)
		return -ENOMEM;

	ret = copy_from_user(edev->message, userbuf, count);
	if (ret) {
		ret = -EFAULT;
		goto out;
	}

	print_hex_dump_bytes("Client: Sending: Message: ", DUMP_PREFIX_ADDRESS,
			     edev->message, EMBOX_MAX_MSG_LEN);
	data = edev->message;
	pr_debug("%s:%d, %d\n", __func__, __LINE__, *((int *)data));
	ret = mbox_send_message(edev->tx_channel, data);

	if (ret < 0 || !edev->tx_channel->active_req)
		dev_err(edev->dev, "Failed to send message via mailbox:%d\n", ret);

out:
	kfree(edev->message);
	edev->tx_channel->active_req =  NULL;

	return ret < 0 ? ret : count;
}

static long _e24_copy_user_phys(struct e24_device *edev,
				unsigned long vaddr, unsigned long size,
				phys_addr_t paddr, unsigned long flags,
				bool to_phys)
{
	void __iomem *p = ioremap(paddr, size);
	unsigned long rc;

	if (!p) {
		dev_err(edev->dev,
			"couldn't ioremap %pap x 0x%08x\n",
			&paddr, (u32)size);
		return -EINVAL;
	}
	if (to_phys)
		rc = raw_copy_from_user(__io_virt(p),
					(void __user *)vaddr, size);
	else
		rc = copy_to_user((void __user *)vaddr,
					__io_virt(p), size);
	iounmap(p);
	if (rc)
		return -EFAULT;
	return 0;
}

static long e24_copy_user_to_phys(struct e24_device *edev,
				unsigned long vaddr, unsigned long size,
				phys_addr_t paddr, unsigned long flags)
{
	return _e24_copy_user_phys(edev, vaddr, size, paddr, flags, true);
}

static long e24_copy_user_from_phys(struct e24_device *edev,
				unsigned long vaddr, unsigned long size,
				phys_addr_t paddr, unsigned long flags)
{
	return _e24_copy_user_phys(edev, vaddr, size, paddr, flags, false);
}

static long e24_copy_virt_to_phys(struct e24_device *edev,
				unsigned long flags,
				unsigned long vaddr, unsigned long size,
				phys_addr_t *paddr,
				struct e24_alien_mapping *mapping)
{
	phys_addr_t phys;
	unsigned long align = clamp(vaddr & -vaddr, 16ul, PAGE_SIZE);
	unsigned long offset = vaddr & (align - 1);
	struct e24_allocation *allocation;
	long rc;

	rc = e24_allocate(edev->pool,
			  size + align, align, &allocation);
	if (rc < 0)
		return rc;

	phys = (allocation->start & -align) | offset;
	if (phys < allocation->start)
		phys += align;

	if (flags & E24_FLAG_READ) {
		if (e24_copy_user_to_phys(edev, vaddr,
					size, phys, flags)) {
			e24_allocation_put(allocation);
			return -EFAULT;
		}
	}

	*paddr = phys;
	*mapping = (struct e24_alien_mapping){
		.vaddr = vaddr,
		.size = size,
		.paddr = *paddr,
		.allocation = allocation,
		.type = ALIEN_COPY,
	};
	pr_debug("%s: copying to pa: %pap\n", __func__, paddr);

	return 0;
}

static long e24_writeback_alien_mapping(struct e24_device *edev,
					struct e24_alien_mapping *alien_mapping,
					unsigned long flags)
{
	struct page *page;
	size_t nr_pages;
	size_t i;
	long ret = 0;

	switch (alien_mapping->type) {
	case ALIEN_GUP:
		e24_dma_sync_for_cpu(edev,
				     alien_mapping->vaddr,
				     alien_mapping->paddr,
				     alien_mapping->size,
				     flags);
		pr_debug("%s: dirtying alien GUP @va = %p, pa = %pap\n",
			 __func__, (void __user *)alien_mapping->vaddr,
			 &alien_mapping->paddr);
		page = pfn_to_page(__phys_to_pfn(alien_mapping->paddr));
		nr_pages = PFN_UP(alien_mapping->vaddr + alien_mapping->size) -
			PFN_DOWN(alien_mapping->vaddr);
		for (i = 0; i < nr_pages; ++i)
			SetPageDirty(page + i);
		break;

	case ALIEN_COPY:
		pr_debug("%s: synchronizing alien copy @pa = %pap back to %p\n",
			 __func__, &alien_mapping->paddr,
			 (void __user *)alien_mapping->vaddr);
		if (e24_copy_user_from_phys(edev,
					    alien_mapping->vaddr,
					    alien_mapping->size,
					    alien_mapping->paddr,
					    flags))
			ret = -EINVAL;
		break;

	default:
		break;
	}
	return ret;
}

static bool vma_needs_cache_ops(struct vm_area_struct *vma)
{
	pgprot_t prot = vma->vm_page_prot;

	return pgprot_val(prot) != pgprot_val(pgprot_noncached(prot)) &&
		pgprot_val(prot) != pgprot_val(pgprot_writecombine(prot));
}

static void e24_alien_mapping_destroy(struct e24_alien_mapping *alien_mapping)
{
	switch (alien_mapping->type) {
	case ALIEN_COPY:
		e24_allocation_put(alien_mapping->allocation);
		break;
	default:
		break;
	}
}

static long __e24_unshare_block(struct file *filp, struct e24_mapping *mapping,
				unsigned long flags)
{
	long ret = 0;
	struct e24_device *edev = filp->private_data;

	switch (mapping->type & ~E24_MAPPING_KERNEL) {
	case E24_MAPPING_NATIVE:
		if (flags & E24_FLAG_WRITE) {
			e24_dma_sync_for_cpu(edev,
				     mapping->native.vaddr,
				     mapping->native.m_allocation->start,
				     mapping->native.m_allocation->size,
				     flags);
		}
		e24_allocation_put(mapping->native.m_allocation);
		break;

	case E24_MAPPING_ALIEN:
		if (flags & E24_FLAG_WRITE) {
			ret = e24_writeback_alien_mapping(edev,
							  &mapping->alien_mapping,
							  flags);
		}
		e24_alien_mapping_destroy(&mapping->alien_mapping);
		break;

	case E24_MAPPING_KERNEL:
		break;

	default:
		break;
	}

	mapping->type = E24_MAPPING_NONE;

	return ret;
}

static long __e24_share_block(struct file *filp,
			      unsigned long virt, unsigned long size,
			      unsigned long flags, phys_addr_t *paddr,
			      struct e24_mapping *mapping)
{
	phys_addr_t phys = ~0ul;
	struct e24_device *edev = filp->private_data;
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = find_vma(mm, virt);
	bool do_cache = true;
	long rc = -EINVAL;

	if (!vma) {
		pr_debug("%s: no vma for vaddr/size = 0x%08lx/0x%08lx\n",
			 __func__, virt, size);
		return -EINVAL;
	}

	if (virt + size < virt || vma->vm_start > virt)
		return -EINVAL;

	if (vma && (vma->vm_file == filp)) {
		struct e24_device *vm_file = vma->vm_file->private_data;
		struct e24_allocation *e24_user_allocation = vma->vm_private_data;

		phys = vm_file->shared_mem + (vma->vm_pgoff << PAGE_SHIFT) +
			virt - vma->vm_start;
		pr_debug("%s: E24 allocation at 0x%08lx, paddr: %pap\n",
			 __func__, virt, &phys);

		rc = 0;
		mapping->type = E24_MAPPING_NATIVE;
		mapping->native.m_allocation = e24_user_allocation;
		mapping->native.vaddr = virt;
		atomic_inc(&e24_user_allocation->ref);
		do_cache = vma_needs_cache_ops(vma);
	}
	if (rc < 0) {
		struct e24_alien_mapping *alien_mapping =
			&mapping->alien_mapping;

		/* Otherwise this is alien allocation. */
		pr_debug("%s: non-E24 allocation at 0x%08lx\n",
			 __func__, virt);

		rc = e24_copy_virt_to_phys(edev, flags,
					   virt, size, &phys,
					   alien_mapping);

		if (rc < 0) {
			pr_debug("%s: couldn't map virt to phys\n",
				 __func__);
			return -EINVAL;
		}

		phys = alien_mapping->paddr +
			virt - alien_mapping->vaddr;

		mapping->type = E24_MAPPING_ALIEN;
	}

	*paddr = phys;
	pr_debug("%s: mapping = %p, mapping->type = %d\n",
		 __func__, mapping, mapping->type);

	return 0;
}


static void e24_unmap_request_nowb(struct file *filp, struct e24_ioctl_request *rq)
{
	if (rq->ioctl_data.in_data_size > E24_DSP_CMD_INLINE_DATA_SIZE)
		__e24_unshare_block(filp, &rq->in_data_mapping, 0);
	if (rq->ioctl_data.out_data_size > E24_DSP_CMD_INLINE_DATA_SIZE)
		__e24_unshare_block(filp, &rq->out_data_mapping, 0);
}

static long e24_unmap_request(struct file *filp, struct e24_ioctl_request *rq)
{
	long ret = 0;

	if (rq->ioctl_data.in_data_size > E24_DSP_CMD_INLINE_DATA_SIZE)
		__e24_unshare_block(filp, &rq->in_data_mapping, E24_FLAG_READ);

	if (rq->ioctl_data.out_data_size > E24_DSP_CMD_INLINE_DATA_SIZE) {
		ret = __e24_unshare_block(filp, &rq->out_data_mapping,
					 E24_FLAG_WRITE);
		if (ret < 0)
			pr_debug("%s: out_data could not be unshared\n", __func__);

	} else {
		if (copy_to_user((void __user *)(unsigned long)rq->ioctl_data.out_data_addr,
				 rq->out_data,
				 rq->ioctl_data.out_data_size)) {
			pr_debug("%s: out_data could not be copied\n", __func__);
			ret = -EFAULT;
		}
	}

	return ret;
}

static bool e24_cmd_complete(struct e24_comm *share_com)
{
	struct e24_dsp_cmd __iomem *cmd = share_com->comm;
	u32 flags = __raw_readl(&cmd->flags);

	rmb();
	return (flags & (E24_CMD_FLAG_REQUEST_VALID |
			 E24_CMD_FLAG_RESPONSE_VALID)) ==
		(E24_CMD_FLAG_REQUEST_VALID |
		 E24_CMD_FLAG_RESPONSE_VALID);
}

static long e24_complete_poll(struct e24_device *edev, struct e24_comm *comm,
				  bool (*cmd_complete)(struct e24_comm *p), struct e24_ioctl_request *rq)
{
	unsigned long deadline = jiffies + firmware_command_timeout * HZ;

	do {
		if (cmd_complete(comm)) {
			pr_debug("%s: poll complete.\n", __func__);
			return 0;
		}
		schedule();
	} while (time_before(jiffies, deadline));

	pr_debug("%s: poll complete cmd timeout.\n", __func__);

	return -EBUSY;
}

static long e24_map_request(struct file *filp, struct e24_ioctl_request *rq, struct mm_struct *mm)
{
	long ret = 0;

	mmap_read_lock(mm);
	if (rq->ioctl_data.in_data_size > E24_DSP_CMD_INLINE_DATA_SIZE) {
		ret = __e24_share_block(filp, rq->ioctl_data.in_data_addr,
					rq->ioctl_data.in_data_size,
					E24_FLAG_READ, &rq->in_data_phys,
					&rq->in_data_mapping);
		if (ret < 0) {
			pr_debug("%s: in_data could not be shared\n", __func__);
			goto share_err;
		}
	} else {
		if (copy_from_user(rq->in_data,
				   (void __user *)(unsigned long)rq->ioctl_data.in_data_addr,
				   rq->ioctl_data.in_data_size)) {
			pr_debug("%s: in_data could not be copied\n",
				 __func__);
			ret = -EFAULT;
			goto share_err;
		}
	}

	if (rq->ioctl_data.out_data_size > E24_DSP_CMD_INLINE_DATA_SIZE) {
		ret = __e24_share_block(filp, rq->ioctl_data.out_data_addr,
					rq->ioctl_data.out_data_size,
					E24_FLAG_WRITE, &rq->out_data_phys,
					&rq->out_data_mapping);
		if (ret < 0) {
			pr_debug("%s: out_data could not be shared\n",
				 __func__);
			goto share_err;
		}
	}
share_err:
	mmap_read_unlock(mm);
	if (ret < 0)
		e24_unmap_request_nowb(filp, rq);
	return ret;

}

static void e24_fill_hw_request(struct e24_dsp_cmd __iomem *cmd,
				struct e24_ioctl_request *rq,
				const struct e24_address_map *map)
{
	__raw_writel(rq->ioctl_data.in_data_size, &cmd->in_data_size);
	__raw_writel(rq->ioctl_data.out_data_size, &cmd->out_data_size);

	if (rq->ioctl_data.in_data_size > E24_DSP_CMD_INLINE_DATA_SIZE)
		__raw_writel(e24_translate_to_dsp(map, rq->in_data_phys),
				&cmd->in_data_addr);
	else
		e24_comm_write(&cmd->in_data, rq->in_data,
			       rq->ioctl_data.in_data_size);

	if (rq->ioctl_data.out_data_size > E24_DSP_CMD_INLINE_DATA_SIZE)
		__raw_writel(e24_translate_to_dsp(map, rq->out_data_phys),
				&cmd->out_data_addr);

	wmb();
	/* update flags */
	__raw_writel(rq->ioctl_data.flags, &cmd->flags);
}

static long e24_complete_hw_request(struct e24_dsp_cmd __iomem *cmd,
				    struct e24_ioctl_request *rq)
{
	u32 flags = __raw_readl(&cmd->flags);

	if (rq->ioctl_data.out_data_size <= E24_DSP_CMD_INLINE_DATA_SIZE)
		e24_comm_read(&cmd->out_data, rq->out_data,
			      rq->ioctl_data.out_data_size);

	__raw_writel(0, &cmd->flags);

	return (flags & E24_QUEUE_VALID_FLAGS) ? -ENXIO : 0;
}

static long e24_ioctl_submit_task(struct file *filp,
				  struct e24_ioctl_user __user *msg)
{
	struct e24_device *edev = filp->private_data;
	struct e24_comm *queue = edev->queue;
	struct e24_ioctl_request rq_data, *vrq = &rq_data;
	void *data = &edev->mbox_data;
	int ret = -ENOMEM;
	int irq_mode = edev->irq_mode;

	if (copy_from_user(&vrq->ioctl_data, msg, sizeof(*msg)))
		return -EFAULT;

	if (vrq->ioctl_data.flags & ~E24_QUEUE_VALID_FLAGS) {
		dev_dbg(edev->dev, "%s: invalid flags 0x%08x\n",
			__func__, vrq->ioctl_data.flags);
		return -EINVAL;
	}

	ret = e24_map_request(filp, vrq, current->mm);
	if (ret < 0)
		return ret;

	mutex_lock(&queue->lock);
	e24_fill_hw_request(queue->comm, vrq, &edev->address_map);

	if (irq_mode) {
		ret = mbox_send_message(edev->tx_channel, data);
		mbox_chan_txdone(edev->tx_channel, ret);
	} else {
		ret = e24_complete_poll(edev, queue, e24_cmd_complete, vrq);
	}

	ret = e24_complete_hw_request(queue->comm, vrq);
	mutex_unlock(&queue->lock);

	if (ret == 0)
		ret = e24_unmap_request(filp, vrq);

	return ret;
}

static long e24_ioctl_get_channel(struct file *filp,
				void __user *msg)
{
	struct e24_device *edev = filp->private_data;

	if (edev->tx_channel == NULL)
		edev->tx_channel = starfive_mbox_request_channel(edev->dev, "tx");
	if (edev->rx_channel == NULL)
		edev->rx_channel = starfive_mbox_request_channel(edev->dev, "rx");

	return 0;
}

static long e24_ioctl_free_channel(struct file *filp,
				void __user *msg)
{
	struct e24_device *edev = filp->private_data;

	if (edev->rx_channel)
		mbox_free_channel(edev->rx_channel);
	if (edev->tx_channel)
		mbox_free_channel(edev->tx_channel);

	edev->rx_channel = NULL;
	edev->tx_channel = NULL;
	return 0;
}

static void e24_allocation_queue(struct e24_device *edev,
				 struct e24_allocation *e24_pool_allocation)
{
	spin_lock(&edev->busy_list_lock);

	e24_pool_allocation->next = edev->busy_list;
	edev->busy_list = e24_pool_allocation;

	spin_unlock(&edev->busy_list_lock);
}

static struct e24_allocation *e24_allocation_dequeue(struct e24_device *edev,
						     phys_addr_t paddr, u32 size)
{
	struct e24_allocation **pcur;
	struct e24_allocation *cur;

	spin_lock(&edev->busy_list_lock);

	for (pcur = &edev->busy_list; (cur = *pcur); pcur = &((*pcur)->next)) {
		pr_debug("%s: %pap / %pap x %d\n", __func__, &paddr, &cur->start, cur->size);
		if (paddr >= cur->start && paddr + size - cur->start <= cur->size) {
			*pcur = cur->next;
			break;
		}
	}

	spin_unlock(&edev->busy_list_lock);
	return cur;
}

static int e24_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int err;
	struct e24_device *edev = filp->private_data;
	unsigned long pfn = vma->vm_pgoff + PFN_DOWN(edev->shared_mem);
	struct e24_allocation *e24_user_allocation;

	e24_user_allocation = e24_allocation_dequeue(filp->private_data,
						pfn << PAGE_SHIFT,
						vma->vm_end - vma->vm_start);
	if (e24_user_allocation) {
		pgprot_t prot = vma->vm_page_prot;

		if (!e24_cacheable(edev, pfn,
				   PFN_DOWN(vma->vm_end - vma->vm_start))) {
			prot = pgprot_writecombine(prot);
			vma->vm_page_prot = prot;
		}

		err = remap_pfn_range(vma, vma->vm_start, pfn,
				      vma->vm_end - vma->vm_start,
				      prot);

		vma->vm_private_data = e24_user_allocation;
		vma->vm_ops = &e24_vm_ops;
	} else {
		err = -EINVAL;
	}

	return err;
}

static long e24_ioctl_free(struct file *filp,
			   struct e24_ioctl_alloc __user *p)
{
	struct mm_struct *mm = current->mm;
	struct e24_ioctl_alloc user_ioctl_alloc;
	struct vm_area_struct *vma;
	unsigned long start;

	if (copy_from_user(&user_ioctl_alloc, p, sizeof(*p)))
		return -EFAULT;

	start = user_ioctl_alloc.addr;
	pr_debug("%s: virt_addr = 0x%08lx\n", __func__, start);

	mmap_read_lock(mm);
	vma = find_vma(mm, start);

	if (vma && vma->vm_file == filp &&
	    vma->vm_start <= start && start < vma->vm_end) {
		size_t size;

		start = vma->vm_start;
		size = vma->vm_end - vma->vm_start;
		mmap_read_unlock(mm);
		pr_debug("%s: 0x%lx x %zu\n", __func__, start, size);
		return vm_munmap(start, size);
	}
	pr_debug("%s: no vma/bad vma for vaddr = 0x%08lx\n", __func__, start);
	mmap_read_unlock(mm);

	return -EINVAL;
}

static long e24_ioctl_alloc(struct file *filp,
			    struct e24_ioctl_alloc __user *p)
{
	struct e24_device *edev = filp->private_data;
	struct e24_allocation *e24_pool_allocation;
	unsigned long vaddr;
	struct e24_ioctl_alloc user_ioctl_alloc;
	long err;

	if (copy_from_user(&user_ioctl_alloc, p, sizeof(*p)))
		return -EFAULT;

	pr_debug("%s: size = %d, align = %x\n", __func__,
		 user_ioctl_alloc.size, user_ioctl_alloc.align);

	err = e24_allocate(edev->pool,
			   user_ioctl_alloc.size,
			   user_ioctl_alloc.align,
			   &e24_pool_allocation);
	if (err)
		return err;

	e24_allocation_queue(edev, e24_pool_allocation);

	vaddr = vm_mmap(filp, 0, e24_pool_allocation->size,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			e24_allocation_offset(e24_pool_allocation));

	user_ioctl_alloc.addr = vaddr;

	if (copy_to_user(p, &user_ioctl_alloc, sizeof(*p))) {
		vm_munmap(vaddr, user_ioctl_alloc.size);
		return -EFAULT;
	}
	return 0;
}

static long e24_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval;

	switch (cmd) {
	case E24_IOCTL_SEND:
		retval = e24_ioctl_submit_task(filp, (struct e24_ioctl_user *)arg);
		break;
	case E24_IOCTL_GET_CHANNEL:
		retval = e24_ioctl_get_channel(filp, NULL);
		break;
	case E24_IOCTL_FREE_CHANNEL:
		retval = e24_ioctl_free_channel(filp, NULL);
		break;
	case E24_IOCTL_ALLOC:
		retval = e24_ioctl_alloc(filp, (struct e24_ioctl_alloc __user *)arg);
		break;
	case E24_IOCTL_FREE:
		retval = e24_ioctl_free(filp, (struct e24_ioctl_alloc __user *)arg);
		break;
	default:
		retval = -EINVAL;
		break;
	}
	return retval;
}

static const struct file_operations e24_fops = {
	.owner  = THIS_MODULE,
	.unlocked_ioctl = e24_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = e24_ioctl,
#endif
	.open = e24_open,
	.release = e24_release,
	.write = mbox_e24_message_write,
	.mmap = e24_mmap,
};

void mailbox_task(struct platform_device *pdev)
{
	struct e24_device *e24_dev = platform_get_drvdata(pdev);

	e24_dev->tx_channel = starfive_mbox_request_channel(e24_dev->dev, "tx");
	e24_dev->rx_channel = starfive_mbox_request_channel(e24_dev->dev, "rx");
	pr_debug("%s:%d.%#llx\n", __func__, __LINE__, (u64)e24_dev->rx_channel);
}

static long e24_init_mem_pool(struct platform_device *pdev, struct e24_device *devs)
{
	struct resource *mem;

	mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ecmd");
	if (!mem)
		return -ENODEV;

	devs->comm_phys = mem->start;
	devs->comm = devm_ioremap(&pdev->dev, mem->start, mem->end - mem->start);
	mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "espace");
	if (!mem)
		return -ENODEV;

	devs->shared_mem = mem->start;
	devs->shared_size = resource_size(mem);
	pr_debug("%s:%d.%llx,%llx\n", __func__, __LINE__, devs->comm_phys, devs->shared_mem);
	return e24_init_private_pool(&devs->pool, devs->shared_mem, devs->shared_size);
}

static int e24_init_address_map(struct device *dev,
			 struct e24_address_map *map)
{
#if IS_ENABLED(CONFIG_OF)
	struct device_node *pnode = dev->of_node;
	struct device_node *node;
	int rlen, off;
	const __be32 *ranges = of_get_property(pnode, "ranges", &rlen);
	int na, pna, ns;
	int i;

	if (!ranges) {
		dev_dbg(dev, "%s: no 'ranges' property in the device tree, no translation at that level\n",
			__func__);
		goto empty;
	}

	node = of_get_next_child(pnode, NULL);
	if (!node) {
		dev_warn(dev, "%s: no child node found in the device tree, no translation at that level\n",
			 __func__);
		goto empty;
	}

	na = of_n_addr_cells(node);
	ns = of_n_size_cells(node);
	pna = of_n_addr_cells(pnode);

	rlen /= 4;
	map->n = rlen / (na + pna + ns);
	map->entry = kmalloc_array(map->n, sizeof(*map->entry), GFP_KERNEL);
	if (!map->entry)
		return -ENOMEM;

	dev_dbg(dev,
		"%s: na = %d, pna = %d, ns = %d, rlen = %d cells, n = %d\n",
		__func__, na, pna, ns, rlen, map->n);

	for (off = 0, i = 0; off < rlen; off += na + pna + ns, ++i) {
		map->entry[i].src_addr = of_translate_address(node,
							      ranges + off);
		map->entry[i].dst_addr = of_read_number(ranges + off, na);
		map->entry[i].size = of_read_number(ranges + off + na + pna, ns);
		dev_dbg(dev,
			"  src_addr = 0x%llx, dst_addr = 0x%lx, size = 0x%lx\n",
			(unsigned long long)map->entry[i].src_addr,
			(unsigned long)map->entry[i].dst_addr,
			(unsigned long)map->entry[i].size);
	}
	sort(map->entry, map->n, sizeof(*map->entry), e24_compare_address_sort, NULL);

	of_node_put(node);
	return 0;

empty:
#endif
	map->n = 1;
	map->entry = kmalloc(sizeof(*map->entry), GFP_KERNEL);
	map->entry->src_addr = 0;
	map->entry->dst_addr = 0;
	map->entry->size = ~0u;
	return -ENOMEM;
}

typedef long e24_init_function(struct platform_device *pdev);

static inline void e24_init(struct e24_device *e24_hw)
{
	if (e24_hw->hw_ops->init)
		e24_hw->hw_ops->init(e24_hw->hw_arg);
}

static inline void e24_release_e24(struct e24_device *e24_hw)
{
	if (e24_hw->hw_ops->reset)
		e24_hw->hw_ops->release(e24_hw->hw_arg);
}

static inline void e24_halt_e24(struct e24_device *e24_hw)
{
	if (e24_hw->hw_ops->halt)
		e24_hw->hw_ops->halt(e24_hw->hw_arg);
}

static inline int e24_enable_e24(struct e24_device *e24_hw)
{
	if (e24_hw->hw_ops->enable)
		return e24_hw->hw_ops->enable(e24_hw->hw_arg);
	else
		return -EINVAL;
}

static inline void e24_reset_e24(struct e24_device *e24_hw)
{
	if (e24_hw->hw_ops->reset)
		e24_hw->hw_ops->reset(e24_hw->hw_arg);
}

static inline void e24_disable_e24(struct e24_device *e24_hw)
{
	if (e24_hw->hw_ops->disable)
		e24_hw->hw_ops->disable(e24_hw->hw_arg);
}

static inline void e24_sendirq_e24(struct e24_device *e24_hw)
{
	if (e24_hw->hw_ops->send_irq)
		e24_hw->hw_ops->send_irq(e24_hw->hw_arg);
}

irqreturn_t e24_irq_handler(int irq, struct e24_device *e24_hw)
{
	dev_dbg(e24_hw->dev, "%s\n", __func__);
	complete(&e24_hw->completion);

	return IRQ_HANDLED;
}
EXPORT_SYMBOL(e24_irq_handler);

static phys_addr_t e24_translate_to_cpu(struct e24_device *mail, Elf32_Phdr *phdr)
{
	phys_addr_t res;
	__be32 addr = cpu_to_be32((u32)phdr->p_paddr);
	struct device_node *node =
		of_get_next_child(mail->dev->of_node, NULL);

	if (!node)
		node = mail->dev->of_node;

	res = of_translate_address(node, &addr);

	if (node != mail->dev->of_node)
		of_node_put(node);
	return res;
}

static int e24_load_segment_to_sysmem(struct e24_device *e24, Elf32_Phdr *phdr)
{
	phys_addr_t pa = e24_translate_to_cpu(e24, phdr);
	struct page *page = pfn_to_page(__phys_to_pfn(pa));
	size_t page_offs = pa & ~PAGE_MASK;
	size_t offs;

	for (offs = 0; offs < phdr->p_memsz; ++page) {
		void *p = kmap(page);
		size_t sz;

		if (!p)
			return -ENOMEM;

		page_offs &= ~PAGE_MASK;
		sz = PAGE_SIZE - page_offs;

		if (offs < phdr->p_filesz) {
			size_t copy_sz = sz;

			if (phdr->p_filesz - offs < copy_sz)
				copy_sz = phdr->p_filesz - offs;

			copy_sz = ALIGN(copy_sz, 4);
			memcpy(p + page_offs,
			       (void *)e24->firmware->data +
			       phdr->p_offset + offs,
			       copy_sz);
			page_offs += copy_sz;
			offs += copy_sz;
			sz -= copy_sz;
		}

		if (offs < phdr->p_memsz && sz) {
			if (phdr->p_memsz - offs < sz)
				sz = phdr->p_memsz - offs;

			sz = ALIGN(sz, 4);
			memset(p + page_offs, 0, sz);
			page_offs += sz;
			offs += sz;
		}
		kunmap(page);
	}
	dma_sync_single_for_device(e24->dev, pa, phdr->p_memsz, DMA_TO_DEVICE);
	return 0;
}

static int e24_load_segment_to_iomem(struct e24_device *e24, Elf32_Phdr *phdr)
{
	phys_addr_t pa = e24_translate_to_cpu(e24, phdr);
	void __iomem *p = ioremap(pa, phdr->p_memsz);

	if (!p) {
		dev_err(e24->dev, "couldn't ioremap %pap x 0x%08x\n",
			&pa, (u32)phdr->p_memsz);
		return -EINVAL;
	}
	if (e24->hw_ops->memcpy_tohw)
		e24->hw_ops->memcpy_tohw(p, (void *)e24->firmware->data +
					 phdr->p_offset, phdr->p_filesz);
	else
		memcpy_toio(p, (void *)e24->firmware->data + phdr->p_offset,
			    ALIGN(phdr->p_filesz, 4));

	if (e24->hw_ops->memset_hw)
		e24->hw_ops->memset_hw(p + phdr->p_filesz, 0,
				       phdr->p_memsz - phdr->p_filesz);
	else
		memset_io(p + ALIGN(phdr->p_filesz, 4), 0,
			ALIGN(phdr->p_memsz - ALIGN(phdr->p_filesz, 4), 4));

	iounmap(p);
	return 0;
}

static int e24_load_firmware(struct e24_device *e24_dev)
{
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)e24_dev->firmware->data;
	u32 *dai = (u32 *)e24_dev->firmware->data;
	int i;

	pr_debug("elf size:%ld,%x,%x\n", e24_dev->firmware->size, dai[0], dai[1]);
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		dev_err(e24_dev->dev, "bad firmware ELF magic\n");
		return -EINVAL;
	}

	if (ehdr->e_type != ET_EXEC) {
		dev_err(e24_dev->dev, "bad firmware ELF type\n");
		return -EINVAL;
	}

	if (ehdr->e_machine != EM_RISCV) {
		dev_err(e24_dev->dev, "bad firmware ELF machine\n");
		return -EINVAL;
	}

	if (ehdr->e_phoff >= e24_dev->firmware->size ||
	    ehdr->e_phoff +
	    ehdr->e_phentsize * ehdr->e_phnum > e24_dev->firmware->size) {
		dev_err(e24_dev->dev, "bad firmware ELF PHDR information\n");
		return -EINVAL;
	}

	for (i = ehdr->e_phnum; i >= 0 ; i--) {
		Elf32_Phdr *phdr = (void *)e24_dev->firmware->data +
			ehdr->e_phoff + i * ehdr->e_phentsize;
		phys_addr_t pa;
		int rc;

		/* Only load non-empty loadable segments, R/W/X */
		if (!(phdr->p_type == PT_LOAD &&
		      (phdr->p_flags & (PF_X | PF_R | PF_W)) &&
		      phdr->p_memsz > 0))
			continue;

		if (phdr->p_offset >= e24_dev->firmware->size ||
		    phdr->p_offset + phdr->p_filesz > e24_dev->firmware->size) {
			dev_err(e24_dev->dev, "bad firmware ELF program header entry %d\n", i);
			return -EINVAL;
		}

		pa = e24_translate_to_cpu(e24_dev, phdr);
		if (pa == (phys_addr_t)OF_BAD_ADDR) {
			dev_err(e24_dev->dev,
				"device address 0x%08x could not be mapped to host physical address",
				(u32)phdr->p_paddr);
			return -EINVAL;
		}
		dev_dbg(e24_dev->dev, "loading segment %d (device 0x%08x) to physical %pap\n",
			i, (u32)phdr->p_paddr, &pa);

		if (pfn_valid(__phys_to_pfn(pa)))
			rc = e24_load_segment_to_sysmem(e24_dev, phdr);
		else
			rc = e24_load_segment_to_iomem(e24_dev, phdr);

		if (rc < 0)
			return rc;
	}
	return 0;
}

static int e24_boot_firmware(struct device *dev)
{
	int ret;
	struct e24_device *e24_dev = dev_get_drvdata(dev);

	if (e24_dev->firmware_name) {
		ret = request_firmware(&e24_dev->firmware, e24_dev->firmware_name, e24_dev->dev);

		if (ret < 0)
			return ret;

		ret = e24_load_firmware(e24_dev);
		if (ret < 0) {
			release_firmware(e24_dev->firmware);
			return ret;
		}

	}

	release_firmware(e24_dev->firmware);
	ret = e24_enable_e24(e24_dev);
	if (ret < 0)
		return ret;

	e24_reset_e24(e24_dev);
	e24_release_e24(e24_dev);
	e24_synchronize(e24_dev);

	return ret;
}

int e24_runtime_suspend(struct device *dev)
{
	struct e24_device *e24_dev = dev_get_drvdata(dev);

	e24_halt_e24(e24_dev);
	e24_disable_e24(e24_dev);

	return 0;
}

long e24_init_v0(struct platform_device *pdev)
{
	long ret = -EINVAL;
	int nodeid, i;
	struct e24_hw_arg *hw_arg;
	struct e24_device *e24_dev;
	char nodename[sizeof("eboot") + 2 * sizeof(int)];

	e24_dev = devm_kzalloc(&pdev->dev, sizeof(*e24_dev), GFP_KERNEL);
	if (e24_dev == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	hw_arg = devm_kzalloc(&pdev->dev, sizeof(*hw_arg), GFP_KERNEL);
	if (hw_arg == NULL)
		return ret;

	platform_set_drvdata(pdev, e24_dev);
	e24_dev->dev = &pdev->dev;
	e24_dev->hw_arg = hw_arg;
	e24_dev->hw_ops = e24_get_hw_ops();
	e24_dev->nodeid = -1;
	hw_arg->e24 = e24_dev;

	ret = e24_init_mem_pool(pdev, e24_dev);
	if (ret < 0)
		goto err;

	ret = e24_init_address_map(e24_dev->dev, &e24_dev->address_map);
	if (ret < 0)
		goto err_free_pool;

	e24_dev->n_queues = 1;
	e24_dev->queue = devm_kmalloc(&pdev->dev,
				  e24_dev->n_queues * sizeof(*e24_dev->queue),
				  GFP_KERNEL);
	if (e24_dev->queue == NULL)
		goto err_free_map;

	for (i = 0; i < e24_dev->n_queues; i++) {
		mutex_init(&e24_dev->queue[i].lock);
		e24_dev->queue[i].comm = e24_dev->comm + E24_CMD_STRIDE * i;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "irq-mode",
		&hw_arg->irq_mode);
	e24_dev->irq_mode = hw_arg->irq_mode;
	if (hw_arg->irq_mode == 0)
		dev_info(&pdev->dev, "using polling mode on the device side\n");

	e24_dev->mbox_data = e24_translate_to_dsp(&e24_dev->address_map, e24_dev->comm_phys);
	ret = device_property_read_string(e24_dev->dev, "firmware-name",
					  &e24_dev->firmware_name);
	if (ret == -EINVAL || ret == -ENODATA) {
		dev_dbg(e24_dev->dev,
			"no firmware-name property, not loading firmware");
	} else if (ret < 0) {
		dev_err(e24_dev->dev, "invalid firmware name (%ld)", ret);
		goto err_free_map;
	}

	e24_init(e24_dev);
	pm_runtime_set_active(e24_dev->dev);
	pm_runtime_enable(e24_dev->dev);
	if (!pm_runtime_enabled(e24_dev->dev)) {
		ret = e24_boot_firmware(e24_dev->dev);
		if (ret)
			goto err_pm_disable;
	}
	nodeid = ida_simple_get(&e24_nodeid, 0, 0, GFP_KERNEL);
	if (nodeid < 0) {
		ret = nodeid;
		goto err_pm_disable;
	}

	e24_dev->nodeid = nodeid;
	sprintf(nodename, "eboot%u", nodeid);

	e24_dev->miscdev = (struct miscdevice){
		.minor = MISC_DYNAMIC_MINOR,
		.name = devm_kstrdup(&pdev->dev, nodename, GFP_KERNEL),
		.nodename = devm_kstrdup(&pdev->dev, nodename, GFP_KERNEL),
		.fops = &e24_fops,
	};

	ret = misc_register(&e24_dev->miscdev);
	if (ret < 0)
		goto err_free_id;

	return PTR_ERR(e24_dev);
err_free_id:
	ida_simple_remove(&e24_nodeid, nodeid);

err_pm_disable:
	pm_runtime_disable(e24_dev->dev);
err_free_map:
	kfree(e24_dev->address_map.entry);
err_free_pool:
	e24_free_pool(e24_dev->pool);
err:
	dev_err(&pdev->dev, "%s: ret = %ld\n", __func__, ret);
	return ret;
}

static const struct of_device_id e24_of_match[] = {
	{
		.compatible = "starfive,e24",
		.data = e24_init_v0,
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, e24_of_match);

int e24_deinit(struct platform_device *pdev)
{
	struct e24_device *e24_dev = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(e24_dev->dev))
		e24_runtime_suspend(e24_dev->dev);

	misc_deregister(&e24_dev->miscdev);
	e24_free_pool(e24_dev->pool);
	kfree(e24_dev->address_map.entry);
	ida_simple_remove(&e24_nodeid, e24_dev->nodeid);

	if (e24_dev->rx_channel)
		mbox_free_channel(e24_dev->rx_channel);
	if (e24_dev->tx_channel)
		mbox_free_channel(e24_dev->tx_channel);
	return 0;
}

static int e24_probe(struct platform_device *pdev)
{
	long ret = -EINVAL;
	const struct of_device_id *match;
	e24_init_function *init;

	match = of_match_device(e24_of_match, &pdev->dev);
	init = match->data;
	ret = init(pdev);

	return IS_ERR_VALUE(ret) ? ret : 0;
}

static int e24_remove(struct platform_device *pdev)
{
	return e24_deinit(pdev);
}

static const struct dev_pm_ops e24_runtime_pm_ops = {
	SET_RUNTIME_PM_OPS(e24_runtime_suspend,
		   e24_boot_firmware, NULL)
};

static struct platform_driver e24_driver = {
	.probe = e24_probe,
	.remove = e24_remove,
	.driver = {
		.name = "e24_boot",
		.of_match_table = of_match_ptr(e24_of_match),
		.pm = &e24_runtime_pm_ops,
	},
};

module_platform_driver(e24_driver);

MODULE_DESCRIPTION("StarFive e24 driver");
MODULE_AUTHOR("Shanlong Li <shanlong.li@starfivetech.com>");
MODULE_LICENSE("GPL");
