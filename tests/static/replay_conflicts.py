#!/usr/bin/env python3
"""Compile the replay table (replay/replay-conflicts.c) with tiny boundary stubs; no engine or
guest.

Run: python3 -I tests/static/replay_conflicts.py

In replay, starting a recording, guest-visible mutation outside the log and fast restore are
refused, with the reason; hypercall answers and device input come from the log; taint and
analysis are allowed. Outside replay nothing is refused. At replay start, what is in use and
refused (a recording, mutators that can't be replayed) refuses the replay. A recording needs the
record capability, no replay, and only logged mutators.
"""
from pathlib import Path
import os
import subprocess
import tempfile

PANDA_TEST = dict(area="rr", tier="static", needs=["cc"], nix_check="replay-conflicts")

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix="replay-conflicts-") as temp:
    out = Path(temp)
    headers = {
        "qemu/osdep.h": "#include <stdbool.h>\n#include <stdint.h>\n#include <string.h>\n#include <assert.h>\n",
        "qemu/typedefs.h": "typedef struct Error Error;\n",
        "qapi/error.h": ("#include \"qemu/typedefs.h\"\nvoid error_setg(Error **errp, const char *fmt, ...);\n"
                         "void error_prepend(Error **errp, const char *fmt, ...);\n"),
        "system/replay.h": ("enum { REPLAY_MODE_NONE, REPLAY_MODE_RECORD, REPLAY_MODE_PLAY };\n"
                            "extern int replay_mode;\n"),
        "system/panda-embed.h": "bool panda_hypercall_replay_safe(Error **errp);\n",
        "panda/hooks.h": ("bool panda_callbacks_replay_safe(Error **errp);\n"
                          "bool panda_callbacks_record_safe(Error **errp);\n"
                          "bool panda_inject_record_safe(Error **errp);\n"),
    }
    for name, text in headers.items():
        path = out / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    harness = r'''
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "system/replay.h"
#include "system/capabilities.h"
#include "system/replay-conflicts.h"
struct Error { char message[256]; };
int replay_mode;
static bool cbs_replay_safe = true, cbs_record_safe = true, hc_safe = true, inj_safe = true;
static uint32_t caps;
bool startup_caps_has(uint32_t c) { return (caps & c) == c; }
static bool fail(Error **errp, bool ok, const char *why)
{
    if (!ok) error_setg(errp, "%s", why);
    return ok;
}
bool panda_callbacks_replay_safe(Error **errp) { return fail(errp, cbs_replay_safe, "mutating callback"); }
bool panda_callbacks_record_safe(Error **errp) { return fail(errp, cbs_record_safe, "unlogged mutator"); }
bool panda_hypercall_replay_safe(Error **errp) { return fail(errp, hc_safe, "KVM hypercall consumer"); }
bool panda_inject_record_safe(Error **errp) { return fail(errp, inj_safe, "inside an injected call"); }
void error_setg(Error **errp, const char *fmt, ...)
{
    if (!errp) return;
    assert(!*errp);
    *errp = calloc(1, sizeof(**errp));
    va_list ap;
    va_start(ap, fmt);
    vsnprintf((*errp)->message, sizeof((*errp)->message), fmt, ap);
    va_end(ap);
}
void error_prepend(Error **errp, const char *fmt, ...)
{
    char head[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(head, sizeof(head), fmt, ap);
    va_end(ap);
    char tail[256];
    strcpy(tail, (*errp)->message);
    snprintf((*errp)->message, sizeof((*errp)->message), "%s%s", head, tail);
}
static void expect(bool ok, Error **error, const char *a, const char *b)
{
    if (!a) { assert(ok && !*error); return; }
    assert(!ok && *error);
    assert(strstr((*error)->message, a));
    assert(!b || strstr((*error)->message, b));
    free(*error);
    *error = NULL;
}
int main(void)
{
    Error *error = NULL;
    static const char *refused[] = {
        [REPLAY_FEATURE_RECORD] = "record",
        [REPLAY_FEATURE_GUEST_MUTATION] = "mutation",
        [REPLAY_FEATURE_FAST_RESTORE] = "fast restore",
    };
    for (int mode = REPLAY_MODE_NONE; mode <= REPLAY_MODE_PLAY; mode++) {
        replay_mode = mode;
        for (int f = 0; f < REPLAY_FEATURE__MAX; f++) {
            bool refuse = mode == REPLAY_MODE_PLAY && f < (int)(sizeof(refused) / sizeof(refused[0])) && refused[f];
            expect(replay_feature_check(f, &error), &error, refuse ? refused[f] : NULL, refuse ? "replay" : NULL);
        }
    }
    /* Replay start: a recording in progress, or mutators that can't be replayed. */
    replay_mode = REPLAY_MODE_NONE;
    expect(replay_start_check(&error), &error, NULL, NULL);
    replay_mode = REPLAY_MODE_RECORD;
    expect(replay_start_check(&error), &error, "replay refuses record", "recording is in progress");
    replay_mode = REPLAY_MODE_PLAY;     /* replay_start() checks once the mode is set */
    expect(replay_start_check(&error), &error, NULL, NULL);
    cbs_replay_safe = false;
    expect(replay_start_check(&error), &error, "guest-visible mutation", "mutating callback");
    cbs_replay_safe = true; hc_safe = false;
    expect(replay_start_check(&error), &error, "guest-visible mutation", "KVM hypercall consumer");
    hc_safe = true;
    /* Recording: the capability first, then not in replay, then logged mutators only. */
    replay_mode = REPLAY_MODE_NONE;
    caps = 0;
    expect(replay_record_check(&error), &error, "record capability", "-capabilities record");
    caps = STARTUP_CAP_TAINT;
    expect(replay_record_check(&error), &error, "record capability", NULL);
    caps = STARTUP_CAP_RECORD;
    expect(replay_record_check(&error), &error, NULL, NULL);
    replay_mode = REPLAY_MODE_RECORD;   /* replay_start() for -icount rr=record */
    expect(replay_record_check(&error), &error, NULL, NULL);
    replay_mode = REPLAY_MODE_PLAY;
    expect(replay_record_check(&error), &error, "record is refused in replay", NULL);
    replay_mode = REPLAY_MODE_NONE;
    cbs_record_safe = false;
    expect(replay_record_check(&error), &error, "unlogged mutator", NULL);
    cbs_record_safe = true; hc_safe = false;
    expect(replay_record_check(&error), &error, "KVM hypercall consumer", NULL);
    hc_safe = true; inj_safe = false;
    expect(replay_record_check(&error), &error, "inside an injected call", NULL);
    return 0;
}
'''
    (out / "test.c").write_text(harness)
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=gnu11", "-O2", "-Wall", "-Werror", "-Wno-unused-function",
        "-I", str(out), "-I", str(ROOT / "engine/src/include"),
        str(ROOT / "engine/src/replay/replay-conflicts.c"), str(out / "test.c"),
        "-o", str(out / "test"),
    ], check=True)
    subprocess.run([str(out / "test")], check=True)
    print("PASS: replay refuses record, unlogged mutation and fast restore with reasons and allows "
          "the rest; replay start refuses a recording and unreplayable mutators; recording needs "
          "the record capability, no replay, logged mutators and no thread inside an injected call")
