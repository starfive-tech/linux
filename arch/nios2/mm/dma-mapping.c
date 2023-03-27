/*
 * Copyright (C) 2011 Tobias Klauser <tklauser@distanz.ch>
 * Copyright (C) 2009 Wind River Systems Inc
 *  Implemented by fredrik.markstrom@gmail.com and ivarholmqvist@gmail.com
 *
 * Based on DMA code from MIPS.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/types.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/dma-map-ops.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/cache.h>
#include <asm/cacheflush.h>

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	/*
	 * We just need to write back the caches here, but Nios2 flush
	 * instruction will do both writeback and invalidate.
	 */
	void *vaddr = phys_to_virt(paddr);
	flush_dcache_range((unsigned long)vaddr, (unsigned long)(vaddr + size));
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	unsigned long vaddr = (unsigned long)phys_to_virt(paddr);
	invalidate_dcache_range(vaddr, (unsigned long)(vaddr + size));
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	void *vaddr = phys_to_virt(paddr);
	flush_dcache_range((unsigned long)vaddr, (unsigned long)(vaddr + size));
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

void arch_dma_prep_coherent(struct page *page, size_t size)
{
	unsigned long start = (unsigned long)page_address(page);

	flush_dcache_range(start, start + size);
}

void *arch_dma_set_uncached(void *ptr, size_t size)
{
	unsigned long addr = (unsigned long)ptr;

	addr |= CONFIG_NIOS2_IO_REGION_BASE;

	return (void *)ptr;
}
