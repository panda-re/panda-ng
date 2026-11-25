/*
 * Between the injection sessions (inject.c) and the system-call entry sites (syscall-pair*.c).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_INJECT_INT_H
#define PANDA_INJECT_INT_H

#include "qemu/atomic.h"

struct CPUState;

/*
 * An entry delivery's hold state, on the vCPU's thread: 0 outside an entry callback, 1 inside
 * one that can be held, 2 once panda_inject_hold() held it (the site then puts the task back at
 * its trap instruction and leaves the trap undone).
 */
extern __thread uint8_t panda_sc_hold;
/* the call held (2), for the site to release its pending record once the delivery is over */
extern __thread struct PandaSyscallCall *panda_sc_hold_call;
/* At the site, after the delivery: true if it was held (its record is released then) */
bool panda_sc_hold_end(struct CPUState *cpu);

/* Held entries whose thread has not executed its trap again yet: not 0 while any is pending */
extern uint32_t panda_inject_resumed_pending;
/* PANDA_SYSCALL_RESUMED if the entry at @pc on @cpu is a held one coming back (consumed) */
uint32_t panda_inject_resumed_take(struct CPUState *cpu, uint64_t pc);

static inline uint32_t panda_inject_entry_flags(struct CPUState *cpu, uint64_t pc)
{
    return unlikely(qatomic_read(&panda_inject_resumed_pending))
           ? panda_inject_resumed_take(cpu, pc) : 0;
}

#endif
