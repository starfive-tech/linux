#ifndef STARFIVE_VIC7100_H
#define STARFIVE_VIC7100_H
#include <asm/io.h>
#include <soc/sifive/sifive_l2_cache.h>

/*cache.c*/
#define starfive_flush_dcache(start, len) \
	sifive_l2_flush64_range(start, len)

/*dma*/
#define CONFIG_DW_DEBUG

#define DMA_PRINTK(fmt,...) \
	printk("[DW_DMA] %s():%d \n" fmt, __func__, __LINE__, ##__VA_ARGS__)

#ifdef CONFIG_DW_DEBUG
#define DMA_DEBUG(fmt,...) \
	printk("[DW_DMA_DEBUG] %s():%d \n" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define DMA_BEBUG(fmt,...)
#endif

#define _dw_virt_to_phys(vaddr) (pfn_to_phys(virt_to_pfn(vaddr)))
#define _dw_phys_to_virt(paddr) (page_to_virt(phys_to_page(paddr)))

void *dw_phys_to_virt(u64 phys);
u64 dw_virt_to_phys(void *vaddr);

int dw_dma_async_do_memcpy(void *src, void *dst, size_t size);
int dw_dma_memcpy_raw(dma_addr_t src_dma, dma_addr_t dst_dma, size_t size);
int dw_dma_memcpy(void *src, void *dst, size_t size);

int dw_dma_mem2mem_arry(void);
int dw_dma_mem2mem_test(void);

#endif /*STARFIVE_VIC7100_H*/