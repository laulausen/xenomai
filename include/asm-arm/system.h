/*
 * Copyright (C) 2001,2002,2003,2004 Philippe Gerum <rpm@xenomai.org>.
 *
 * ARM port
 *   Copyright (C) 2005 Stelian Pop
 *
 * Xenomai is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _XENO_ASM_ARM_SYSTEM_H
#define _XENO_ASM_ARM_SYSTEM_H

#ifdef __KERNEL__

#include <linux/ptrace.h>
#include <asm-generic/xenomai/system.h>
#include <asm/xenomai/syscall.h>

#define XNARCH_THREAD_STACKSZ	4096

#define xnarch_stack_size(tcb)	((tcb)->stacksize)
#define xnarch_stack_base(tcb)	((tcb)->stackbase)
#define xnarch_stack_end(tcb)	((caddr_t)(tcb)->stackbase - (tcb)->stacksize)
#define xnarch_user_task(tcb)	((tcb)->user_task)
#define xnarch_user_pid(tcb)	((tcb)->user_task->pid)

struct xnthread;
struct task_struct;

typedef struct xnarchtcb {  /* Per-thread arch-dependent block */

	/* Kernel mode side */

#ifdef CONFIG_XENO_HW_FPU
	rthal_fpenv_t fpuenv;
	rthal_fpenv_t *fpup;	/* Pointer to the FPU backup area */
	struct task_struct *user_fpu_owner;
	/*
	 * Pointer the the FPU owner in userspace:
	 * - NULL for RT K threads,
	 * - last_task_used_math for Linux US threads (only current or NULL when MP)
	 * - current for RT US threads.
	 */
	unsigned is_root;
#define xnarch_fpu_ptr(tcb)	((tcb)->fpup)
#else /* !CONFIG_XENO_HW_FPU */
#define xnarch_fpu_ptr(tcb)	NULL
#endif /* CONFIG_XENO_HW_FPU */

	unsigned stacksize;	    /* Aligned size of stack (bytes) */
	unsigned long *stackbase;   /* Stack space */

	/* User mode side */
	struct task_struct *user_task;	    /* Shadowed user-space task */
	struct task_struct *active_task;    /* Active user-space task */
	struct mm_struct *mm;
	struct mm_struct *active_mm;
	struct thread_info ti;		    /* Holds kernel-based thread info */
	struct thread_info *tip;	    /* Pointer to the active thread info (ti or user->thread_info). */
	struct {
		unsigned long pc;
		unsigned long r0;
#ifdef CONFIG_XENO_ARM_EABI
		unsigned long r7;
#endif
	} mayday;

	/* Init block */
	struct xnthread *self;
	int imask;
	const char *name;
	void (*entry)(void *cookie);
	void *cookie;

} xnarchtcb_t;

#define xnarch_fault_regs(d)	((d)->regs)
#define xnarch_fault_trap(d)	((d)->exception)
#define xnarch_fault_code(d)	(0)
#define xnarch_fault_pc(d)	((d)->regs->ARM_pc - (thumb_mode((d)->regs) ? 2 : 4)) /* XXX ? */
#ifndef CONFIG_XENO_HW_FPU
/*
 * It is normal on ARM for user-space support running with a kernel
 * compiled with FPU support to make FPU faults, even on the context
 * of real-time threads which do not otherwise use FPU, so we simply
 * ignore these faults.
 */
#define xnarch_fault_fpu_p(d) (0)
#else /* CONFIG_XENO_HW_FPU */
static inline int xnarch_fault_fpu_p(struct ipipe_trap_data *d)
{
	/* This function does the same thing to decode the faulting instruct as
	   "call_fpe" in arch/arm/entry-armv.S */
	static unsigned copro_to_exc[16] = {
		IPIPE_TRAP_UNDEFINSTR,
		/* FPE */
		IPIPE_TRAP_FPU, IPIPE_TRAP_FPU,
		IPIPE_TRAP_UNDEFINSTR,
#ifdef CONFIG_CRUNCH
		IPIPE_TRAP_FPU, IPIPE_TRAP_FPU, IPIPE_TRAP_FPU,
#else /* !CONFIG_CRUNCH */
		IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR,
#endif /* !CONFIG_CRUNCH */
		IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR,
#ifdef CONFIG_VFP
		IPIPE_TRAP_VFP, IPIPE_TRAP_VFP,
#else /* !CONFIG_VFP */
		IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR,
#endif /* !CONFIG_VFP */
		IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR,
		IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR,
	};
	unsigned instr, exc, cp;
	char *pc;

	if (d->exception == IPIPE_TRAP_FPU)
		return 1;

#ifdef CONFIG_VFP
	if (d->exception == IPIPE_TRAP_VFP)
		goto trap_vfp;
#endif

	/*
	 * When an FPU fault occurs in user-mode, it will be properly
	 * resolved before __ipipe_report_trap() is called.
	 */
	if (d->exception != IPIPE_TRAP_UNDEFINSTR || xnarch_fault_um(d))
		return 0;

	pc = (char *) xnarch_fault_pc(d);
	if (unlikely(thumb_mode(d->regs))) {
		unsigned short thumbh, thumbl;

#if defined(CONFIG_ARM_THUMB) && __LINUX_ARM_ARCH__ >= 6 && defined(CONFIG_CPU_V7)
#if __LINUX_ARM_ARCH__ < 7
		if (cpu_architecture() < CPU_ARCH_ARMv7)
#else
		if (0)
#endif /* arch < 7 */
#endif /* thumb && arch >= 6 && cpu_v7 */
			return 0;

		thumbh = *(unsigned short *) pc;
		thumbl = *((unsigned short *) pc + 1);

		if ((thumbh & 0x0000f800) < 0x0000e800)
			return 0;
		instr = (thumbh << 16) | thumbl;

#ifdef CONFIG_NEON
		if ((instr & 0xef000000) == 0xef000000
		    || (instr & 0xff100000) == 0xf9000000)
			goto trap_vfp;
#endif
	} else {
		instr = *(unsigned *) pc;

#ifdef CONFIG_NEON
		if ((instr & 0xfe000000) == 0xf2000000
		    || (instr & 0xff100000) == 0xf4000000)
			goto trap_vfp;
#endif
	}

	if ((instr & 0x0c000000) != 0x0c000000)
		return 0;

	cp = (instr & 0x00000f00) >> 8;
#ifdef CONFIG_IWMMXT
	/* We need something equivalent to _TIF_USING_IWMMXT for Xenomai kernel
	   threads */
	if (cp <= 1) {
		d->exception = IPIPE_TRAP_FPU;
		return 1;
	}
#endif

	exc = copro_to_exc[cp];
#ifdef CONFIG_VFP
	if (exc == IPIPE_TRAP_VFP) {
	  trap_vfp:
		/* If an exception is pending, the VFP fault is not really an
		   "FPU unavailable" fault, so we return undefinstr in that
		   case, the nucleus will let linux handle the fault. */
		exc = rthal_vfp_fmrx(FPEXC);
		if (exc & (FPEXC_EX|FPEXC_DEX)
		    || ((exc & FPEXC_EN) && rthal_vfp_fmrx(FPSCR) & FPSCR_IXE))
			exc = IPIPE_TRAP_UNDEFINSTR;
		else
			exc = IPIPE_TRAP_VFP;
	}
#endif
	d->exception = exc;
	return exc != IPIPE_TRAP_UNDEFINSTR;
}
#endif /* CONFIG_XENO_HW_FPU */
/* The following predicates are only usable over a regular Linux stack
   context. */
#define xnarch_fault_pf_p(d)	((d)->exception == IPIPE_TRAP_ACCESS)
#define xnarch_fault_bp_p(d)	((current->ptrace & PT_PTRACED) &&	\
				 ((d)->exception == IPIPE_TRAP_BREAK))

#define xnarch_fault_notify(d) (!xnarch_fault_bp_p(d))

#else /* !__KERNEL__ */

#include <nucleus/system.h>
#include <bits/local_lim.h>

#endif /* __KERNEL__ */

#endif /* !_XENO_ASM_ARM_SYSTEM_H */

// vim: ts=4 et sw=4 sts=4
