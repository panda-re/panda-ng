#!/usr/bin/env python3
"""Compile the actual common policy with tiny boundary stubs; no engine/guest.

Run: python3 -I tests/static/analysis_policy.py
Each scenario starts a fresh executable process, so no test-only reset API exists.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix="analysis-policy-") as temp:
    out = Path(temp)
    headers = {
        "qemu/osdep.h": "#include <stdbool.h>\n#include <stdint.h>\n",
        "qemu/typedefs.h": "typedef struct Error Error;\n",
        "qemu/atomic.h": (
            "#define qatomic_set(p,v) __atomic_store_n((p),(v),__ATOMIC_SEQ_CST)\n"
            "#define qatomic_read(p) __atomic_load_n((p),__ATOMIC_SEQ_CST)\n"),
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
#include "system/analysis-policy.h"
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
int main(int argc, char **argv)
{
    assert(argc == 2);
    Error *error = NULL;
    uint32_t requested = strtoul(argv[1], NULL, 0);
    uint32_t effective = requested;
    if (requested & (ANALYSIS_CAP_FUZZ | ANALYSIS_CAP_RR)) {
        effective |= ANALYSIS_CAP_ANALYSIS;
    }
    assert(!analysis_policy_requested() && !analysis_policy_capabilities());
    assert(!analysis_policy_is_frozen() && !analysis_policy_analysis_active());
    assert(analysis_policy_has(0));
    assert(analysis_policy_set_analysis_active(false, &error));
    assert(!analysis_policy_set_analysis_active(true, &error));
    clear_error(&error, "frozen");
    assert(analysis_policy_configure(ANALYSIS_CAP_PENGUIN, &error));
    assert(analysis_policy_configure(requested, &error));
    assert(!error && analysis_policy_requested() == requested);
    assert(analysis_policy_capabilities() == effective);
    assert(!analysis_policy_analysis_active());
    for (uint32_t mask = 0; mask <= ANALYSIS_CAP_ALL; mask++) {
        assert(analysis_policy_has(mask) == ((effective & mask) == mask));
    }
    assert(!analysis_policy_has(1u << 31));
    assert(!analysis_policy_configure(requested | (1u << 31), &error));
    clear_error(&error, "unknown");
    assert(analysis_policy_requested() == requested);
    assert(analysis_policy_capabilities() == effective);
    assert(!analysis_policy_configure(1u << 30, NULL));
    analysis_policy_freeze();
    analysis_policy_freeze(); // idempotent, no configuration mutation
    assert(analysis_policy_is_frozen());
    assert(!analysis_policy_configure(0, &error));
    clear_error(&error, "frozen");
    assert(analysis_policy_requested() == requested);
    assert(analysis_policy_capabilities() == effective);
    bool provisioned = effective & ANALYSIS_CAP_ANALYSIS;
    assert(analysis_policy_set_analysis_active(true, &error) == provisioned);
    if (!provisioned) {
        clear_error(&error, "not provisioned");
    }
    assert(analysis_policy_analysis_active() == provisioned);
    assert(analysis_policy_set_analysis_active(false, &error));
    assert(!analysis_policy_analysis_active());
    assert(analysis_policy_capabilities() == effective);
    assert(analysis_policy_set_analysis_active(true, NULL) == provisioned);
    assert(analysis_policy_analysis_active() == provisioned);
    assert(!error);
}
'''
    (out / "test.c").write_text(harness)
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I", str(out), "-I", str(ROOT / "engine/src/include"),
        str(ROOT / "engine/src/system/analysis-policy.c"), str(out / "test.c"),
        "-o", str(out / "test"),
    ], check=True)
    for requested in range(64):
        subprocess.run([str(out / "test"), str(requested)], check=True)
    print("PASS: all64 capability combinations, zero default, prerequisite normalization, unknown rejection, immutable freeze, recoverable errors and separately provisioned runtime activity")
