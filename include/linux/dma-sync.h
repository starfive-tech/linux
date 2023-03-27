// SPDX-License-Identifier: GPL-2.0
/*
 * Cache operations depending on function and direction argument, inspired by
 * https://lore.kernel.org/lkml/20180518175004.GF17671@n2100.armlinux.org.uk
 * "dma_sync_*_for_cpu and direction=TO_DEVICE (was Re: [PATCH 02/20]
 * dma-mapping: provide a generic dma-noncoherent implementation)"
 *
 *          |   map          ==  for_device     |   unmap     ==  for_cpu
 *          |----------------------------------------------------------------
 * TO_DEV   |   writeback        writeback      |   none          none
 * FROM_DEV |   invalidate       invalidate     |   invalidate*   invalidate*
 * BIDIR    |   writeback        writeback      |   invalidate    invalidate
 *
 *     [*] needed for CPU speculative prefetches
 *
 * NOTE: we don't check the validity of direction argument as it is done in
 * upper layer functions (in include/linux/dma-mapping.h)
 *
 * This file can be included by arch/.../kernel/dma-noncoherent.c to provide
 * the respective high-level operations without having to expose the
 * cache management ops to drivers.
 */

void arch_sync_dma_for_device(phys_addr_t paddr, size_t size,
		enum dma_data_direction dir)
{
	switch (dir) {
	case DMA_TO_DEVICE:
		/*
		 * This may be an empty function on write-through caches,
		 * and it might invalidate the cache if an architecture has
		 * a write-back cache but no way to write it back without
		 * invalidating
		 */
		arch_dma_cache_wback(paddr, size);
		break;

	case DMA_FROM_DEVICE:
		/*
		 * FIXME: this should be handled the same across all
		 * architectures, see
		 * https://lore.kernel.org/all/20220606152150.GA31568@willie-the-truck/
		 */
		if (!arch_sync_dma_clean_before_fromdevice()) {
			arch_dma_cache_inv(paddr, size);
			break;
		}
		fallthrough;

	case DMA_BIDIRECTIONAL:
		/* Skip the invalidate here if it's done later */
		if (IS_ENABLED(CONFIG_ARCH_HAS_SYNC_DMA_FOR_CPU) &&
		    arch_sync_dma_cpu_needs_post_dma_flush())
			arch_dma_cache_wback(paddr, size);
		else
			arch_dma_cache_wback_inv(paddr, size);
		break;

	default:
		break;
	}
}

#ifdef CONFIG_ARCH_HAS_SYNC_DMA_FOR_CPU
/*
 * Mark the D-cache clean for these pages to avoid extra flushing.
 */
static void arch_dma_mark_dcache_clean(phys_addr_t paddr, size_t size)
{
#ifdef CONFIG_ARCH_DMA_MARK_DCACHE_CLEAN
	unsigned long pfn = PFN_UP(paddr);
	unsigned long off = paddr & (PAGE_SIZE - 1);
	size_t left = size;

	if (off)
		left -= PAGE_SIZE - off;

	while (left >= PAGE_SIZE) {
		struct page *page = pfn_to_page(pfn++);
		set_bit(PG_dcache_clean, &page->flags);
		left -= PAGE_SIZE;
	}
#endif
}

void arch_sync_dma_for_cpu(phys_addr_t paddr, size_t size,
		enum dma_data_direction dir)
{
	switch (dir) {
	case DMA_TO_DEVICE:
		break;

	case DMA_FROM_DEVICE:
	case DMA_BIDIRECTIONAL:
		/* FROM_DEVICE invalidate needed if speculative CPU prefetch only */
		if (arch_sync_dma_cpu_needs_post_dma_flush())
			arch_dma_cache_inv(paddr, size);

		if (size > PAGE_SIZE)
			arch_dma_mark_dcache_clean(paddr, size);
		break;

	default:
		break;
	}
}
#endif
