#!/usr/bin/env python3
"""Compile the engine's startup-capability policy (system/capabilities.c) with tiny boundary stubs;
no engine or guest.

Run: python3 -I tests/static/capability_policy.py
Each scenario starts a fresh executable process, so no test-only reset API exists.
"""
from pathlib import Path
import os
import subprocess
import tempfile

PANDA_TEST = dict(area="capabilities", tier="static", needs=["cc"], nix_check="capability-policy")

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix="capability-policy-") as temp:
    out = Path(temp)
    headers = {
        "qemu/osdep.h": "#include <stdbool.h>\n#include <stdint.h>\n#include <string.h>\n",
        "qemu/typedefs.h": "typedef struct Error Error;\n",
        "qapi/error.h": "#include \"qemu/typedefs.h\"\nvoid error_setg(Error **errp, const char *fmt, ...);\n",
    }
    for name, text in headers.items():
        path = out / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    harness = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system/capabilities.h"
struct Error { char message[160]; };
void error_setg(Error **errp, const char *fmt, ...)
{
    if (!errp) {
        return;
    }
    assert(!*errp);
    *errp = calloc(1, sizeof(**errp));
    va_list ap;
    va_start(ap, fmt);
    vsnprintf((*errp)->message, sizeof((*errp)->message), fmt, ap);
    va_end(ap);
}
static void clear_error(Error **error, const char *reason)
{
    assert(*error && strstr((*error)->message, reason));
    free(*error);
    *error = NULL;
}
static uint32_t parse(const char *names)
{
    uint32_t caps = 0xdead;
    Error *error = NULL;
    assert(startup_caps_parse(names, &caps, &error) && !error);
    return caps;
}
static void parse_fails(const char *names, const char *reason)
{
    uint32_t caps = 0xdead;
    Error *error = NULL;
    assert(!startup_caps_parse(names, &caps, &error));
    clear_error(&error, reason);
    assert(caps == 0xdead);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    Error *error = NULL;
    uint32_t requested = strtoul(argv[1], NULL, 0);
    assert(STARTUP_CAP_ALL == 3);
    assert(!strcmp(startup_cap_name(STARTUP_CAP_RECORD), "record"));
    assert(!strcmp(startup_cap_name(STARTUP_CAP_TAINT), "taint"));
    assert(!startup_cap_name(1u << 5));
    assert(parse("") == 0);
    assert(parse("record") == STARTUP_CAP_RECORD);
    assert(parse("taint,record") == STARTUP_CAP_ALL);
    assert(parse("record,record") == STARTUP_CAP_RECORD);
    parse_fails("analysis", "'analysis'");
    parse_fails("record,rr", "'rr'");
    parse_fails("record,", "''");
    parse_fails(",taint", "''");
    parse_fails("recordx", "'recordx'");
    /* Nothing chosen before the seal: none by default. */
    assert(!startup_caps_is_frozen() && !startup_caps());
    assert(startup_caps_has(0) && !startup_caps_has(STARTUP_CAP_RECORD));
    assert(!startup_caps_seal(requested | (1u << 31), &error));
    clear_error(&error, "unknown");
    assert(!startup_caps_is_frozen() && !startup_caps());
    assert(startup_caps_seal(requested, &error) && !error);
    assert(startup_caps_is_frozen() && startup_caps() == requested);
    for (uint32_t mask = 0; mask <= STARTUP_CAP_ALL; mask++) {
        assert(startup_caps_has(mask) == ((requested & mask) == mask));
    }
    assert(!startup_caps_has(1u << 31));
    /* Frozen: only the identical request is accepted; a refusal changes nothing. */
    assert(startup_caps_seal(requested, &error) && !error);
    assert(!startup_caps_seal(requested ^ STARTUP_CAP_RECORD, &error));
    clear_error(&error, "differ");
    assert(!startup_caps_seal(requested ^ STARTUP_CAP_TAINT, NULL));
    assert(startup_caps() == requested);
    return 0;
}
'''
    (out / "test.c").write_text(harness)
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I", str(out), "-I", str(ROOT / "engine/src/include"),
        str(ROOT / "engine/src/system/capabilities.c"), str(out / "test.c"),
        "-o", str(out / "test"),
    ], check=True)
    for requested in range(4):
        subprocess.run([str(out / "test"), str(requested)], check=True)
    print("PASS: record and taint, none by default, parsing, unknown rejection, immutable seal")
