/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Western Digital Corporation or its affiliates.
 */
#ifndef __ASM_SECTIONS_H
#define __ASM_SECTIONS_H

#include <asm-generic/sections.h>

extern char _start[];
extern char _start_kernel[];
extern phys_addr_t end_linear_map;
#endif /* __ASM_SECTIONS_H */
