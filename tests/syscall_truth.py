"""The guest kernel as the reference: every system call the kernel's ftrace records, the engine's
syscall trap reports, with the same number, arguments and return, on the same vCPU and in the same
order.

Run: PANDA_PLUGIN_DIR=<test-plugins>/lib/panda/plugins panda-python tests/syscall_truth.py \\
         <dir>/debian-13-x86_64.json [--smp N] [--intel]
(x86_64 only for now; the sctrace plugin is in `nix build .#test-plugins`). It runs on a
tests/testmachine.py machine on the image's kernel (busybox, the agent; built and snapshotted the
first time, about 10 s), with N vCPUs (default 1); --intel makes the CPU GenuineIntel, whose
32-bit vDSO enters through sysenter (the default, qemu64, is AMD: syscall). Programs go in and the
kernel's trace comes out through the agent.

For each workload the guest's ftrace records raw_syscalls:sys_enter and sys_exit (pid, number,
arguments, return) and the sctrace plugin records every trap event, both around the workload. The
tracing window is inside the trap's, so on each vCPU the kernel's enters are a contiguous run of
the trap's: the trap's enter comes at the instruction and the kernel's tracepoint follows it on the
same vCPU before anything can preempt the task (the guest is PREEMPT_VOLUNTARY). Checks, per
workload:
- the enters: on every vCPU, the kernel's enters are the trap's, in order, with the same number
  and arguments (i386: 32 bits); the first ten differences are printed with their context;
- the returns, per call (the kernel's enter gives the trap's call id; the kernel's exit is that
  task's next event): the trap reports exactly one return, with the kernel's value; except
  - a call the kernel ends with -ERESTART*: the trap's return has `restart` set, or carries -EINTR
    (the signal code's value, which the task sees), or there is none (the task was killed);
  - no return: exit and exit_group (the kernel records no exit either), execve and execveat that
    succeed, and rt_sigreturn/sigreturn (they return to the interrupted code, not past the call);
  - a task's last call in the window may have no exit recorded, or an exit and no return (the
    task died on its way to user mode, as a thread does in a group exit): not checked;
- nothing was lost by either side (ftrace's overrun counters, the plugin's buffer).
A fork child's first event is the exit of its parent's clone (ret 0) with no enter: the trap
doesn't report child returns, and these are skipped. Checks are split by ABI and vfork, so that
the trap's known bugs (testlib.Checks GAPs, listed below) don't hide the rest.
"""
import argparse
import base64
import importlib
import re
import sys
from collections import defaultdict
from pathlib import Path

from testlib import Checks, progress
from testmachine import Agent, Machine
from cffi import FFI

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
ap.add_argument("--intel", action="store_true")
opt = ap.parse_args()

ABI_X86_64, ABI_I386 = 0, 1
M64, M32 = (1 << 64) - 1, (1 << 32) - 1
T = "/sys/kernel/tracing"
RESTART = {-512, -513, -514, -516}      # ERESTARTSYS, ERESTARTNOINTR, ERESTARTNOHAND, ERESTART_RESTARTBLOCK
EINTR = 4
# Per ABI: calls the kernel never ends (no exit event), calls that don't return past the call
# (rt_sigreturn), calls that don't return when they succeed (execve), and vfork.
NO_EXIT = {ABI_X86_64: {60, 231}, ABI_I386: {1, 252}}
SIGRETURN = {ABI_X86_64: {15}, ABI_I386: {119, 173}}
EXECVE = {ABI_X86_64: {59, 322}, ABI_I386: {11, 358}}
VFORK = {ABI_X86_64: {58}, ABI_I386: {190}}
CLONE = {ABI_X86_64: 56, ABI_I386: 120}
CLONE_VFORK = 0x4000

# Workloads: name, the shell command run inside the window. Each exits 0 but threads (3: its side
# thread's exit_group).
WORKLOADS = [
    ("x86_64 program", "/tmp/sctruth_x86_64"),
    ("i386 program", "/tmp/sctruth_i386"),
    ("return paths", "/tmp/scpaths_x86_64"),
    ("pipelines", "ls -la /bin | sort -k5 -n | tail -3 | md5sum; "
                  "find /proc/self/ -maxdepth 1 | xargs ls -ld | wc -l; grep -c processor /proc/cpuinfo"),
    ("fork/exec", "for i in $(seq 50); do /bin/true; sh -c 'echo x > /dev/null'; done"),
    ("threads", "/tmp/scwork threads"),
    ("signals", "/tmp/scwork signals"),
]

# Known trap bugs: GAP while they fail, XPASS (a failure) once fixed.
GAPS = {}
machine = Machine(opt.image, smp=opt.smp, intel=opt.intel)
panda = machine.panda()
agent = Agent(panda)
arch = panda.arch_name
C = Checks(arch, GAPS)
if arch != "x86_64":
    print(f"syscall_truth: no workloads for {arch} yet")
    sys.exit(C.report("SYSCALL_TRUTH skipped"))

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

LINE = re.compile(r"^\s*(.*)-(\d+)\s+\[(\d+)\]\s+\S+\s+(\d+\.\d+): sys_(enter|exit): NR (-?\d+) "
                  r"(?:\(([0-9a-f, ]*)\)|= (-?\d+))$")


def program(name, arch):
    return base64.b64decode(importlib.import_module(f"{name}_programs").PROGRAMS[arch])


def run(name, cmd, ncpu):
    """Run cmd inside both windows; (ftrace events per CPU, trap events, trap lost, overruns)."""
    rc = lib.sctrace_start(0, M64, 1 << 21, 0)
    if rc != 0:
        return None, f"sctrace_start: {rc}", 0, {}, None
    progress(f"{name}: {cmd[:60]}")
    status, out = agent.run(f"echo > {T}/trace; echo 1 > {T}/tracing_on; {cmd}; rc=$?; "
                            f"echo 0 > {T}/tracing_on; echo rc=$rc")
    rc = out.decode(errors="replace").rsplit("rc=", 1)[-1].strip()
    lib.sctrace_stop()
    n = lib.sctrace_count()
    buf = ffi.new("SctraceEvent[]", max(n, 1))
    lib.sctrace_read(0, n, buf)
    trap = [(e.kind, e.abi, e.cpu, e.flags, e.id, e.num, list(e.args), e.pc, e.ret)
            for e in buf[0:n]]
    lost = lib.sctrace_lost()
    ftrace, overrun = {}, {}
    for c in range(ncpu):
        agent.run(f"cat {T}/per_cpu/cpu{c}/trace > /tmp/trace")
        text = agent.get("/tmp/trace").decode(errors="replace")
        stats = agent.get(f"{T}/per_cpu/cpu{c}/stats").decode()
        overrun[c] = sum(int(m) for m in re.findall(r"(?:overrun|dropped events): (\d+)", stats))
        evs = []
        for line in text.splitlines():
            m = LINE.match(line)
            if not m:
                continue
            comm, pid, cpu, ts, kind, nr, args, ret = m.groups()
            evs.append(dict(comm=comm.strip(), pid=int(pid), cpu=int(cpu), ts=float(ts), kind=kind,
                            nr=int(nr), args=[int(a, 16) for a in args.split(", ")] if args else None,
                            ret=int(ret) if ret is not None else None, i=len(evs)))
        ftrace[c] = evs
    return ftrace, trap, lost, overrun, rc


def same_enter(f, t):
    """Is the kernel's enter f the trap's enter t?"""
    mask = M32 if t[1] == ABI_I386 else M64
    return ((f["nr"] - t[5]) & mask) == 0 and all(((a - b) & mask) == 0
                                                  for a, b in zip(f["args"], t[6]))


def align(F, Tc):
    """Walk the kernel's enters F against the trap's Tc from the first offset where they agree;
    ({index in F: index in Tc}, [differences])."""
    def agree(i, j, k=16):
        return all(same_enter(F[i + d], Tc[j + d]) for d in range(min(k, len(F) - i, len(Tc) - j)))

    start = next((j for j in range(len(Tc)) if same_enter(F[0], Tc[j]) and agree(0, j)), None)
    if start is None:
        return {}, [("no offset in the trap's enters matches the kernel's first ones",
                     fmt_f(F[0]), len(Tc))]
    pairs, diffs, i, j = {}, [], 0, start
    while i < len(F) and j < len(Tc):
        if same_enter(F[i], Tc[j]):
            pairs[i] = j
            i, j = i + 1, j + 1
            continue
        extra = next((k for k in range(1, 32) if j + k < len(Tc) and agree(i, j + k, 4)), None)
        missing = next((k for k in range(1, 32) if i + k < len(F) and agree(i + k, j, 4)), None)
        ctx = dict(kernel=[fmt_f(x) for x in F[max(i - 2, 0):i + 3]],
                   trap=[fmt_t(x) for x in Tc[max(j - 2, 0):j + 3]])
        if extra is not None and (missing is None or extra <= missing):
            diffs.append((f"the trap has {extra} enter(s) the kernel doesn't", ctx))
            j += extra
        elif missing is not None:
            diffs.append((f"the trap is missing {missing} enter(s)", ctx))
            i += missing
        else:
            diffs.append(("different", ctx))
            i, j = i + 1, j + 1
    if i < len(F):
        diffs.append((f"the trap's enters end {len(F) - i} before the kernel's",
                      [fmt_f(x) for x in F[i:i + 3]]))
    return pairs, diffs


def fmt_f(f):
    if f["kind"] == "enter":
        return f"{f['comm']}-{f['pid']} enter {f['nr']} ({', '.join(hex(a) for a in f['args'])})"
    return f"{f['comm']}-{f['pid']} exit {f['nr']} = {f['ret']}"


def fmt_t(t):
    kind = {1: "enter", 2: "return", 3: "drop"}[t[0]]
    return (f"#{t[4]} {kind} abi={t[1]} nr={t[5]} ({', '.join(hex(a) for a in t[6])}) pc={t[7]:#x}"
            + (f" = {t[8]}" if t[0] == 2 else ""))


def check(name, ftrace, trap, lost, overrun):
    p = f"{name}: "
    C.check(p + "nothing was lost (ftrace overruns, the plugin's buffer)",
            lost == 0 and not any(overrun.values()), dict(lost=lost, overrun=overrun))
    enters = defaultdict(list)                  # cpu: [trap enter]
    by_id = defaultdict(list)                   # id: [trap return or drop]
    for e in trap:
        if e[0] == 1:
            enters[e[2]].append(e)
        else:
            by_id[e[4]].append(e)

    # Enters, CPU by CPU.
    trap_of = {}                                # (cpu, ftrace index): trap enter
    n_enters, enter_diffs = 0, []
    for c, evs in ftrace.items():
        F = [f for f in evs if f["kind"] == "enter"]
        if not F:
            continue
        n_enters += len(F)
        pairs, diffs = align(F, enters.get(c, []))
        enter_diffs += [(c, *d) for d in diffs]
        for i, j in pairs.items():
            trap_of[(c, F[i]["i"])] = enters[c][j]
    C.check(p + f"on every vCPU the kernel's {n_enters} enters are the trap's, in order",
            n_enters and not enter_diffs, enter_diffs[:10])

    # Returns, call by call: each task's events in time order.
    tasks = defaultdict(list)
    for c, evs in ftrace.items():
        for f in evs:
            tasks[f["pid"]].append(f)
    bad, counted = defaultdict(list), defaultdict(int)
    for pid, evs in tasks.items():
        evs.sort(key=lambda f: (f["ts"], f["cpu"], f["i"]))
        for k, f in enumerate(evs):
            if f["kind"] != "enter":
                continue
            t = trap_of.get((f["cpu"], f["i"]))
            if t is None:
                continue                        # an enter the trap missed: counted above
            abi, nr = t[1], t[5] & (M32 if t[1] == ABI_I386 else M64)
            vfork = nr in VFORK[abi] or (nr == CLONE[abi] and t[6][0] & CLONE_VFORK)
            group = "vfork" if vfork else {ABI_X86_64: "x86_64 ABI", ABI_I386: "i386 ABI"}[abi]
            counted[group] += 1
            nxt = evs[k + 1] if k + 1 < len(evs) else None
            # A task's calls are serial: its next event is this call's exit. The exit's number can
            # differ: -1 for rt_sigreturn, the i386 one after an execve of a 32-bit program.
            exit_ = nxt if nxt and nxt["kind"] == "exit" else None
            last = k + 2 >= len(evs)
            rets = [r for r in by_id.get(t[4], []) if r[0] == 2]
            got = [(r[8], r[3]) for r in rets]
            mask = M32 if abi == ABI_I386 else M64
            if exit_ is None:
                if nr in NO_EXIT[abi]:
                    ok, want = not rets, "no return (it ends the task)"
                elif nxt is None:
                    continue                    # the task's last call in the window
                else:
                    ok, want = False, f"the kernel has no exit before {fmt_f(nxt)}"
            elif nr in SIGRETURN[abi] or (nr in EXECVE[abi] and exit_["ret"] == 0):
                ok, want = not rets, "no return"
            elif exit_["ret"] in RESTART:
                ok = len(rets) <= 1 and (not rets or rets[0][3] == 1 or
                                         ((rets[0][8] + EINTR) & mask) == 0)
                want = f"restarted, -EINTR or none ({exit_['ret']})"
            elif not rets and last:
                continue                        # the task may have died before user mode (a group
                                                # exit), or not got there by the window's end
            else:
                ok = len(rets) == 1 and ((rets[0][8] - exit_["ret"]) & mask) == 0 and not rets[0][3]
                want = exit_["ret"]
            if not ok:
                bad[group].append((fmt_f(f), dict(want=want, trap=got,
                                                  drops=sum(r[0] == 3 for r in by_id[t[4]]))))
    # How spread the workload was: enters per vCPU, and calls that returned on another vCPU.
    cpu_of = {e[4]: e[2] for e in trap if e[0] == 1}
    moved = sum(1 for e in trap if e[0] == 2 and cpu_of.get(e[4], e[2]) != e[2])
    counted["spread"] = (dict(sorted((c, len(v)) for c, v in enters.items())), moved)
    for group, n in counted.items():
        if group == "spread":
            continue
        C.check(p + f"{group}: each call's return is the kernel's", not bad[group],
                f"{len(bad[group])} of {n} calls: {bad[group][:5]}")
    return n_enters, dict(counted)


R = {}


@panda.queue_blocking
def drive():
    machine.revert(panda, agent)
    R["cpu"] = agent.run("grep -m1 vendor_id /proc/cpuinfo")[1].decode().strip()
    ncpu = int(agent.run("nproc")[1])
    R["ncpu"] = ncpu
    agent.put("/tmp/sctruth_x86_64", program("sctruth", "x86_64"))
    agent.put("/tmp/sctruth_i386", program("sctruth", "i386"))
    agent.put("/tmp/scpaths_x86_64", program("scpaths", "x86_64"))
    agent.put("/tmp/scwork", program("scwork", "x86_64"))
    agent.run(f"echo 0 > {T}/tracing_on; echo {32768 // ncpu} > {T}/buffer_size_kb; "
              f"echo global > {T}/trace_clock; echo 8192 > {T}/saved_cmdlines_size; "
              f"echo 1 > {T}/events/raw_syscalls/enable")
    for name, cmd in WORKLOADS:
        R[name] = run(name, cmd, ncpu)
    panda.end_analysis()


panda.run()

summary = []
for name, _ in WORKLOADS:
    if name not in R:
        C.check(f"{name}: ran", False, "the guest stopped before it")
        continue
    ftrace, trap, lost, overrun, rc = R[name]
    if ftrace is None:
        C.check(f"{name}: ran", False, trap)
        continue
    want = "3" if name == "threads" else "0"
    C.check(f"{name}: the workload exited {want}", rc == want, rc)
    n, groups = check(name, ftrace, trap, lost, overrun)
    per_cpu, moved = groups["spread"]
    summary.append(f"{name.replace(' ', '_')}={n}")
    print(f"{name}: kernel enters {n}; trap enters per vCPU {per_cpu}; returns on another vCPU "
          f"{moved}")
print("guest CPU:", R.get("cpu", "?"), "vCPUs:", R.get("ncpu", "?"))
sys.exit(C.report(f"SYSCALL_TRUTH arch={arch} smp={opt.smp} " + " ".join(summary)))
