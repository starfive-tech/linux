// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2009-2010 PetaLogix
 * Copyright (C) 2006 Benjamin Herrenschmidt, IBM Corporation
 *
 * Provide default implementations of the DMA mapping callbacks for
 * directly mapped busses.
 */

#include <linux/device.h>
#include <linux/dma-map-ops.h>
#include <linux/gfp.h>
#include <linux/export.h>
#include <linux/bug.h>
#include <asm/cacheflush.h>

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	/* writeback plus invalidate, could be a nop on WT caches */
	flush_dcache_range(paddr, paddr + size);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	invalidate_dcache_range(paddr, paddr + size);
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
	return true;
}

#include <linux/dma-sync.h>
