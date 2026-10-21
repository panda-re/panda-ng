/*
 * TCG address hooks: call a host function before a guest instruction.
 *
 * A hook is keyed by the guest virtual address of an instruction.  The
 * translator emits a direct helper call at that instruction when it
 * translates it, so code without hooks runs unchanged and a hooked
 * instruction costs one helper call.  Inserting a hook at a new address
 * invalidates only the translated blocks that contain it.
 *
 * Hooks match the guest virtual address in every address space.  A
 * callback that only cares about one process filters on its own (e.g. by
 * page-table root).  While hooks are in use, CF_PCREL is turned off, so a
 * translated block is never reused at a virtual address other than the
 * one it was translated for.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ACCEL_TCG_HOOKS_H
#define ACCEL_TCG_HOOKS_H

#include "exec/vaddr.h"
#include "hw/core/cpu.h"
#include "tcg/tcg-cond.h"

typedef struct TCGHook TCGHook;

/*
 * Called on the vCPU thread, before the instruction at @pc executes.
 * General-purpose registers are up to date in the CPU state and may be
 * written.  The program counter and other lazily computed state (x86
 * flags, arm condexec) are not: call tcg_hook_restore_state() first if
 * the callback needs them.  The callback may also leave the instruction:
 * tcg_hook_set_pc(), or raise a guest exception with
 * cpu_loop_exit_restore(cpu, tcg_hook_retaddr(cpu)).
 */
typedef void (*TCGHookFn)(CPUState *cpu, vaddr pc, void *opaque);

/*
 * Insert a hook.  Callable from any thread, including from a hook
 * callback.  A hook at an address that never had one takes effect once
 * the translated blocks containing it are invalidated: before the next
 * instruction runs if inserted before the vCPUs start, otherwise at the
 * next point all vCPUs leave the translated code (the inserting vCPU is
 * kicked).  An address that had hooks before needs no invalidation.
 */
TCGHook *tcg_hook_insert(vaddr pc, TCGHookFn fn, void *opaque);

/*
 * A condition tested by the generated code before a guarded hook is
 * called: (value & mask) <cond> @value, on the low @size bytes (4 or 8)
 * of a guest register or of a host variable.  For a 4-byte compare the
 * value is sign-extended under a signed condition, zero-extended
 * otherwise.  A mask of 0 means no mask.
 */
typedef enum {
    TCG_HOOK_GUARD_REG,                 /* gdbstub register @regno */
    TCG_HOOK_GUARD_HOST,                /* the variable at @host */
} TCGHookGuardKind;

typedef struct TCGHookGuard {
    TCGHookGuardKind kind;
    int regno;
    const void *host;
    unsigned size;
    TCGCond cond;                       /* EQ NE LT GE LE GT LTU GEU LEU GTU */
    uint64_t mask;
    uint64_t value;
    bool compiled;                      /* set by the translator */
} TCGHookGuard;

/*
 * Insert a hook that is called only when every guard holds.  A guard on
 * a register that is not held in one field of the CPU state is evaluated
 * in C instead, just before the callback.  A host variable must stay
 * valid until the hook is removed.  Unlike tcg_hook_insert(), this always
 * invalidates the translated blocks containing @pc.
 */
TCGHook *tcg_hook_insert_guarded(vaddr pc, TCGHookFn fn, void *opaque,
                                 const TCGHookGuard *guards, unsigned n);

/*
 * Where gdbstub register @regno lives: its offset from cpu_env(@cpu) and
 * size, if it is held in a single field.  Found by writing the register
 * through the gdbstub and checking that exactly one field changes (the
 * value is restored), once per CPU class; so ask only about registers
 * whose writes have no side effects, such as general-purpose registers.
 */
bool tcg_register_location(CPUState *cpu, int regno, ptrdiff_t *offset,
                           unsigned *size);

/*
 * Remove a hook.  The callback is not called again once this returns
 * (from a callback, other hooks at the same address in the same pass may
 * still run).  The address keeps a cheap inactive check in translated
 * code until the next TB flush.
 */
void tcg_hook_remove(TCGHook *hook);

/* Number of hooks at @pc (0 if none). */
int tcg_hook_count(vaddr pc);

/* From a callback: host return address into the hooked block. */
static inline uintptr_t tcg_hook_retaddr(CPUState *cpu)
{
    return cpu->tcg_hook_ra;
}

/* From a callback: make PC and lazily computed state exact. */
void tcg_hook_restore_state(CPUState *cpu);

/* From a callback: skip or redirect, resuming at @pc. */
G_NORETURN void tcg_hook_set_pc(CPUState *cpu, vaddr pc);

#endif
