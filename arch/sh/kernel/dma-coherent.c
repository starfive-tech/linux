// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2004 - 2007  Paul Mundt
 */
#include <linux/mm.h>
#include <linux/dma-map-ops.h>
#include <asm/cacheflush.h>
#include <asm/addrspace.h>

void arch_dma_prep_coherent(struct page *page, size_t size)
{
	__flush_purge_region(page_address(page), size);
}

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	void *addr = sh_cacheop_vaddr(phys_to_virt(paddr));

	__flush_wback_region(addr, size);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	void *addr = sh_cacheop_vaddr(phys_to_virt(paddr));

	__flush_invalidate_region(addr, size);
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	void *addr = sh_cacheop_vaddr(phys_to_virt(paddr));

	__flush_purge_region(addr, size);
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
