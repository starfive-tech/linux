// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Hangzhou C-SKY Microsystems co.,ltd.

#include <linux/cache.h>
#include <linux/dma-map-ops.h>
#include <linux/genalloc.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/types.h>
#include <asm/cache.h>

static inline void cache_op(phys_addr_t paddr, size_t size,
			    void (*fn)(unsigned long start, unsigned long end))
{
	struct page *page    = phys_to_page(paddr);
	void *start          = __va(page_to_phys(page));
	unsigned long offset = offset_in_page(paddr);
	size_t left          = size;

	do {
		size_t len = left;

		if (offset + len > PAGE_SIZE)
			len = PAGE_SIZE - offset;

		if (PageHighMem(page)) {
			start = kmap_atomic(page);

			fn((unsigned long)start + offset,
					(unsigned long)start + offset + len);

			kunmap_atomic(start);
		} else {
			fn((unsigned long)start + offset,
					(unsigned long)start + offset + len);
		}
		offset = 0;

		page++;
		start += PAGE_SIZE;
		left -= len;
	} while (left);
}

static void dma_wbinv_set_zero_range(unsigned long start, unsigned long end)
{
	memset((void *)start, 0, end - start);
	dma_wbinv_range(start, end);
}

void arch_dma_prep_coherent(struct page *page, size_t size)
{
	cache_op(page_to_phys(page), size, dma_wbinv_set_zero_range);
}

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	cache_op(paddr, size, dma_wb_range);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	cache_op(paddr, size, dma_inv_range);
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	cache_op(paddr, size, dma_wbinv_range);
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
