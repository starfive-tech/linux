// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Based on linux/arch/arm/mm/dma-mapping.c
 *
 *  Copyright (C) 2000-2004 Russell King
 */

#include <linux/dma-map-ops.h>
#include <asm/cachetype.h>
#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include <asm/cp15.h>

#include "dma.h"

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	dmac_clean_range(__va(paddr), __va(paddr + size));
	outer_clean_range(paddr, paddr + size);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	dmac_inv_range(__va(paddr), __va(paddr + size));
	outer_inv_range(paddr, paddr + size);
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	dmac_flush_range(__va(paddr), __va(paddr + size));
	outer_flush_range(paddr, paddr + size);
}

static inline bool arch_sync_dma_clean_before_fromdevice(void)
{
	return false;
}

static inline bool arch_sync_dma_cpu_needs_post_dma_flush(void)
{
	return true;
}

#include <linux/dma-sync.h>

void arch_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
			const struct iommu_ops *iommu, bool coherent)
{
	if (IS_ENABLED(CONFIG_CPU_V7M)) {
		/*
		 * Cache support for v7m is optional, so can be treated as
		 * coherent if no cache has been detected. Note that it is not
		 * enough to check if MPU is in use or not since in absense of
		 * MPU system memory map is used.
		 */
		dev->dma_coherent = cacheid ? coherent : true;
	} else {
		/*
		 * Assume coherent DMA in case MMU/MPU has not been set up.
		 */
		dev->dma_coherent = (get_cr() & CR_M) ? coherent : true;
	}
}
