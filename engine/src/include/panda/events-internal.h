/*
 * The event registry's engine-side interface: what callbacks.c and the rr start paths call.
 * Not for plugins.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_EVENTS_INTERNAL_H
#define PANDA_EVENTS_INTERNAL_H

#include <glib.h>
#include "panda/events.h"

/* A core callback type, declared as "cb.<name>" in PANDA_EVENT_NS (events-core.c). Types with
 * no modes (no trampoline) are not declared. */
typedef struct PandaCoreEventDef {
    int type;                   /* panda_cb_type */
    const char *name;           /* "before_block_exec" */
    const char *signature;
    bool rr_mutate;             /* mutators admitted in rr (the hypercall answer is logged) */
} PandaCoreEventDef;

extern const PandaCoreEventDef panda_core_events[];
extern const size_t panda_core_events_count;
uint32_t panda_core_event_modes(int type);      /* panda_callback_supported_modes */

/* Provided by callbacks.c: a plugin's name for listings, and the core callback list of a type,
 * appended to a listing. */
const char *panda_event_owner_name(void *owner);
void panda_core_event_describe_subs(int type, GString *out, bool json);
/* One subscription line (or JSON object) of a listing, for those core lists. */
void panda_events_describe_sub(GString *out, bool json, const char *owner, const char *state,
                               const char *reason, const char *kind, uint32_t modes);

panda_event_id panda_core_event(int type);      /* NULL for an undeclared (removed) type */

/* Provided by callbacks.c: false before the machine has vCPUs. Then nothing dispatches, and
 * QEMU's RCU thread cannot run callbacks yet (it takes the BQL, not initialized before
 * qemu_init): what would be freed after a grace period is freed at once. */
bool panda_events_rcu_ready(void);

/* The declaring or subscribing plugin is going away: its subscriptions are removed, and the
 * events it declared become undeclared (their subscribers PENDING). */
void panda_events_owner_gone(void *owner);

/* Record/replay is starting: false, with a g_malloc'd reason, if an active subscription is not
 * admissible in rr. Re-evaluate admission after a mode change (refused subscriptions may become
 * active, and the other way round). */
bool panda_events_rr_safe(bool replay, char **why);
void panda_events_reevaluate(void);

/*
 * For publishers that dispatch themselves, with their own per-subscription filters (the syscall
 * events, syscall-events.c). The watcher runs with the registry locked whenever @ev's active
 * subscriptions change (subscribe, unsubscribe, enable, priority, data size, admission), and once
 * when it is set; it must not call back into the registry's locking calls.
 */
typedef void (*panda_event_watcher_fn)(panda_event_id ev, void *opaque);
void panda_event_set_watcher(panda_event_id ev, panda_event_watcher_fn fn, void *opaque);

typedef struct PandaSubInfo {
    panda_event_fn fn;
    void *ctx;
    uint32_t flags;             /* PANDA_SUB_* */
    int priority;
    uint64_t seq;               /* subscription order, across events */
    size_t data_size;
    void *owner;
} PandaSubInfo;

/* With the registry locked (in a watcher, or panda_sub_with_lock's @fn): */
unsigned panda_event_active_subs(panda_event_id ev, panda_sub *out, unsigned max);
void panda_sub_info(panda_sub sub, PandaSubInfo *out);
bool panda_event_has_sub(panda_event_id ev, panda_sub sub);     /* subscribed, any state */
/* Run @fn with the registry locked if @sub is still subscribed; false if it is not. */
bool panda_sub_with_lock(panda_sub sub, void (*fn)(panda_sub, panda_event_id, void *),
                         void *opaque);

/* Parse a signature into its canonical forms. False, with a g_malloc'd reason, if malformed.
 * @hashed is the part before `|`, @full the whole; @layout_size the struct size it implies. */
bool panda_event_signature_parse(const char *sig, char **hashed, char **full,
                                 uint32_t *layout_size, char **why);
void panda_event_uuid_derive(const panda_event_uuid *ns, const char *name, const char *hashed,
                             panda_event_uuid *out);

#endif
