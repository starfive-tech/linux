// SPDX-License-Identifier: GPL-2.0
/*
 * StarFive L2 cache controller driver
 *
 * Copyright (C) 2023 StarFive Technology Co., Ltd.
 *
 */
#include <linux/cacheflush.h>
#include <linux/of.h>

#include <asm/dma-noncoherent.h>
#include <asm/sbi.h>

#define STARFIVE_SBI_EXT		0x0900067e

enum starfive_sbi_ext_fid {
	STARFIVE_SBI_EXT_L2_FLUSH = 0,
	STARFIVE_SBI_EXT_L2_INVALIDATE,
};

static void sbi_l2cache_flush(phys_addr_t paddr, size_t size)
{
	sbi_ecall(STARFIVE_SBI_EXT, STARFIVE_SBI_EXT_L2_FLUSH,
		  paddr, size, 0, 0, 0, 0);
}

static void sbi_l2cache_invalidate(phys_addr_t paddr, size_t size)
{
	sbi_ecall(STARFIVE_SBI_EXT, STARFIVE_SBI_EXT_L2_INVALIDATE,
		  paddr, size, 0, 0, 0, 0);
}

static const struct riscv_nonstd_cache_ops dubhe_cmo_ops __initdata = {
	.wback = &sbi_l2cache_flush,
	.inv = &sbi_l2cache_invalidate,
	.wback_inv = &sbi_l2cache_invalidate,
};

static const struct of_device_id starfive_l2cache_ids[] = {
	{ .compatible = "starfive,dubhe-l2cache" },
	{ /* end of table */ }
};

static int __init starfive_cache_init(void)
{
	struct device_node *np;

	np = of_find_matching_node(NULL, starfive_l2cache_ids);
	if (!of_device_is_available(np))
		return -ENODEV;

	riscv_cbom_block_size = L1_CACHE_BYTES;
	riscv_noncoherent_supported();
	riscv_noncoherent_register_cache_ops(&dubhe_cmo_ops);

	return 0;
}

arch_initcall(starfive_cache_init);
