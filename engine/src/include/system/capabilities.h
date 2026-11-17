/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SYSTEM_CAPABILITIES_H
#define SYSTEM_CAPABILITIES_H

#include <stdbool.h>
#include <stdint.h>
#include "qemu/typedefs.h"

/*
 * Startup capabilities: what a run must choose before the machine exists, because it changes
 * the run's contract or layout in a way that can't be undone mid-run. Everything else (callbacks,
 * plugins, guest hypercalls, fast snapshots, cross-mode snapshot loading) is always available, or
 * is decided from the running system, and has no capability. None are selected by default.
 *
 * The values are the C API's PANDA_CAP_* (panda/panda_api.h).
 */
typedef enum StartupCapability {
    STARTUP_CAP_RECORD = 1u << 0,   /* the run may record: a recordable machine from launch */
    STARTUP_CAP_TAINT = 1u << 1,    /* taint's shadow state is reserved before guest memory */
} StartupCapability;

#define STARTUP_CAP_ALL ((uint32_t)(STARTUP_CAP_RECORD | STARTUP_CAP_TAINT))

/*
 * Choose the capabilities and freeze them, before anything that depends on them runs (the
 * embedding calls it before qemu_init, the command line at the start of qemu_init). Once frozen,
 * only the identical request succeeds; a failure changes nothing.
 */
bool startup_caps_seal(uint32_t requested, Error **errp);
bool startup_caps_is_frozen(void);
uint32_t startup_caps(void);
/* True iff every bit in @caps was chosen; unknown bits return false. */
bool startup_caps_has(uint32_t caps);

/* "record,taint" (comma-separated, no empty names) to bits. "" is no capabilities. */
bool startup_caps_parse(const char *names, uint32_t *caps, Error **errp);
/* The name of one capability bit ("record"), or NULL. */
const char *startup_cap_name(uint32_t cap);

#endif
