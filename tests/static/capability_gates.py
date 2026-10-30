#!/usr/bin/env python3
"""Check patch0087 and compile real CLI/startup/ARM bodies with tiny stubs.

Run: python3 -I tests/static/capability_gates.py PATCHED_QEMU_TREE
No engine build or guest. Requires a strict tree including patch0087.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TREE = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else None
if TREE is None:
    raise SystemExit("pass strict patched QEMU source including0087")


def function(text, name):
    match = re.search(r'^\w[^\n]*\b' + re.escape(name) + r'\([^;]*?\)\s*\{', text, re.M)
    assert match, name
    return text[match.start():text.index('\n}', match.end()) + 2]


vl = (TREE / "system/vl.c").read_text()
board = function(vl, "qemu_init_board")
assert board.index("analysis_policy_freeze()") < board.index("analysis_policy_set_analysis_active")
assert board.index("analysis_policy_set_analysis_active") < board.index("qemu_plugin_load_list") < board.index("machine_run_board_init")
assert "case QEMU_OPTION_analysis_capabilities:" in vl
assert "parse_analysis_capabilities(optarg, &requested, errp)" in vl
assert "'analysis-policy.c'" in (TREE / "system/meson.build").read_text()
assert 'DEF("analysis-capabilities", HAS_ARG' in (TREE / "qemu-options.hx").read_text()

arm64 = function((TREE / "target/arm/tcg/translate-a64.c").read_text(), "trans_PENGUIN")
arm32 = function((TREE / "target/arm/tcg/translate.c").read_text(), "trans_MCR")
for body in (arm64, arm32):
    assert body.index("analysis_guest_hypercall_enabled()") < body.index("gen_helper_penguin_guest_hypercall")
assert "return false; /* preserve upstream UNDEF decoding */" in arm64
assert arm32.index("analysis_guest_hypercall_enabled") < arm32.index("valid_cp") < arm32.index("do_coproc_insn")
for path, token in [
    ("target/mips/tcg/translate.c", "opc == OPC_MOVZ"),
    ("target/ppc/translate/fixedpoint-impl.c.inc", "!shift && a->rt == 10"),
    ("target/riscv/tcg/translate.c", "a->rs1 == 0 && a->rd == 0"),
    ("target/loongarch/tcg/insn_trans/trans_extra.c.inc", "a->rd == 0 && a->rj == 0"),
]:
    source = (TREE / path).read_text()
    assert re.search(r'if \(analysis_guest_hypercall_enabled\(\) &&\s*' + re.escape(token), source), path
pc = (TREE / "hw/i386/pc.c").read_text()
port = re.search(r'    if \(analysis_policy_has\(ANALYSIS_CAP_GUEST_HYPERCALL\)\) \{\n.*?\n    \}', pc, re.S).group()
assert port.count("g_new(MemoryRegion, 1)") == 1
assert "memory_region_init_io(ioport88_io" in port
assert "memory_region_add_subregion(isa_bus->address_space_io, 0x88, ioport88_io)" in port
assert pc.count("g_new(MemoryRegion, 1);") == 3

with tempfile.TemporaryDirectory(prefix="capability-gates-") as temp:
    out = Path(temp)
    for name, text in {
        "qemu/osdep.h": "#include <stdbool.h>\n#include <stdint.h>\n",
        "qemu/typedefs.h": "typedef struct Error Error;\n",
        "qemu/atomic.h": "#define qatomic_set(p,v) (*(p)=(v))\n#define qatomic_read(p) (*(p))\n",
        "qapi/error.h": '#include "qemu/typedefs.h"\nvoid error_setg(Error **, const char *, ...);\n',
    }.items():
        path = out / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    stubs = r'''
#include <assert.h>
#include <glib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "system/analysis-policy.h"
struct Error { char message[160]; };
static Error *error_fatal;
void error_setg(Error **errp, const char *fmt, ...) {
    if (!errp) return;
    assert(!*errp);
    *errp = g_new0(Error, 1);
    va_list ap; va_start(ap, fmt);
    vsnprintf((*errp)->message, sizeof((*errp)->message), fmt, ap);
    va_end(ap);
}
static int translated, fallback, loaded, initialized;
typedef struct DisasContext { int unused; } DisasContext;
typedef struct arg_PENGUIN { int unused; } arg_PENGUIN;
typedef struct arg_MCR { int cp, opc1, rt, crn, crm, opc2; } arg_MCR;
typedef int TCGv_i64;
typedef int TCGv_i32;
static int tcg_env;
#define cpu_reg(...) 0
#define gen_helper_penguin_guest_hypercall(...) (++translated)
#define tcg_temp_new_i64() 0
#define tcg_temp_new_i32() 0
#define tcg_gen_extu_i32_i64(...) ((void)0)
#define tcg_gen_extrl_i64_i32(...) ((void)0)
#define load_reg(...) 0
#define store_reg(...) ((void)0)
#define valid_cp(...) (fallback++, false)
#define do_coproc_insn(...) ((void)0)
typedef struct MachineClass { bool auto_create_sdcard; const char *name; } MachineClass;
typedef struct Object { int unused; } Object;
static MachineClass test_machine_class;
static void *current_machine, *mem_path, *plugin_list;
#define MACHINE_GET_CLASS(...) (&test_machine_class)
#define qemu_plugin_load_list(...) (loaded++)
#define machine_run_board_init(...) (initialized++)
#define TYPE_SD_BUS "sd-bus"
#define object_resolve_path_type(...) NULL
#define drive_check_orphaned() ((void)0)
#define realtime_init() ((void)0)
'''
    tests = r'''
int main(int argc, char **argv) {
    assert(argc == 2);
    Error *error = NULL;
    uint32_t requested = 0;
    assert(!analysis_policy_capabilities());
    assert(!parse_analysis_capabilities("", &requested, &error)); g_clear_pointer(&error, g_free);
    assert(!parse_analysis_capabilities("vanilla,analysis", &requested, &error)); g_clear_pointer(&error, g_free);
    assert(!parse_analysis_capabilities("analysis,vanilla", &requested, &error)); g_clear_pointer(&error, g_free);
    assert(!parse_analysis_capabilities("analysis,", &requested, &error)); g_clear_pointer(&error, g_free);
    assert(!parse_analysis_capabilities("kvm", &requested, &error)); g_clear_pointer(&error, g_free);
    assert(!parse_analysis_capabilities(" analysis", &requested, &error)); g_clear_pointer(&error, g_free);
    assert(parse_analysis_capabilities("analysis", &requested, &error));
    assert(!parse_analysis_capabilities("fuzz,bad", &requested, &error)); g_clear_pointer(&error, g_free);
    assert(requested == ANALYSIS_CAP_ANALYSIS);
    assert(!analysis_policy_requested());
    assert(parse_analysis_capabilities("fuzz,rr,handoff,penguin", &requested, &error));
    assert(requested == (ANALYSIS_CAP_FUZZ | ANALYSIS_CAP_RR | ANALYSIS_CAP_HANDOFF | ANALYSIS_CAP_PENGUIN));
    assert(!analysis_policy_has(ANALYSIS_CAP_GUEST_HYPERCALL));
    if (!strcmp(argv[1], "bare")) {
        assert(parse_analysis_capabilities("vanilla", &requested, &error));
    } else {
        assert(parse_analysis_capabilities(argv[1], &requested, &error));
    }
    assert(analysis_policy_seal(requested, &error));
    bool hypercall = analysis_policy_has(ANALYSIS_CAP_GUEST_HYPERCALL);
    bool analysis = analysis_policy_has(ANALYSIS_CAP_ANALYSIS);
    assert(!analysis_policy_analysis_active());
    qemu_init_board();
    assert(analysis_policy_is_frozen() && loaded == 1 && initialized == 1 && !error_fatal);
    assert(analysis_policy_analysis_active() == analysis);
    assert(!analysis_policy_configure(requested, &error)); g_clear_pointer(&error, g_free);
    DisasContext ctx = {0}; arg_PENGUIN a64 = {0}; arg_MCR a32 = {.cp=7};
    assert(trans_PENGUIN(&ctx, &a64) == hypercall);
    assert(trans_MCR(&ctx, &a32) == hypercall);
    assert(translated == (hypercall ? 2 : 0));
    assert(fallback == (hypercall ? 0 : 1));
}
'''
    wrapper = function((TREE / "target/arm/tcg/translate-a64.c").read_text(),
                       "analysis_guest_hypercall_enabled")
    (out / "test.c").write_text(stubs + wrapper + function(vl, "parse_analysis_capabilities") + board + arm64 + arm32 + tests)
    flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0"], text=True))
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-O2", "-I", str(out),
                    "-I", str(ROOT / "engine/src/include"),
                    str(ROOT / "engine/src/system/analysis-policy.c"), str(out / "test.c"),
                    "-o", str(out / "test"), *flags], check=True)
    for scenario in ("bare", "vanilla", "analysis", "fuzz", "rr", "guest-hypercall", "analysis,guest-hypercall", "handoff,penguin"):
        subprocess.run([str(out / "test"), scenario], check=True)
    # Link actual user-only wrappers without policy source: no unresolved common
    # system query may remain in any target translator.
    for index, name in enumerate(("arm/tcg/translate-a64.c", "arm/tcg/translate.c",
                                  "mips/tcg/translate.c", "ppc/translate.c",
                                  "riscv/tcg/translate.c", "loongarch/tcg/translate.c")):
        text = (TREE / "target" / name).read_text()
        assert '#ifndef CONFIG_USER_ONLY\n#include "system/analysis-policy.h"\n#endif' in text
        wrapper = function(text, "analysis_guest_hypercall_enabled")
        source = '#include <stdbool.h>\n#include <assert.h>\n' + wrapper
        source += '\nint main(void) { assert(!analysis_guest_hypercall_enabled()); }\n'
        (out / "user.c").write_text(source)
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Werror",
                        "-DCONFIG_USER_ONLY", str(out / "user.c"), "-o", str(out / "user")], check=True)
        subprocess.run([str(out / "user")], check=True)
print("PASS: actual CLI atomic parse/rejection, startup freeze/activity ordering, ARM upstream fallback; x86 port/recognizer checks;6 user-only wrappers link without system policy")
