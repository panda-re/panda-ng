"""Every system call number through every entry instruction: each call reported exactly once at its
entry and once at its return, with its ABI, number, all six arguments, pc and return value.

Run: PANDA_PLUGIN_DIR=<test-plugins>/lib/panda/plugins panda-python tests/syscall_cover.py \\
         <dir>/debian-13-x86_64.json
(x86_64 only for now; the sctrace plugin is in `nix build .#test-plugins`).

Each program in tests/programs/sccover_<prog>.S installs a seccomp filter that makes the kernel
refuse every call with -1234, then makes calls with arguments the test can predict through each
entry instruction its task has, and writes back what each call returned (see the headers):
- x86_64 (a 64-bit task): numbers 0..599 (but uretprobe) and the x32 range through `syscall`,
  out-of-range specials, and 0..599 through `int 0x80`;
- i386 (a 32-bit task): 0..599 through `int 0x80` and through the vDSO's __kernel_vsyscall
  (sysenter on Intel CPUs, syscall on AMD; the default qemu64 CPU is AMD), and specials.
The sctrace plugin records the calls from the engine's syscall trap: those whose pc is in the
program, and for i386 every call in its address space (the vDSO's are outside the program).
Checks, per program:
- the program ran to the end and every call returned -1234 to it;
- the events are exactly the program's calls in order: prctl, seccomp, the sweeps, write,
  exit_group; each enter with its ABI, number (i386: eax), all six arguments (i386: 32 bits; the
  vDSO's argument 2 from ebp or ecx and argument 6 from the stack), and pc (the address after the
  instruction, one per site; the vDSO's: past its int 0x80, within 64 bytes of AT_SYSINFO);
- per entry instruction: each call has exactly one return (no drop), with its enter's id and pc,
  and the value the program saw; exit_group has none;
- nothing was lost (the event buffer was large enough).
Then tests/programs/scpaths_x86_64.c makes calls whose return is not the plain sysret (iret with
TF set, a restarted read, -EINTR, restart_syscall after a stop, fork, vfork, a thread, clone3,
execve failing and succeeding; see its header), and the checks are that each returns as the
kernel defines it: once, restarted, or never (exit, exit_group, execve, rt_sigreturn), and that
a child's return is never reported as the parent's.
Known trap bugs are GAPs (testlib.Checks), listed below with their cause.
"""
import base64
import importlib
import struct
import sys
from pathlib import Path

from testlib import Checks, PROGRAM_BASE, PROGRAM_END, progress, upload
from cffi import FFI
from pandare2 import Panda

ABI_X86_64, ABI_I386 = 0, 1
REFUSED = -1234
UPROBE = 335        # uretprobe: seccomp lets it through and it raises SIGILL; the program skips it
M64, M32 = (1 << 64) - 1, (1 << 32) - 1

# The programs' arguments: argument i of call seq is BASE[i] + seq.
BASE64 = [0xa0a1000011100000, 0xa2a3000022200000, 0xa4a5000033300000,
          0xa6a7000044400000, 0xa8a9000055500000, 0xaaab000066600000]
BASE32 = [0x11100000, 0x22200000, 0x33300000, 0x44400000, 0x55500000, 0x66600000]
SPECIALS64 = [-1, 0x100000000, 0x7fffffff, 0x80000000, 0xffffffff, 0x8000000000000000, 0x3fffffff]
SPECIALS32 = [-1, 0x7fffffff, 0x80000000, 0x40000000]
MARK64, MARK32 = 0x5afe5afe << 32, 0x5afe5afe

# Known trap bugs: GAP while they fail, XPASS (a failure) once fixed, so this list stays true.
GAPS = {}

panda = Panda(generic=sys.argv[1])
arch = panda.arch_name
C = Checks(arch, GAPS)
if arch != "x86_64":
    print(f"syscall_cover: no program for {arch} yet")
    sys.exit(C.report("SYSCALL_COVER skipped"))

so = Path(panda.get_plugin_path()) / f"libpanda-sctrace_{arch}-softmmu.so"
panda.load_plugin("sctrace")
ffi = FFI()
ffi.cdef("""
typedef struct { uint32_t kind; int32_t abi; int32_t cpu; uint32_t flags; uint64_t id; int64_t num;
                 uint64_t args[6]; uint64_t pc; int64_t ret; } SctraceEvent;
int sctrace_start(uint64_t lo, uint64_t hi, uint64_t cap, int follow);
void sctrace_stop(void);
uint64_t sctrace_count(void);
uint64_t sctrace_lost(void);
uint64_t sctrace_read(uint64_t first, uint64_t n, SctraceEvent *out);
""")
lib = ffi.dlopen(str(so))


def sites(prog):
    """[(pc after it, kind)] for each syscall / int 0x80 in the program's text, in order."""
    elf = base64.b64decode(importlib.import_module("sccover_programs").PROGRAMS[prog])
    wide = elf[4] == 2
    if wide:
        shoff, = struct.unpack_from("<Q", elf, 0x28)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 0x3a)
        fmt, at = "<IIQQQQ", (0, 3, 4, 5)
    else:
        shoff, = struct.unpack_from("<I", elf, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 0x2e)
        fmt, at = "<IIIIII", (0, 3, 4, 5)
    sec = [struct.unpack_from(fmt, elf, shoff + i * shentsize) for i in range(shnum)]
    names = sec[shstrndx][4]
    for s in sec:
        name, addr, off, size = (s[i] for i in at)
        if elf[names + name:elf.index(b"\0", names + name)] == b".text":
            text = elf[off:off + size]
            found = [(i, kind) for kind, pat in (("syscall", b"\x0f\x05"), ("int80", b"\xcd\x80"))
                     for i in range(len(text) - 1) if text[i:i + 2] == pat]
            return [(addr + i + 2, kind) for i, kind in sorted(found)]
    raise ValueError("no .text")


def expected(prog):
    """([(abi, num, args, group, site)], number of sweep calls, [site kinds]): every call the
    program makes, in order. site: an index into sites(prog), or "vdso". args None: any."""
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
        for nr in SPECIALS64:
            sweep(ABI_X86_64, nr, "syscall specials", 4, BASE64, M64)
        for nr in range(600):
            sweep(ABI_I386, nr, "int 0x80", 5, BASE64, M32)
        calls += [(ABI_X86_64, 1, [MARK64 | 1, None, 8 * seq[0], None, None, None], "write", 6),
                  (ABI_X86_64, 231, [MARK64, None, None, None, None, None], "exit_group", 7)]
        kinds = ["syscall"] * 5 + ["int80"] + ["syscall"] * 2
    else:
        calls += [(ABI_I386, 172, [38, 1, 0, 0, 0, None], "prctl, seccomp", 0),
                  (ABI_I386, 354, [1, 0, None, None, None, None], "prctl, seccomp", 1)]
        for nr in range(600):
            sweep(ABI_I386, nr, "int 0x80", 2, BASE32, M32)
        for nr in range(600):
            sweep(ABI_I386, nr, "vDSO", "vdso", BASE32, M32)
        for nr in SPECIALS32:
            sweep(ABI_I386, nr, "int 0x80 specials", 3, BASE32, M32)
        calls += [(ABI_I386, 4, [1, None, 4 + 4 * seq[0], None, None, MARK32], "write", 4),
                  (ABI_I386, 252, [0, None, None, None, None, MARK32], "exit_group", 5)]
        kinds = ["int80"] * 6
    return calls, seq[0], kinds


def run(prog, name="sccover"):
    """Upload and run <name>_<prog>; (output, events, lost, written bytes)."""
    path = upload(panda, name, prog, f"/tmp/{name}_{prog}")
    rc = lib.sctrace_start(PROGRAM_BASE, PROGRAM_END, 1 << 16, int(prog == "i386"))
    if rc != 0:
        return f"sctrace_start: {rc}", [], 0, b""
    progress(f"running {path}")
    out = panda.run_serial_cmd(f"{path} > {path}.out; echo rc=$?")
    lib.sctrace_stop()
    written = panda.run_serial_cmd(f"base64 -w0 {path}.out; echo")
    n = lib.sctrace_count()
    buf = ffi.new("SctraceEvent[]", max(n, 1))
    lib.sctrace_read(0, n, buf)
    events = [(e.kind, e.abi, e.cpu, e.flags, e.id, e.num, list(e.args), e.pc, e.ret)
              for e in buf[0:n]]
    try:
        raw = base64.b64decode(written.strip().splitlines()[-1])
    except (IndexError, ValueError):
        raw = b""
    return out, events, lib.sctrace_lost(), raw


def check(prog, out, events, lost, raw):
    calls, n_sweep, kinds = expected(prog)
    p = f"{prog}: "
    if prog == "x86_64":
        vsys, rets = None, list(struct.unpack(f"<{len(raw) // 8}q", raw[:len(raw) // 8 * 8]))
        written = 8 * n_sweep
    else:
        vsys = struct.unpack_from("<I", raw)[0] if len(raw) >= 4 else None
        rets = list(struct.unpack(f"<{len(raw) // 4 - 1}i", raw[4:len(raw) // 4 * 4])) if raw else []
        written = 4 + 4 * n_sweep
    C.check(p + "the program ran to the end (rc=0)", "rc=0" in out, out)
    C.check(p + f"the program wrote back {n_sweep} return values, all {REFUSED}",
            len(rets) == n_sweep and set(rets) == {REFUSED}, (len(rets), sorted(set(rets))[:8]))
    C.check(p + "nothing was lost", lost == 0, lost)
    pcs = sites(prog)
    C.check(p + f"the program has its {len(kinds)} call sites", [k for _, k in pcs] == kinds, pcs)

    enters = [e for e in events if e[0] == 1]
    by_id = {}
    for e in events:
        by_id.setdefault(e[4], []).append(e)
    groups = list(dict.fromkeys(c[3] for c in calls))
    bad_enter, bad_return = [], {g: [] for g in groups}
    vdso_pcs = set()
    for i, (abi, num, args, group, site) in enumerate(calls):
        if i >= len(enters):
            bad_enter.append((i, "missing"))
            continue
        kind, eabi, cpu, flags, eid, enum, eargs, pc, _ = enters[i]
        if site == "vdso":
            vdso_pcs.add(pc)
            pc_ok = vsys is not None and vsys < pc <= vsys + 64
        else:
            pc_ok = len(pcs) == len(kinds) and pc == pcs[site][0]
        ok = (eabi == abi and enum & M64 == num & M64 and pc_ok and flags == 0 and
              all(w is None or w == g for w, g in zip(args, eargs)))
        if not ok and len(bad_enter) < 10:
            bad_enter.append((i, group, dict(abi=(eabi, abi), num=(hex(enum & M64), hex(num & M64)),
                                             pc=hex(pc), args=[(hex(g), w and hex(w))
                                                               for w, g in zip(args, eargs)
                                                               if w is not None and w != g])))
        rest = [e for e in by_id.get(eid, []) if e[0] != 1]
        bad = bad_return[group]
        if i == len(calls) - 1:                 # exit_group: never returns
            if rest:
                bad.append((i, "exit_group returned", rest))
            continue
        seq = i - 2
        want_ret = 0 if i < 2 else written if i == len(calls) - 2 else REFUSED
        if (len(rest) != 1 or rest[0][0] != 2 or rest[0][7] != pc or rest[0][8] != want_ret or
                rest[0][3]):
            bad.append((i, [("return" if k == 2 else "drop" if k == 3 else k, hex(rp), r, f)
                            for k, _, _, f, _, _, _, rp, r in rest], want_ret))
        elif 0 <= seq < len(rets) and rest[0][8] != rets[seq]:
            bad.append((i, "trap and program disagree", rest[0][8], rets[seq]))

    C.check(p + f"exactly {len(calls)} enters, one per call", len(enters) == len(calls),
            len(enters))
    C.check(p + "each enter has the call's ABI, number, arguments and pc", not bad_enter,
            bad_enter[:10])
    if vdso_pcs:
        C.check(p + "the vDSO's calls are all at one pc", len(vdso_pcs) == 1,
                [hex(x) for x in vdso_pcs])
    for g in groups:
        what = "never returns" if g == "exit_group" else "each has one return with its pc and value"
        C.check(p + f"{g}: {what}", not bad_return[g],
                f"{len(bad_return[g])} calls: {bad_return[g][:5]}")
    C.check(p + "every event is from vCPU 0 (one vCPU)", {e[2] for e in events if e[0] != 3} <= {0},
            {e[2] for e in events})
    return len(calls), len(events)


NR = {"read": 0, "write": 1, "rt_sigaction": 13, "rt_sigreturn": 15, "getpid": 39, "nanosleep": 35,
      "clone": 56, "fork": 57, "vfork": 58, "execve": 59, "exit": 60, "wait4": 61,
      "restart_syscall": 219, "exit_group": 231, "clone3": 435}
EINTR, ENOENT, EBADF = 4, 2, 9


def check_paths(out, events, lost, raw):
    """scpaths_x86_64: the return paths (see its header). Events by case, split at the markers."""
    p = "x86_64 return paths: "
    C.check(p + "the program ran to the end (execve(/bin/true): rc=0)", "rc=0" in out, out)
    C.check(p + "nothing was lost", lost == 0, lost)
    cases, cur = {}, None
    for e in events:
        kind, abi, cpu, flags, eid, num, args, pc, ret = e
        if kind == 1 and num == NR["write"] and args[0] == M64:
            cur = args[2]
        cases.setdefault(cur, []).append(e)
    calls = {}                  # id: (enter, [returns and drops])
    for e in events:
        if e[0] == 1:
            calls[e[4]] = (e, [])
        elif e[4] in calls:
            calls[e[4]][1].append(e)

    def of(case, name):
        """[(enter, [later events])] of the calls named name in case, in order."""
        return [calls[e[4]] for e in cases.get(case, []) if e[0] == 1 and e[5] == NR[name]]

    def rets(c):
        return [(r[8], r[3]) for r in c[1] if r[0] == 2]

    def one(case, name, i=0):
        found = of(case, name)
        return found[i] if len(found) > i else (None, [])

    def check(case, what, ok, detail):
        C.check(p + f"{case} {what}", ok, detail)

    getpid = one(1, "getpid")
    check(1, "iret: getpid (TF set) returns once, with the pid", len(rets(getpid)) == 1 and
          rets(getpid)[0][0] > 0 and rets(getpid)[0][1] == 0, rets(getpid))
    sigret = [c for case in cases for c in of(case, "rt_sigreturn")]
    C.check(p + "rt_sigreturn never returns (each: enter only, or a drop)",
            sigret and all(not rets(c) for c in sigret), [rets(c) for c in sigret])
    reads = of(2, "read")
    check(2, "restart: the read returns restarted, enters again at its pc, and returns 1",
          len(reads) == 2 and rets(reads[0]) and rets(reads[0])[-1][1] == 1 and
          reads[1][0][7] == reads[0][0][7] and reads[1][0][6][:3] == reads[0][0][6][:3] and
          rets(reads[1]) == [(1, 0)], [(r[0][6][:3], rets(r)) for r in reads])
    reads = of(3, "read")
    check(3, "eintr: the read returns -EINTR, not restarted", len(reads) == 1 and
          rets(reads[0]) == [(-EINTR, 0)], [rets(r) for r in reads])
    sleeps = [c for c in of(4, "nanosleep") if c[0][6][0] and rets(c) and rets(c)[-1][1] == 1]
    rs = of(4, "restart_syscall")
    check(4, "restart_syscall: the stopped nanosleep returns restarted, restart_syscall enters at its "
          "pc and returns 0", len(sleeps) == 1 and len(rs) == 1 and rs[0][0][7] == sleeps[0][0][7]
          and rets(rs[0]) == [(0, 0)], (len(sleeps), [(hex(r[0][7]), rets(r)) for r in rs]))
    for case, name in ((5, "fork"), (6, "vfork"), (7, "clone"), (8, "clone3")):
        c = one(case, name)
        waited = [rets(w)[0][0] for w in of(case, "wait4") if rets(w)]
        child = waited[0] if waited else None
        if name == "clone":
            child = rets(c)[0][0] if rets(c) else None          # the thread's tid
        check(case, f"{name}: one return, in the parent, with the child's id (the child's return "
              "is not reported)", rets(c) == [(child, 0)] and child and child > 0,
              dict(returns=rets(c), child=child))
    for case, name in ((5, "exit_group"), (6, "exit_group"), (7, "exit"), (8, "exit_group")):
        ex = of(case, name)
        check(case, f"the child's {name} never returns", len(ex) == 1 and not rets(ex[0]),
              [rets(x) for x in ex])
    check(9, "execve of a missing file returns -ENOENT", rets(one(9, "execve")) == [(-ENOENT, 0)],
          rets(one(9, "execve")))
    ex = one(10, "execve")
    check(10, "execve(/bin/true) enters and never returns", ex[0] is not None and not rets(ex),
          rets(ex))
    marks = [e[6][2] for e in events if e[0] == 1 and e[5] == NR["write"] and e[6][0] == M64]
    C.check(p + "the markers are 1..10 in order, each returning -EBADF once",
            marks == list(range(1, 11)) and all(rets(calls[e[4]]) == [(-EBADF, 0)] for e in events
                                                if e[0] == 1 and e[5] == NR["write"] and
                                                e[6][0] == M64), marks)
    drops = [calls[e[4]][0][5] for e in events if e[0] == 3 and e[4] in calls]
    C.check(p + "only rt_sigreturn calls are dropped", set(drops) <= {NR["rt_sigreturn"]}, drops)
    return len(events)


R = {}


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    R["cpu"] = panda.run_serial_cmd("grep -m1 vendor_id /proc/cpuinfo")
    for prog in ("x86_64", "i386"):
        R[prog] = run(prog)
    R["paths"] = run("x86_64", "scpaths")
    panda.end_analysis()


panda.run()

summary = []
for prog in ("x86_64", "i386"):
    if prog in R:
        n_calls, n_events = check(prog, *R[prog])
        summary.append(f"{prog} calls={n_calls} events={n_events}")
    else:
        C.check(f"{prog}: ran", False, "the guest stopped before it")
if "paths" in R:
    summary.append(f"paths events={check_paths(*R['paths'])}")
else:
    C.check("x86_64 return paths: ran", False, "the guest stopped before it")
print("guest CPU:", R.get("cpu", "?").strip())
sys.exit(C.report(f"SYSCALL_COVER arch={arch} " + " ".join(summary)))
