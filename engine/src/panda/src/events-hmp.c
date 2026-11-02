/*
 * `info panda-events`: the event registry's listing in the human monitor (hmp-commands-info.hx,
 * engine patch 0102). The same text as panda_events_describe(false): every event with its UUID,
 * publisher, signature and namespace, then each subscription with its owner, state, kind, modes
 * and reason.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "monitor/monitor.h"
#include "panda/events-internal.h"

void hmp_info_panda_events(Monitor *mon, const QDict *qdict)
{
    g_autofree char *text = panda_events_describe(false);

    monitor_puts(mon, *text ? text : "no PANDA events\n");
}
