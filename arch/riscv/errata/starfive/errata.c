// SPDX-License-Identifier: GPL-2.0-only
/*
 * Erratas to be applied for StarFive CPU cores
 *
 * Copyright (C) 2023 Shanghai StarFive Technology Co., Ltd.
 *
 */

#include <linux/memory.h>
#include <linux/module.h>

#include <asm/alternative.h>
#include <asm/cacheflush.h>
#include <asm/errata_list.h>
#include <asm/patch.h>
#include <asm/processor.h>
#include <asm/sbi.h>
#include <asm/vendorid_list.h>

#define STARFIVE_DUBHE90_MARCHID	0x80000000DB000090UL
#define STARFIVE_DUBHE90_MIMPID		0x0000000020230930UL

#define STARFIVE_DUBHE80_MARCHID	0x80000000DB000080UL
#define STARFIVE_DUBHE80_MIMPID		0x0000000020230831UL

#define STARFIVE_DUBHE70_MARCHID	0x80000000DB000070UL
#define STARFIVE_DUBHE70_MIMPID		0x0000000020240131UL

DEFINE_STATIC_KEY_FALSE(bypass_envcfg_csr_key);

static void errata_probe_cmo(unsigned int stage, unsigned long arch_id,
			     unsigned long impid)
{
	if (!IS_ENABLED(CONFIG_ERRATA_STARFIVE_CMO))
		return;

	if (arch_id == STARFIVE_DUBHE90_MARCHID) {
		if (impid > STARFIVE_DUBHE90_MIMPID)
			return;
	}

	if (arch_id == STARFIVE_DUBHE80_MARCHID) {
		if (impid > STARFIVE_DUBHE80_MIMPID)
			return;
	}

	riscv_cbom_block_size = L1_CACHE_BYTES;
	riscv_noncoherent_supported();
}

static void errata_bypass_envcfg_csr(unsigned int stage, unsigned long arch_id,
				     unsigned long impid)
{
	if (arch_id == STARFIVE_DUBHE90_MARCHID) {
		if (impid > STARFIVE_DUBHE90_MIMPID)
			return;
	}

	if (arch_id == STARFIVE_DUBHE80_MARCHID) {
		if (impid > STARFIVE_DUBHE80_MIMPID)
			return;
	}

	static_branch_enable(&bypass_envcfg_csr_key);
}

void starfive_errata_patch_func(struct alt_entry *begin,
				struct alt_entry *end,
				unsigned long archid,
				unsigned long impid,
				unsigned int stage)
{
	if (archid == STARFIVE_DUBHE70_MARCHID)
		return;

	if (stage == RISCV_ALTERNATIVES_BOOT) {
		errata_probe_cmo(stage, archid, impid);
		errata_bypass_envcfg_csr(stage, archid, impid);
	}
}
