"""Page-in requests: panda.pagein and virtual_memory_read(fault=True) on a test machine.

Run: panda-python tests/pagein.py <dir>/busybox-<arch>.json [--smp N] [--stress N] [--json OUT]
(the machine is tests/testmachine.py's)

tests/programs/pagein_target.c maps, at fixed addresses, a file dropped from the page cache, never
touched anonymous memory, a range for the test to write, no mapping at all, a file truncated
under its mapping, PROT_NONE and a read-only file mapping; it tells the test its address space by
hypercall, then spins in user mode with two copies of a value in registers (or sleeps), and at
the end checks its own memory. Checks:
- reads from the queue_blocking thread page in: file bytes as written, zero anonymous memory;
- a write pages in for writing; the program sees exactly those bytes;
- a range the kernel won't map (no mapping, truncated file, PROT_NONE, a kernel address) and a
  write to a read-only mapping fail with PageInFailed and the guest's errno, and the program
  gets no signal and keeps running;
- inside a callback, fault=True returns what is present, and raises PageInWouldBlock for what
  isn't; the completion form works from a user-mode callback (the hypercall), a kernel-mode one
  (asid_changed) and another Python thread;
- an address space that never runs, and the program's after it exited: PageInTimeout;
- --stress N requests from all of those contexts while the program spins: every one completes,
  with data that matches, no register of the program changes (its two copies never differ), it
  exits 0, and the guest console shows no oops, BUG or signal;
- the program's exit is clean (no SIGSEGV/SIGBUS: it would exit 99/98).
"""
import argparse
import base64
import errno
import gzip
import json
import random
import re
import struct
import sys
import threading
import time

from testlib import Checks, progress
from testmachine import Agent, Machine

# Every test-machine arch: x86_64, aarch64 and riscv64 serve requests; the others check that
# they are refused. --stress 2000 is the long form.
PANDA_TEST = dict(area="pagein", tier="guest", fixture="testmachine",
                  arches=["x86_64", "aarch64", "riscv64", "arm", "i386", "mipsel", "mips", "mips64el",
                          "mips64", "ppc", "ppc64"],
                  plugins=["hypercaller"],
                  variants=dict(smp=[1, 4], intel=dict(values=[False, True], arches=["x86_64"])),
                  timeout=900)

MAGIC = 0x46525431
PG, NP = 4096, 64
# The program's layout: BASE + n * STEP, which it reports in its READY hypercall (layout())
TAG, FILEA, ANON, WR, HOLE, TRUNC, NONE, RO, NONE2, SW, GUARDR, GROW = (0x7e0000000000 + i * 0x10000000 for i in range(12))
KADDR = {"x86_64": 0xffffffff81000000, "aarch64": 0xffff800080000000, "riscv64": 0xffffffff80000000,
         "arm": 0xc0008000, "mipsel": 0x80100000, "mips": 0x80100000, "mips64el": 0xffffffff80100000,
         "mips64": 0xffffffff80100000, "ppc": 0xc0000000, "ppc64": 0xc000000000000000,
         "loongarch64": 0x9000000000200000}


def layout(base, step):
    global TAG, FILEA, ANON, WR, HOLE, TRUNC, NONE, RO, NONE2, SW, GUARDR, GROW
    TAG, FILEA, ANON, WR, HOLE, TRUNC, NONE, RO, NONE2, SW, GUARDR, GROW = (base + i * step for i in range(12))
BAD = re.compile(r"(Oops|BUG:|Kernel panic|Unable to handle|general protection|segfault|"
                 r"Internal error|Call Trace|PT SIGNAL)", re.I)


E = "<"     # the guest's byte order, from the agent once it has polled


def Q(v):
    """A 64-bit word as the guest stores it"""
    return struct.pack(E + "Q", v & (2**64 - 1))


def pat(va):
    return va ^ 0xa5a5a5a5a5a5a5a5


def expect_file(addr, n):
    a0 = addr & ~7
    words = b"".join(Q(pat(a0 + i)) for i in range(0, n + 16, 8))
    return words[addr - a0:addr - a0 + n]


ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
ap.add_argument("--stress", type=int, default=200)
ap.add_argument("--secs", type=int, default=0)
ap.add_argument("--intel", action="store_true")
ap.add_argument("--json")
opt = ap.parse_args()

m = Machine(opt.image, smp=opt.smp, intel=opt.intel)
panda = m.panda()
arch = m.arch
# the program build: ppc64 machines run big- or little-endian kernels
prog = "ppc64le" if arch == "ppc64" and "64le" in str(m.image.get("extra_args")) else arch
C = Checks(arch)
agent = Agent(panda)
from programs.pagein_programs import PROGRAMS  # noqa: E402
from pandare2.pagein import (PageInFailed, PageInRefused, PageInTimeout, PageInWouldBlock)  # noqa: E402

st = dict(asid=None, ready=threading.Event(), done=threading.Event(), done_args=None,
          cb_sync=None, cb_block=None, k_budget=0, counts={}, wrong=0, lock=threading.Lock(),
          outstanding=0, alldone=threading.Event(), kb_budget=0)


def count(ctx, what):
    with st["lock"]:
        st["counts"].setdefault(ctx, {}).setdefault(what, 0)
        st["counts"][ctx][what] += 1


def check_data(addr, data):
    if FILEA <= addr < FILEA + NP * PG:
        return data == expect_file(addr, len(data))
    if SW <= addr < SW + NP * PG:
        a0 = addr & ~7
        return data == b"".join(Q(pat(a0 + i)) for i in range(0, len(data) + 16, 8))[addr - a0:addr - a0 + len(data)]
    if ANON <= addr < ANON + NP * PG:
        return data == bytes(len(data))
    return True


def completion(ctx, addr):
    def done(data, err):
        if err is None:
            ok = check_data(addr, data)
            count(ctx, "ok" if ok else "WRONG")
        else:
            count(ctx, type(err).__name__ + (f":{errno.errorcode.get(err.guest_errno)}"
                                              if err.guest_errno else ""))
        with st["lock"]:
            st["outstanding"] -= 1
            if st["outstanding"] == 0:
                st["alldone"].set()
    return done


def submit(ctx, addr, n):
    with st["lock"]:
        st["outstanding"] += 1
        st["alldone"].clear()
    try:
        panda.pagein.read(st["asid"], addr, n, completion(ctx, addr), timeout_ns=30_000_000_000)
    except Exception as e:
        count(ctx, "refused:" + type(e).__name__)
        with st["lock"]:
            st["outstanding"] -= 1
            if st["outstanding"] == 0:
                st["alldone"].set()


def walk(root, va):
    """The guest's own page-table entry for va: ("leaf", level, value) or ("empty", level, 0)."""
    table = root
    for level in range(4):
        idx = (va >> (39 - 9 * level)) & 511
        e = int.from_bytes(panda.physical_memory_read(table + idx * 8, 8), "little")
        valid = e & 1
        if level == 3 or not valid or (arch == "x86_64" and level in (1, 2) and e & 0x80) or                 (arch == "aarch64" and level in (1, 2) and (e & 3) == 1):
            return ("empty" if e == 0 else "leaf", level, e)
        table = e & 0x0000fffffffff000
    return ("leaf", 3, e)


def classify(e):
    kind, level, v = e
    if kind == "empty":
        return f"empty at level {level}"
    present = v & 1
    if present:
        return f"present (level {level})"
    if arch == "x86_64":
        return "PROT_NONE (not present, _PAGE_PROTNONE)" if v & 0x100 else "swap/migration entry"
    if arch == "aarch64":
        return "PROT_NONE (not valid, PTE_PROT_NONE)" if v >> 58 & 1 else "swap/migration entry"
    return hex(v)


def rand_target(rng):
    kind = rng.choice(["file", "anon", "hole", "none", "trunc", "swap", "none2"])
    base = dict(file=FILEA, anon=ANON, hole=HOLE, none=NONE, trunc=TRUNC, swap=SW, none2=NONE2)[kind]
    return base + rng.randrange(NP) * PG + rng.randrange(0, PG, 8), rng.choice([8, 64, 512, 4096])


@panda.hypercall(MAGIC, mutates=True)
def hc(cpu):
    op, a, b = (panda.arch.get_arg(cpu, i, convention="syscall") for i in range(2, 5))
    if op == 1:                                          # READY: a user-mode callback
        layout(a, b)
        st["asid"] = panda.current_asid(cpu)
        st["ptes"] = {}
        for name, a in ({} if arch not in ("x86_64", "aarch64") else dict(file_dropped=FILEA, anon_untouched=ANON, hole=HOLE, prot_none_untouched=NONE,
                            truncated=TRUNC, prot_none_after_write=NONE2, swapped=SW, tag_present=TAG,
                            stack_guard=0)).items():
            if a == 0:
                continue
            try:
                e = walk(st["asid"], a)
                dbg = "debug read ok" if panda._memory_read(cpu, a, 8) is not None else ""
            except ValueError:
                dbg = "debug read fails"
            except Exception as ex:
                e, dbg = ("?", 0, 0), repr(ex)
            st["ptes"][name] = f"{classify(e)}; {dbg}; pte={e[2]:#x}"
        st["cb_sync"] = bytes(panda.virtual_memory_read(cpu, TAG, 16, fault=True))
        try:
            panda.virtual_memory_read(cpu, FILEA, 8, fault=True)
            st["cb_block"] = "returned"
        except PageInWouldBlock:
            st["cb_block"] = "PageInWouldBlock"
        except Exception as e:
            st["cb_block"] = repr(e)
        submit("user-callback", FILEA + 5 * PG, 64)      # completion form from a vCPU callback
        st["ready"].set()
    elif op == 2:                                        # DONE
        st["done_args"] = (a, b)
        st["done"].set()
    panda.guest.set_retval(cpu, 0)


@panda.cb_asid_changed(name="kctx", enabled=False)
def kctx(cpu, old, new):
    # kernel mode, in the middle of a context switch
    if st["k_budget"] > 0 and st["asid"] is not None:
        st["k_budget"] -= 1
        rng = random.Random(st["k_budget"])
        a, n = rand_target(rng)
        submit("kernel-callback", a, n)
    return False


@panda.cb_start_block_exec(name="kblock", enabled=False)
def kblock(cpu, tb):
    # kernel mode, anywhere: interrupts, softirqs, spinlocks held, the scheduler
    if st["kb_budget"] > 0 and panda.arch.get_pc(cpu) >> 63:     # a kernel address
        st["kb_budget"] -= 1
        rng = random.Random(st["kb_budget"] + 99)
        a, n = rand_target(rng)
        submit("kernel-block", a, n)
        if st["kb_budget"] == 0:
            panda.disable_callback("kblock")    # a Python call per block stalls the guest


def other_thread(n, seed):
    rng = random.Random(seed)
    for _ in range(n):
        a, n_ = rand_target(rng)
        submit("other-thread", a, n_)
        time.sleep(0.002)


def refused_only():
    """Targets without a system-call injector: every request is refused, with its reason, and the
    guest is unaffected."""
    m.revert(panda, agent)
    asids = []

    @panda.cb_asid_changed(name="any")
    def any_asid(cpu, old, new):
        if not asids:
            asids.append(new)
        return False
    agent.run("ls / >/dev/null")
    panda.disable_callback("any")
    try:
        panda.pagein.read(asids[0] if asids else 0x1000, 0x400000, 8, lambda d, e: None)
        C.check("no injector: a request is refused", False, "accepted")
    except PageInRefused as e:
        C.check("no injector: a request is refused, with its reason", e.status == -6, str(e))
        progress(f"pagein: {e}")
    try:
        panda.virtual_memory_read(None, 0x400000, 8, fault=True, asid=0x1000)
        C.check("no injector: fault=True is refused", False, "returned")
    except PageInRefused as e:
        C.check("no injector: fault=True is refused", e.status == -6, str(e))
    st_, out = agent.run("echo alive")
    C.check("the guest runs on", out.strip() == b"alive", repr(out))
    panda.end_analysis()


@panda.queue_blocking
def drive():
    if prog not in PROGRAMS:
        return refused_only()
    global E
    m.revert(panda, agent)
    E = agent.order or "<"
    agent.put("/tmp/pt", gzip.decompress(base64.b64decode(PROGRAMS[prog])))
    # a filesystem and swap on loop devices (the machine has no disk): file pages through the
    # block layer, swapped pages out and back
    st_, out = agent.run("chmod +x /tmp/pt; mkdir -p /mnt; dd if=/dev/zero of=/tmp/disk.img bs=1M count=24 2>/dev/null; "
                         "losetup /dev/loop0 /tmp/disk.img && mke2fs -q -F /dev/loop0 >/dev/null 2>&1 && "
                         "mount -t ext2 /dev/loop0 /mnt; dd if=/dev/zero of=/tmp/swap.img bs=1M count=16 2>/dev/null; "
                         "losetup /dev/loop1 /tmp/swap.img && mkswap /dev/loop1 >/dev/null && swapon /dev/loop1; "
                         "grep -c loop1 /proc/swaps >&2; mount | grep -c ' /mnt '; true")
    d = "/mnt" if out.strip().endswith(b"1") else "/tmp"
    progress(f"pagein: setup: {out.decode(errors='replace').strip()!r}")
    secs = opt.secs or (8 + opt.stress // 100)
    agent.run(f"(/tmp/pt {d} {secs} spin > /tmp/pt.out 2>&1; echo exit=$? >> /tmp/pt.out) &")
    if not st["ready"].wait(120):
        C.check("program ready", False, "no READY hypercall")
        panda.end_analysis()
        return
    progress(f"pagein: target asid {st['asid']:#x}, files on {d}")
    for k, v in st["ptes"].items():
        progress(f"pagein: pte {k}: {v}")
    asid = st["asid"]
    pi = panda.pagein
    C.check("callback: fault=True returns present bytes", st["cb_sync"][:8] == Q(0x5441475441475441))
    C.check("callback: fault=True on a missing page raises PageInWouldBlock",
            st["cb_block"] == "PageInWouldBlock", st["cb_block"])

    # reads and writes from this (queue_blocking) thread
    t = time.monotonic()
    data = panda.virtual_memory_read(None, FILEA + 3 * PG + 40, 200, fault=True, asid=asid)
    C.check("thread: file page read through the guest's page cache",
            bytes(data) == expect_file(FILEA + 3 * PG + 40, 200), bytes(data[:16]).hex())
    C.check("thread: never-touched anonymous page reads zero",
            bytes(panda.virtual_memory_read(None, ANON + 7 * PG, 4096, fault=True, asid=asid)) == bytes(4096))
    span = panda.virtual_memory_read(None, FILEA + 9 * PG - 32, 64, fault=True, asid=asid)
    C.check("thread: a read across two missing pages", bytes(span) == expect_file(FILEA + 9 * PG - 32, 64))
    s = panda.read_str(None, FILEA + 20 * PG, max_length=24, fault=True, asid=asid)
    C.check("thread: read_str(fault=True)", len(s) > 0)
    progress(f"pagein: basic reads {time.monotonic() - t:.2f}s")
    wdata = b"".join(Q(pat(WR + i)) for i in range(0, NP * PG, 8))
    panda.virtual_memory_write(None, WR, wdata, fault=True, asid=asid)
    panda.virtual_memory_write(None, TAG + 16, Q(1), fault=True, asid=asid)
    C.check("thread: write paged in for writing, reads back",
            bytes(panda.virtual_memory_read(None, WR + 5 * PG, 64, fault=True, asid=asid)) == wdata[5 * PG:5 * PG + 64])

    sw = panda.virtual_memory_read(None, SW + 11 * PG, 128, fault=True, asid=asid)
    C.check("thread: swapped-out page swaps in", check_data(SW + 11 * PG, bytes(sw)))
    guard = panda.virtual_memory_read(None, TAG + 24, 8, fault=True, asid=asid)
    if guard != bytes(8):
        bad_guard = {"guard region (MADV_GUARD_INSTALL)": GUARDR + 3 * PG}
    else:
        bad_guard = {}
        progress("pagein: the guest kernel has no MADV_GUARD_INSTALL")
    bad = {**bad_guard, "below a MAP_GROWSDOWN region": GROW - PG, "PROT_NONE after a write": NONE2, "no mapping": HOLE + 8, "truncated file": TRUNC + PG, "PROT_NONE": NONE,
           "kernel address": KADDR[arch]}
    for name, addr in bad.items():
        try:
            panda.virtual_memory_read(None, addr, 8, fault=True, asid=asid)
            C.check(f"bad address ({name}): refused", False, "read returned")
        except PageInFailed as e:
            C.check(f"bad address ({name}): refused with the guest's errno",
                    e.guest_errno != 0, f"{e}")
            progress(f"pagein: {name}: {e}")
        except Exception as e:
            C.check(f"bad address ({name}): refused", False, repr(e))
    try:
        panda.virtual_memory_write(None, RO, b"x" * 8, fault=True, asid=asid)
        C.check("write to a read-only mapping: refused", False, "write returned")
    except PageInFailed as e:
        C.check("write to a read-only mapping: refused", True)
        progress(f"pagein: read-only write: {e}")
    try:
        panda.pagein.wait("bogus", lambda done: pi.read(0x1230000, 0x400000, 8, done, timeout_ns=200_000_000), 60)
        C.check("an address space that never runs: PageInTimeout", False, "completed")
    except PageInTimeout:
        C.check("an address space that never runs: PageInTimeout", True)

    # the stress: requests from every context while the program spins
    t = time.monotonic()
    third = max(1, opt.stress // 4)
    st["k_budget"] = third
    st["kb_budget"] = third
    panda.enable_callback("kctx")
    panda.enable_callback("kblock")
    th = threading.Thread(target=other_thread, args=(third, 1))
    th.start()
    rng = random.Random(7)
    for _ in range(third):
        a, n = rand_target(rng)
        submit("blocking-thread", a, n)
    th.join()
    if not st["alldone"].wait(300):
        C.check("stress: every request completed", False, f"{st['outstanding']} outstanding")
    panda.disable_callback("kctx")
    panda.disable_callback("kblock")
    progress(f"pagein: stress {time.monotonic() - t:.1f}s {json.dumps(st['counts'])}")
    wrong = sum(v.get("WRONG", 0) for v in st["counts"].values())
    C.check("stress: every request completed", st["outstanding"] == 0, str(st["outstanding"]))
    C.check("stress: no wrong data", wrong == 0, str(wrong))
    C.check("stress: requests from kernel mode were made",
            "kernel-callback" in st["counts"] or "kernel-block" in st["counts"], str(list(st["counts"])))

    if not st["done"].wait(secs * 30 + 120):
        C.check("program finished", False, "no DONE hypercall")
    else:
        mism, badb = st["done_args"]
        C.check("program: no register changed under it", mism == 0, str(mism))
        C.check("program: its memory is intact (and holds the test's write)", badb == 0, str(badb))
    time.sleep(0.5)
    try:
        panda.pagein.wait("exited", lambda done: pi.read(asid, FILEA, 8, done, timeout_ns=300_000_000), 60)
        C.check("a process that exited: PageInTimeout", False, "completed")
    except PageInTimeout:
        C.check("a process that exited: PageInTimeout", True)
    _, out = agent.run("cat /tmp/pt.out; dmesg | tail -n 40")
    txt = out.decode(errors="replace")
    for l in txt.splitlines():
        if l.startswith("PT ") or l.startswith("exit="):
            progress(f"pagein: program: {l}")
    C.check("program exited 0 (no SIGSEGV/SIGBUS)", "exit=0" in txt, txt[-400:])
    C.check("the MAP_GROWSDOWN region did not grow", "grew 0" in txt, txt[-400:])
    badlines = [l for l in (txt + m.console_tail(n=20000)).splitlines() if BAD.search(l)]
    C.check("guest: no oops, BUG or signal", not badlines, "; ".join(badlines[:5]))
    st["stats"] = panda.pagein.stats()
    progress(f"pagein: engine stats {st['stats']}")
    panda.end_analysis()


panda.run()
rc = C.report(f"pagein {arch} smp{opt.smp}")
if opt.json:
    with open(opt.json, "w") as f:
        json.dump(dict(arch=arch, smp=opt.smp, counts=st["counts"], ptes=st.get("ptes"), stats=st.get("stats"),
                       results={k: v[0] for k, v in C.results.items()}), f, indent=1)
sys.exit(rc)
