// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
 */

#include <linux/gfp.h>
#include <linux/cache.h>
#include <linux/dma-map-ops.h>
#include <linux/iommu.h>
#include <xen/xen.h>

#include <asm/cacheflush.h>
#include <asm/xen/xen-ops.h>

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	dcache_clean_poc(paddr, paddr + size);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	dcache_inval_poc(paddr, paddr + size);
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	dcache_clean_inval_poc(paddr, paddr + size);
}

static inline bool arch_sync_dma_clean_before_fromdevice(void)
{
	return true;
}

static inline bool arch_sync_dma_cpu_needs_post_dma_flush(void)
{
	return true;
}

#include <linux/dma-sync.h>

void arch_dma_prep_coherent(struct page *page, size_t size)
{
	unsigned long start = (unsigned long)page_address(page);

	dcache_clean_poc(start, start + size);
}

#ifdef CONFIG_IOMMU_DMA
void arch_teardown_dma_ops(struct device *dev)
{
	dev->dma_ops = NULL;
}
#endif

void arch_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
			const struct iommu_ops *iommu, bool coherent)
{
	int cls = cache_line_size_of_cpu();

	WARN_TAINT(!coherent && cls > ARCH_DMA_MINALIGN,
		   TAINT_CPU_OUT_OF_SPEC,
		   "%s %s: ARCH_DMA_MINALIGN smaller than CTR_EL0.CWG (%d < %d)",
		   dev_driver_string(dev), dev_name(dev),
		   ARCH_DMA_MINALIGN, cls);

	dev->dma_coherent = coherent;
	if (iommu)
		iommu_setup_dma_ops(dev, dma_base, dma_base + size - 1);

	xen_setup_dma_ops(dev);
}
