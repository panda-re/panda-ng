"""Injected system calls: panda.inject on a test machine.

Run: panda-python tests/inject.py <dir>/busybox-<arch>.json [--smp N] [--stress N] [--json OUT]
(the machine is tests/testmachine.py's)

Held entries (every arch with an injector): a busybox `sleep` runs with its pid in a file. The
first entry of its nanosleep (clock_nanosleep) is held from a syscalls2 entry callback, and a
session runs before it, HyDE's way: getpid and getppid, an anonymous mapping, a path written into
it, openat/read of a file the test wrote, write() of those bytes to the task's own stdout, close,
and an openat that fails (ENOENT, with check=True raising in the generator). Checks:
- the session's results: the pid is the sleep's, the bytes are the file's, ENOENT is -2;
- the held entry is delivered again, flagged resumed, and the sleep then sleeps and exits 0;
- its stdout holds the bytes the injected write() wrote;
- a fork-like call is refused inside the generator.

Sessions at a user-mode boundary (the arches with a tests/programs/pagein_target.c build): the
program spins in user mode with two copies of a value in registers (and, unless --no-signals, a
timer signal whose handler longjmps). Sessions by address space, from this thread, another Python
thread and a kernel-mode callback (asid_changed), each getpid + getppid + gettid; and the HyDE
sequence again. Checks: every session completes (OK, or abandoned by the program's longjmp), the
pids are the program's, no register of the program changes (its two copies never differ), it
exits 0 with its memory intact, an address space that never runs times out, and the guest console
shows no oops, BUG or signal.
"""
import argparse
import base64
import gzip
import json
import random
import re
import sys
import threading
import time

from testlib import Checks, progress
from testmachine import Agent, Machine

PANDA_TEST = dict(area="inject", tier="guest", fixture="testmachine",
                  arches=["x86_64", "aarch64", "riscv64", "arm", "i386", "mipsel", "mips",
                          "mips64el", "mips64", "ppc", "ppc64"],
                  plugins=["syscalls2", "hypercaller"],
                  variants=dict(smp=[1, 4], intel=dict(values=[False, True], arches=["x86_64"])),
                  timeout=900)

MAGIC = 0x46525431
BAD = re.compile(r"(Oops|BUG:|Kernel panic|Unable to handle|general protection|segfault|"
                 r"Internal error|Call Trace|PT SIGNAL)", re.I)
DEBUG = bool(__import__("os").environ.get("INJECT_TEST_DEBUG"))
MARK = b"injected-read-%d\n" % random.Random().randrange(10**9)

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
ap.add_argument("--stress", type=int, default=200)
ap.add_argument("--intel", action="store_true")
ap.add_argument("--no-signals", action="store_true")
ap.add_argument("--json")
opt = ap.parse_args()

m = Machine(opt.image, smp=opt.smp, intel=opt.intel)
panda = m.panda()
arch = m.arch
prog = "ppc64le" if arch == "ppc64" and "64le" in str(m.image.get("extra_args")) else arch
nosig = opt.no_signals or arch in ("mips", "mipsel")
C = Checks(arch)
agent = Agent(panda)
panda.load_plugin("syscalls2", {"load-info": False})

from programs.pagein_programs import PROGRAMS  # noqa: E402
from pandare2.inject import (syscall, InjectFailed, InjectRefused, InjectTimeout,  # noqa: E402
                             InjectAbandoned)
from pandare2.inject_numbers import NUMBERS  # noqa: E402

st = dict(lock=threading.Lock(), hold=None, holds=0, resumed=0, hold_asid=None,
          pt_asid=None, ready=threading.Event(), done=threading.Event(), done_args=None,
          counts={}, outstanding=0, alldone=threading.Event(), k_budget=0, kb_budget=0, pids=set(),
          fork_refused=None)


def hyde(path, want_fork_refusal=False):
    """HyDE's demo: getpid, a buffer, open/read a file, write it to the task's stdout"""
    def gen(ctx):
        out = {}
        out["pid"] = yield syscall("getpid")
        out["ppid"] = yield syscall("getppid")
        buf = yield from ctx.alloc(4096)
        out["buf"] = buf
        yield from ctx.write(buf, path.encode() + b"\0")
        fd = yield syscall("openat", -100, buf, 0, check=True)
        n = yield syscall("read", fd, buf + 256, 200)
        out["data"] = (yield from ctx.read(buf + 256, n)) if n > 0 else b""
        out["wrote"] = yield syscall("write", 1, buf + 256, max(n, 0))
        yield syscall("close", fd)
        yield from ctx.write(buf, b"/nonexistent/inject\0")
        try:
            yield syscall("openat", -100, buf, 0, check=True)
            out["enoent"] = "opened"
        except InjectFailed as e:
            out["enoent"] = -e.errno
        if want_fork_refusal:
            try:
                yield syscall("fork")
                out["fork"] = "injected"
            except InjectRefused:
                out["fork"] = "refused"
        return out
    return gen


SLEEP = 3       # the sleep's seconds: how its nanosleep is told from the agent's polls


def sleep_ts(cpu, callno):
    """The timespec argument if callno is a nanosleep of this ABI, else None"""
    abi = panda.inject.abi(cpu)
    if abi is None:
        return None
    t = NUMBERS[abi]
    for name, i in (("nanosleep", 0), ("clock_nanosleep", 2), ("clock_nanosleep_time64", 2),
                    ("clock_nanosleep_time32", 2)):
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
    # tv_sec, 32- or 64-bit, either byte order
    return SLEEP in (int.from_bytes(b[:4], "little"), int.from_bytes(b[:4], "big"),
                     int.from_bytes(b[4:], "big"))


@panda.ppp("syscalls2", "on_all_sys_enter")
def enter(cpu, pc, callno):
    h = st["hold"]
    if h is None:
        return
    if DEBUG and sleep_ts(cpu, callno) is not None:
        ts = sleep_ts(cpu, callno)
        try:
            b = bytes(panda.virtual_memory_read(cpu, ts, 8)).hex()
        except ValueError:
            b = "unreadable"
        progress(f"inject: sleep-like entry {callno} asid {panda.current_asid(cpu):#x} ts {ts:#x} "
                 f"{b} resumed {panda.inject.resumed(cpu)} abi {panda.inject.abi(cpu)}")
    asid = panda.current_asid(cpu)
    if st["hold_asid"] is not None:
        # the held sleep's again: its asid and a sleep call (its timespec may be out of the TLB
        # by now on soft-TLB CPUs, e500 and MIPS)
        if asid != st["hold_asid"] or sleep_ts(cpu, callno) is None:
            return
    elif not is_our_sleep(cpu, callno):
        return
    if DEBUG:
        progress(f"inject: entry {callno} asid {asid:#x} resumed {panda.inject.resumed(cpu)} "
                 f"current {panda.plugins['syscalls2'].syscalls2_current_call_ptr(cpu, panda.ffi.new('uint32_t *'))}")
    if panda.inject.resumed(cpu):
        with st["lock"]:
            st["resumed"] += 1
        return
    if st["holds"]:
        return
    try:
        s = panda.inject(hyde("/tmp/inj.txt", want_fork_refusal=True), cpu=cpu,
                         before_syscall=True, timeout=30)
    except Exception as e:
        h["error"] = e
        h["event"].set()
        st["hold"] = None
        return
    st["holds"] += 1
    st["hold_asid"] = asid
    h["session"] = s
    s.done(lambda r, e: (h.update(result=r, error=e), h["event"].set()))


@panda.hypercall(MAGIC, mutates=True)
def hc(cpu):
    op, a, b = (panda.arch.get_arg(cpu, i, convention="syscall") for i in range(2, 5))
    if op == 1:                                          # READY
        st["pt_asid"] = panda.current_asid(cpu)
        st["ready"].set()
    elif op == 2:                                        # DONE
        st["done_args"] = (a, b)
        st["done"].set()
    panda.guest.set_retval(cpu, 0)


def count(ctx, what):
    with st["lock"]:
        st["counts"].setdefault(ctx, {}).setdefault(what, 0)
        st["counts"][ctx][what] += 1


def ids(ctx):
    pid = yield syscall("getpid")
    ppid = yield syscall("getppid")
    tid = yield syscall("gettid")
    return pid, ppid, tid


def submit(ctx):
    with st["lock"]:
        st["outstanding"] += 1
        st["alldone"].clear()

    def done(r, e):
        if e is None:
            with st["lock"]:
                st["pids"].add(r[0])
            count(ctx, "ok")
        else:
            count(ctx, type(e).__name__)
        with st["lock"]:
            st["outstanding"] -= 1
            if st["outstanding"] == 0:
                st["alldone"].set()
    try:
        panda.inject(ids, asid=st["pt_asid"], timeout=30).done(done)
    except Exception as e:
        count(ctx, "refused:" + type(e).__name__)
        with st["lock"]:
            st["outstanding"] -= 1
            if st["outstanding"] == 0:
                st["alldone"].set()


@panda.cb_asid_changed(name="kctx", enabled=False)
def kctx(cpu, old, new):
    if st["k_budget"] > 0 and st["pt_asid"] is not None:
        st["k_budget"] -= 1
        submit("kernel-callback")
    return False


@panda.cb_start_block_exec(name="kblock", enabled=False)
def kblock(cpu, tb):
    # kernel mode, anywhere: interrupts, softirqs, spinlocks held, the scheduler
    if st["kb_budget"] <= 0:
        panda.disable_callback("kblock")    # a Python call per block stalls the guest
        return
    if panda.in_kernel_mode(cpu):
        st["kb_budget"] -= 1
        submit("kernel-block")
        if st["kb_budget"] == 0:
            panda.disable_callback("kblock")


def other_thread(n):
    for _ in range(n):
        submit("other-thread")
        time.sleep(0.002)


def held():
    """A held nanosleep entry of a busybox sleep"""
    agent.run(f"printf '{MARK.decode().strip()}\\n' > /tmp/inj.txt")
    h = dict(event=threading.Event())
    st["hold"] = h
    agent.run(f"sh -c 'echo $$ > /tmp/sleep.pid; exec sleep {SLEEP}' > /tmp/sleep.out 2>&1 "
              "< /dev/null & echo started")
    ok = h["event"].wait(120)
    C.check("hold: a session ran before the held nanosleep", ok, "no session")
    if not ok:
        return
    st_, out = agent.run(f"sleep {SLEEP + 1}; cat /tmp/sleep.pid; echo ==; cat /tmp/sleep.out")
    st["hold"] = None
    pidtxt, _, sout = out.partition(b"==\n")
    progress(f"inject: hold: {h.get('result')} error {h.get('error')!r}; stdout {sout!r}")
    r, e = h.get("result"), h.get("error")
    C.check("hold: the session completed", e is None and r is not None, repr(e))
    if r:
        try:
            pid = int(pidtxt.strip())
        except ValueError:
            pid = None
        C.check("hold: getpid is the sleep's pid", r["pid"] == pid, f"{r['pid']} != {pid}")
        C.check("hold: read() returned the file's bytes", r["data"] == MARK, repr(r["data"]))
        C.check("hold: write() wrote them to the task's stdout", r["wrote"] == len(MARK) and
                MARK in sout, f"{r['wrote']} {sout!r}")
        C.check("hold: a failing openat gives -ENOENT (check=True raised in the generator)",
                r["enoent"] == -2, str(r["enoent"]))
        C.check("hold: fork is refused in the generator", r.get("fork") == "refused",
                str(r.get("fork")))
    C.check("hold: the held entry was delivered again, flagged resumed", st["resumed"] >= 1,
            str(st["resumed"]))


def boundary():
    """Sessions by address space into a spinning program"""
    agent.put("/tmp/pt", gzip.decompress(base64.b64decode(PROGRAMS[prog])))
    secs = 8 + opt.stress // 100
    agent.run(f"(/tmp/pt /tmp {secs} spin {'nosig' if nosig else 'sig'} > /tmp/pt.out 2>&1; "
              "echo exit=$? >> /tmp/pt.out) &")
    if not st["ready"].wait(120):
        C.check("program ready", False, "no READY hypercall")
        return
    asid = st["pt_asid"]
    progress(f"inject: program asid {asid:#x}")
    s = panda.inject(hyde("/tmp/inj.txt"), asid=asid, timeout=30)
    try:
        r = s.result(120)
        C.check("boundary: the HyDE sequence (getpid, mmap, openat, read, write, close)",
                r["data"] == MARK and r["enoent"] == -2 and r["wrote"] == len(MARK), str(r))
        pid0 = r["pid"]
    except InjectAbandoned:
        C.check("boundary: the HyDE sequence", True)    # a longjmp took the thread: retry
        pid0 = panda.inject(ids, asid=asid).result(120)[0]
    except Exception as e:
        C.check("boundary: the HyDE sequence", False, repr(e))
        pid0 = None
    try:
        panda.inject(ids, asid=0x1230000, timeout=0.2).result(60)
        C.check("an address space that never runs: InjectTimeout", False, "completed")
    except InjectTimeout:
        C.check("an address space that never runs: InjectTimeout", True)

    t = time.monotonic()
    third = max(1, opt.stress // 3)
    st["k_budget"] = third // 2
    st["kb_budget"] = third - third // 2
    panda.enable_callback("kctx")
    panda.enable_callback("kblock")
    th = threading.Thread(target=other_thread, args=(third,))
    th.start()
    for _ in range(third):
        submit("blocking-thread")
        time.sleep(0.001)
    th.join()
    if not st["alldone"].wait(300):
        C.check("stress: every session completed", False, f"{st['outstanding']} outstanding")
    panda.disable_callback("kctx")
    panda.disable_callback("kblock")
    progress(f"inject: stress {time.monotonic() - t:.1f}s {json.dumps(st['counts'])} "
             f"engine {panda.inject.stats()}")
    C.check("stress: every session completed", st["outstanding"] == 0, str(st["outstanding"]))
    bad = sum(n for c in st["counts"].values() for k, n in c.items()
              if k not in ("ok", "InjectAbandoned"))
    C.check("stress: every session OK (or abandoned by the program's longjmp)", bad == 0,
            json.dumps(st["counts"]))
    C.check("stress: every getpid is the program's", st["pids"] <= {pid0}, str(st["pids"]))
    C.check("stress: sessions from kernel mode were made",
            "kernel-callback" in st["counts"] or "kernel-block" in st["counts"],
            str(list(st["counts"])))
    if not st["done"].wait(secs * 6 + 60):
        C.check("program finished", False, "no DONE hypercall")
    else:
        mism, badb = st["done_args"]
        C.check("program: no register changed under it", mism == 0, str(mism))
        C.check("program: its memory is intact", badb == 0, str(badb))
    time.sleep(0.5)
    st_, out = agent.run("cat /tmp/pt.out")
    C.check("program exits 0", b"exit=0" in out, out.decode(errors="replace")[-300:])


@panda.queue_blocking
def drive():
    try:
        m.revert(panda, agent)
        held()
        if prog in PROGRAMS:
            boundary()
        else:
            progress(f"inject: no pagein_target build for {prog}: boundary sessions skipped")
    except Exception as e:
        import traceback
        C.check("the checks ran to the end", False, repr(e))
        progress("inject: " + traceback.format_exc().replace("\n", " | ")[-800:])
    progress(f"inject: engine stats {panda.inject.stats()}")
    st_, out = agent.run("echo alive")
    C.check("the guest runs on", out.strip() == b"alive", repr(out))
    con = m.console_tail(n=1 << 20)
    hits = [ln for ln in con.splitlines() if BAD.search(ln)]
    C.check("guest console: no oops, BUG or segfault", not hits, " | ".join(hits[:3]))
    panda.end_analysis()


panda.run()
rc = C.report(f"inject {arch} smp{opt.smp}")
if opt.json:
    with open(opt.json, "w") as f:
        json.dump(dict(arch=arch, smp=opt.smp, counts=st["counts"],
                       results={k: v[0] for k, v in C.results.items()}), f, indent=1)
sys.exit(rc)
