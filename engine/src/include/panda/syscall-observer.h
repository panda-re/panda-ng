/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PANDA_SYSCALL_OBSERVER_H
#define PANDA_SYSCALL_OBSERVER_H

#include <stdbool.h>
#include <stdint.h>
typedef struct CPUState CPUState;

#ifdef __cplusplus
extern "C" {
#endif

/* Opt-in system-mode observation, not the linux-user QEMU plugin API.
 * One exclusive consumer, shared with Penguin and the QEMU plugin bridge.
 * Initial supported ABI: Linux x86_64. No guest mutation is propagated.
 */
#define PANDA_SYSCALL_OBSERVER_X86_64 0
#define PANDA_SYSCALL_OBSERVER_ENTER 1
#define PANDA_SYSCALL_OBSERVER_RETURN 2

typedef struct PandaSyscallObservation {
    int abi;
    int64_t num;
    uint64_t args[6];
    uint64_t pc; /* entry: after syscall instruction; return: user return PC */
    int64_t ret;
    bool restart;
} PandaSyscallObservation;

typedef struct PandaSyscallObserverOps {
    /* NULL cookie means no return tracking. A non-NULL cookie requests tracking
     * only when the watch includes RETURN. Host-only cookie is always dropped.
     */
    void *(*enter)(CPUState *, const PandaSyscallObservation *, void *opaque);
    /* Called before drop, including restart notifications; never frees cookie. */
    void (*ret)(CPUState *, const PandaSyscallObservation *, void *cookie, void *opaque);
    /* Exactly once per non-NULL cookie: completion/restart/cancel/eviction/detach.
     * Runs outside the core lock; must not reenter registration or guest execution.
     */
    void (*drop)(void *cookie, void *opaque);
    /* Optional return correlation predicate. False retains the pending cookie
     * and suppresses ret for this boundary (e.g. a vfork child's return through
     * its creator's slot). Must not mutate guest state or free the cookie.
     * Restarts bypass this predicate: ret receives restart=true and drop follows,
     * preserving the existing retrap/new-cookie contract. NULL accepts all.
     * Source-compatible trailing field; native clients must rebuild for this ABI.
     */
    bool (*accept_return)(CPUState *, const PandaSyscallObservation *,
                          void *cookie, void *opaque);
} PandaSyscallObserverOps;

/* Provision/seal ANALYSIS before loading the owner (panda_configure_capabilities
 * for embedding, or the startup CLI policy). Register before vCPU initialization,
 * or main thread/BQL with every vCPU stopped or
 * paused afterward (never inside a vCPU callback). Returns false (no partial change)
 * on unsafe context, invalid owner/ops or exclusive conflict. Register only a
 * live PANDA plugin handle. Unregister drains cookies while old callbacks are
 * mapped and preserves other consumers. Unload first unregisters its owner;
 * unsafe detach refuses unload without uninit/dlclose or partial removal.
 * Late queue/vCPU registration fails, rather than silently falling back.
 */
bool panda_syscall_observer_register(void *owner, const PandaSyscallObserverOps *ops, void *opaque);
/* RETURN requires ENTER (the entry establishes its cookie); zero unwatches. */
bool panda_syscall_observer_watch(void *owner, int abi, int64_t num, unsigned flags);
bool panda_syscall_observer_unregister(void *owner);

#ifdef __cplusplus
}
#endif
#endif
