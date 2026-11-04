/*
 * PANDA syscall events: syscall.enter and syscall.return (docs/design/syscalls.md, L1).
 *
 * Both are events on the registry (panda/events.h) in PANDA_EVENT_NS, declared by the engine,
 * with PandaSyscall as their argument struct and with instances: a subscription's instance data
 * (panda_sub_set_data_size) is zeroed when a call first reaches it and is the same bytes at that
 * call's return.
 *
 *     panda_sub sub = panda_subscribe(self, panda_syscall_event(PANDA_SC_ENTER), on_call, ctx,
 *                                     PANDA_MODE_ANALYSIS, PANDA_SUB_OBSERVE);
 *     panda_sub_set_data_size(sub, sizeof(struct my_call));
 *     panda_syscall_watch(sub, SYSCALL_ABI_X86_64, 257, PANDA_SC_ENTER | PANDA_SC_RETURN);
 *
 * A subscription is called only for the numbers it watches. A syscall.enter subscription may
 * watch entries and returns: its function then runs at both, with the same instance data, and
 * PANDA_SC_AT_RETURN set at the return. A syscall.return subscription watches returns only.
 * With no subscription, the engine has no syscall consumer and the trap costs nothing.
 *
 * Order: PANDA_SUB_OBSERVE_ORIGINAL observers, then mutators by priority, then observers. A
 * mutator that faults or injects holds the call: it resumes at that mutator (PANDA_SC_RESUMED),
 * then reaches the subscriptions after it. Unsubscribing a mutator that holds calls releases them
 * as if it had returned without changes.
 *
 * Mutators change the call through the calls below, only from their own callback for that call.
 * Each returns false, with panda_syscall_refusal() saying why, when it is not allowed: from an
 * observer, at the wrong phase, under KVM, or during record/replay (mutations of syscalls are
 * not logged yet; mutator subscriptions are refused there too). Nothing is skipped silently.
 *
 * Exit calls (exit, exit_group) are not tracked to a return unless the subscription watches the
 * return with PANDA_SC_NORETURN_TOO: a dead task's key can be reused by a new task, whose return
 * would be paired with it.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_SYSCALL_EVENTS_H
#define PANDA_SYSCALL_EVENTS_H

#include <stdbool.h>
#include <stdint.h>
#include "panda/events.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The event's arguments. ABIs are the values of enum qemu_plugin_syscall_abi. */
typedef struct PandaSyscall {
    uint32_t size;          /* sizeof(PandaSyscall) as the engine built it */
    int32_t abi;
    int64_t num;
    uint64_t args[6];
    uint64_t pc;            /* past the trap instruction */
    uint64_t call_id;       /* unique per call; the same at entry and return */
    int64_t ret;            /* return: the value (-errno on error) */
    uint32_t flags;         /* PANDA_SC_* below */
} PandaSyscall;

/* The signature both events are declared with (args as six u64 fields). */
#define PANDA_SYSCALL_SIGNATURE \
    "abi:i32,num:i64,arg0:u64,arg1:u64,arg2:u64,arg3:u64,arg4:u64,arg5:u64," \
    "pc:u64,call_id:u64,ret:i64,flags:u32"

/* PandaSyscall.flags */
enum {
    PANDA_SC_AT_RETURN = 1u << 0,   /* this is the call's return */
    PANDA_SC_RESTART   = 1u << 1,   /* return: the kernel restarts the call (a new entry follows) */
    PANDA_SC_RESUMED   = 1u << 2,   /* the call this subscription held is back */
    PANDA_SC_INJECTED  = 1u << 3,   /* resumed after its injected call: ret is that call's value */
    PANDA_SC_SKIPPED   = 1u << 4,   /* the call is skipped; ret is the value the task gets */
};

/* panda_syscall_watch @what */
enum {
    PANDA_SC_ENTER        = 1u << 0,
    PANDA_SC_RETURN       = 1u << 1,
    PANDA_SC_NORETURN_TOO = 1u << 2,    /* with RETURN: track exit calls' returns too */
};

/* PANDA_SC_ENTER: syscall.enter; PANDA_SC_RETURN: syscall.return. */
panda_event_id panda_syscall_event(unsigned which);

/* @num -1: every number of @abi. Watches add up; unwatch removes the bits given. */
bool panda_syscall_watch(panda_sub sub, int abi, int64_t num, unsigned what);
bool panda_syscall_unwatch(panda_sub sub, int abi, int64_t num, unsigned what);
/* From an entry callback: deliver this call's return to the same subscription. */
bool panda_syscall_want_return(PandaSyscall *sc);

/* Mutators, from their own callback. */
bool panda_syscall_skip(PandaSyscall *sc, int64_t ret);                   /* entry */
bool panda_syscall_set_arg(PandaSyscall *sc, unsigned i, uint64_t value); /* entry */
bool panda_syscall_set_ret(PandaSyscall *sc, int64_t ret);  /* return, or a skipped entry */
/* Have the task take the fault a user access to @addr would, then resume the call here. */
bool panda_syscall_fault(PandaSyscall *sc, uint64_t addr, bool write);
/* Run call @num(@args) in the task first, then resume this one here with its value. */
bool panda_syscall_inject(PandaSyscall *sc, int64_t num, const uint64_t args[6]);

/* Why the last call above refused, on this thread (NULL after a success). */
const char *panda_syscall_refusal(void);

#ifdef __cplusplus
}
#endif

#endif
