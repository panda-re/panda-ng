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

/* The declaring or subscribing plugin is going away: its subscriptions are removed, and the
 * events it declared become undeclared (their subscribers PENDING). */
void panda_events_owner_gone(void *owner);

/* Record/replay is starting: false, with a g_malloc'd reason, if an active subscription is not
 * admissible in rr. Re-evaluate admission after a mode change (refused subscriptions may become
 * active, and the other way round). */
bool panda_events_rr_safe(bool replay, char **why);
void panda_events_reevaluate(void);

/* Startup is over: log every subscription still PENDING or MISMATCH, with its reason. False if
 * there is one and panda_events_strict(). It runs once per run: a later call returns the first
 * call's answer. The command line runs it from a machine-init-done notifier (events.c) and exits
 * on false; panda_init defers that (panda_events_defer_startup_check, before qemu_init), runs it
 * after after_machine_init, and returns nonzero on false. A subscription that goes PENDING later
 * is logged when it does. */
bool panda_events_startup_done(void);
void panda_events_defer_startup_check(void);

/* Parse a signature into its canonical forms. False, with a g_malloc'd reason, if malformed.
 * @hashed is the part before `|`, @full the whole; @layout_size the struct size it implies. */
bool panda_event_signature_parse(const char *sig, char **hashed, char **full,
                                 uint32_t *layout_size, char **why);
void panda_event_uuid_derive(const panda_event_uuid *ns, const char *name, const char *hashed,
                             panda_event_uuid *out);

/* `info panda-events` (events-hmp.c; hmp-commands-info.hx, patch 0101). Struct tags rather
 * than QEMU's typedefs, so this header needs nothing from the monitor. */
struct Monitor;
struct QDict;
void hmp_info_panda_events(struct Monitor *mon, const struct QDict *qdict);

#endif
