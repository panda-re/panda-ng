/*
 * PANDA hook points called from QEMU core and target code.
 *
 * With --enable-panda these are the dispatchers in panda/src/cb-support.c. Without it they are
 * empty inlines that return "no callback acted", so a default build compiles every hook site
 * away and is identical to one without PANDA.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_HOOKS_H
#define PANDA_HOOKS_H

#include "hw/core/cpu.h"

/* Bits of panda_loop_hooks: what the CPU loop calls into PANDA for. Nothing set, no call. */
#define PANDA_LOOP_EXCEPTION 1      /* a before_handle_exception callback is registered */
#define PANDA_LOOP_FLUSH     2      /* panda_do_flush_tb() asked for a flush not yet done */

#ifdef CONFIG_PANDA

/* Called before preparing a recording or replay and again after the mode switch,
 * before any guest execution. They never refuse: after the switch they warn about
 * each enabled callback that dispatch will skip until the recording or replay ends. */
bool panda_callbacks_replay_safe(Error **errp);
bool panda_callbacks_record_safe(Error **errp);

struct qemu_plugin_tb;

extern uint32_t panda_loop_hooks;

static inline bool panda_loop_wants(uint32_t bits)
{
    return unlikely(qatomic_read(&panda_loop_hooks) & bits);
}

/* A new vCPU's tcg_cflags: no chaining after panda_disable_tb_chaining() */
void panda_vcpu_init_cflags(CPUState *cpu);

void panda_callbacks_before_block_translate(CPUState *cpu, uint64_t pc);
void panda_callbacks_after_block_translate(CPUState *cpu, TranslationBlock *tb);
int32_t panda_callbacks_before_handle_exception(CPUState *cpu, int32_t exception_index);
/* true: a plugin vetoed the change, leave the old value in place */
bool panda_callbacks_asid_changed(CPUState *cpu, uint64_t oldval, uint64_t newval);
/* The guest writes @val (@size bytes: 4 or 8) to the register at @reg, which panda_current_asid
 * reads (x86 CR3, arm TTBRs, riscv satp, mips EntryHi). If the process identifier changes,
 * asid_changed runs with the old and new identifiers. true: a plugin vetoed the change, leave the
 * register alone; false: the caller writes it. */
bool panda_asid_reg_write(CPUState *cpu, void *reg, unsigned size, uint64_t val);
/* true: a plugin handled the hypercall and set the guest's registers itself */
bool panda_callbacks_guest_hypercall(CPUState *cpu);
/* false after panda_disable_tb_chaining(): every TB returns to cpu_exec (CF_NO_GOTO_TB and
 * CF_NO_GOTO_PTR in each vCPU's tcg_cflags). The block-exec callbacks don't need it (they run from
 * the block's own code); kept for plugins that want it. */
bool panda_tb_chaining_enabled(void);
/* Before each TB lookup: unload plugins that asked to be, and flush the TBs if panda_do_flush_tb()
 * asked (it leaves the CPU loop, which then looks the TB up again) */
void panda_callbacks_before_find_fast(void);
/* Each main-loop iteration (pandare2 runs its queued snapshot/revert work here) */
void panda_callbacks_main_loop_wait(void);
/* After load_snapshot() restored a snapshot (loadvm, panda_revert, an rr start) */
void panda_callbacks_after_loadvm(CPUState *env);
/* qemu_loadvm_state() is about to load RAM (loadvm, an incoming migration): the RAM it writes
 * isn't logged dirty, and a failed load leaves it half written */
void panda_before_loadvm(void);
/* Stopped-vCPU destructive restore/reset boundary: cancel transient analysis,
 * independently of whether the transition subsequently succeeds. */
void panda_analysis_reset(void);

#else

static inline bool panda_callbacks_replay_safe(Error **errp) { return true; }
static inline bool panda_callbacks_record_safe(Error **errp) { return true; }

static inline bool panda_loop_wants(uint32_t bits)
{
    return false;
}
static inline void panda_vcpu_init_cflags(CPUState *cpu) {}
static inline void panda_callbacks_before_block_translate(CPUState *cpu, uint64_t pc) {}
static inline void panda_callbacks_after_block_translate(CPUState *cpu, TranslationBlock *tb) {}
static inline int32_t panda_callbacks_before_handle_exception(CPUState *cpu,
                                                              int32_t exception_index)
{
    return exception_index;
}
static inline bool panda_tb_chaining_enabled(void)
{
    return true;
}
static inline void panda_callbacks_before_find_fast(void) {}
static inline void panda_callbacks_main_loop_wait(void) {}
static inline void panda_callbacks_after_loadvm(CPUState *env) {}
static inline void panda_before_loadvm(void) {}
static inline void panda_analysis_reset(void) {}
static inline bool panda_callbacks_asid_changed(CPUState *cpu, uint64_t oldval, uint64_t newval)
{
    return false;
}
static inline bool panda_asid_reg_write(CPUState *cpu, void *reg, unsigned size, uint64_t val)
{
    return false;
}
static inline bool panda_callbacks_guest_hypercall(CPUState *cpu)
{
    return false;
}

#endif

#endif
