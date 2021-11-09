/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 SiFive
 */

#ifndef __ASM_RISCV_VECTOR_H
#define __ASM_RISCV_VECTOR_H

#include <linux/types.h>

void rvv_enable(void);
void rvv_disable(void);
void kernel_rvv_begin(void);
void kernel_rvv_end(void);

#endif /* ! __ASM_RISCV_VECTOR_H */
