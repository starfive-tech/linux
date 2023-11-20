/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */

#ifndef _ASM_RISCV_IRQ_H
#define _ASM_RISCV_IRQ_H

#include <linux/interrupt.h>
#include <linux/linkage.h>

#include <asm-generic/irq.h>

void riscv_set_intc_hwnode_fn(struct fwnode_handle *(*fn)(void));

struct fwnode_handle *riscv_get_intc_hwnode(void);

#ifdef CONFIG_RISCV_AMP
#define IPI_AMP		15
void riscv_set_ipi_amp_enable(void);
int riscv_get_ipi_amp_enable(void);
void ipi_set_extra_bits(unsigned long (*func)(void));
unsigned long riscv_clear_amp_bits(void);
void register_ipi_mailbox_handler(void (*handler)(unsigned long));
#endif

#endif /* _ASM_RISCV_IRQ_H */
