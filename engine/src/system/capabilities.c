/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "system/capabilities.h"

static const struct {
    uint32_t bit;
    const char *name;
} cap_names[] = {
    { STARTUP_CAP_RECORD, "record" },
    { STARTUP_CAP_TAINT, "taint" },
};

static uint32_t chosen;
static bool frozen;

bool startup_caps_seal(uint32_t requested, Error **errp)
{
    if (requested & ~STARTUP_CAP_ALL) {
        error_setg(errp, "unknown startup capability bits: 0x%x",
                   requested & ~STARTUP_CAP_ALL);
        return false;
    }
    if (frozen) {
        if (requested != chosen) {
            error_setg(errp, "startup capabilities differ from the ones already chosen "
                       "(requested 0x%x, chosen 0x%x)", requested, chosen);
            return false;
        }
        return true;
    }
    chosen = requested;
    frozen = true;
    return true;
}

bool startup_caps_is_frozen(void)
{
    return frozen;
}

uint32_t startup_caps(void)
{
    return chosen;
}

bool startup_caps_has(uint32_t caps)
{
    return !(caps & ~STARTUP_CAP_ALL) && (chosen & caps) == caps;
}

const char *startup_cap_name(uint32_t cap)
{
    for (size_t i = 0; i < sizeof(cap_names) / sizeof(cap_names[0]); i++) {
        if (cap_names[i].bit == cap) {
            return cap_names[i].name;
        }
    }
    return NULL;
}

bool startup_caps_parse(const char *names, uint32_t *caps, Error **errp)
{
    uint32_t parsed = 0;
    const char *p = names;

    while (*names) {
        const char *end = strchr(p, ',');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        size_t i;

        for (i = 0; i < sizeof(cap_names) / sizeof(cap_names[0]); i++) {
            if (strlen(cap_names[i].name) == len && !strncmp(p, cap_names[i].name, len)) {
                parsed |= cap_names[i].bit;
                break;
            }
        }
        if (i == sizeof(cap_names) / sizeof(cap_names[0])) {
            error_setg(errp, "unknown startup capability '%.*s' (expected record or taint)",
                       (int)len, p);
            return false;
        }
        if (!end) {
            break;
        }
        p = end + 1;
    }
    *caps = parsed;
    return true;
}
