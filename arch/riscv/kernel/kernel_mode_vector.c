// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
 * Copyright (C) 2017 Linaro Ltd. <ard.biesheuvel@linaro.org>
 * Copyright (C) 2021 SiFive
 */
#include <linux/compiler.h>
#include <linux/irqflags.h>
#include <linux/percpu.h>
#include <linux/preempt.h>
#include <linux/types.h>

#include <asm/vector.h>
#include <asm/switch_to.h>

DECLARE_PER_CPU(bool, vector_context_busy);
DEFINE_PER_CPU(bool, vector_context_busy);

/*
 * may_use_vector - whether it is allowable at this time to issue vector
 *                instructions or access the vector register file
 *
 * Callers must not assume that the result remains true beyond the next
 * preempt_enable() or return from softirq context.
 */
static __must_check inline bool may_use_vector(void)
{
	/*
	 * vector_context_busy is only set while preemption is disabled,
	 * and is clear whenever preemption is enabled. Since
	 * this_cpu_read() is atomic w.r.t. preemption, vector_context_busy
	 * cannot change under our feet -- if it's set we cannot be
	 * migrated, and if it's clear we cannot be migrated to a CPU
	 * where it is set.
	 */
	return !in_irq() && !irqs_disabled() && !in_nmi() &&
	       !this_cpu_read(vector_context_busy);
}



/*
 * Claim ownership of the CPU vector context for use by the calling context.
 *
 * The caller may freely manipulate the vector context metadata until
 * put_cpu_vector_context() is called.
 */
static void get_cpu_vector_context(void)
{
	bool busy;

	preempt_disable();
	busy = __this_cpu_xchg(vector_context_busy, true);

	WARN_ON(busy);
}

/*
 * Release the CPU vector context.
 *
 * Must be called from a context in which get_cpu_vector_context() was
 * previously called, with no call to put_cpu_vector_context() in the
 * meantime.
 */
static void put_cpu_vector_context(void)
{
	bool busy = __this_cpu_xchg(vector_context_busy, false);

	WARN_ON(!busy);
	preempt_enable();
}

void rvv_enable(void)
{
	csr_set(CSR_STATUS, SR_VS);
}
EXPORT_SYMBOL(rvv_enable);

void rvv_disable(void)
{
	csr_clear(CSR_STATUS, SR_VS);
}
EXPORT_SYMBOL(rvv_disable);

/*
 * kernel_rvv_begin(): obtain the CPU vector registers for use by the calling
 * context
 *
 * Must not be called unless may_use_vector() returns true.
 * Task context in the vector registers is saved back to memory as necessary.
 *
 * A matching call to kernel_rvv_end() must be made before returning from the
 * calling context.
 *
 * The caller may freely use the vector registers until kernel_rvv_end() is
 * called.
 */
void kernel_rvv_begin(void)
{
	if (WARN_ON(!has_vector()))
		return;

	WARN_ON(!may_use_vector());

	/* Acquire kernel mode vector */
	get_cpu_vector_context();

	/* Save vector state, if any */
	vstate_save(current, task_pt_regs(current));

	/* Enable vector */
	rvv_enable();

	/* Invalidate vector regs */
	vector_flush_cpu_state();
}
EXPORT_SYMBOL_GPL(kernel_rvv_begin);

/*
 * kernel_rvv_end(): give the CPU vector registers back to the current task
 *
 * Must be called from a context in which kernel_rvv_begin() was previously
 * called, with no call to kernel_rvv_end() in the meantime.
 *
 * The caller must not use the vector registers after this function is called,
 * unless kernel_rvv_begin() is called again in the meantime.
 */
void kernel_rvv_end(void)
{
	if (WARN_ON(!has_vector()))
		return;

	/* Invalidate vector regs */
	vector_flush_cpu_state();

	/* Restore vector state, if any */
	vstate_restore(current, task_pt_regs(current));

	/* disable vector */
	rvv_disable();

	/* release kernel mode vector */
	put_cpu_vector_context();
}
EXPORT_SYMBOL_GPL(kernel_rvv_end);
