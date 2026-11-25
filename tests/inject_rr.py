"""Injected system calls and record/replay.

Run: panda-python tests/inject_rr.py <dir>/busybox-<arch>.json   (in a scratch directory: the
recording is written there; the machine is tests/testmachine.py's)

Checks:
- while a task is inside an injected call (a 2 s nanosleep injected before a busybox sleep's
  own), starting a recording is refused, with the reason;
- once the call is back (its registers restored), the recording starts;
- during the recording panda.inject is refused (InjectRefused, ERECORD), and a session queued
  before the recording started completes refused (or cancelled), never served;
- after the recording, sessions are accepted and served again.
"""
import argparse
import os
import struct
import sys
import threading
import time

from testlib import Checks, progress
from testmachine import Agent, Machine

PANDA_TEST = dict(area="inject", tier="guest", fixture="testmachine", arches=["x86_64", "aarch64"],
                  plugins=["syscalls2"], timeout=600)

ap = argparse.ArgumentParser()
ap.add_argument("image")
opt = ap.parse_args()

m = Machine(opt.image)
panda = m.panda(capabilities=["record"])
arch = m.arch
C = Checks(arch)
agent = Agent(panda)
panda.load_plugin("syscalls2", {"load-info": False})

from pandare2.inject import syscall, InjectRefused  # noqa: E402
from pandare2.inject_numbers import NUMBERS  # noqa: E402

SLEEP = 3
st = dict(arm=False, session=None, inside=threading.Event(), done=threading.Event(), result=None)


def long_call(ctx):
    ts = yield from ctx.alloc(4096)
    yield from ctx.write(ts, struct.pack("<qq", 2, 0))
    st["inside"].set()
    r = yield syscall("nanosleep", ts, 0)
    return r


@panda.ppp("syscalls2", "on_all_sys_enter")
def enter(cpu, pc, callno):
    if not st["arm"]:
        return
    abi = panda.inject.abi(cpu)
    t = NUMBERS.get(abi or "", {})
    nums = [t[n][0] for n in ("nanosleep", "clock_nanosleep") if n in t]
    if callno not in nums or panda.inject.resumed(cpu):
        return
    i = 1 if callno == t.get("nanosleep", (None,))[0] else 3
    try:
        sec = bytes(panda.virtual_memory_read(cpu, panda.arch.get_arg(cpu, i, convention="syscall"), 4))
    except ValueError:
        return
    if int.from_bytes(sec, "little") != SLEEP:
        return
    st["arm"] = False
    s = panda.inject(long_call, cpu=cpu, before_syscall=True, timeout=60)
    s.done(lambda r, e: (st.update(result=(r, e)), st["done"].set()))


def try_record(name):
    try:
        panda.record(name)
        return None
    except Exception as e:
        return e


@panda.queue_blocking
def drive():
    m.revert(panda, agent)
    st["arm"] = True
    agent.run(f"sleep {SLEEP} > /dev/null 2>&1 & echo started")
    ok = st["inside"].wait(60)
    C.check("a session is inside its injected nanosleep", ok)
    # a mutating callback refuses a recording by itself (pandare2): this one's work is done
    panda.disable_ppp("enter")
    time.sleep(0.3)
    held = panda.inject.stats()["held"]
    err = try_record("inject-rr")
    # (the engine's reason, "... inside an injected system call ...", is on stderr)
    C.check("record refused while a task is inside an injected call",
            held == 1 and err is not None, f"held {held}: {err!r}")
    C.check("the session completes", st["done"].wait(60) and st["result"][1] is None,
            repr(st["result"]))
    stats = panda.inject.stats()
    progress(f"inject_rr: after the session: {stats}")
    C.check("no thread is inside an injected call now", stats["held"] == 0, str(stats))
    # an address space that never runs: queued, never served
    early = {}
    ev = threading.Event()
    panda.inject(lambda ctx: (yield syscall("getpid")), asid=0x1230000, timeout=60).done(
        lambda r, e: (early.update(r=r, e=e), ev.set()))
    err = try_record("inject-rr")
    C.check("record starts once the call is back", err is None, repr(err))
    if err is None:
        panda.wait_rr("record") if hasattr(panda, "wait_rr") else time.sleep(2)
        refused = None
        t = time.monotonic()
        while time.monotonic() - t < 20:
            try:
                panda.inject(lambda ctx: (yield syscall("getpid")), asid=0x1230000, timeout=1)
            except InjectRefused as e:
                refused = e
                break
            time.sleep(0.05)
        C.check("panda.inject during the recording: InjectRefused (ERECORD)",
                refused is not None and refused.status == -4, repr(refused))
        st_, out = agent.run("echo alive")
        C.check("the guest runs on while recording", out.strip() == b"alive", repr(out))
        ev.wait(30)
        C.check("a session queued before the recording is not served",
                early.get("e") is not None and early["e"].status in (-4, -3), repr(early))
        panda.end_record()
        t = time.monotonic()
        after = None
        while time.monotonic() - t < 20:
            try:
                after = panda.inject(lambda ctx: (yield syscall("getpid")), cpu=None,
                                     asid=0x1230000, timeout=0.1)
                break
            except InjectRefused:
                time.sleep(0.05)
        C.check("after the recording, sessions are accepted", after is not None)
    panda.end_analysis()


panda.run()
rc = C.report(f"inject_rr {arch}")
for f in os.listdir("."):
    if f.startswith("inject-rr"):
        os.unlink(f)
sys.exit(rc)
