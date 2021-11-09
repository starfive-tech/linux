/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2021 SiFive
 */

#include <linux/hardirq.h>
#include <asm-generic/xor.h>
#ifdef CONFIG_VECTOR
#include <asm/vector.h>
#include <asm/switch_to.h>

void xor_regs_2_(unsigned long bytes, unsigned long *p1, unsigned long *p2);
void xor_regs_3_(unsigned long bytes, unsigned long *p1, unsigned long *p2, unsigned long *p3);
void xor_regs_4_(unsigned long bytes, unsigned long *p1, unsigned long *p2, unsigned long *p3,
		 unsigned long *p4);
void xor_regs_5_(unsigned long bytes, unsigned long *p1, unsigned long *p2, unsigned long *p3,
		 unsigned long *p4, unsigned long *p5);

static void xor_rvv_2(unsigned long bytes, unsigned long *p1, unsigned long *p2)
{
	kernel_rvv_begin();
	xor_regs_2_(bytes, p1, p2);
	kernel_rvv_end();
}

static void xor_rvv_3(unsigned long bytes, unsigned long *p1, unsigned long *p2, unsigned long *p3)
{
	kernel_rvv_begin();
	xor_regs_3_(bytes, p1, p2, p3);
	kernel_rvv_end();
}

static void xor_rvv_4(unsigned long bytes, unsigned long *p1, unsigned long *p2, unsigned long *p3,
		      unsigned long *p4)
{
	kernel_rvv_begin();
	xor_regs_4_(bytes, p1, p2, p3, p4);
	kernel_rvv_end();
}

static void xor_rvv_5(unsigned long bytes, unsigned long *p1, unsigned long *p2, unsigned long *p3,
		      unsigned long *p4, unsigned long *p5)
{
	kernel_rvv_begin();
	xor_regs_5_(bytes, p1, p2, p3, p4, p5);
	kernel_rvv_end();
}

static struct xor_block_template xor_block_rvv = {
	.name = "rvv",
	.do_2 = xor_rvv_2,
	.do_3 = xor_rvv_3,
	.do_4 = xor_rvv_4,
	.do_5 = xor_rvv_5
};

#undef XOR_TRY_TEMPLATES
#define XOR_TRY_TEMPLATES           \
	do {        \
		xor_speed(&xor_block_8regs);    \
		xor_speed(&xor_block_32regs);    \
		if (has_vector()) { \
			xor_speed(&xor_block_rvv);\
		} \
	} while (0)
#endif
