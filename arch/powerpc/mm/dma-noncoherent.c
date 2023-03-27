// SPDX-License-Identifier: GPL-2.0-only
/*
 *  PowerPC version derived from arch/arm/mm/consistent.c
 *    Copyright (C) 2001 Dan Malek (dmalek@jlc.net)
 *
 *  Copyright (C) 2000 Russell King
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/highmem.h>
#include <linux/dma-direct.h>
#include <linux/dma-map-ops.h>

#include <asm/tlbflush.h>
#include <asm/dma.h>

enum dma_cache_op {
	DMA_CACHE_CLEAN,
	DMA_CACHE_INVAL,
	DMA_CACHE_FLUSH,
};

/*
 * make an area consistent.
 */
static void __dma_op(void *vaddr, size_t size, enum dma_cache_op op)
{
	unsigned long start = (unsigned long)vaddr;
	unsigned long end   = start + size;

	switch (op) {
	case DMA_CACHE_CLEAN:
		clean_dcache_range(start, end);
		break;
	case DMA_CACHE_INVAL:
		invalidate_dcache_range(start, end);
		break;
	case DMA_CACHE_FLUSH:
		flush_dcache_range(start, end);
		break;
	}
}

#ifdef CONFIG_HIGHMEM
/*
 * __dma_highmem_op() implementation for systems using highmem.
 * In this case, each page of a buffer must be kmapped/kunmapped
 * in order to have a virtual address for __dma_op(). This must
 * not sleep so kmap_atomic()/kunmap_atomic() are used.
 *
 * Note: yes, it is possible and correct to have a buffer extend
 * beyond the first page.
 */
static inline void __dma_highmem_op(struct page *page,
		unsigned long offset, size_t size, enum dma_cache_op op)
{
	size_t seg_size = min((size_t)(PAGE_SIZE - offset), size);
	size_t cur_size = seg_size;
	unsigned long flags, start, seg_offset = offset;
	int nr_segs = 1 + ((size - seg_size) + PAGE_SIZE - 1)/PAGE_SIZE;
	int seg_nr = 0;

	local_irq_save(flags);

	do {
		start = (unsigned long)kmap_atomic(page + seg_nr) + seg_offset;

		/* Sync this buffer segment */
		__dma_op((void *)start, seg_size, op);
		kunmap_atomic((void *)start);
		seg_nr++;

		/* Calculate next buffer segment size */
		seg_size = min((size_t)PAGE_SIZE, size - cur_size);

		/* Add the segment size to our running total */
		cur_size += seg_size;
		seg_offset = 0;
	} while (seg_nr < nr_segs);

	local_irq_restore(flags);
}
#endif /* CONFIG_HIGHMEM */

/*
 * __dma_phys_op makes memory consistent. identical to __dma_op, but
 * takes a phys_addr_t instead of a virtual address
 */
static void __dma_phys_op(phys_addr_t paddr, size_t size, enum dma_cache_op op)
{
	struct page *page = pfn_to_page(paddr >> PAGE_SHIFT);
	unsigned offset = paddr & ~PAGE_MASK;

#ifdef CONFIG_HIGHMEM
	__dma_highmem_op(page, offset, size, op);
#else
	unsigned long start = (unsigned long)page_address(page) + offset;
	__dma_op((void *)start, size, op);
#endif
}

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	__dma_phys_op(paddr, size, DMA_CACHE_CLEAN);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	__dma_phys_op(paddr, size, DMA_CACHE_INVAL);
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	__dma_phys_op(paddr, size, DMA_CACHE_FLUSH);
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
	unsigned long kaddr = (unsigned long)page_address(page);

	flush_dcache_range(kaddr, kaddr + size);
}
