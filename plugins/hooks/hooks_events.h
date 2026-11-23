/*
 * The hooks plugin's events, for consumers of the PANDA event registry (panda/events.h).
 *
 * hooks.hit: a hook added with add_hook() matched (its address, asid and kernel/user mode) and
 * its function has just run. Emitted once per hook per hit, on the vCPU thread, only if
 * something subscribed. Observers only: a subscriber must not change the guest, nor the hook.
 *   cpu   the CPUState
 *   tb    the translated block that holds the hooked instruction (as the hook function got it)
 *   pc    the hooked address
 *   hook  the plugin's struct hook (hooks_int_fns.h), valid until the subscriber returns. If
 *         the hook function disabled it, it is removed after this event.
 *
 *     panda_event_id ev = panda_hooks_hit_event();
 *     panda_sub sub = panda_subscribe(self, ev, on_hit, ctx, PANDA_MODE_ANALYSIS,
 *                                     PANDA_SUB_OBSERVE);
 *     static void on_hit(void *ctx, void *args, void *data)
 *     {
 *         struct panda_hooks_hit_args *a = args;
 *         ...
 *     }
 *
 * The event can be found before the hooks plugin loads: the subscription is PENDING until then.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_HOOKS_EVENTS_H
#define PANDA_HOOKS_EVENTS_H

#include "panda/events.h"

#define PANDA_HOOKS_HIT_NAME "hooks.hit"
#define PANDA_HOOKS_HIT_ARGS(F) \
    PANDA_EVENT_PTR(F, cpu, CPUState) PANDA_EVENT_PTR(F, tb, TranslationBlock) F(pc, u64) \
    F(hook, ptr)
PANDA_EVENT_STRUCT(panda_hooks_hit_args, PANDA_HOOKS_HIT_ARGS);
#define PANDA_HOOKS_HIT_SIGNATURE PANDA_EVENT_SIGNATURE(PANDA_HOOKS_HIT_ARGS)

static inline panda_event_id panda_hooks_hit_event(void)
{
    return panda_event_find(&PANDA_EVENT_NS, PANDA_HOOKS_HIT_NAME, PANDA_HOOKS_HIT_SIGNATURE);
}

#endif
