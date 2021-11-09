/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_SWITCH_TO_H
#define _ASM_RISCV_SWITCH_TO_H

#include <linux/jump_label.h>
#include <linux/slab.h>
#include <linux/sched/task_stack.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/csr.h>
#include <asm/asm-offsets.h>

#ifdef CONFIG_FPU
extern void __fstate_save(struct task_struct *save_to);
extern void __fstate_restore(struct task_struct *restore_from);

static inline void __fstate_clean(struct pt_regs *regs)
{
	regs->status = (regs->status & ~SR_FS) | SR_FS_CLEAN;
}

static inline void fstate_off(struct task_struct *task,
			      struct pt_regs *regs)
{
	regs->status = (regs->status & ~SR_FS) | SR_FS_OFF;
}

static inline void fstate_save(struct task_struct *task,
			       struct pt_regs *regs)
{
	if ((regs->status & SR_FS) == SR_FS_DIRTY) {
		__fstate_save(task);
		__fstate_clean(regs);
	}
}

static inline void fstate_restore(struct task_struct *task,
				  struct pt_regs *regs)
{
	if ((regs->status & SR_FS) != SR_FS_OFF) {
		__fstate_restore(task);
		__fstate_clean(regs);
	}
}

static inline void __switch_to_fpu(struct task_struct *prev,
				   struct task_struct *next)
{
	struct pt_regs *regs;

	regs = task_pt_regs(prev);
	if (unlikely(regs->status & SR_SD))
		fstate_save(prev, regs);
	fstate_restore(next, task_pt_regs(next));
}

extern struct static_key_false cpu_hwcap_fpu;
static __always_inline bool has_fpu(void)
{
	return static_branch_likely(&cpu_hwcap_fpu);
}
#else
static __always_inline bool has_fpu(void) { return false; }
#define fstate_save(task, regs) do { } while (0)
#define fstate_restore(task, regs) do { } while (0)
#define __switch_to_fpu(__prev, __next) do { } while (0)
#endif

#ifdef CONFIG_VECTOR
extern struct static_key_false cpu_hwcap_vector;
static __always_inline bool has_vector(void)
{
	return static_branch_likely(&cpu_hwcap_vector);
}
extern unsigned long riscv_vsize;
extern void __vstate_save(struct __riscv_v_state *save_to, void *datap);
extern void __vstate_restore(struct __riscv_v_state *restore_from, void *datap);

static inline void __vstate_clean(struct pt_regs *regs)
{
	regs->status = (regs->status & ~(SR_VS)) | SR_VS_CLEAN;
}

static inline void vstate_off(struct task_struct *task,
			      struct pt_regs *regs)
{
	regs->status = (regs->status & ~SR_VS) | SR_VS_OFF;
}

static inline void vstate_save(struct task_struct *task,
			       struct pt_regs *regs)
{
	if ((regs->status & SR_VS) == SR_VS_DIRTY) {
		struct __riscv_v_state *vstate = &(task->thread.vstate);

		__vstate_save(vstate, vstate->datap);
		__vstate_clean(regs);
	}
}

static inline void vstate_restore(struct task_struct *task,
				  struct pt_regs *regs)
{
	if ((regs->status & SR_VS) != SR_VS_OFF) {
		struct __riscv_v_state *vstate = &(task->thread.vstate);
		__vstate_restore(vstate, vstate->datap);
		__vstate_clean(regs);
	}
}

static inline void __switch_to_vector(struct task_struct *prev,
				   struct task_struct *next)
{
	struct pt_regs *regs;

	regs = task_pt_regs(prev);
	if (unlikely(regs->status & SR_SD))
		vstate_save(prev, regs);
	vstate_restore(next, task_pt_regs(next));
}

#else
static __always_inline bool has_vector(void) { return false; }
#define riscv_vsize (0)
#define vstate_save(task, regs) do { } while (0)
#define vstate_restore(task, regs) do { } while (0)
#define __switch_to_vector(__prev, __next) do { } while (0)
#endif

extern struct task_struct *__switch_to(struct task_struct *,
				       struct task_struct *);

#define switch_to(prev, next, last)			\
do {							\
	struct task_struct *__prev = (prev);		\
	struct task_struct *__next = (next);		\
	if (has_fpu())					\
		__switch_to_fpu(__prev, __next);	\
	if (has_vector())					\
		__switch_to_vector(__prev, __next);	\
	((last) = __switch_to(__prev, __next));		\
} while (0)

#endif /* _ASM_RISCV_SWITCH_TO_H */
