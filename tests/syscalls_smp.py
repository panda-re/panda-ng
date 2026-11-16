"""syscalls2 with return callbacks while every vCPU makes system calls.

Run: panda-python tests/syscalls_smp.py <dir>/busybox-<arch>.json --smp 4   (an image from
tests/mkbusybox.py; the machine is tests/testmachine.py's)

With a return callback subscribed, syscalls2 adds a hooks-plugin hook at each call's return
address and keeps the call's context until it returns. Calls enter and return on every vCPU at
once, and a task may return on another vCPU than it entered on. One `dd bs=1` per vCPU, pinned
there, copies a COUNT-byte file: COUNT reads and writes (and a read at its end); the rest of the
guest makes a few more. Checks:
- the run ends (no crash: the hooks plugin's per-vCPU lists of new hooks and syscalls2's map of
  running calls are written from several vCPUs at once);
- on_all_sys_enter sees at least COUNT reads and COUNT writes per dd, from every vCPU (a block
  translated on one vCPU and run on another reports its calls with their own number);
- on_all_sys_return sees as many of them return.
"""
import argparse
import sys
import threading

from testlib import Checks, progress
from testmachine import Agent, Machine

PANDA_TEST = dict(area="syscalls", tier="guest", fixture="testmachine", arches=["x86_64", "aarch64"],
                  plugins=["syscalls2", "hooks"], variants=dict(smp=[1, 4]), timeout=900)

COUNT = 20000
# (read, write) numbers
CALLS = {"x86_64": (0, 1), "aarch64": (63, 64), "riscv64": (63, 64)}

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
ap.add_argument("--rounds", type=int, default=3)
opt = ap.parse_args()

m = Machine(opt.image, smp=opt.smp)
panda = m.panda()
arch = m.arch
C = Checks(arch)
READ, WRITE = CALLS[arch]
agent = Agent(panda)
lock = threading.Lock()
seen = {"enter": {}, "return": {}, "cpus": set(), "on": False}


def count(kind, cpu, callno):
    if not seen["on"] or callno not in (READ, WRITE):
        return
    with lock:
        d = seen[kind]
        d[callno] = d.get(callno, 0) + 1
        if kind == "enter":
            seen["cpus"].add(cpu.cpu_index)


panda.load_plugin("syscalls2", {"load-info": False})


@panda.ppp("syscalls2", "on_all_sys_enter")
def enter(cpu, pc, callno):
    count("enter", cpu, callno)


@panda.ppp("syscalls2", "on_all_sys_return")
def ret(cpu, pc, callno):
    count("return", cpu, callno)


out = {}


@panda.queue_blocking
def drive():
    try:
        m.revert(panda, agent, timeout=300)
        rc, o = agent.run("nproc")
        out["nproc"] = o.decode().strip()
        agent.put("/tmp/src", b"x" * COUNT)
        seen["on"] = True
        out["runs"] = []
        for r in range(opt.rounds):
            progress(f"round {r + 1}: dd bs=1 count={COUNT} on each of {opt.smp} vCPUs")
            cmd = " ".join(f"taskset {1 << c:x} dd if=/tmp/src of=/tmp/dst{c} bs=1 2>&1 &"
                           for c in range(opt.smp)) + " wait; echo done"
            out["runs"].append(agent.run(cmd, timeout=600))
        seen["on"] = False
    except Exception as e:
        out["error"] = repr(e)
    panda.end_analysis()


panda.run()

want = COUNT * opt.smp * opt.rounds
runs = out.get("runs", [])
C.check("the guest has the vCPUs asked for", out.get("nproc") == str(opt.smp), out.get("nproc"))
C.check("every round ran to the end, each dd copying the whole file",
        len(runs) == opt.rounds and all(rc == 0 and o.strip().endswith(b"done") and
                                        o.count(b"%d+0 records out" % COUNT) == opt.smp for rc, o in runs),
        out.get("error") or [(rc, o[-300:]) for rc, o in runs])
for name, no in (("read", READ), ("write", WRITE)):
    e, r = seen["enter"].get(no, 0), seen["return"].get(no, 0)
    C.check(f"on_all_sys_enter saw every dd {name} (at least {want})", e >= want, e)
    C.check(f"on_all_sys_return saw every dd {name} return (at least {want})", r >= want, r)
C.check("calls entered on every vCPU", seen["cpus"] == set(range(opt.smp)), sorted(seen["cpus"]))
sys.exit(C.report(f"syscalls_smp {arch} smp{opt.smp}"))
