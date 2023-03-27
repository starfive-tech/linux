// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2000  Ani Joshi <ajoshi@unixbox.com>
 * Copyright (C) 2000, 2001, 06	 Ralf Baechle <ralf@linux-mips.org>
 * swiped from i386, and cloned for MIPS by Geert, polished by Ralf.
 */
#include <linux/dma-direct.h>
#include <linux/dma-map-ops.h>
#include <linux/highmem.h>

#include <asm/cache.h>
#include <asm/cpu-type.h>
#include <asm/io.h>

/*
 * The affected CPUs below in 'cpu_needs_post_dma_flush()' can speculatively
 * fill random cachelines with stale data at any time, requiring an extra
 * flush post-DMA.
 *
 * Warning on the terminology - Linux calls an uncached area coherent;  MIPS
 * terminology calls memory areas with hardware maintained coherency coherent.
 *
 * Note that the R14000 and R16000 should also be checked for in this condition.
 * However this function is only called on non-I/O-coherent systems and only the
 * R10000 and R12000 are used in such systems, the SGI IP28 Indigo² rsp.
 * SGI IP32 aka O2.
 */
static inline bool cpu_needs_post_dma_flush(void)
{
	switch (boot_cpu_type()) {
	case CPU_R10000:
	case CPU_R12000:
	case CPU_BMIPS5000:
	case CPU_LOONGSON2EF:
	case CPU_XBURST:
		return true;
	default:
		/*
		 * Presence of MAARs suggests that the CPU supports
		 * speculatively prefetching data, and therefore requires
		 * the post-DMA flush/invalidate.
		 */
		return cpu_has_maar;
	}
}

void arch_dma_prep_coherent(struct page *page, size_t size)
{
	dma_cache_wback_inv((unsigned long)page_address(page), size);
}

void *arch_dma_set_uncached(void *addr, size_t size)
{
	return (void *)(__pa(addr) + UNCAC_BASE);
}

/*
 * A single sg entry may refer to multiple physically contiguous pages.  But
 * we still need to process highmem pages individually.  If highmem is not
 * configured then the bulk of this loop gets optimized out.
 */
static inline void dma_sync_phys(phys_addr_t paddr, size_t size,
		void(*cache_op)(unsigned long start, unsigned long size))
{
	struct page *page = pfn_to_page(paddr >> PAGE_SHIFT);
	unsigned long offset = paddr & ~PAGE_MASK;
	size_t left = size;

	do {
		size_t len = left;
		void *addr;

		if (PageHighMem(page)) {
			if (offset + len > PAGE_SIZE)
				len = PAGE_SIZE - offset;
		}

		addr = kmap_atomic(page);
		cache_op((unsigned long)addr + offset, len);
		kunmap_atomic(addr);

		offset = 0;
		page++;
		left -= len;
	} while (left);
}

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	dma_sync_phys(paddr, size, _dma_cache_wback);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	dma_sync_phys(paddr, size, _dma_cache_inv);
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	dma_sync_phys(paddr, size, _dma_cache_wback_inv);
}

static inline bool arch_sync_dma_clean_before_fromdevice(void)
{
	return false;
}

static inline bool arch_sync_dma_cpu_needs_post_dma_flush(void)
{
	return IS_ENABLED(CONFIG_ARCH_HAS_SYNC_DMA_FOR_CPU) &&
                    cpu_needs_post_dma_flush();
}

#include <linux/dma-sync.h>

#ifdef CONFIG_ARCH_HAS_SETUP_DMA_OPS
void arch_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
               const struct iommu_ops *iommu, bool coherent)
{
       dev->dma_coherent = coherent;
}
#endif
