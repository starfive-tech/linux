/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#undef DEBUG

#include <linux/dma-map-ops.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/export.h>

#include <asm/cacheflush.h>

#if defined(CONFIG_MMU) && !defined(CONFIG_COLDFIRE)
void arch_dma_prep_coherent(struct page *page, size_t size)
{
	cache_push(page_to_phys(page), size);
}

pgprot_t pgprot_dmacoherent(pgprot_t prot)
{
	if (CPU_IS_040_OR_060) {
		pgprot_val(prot) &= ~_PAGE_CACHE040;
		pgprot_val(prot) |= _PAGE_GLOBAL040 | _PAGE_NOCACHE_S;
	} else {
		pgprot_val(prot) |= _PAGE_NOCACHE030;
	}
	return prot;
}
#else
void *arch_dma_alloc(struct device *dev, size_t size, dma_addr_t *dma_handle,
		gfp_t gfp, unsigned long attrs)
{
	void *ret;

	if (dev == NULL || (*dev->dma_mask < 0xffffffff))
		gfp |= GFP_DMA;
	ret = (void *)__get_free_pages(gfp, get_order(size));

	if (ret != NULL) {
		memset(ret, 0, size);
		*dma_handle = virt_to_phys(ret);
	}
	return ret;
}

void arch_dma_free(struct device *dev, size_t size, void *vaddr,
		dma_addr_t dma_handle, unsigned long attrs)
{
	free_pages((unsigned long)vaddr, get_order(size));
}

#endif /* CONFIG_MMU && !CONFIG_COLDFIRE */

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	/*
	 * cache_push() always invalidates in addition to cleaning
	 * write-back caches.
	 */
	cache_push(paddr, size);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	cache_clear(paddr, size);
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	cache_push(paddr, size);
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
