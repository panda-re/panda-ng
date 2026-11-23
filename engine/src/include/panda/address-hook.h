/*
 * PANDA address hooks: call a function before the guest executes the instruction at a virtual
 * address. This is the engine's TCG address hook (accel/tcg/hooks.h) under PANDA names, for
 * plugins: the hooks plugin's add_hook() is built on it.
 *
 * The translator emits a direct call at a hooked instruction, so code without hooks runs
 * unchanged, and the page holding a hook is translated for its virtual address (no CF_PCREL
 * sharing between mappings). A hook matches its address in every address space: the callback
 * filters by process itself. Inserting the first hook at an address invalidates the blocks that
 * contain it; one at an address that had a hook before needs no invalidation. TCG only: under
 * KVM a hook is accepted and never called.
 *
 * In the callback, general-purpose registers are current and may be written. The program
 * counter and lazily computed state (x86 flags) are not; panda_current_pc() reads the value the
 * block left there. The callback runs on the vCPU thread inside an RCU read-side critical
 * section: memory it reads that another thread frees must be freed with panda_free_rcu().
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_ADDRESS_HOOK_H
#define PANDA_ADDRESS_HOOK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CPUState;
struct TranslationBlock;

typedef struct PandaAddressHook PandaAddressHook;

/* @pc is the hooked address, @opaque what panda_address_hook_insert was given. */
typedef void (*panda_address_hook_fn)(struct CPUState *cpu, uint64_t pc, void *opaque);

/* Insert a hook. Callable from any thread, a hook callback included. NULL if it cannot hook. */
PandaAddressHook *panda_address_hook_insert(uint64_t pc, panda_address_hook_fn fn, void *opaque);

/* Remove a hook (NULL is ignored). Callable from any thread, a hook callback included. Once this
 * returns the hook is not called again, except that a pass over the hooks at its address already
 * under way on another vCPU may still call it; free its opaque with panda_free_rcu(). */
void panda_address_hook_remove(PandaAddressHook *hook);

/* From a hook callback: the translated block that holds the hooked instruction. NULL outside a
 * callback. Valid until the callback returns. */
struct TranslationBlock *panda_address_hook_tb(struct CPUState *cpu);

/* free(@ptr) once every vCPU has left the callbacks it is running now (an RCU grace period).
 * The engine does the free, so the caller's code may be unloaded before it happens. */
void panda_free_rcu(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
