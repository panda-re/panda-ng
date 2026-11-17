/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SYSTEM_REPLAY_CONFLICTS_H
#define SYSTEM_REPLAY_CONFLICTS_H

#include <stdbool.h>
#include "qemu/typedefs.h"

/*
 * Replay is a mode, entered by loading a recording; the recording decides the machine and its
 * inputs. What replay allows is one table (replay-conflicts.c), consulted when replay starts and
 * when a feature is requested, instead of a refusal in each feature.
 */
typedef enum ReplayFeature {
    REPLAY_FEATURE_RECORD,          /* starting a recording */
    REPLAY_FEATURE_GUEST_MUTATION,  /* guest register/memory writes outside the log */
    REPLAY_FEATURE_FAST_RESTORE,    /* panda_fast_snap / panda_fast_restore */
    REPLAY_FEATURE_HYPERCALL,       /* guest hypercall answers */
    REPLAY_FEATURE_DEVICE_INPUT,    /* live devices and network input */
    REPLAY_FEATURE_TAINT,
    REPLAY_FEATURE_ANALYSIS,        /* observe-only callbacks and plugins */
    REPLAY_FEATURE__MAX,
} ReplayFeature;

/* A feature is requested: false, with the reason in @errp, if replay refuses it now. */
bool replay_feature_check(ReplayFeature feature, Error **errp);
/* Replay is starting: false, with the reason, if something in use is a feature replay refuses. */
bool replay_start_check(Error **errp);
/* A recording is starting: the record capability, not in replay, and only logged mutators. */
bool replay_record_check(Error **errp);

#endif
