/*
 * Admission: whether a subscriber may attach to an event in the active mode.
 *
 * One policy for every surface: core callbacks (callbacks.c's callback_mode_valid) and
 * registry events (events.c). It is checked at subscribe and enable time and when record or
 * replay starts, never per dispatch.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_ADMISSION_H
#define PANDA_ADMISSION_H

#include <stdbool.h>
#include <stdint.h>

/* The includer provides analysis_policy_*, kvm_enabled, replay_mode and PANDA_MODE_*. */

typedef enum PandaAdmit {
    PANDA_ADMIT_OK,
    PANDA_ADMIT_NO_ANALYSIS,    /* analysis is not provisioned (or the policy is not sealed yet) */
    PANDA_ADMIT_CAPABILITY,     /* the event needs a capability this run was not given */
    PANDA_ADMIT_MODES,          /* no modes, or modes the event cannot fire in */
    PANDA_ADMIT_KVM,            /* running under KVM, and the subscriber did not ask for it */
    PANDA_ADMIT_RR,             /* record/replay, and the subscriber did not ask for it */
    PANDA_ADMIT_RR_MUTATE,      /* record/replay, a mutator, and the source does not log mutations */
} PandaAdmit;

/*
 * @supported: the modes the event can fire in. @capability: ANALYSIS_CAP_* it needs besides
 * analysis. @rr_mutate: the source logs what mutators do, so they are admitted in rr (today only
 * the hypercall site, 0093). @modes and @observe_only: what the subscriber declared.
 */
static inline PandaAdmit panda_admit(uint32_t supported, uint32_t capability, bool rr_mutate,
                                     uint32_t modes, bool observe_only)
{
    if (!analysis_policy_is_frozen() || !analysis_policy_has(ANALYSIS_CAP_ANALYSIS)) {
        return PANDA_ADMIT_NO_ANALYSIS;
    }
    if (capability && !analysis_policy_has(capability)) {
        return PANDA_ADMIT_CAPABILITY;
    }
    if (!modes || (modes & ~supported)) {
        return PANDA_ADMIT_MODES;
    }
    if (kvm_enabled()) {
        return (modes & PANDA_MODE_KVM) ? PANDA_ADMIT_OK : PANDA_ADMIT_KVM;
    }
    if (replay_mode != REPLAY_MODE_NONE) {
        if (!(modes & PANDA_MODE_RR)) {
            return PANDA_ADMIT_RR;
        }
        if (!observe_only && !rr_mutate) {
            return PANDA_ADMIT_RR_MUTATE;
        }
    }
    return PANDA_ADMIT_OK;
}

static inline const char *panda_admit_reason(PandaAdmit a)
{
    switch (a) {
    case PANDA_ADMIT_OK:            return "admitted";
    case PANDA_ADMIT_NO_ANALYSIS:   return "analysis is not provisioned for this run";
    case PANDA_ADMIT_CAPABILITY:    return "the event needs a capability this run does not have";
    case PANDA_ADMIT_MODES:         return "the requested modes are empty or not ones the event fires in";
    case PANDA_ADMIT_KVM:           return "running under KVM, and the subscriber did not declare PANDA_MODE_KVM";
    case PANDA_ADMIT_RR:            return "record/replay is active, and the subscriber did not declare PANDA_MODE_RR";
    case PANDA_ADMIT_RR_MUTATE:     return "record/replay is active, and mutations of this event are not logged: subscribe as an observer";
    }
    return "unknown";
}

#endif
