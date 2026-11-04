"""Every system call number through every entry instruction: each call reported exactly once at its
entry and once at its return, with its ABI, number, all six arguments, pc and return value; then
the return paths that are not a plain return.

Run: PANDA_PLUGIN_DIR=<test-plugins>/lib/panda/plugins panda-python tests/syscall_cover.py \\
         <images>/busybox-<arch>.json [--intel]
(busybox-x86_64, -aarch64 and -armel; images from tests/mkbusybox.py, the sctrace plugin from
`nix build .#test-plugins`). It runs on a tests/testmachine.py machine (built and snapshotted the
first time, 4-6 s); programs go in and their output comes out through the agent. --intel makes
the x86 CPU GenuineIntel, whose 32-bit vDSO enters through sysenter (the default, qemu64, is AMD:
syscall). About 2 s per arch once the machine exists.

The events come from the trap through the QEMU plugin syscall API (tests/sctrace.py).

Each program in tests/programs/sccover_<prog>.S installs a seccomp filter that makes the kernel
refuse every call with -1234, then makes calls with arguments the test can predict through each
entry instruction its task has, and writes back what each call returned (see the headers):
- x86_64 (a 64-bit task): numbers 0..599 (but uretprobe) and the x32 range through `syscall`,
  out-of-range specials, and 0..599 through `int 0x80`;
- i386 (a 32-bit task on the x86_64 guest): 0..599 through `int 0x80` and through the vDSO's
  __kernel_vsyscall (sysenter or syscall), and specials;
- aarch64: 0..599 through `svc #0` and `svc #0x5a`, and specials.
(arm has no sweep program yet: there, only the return paths.)
Checks, per program:
- the program ran to the end and every sweep call returned -1234 to it;
- the events are exactly the program's calls in order: prctl, seccomp, the sweeps, write,
  exit_group; each enter with its ABI, number (i386: eax), all six arguments (i386: 32 bits; the
  vDSO's argument 2 from ebp or ecx and argument 6 from the stack), and pc (the address after the
  instruction, one per site; the vDSO's: within 64 bytes past AT_SYSINFO);
- per entry instruction: each call has exactly one return, with its enter's pc and the value the
  program saw; exit_group has none;
- nothing was lost (the event buffer was large enough).
Then tests/programs/scpaths.c makes calls whose return is not the plain one (see its header; on
aarch64 also its arm build, a 32-bit task on the compat ABI):
iret with TF set (x86_64), a restarted read, -EINTR, restart_syscall after a stop, fork, vfork,
a thread, clone3, clone3 with CLONE_VM | CLONE_VFORK (the child returns from the parent's call at
its pc on its stack), a thread and the main thread blocked in read at the same instruction, and
execve failing and succeeding. The checks are that each returns as the kernel defines it: once,
restarted, or never (exit, exit_group, execve, rt_sigreturn), that a child's return is never
reported as the parent's, and that same-site calls of two threads each get their own return.
Known trap bugs are GAPs (testlib.Checks), listed below with their cause.
"""
import argparse
import base64
import importlib
import struct
import sys

from testlib import Checks, PROGRAM_BASE, PROGRAM_END, progress
from testmachine import Agent, Machine
import sctrace
from sctrace import ABI_X86_64, ABI_I386, ABI_AARCH64, ENTER, RETURN

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--intel", action="store_true")
opt = ap.parse_args()

REFUSED = -1234
UPROBE = 335        # x86_64 uretprobe: seccomp lets it through and it raises SIGILL; skipped
M64, M32 = (1 << 64) - 1, (1 << 32) - 1

# The programs' arguments: argument i of call seq is BASE[i] + seq.
BASE64 = [0xa0a1000011100000, 0xa2a3000022200000, 0xa4a5000033300000,
          0xa6a7000044400000, 0xa8a9000055500000, 0xaaab000066600000]
BASE32 = [0x11100000, 0x22200000, 0x33300000, 0x44400000, 0x55500000, 0x66600000]
SPECIALS = {"x86_64": [-1, 0x100000000, 0x7fffffff, 0x80000000, 0xffffffff, 0x8000000000000000,
                       0x3fffffff],
            "i386": [-1, 0x7fffffff, 0x80000000, 0x40000000],
            "aarch64": [-1, 0x100000000, 0x7fffffff, 0x80000000, 0xffffffff, 0x8000000000000000]}
MARK64, MARK32 = 0x5afe5afe << 32, 0x5afe5afe

# guest arch: the cover programs (sccover_<prog>) and the return-path program's builds (scpaths;
# arm on aarch64: a 32-bit task, the compat ABI)
PROGRAMS = {"x86_64": (["x86_64", "i386"], ["x86_64"]), "aarch64": (["aarch64"], ["aarch64", "arm"]),
            "arm": ([], ["arm"])}

# Known trap bugs: GAP while they fail, XPASS (a failure) once fixed, so this list stays true.
_X86_VFORK = ("the x86 trap keys a call by (CR3, user sp, pc): a vfork child returns from the "
              "parent's call at its pc, on its stack and in its address space, so the child's "
              "return (0) is taken as the parent's and the parent's is never reported")
GAPS = {
    "x86_64: int 0x80: each has one return with its pc and the value the program saw":
        "int 0x80 from a 64-bit task: the x86 trap keys the entry by the 32-bit user sp and the "
        "return by the 64-bit one, so no return matches",
    "x86_64 return paths: 6 vfork: one return, in the parent, with the child's id (the child's "
    "return is not reported)": _X86_VFORK,
    "x86_64 return paths: 9 clone3 vfork: one return, in the parent, with the child's id (the "
    "child's return is not reported)": _X86_VFORK,
}

machine = Machine(opt.image, intel=opt.intel)
arch = machine.arch
C = Checks(arch, GAPS)
if arch not in PROGRAMS:
    print(f"syscall_cover: no programs for {arch} yet (tests/README.md, \"Test machines\")")
    sys.exit(C.report("SYSCALL_COVER skipped"))
panda = machine.panda(extra_args=sctrace.extra_args(arch))
agent = Agent(panda)
trace = sctrace.Sctrace(arch)


def elf(name, prog):
    return base64.b64decode(importlib.import_module(f"{name}_programs").PROGRAMS[prog])


def sites(prog):
    """[(pc after it, kind)] for each trap instruction in the program's text, in order."""
    data = elf("sccover", prog)
    if data[4] == 2:
        shoff, = struct.unpack_from("<Q", data, 0x28)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3a)
        fmt = "<IIQQQQ"
    else:
        shoff, = struct.unpack_from("<I", data, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x2e)
        fmt = "<IIIIII"
    sec = [struct.unpack_from(fmt, data, shoff + i * shentsize) for i in range(shnum)]
    names = sec[shstrndx][4]
    for s in sec:
        name, addr, off, size = s[0], s[3], s[4], s[5]
        if data[names + name:data.index(b"\0", names + name)] != b".text":
            continue
        text = data[off:off + size]
        if prog == "aarch64":
            words = struct.unpack(f"<{len(text) // 4}I", text[:len(text) // 4 * 4])
            return [(addr + 4 * i + 4, f"svc{(w >> 5) & 0xffff:#x}") for i, w in enumerate(words)
                    if w & 0xffe0001f == 0xd4000001]
        found = [(i, kind) for kind, pat in (("syscall", b"\x0f\x05"), ("int80", b"\xcd\x80"))
                 for i in range(len(text) - 1) if text[i:i + 2] == pat]
        return [(addr + i + 2, kind) for i, kind in sorted(found)]
    raise ValueError("no .text")


def expected(prog):
    """([(abi, num, args, group, site)], number of sweep calls, [site kinds], the sweep groups
    whose calls must all be refused): every call the program makes, in order. site: an index
    into sites(prog), or "vdso". args None: any."""
    calls, seq = [], [0]

    def sweep(abi, num, group, site, base, mask):
        calls.append((abi, num & mask, [(b + seq[0]) & mask for b in base], group, site))
        seq[0] += 1

    if prog == "x86_64":
        calls += [(ABI_X86_64, 157, [38, 1, 0, 0, 0, None], "prctl, seccomp", 0),
                  (ABI_X86_64, 317, [1, 0, None, None, None, None], "prctl, seccomp", 1)]
        for nr in range(600):
            if nr != UPROBE:
                sweep(ABI_X86_64, nr, "syscall", 2, BASE64, M64)
        for nr in range(600):
            sweep(ABI_X86_64, 0x40000000 + nr, "syscall x32", 3, BASE64, M64)
        for nr in SPECIALS[prog]:
            sweep(ABI_X86_64, nr, "syscall specials", 4, BASE64, M64)
        for nr in range(600):
            sweep(ABI_I386, nr, "int 0x80", 5, BASE64, M32)
        calls += [(ABI_X86_64, 1, [MARK64 | 1, None, 8 * seq[0], None, None, None], "write", 6),
                  (ABI_X86_64, 231, [MARK64, None, None, None, None, None], "exit_group", 7)]
        kinds = ["syscall"] * 5 + ["int80"] + ["syscall"] * 2
        refused = ["syscall", "syscall x32", "int 0x80"]
    elif prog == "i386":
        calls += [(ABI_I386, 172, [38, 1, 0, 0, 0, None], "prctl, seccomp", 0),
                  (ABI_I386, 354, [1, 0, None, None, None, None], "prctl, seccomp", 1)]
        for nr in range(600):
            sweep(ABI_I386, nr, "int 0x80", 2, BASE32, M32)
        for nr in range(600):
            sweep(ABI_I386, nr, "vDSO", "vdso", BASE32, M32)
        for nr in SPECIALS[prog]:
            sweep(ABI_I386, nr, "int 0x80 specials", 3, BASE32, M32)
        calls += [(ABI_I386, 4, [1, None, 4 + 4 * seq[0], None, None, MARK32], "write", 4),
                  (ABI_I386, 252, [0, None, None, None, None, MARK32], "exit_group", 5)]
        kinds = ["int80"] * 6
        refused = ["int 0x80", "vDSO"]
    else:
        calls += [(ABI_AARCH64, 167, [38, 1, 0, 0, 0, None], "prctl, seccomp", 0),
                  (ABI_AARCH64, 277, [1, 0, None, None, None, None], "prctl, seccomp", 1)]
        for nr in range(600):
            sweep(ABI_AARCH64, nr, "svc #0", 2, BASE64, M64)
        for nr in range(600):
            sweep(ABI_AARCH64, nr, "svc #0x5a", 3, BASE64, M64)
        for nr in SPECIALS[prog]:
            sweep(ABI_AARCH64, nr, "svc specials", 4, BASE64, M64)
        calls += [(ABI_AARCH64, 64, [MARK64 | 1, None, 8 * seq[0], None, None, None], "write", 5),
                  (ABI_AARCH64, 94, [MARK64, None, None, None, None, None], "exit_group", 6)]
        kinds = ["svc0x0"] * 3 + ["svc0x5a"] + ["svc0x0"] * 3
        refused = ["svc #0", "svc #0x5a"]
    return calls, seq[0], kinds, refused


def run(name, prog, follow=False):
    """Put and run <name>_<prog>; (rc, events, lost, its stdout). follow: record every call (the
    program's are picked out by window()), for the i386 vDSO's, which are outside the program."""
    path = f"/tmp/{name}_{prog}"
    agent.put(path, elf(name, prog))
    if follow:
        trace.start(cap=1 << 16)
    else:
        trace.start(PROGRAM_BASE, PROGRAM_END, cap=1 << 16)
    progress(f"running {path}")
    rc, out = agent.run(f"{path} > {path}.out")
    events, lost = trace.stop()
    return rc, events, lost, agent.get(f"{path}.out")


def window(events, exit_nr, mark):
    """A followed run's events of the program: the enters from its first call in the program to
    its exit_group (exit_nr with argument 6 mark), and their returns (one vCPU, and the agent and
    shell wait meanwhile)."""
    first = next((i for i, e in enumerate(events)
                  if e.kind == ENTER and PROGRAM_BASE <= e.pc < PROGRAM_END), None)
    if first is None:
        return []
    last = next((i for i in range(first, len(events)) if events[i].kind == ENTER and
                 events[i].num == exit_nr and events[i].args[5] == mark),
                len(events) - 1)
    ids = {e.id for e in events[first:last + 1] if e.kind == ENTER}
    return [e for i, e in enumerate(events) if (first <= i <= last and e.kind == ENTER) or
            (e.kind == RETURN and e.id in ids)]


def check(prog, rc, events, lost, raw):
    calls, n_sweep, kinds, refused = expected(prog)
    p = f"{prog}: "
    if prog == "i386":
        vsys = struct.unpack_from("<I", raw)[0] if len(raw) >= 4 else None
        rets = list(struct.unpack(f"<{len(raw) // 4 - 1}i", raw[4:len(raw) // 4 * 4])) if raw else []
        written = 4 + 4 * n_sweep
    else:
        vsys, rets = None, list(struct.unpack(f"<{len(raw) // 8}q", raw[:len(raw) // 8 * 8]))
        written = 8 * n_sweep
    C.check(p + "the program ran to the end (rc=0)", rc == 0, rc)
    sweep_calls = calls[2:-2]
    want = [c[3] in refused for c in sweep_calls]
    C.check(p + f"the program wrote back {n_sweep} return values, {REFUSED} for every swept number",
            len(rets) == n_sweep and all(r == REFUSED for r, w in zip(rets, want) if w),
            (len(rets), sorted({r for r, w in zip(rets, want) if w and r != REFUSED})[:8]))
    C.check(p + "nothing was lost", lost == 0, lost)
    pcs = sites(prog)
    C.check(p + f"the program has its {len(kinds)} call sites", [k for _, k in pcs] == kinds, pcs)

    enters = [e for e in events if e.kind == ENTER]
    by_id = {}
    for e in events:
        if e.kind == RETURN:
            by_id.setdefault(e.id, []).append(e)
    groups = list(dict.fromkeys(c[3] for c in calls))
    bad_enter, bad_return = [], {g: [] for g in groups}
    vdso_pcs = set()
    for i, (abi, num, args, group, site) in enumerate(calls):
        if i >= len(enters):
            bad_enter.append((i, "missing"))
            continue
        e = enters[i]
        if site == "vdso":
            vdso_pcs.add(e.pc)
            pc_ok = vsys is not None and vsys < e.pc <= vsys + 64
        else:
            pc_ok = len(pcs) == len(kinds) and e.pc == pcs[site][0]
        ok = (e.abi == abi and e.num & M64 == num & M64 and pc_ok and
              all(w is None or w == g for w, g in zip(args, e.args)))
        if not ok and len(bad_enter) < 10:
            bad_enter.append((i, group, dict(abi=(e.abi, abi), num=(hex(e.num & M64), hex(num & M64)),
                                             pc=hex(e.pc), args=[(hex(g), w and hex(w))
                                                                 for w, g in zip(args, e.args)
                                                                 if w is not None and w != g])))
        rest = by_id.get(e.id, [])
        bad = bad_return[group]
        if i == len(calls) - 1:                 # exit_group: never returns
            if rest:
                bad.append((i, "exit_group returned", rest))
            continue
        seq = i - 2
        if i < 2:
            want_ret = 0
        elif i == len(calls) - 2:
            want_ret = written
        else:
            want_ret = rets[seq] if 0 <= seq < len(rets) else REFUSED
        if len(rest) != 1 or rest[0].pc != e.pc or rest[0].ret != want_ret or rest[0].restart:
            bad.append((i, [(hex(r.pc), r.ret, r.restart) for r in rest], want_ret))

    C.check(p + f"exactly {len(calls)} enters, one per call", len(enters) == len(calls),
            len(enters))
    C.check(p + "each enter has the call's ABI, number, arguments and pc", not bad_enter,
            bad_enter[:10])
    if vdso_pcs:
        C.check(p + "the vDSO's calls are all at one pc", len(vdso_pcs) == 1,
                [hex(x) for x in vdso_pcs])
    for g in groups:
        what = "never returns" if g == "exit_group" else \
            "each has one return with its pc and the value the program saw"
        C.check(p + f"{g}: {what}", not bad_return[g],
                f"{len(bad_return[g])} calls: {bad_return[g][:5]}")
    C.check(p + "every event is from vCPU 0 (one vCPU)", {e.cpu for e in events} <= {0},
            {e.cpu for e in events})
    return len(calls), len(events)


NR = {"x86_64": {"read": 0, "write": 1, "rt_sigreturn": 15, "getpid": 39, "nanosleep": 35,
                 "clone": 56, "vfork": 58, "execve": 59, "exit": 60, "wait4": 61,
                 "restart_syscall": 219, "exit_group": 231, "clone3": 435},
      "aarch64": {"read": 63, "write": 64, "rt_sigreturn": 139, "getpid": 172, "nanosleep": 101,
                  "clone": 220, "vfork": 220, "execve": 221, "exit": 93, "wait4": 260,
                  "restart_syscall": 128, "exit_group": 94, "clone3": 435},
      "arm": {"read": 3, "write": 4, "rt_sigreturn": 173, "getpid": 20, "nanosleep": 162,
              "clone": 120, "vfork": 190, "execve": 11, "exit": 1, "wait4": 114,
              "restart_syscall": 0, "exit_group": 248, "clone3": 435}}
EINTR, ENOENT, EBADF = 4, 2, 9


def check_paths(prog, rc, events, lost, raw):
    """scpaths: the return paths (see its header). Events by case, split at the markers."""
    p = f"{prog} return paths: "
    nr = NR[prog]
    neg1 = M32 if prog == "arm" else M64      # the markers' fd, -1
    C.check(p + "the program ran to the end (execve(/bin/true): rc=0)", rc == 0, rc)
    C.check(p + "nothing was lost", lost == 0, lost)
    cases, cur = {}, None
    for e in events:
        if e.kind == ENTER and e.num == nr["write"] and e.args[0] == neg1:
            cur = e.args[2]
        cases.setdefault(cur, []).append(e)
    calls = {}                  # id: (enter, [returns])
    for e in events:
        if e.kind == ENTER:
            calls[e.id] = (e, [])
        elif e.id in calls:
            calls[e.id][1].append(e)

    def of(case, name):
        """[(enter, [returns])] of the calls named name in case, in order."""
        return [calls[e.id] for e in cases.get(case, []) if e.kind == ENTER and e.num == nr[name]]

    def rets(c):
        return [(r.ret, r.restart) for r in c[1]]

    def one(case, name, i=0):
        found = of(case, name)
        return found[i] if len(found) > i else (None, [])

    def check(case, what, ok, detail):
        C.check(p + f"{case} {what}", ok, detail)

    if prog == "x86_64":
        getpid = one(1, "getpid")
        check(1, "iret: getpid (TF set) returns once, with the pid", len(rets(getpid)) == 1 and
              rets(getpid)[0][0] > 0 and rets(getpid)[0][1] == 0, rets(getpid))
    sigret = [c for case in cases for c in of(case, "rt_sigreturn")]
    C.check(p + "rt_sigreturn never returns", sigret and all(not rets(c) for c in sigret),
            [rets(c) for c in sigret])
    reads = of(2, "read")
    check(2, "restart: the read returns restarted, enters again at its pc, and returns 1",
          len(reads) == 2 and rets(reads[0]) and rets(reads[0])[-1][1] == 1 and
          reads[1][0].pc == reads[0][0].pc and reads[1][0].args[:3] == reads[0][0].args[:3] and
          rets(reads[1]) == [(1, 0)], [(r[0].args[:3], rets(r)) for r in reads])
    reads = of(3, "read")
    check(3, "eintr: the read returns -EINTR, not restarted", len(reads) == 1 and
          rets(reads[0]) == [(-EINTR, 0)], [rets(r) for r in reads])
    sleeps = [c for c in of(4, "nanosleep") if rets(c) and rets(c)[-1][1] == 1]
    rs = of(4, "restart_syscall")
    if arch == "arm":
        # A 32-bit arm kernel restarts a call that no handler interrupted without going back to
        # user mode (entry-common.S, local_restart): the trap sees one call and one return.
        sleeps = of(4, "nanosleep")
        check(4, "restart_syscall: (the kernel restarts it in kernel mode) each nanosleep returns 0 "
              "once, not restarted, and there is no restart_syscall", len(sleeps) == 2 and not rs and
              all(rets(c) == [(0, 0)] for c in sleeps), ([rets(c) for c in sleeps], len(rs)))
    else:
        check(4, "restart_syscall: the stopped nanosleep returns restarted, restart_syscall enters at "
              "its pc and returns 0", len(sleeps) == 1 and len(rs) == 1 and
              rs[0][0].pc == sleeps[0][0].pc and rets(rs[0]) == [(0, 0)],
              (len(sleeps), [(hex(r[0].pc), rets(r)) for r in rs]))
    for case, name in ((5, "clone"), (6, "vfork"), (7, "clone"), (8, "clone3"), (9, "clone3")):
        c = one(case, name)
        waited = [rets(w)[0][0] for w in of(case, "wait4") if rets(w)]
        child = waited[0] if waited else None
        what = {5: "fork", 6: "vfork", 7: "thread", 8: "clone3", 9: "clone3 vfork"}[case]
        if case == 7:
            child = rets(c)[0][0] if rets(c) else None          # the thread's tid
        check(case, f"{what}: one return, in the parent, with the child's id (the child's return "
              "is not reported)", rets(c) == [(child, 0)] and child and child > 0,
              dict(returns=rets(c), child=child))
    for case, name in ((5, "exit_group"), (6, "exit_group"), (7, "exit"), (8, "exit_group"),
                       (9, "exit_group")):
        ex = of(case, name)
        check(case, f"the child's {name} never returns", len(ex) == 1 and not rets(ex[0]),
              [rets(x) for x in ex])
    reads = sorted(of(10, "read"), key=lambda c: c[0].args[0])  # pipe A's fd (the thread's) first
    check(10, "same-site read: two reads at one pc, the thread's returns 1 and the main thread's 2, "
          "each once", len(reads) == 2 and reads[0][0].pc == reads[1][0].pc and
          rets(reads[0]) == [(1, 0)] and rets(reads[1]) == [(2, 0)],
          [(r[0].args[0], hex(r[0].pc), rets(r)) for r in reads])
    check(11, "execve of a missing file returns -ENOENT", rets(one(11, "execve")) == [(-ENOENT, 0)],
          rets(one(11, "execve")))
    ex = one(12, "execve")
    check(12, "execve(/bin/true) enters and never returns", ex[0] is not None and not rets(ex),
          rets(ex))
    marks = [e.args[2] for e in events if e.kind == ENTER and e.num == nr["write"] and
             e.args[0] == neg1]
    C.check(p + "the markers are 1..12 in order, each returning -EBADF once",
            marks == list(range(1, 13)) and all(rets(calls[e.id]) == [(-EBADF, 0)] for e in events
                                                if e.kind == ENTER and e.num == nr["write"] and
                                                e.args[0] == neg1), marks)
    return len(events)


R = {}
progs, paths = PROGRAMS[arch]


@panda.queue_blocking
def drive():
    machine.revert(panda, agent)
    R["cpu"] = agent.run("grep -m1 vendor_id /proc/cpuinfo; uname -m")[1].decode()
    for prog in progs:
        R[prog] = run("sccover", prog, follow=prog == "i386")
    for prog in paths:
        R["paths", prog] = run("scpaths", prog)
    panda.end_analysis()


panda.run()

summary = []
for prog in progs:
    if prog not in R:
        C.check(f"{prog}: ran", False, "the guest stopped before it")
        continue
    rc, events, lost, raw = R[prog]
    if prog == "i386":
        events = window(events, 252, MARK32)
    n_calls, n_events = check(prog, rc, events, lost, raw)
    summary.append(f"{prog} calls={n_calls} events={n_events}")
for prog in paths:
    if ("paths", prog) in R:
        summary.append(f"{prog}_paths events={check_paths(prog, *R['paths', prog])}")
    else:
        C.check(f"{prog} return paths: ran", False, "the guest stopped before it")
print("guest:", " ".join(R.get("cpu", "?").split()))
sys.exit(C.report(f"SYSCALL_COVER arch={arch} " + " ".join(summary)))
