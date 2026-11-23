"""The bridge's translation callback is registered with QEMU only once something needs it.

Run: panda-python tests/tbtrans.py <dir>/busybox-<arch>.json [--smp N] [--case NAME]
(an image from tests/mkbusybox.py; the machine is tests/testmachine.py's)

Until a block, instruction or memory callback, block_translate or coverage is registered, QEMU
translates as it does without plugins. Whether a vCPU translates through QEMU's
plugin path is the bit plugin_gen_tb_start tests (panda_bridge_tb_trans_on). Each case runs in a
child process (the first registration happens once per process) and runs
tests/programs/blocks_<arch>.S (a loop whose block "loop" runs N times, then two stores) several
times through the guest agent:
- none: nothing registered: no vCPU ever translates through the plugin path;
- block-init, insn-init, mem-init: a start_block_exec, insn_translate + insn_exec, or
  virt_mem_after_write callback registered (disabled) before the machine starts: every vCPU
  translates through the plugin path, and the callback, enabled from hypercall 1 to hypercall 2
  (a Python call for every block or store of the whole guest is slow), sees the program;
- block, insn, mem: the same callback registered at the program's hypercall 1 in its second run,
  when its code is already translated without instrumentation: every vCPU translates through the
  plugin path after that run, and the callback sees the whole run. It is disabled at hypercall 2
  (third run: nothing), enabled at hypercall 1 of the fourth run (the whole run), deleted at its
  hypercall 2 (fifth run: nothing, the plugin path stays on) and registered again at hypercall 1
  of the sixth run (the whole run);
- hook: a hook on the loop (panda.hook, the hooks plugin, on the engine's address hooks) added
  from the host thread between two runs: it fires N times in the next run, and no vCPU ever
  translates through the plugin path;
- cov: edge coverage turned on at hypercall 1 of the second run: the map gets the program's edges.
With --smp N > 1 the program runs on whichever vCPU the guest picks, and every vCPU must switch.
There only the cases whose Python callbacks run for the program's code alone run (none,
insn-init, insn, hook, cov): a Python callback for every block or store of several vCPUs at once
leaves the guest too slow to finish.
"""
import argparse
import base64
import os
import subprocess
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "programs"))
from blocks_programs import PROGRAMS  # noqa: E402
from testlib import MAGIC, PROGRAM_BASE, PROGRAM_END, Checks, progress  # noqa: E402

PANDA_TEST = dict(area="engine", tier="guest", fixture="testmachine", arches=["x86_64", "aarch64"],
                  plugins=["hooks"], variants=dict(smp=[1, 4]), timeout=900)

N = 1000
LABELS = ["setup", "loop", "odd", "even", "fault", "fault_store", "sys"]
CASES = ["none", "block-init", "insn-init", "mem-init", "block", "insn", "mem", "hook", "cov"]
SMP_CASES = ["none", "insn-init", "insn", "hook", "cov"]
# Mid-run cases, per run: what happens at hypercall 1 and 2, and the calls the run must see.
SCHEDULE = {1: (None, None, 0), 2: ("register", "disable", N), 3: (None, None, 0),
            4: ("enable", "delete", N), 5: (None, None, 0), 6: ("register", "delete", N)}

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
ap.add_argument("--case", choices=CASES)
opt = ap.parse_args()

if opt.case is None:
    failed = []
    for case in CASES if opt.smp == 1 else SMP_CASES:
        progress(f"case {case}")
        cmd = [sys.executable, os.path.abspath(__file__), opt.image, "--smp", str(opt.smp), "--case", case]
        r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=600)
        print("\n".join(f"  [{case}] {line}" for line in r.stdout.splitlines()), flush=True)
        if r.returncode:
            failed.append(case)
    print(f"TBTRANS smp{opt.smp} {'FAIL ' + ' '.join(failed) if failed else 'PASS'}")
    sys.exit(1 if failed else 0)

import cffi  # noqa: E402

from testmachine import Agent, Machine  # noqa: E402

m = Machine(opt.image, smp=opt.smp)
panda = m.panda()
arch = m.arch
C = Checks(arch)
agent = Agent(panda)
probe = cffi.FFI()
probe.cdef("bool panda_bridge_tb_trans_on(int cpu_index);")
engine = probe.dlopen(panda.libpanda_path)
kind = opt.case.split("-")[0]
init = opt.case.endswith("-init")
st = {"run": 0, "on": False, "labels": {}, "asid": None, "hc": [], "calls": Counter(),
      "stray": Counter(), "cov": {}}


def tb_trans_on():
    return [bool(engine.panda_bridge_tb_trans_on(i)) for i in range(opt.smp)]


def ours(cpu, pc):
    return PROGRAM_BASE <= pc < PROGRAM_END and panda.current_asid(cpu) == st["asid"]


def seen(cpu, pc, counted):
    if counted and ours(cpu, pc):
        if st["on"]:
            st["calls"][st["run"]] += 1
        else:
            st["stray"][st["run"]] += 1


def on_block(cpu, tb):
    seen(cpu, panda.tb_pc(cpu, tb), panda.tb_pc(cpu, tb) == st["labels"].get("loop"))


def on_insn_translate(cpu, pc):
    return PROGRAM_BASE <= pc < PROGRAM_END


def on_insn_exec(cpu, pc):
    seen(cpu, pc, pc == st["labels"].get("loop"))
    return 0


def on_after_write(cpu, pc, addr, size, buf):
    seen(cpu, pc, True)


def on_hook(cpu, tb, hook):
    seen(cpu, st["labels"].get("loop"), True)


def register(enabled=True):
    if kind == "block":
        panda.cb_start_block_exec(name="t", enabled=enabled)(on_block)
    elif kind == "insn":
        panda.cb_insn_translate(name="t", enabled=enabled)(on_insn_translate)
        panda.cb_insn_exec(name="t2", enabled=enabled)(on_insn_exec)
    elif kind == "mem":
        panda.cb_virt_mem_after_write(name="t", enabled=enabled)(on_after_write)


def change(what):
    names = {"insn": ["t", "t2"]}.get(kind, ["t"])
    if what == "register":
        register()
    elif what == "cov-on":
        panda.enable_fast_coverage(filter_lo=PROGRAM_BASE, filter_hi=PROGRAM_END)
    elif what == "cov-off":
        panda.disable_fast_coverage()
    else:
        for name in names:
            {"enable": panda.enable_callback, "disable": panda.disable_callback,
             "delete": panda.delete_callback}[what](name)


def plan(run, op):
    """The change at hypercall op of a run, and whether the callback is on after it."""
    if kind in ("block", "insn", "mem") and not init:
        at1, at2, _ = SCHEDULE[run]
        what = at1 if op == 1 else at2
        on = {"register": True, "enable": True}.get(what, False) if what else None
        return what, on
    if kind == "cov" and run == 2:
        return ("cov-on", True) if op == 1 else ("cov-off", False)
    if init:
        return ("enable", True) if op == 1 else ("disable", False)
    if kind == "hook":
        return None, op == 1
    return None, False


if init:
    register(enabled=False)     # enabled at each run's hypercall 1, disabled at its hypercall 2


@panda.hypercall(MAGIC, observe_only=True)
def hypercall(cpu):
    op = panda.arch.get_arg(cpu, 2, convention="syscall")
    st["hc"].append((st["run"], op))
    if op == 1:
        table = panda.arch.get_arg(cpu, 3, convention="syscall")
        raw = bytes(panda.virtual_memory_read(cpu, table, 8 * len(LABELS)))
        st["labels"] = {k: int.from_bytes(raw[8 * i:8 * i + 8], "little") for i, k in enumerate(LABELS)}
        st["asid"] = panda.current_asid(cpu)
    what, on = plan(st["run"], op)
    if what:
        change(what)
    if on is not None:
        st["on"] = on
    if kind == "cov" and op == 2 and st["run"] == 2:
        st["cov"][2] = sum(1 for b in panda.fast_coverage_map() if b)


out = {"bits": {}}
RUNS = 6 if kind in ("block", "insn", "mem") and not init else 3


@panda.queue_blocking
def drive():
    try:
        out["bits"]["start"] = tb_trans_on()
        m.revert(panda, agent)
        out["bits"]["revert"] = tb_trans_on()
        agent.put("/tmp/blocks", base64.b64decode(PROGRAMS[arch]))
        out["runs"] = {}
        for run in range(1, RUNS + 1):
            st["run"] = run
            if kind == "hook" and run == 2:
                panda.hook(st["labels"]["loop"], kernel=False)(on_hook)
            out["runs"][run] = agent.run("/tmp/blocks; echo rc=$?", timeout=300)
            agent.run("true")
            out["bits"][run] = tb_trans_on()
            progress(f"run {run}: {out['runs'][run][1].strip()!r}, calls {st['calls'][run]}, "
                     f"plugin path {out['bits'][run]}")
        if kind == "cov":
            st["cov"][3] = sum(1 for b in panda.fast_coverage_map() if b)
    except Exception as e:
        out["error"] = repr(e)
    panda.end_analysis()


panda.run()

bits, runs = out["bits"], out.get("runs", {})
off, on = [False] * opt.smp, [True] * opt.smp
C.check("no error in the driver", "error" not in out, out.get("error"))
C.check("the program ran each time, exit 0",
        len(runs) == RUNS and all(rc == 0 and b"rc=0" in o for rc, o in runs.values()),
        {r: o[-120:] for r, (rc, o) in runs.items()})
C.check("hypercalls 1 and 2 in each run",
        st["hc"] == [(r, op) for r in range(1, RUNS + 1) for op in (1, 2)], st["hc"])
if kind == "none":
    C.check("no vCPU translates through the plugin path", all(b == off for b in bits.values()), bits)
elif init:
    C.check("every vCPU translates through the plugin path after the revert", bits.get("revert") == on, bits)
    C.check(f"the callback sees the whole program each run ({'N loop entries' if kind != 'mem' else 'its 2 stores'})",
            all(st["calls"][r] == (2 if kind == "mem" else N) for r in range(1, RUNS + 1)), dict(st["calls"]))
elif kind in ("block", "insn", "mem"):
    want = {r: (2 if kind == "mem" and n else n) for r, (_, _, n) in SCHEDULE.items()}
    C.check("no vCPU translates through the plugin path before the registration",
            bits.get("revert") == off and bits.get(1) == off, bits)
    C.check("every vCPU translates through the plugin path after it, also after the callback is deleted",
            all(bits.get(r) == on for r in range(2, RUNS + 1)), bits)
    C.check("registered late, disabled, enabled, deleted and registered again: the callback sees "
            "exactly the runs it is on for", {r: st["calls"][r] for r in want} == want,
            (dict(st["calls"]), want))
    C.check("no call while it is disabled or deleted", not st["stray"], dict(st["stray"]))
elif kind == "hook":
    C.check("no vCPU translates through the plugin path, before the hook or after it",
            all(b == off for b in bits.values()), bits)
    C.check(f"the hook fires N={N} times in each run after it is added",
            st["calls"][2] == N and st["calls"][3] == N, dict(st["calls"]))
elif kind == "cov":
    C.check("no vCPU translates through the plugin path before coverage", bits.get(1) == off, bits)
    C.check("every vCPU translates through the plugin path after it", bits.get(2) == on, bits)
    C.check("coverage turned on mid-run records the program's edges", st["cov"].get(2, 0) >= 3, st["cov"])
    C.check("nothing more once it is off", st["cov"].get(3) == st["cov"].get(2), st["cov"])
sys.exit(C.report(f"TBTRANS {opt.case} arch={arch} smp{opt.smp} calls={dict(st['calls'])} bits={bits}"))
