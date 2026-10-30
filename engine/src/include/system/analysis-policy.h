/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SYSTEM_ANALYSIS_POLICY_H
#define SYSTEM_ANALYSIS_POLICY_H

#include <stdbool.h>
#include <stdint.h>
#include "qemu/typedefs.h"

/* Startup capabilities, not accelerator selection or callback compatibility.
 * Zero means explicit vanilla intent; no parity is implied until every extension
 * path is gated. Fuzz and RR provision analysis as a prerequisite. */
typedef enum AnalysisCapability {
    ANALYSIS_CAP_ANALYSIS = 1u << 0,
    ANALYSIS_CAP_FUZZ = 1u << 1,
    ANALYSIS_CAP_RR = 1u << 2,
    ANALYSIS_CAP_GUEST_HYPERCALL = 1u << 3,
    ANALYSIS_CAP_HANDOFF = 1u << 4,
    ANALYSIS_CAP_PENGUIN = 1u << 5,
} AnalysisCapability;

#define ANALYSIS_CAP_ALL ((uint32_t)(ANALYSIS_CAP_ANALYSIS | ANALYSIS_CAP_FUZZ | \
    ANALYSIS_CAP_RR | ANALYSIS_CAP_GUEST_HYPERCALL | ANALYSIS_CAP_HANDOFF | \
    ANALYSIS_CAP_PENGUIN))

/* What the command line provisions with no -analysis-capabilities and no
 * embedded seal: the behavior before capabilities existed. Vanilla is explicit. */
#define ANALYSIS_CAP_CLI_DEFAULT ((uint32_t)(ANALYSIS_CAP_ANALYSIS | \
    ANALYSIS_CAP_GUEST_HYPERCALL | ANALYSIS_CAP_PENGUIN))

/* Configure on the startup thread before freeze, replacing the previous request.
 * Failure leaves all policy state unchanged. Unconfigured policy is zero.
 * Freeze before machine realization/execution and any analysis activation.
 * Configuration/freeze must not race with readers or activation. */
bool analysis_policy_configure(uint32_t requested, Error **errp);
void analysis_policy_freeze(void);
/* Provision and freeze before any embedding/plugin constructor side effects.
 * An already frozen policy accepts only the identical requested mask. Does not
 * activate analysis. Startup CLI may confirm, but never override, this seal. */
bool analysis_policy_seal(uint32_t requested, Error **errp);
bool analysis_policy_is_frozen(void);
uint32_t analysis_policy_requested(void);
uint32_t analysis_policy_capabilities(void);
/* True iff every requested bit is provisioned; unknown bits return false. */
bool analysis_policy_has(uint32_t capabilities);

/* Runtime activity is separate from immutable provisioned capabilities.
 * Enabling requires frozen policy with ANALYSIS provisioned; configuring does
 * not activate anything. Callers serialize instrumentation changes/quiescence;
 * this flag publishes intent only and performs no callback/TB work itself.
 * Activation wiring must invalidate/retranslate affected TBs using the existing
 * engine flush mechanisms before guest execution resumes. */
bool analysis_policy_set_analysis_active(bool active, Error **errp);
bool analysis_policy_analysis_active(void);

#endif
