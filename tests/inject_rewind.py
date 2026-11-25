"""panda.inject.rewind: a skipped system call made again after a session (the engine path).

Run: panda-python tests/inject_rewind.py <dir>/busybox-<arch>.json [--smp N]
(the machine is tests/testmachine.py's)

panda_inject_rewind() is for a kernel-side system-call hook (a guest module's entry hypercall)
that holds a call: the kernel skips it and returns to user mode, and the engine runs a session
at that return, then puts the task back at its trap with the call's number and arguments. These
test machines have no such guest module, so this test EMULATES THE KERNEL SIDE from the host, to
exercise only the engine path: at a busybox sleep's nanosleep entry (a syscalls2 callback, before
the kernel reads the registers) it replaces the call's number with getpid's, so the kernel runs
getpid instead and returns to user mode with getpid's value in the return register, as a skipped
call would come back; then it asks for the rewind. Checks:
- the session runs at that return: getpid, a buffer, openat/read of a file, write() to the task's
  own stdout;
- the task then makes its nanosleep after all: the sleep really sleeps (about its 3 s) and exits
  0, and that entry is delivered flagged resumed;
- a rewind with no session (next NULL) puts a second skipped call back the same way.
"""
import argparse
import sys
import threading

from testlib import Checks, progress
from testmachine import Agent, Machine

PANDA_TEST = dict(area="inject", tier="guest", fixture="testmachine",
                  arches=["x86_64", "aarch64", "arm", "mipsel"], plugins=["syscalls2"],
                  variants=dict(smp=[1, 4]), timeout=600)

SLEEP = 3
MARK = b"rewound-read\n"
TRAP_LEN = {"x86_64": 2, "i386": 2}             # else 4 (busybox armel runs in ARM state)

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
ap.add_argument("--stress", type=int, default=0)    # (matrix.sh passes it; unused)
ap.add_argument("--json")
opt = ap.parse_args()

m = Machine(opt.image, smp=opt.smp)
panda = m.panda()
arch = m.arch
C = Checks(arch)
agent = Agent(panda)
panda.load_plugin("syscalls2", {"load-info": False})

from pandare2.inject import syscall  # noqa: E402
from pandare2.inject_numbers import NUMBERS  # noqa: E402

st = dict(mode=None, asid=None, done=threading.Event(), result=None, resumed=0, rewinds=0)


def sleep_ts(cpu, callno):
    abi = panda.inject.abi(cpu)
    t = NUMBERS.get(abi or "", {})
    for name, i in (("nanosleep", 0), ("clock_nanosleep", 2), ("clock_nanosleep_time32", 2)):
        if name in t and t[name][0] == callno:
            return panda.arch.get_arg(cpu, i + 1, convention="syscall")
    return None


def is_our_sleep(cpu, callno):
    ts = sleep_ts(cpu, callno)
    if ts is None:
        return False
    try:
        b = bytes(panda.virtual_memory_read(cpu, ts, 8))
    except ValueError:
        return False
    return SLEEP in (int.from_bytes(b[:4], "little"), int.from_bytes(b[:4], "big"),
                     int.from_bytes(b[4:], "big"))


def session(ctx):
    out = {"pid": (yield syscall("getpid"))}
    buf = yield from ctx.alloc(4096)
    yield from ctx.write(buf, b"/tmp/rw.txt\0")
    fd = yield syscall("openat", -100, buf, 0, check=True)
    n = yield syscall("read", fd, buf + 256, 100)
    out["data"] = (yield from ctx.read(buf + 256, n)) if n > 0 else b""
    out["wrote"] = yield syscall("write", 1, buf + 256, max(n, 0))
    yield syscall("close", fd)
    return out


@panda.ppp("syscalls2", "on_all_sys_enter")
def enter(cpu, pc, callno):
    if st["mode"] is None:
        return
    asid = panda.current_asid(cpu)
    if st["asid"] is not None:
        if asid != st["asid"] or sleep_ts(cpu, callno) is None:
            return
        if panda.inject.resumed(cpu):
            st["resumed"] += 1
        return
    if not is_our_sleep(cpu, callno):
        return
    st["asid"] = asid
    abi = panda.inject.abi(cpu)
    nargs = 4 if callno != NUMBERS[abi].get("nanosleep", (None,))[0] else 2
    args = [panda.arch.get_arg(cpu, i + 1, convention="syscall") for i in range(nargs)]
    sp = panda.arch.get_reg(cpu, panda.arch.reg_sp)
    length = TRAP_LEN.get(arch, 4)
    # the emulated kernel-side skip: the kernel runs getpid instead and returns its value
    panda.arch.set_arg(cpu, 0, NUMBERS[abi]["getpid"][0], convention="syscall")
    gen = session if st["mode"] == "session" else None
    s = panda.inject.rewind(asid, sp, pc + length, callno, args, length, gen=gen, timeout=30,
                            nargs=nargs)
    s.done(lambda r, e: (st.update(result=(r, e)), st["done"].set()))


def one(mode):
    st.update(mode=mode, asid=None, result=None, resumed=0)
    st["done"].clear()
    agent.run(f"sh -c 't0=$(date +%s); sleep {SLEEP}; echo rc=$? slept $(( $(date +%s) - t0 ))'"
              f" > /tmp/rw-{mode}.out 2>&1 < /dev/null & echo started")
    ok = st["done"].wait(60)
    _, out = agent.run(f"sleep {SLEEP + 2}; cat /tmp/rw-{mode}.out")
    st["mode"] = None
    progress(f"inject_rewind: {mode}: {st['result']}; output {out!r}; resumed {st['resumed']}")
    C.check(f"{mode}: the rewind session completed", ok and st["result"][1] is None,
            repr(st["result"]))
    slept = None
    for tok in out.split():
        if tok.isdigit():
            slept = int(tok)
    C.check(f"{mode}: the skipped nanosleep was made again (sleep slept, exit 0)",
            b"rc=0" in out and slept is not None and slept >= SLEEP - 1, repr(out))
    C.check(f"{mode}: its entry was delivered again, flagged resumed", st["resumed"] >= 1,
            str(st["resumed"]))
    return out


@panda.queue_blocking
def drive():
    try:
        m.revert(panda, agent)
        agent.run(f"printf '{MARK.decode().strip()}\\n' > /tmp/rw.txt")
        out = one("session")
        r = st["result"][0] if st["result"] else None
        C.check("session: getpid, openat/read, write to the task's stdout",
                bool(r) and r["data"] == MARK and r["wrote"] == len(MARK) and MARK in out,
                f"{r} {out!r}")
        one("bare")
    except Exception as e:
        import traceback
        C.check("the checks ran to the end", False, repr(e))
        progress("inject_rewind: " + traceback.format_exc().replace("\n", " | ")[-800:])
    stats = panda.inject.stats()
    progress(f"inject_rewind: engine stats {stats}")
    C.check("engine: two rewinds, nothing left held", stats["rewinds"] == 2 and stats["held"] == 0,
            str(stats))
    panda.end_analysis()


panda.run()
sys.exit(C.report(f"inject_rewind {arch} smp{opt.smp}"))
