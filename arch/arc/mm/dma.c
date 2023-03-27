// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 */

#include <linux/dma-map-ops.h>
#include <asm/cache.h>
#include <asm/cacheflush.h>

/*
 * ARCH specific callbacks for generic noncoherent DMA ops
 *  - hardware IOC not available (or "dma-coherent" not set for device in DT)
 *  - But still handle both coherent and non-coherent requests from caller
 *
 * For DMA coherent hardware (IOC) generic code suffices
 */

void arch_dma_prep_coherent(struct page *page, size_t size)
{
	/*
	 * Evict any existing L1 and/or L2 lines for the backing page
	 * in case it was used earlier as a normal "cached" page.
	 * Yeah this bit us - STAR 9000898266
	 *
	 * Although core does call flush_cache_vmap(), it gets kvaddr hence
	 * can't be used to efficiently flush L1 and/or L2 which need paddr
	 * Currently flush_cache_vmap nukes the L1 cache completely which
	 * will be optimized as a separate commit
	 */
	dma_cache_wback_inv(page_to_phys(page), size);
}

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	dma_cache_wback(paddr, size);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	dma_cache_inv(paddr, size);
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	dma_cache_wback_inv(paddr, size);
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

/*
 * Plug in direct dma map ops.
 */
void arch_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
			const struct iommu_ops *iommu, bool coherent)
{
	/*
	 * IOC hardware snoops all DMA traffic keeping the caches consistent
	 * with memory - eliding need for any explicit cache maintenance of
	 * DMA buffers.
	 */
	if (is_isa_arcv2() && ioc_enable && coherent)
		dev->dma_coherent = true;

	dev_info(dev, "use %scoherent DMA ops\n",
		 dev->dma_coherent ? "" : "non");
}
