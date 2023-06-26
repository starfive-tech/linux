/* SPDX-License-Identifier: GPL-2.0 */
/*
 * SiFive Composable Cache Controller header file
 *
 */

#ifndef __SOC_SIFIVE_CCACHE_H
#define __SOC_SIFIVE_CCACHE_H

#include <linux/io.h>
#include <linux/jump_label.h>

extern int register_sifive_ccache_error_notifier(struct notifier_block *nb);
extern int unregister_sifive_ccache_error_notifier(struct notifier_block *nb);

#define SIFIVE_CCACHE_ERR_TYPE_CE 0
#define SIFIVE_CCACHE_ERR_TYPE_UE 1

DECLARE_STATIC_KEY_FALSE(sifive_ccache_handle_noncoherent_key);

static inline bool sifive_ccache_handle_noncoherent(void)
{
#ifdef CONFIG_SIFIVE_CCACHE
	return static_branch_unlikely(&sifive_ccache_handle_noncoherent_key);
#else
	return false;
#endif
}

void sifive_ccache_flush_range(phys_addr_t start, size_t len);
void *sifive_ccache_set_uncached(void *addr, size_t size);
static inline void sifive_ccache_clear_uncached(void *addr, size_t size)
{
	memunmap(addr);
}

#endif /* __SOC_SIFIVE_CCACHE_H */
