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

#ifdef CONFIG_PANDA

struct qemu_plugin_tb;

void panda_callbacks_before_block_exec(CPUState *cpu, TranslationBlock *tb);
void panda_callbacks_after_block_exec(CPUState *cpu, TranslationBlock *tb, uint8_t exitCode);
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
/* false after panda_disable_tb_chaining(): every TB returns to cpu_exec. The block-exec callbacks
 * don't need it (they run from the block's own code); kept for plugins that want it. */
bool panda_tb_chaining_enabled(void);
/* A longjmp left the block a vCPU was running (a fault, an exception its last instruction raised,
 * cpu_loop_exit_noexc): its after_block_exec runs now, with TB_EXIT_REQUESTED, before the exception
 * is delivered. Nothing pending (the usual case: no block-exec callback), nothing done. */
void panda_callbacks_block_longjmp(CPUState *cpu);
/* Before each TB lookup: unload plugins that asked to be, and flush the TBs if panda_do_flush_tb()
 * asked (it leaves the CPU loop, which then looks the TB up again) */
void panda_callbacks_before_find_fast(void);
/* Each main-loop iteration (pandare2 runs its queued snapshot/revert work here) */
void panda_callbacks_main_loop_wait(void);
/* After load_snapshot() restored a snapshot (loadvm, panda_revert, an rr start) */
void panda_callbacks_after_loadvm(CPUState *env);

#else

static inline void panda_callbacks_before_block_exec(CPUState *cpu, TranslationBlock *tb) {}
static inline void panda_callbacks_after_block_exec(CPUState *cpu, TranslationBlock *tb,
                                                    uint8_t exitCode) {}
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
static inline void panda_callbacks_block_longjmp(CPUState *cpu) {}
static inline void panda_callbacks_before_find_fast(void) {}
static inline void panda_callbacks_main_loop_wait(void) {}
static inline void panda_callbacks_after_loadvm(CPUState *env) {}
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
