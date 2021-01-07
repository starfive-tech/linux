/*
 * Copyright 2020 StarFive, Inc <samin.guo@starfivetech.com>
 *
 * API for dma mem2mem.
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

#include <linux/acpi_iort.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/dmaengine.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <linux/slab.h>

#include <soc/starfive/vic7100.h>

static volatile int dma_finished = 0;
static DECLARE_WAIT_QUEUE_HEAD(wq);

u64 dw_virt_to_phys(void *vaddr)
{
	u64 pfn_offset = ((u64)vaddr) & 0xfff;

	return _dw_virt_to_phys((u64 *)vaddr) + pfn_offset;
}
EXPORT_SYMBOL(dw_virt_to_phys);

void *dw_phys_to_virt(u64 phys)
{
	u64 pfn_offset = phys & 0xfff;

	return (void *)(_dw_phys_to_virt(phys) + pfn_offset);
}
EXPORT_SYMBOL(dw_phys_to_virt);

static void tx_callback(void *dma_async_param)
{
	dma_finished = 1;
	wake_up_interruptible(&wq);
}

static int _dma_async_alloc_buf(struct device *dma_dev,
				void **src, void **dst, size_t size,
				dma_addr_t *src_dma, dma_addr_t *dst_dma)
{
	*src = dma_alloc_coherent(dma_dev, size, src_dma, GFP_KERNEL);
	if(!(*src)) {
		DMA_DEBUG("src alloc err.\n");
		goto _FAILED_ALLOC_SRC;
	}

	*dst = dma_alloc_coherent(dma_dev, size, dst_dma, GFP_KERNEL);
	if(!(*dst)) {
		DMA_DEBUG("dst alloc err.\n");
		goto _FAILED_ALLOC_DST;
	}

	return 0;

_FAILED_ALLOC_DST:
	dma_free_coherent(dma_dev, size, *src, *src_dma);

_FAILED_ALLOC_SRC:
	dma_free_coherent(dma_dev, size, *dst, *dst_dma);

	return -1;
}

static int _dma_async_prebuf(void *src, void *dst, size_t size)
{
	memset((u8 *)src, 0xff, size);
	memset((u8 *)dst, 0x00, size);
	return 0;
}

static int _dma_async_check_data(void *src, void *dst, size_t size)
{
	return memcmp(src, dst, size);
}

static void _dma_async_release(struct dma_chan *chan)
{
	dma_release_channel(chan);
}

static struct dma_chan *_dma_get_channel(enum dma_transaction_type tx_type)
{
	dma_cap_mask_t dma_mask;

	dma_cap_zero(dma_mask);
	dma_cap_set(tx_type, dma_mask);

	return dma_request_channel(dma_mask, NULL, NULL);
}

static struct dma_async_tx_descriptor *_dma_async_get_desc(
	struct dma_chan *chan,
	dma_addr_t src_dma, dma_addr_t dst_dma,
	size_t size)
{
	dma_finished = 0;
	return dmaengine_prep_dma_memcpy(chan, dst_dma, src_dma, size,
					 DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
}

static void _dma_async_do_start(struct dma_async_tx_descriptor *desc,
				struct dma_chan *chan)
{
	dma_cookie_t dma_cookie = dmaengine_submit(desc);
	if (dma_submit_error(dma_cookie))
		DMA_DEBUG("Failed to do DMA tx_submit\n");

	dma_async_issue_pending(chan);
	wait_event_interruptible(wq, dma_finished);
}

int dw_dma_async_do_memcpy(void *src, void *dst, size_t size)
{
	int ret;
	struct device *dma_dev;
	struct dma_chan *chan;
	dma_addr_t src_dma, dst_dma;
	struct dma_async_tx_descriptor *desc;

	const struct iommu_ops *iommu;
	u64 dma_addr = 0, dma_size = 0;

	dma_dev = kzalloc(sizeof(*dma_dev), GFP_KERNEL);
	if(!dma_dev){
		dev_err(dma_dev, "kmalloc error.\n");
		return -ENOMEM;
	}

	dma_dev->bus = NULL;
	dma_dev->coherent_dma_mask = 0xffffffff;

	iort_dma_setup(dma_dev, &dma_addr, &dma_size);
	iommu = iort_iommu_configure_id(dma_dev, NULL);
	if (PTR_ERR(iommu) == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	arch_setup_dma_ops(dma_dev, dst_dma, dma_size, iommu, true);

	if(_dma_async_alloc_buf(dma_dev, &src, &dst, size, &src_dma, &dst_dma)) {
		dev_err(dma_dev, "Err alloc.\n");
		return -ENOMEM;
	}

	DMA_DEBUG("src=%#llx, dst=%#llx\n", (u64)src, (u64)dst);
	DMA_DEBUG("dma_src=%#x dma_dst=%#x\n", (u32)src_dma, (u32)dst_dma);

	_dma_async_prebuf(src, dst, size);

	chan = _dma_get_channel(DMA_MEMCPY);
	if(!chan ){
		DMA_PRINTK("Err get chan.\n");
		return -EBUSY;
	}
	DMA_DEBUG("get chan ok.\n");

	desc = _dma_async_get_desc(chan, src_dma, dst_dma, size);
	if(!desc){
		DMA_PRINTK("Err get desc.\n");
		dma_release_channel(chan);
		return -ENOMEM;
	}
	DMA_DEBUG("get desc ok.\n");

	desc->callback = tx_callback;

	starfive_flush_dcache(src_dma, size);
	starfive_flush_dcache(dst_dma, size);

	_dma_async_do_start(desc, chan);
	_dma_async_release(chan);

	ret = _dma_async_check_data(src, dst, size);

	dma_free_coherent(dma_dev, size, src, src_dma);
	dma_free_coherent(dma_dev, size, dst, dst_dma);

	return ret;
}
EXPORT_SYMBOL(dw_dma_async_do_memcpy);

/*
* phys addr for dma.
*/
int dw_dma_memcpy_raw(dma_addr_t src_dma, dma_addr_t dst_dma, size_t size)
{
	struct dma_chan *chan;
	struct device *dma_dev;
	struct dma_async_tx_descriptor *desc;

	const struct iommu_ops *iommu;
	u64 dma_addr = 0, dma_size = 0;

	dma_dev = kzalloc(sizeof(*dma_dev), GFP_KERNEL);
	if(!dma_dev){
		DMA_PRINTK("kmalloc error.\n");
		return -ENOMEM;
	}

	dma_dev->bus = NULL;
	dma_dev->coherent_dma_mask = 0xffffffff;

	iort_dma_setup(dma_dev, &dma_addr, &dma_size);
	iommu = iort_iommu_configure_id(dma_dev, NULL);
	if (PTR_ERR(iommu) == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	arch_setup_dma_ops(dma_dev, dst_dma, dma_size, iommu, true);

	chan = _dma_get_channel(DMA_MEMCPY);
	if(!chan){
		DMA_PRINTK("Error get chan.\n");
		return -EBUSY;
	}
	DMA_DEBUG("get chan ok.\n");

	DMA_DEBUG("src_dma=%#llx, dst_dma=%#llx \n", src_dma, dst_dma);
	desc = _dma_async_get_desc(chan, src_dma, dst_dma, size);
	if(!desc){
		DMA_PRINTK("Error get desc.\n");
		dma_release_channel(chan);
		return -ENOMEM;
	}
	DMA_DEBUG("get desc ok.\n");

	desc->callback = tx_callback;

	starfive_flush_dcache(src_dma, size);
	starfive_flush_dcache(dst_dma, size);

	_dma_async_do_start(desc, chan);
	_dma_async_release(chan);

	return 0;
}
EXPORT_SYMBOL(dw_dma_memcpy_raw);

/*
*virtl addr for cpu.
*/
int dw_dma_memcpy(void *src, void *dst, size_t size)
{
	dma_addr_t src_dma, dst_dma;

	src_dma = dw_virt_to_phys(src);
	dst_dma = dw_virt_to_phys(dst);

	dw_dma_memcpy_raw(src_dma, dst_dma, size);
	return 0;
}
EXPORT_SYMBOL(dw_dma_memcpy);

int dw_dma_mem2mem_test(void)
{
	int ret;
	void *src = NULL;
	void *dst = NULL;
	size_t size = 256;

	ret = dw_dma_async_do_memcpy(src, dst, size);
	if(ret){
		DMA_PRINTK("memcpy failed.\n");
	} else {
		DMA_PRINTK("memcpy ok.\n");
	}

	return ret;
}
