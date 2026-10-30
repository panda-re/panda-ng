/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qemu/atomic.h"
#include "qapi/error.h"
#include "system/analysis-policy.h"

static uint32_t requested_capabilities;
static uint32_t effective_capabilities;
static bool policy_frozen;
static bool analysis_active;

bool analysis_policy_configure(uint32_t requested, Error **errp)
{
    uint32_t effective = requested;

    if (policy_frozen) {
        error_setg(errp, "analysis policy is frozen after startup");
        return false;
    }
    if (requested & ~ANALYSIS_CAP_ALL) {
        error_setg(errp, "unknown analysis capability bits: 0x%x",
                   requested & ~ANALYSIS_CAP_ALL);
        return false;
    }
    if (requested & (ANALYSIS_CAP_FUZZ | ANALYSIS_CAP_RR)) {
        effective |= ANALYSIS_CAP_ANALYSIS;
    }
    requested_capabilities = requested;
    effective_capabilities = effective;
    return true;
}

void analysis_policy_freeze(void)
{
    policy_frozen = true;
}

bool analysis_policy_seal(uint32_t requested, Error **errp)
{
    if (policy_frozen) {
        if (requested != requested_capabilities) {
            error_setg(errp, "startup capabilities differ from sealed policy "
                       "(requested 0x%x, sealed 0x%x)",
                       requested, requested_capabilities);
            return false;
        }
        return true;
    }
    if (!analysis_policy_configure(requested, errp)) {
        return false;
    }
    analysis_policy_freeze();
    return true;
}

bool analysis_policy_is_frozen(void)
{
    return policy_frozen;
}

uint32_t analysis_policy_requested(void)
{
    return requested_capabilities;
}

uint32_t analysis_policy_capabilities(void)
{
    return effective_capabilities;
}

bool analysis_policy_has(uint32_t capabilities)
{
    return !(capabilities & ~ANALYSIS_CAP_ALL) &&
           (effective_capabilities & capabilities) == capabilities;
}

bool analysis_policy_set_analysis_active(bool active, Error **errp)
{
    if (active && !policy_frozen) {
        error_setg(errp, "analysis activation requires frozen startup policy");
        return false;
    }
    if (active && !analysis_policy_has(ANALYSIS_CAP_ANALYSIS)) {
        error_setg(errp, "analysis was not provisioned at startup");
        return false;
    }
    qatomic_set(&analysis_active, active);
    return true;
}

bool analysis_policy_analysis_active(void)
{
    return qatomic_read(&analysis_active);
}
