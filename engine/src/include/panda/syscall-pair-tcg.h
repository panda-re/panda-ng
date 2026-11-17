/*
 * Engine-internal: the target sites of the system-call pairing (panda/syscall-pair.h).
 *
 * Translation time:
 *   panda_sc_enter_inline(form)   the form is declared: its trap site emits
 *                                 panda_sc_gen_enter() before the plain trap helper or exception
 *   panda_sc_return_inline()      some return is wanted: the return-to-user sites emit
 *                                 panda_sc_gen_return() after the plain return helper
 * Both are false in a run without a declaration, so its translated code is unchanged.
 *
 * Run time, from C trap sites (forms whose trap is taken in C: X86_INT80, A32_SVC):
 *   panda_sc_enter_c(cpu, form, pc, len)   if panda_sc_enter_inline(form)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_SYSCALL_PAIR_TCG_H
#define PANDA_SYSCALL_PAIR_TCG_H

#include "panda/syscall-pair.h"

#ifndef CONFIG_USER_ONLY
extern uint32_t panda_sc_forms;         /* declared forms (1u << PANDA_SYSCALL_FORM_*) */
extern bool panda_sc_armed;             /* return sites check inline */

static inline bool panda_sc_enter_inline(unsigned form)
{
    return __atomic_load_n(&panda_sc_forms, __ATOMIC_RELAXED) & (1u << form);
}

static inline bool panda_sc_return_inline(void)
{
    return __atomic_load_n(&panda_sc_armed, __ATOMIC_RELAXED);
}

void panda_sc_enter_c(CPUState *cpu, unsigned form, uint64_t pc, uint32_t len);

/* Reset, snapshot load and fast restore, with the vCPUs stopped (panda_analysis_reset): every
 * pending call and task-key cache is dropped. */
void panda_syscall_pair_reset(void);

/*
 * Emitters (tcg-op context). gen_enter: at a declared form's trap, in its mode, with the PC
 * synchronized (x86: EIP at the SYSCALL; AArch64: PC past the SVC; RISC-V, MIPS and PowerPC:
 * the PC at the trap) and @nr the number
 * register's TCG global (of the target's word size): the number's want byte, then a call of the
 * entry function if it is not 0. gen_return: after the plain return helper.
 */
void panda_sc_gen_return(void);
#ifdef TCG_TCG_OP_H
void panda_sc_gen_enter(unsigned form, uint32_t len, TCGv nr);
#endif
#endif

#endif /* PANDA_SYSCALL_PAIR_TCG_H */
