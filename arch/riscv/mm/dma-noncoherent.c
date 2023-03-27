// SPDX-License-Identifier: GPL-2.0-only
/*
 * RISC-V specific functions to support DMA for non-coherent devices
 *
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
 */

#include <linux/dma-direct.h>
#include <linux/dma-map-ops.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>

static bool noncoherent_supported;

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	void *vaddr = phys_to_virt(paddr);

	ALT_CMO_OP(clean, vaddr, size, riscv_cbom_block_size);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	void *vaddr = phys_to_virt(paddr);

	ALT_CMO_OP(inval, vaddr, size, riscv_cbom_block_size);
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	void *vaddr = phys_to_virt(paddr);

	ALT_CMO_OP(flush, vaddr, size, riscv_cbom_block_size);
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
	void *flush_addr = page_address(page);

	ALT_CMO_OP(flush, flush_addr, size, riscv_cbom_block_size);
}

void arch_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
		const struct iommu_ops *iommu, bool coherent)
{
	WARN_TAINT(!coherent && riscv_cbom_block_size > ARCH_DMA_MINALIGN,
		   TAINT_CPU_OUT_OF_SPEC,
		   "%s %s: ARCH_DMA_MINALIGN smaller than riscv,cbom-block-size (%d < %d)",
		   dev_driver_string(dev), dev_name(dev),
		   ARCH_DMA_MINALIGN, riscv_cbom_block_size);

	WARN_TAINT(!coherent && !noncoherent_supported, TAINT_CPU_OUT_OF_SPEC,
		   "%s %s: device non-coherent but no non-coherent operations supported",
		   dev_driver_string(dev), dev_name(dev));

	dev->dma_coherent = coherent;
}

void riscv_noncoherent_supported(void)
{
	WARN(!riscv_cbom_block_size,
	     "Non-coherent DMA support enabled without a block size\n");
	noncoherent_supported = true;
}
