// SPDX-License-Identifier: GPL-2.0-only
/*
 * RISC-V specific functions to support DMA for non-coherent devices
 *
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
 */

#include <linux/dma-direct.h>
#include <linux/dma-map-ops.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>
#include <asm/sbi.h>
#include <soc/sifive/sifive_ccache.h>

static bool noncoherent_supported;

#ifdef CONFIG_SOC_STARFIVE_DUBHE

#define DUBHE_UNCACHED_OFFSET 0x400000000
static bool noncoherent_supported = true;

void arch_sync_dma_for_device(phys_addr_t paddr, size_t size, enum dma_data_direction dir)
{
	switch (dir) {
	case DMA_BIDIRECTIONAL:
	case DMA_TO_DEVICE:
		sbi_cache_flush(paddr, size);
		break;
	case DMA_FROM_DEVICE:
		sbi_cache_invalidate(paddr, size);
		break;
	default:
		BUG();
	}
}

void arch_sync_dma_for_cpu(phys_addr_t paddr, size_t size, enum dma_data_direction dir)
{
	switch (dir) {
	case DMA_BIDIRECTIONAL:
	case DMA_FROM_DEVICE:
		sbi_cache_invalidate(paddr, size);
		break;
	case DMA_TO_DEVICE:
		break;
	default:
		BUG();
	}
}

void arch_dma_prep_coherent(struct page *page, size_t size)
{
	void *flush_addr = page_address(page);

	sbi_cache_flush(__pa(flush_addr), size);
}

void arch_dma_clear_uncached(void *addr, size_t size)
{
	memunmap(addr);
}

void *arch_dma_set_uncached(void *addr, size_t size)
{
	phys_addr_t phys_addr = __pa(addr) + DUBHE_UNCACHED_OFFSET;
	void *mem_base = NULL;

	mem_base = memremap(phys_addr, size, MEMREMAP_WT);
	if (!mem_base) {
		pr_err("%s memremap failed for addr %px\n", __func__, addr);
		return ERR_PTR(-EINVAL);
	}

	return mem_base;
}

#else
void arch_sync_dma_for_device(phys_addr_t paddr, size_t size,
			      enum dma_data_direction dir)
{
	void *vaddr;

	if (sifive_ccache_handle_noncoherent()) {
		sifive_ccache_flush_range(paddr, size);
		return;
	}

	vaddr = phys_to_virt(paddr);
	switch (dir) {
	case DMA_TO_DEVICE:
		ALT_CMO_OP(clean, vaddr, size, riscv_cbom_block_size);
		break;
	case DMA_FROM_DEVICE:
		ALT_CMO_OP(clean, vaddr, size, riscv_cbom_block_size);
		break;
	case DMA_BIDIRECTIONAL:
		ALT_CMO_OP(flush, vaddr, size, riscv_cbom_block_size);
		break;
	default:
		break;
	}
}

void arch_sync_dma_for_cpu(phys_addr_t paddr, size_t size,
			   enum dma_data_direction dir)
{
	void *vaddr;

	if (sifive_ccache_handle_noncoherent()) {
		sifive_ccache_flush_range(paddr, size);
		return;
	}

	vaddr = phys_to_virt(paddr);
	switch (dir) {
	case DMA_TO_DEVICE:
		break;
	case DMA_FROM_DEVICE:
	case DMA_BIDIRECTIONAL:
		ALT_CMO_OP(flush, vaddr, size, riscv_cbom_block_size);
		break;
	default:
		break;
	}
}

void *arch_dma_set_uncached(void *addr, size_t size)
{
	if (sifive_ccache_handle_noncoherent())
		return sifive_ccache_set_uncached(addr, size);

	return addr;
}

void arch_dma_clear_uncached(void *addr, size_t size)
{
	if (sifive_ccache_handle_noncoherent())
		sifive_ccache_clear_uncached(addr, size);
}

void arch_dma_prep_coherent(struct page *page, size_t size)
{
	void *flush_addr = page_address(page);

	if (sifive_ccache_handle_noncoherent()) {
		memset(flush_addr, 0, size);
		sifive_ccache_flush_range(__pa(flush_addr), size);
		return;
	}

	ALT_CMO_OP(flush, flush_addr, size, riscv_cbom_block_size);
}
#endif

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
