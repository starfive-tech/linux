// SPDX-License-Identifier: GPL-2.0-only
/*
 * DMA implementation for Hexagon
 *
 * Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 */

#include <linux/dma-map-ops.h>
#include <linux/memblock.h>
#include <asm/page.h>

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	hexagon_clean_dcache_range(paddr, paddr + size);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	hexagon_inv_dcache_range(paddr, paddr + size);
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	flush_dcache_range(paddr, paddr + size);
}

static inline bool arch_sync_dma_clean_before_fromdevice(void)
{
	return false;
}

static inline bool arch_sync_dma_cpu_needs_post_dma_flush(void)
{
	return false;
}

#include <linux/dma-sync.h>

/*
 * Our max_low_pfn should have been backed off by 16MB in mm/init.c to create
 * DMA coherent space.  Use that for the pool.
 */
static int __init hexagon_dma_init(void)
{
	return dma_init_global_coherent(PFN_PHYS(max_low_pfn),
					hexagon_coherent_pool_size);
}
core_initcall(hexagon_dma_init);
