/* SPDX-License-Identifier: GPL-2.0 */
/*
 * SiFive L2 Cache Controller header file
 *
 */

#ifndef __SOC_SIFIVE_L2_CACHE_H
#define __SOC_SIFIVE_L2_CACHE_H

#ifdef CONFIG_SIFIVE_L2_FLUSH
extern void sifive_l2_flush64_range(unsigned long start, unsigned long len);
#endif

extern int register_sifive_l2_error_notifier(struct notifier_block *nb);
extern int unregister_sifive_l2_error_notifier(struct notifier_block *nb);

#define SIFIVE_L2_ERR_TYPE_CE 0
#define SIFIVE_L2_ERR_TYPE_UE 1

#endif /* __SOC_SIFIVE_L2_CACHE_H */
