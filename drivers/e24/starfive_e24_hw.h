/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __STARFIVE_E24_HW_H__
#define __STARFIVE_E24_HW_H__
/*
 * Hardware-specific operation entry points.
 */
struct e24_hw_ops {
	/*
	 * Gets the clock for E24.
	 */
	int (*init)(void *hw_arg);
	/*
	 * Enable power/clock, but keep the core stalled.
	 */
	int (*enable)(void *hw_arg);
	/*
	 * Diable power/clock.
	 */
	void (*disable)(void *hw_arg);
	/*
	 * Reset the core.
	 */
	void (*reset)(void *hw_arg);
	/*
	 * Unstall the core.
	 */
	void (*release)(void *hw_arg);
	/*
	 * Stall the core.
	 */
	void (*halt)(void *hw_arg);

	/* Get HW-specific data to pass to the DSP on synchronization
	 *
	 *  param hw_arg: opaque parameter passed to DSP at initialization
	 *  param sz: return size of sync data here
	 *  return a buffer allocated with kmalloc that the caller will free
	 */
	void *(*get_hw_sync_data)(void *hw_arg, size_t *sz);

	/*
	 * Send IRQ to the core.
	 */
	void (*send_irq)(void *hw_arg);

	/*
	 * Check whether region of physical memory may be handled by
	 * dma_sync_* operations
	 *
	 * \param hw_arg: opaque parameter passed to DSP at initialization
	 *                time
	 */
	bool (*cacheable)(void *hw_arg, unsigned long pfn, unsigned long n_pages);
	/*
	 * Synchronize region of memory for DSP access.
	 *
	 * \param hw_arg: opaque parameter passed to DSP at initialization
	 *                time
	 */
	void (*dma_sync_for_device)(void *hw_arg,
				    void *vaddr, phys_addr_t paddr,
				    unsigned long sz, unsigned int flags);
	/*
	 * Synchronize region of memory for host access.
	 *
	 * \param hw_arg: opaque parameter passed to DSP at initialization
	 *                time
	 */
	void (*dma_sync_for_cpu)(void *hw_arg,
				 void *vaddr, phys_addr_t paddr,
				 unsigned long sz, unsigned int flags);

	/*
	 * memcpy data/code to device-specific memory.
	 */
	void (*memcpy_tohw)(void __iomem *dst, const void *src, size_t sz);
	/*
	 * memset device-specific memory.
	 */
	void (*memset_hw)(void __iomem *dst, int c, size_t sz);

	/*
	 * Check DSP status.
	 *
	 * \param hw_arg: opaque parameter passed to DSP at initialization
	 *                time
	 * \return whether the core has crashed and needs to be restarted
	 */
	bool (*panic_check)(void *hw_arg);
};

long e24_init_hw(struct platform_device *pdev, void *hw_arg);
struct e24_hw_ops *e24_get_hw_ops(void);
#endif
