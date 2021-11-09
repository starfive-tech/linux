// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2009 Sunplus Core Technology Co., Ltd.
 *  Chen Liqin <liqin.chen@sunplusct.com>
 *  Lennox Wu <lennox.wu@sunplusct.com>
 * Copyright (C) 2012 Regents of the University of California
 */

#include <linux/signal.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/tracehook.h>
#include <linux/linkage.h>

#include <asm/ucontext.h>
#include <asm/vdso.h>
#include <asm/switch_to.h>
#include <asm/csr.h>

extern u32 __user_rt_sigreturn[2];
static size_t rvv_sc_size;

#define DEBUG_SIG 0

struct rt_sigframe {
	struct siginfo info;
#ifndef CONFIG_MMU
	u32 sigreturn_code[2];
#endif
	struct ucontext uc;
};

#ifdef CONFIG_FPU
static long restore_fp_state(struct pt_regs *regs,
			     union __riscv_fp_state __user *sc_fpregs)
{
	long err;
	struct __riscv_d_ext_state __user *state = &sc_fpregs->d;
	size_t i;

	err = __copy_from_user(&current->thread.fstate, state, sizeof(*state));
	if (unlikely(err))
		return err;

	fstate_restore(current, regs);

	/* We support no other extension state at this time. */
	for (i = 0; i < ARRAY_SIZE(sc_fpregs->q.reserved); i++) {
		u32 value;

		err = __get_user(value, &sc_fpregs->q.reserved[i]);
		if (unlikely(err))
			break;
		if (value != 0)
			return -EINVAL;
	}

	return err;
}

static long save_fp_state(struct pt_regs *regs,
			  union __riscv_fp_state __user *sc_fpregs)
{
	long err;
	struct __riscv_d_ext_state __user *state = &sc_fpregs->d;
	size_t i;

	fstate_save(current, regs);
	err = __copy_to_user(state, &current->thread.fstate, sizeof(*state));
	if (unlikely(err))
		return err;

	/* We support no other extension state at this time. */
	for (i = 0; i < ARRAY_SIZE(sc_fpregs->q.reserved); i++) {
		err = __put_user(0, &sc_fpregs->q.reserved[i]);
		if (unlikely(err))
			break;
	}

	return err;
}
#else
#define save_fp_state(task, regs) (0)
#define restore_fp_state(task, regs) (0)
#endif

#ifdef CONFIG_VECTOR
static long restore_v_state(struct pt_regs *regs, void **sc_reserved_ptr)
{
	long err;
	struct __sc_riscv_v_state __user *state = (struct __sc_riscv_v_state *)(*sc_reserved_ptr);
	void *datap;
	__u32 magic;
	__u32 size;

	/* Get magic number and check it. */
	err = __get_user(magic, &state->head.magic);
	err = __get_user(size, &state->head.size);
	if (unlikely(err))
		return err;

	if (magic != RVV_MAGIC || size != rvv_sc_size)
		return -EINVAL;

	/* Copy everything of __sc_riscv_v_state except datap. */
	err = __copy_from_user(&current->thread.vstate, &state->v_state,
			       RISCV_V_STATE_DATAP);
	if (unlikely(err))
		return err;

	/* Copy the pointer datap itself. */
	err = __get_user(datap, &state->v_state.datap);
	if (unlikely(err))
		return err;


	/* Copy the whole vector content from user space datap. */
	err = __copy_from_user(current->thread.vstate.datap, datap, riscv_vsize);
	if (unlikely(err))
		return err;

	vstate_restore(current, regs);

	/* Move sc_reserved_ptr to point the next signal context frame. */
	*sc_reserved_ptr += size;

	return err;
}

static long save_v_state(struct pt_regs *regs, void **sc_reserved_free_ptr)
{
	/*
	 * Put __sc_riscv_v_state to the user's signal context space pointed
	 * by sc_reserved_free_ptr and the datap point the address right
	 * after __sc_riscv_v_state.
	 */
	struct __sc_riscv_v_state __user *state = (struct __sc_riscv_v_state *)
		(*sc_reserved_free_ptr);
	void *datap = state + 1;
	long err;

	*sc_reserved_free_ptr += rvv_sc_size;

	err = __put_user(RVV_MAGIC, &state->head.magic);
	err = __put_user(rvv_sc_size, &state->head.size);

	vstate_save(current, regs);
	/* Copy everything of vstate but datap. */
	err = __copy_to_user(&state->v_state, &current->thread.vstate,
			     RISCV_V_STATE_DATAP);
	if (unlikely(err))
		return err;

	/* Copy the pointer datap itself. */
	err = __put_user(datap, &state->v_state.datap);
	if (unlikely(err))
		return err;

	/* Copy the whole vector content to user space datap. */
	err = __copy_to_user(datap, current->thread.vstate.datap, riscv_vsize);

	return err;
}
#else
#define save_v_state(task, regs) (0)
#define restore_v_state(task, regs) (0)
#endif

static long restore_sigcontext(struct pt_regs *regs,
	struct sigcontext __user *sc)
{
	long err;
	void *sc_reserved_ptr = sc->__reserved;
	/* sc_regs is structured the same as the start of pt_regs */
	err = __copy_from_user(regs, &sc->sc_regs, sizeof(sc->sc_regs));
	/* Restore the floating-point state. */
	if (has_fpu())
		err |= restore_fp_state(regs, &sc->sc_fpregs);

	while (1 && !err) {
		__u32 magic, size;
		struct __riscv_ctx_hdr *head = (struct __riscv_ctx_hdr *)sc_reserved_ptr;

		err |= __get_user(magic, &head->magic);
		err |= __get_user(size, &head->size);
		if (err)
			goto done;

		switch (magic) {
		case 0:
			if (size)
				goto invalid;
			goto done;
		case RVV_MAGIC:
			if (!has_vector())
				goto invalid;
			if (size != rvv_sc_size)
				goto invalid;
			err |= restore_v_state(regs, &sc_reserved_ptr);
			break;
		default:
			goto invalid;
		}
	}
done:
	return err;

invalid:
	return -EINVAL;
}

static size_t cal_rt_frame_size(void)
{
	struct rt_sigframe __user *frame;
	static size_t frame_size;
	size_t total_context_size = 0;
	size_t sc_reserved_size = sizeof(frame->uc.uc_mcontext.__reserved);

	if (frame_size)
		goto done;

	frame_size = sizeof(*frame);

	if (has_vector())
		total_context_size += rvv_sc_size;
	/* Preserved a __riscv_ctx_hdr for END signal context header. */
	total_context_size += sizeof(struct __riscv_ctx_hdr);

	if (total_context_size > sc_reserved_size)
		frame_size += (total_context_size - sc_reserved_size);

	frame_size = round_up(frame_size, 16);
done:
	return frame_size;

}

SYSCALL_DEFINE0(rt_sigreturn)
{
	struct pt_regs *regs = current_pt_regs();
	struct rt_sigframe __user *frame;
	struct task_struct *task;
	sigset_t set;
	size_t frame_size = cal_rt_frame_size();

	/* Always make any pending restarted system calls return -EINTR */
	current->restart_block.fn = do_no_restart_syscall;

	frame = (struct rt_sigframe __user *)regs->sp;

	if (!access_ok(frame, frame_size))
		goto badframe;

	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	set_current_blocked(&set);

	if (restore_sigcontext(regs, &frame->uc.uc_mcontext))
		goto badframe;

	if (restore_altstack(&frame->uc.uc_stack))
		goto badframe;

	return regs->a0;

badframe:
	task = current;
	if (show_unhandled_signals) {
		pr_info_ratelimited(
			"%s[%d]: bad frame in %s: frame=%p pc=%p sp=%p\n",
			task->comm, task_pid_nr(task), __func__,
			frame, (void *)regs->epc, (void *)regs->sp);
	}
	force_sig(SIGSEGV);
	return 0;
}

static long setup_sigcontext(struct rt_sigframe __user *frame,
	struct pt_regs *regs)
{
	struct sigcontext __user *sc = &frame->uc.uc_mcontext;
	long err;
	void *sc_reserved_free_ptr = sc->__reserved;

	/* sc_regs is structured the same as the start of pt_regs */
	err = __copy_to_user(&sc->sc_regs, regs, sizeof(sc->sc_regs));
	/* Save the floating-point state. */
	if (has_fpu())
		err |= save_fp_state(regs, &sc->sc_fpregs);
	/* Save the vector state. */
	if (has_vector())
		err |= save_v_state(regs, &sc_reserved_free_ptr);

	/* Put END __riscv_ctx_hdr at the end. */
	err = __put_user(END_MAGIC, &((struct __riscv_ctx_hdr *)sc_reserved_free_ptr)->magic);
	err = __put_user(END_HDR_SIZE, &((struct __riscv_ctx_hdr *)sc_reserved_free_ptr)->size);
	return err;
}

static inline void __user *get_sigframe(struct ksignal *ksig,
	struct pt_regs *regs, size_t framesize)
{
	unsigned long sp;
	/* Default to using normal stack */
	sp = regs->sp;

	/*
	 * If we are on the alternate signal stack and would overflow it, don't.
	 * Return an always-bogus address instead so we will die with SIGSEGV.
	 */
	if (on_sig_stack(sp) && !likely(on_sig_stack(sp - framesize)))
		return (void __user __force *)(-1UL);

	/* This is the X/Open sanctioned signal stack switching. */
	sp = sigsp(sp, ksig) - framesize;

	/* Align the stack frame. */
	sp &= ~0xfUL;

	return (void __user *)sp;
}

static int setup_rt_frame(struct ksignal *ksig, sigset_t *set,
	struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	long err = 0;
	size_t frame_size = cal_rt_frame_size();

	frame = get_sigframe(ksig, regs, frame_size);
	if (!access_ok(frame, frame_size))
		return -EFAULT;

	err |= copy_siginfo_to_user(&frame->info, &ksig->info);

	/* Create the ucontext. */
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(NULL, &frame->uc.uc_link);
	err |= __save_altstack(&frame->uc.uc_stack, regs->sp);
	err |= setup_sigcontext(frame, regs);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));
	if (err)
		return -EFAULT;

	/* Set up to return from userspace. */
#ifdef CONFIG_MMU
	regs->ra = (unsigned long)VDSO_SYMBOL(
		current->mm->context.vdso, rt_sigreturn);
#else
	/*
	 * For the nommu case we don't have a VDSO.  Instead we push two
	 * instructions to call the rt_sigreturn syscall onto the user stack.
	 */
	if (copy_to_user(&frame->sigreturn_code, __user_rt_sigreturn,
			 sizeof(frame->sigreturn_code)))
		return -EFAULT;
	regs->ra = (unsigned long)&frame->sigreturn_code;
#endif /* CONFIG_MMU */

	/*
	 * Set up registers for signal handler.
	 * Registers that we don't modify keep the value they had from
	 * user-space at the time we took the signal.
	 * We always pass siginfo and mcontext, regardless of SA_SIGINFO,
	 * since some things rely on this (e.g. glibc's debug/segfault.c).
	 */
	regs->epc = (unsigned long)ksig->ka.sa.sa_handler;
	regs->sp = (unsigned long)frame;
	regs->a0 = ksig->sig;                     /* a0: signal number */
	regs->a1 = (unsigned long)(&frame->info); /* a1: siginfo pointer */
	regs->a2 = (unsigned long)(&frame->uc);   /* a2: ucontext pointer */

#if DEBUG_SIG
	pr_info("SIG deliver (%s:%d): sig=%d pc=%p ra=%p sp=%p\n",
		current->comm, task_pid_nr(current), ksig->sig,
		(void *)regs->epc, (void *)regs->ra, frame);
#endif

	return 0;
}

static void handle_signal(struct ksignal *ksig, struct pt_regs *regs)
{
	sigset_t *oldset = sigmask_to_save();
	int ret;

	/* Are we from a system call? */
	if (regs->cause == EXC_SYSCALL) {
		/* Avoid additional syscall restarting via ret_from_exception */
		regs->cause = -1UL;
		/* If so, check system call restarting.. */
		switch (regs->a0) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			regs->a0 = -EINTR;
			break;

		case -ERESTARTSYS:
			if (!(ksig->ka.sa.sa_flags & SA_RESTART)) {
				regs->a0 = -EINTR;
				break;
			}
			fallthrough;
		case -ERESTARTNOINTR:
                        regs->a0 = regs->orig_a0;
			regs->epc -= 0x4;
			break;
		}
	}

	/* Set up the stack frame */
	ret = setup_rt_frame(ksig, oldset, regs);

	signal_setup_done(ret, ksig, 0);
}

static void do_signal(struct pt_regs *regs)
{
	struct ksignal ksig;

	if (get_signal(&ksig)) {
		/* Actually deliver the signal */
		handle_signal(&ksig, regs);
		return;
	}

	/* Did we come from a system call? */
	if (regs->cause == EXC_SYSCALL) {
		/* Avoid additional syscall restarting via ret_from_exception */
		regs->cause = -1UL;

		/* Restart the system call - no handlers present */
		switch (regs->a0) {
		case -ERESTARTNOHAND:
		case -ERESTARTSYS:
		case -ERESTARTNOINTR:
                        regs->a0 = regs->orig_a0;
			regs->epc -= 0x4;
			break;
		case -ERESTART_RESTARTBLOCK:
                        regs->a0 = regs->orig_a0;
			regs->a7 = __NR_restart_syscall;
			regs->epc -= 0x4;
			break;
		}
	}

	/*
	 * If there is no signal to deliver, we just put the saved
	 * sigmask back.
	 */
	restore_saved_sigmask();
}

/*
 * notification of userspace execution resumption
 * - triggered by the _TIF_WORK_MASK flags
 */
asmlinkage __visible void do_notify_resume(struct pt_regs *regs,
					   unsigned long thread_info_flags)
{
	if (thread_info_flags & _TIF_UPROBE)
		uprobe_notify_resume(regs);

	/* Handle pending signal delivery */
	if (thread_info_flags & (_TIF_SIGPENDING | _TIF_NOTIFY_SIGNAL))
		do_signal(regs);

	if (thread_info_flags & _TIF_NOTIFY_RESUME)
		tracehook_notify_resume(regs);
}

unsigned long __ro_after_init signal_minsigstksz;

void init_rt_signal_env(void);
void __init init_rt_signal_env(void)
{
	rvv_sc_size = sizeof(struct __sc_riscv_v_state) + riscv_vsize;
	/*
	 * Determine the stack space required for guaranteed signal delivery.
	 * The signal_minsigstksz will be populated into the AT_MINSIGSTKSZ entry
	 * in the auxiliary array at process startup.
	 */
	signal_minsigstksz = cal_rt_frame_size();
}
