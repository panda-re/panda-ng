"""The hooks plugin (add_hook, pandare2's panda.hook): what a hook sees and when it fires.

Run: panda-python tests/hooks.py <dir>/busybox-<arch>.json [--smp N] [--case NAME]
(an image from tests/mkbusybox.py; the machine is tests/testmachine.py's)

The guest program is tests/programs/hooks_<arch>.S: hypercall 1 (with its label table), three
phases separated by hypercalls 3 and 4, then hypercall 2. Each phase runs R rounds of: a block of
three adjacent instructions h0 h1 h2; p0 at its own address and at a second mapping of the same
file page (p0 + ALT_DELTA: the same code at another virtual address); a block that runs from one
page into the next (x0 on the first page, xs straddling the boundary on x86_64, the last
instruction of the page on aarch64, x1 on the second page); getpid. So a hook fires R times per
phase, or not at all. Each case runs in a child process:
- type-<t>: a Python hook of each cb_type pandare2 offers, at h0: R per phase, with the arguments
  of that type's signature (cpu; the block holding h0; the hook, at h0, of that type; pc for
  before_block_translate, an exit code for after_block_exec);
- c: C hooks (tests/hooks_c.c) of type before_tcg_codegen: one counting at h0, one that disables
  itself at h1 (fires once);
- filters: asid (the program's, and another: never), kernel/user mode (kernel=True on user code:
  never), and hooks in kernel code: getpid's kernel function (from /proc/kallsyms) with
  kernel=True fires R per phase in the program's process, with kernel=False never;
- multi: two hooks at h1 and one at each neighbour, h0 and h2, in one block; a hook with an asid
  added last at h1 runs first there, then the others in the order they were added;
- late: hooks added at hypercall 3 (h1, mid-block, its block already run R times) and 4 (h0):
  0 R R and 0 0 R;
- remove: one that disables itself (fires once), one disabled through the pointer its callback
  got (R 0 0), one under disable_hooking from hypercall 3 to 4 (R 0 R);
- alias: p0 and its second mapping hooked from the start, each at its own address only: R per
  phase each; alias-late: the second mapping's hook added at hypercall 3, after both ran: R R R
  and 0 R R;
- cross: x0, xs and x1: R per phase each;
- path: the hooks plugin loaded with no hooks, then a hook added at hypercall 3: whether any vCPU
  translates through QEMU's plugin path (panda_bridge_tb_trans_on) before and after. Expected: never
  (the hooks are the engine's own);
- unload: a C and a Python hook (R per phase), then the hooks plugin unloaded from the host
  thread between two runs of the program. The unload is asynchronous (a main-loop bottom half
  pauses the vCPUs, runs uninit_plugin, which removes the engine hooks, and closes the library);
  the test waits for the plugin to be gone, then runs the program again: no hook fires, and the
  program finishes;
- smp (--smp > 1): the program on every vCPU at once, with a C hook counting at h0 and one at h1
  that adds a self-disabling hook at h0 on every hit (the hooks at h0 replaced while every vCPU
  runs them): h0's counter is exactly vCPUs * 3R, h1's too.
Record/replay is tests/hooks_rr.py (a cloud image: a test machine's recording does not replay).
"""
import argparse
import base64
import ctypes
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "programs"))
from testlib import MAGIC, Checks, progress  # noqa: E402

PANDA_TEST = dict(area="hooks", tier="guest", fixture="testmachine", arches=["x86_64", "aarch64"],
                  plugins=["hooks"], variants=dict(smp=[1, 4]), needs=["cc"], timeout=1800)

R = 100
ALT_DELTA = 0x200000
LABELS = ["h0", "h1", "h2", "p0", "x0", "xs", "x1"]
TYPES = ["start_block_exec", "before_block_exec", "end_block_exec", "after_block_translate",
         "before_block_exec_invalidate_opt", "after_block_exec", "before_block_translate"]
CASES = [f"type-{t}" for t in TYPES] + ["c", "filters", "multi", "late", "remove", "alias",
                                         "alias-late", "cross", "path", "unload"]
SMP_CASES = ["smp", "path"]
KSYM = {"x86_64": "__x64_sys_getpid", "aarch64": "__arm64_sys_getpid"}

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
ap.add_argument("--case")
opt = ap.parse_args()

from hooks_programs import PROGRAMS  # noqa: E402

if json.load(open(opt.image)).get("arch") not in PROGRAMS:
    print("SKIP hooks: no tests/programs/hooks_<arch>.S for this image's arch")
    sys.exit(0)

if opt.case is None:
    failed = []
    for case in CASES if opt.smp == 1 else SMP_CASES:
        progress(f"case {case}")
        cmd = [sys.executable, os.path.abspath(__file__), opt.image, "--smp", str(opt.smp),
               "--case", case]
        try:
            r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                               timeout=900)
            out, rc = r.stdout, r.returncode
        except subprocess.TimeoutExpired as e:
            out, rc = (e.stdout or b"").decode(errors="replace") + "\nTIMEOUT", -1
        print("\n".join(f"  [{case}] {line}" for line in out.splitlines()), flush=True)
        if rc:
            failed.append(f"{case}(rc={rc})" if rc < 0 or rc > 1 else case)
    print(f"HOOKS smp{opt.smp} {'FAIL ' + ' '.join(failed) if failed else 'PASS'}")
    sys.exit(1 if failed else 0)

import cffi  # noqa: E402

from testmachine import Agent, Machine  # noqa: E402

case = opt.case
m = Machine(opt.image, smp=opt.smp)
arch = m.arch
C = Checks(arch)
panda = m.panda()
agent = Agent(panda)
probe = cffi.FFI()
probe.cdef("bool panda_bridge_tb_trans_on(int cpu_index);")
engine = probe.dlopen(panda.libpanda_path)

# What the callbacks saw: hits[name][phase], the order of hits at one address, bad arguments.
st = {"phase": 0, "labels": {}, "asid": None, "hc": [], "seq": [], "bad": [], "kept": None,
      "bits": {}, "stray": Counter(), "ksym": None}
hits = defaultdict(Counter)


def tb_trans_on():
    return [bool(engine.panda_bridge_tb_trans_on(i)) for i in range(opt.smp)]


def ours(cpu):
    return st["asid"] is not None and panda.current_asid(cpu) == st["asid"]


def bad(name, what):
    if len(st["bad"]) < 20:
        st["bad"].append(f"{name}: {what}")


def check_tb(name, cpu, tb, addr):
    if tb == panda.ffi.NULL:
        bad(name, "tb is NULL")
    elif not (tb.pc <= addr < tb.pc + tb.size):
        bad(name, f"tb {tb.pc:#x}+{tb.size} does not hold {addr:#x}")


def make_hook(name, addr, cb_type="start_block_exec", **kw):
    """A counting Python hook of @cb_type at @addr, with argument checks."""
    type_num = getattr(panda.libpanda, "PANDA_CB_" + cb_type.upper())

    def check_hook(hook):
        if hook == panda.ffi.NULL or hook.addr != addr or hook.type != type_num:
            bad(name, "hook pointer" if hook == panda.ffi.NULL else
                f"hook addr {hook.addr:#x} type {hook.type}, want {addr:#x} {type_num}")

    def note(cpu):
        if cpu == panda.ffi.NULL or not 0 <= cpu.cpu_index < opt.smp:
            bad(name, "cpu")
        if not ours(cpu):
            st["stray"][name] += 1
            return
        hits[name][st["phase"]] += 1
        st["seq"].append((name, 0))

    if cb_type == "after_block_exec":
        def fn(cpu, tb, code, hook):
            check_hook(hook)
            check_tb(name, cpu, tb, addr)
            if code != 0:
                bad(name, f"exit code {code}")
            note(cpu)
    elif cb_type == "before_block_translate":
        def fn(cpu, pc, hook):
            check_hook(hook)
            if pc != addr:
                bad(name, f"pc {pc:#x}")
            note(cpu)
    elif cb_type == "before_block_exec_invalidate_opt":
        def fn(cpu, tb, hook):
            check_hook(hook)
            check_tb(name, cpu, tb, addr)
            note(cpu)
            return False
    else:
        def fn(cpu, tb, hook):
            check_hook(hook)
            check_tb(name, cpu, tb, addr)
            note(cpu)
    panda.hook(addr, cb_type=cb_type, observe_only=True, **kw)(fn)


def counter_hook(name, addr, then=None, **kw):
    """A plain hook that counts its hits in the program's process (busybox's own text is at
    0x401000 too, and the agent's shell runs it), then calls then(cpu, tb, hook)."""
    def fn(cpu, tb, hook):
        if not ours(cpu):
            st["stray"][name] += 1
            return
        hits[name][st["phase"]] += 1
        st["seq"].append((name, 0))
        if then:
            then(cpu, tb, hook)
    panda.hook(addr, observe_only=True, **kw)(fn)


# --- the C hooks ----------------------------------------------------------------------------
hookc = None


def load_hookc(churn_addr=0):
    """Build tests/hooks_c.c and point it at the hooks plugin's add_hook (loaded already)."""
    global hookc
    tmp = tempfile.mkdtemp(prefix="hooksc-")
    so = os.path.join(tmp, "hookc.so")
    subprocess.run(["cc", "-shared", "-fPIC", "-O2", "-o", so, os.path.join(HERE, "hooks_c.c")],
                   check=True)
    ffi = cffi.FFI()
    ffi.cdef("void hookc_setup(void *add_hook, int type, uint64_t addr);"
             "void hookc_add(uint64_t addr, int fn, int counter); uint64_t hookc_get(int i);")
    hookc = ffi.dlopen(so)
    shutil.rmtree(tmp)          # mapped: the file is no longer needed
    path = next(line.split()[-1] for line in open("/proc/self/maps") if "libpanda-hooks_" in line)
    add = ctypes.cast(ctypes.CDLL(path, mode=os.RTLD_NOLOAD | os.RTLD_NOW).add_hook, ctypes.c_void_p)
    hookc.hookc_setup(ffi.cast("void *", add.value), panda.libpanda.PANDA_CB_BEFORE_TCG_CODEGEN,
                      churn_addr)


def kernel_getpid():
    rc, out = agent.run(f"grep ' {KSYM[arch]}$' /proc/kallsyms")
    words = out.split()
    return int(words[0], 16) if rc == 0 and words else None


# --- what each case does at each hypercall --------------------------------------------------
def at_hc1(cpu):
    L = st["labels"]
    if case.startswith("type-"):
        make_hook("h0", L["h0"], cb_type=case[5:], kernel=False)
    elif case == "c":
        hookc.hookc_add(L["h0"], 0, 0)
        hookc.hookc_add(L["h1"], 1, 1)
    elif case == "filters":
        counter_hook("asid", L["h0"], asid=st["asid"], kernel=False)
        counter_hook("other-asid", L["h1"], asid=st["asid"] + 0x1000, kernel=False)
        counter_hook("kernel-on-user", L["h2"], kernel=True)
        counter_hook("user-on-user", L["h2"], kernel=False)
        if st["ksym"]:
            counter_hook("kernel-on-kernel", st["ksym"], kernel=True)
            counter_hook("user-on-kernel", st["ksym"], kernel=False)
    elif case == "multi":
        def order(tag):
            return lambda cpu, tb, hook: st["seq"].append(("order", tag))
        counter_hook("h1-a", L["h1"], then=order("a"))
        counter_hook("h1-b", L["h1"], then=order("b"))
        counter_hook("h0", L["h0"])
        counter_hook("h2", L["h2"])
        counter_hook("h1-asid", L["h1"], then=order("asid"), asid=st["asid"], kernel=False)
    elif case == "remove":
        def once(cpu, tb, hook):
            hook.enabled = False
        counter_hook("once", L["h0"], then=once)

        def keep(cpu, tb, hook):
            st["kept"] = hook
        counter_hook("kept", L["h1"], then=keep)
        counter_hook("global", L["h2"])
    elif case == "alias":
        counter_hook("p0", L["p0"])
        counter_hook("p0-alt", L["p0"] + ALT_DELTA)
    elif case == "alias-late":
        counter_hook("p0", L["p0"])
    elif case == "cross":
        for k in ("x0", "xs", "x1"):
            counter_hook(k, L[k])
    elif case == "unload" and not st.get("added"):     # the first run only
        st["added"] = True
        hookc.hookc_add(L["h0"], 0, 0)
        counter_hook("py", L["h1"])


def at_hc3(cpu):
    L = st["labels"]
    if case == "late":
        counter_hook("h1", L["h1"])
    elif case == "remove":
        if st["kept"] is not None:
            st["kept"].enabled = False
        panda.plugins["hooks"].disable_hooking()
    elif case == "alias-late":
        counter_hook("p0-alt", L["p0"] + ALT_DELTA)
    elif case == "path":
        st["bits"]["before-hook"] = tb_trans_on()
        counter_hook("h0", L["h0"])


def at_hc4(cpu):
    if case == "late":
        counter_hook("h0", st["labels"]["h0"])
    elif case == "remove":
        panda.plugins["hooks"].enable_hooking()


def hypercall(cpu):
    op = panda.arch.get_arg(cpu, 2, convention="syscall")
    if not ours(cpu) and op != 1 and st["asid"] is not None and case != "smp":
        return
    st["hc"].append(op)
    if case == "smp":
        if op == 1 and not st["labels"]:
            table = panda.arch.get_arg(cpu, 3, convention="syscall")
            raw = bytes(panda.virtual_memory_read(cpu, table, 8 * len(LABELS)))
            st["labels"] = {k: int.from_bytes(raw[8 * i:8 * i + 8], "little")
                            for i, k in enumerate(LABELS)}
        return
    if op == 1:
        table = panda.arch.get_arg(cpu, 3, convention="syscall")
        raw = bytes(panda.virtual_memory_read(cpu, table, 8 * len(LABELS)))
        st["labels"] = {k: int.from_bytes(raw[8 * i:8 * i + 8], "little") for i, k in enumerate(LABELS)}
        st["asid"] = panda.current_asid(cpu)
        st["phase"] = 1
        at_hc1(cpu)
    elif op == 3:
        st["phase"] = 2
        at_hc3(cpu)
    elif op == 4:
        st["phase"] = 3
        at_hc4(cpu)
    elif op == 2:
        st["phase"] = 4


panda.hypercall(MAGIC, observe_only=True)(hypercall)

out = {}


def put_program():
    agent.put("/tmp/hooks", base64.b64decode(PROGRAMS[arch]))


@panda.queue_blocking
def drive():
    try:
        st["bits"]["start"] = tb_trans_on()
        if case in ("path", "c", "unload", "smp"):
            panda.load_plugin("hooks")
        if case in ("c", "unload"):
            load_hookc()
        m.revert(panda, agent)
        put_program()
        if case == "filters":
            st["ksym"] = kernel_getpid()
        if case == "smp":
            # a first run for the labels (static addresses), then hooks from here
            out["first"] = agent.run("/tmp/hooks; echo rc=$?", timeout=300)
            out["labels"] = dict(st["labels"])
        if case == "smp":
            load_hookc(st["labels"]["h0"])
            hookc.hookc_add(st["labels"]["h0"], 0, 0)
            hookc.hookc_add(st["labels"]["h1"], 2, 1)
            cmd = " ".join(f"(taskset {1 << c:x} /tmp/hooks; echo rc{c}=$?) &" for c in range(opt.smp))
            out["run"] = agent.run(f"{cmd} wait; echo rc=$?", timeout=600)
        else:
            out["run"] = agent.run("/tmp/hooks; echo rc=$?", timeout=300)
            agent.run("true")
        if case == "unload":
            st["before"] = (hookc.hookc_get(0), dict(hits["py"]))
            panda.unload_plugin("hooks")        # done later, by the main loop
            t = time.monotonic()
            while panda.libpanda.panda_get_plugin_by_name(b"hooks") != panda.ffi.NULL \
                    and time.monotonic() - t < 60:
                time.sleep(0.05)
            st["gone"] = panda.libpanda.panda_get_plugin_by_name(b"hooks") == panda.ffi.NULL
            out["run2"] = agent.run("/tmp/hooks; echo rc=$?", timeout=300)
        st["bits"]["end"] = tb_trans_on()
    except Exception as e:
        out["error"] = repr(e)
    panda.end_analysis()


panda.run()

rc_ok = out.get("run") and out["run"][0] == 0 and b"rc=0" in out["run"][1]
C.check("no error in the driver", "error" not in out, out.get("error"))
C.check("the program ran, exit 0", rc_ok, out.get("run") and out["run"][1][-200:])
per = {name: [c[p] for p in (1, 2, 3)] for name, c in hits.items()}
RRR, ZERO = [R, R, R], [0, 0, 0]


def expect(name, want):
    C.check(f"{name}: {want} hits per phase", per.get(name, ZERO) == want, per.get(name, ZERO))


if case != "smp":
    C.check("hypercalls 1, 3, 4, 2", st["hc"][-4:] == [1, 3, 4, 2], st["hc"])
if case.startswith("type-"):
    expect("h0", RRR)
    C.check("the arguments are the type's", not st["bad"], st["bad"])
elif case == "c":
    C.check("C counting hook at h0: R per phase, 3R in all", hookc.hookc_get(0) == 3 * R,
            hookc.hookc_get(0))
    C.check("C hook that disables itself at h1: once", hookc.hookc_get(1) == 1, hookc.hookc_get(1))
    C.check("C hooks got a cpu and a block", hookc.hookc_get(-1) == 0, hookc.hookc_get(-1))
elif case == "filters":
    expect("asid", RRR)
    expect("other-asid", ZERO)
    expect("kernel-on-user", ZERO)
    expect("user-on-user", RRR)
    C.check(f"{KSYM[arch]} found in /proc/kallsyms", st["ksym"] is not None)
    expect("kernel-on-kernel", RRR)
    expect("user-on-kernel", ZERO)
elif case == "multi":
    for k in ("h1-a", "h1-b", "h0", "h2", "h1-asid"):
        expect(k, RRR)
    orders = [t for k, t in st["seq"] if k == "order"]
    C.check("at h1: the asid hook first, then the others in the order they were added",
            orders == ["asid", "a", "b"] * 3 * R, orders[:6])
elif case == "late":
    expect("h1", [0, R, R])
    expect("h0", [0, 0, R])
elif case == "remove":
    C.check("a hook that disables itself fires once", sum(per.get("once", ZERO)) == 1, per.get("once"))
    expect("kept", [R, 0, 0])
    expect("global", [R, 0, R])
elif case == "alias":
    expect("p0", RRR)
    expect("p0-alt", RRR)
elif case == "alias-late":
    expect("p0", RRR)
    expect("p0-alt", [0, R, R])
elif case == "cross":
    for k in ("x0", "xs", "x1"):
        expect(k, RRR)
elif case == "path":
    if opt.smp == 1:
        expect("h0", [0, R, R])
    bits = st["bits"]
    C.check("the hooks plugin loaded with no hooks: no vCPU translates through the plugin path",
            bits.get("before-hook") == [False] * opt.smp, bits)
    C.check("with a hook: still none", bits.get("end") == [False] * opt.smp, bits)
elif case == "unload":
    c_before, py_before = st.get("before", (None, {}))
    C.check("before the unload: R hits per phase for the C and the Python hook",
            c_before == 3 * R and [py_before.get(p, 0) for p in (1, 2, 3)] == RRR, (c_before, py_before))
    C.check("the plugin is gone (the main loop unloaded it)", st.get("gone") is True, st.get("gone"))
    C.check("the program runs again after the unload, exit 0",
            out.get("run2") and out["run2"][0] == 0 and b"rc=0" in out["run2"][1], out.get("run2"))
    C.check("no hook fires after the unload", hookc.hookc_get(0) == c_before and
            dict(hits["py"]) == py_before, (hookc.hookc_get(0), dict(hits["py"])))
elif case == "smp":
    n = opt.smp
    C.check(f"every copy of the program ran ({n})", out.get("run") and
            all(f"rc{c}=0".encode() in out["run"][1] for c in range(n)), out.get("run"))
    C.check(f"C hook at h0: exactly {n} * 3R hits", hookc.hookc_get(0) == n * 3 * R,
            hookc.hookc_get(0))
    C.check(f"C churn hook at h1: exactly {n} * 3R hits", hookc.hookc_get(1) == n * 3 * R,
            hookc.hookc_get(1))
    C.check("the self-disabling hooks it added fired, at most once per add",
            0 < hookc.hookc_get(2) <= hookc.hookc_get(1), hookc.hookc_get(2))
    C.check("C hooks got a cpu and a block", hookc.hookc_get(-1) == 0, hookc.hookc_get(-1))
sys.exit(C.report(f"HOOKS {case} arch={arch} smp{opt.smp} hits={per} stray={dict(st['stray'])} "
                   f"bad={st['bad'][:3]}"))
