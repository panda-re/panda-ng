/*
 * What replay allows: one table, consulted when replay starts (replay_start_check) and when a
 * feature is requested (replay_feature_check). See docs/design/capabilities.md.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "system/replay.h"
#include "system/capabilities.h"
#include "system/replay-conflicts.h"
#include "system/panda-embed.h"
#include "panda/hooks.h"

typedef enum ReplayPolicy {
    REPLAY_ALLOWED,         /* works in replay as it does live */
    REPLAY_FROM_LOG,        /* the log supplies it; live sources only observe */
    REPLAY_REFUSED,         /* refused while replaying */
} ReplayPolicy;

typedef struct ReplayConflict {
    const char *name;
    ReplayPolicy policy;
    const char *why;        /* for REPLAY_REFUSED: the refusal's reason */
    /* At replay start: false, with the reason, if the feature is in use in a way replay refuses. */
    bool (*start_ok)(Error **errp);
} ReplayConflict;

static bool record_not_active(Error **errp)
{
    if (replay_mode == REPLAY_MODE_RECORD) {
        error_setg(errp, "a recording is in progress");
        return false;
    }
    return true;
}

/* Mutating callbacks and a KVM hypercall consumer can't be replayed; observers can. */
static bool mutators_replayable(Error **errp)
{
    return panda_callbacks_replay_safe(errp) && panda_hypercall_replay_safe(errp);
}

static const ReplayConflict replay_conflicts[REPLAY_FEATURE__MAX] = {
    [REPLAY_FEATURE_RECORD] = {
        "record", REPLAY_REFUSED, "a replay can't record", record_not_active,
    },
    [REPLAY_FEATURE_GUEST_MUTATION] = {
        "guest-visible mutation", REPLAY_REFUSED,
        "replay applies only the writes the recording logged", mutators_replayable,
    },
    [REPLAY_FEATURE_FAST_RESTORE] = {
        "fast restore", REPLAY_REFUSED,
        "a restore rewrites RAM and device state outside the log", NULL,
    },
    [REPLAY_FEATURE_HYPERCALL] = { "hypercall answers", REPLAY_FROM_LOG, NULL, NULL },
    [REPLAY_FEATURE_DEVICE_INPUT] = { "device and network input", REPLAY_FROM_LOG, NULL, NULL },
    [REPLAY_FEATURE_TAINT] = { "taint", REPLAY_ALLOWED, NULL, NULL },
    [REPLAY_FEATURE_ANALYSIS] = { "analysis", REPLAY_ALLOWED, NULL, NULL },
};

bool replay_feature_check(ReplayFeature feature, Error **errp)
{
    const ReplayConflict *c;

    assert(feature < REPLAY_FEATURE__MAX);
    c = &replay_conflicts[feature];
    if (replay_mode != REPLAY_MODE_PLAY || c->policy != REPLAY_REFUSED) {
        return true;
    }
    error_setg(errp, "%s is refused in replay: %s", c->name, c->why);
    return false;
}

bool replay_start_check(Error **errp)
{
    for (int f = 0; f < REPLAY_FEATURE__MAX; f++) {
        const ReplayConflict *c = &replay_conflicts[f];

        if (c->policy == REPLAY_REFUSED && c->start_ok && !c->start_ok(errp)) {
            error_prepend(errp, "replay refuses %s: ", c->name);
            return false;
        }
    }
    return true;
}

bool replay_record_check(Error **errp)
{
    if (!startup_caps_has(STARTUP_CAP_RECORD)) {
        error_setg(errp, "recording needs the record capability, chosen at startup "
                   "(-capabilities record; pandare2: capabilities=[\"record\"])");
        return false;
    }
    if (!replay_feature_check(REPLAY_FEATURE_RECORD, errp)) {
        return false;
    }
    /* A recording logs what the mutators it admits do; the rest would make it unreplayable.
     * A thread inside an injected system call would come back to registers nothing logged. */
    return panda_callbacks_record_safe(errp) && panda_hypercall_replay_safe(errp) &&
           panda_inject_record_safe(errp);
}
