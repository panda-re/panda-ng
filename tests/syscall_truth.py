"""The guest kernel as the reference: every system call the kernel's ftrace records, syscalls2
reports, with the same number, arguments and return, on the same vCPU and in the same order.

Run: panda-python tests/syscall_truth.py <images>/busybox-<arch>.json [--smp N] [--intel]
(tests/mkbusybox.py images: x86_64, aarch64, armel, riscv64, mipsel, mipseb, mips64el, mips64eb,
powerpc, powerpc64, powerpc64le; an i386 one is a 32-bit Debian kernel with busybox, made the same
way). It runs on a tests/testmachine.py machine (built and snapshotted the first time, 4-10 s) with
N vCPUs (default 1; the MIPS malta and PowerPC e500 machines boot one CPU); --intel makes the x86
CPU GenuineIntel, whose 32-bit vDSO on x86_64 enters through sysenter (the default, qemu64, is AMD:
syscall). Programs go in and the kernel's trace comes out through the agent. 10-40 s per arch once
the machine exists. On aarch64 the arm builds of the programs also run, as 32-bit tasks; on x86_64
and i386, a 32-bit program through int 0x80 and the vDSO.

On these guests (Linux) syscalls2's returns come from the engine's pairing (panda/syscall-pair.h),
keyed by task (x86_64: the per-CPU kernel stack word; i386: TSS.sp1; aarch64: SP_EL1; arm: SP_svc;
riscv64: sscratch; MIPS: kernelsp[cpu]; ppc: SPRG3; ppc64: the PACA's kstack); on x86_64 its
x86_64 ABI and the i386 ABIs (int 0x80, and a 32-bit task's vDSO
entries: sysenter, and syscall from compat mode). on_all_sys_enter2 and on_all_sys_return2
record every call, with the call's id from syscalls2_current_call() (the same at its entry and its
return) and the arguments where the kernel reads them at the entry: the ABI's registers, and for
a vDSO entry argument 2 from ebp (syscall) and argument 6 from the user stack, where
__kernel_vsyscall pushed ebp; MIPS o32 arguments 5 and 6 from the user stack (unknown, and not
compared, when the page is not in the TLB, through which alone the host reaches it). A return's
value is the kernel's: MIPS and PowerPC sc report an error with a flag ($a3, CR0.SO) and the
positive error number, which the test negates.
SC2_ARGS="task_key=false" loads syscalls2 with the call key instead. SC2_ON_ALL=1 records with
on_all_sys_enter and on_all_sys_return instead (the callbacks without a syscall_ctx_t: the
number and the address), the ABI from the trap (its instruction, the task's mode, the number's
range): the same checks but syscalls2's own decoding of mmap2's argument 6, which needs the
context. SC2_SETS_LOG=n gives the
engine 2^n sets of 4 pending calls (PANDA_SYSCALL_PAIR_SETS_LOG; 0: one set, so that the
workloads' blocked calls fill it): the oldest calls are evicted, so a call may have no return,
but a return delivered must still be its call's, and there must be no more calls without a
return than calls evicted or not kept (a full set whose other calls are all being delivered).

The "blocking" workload has many tasks blocked in reads and nanosleeps at once and moves them to
other vCPUs while they are blocked: with --smp 4 their calls return on other vCPUs than they
entered.

For each workload the guest's ftrace records raw_syscalls:sys_enter and sys_exit (pid, number,
arguments, return) and syscalls2 every call, both around the workload. The tracing window is
inside syscalls2's, so on each vCPU the kernel's enters are a contiguous run of syscalls2's: its
enter comes at the trap and the kernel's tracepoint follows it on the same vCPU before anything
can preempt the task. Checks, per workload:
- the enters: on every vCPU, the kernel's enters are syscalls2's, in order, with the same number
  and arguments (i386 and arm: 32 bits); the first ten differences are printed with their context;
- the i386 program's calls through the vDSO: syscalls2 reports them (under its i386 profile),
  mmap2's argument 6 as syscalls2 decodes it is the kernel's, and signals interrupted some (a
  restart runs the vDSO's int $0x80: a new call);
- each call the kernel restarted (it ended with -ERESTART* and syscalls2 reported no return)
  is one of the engine's restarts (counted over the workload), so none was left pending;
- the returns, per call (the kernel's enter gives syscalls2's call id; the kernel's exit is that
  task's next event): exactly one return, with the kernel's value; except
  - a call the kernel ends with -ERESTART*: none (the task traps again: a new call), or one with
    -EINTR (the signal code's value, which the task sees);
  - no return: exit and exit_group (the kernel records no exit either), execve and execveat that
    succeed, and rt_sigreturn/sigreturn (they return to the interrupted code, not past the call);
    and the calls syscalls2's tables mark panda_noreturn (execve on arm and i386, execveat), even
    when they fail;
  - a task's last call in the window may have no exit recorded, or an exit and no return (the
    task died on its way to user mode, as a thread does in a group exit): not checked;
- nothing was lost (ftrace's overrun counters).
A child's first event is the exit of its parent's clone (ret 0) with no enter. syscalls2 reports
it for a child in its caller's address space (vfork, threads) as a return of the caller's call,
flagged CHILD: these are not the caller's return, and are left out. Checks are split by ABI,
int 0x80 from a 64-bit task, the i386 vDSO entries (whose returns land past the vDSO's int $0x80,
not after the entry instruction), vfork and clone3 (whose flags are in memory).
"""
import argparse
import base64
import importlib
import os
import re
import sys
import threading
from collections import defaultdict, namedtuple

from testlib import Checks, progress
from testmachine import AGENT_ARCH, Agent, Machine, agent_key

PANDA_TEST = dict(area="syscalls", tier="guest", fixture="testmachine",
                  arches=["x86_64", "aarch64", "arm", "i386", "riscv64", "mipsel", "mips", "mips64el",
                          "mips64", "ppc", "ppc64"],
                  plugins=["syscalls2"], variants=dict(smp=[1, 4], intel=[False, True]), timeout=1800)

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
ap.add_argument("--intel", action="store_true")
opt = ap.parse_args()

ABI_X86_64, ABI_I386, ABI_AARCH64, ABI_ARM, ABI_RISCV64, ABI_O32, ABI_N64, ABI_PPC32, ABI_PPC64 = range(9)
ENTER, RETURN = 1, 2
M64, M32 = (1 << 64) - 1, (1 << 32) - 1
T = "/sys/kernel/tracing"
RESTART = {-512, -513, -514, -516}      # ERESTARTSYS, ERESTARTNOINTR, ERESTARTNOHAND, ERESTART_RESTARTBLOCK
EINTR = 4
# Per ABI: calls the kernel never ends (no exit event), calls that don't return past the call
# (rt_sigreturn), calls that don't return when they succeed (execve), vfork, and clone.
NO_EXIT = {ABI_X86_64: {60, 231}, ABI_I386: {1, 252}, ABI_AARCH64: {93, 94}, ABI_ARM: {1, 248},
           ABI_RISCV64: {93, 94}, ABI_O32: {4001, 4246}, ABI_N64: {5058, 5205},
           ABI_PPC32: {1, 234}, ABI_PPC64: {1, 234}}
SIGRETURN = {ABI_X86_64: {15}, ABI_I386: {119, 173}, ABI_AARCH64: {139}, ABI_ARM: {119, 173},
             ABI_RISCV64: {139}, ABI_O32: {4119, 4193}, ABI_N64: {5211}, ABI_PPC32: {119, 172},
             ABI_PPC64: {119, 172}}
EXECVE = {ABI_X86_64: {59, 322}, ABI_I386: {11, 358}, ABI_AARCH64: {221, 281}, ABI_ARM: {11, 387},
          ABI_RISCV64: {221, 281}, ABI_O32: {4011, 4356}, ABI_N64: {5057, 5316},
          ABI_PPC32: {11, 362}, ABI_PPC64: {11, 362}}
# Calls syscalls2's tables mark panda_noreturn: it reports no return for them, even a failed one
SC2_NORETURN = {ABI_X86_64: {322}, ABI_I386: {11, 358}, ABI_AARCH64: set(), ABI_ARM: {11, 387},
                ABI_RISCV64: set(), ABI_O32: {4356}, ABI_N64: set(), ABI_PPC32: set(),
                ABI_PPC64: set()}
VFORK = {ABI_X86_64: {58}, ABI_I386: {190}, ABI_AARCH64: set(), ABI_ARM: {190}, ABI_RISCV64: set(),
         ABI_O32: set(), ABI_N64: set(), ABI_PPC32: {189}, ABI_PPC64: {189}}
CLONE = {ABI_X86_64: 56, ABI_I386: 120, ABI_AARCH64: 220, ABI_ARM: 120, ABI_RISCV64: 220,
         ABI_O32: 4120, ABI_N64: 5055, ABI_PPC32: 120, ABI_PPC64: 120}
CLONE3 = 435
CLONE_VFORK = 0x4000
ABI_GROUP = {ABI_X86_64: "x86_64 ABI", ABI_I386: "i386 ABI", ABI_AARCH64: "aarch64 ABI",
             ABI_ARM: "arm ABI", ABI_RISCV64: "riscv64 ABI", ABI_O32: "o32 ABI", ABI_N64: "n64 ABI",
             ABI_PPC32: "ppc32 ABI", ABI_PPC64: "ppc64 ABI"}
# syscalls2's ProfileType (syscalls2.h) of each ABI it pairs through the engine; 0, its i386
# profile for the vDSO's entries
PROFILE_ABI = {12: ABI_X86_64, 1: ABI_I386, 0: ABI_I386, 3: ABI_AARCH64, 2: ABI_ARM,
               14: ABI_RISCV64, 4: ABI_O32, 6: ABI_N64, 15: ABI_PPC32, 16: ABI_PPC64}
PROFILE_VDSO = 0
MMAP2 = 192
CHILD = 1 << 4                          # PANDA_SYSCALL_CHILD

# Workloads: name, the guest programs it needs ((name, build), put at /tmp/<name>), the shell
# command run inside the window. Each exits 0 but threads (3: its side thread's exit_group).
SHELL = [
    ("pipelines", [], "ls -la /bin | sort -k5 -n | tail -3 | md5sum; "
                      "find /proc/self/ -maxdepth 1 | xargs ls -ld | wc -l; grep -c processor /proc/cpuinfo"),
    ("fork/exec", [], "for i in $(seq 50); do /bin/true; sh -c 'echo x > /dev/null'; done"),
    # Many tasks blocked in calls at once, moved to other vCPUs while they are blocked: readers
    # block in read on a pipe for each line their writer echoes (busybox's read takes a byte at
    # a time), writers in nanosleep, and a loop keeps changing every shell's CPU affinity, so
    # that a blocked call returns on another vCPU than it entered (at --smp 4).
    ("blocking", [], "for j in $(seq 8); do (for i in $(seq 30); do echo $i; usleep 1000; done | "
                     "while read x; do :; done) & done; n=$(nproc); for k in $(seq 30); do "
                     "for p in $(pidof sh); do taskset -p $((1 << (k % n))) $p > /dev/null 2>&1; "
                     "done; usleep 500; done; wait"),
]
# The other arches: the programs built for the machine's agent arch (testmachine.agent_key)
OTHER = lambda b: [("return paths", [("scpaths", b)], "/tmp/scpaths")] + SHELL + [
    ("threads", [("scwork", b)], "/tmp/scwork threads"),
    ("signals", [("scwork", b)], "/tmp/scwork signals"),
]
WORKLOADS = {
    "x86_64": [
        ("x86_64 program", [("sctruth", "x86_64")], "/tmp/sctruth"),
        ("i386 program", [("sctruth", "i386")], "/tmp/sctruth"),
        ("return paths", [("scpaths", "x86_64")], "/tmp/scpaths"),
    ] + SHELL + [
        ("threads", [("scwork", "x86_64")], "/tmp/scwork threads"),
        ("signals", [("scwork", "x86_64")], "/tmp/scwork signals"),
    ],
    "aarch64": [
        ("return paths", [("scpaths", "aarch64")], "/tmp/scpaths"),
    ] + SHELL + [
        ("threads", [("scwork", "aarch64")], "/tmp/scwork threads"),
        ("signals", [("scwork", "aarch64")], "/tmp/scwork signals"),
        # 32-bit tasks: the compat ABI
        ("arm return paths", [("scpaths", "arm")], "/tmp/scpaths"),
        ("arm threads", [("scwork", "arm")], "/tmp/scwork threads"),
        ("arm signals", [("scwork", "arm")], "/tmp/scwork signals"),
    ],
}

# Not yet declared to the engine: GAP while they fail, XPASS (a failure) once fixed.
GAPS = {}
try:
    machine = Machine(opt.image, smp=opt.smp, intel=opt.intel)
except NotImplementedError as e:
    # no test machine agent for this arch (loongarch64 in tests/busybox.sh's set)
    print(f"syscall_truth: {e}")
    print("SYSCALL_TRUTH skipped")
    sys.exit(0)
arch = machine.arch
if arch == "i386":
    # a 32-bit kernel: int 0x80 and the vDSO's sysenter
    WORKLOADS[arch] = [("i386 program", [("sctruth", "i386")], "/tmp/sctruth")] + OTHER("i386")
elif arch not in WORKLOADS and arch in AGENT_ARCH:
    WORKLOADS[arch] = OTHER(agent_key(machine.image))
BYTE_ORDER = "big" if agent_key(machine.image) in ("mips", "mips64", "ppc", "ppc64") else "little"
C = Checks(arch, GAPS)
if arch not in WORKLOADS:
    print(f"syscall_truth: no workloads for {arch}")
    sys.exit(C.report("SYSCALL_TRUTH skipped"))
if arch.startswith("mips") and "task_key=false" in os.environ.get("SC2_ARGS", ""):
    # The call key's address-space part on MIPS is the ASID, which Linux gives a process anew when
    # it flushes its TLB entries (fork does, for copy-on-write): a fork's return is lost to it.
    print("syscall_truth: the call key does not pair a MIPS fork (its ASID changes)")
    sys.exit(C.report("SYSCALL_TRUTH skipped"))
EVICT = bool(os.environ.get("SC2_SETS_LOG"))
ON_ALL = bool(os.environ.get("SC2_ON_ALL"))
MISSING = [0]                           # EVICT: calls with no return
if EVICT:
    os.environ["PANDA_SYSCALL_PAIR_SETS_LOG"] = os.environ["SC2_SETS_LOG"]
panda = machine.panda()
agent = Agent(panda)

Event = namedtuple("Event", "kind abi cpu vdso id num args pc ret sc2arg6")


class Sc2Trace:
    """syscalls2's calls, entries and returns, while on."""

    def __init__(self):
        args = {}
        for kv in os.environ.get("SC2_ARGS", "").split(","):
            if "=" in kv:
                k, v = kv.split("=", 1)
                args[k] = v
        panda.load_plugin("syscalls2", args)
        from cffi import FFI
        ffi = FFI()
        ffi.cdef("bool syscalls2_current_call(void *cpu, uint64_t *id, uint32_t *flags);"
                 "typedef struct { uint64_t entries, kept, paired, restarts, children, ended,"
                 " elsewhere, misses, evicted, not_kept, resolves, call_key, pending; }"
                 " PandaSyscallStats;"
                 "void panda_syscall_stats(PandaSyscallStats *out);")
        # the plugin's handle also finds the engine's functions (its dependency)
        self.lib = ffi.dlopen(os.path.join(panda.get_plugin_path(),
                                           f"libpanda-syscalls2_{arch}-softmmu.so"))
        self.ffi = ffi
        self.on = False
        self.events = []
        self.children = 0
        self.tls = threading.local()        # cffi releases the GIL in the call: per-vCPU buffers

        if ON_ALL:
            @panda.ppp("syscalls2", "on_all_sys_enter", name="truth_enter")
            def enter(cpu, pc, no):
                if self.on:
                    self.record(ENTER, cpu, pc, None, no)

            @panda.ppp("syscalls2", "on_all_sys_return", name="truth_return")
            def ret(cpu, pc, no):
                if self.on:
                    self.record(RETURN, cpu, pc, None, no)
            return

        @panda.ppp("syscalls2", "on_all_sys_enter2", name="truth_enter")
        def enter(cpu, pc, call, ctx):
            if self.on:
                self.record(ENTER, cpu, pc, ctx)

        @panda.ppp("syscalls2", "on_all_sys_return2", name="truth_return")
        def ret(cpu, pc, call, ctx):
            if self.on:
                self.record(RETURN, cpu, pc, ctx)

    def on_all_abi(self, kind, cpu, pc, no):
        """SC2_ON_ALL: (ABI, vDSO entry) of a call, from the trap: at a return, @pc is where it
        returned, after the trap (x86: 2 bytes; a vDSO call: past the int $0x80 after it)"""
        env = panda.cpu_env(cpu)
        if arch in ("x86_64", "i386"):
            # int $0x80 (from any task, and after a vDSO entry, where its returns land)
            int80 = bytes(panda.virtual_memory_read(cpu, pc - 2 if kind == RETURN else pc, 2)) \
                == b"\xcd\x80"
            if int80:
                return ABI_I386, False
            if arch == "x86_64" and env.hflags & (1 << 15):     # HF_CS64: a 64-bit task
                return ABI_X86_64, False
            return ABI_I386, kind == ENTER                      # a vDSO entry
        if arch == "aarch64":
            return (ABI_AARCH64 if env.aarch64 else ABI_ARM), False
        if arch == "arm":
            return ABI_ARM, False
        if arch == "riscv64":
            return ABI_RISCV64, False
        if arch.startswith("mips"):
            return (ABI_O32 if 4000 <= no < 5000 else ABI_N64), False
        if arch.startswith("ppc"):
            return (ABI_PPC64 if arch.startswith("ppc64") and env.msr >> 63 else ABI_PPC32), False
        return None, False

    def regs(self, cpu, abi, pc, vdso):
        env = panda.cpu_env(cpu)
        if abi == ABI_X86_64:
            r = env.regs
            return [r[7], r[6], r[2], r[10], r[8], r[9]], r[0]
        if abi == ABI_I386:
            r = env.regs
            a = [r[3] & M32, r[1] & M32, r[2] & M32, r[6] & M32, r[7] & M32, r[5] & M32]
            if vdso:
                # where the kernel reads them: syscall (0f 05) loses ecx, so __kernel_vsyscall
                # passes argument 2 in ebp; argument 6 is ebp as it pushed it, at the user stack
                # pointer (sysenter: in ebp; syscall: esp)
                syscall = bytes(panda.virtual_memory_read(cpu, pc, 2)) == b"\x0f\x05"
                if syscall:
                    a[1] = r[5] & M32
                sp = (r[4] if syscall else r[5]) & M32
                a[5] = int.from_bytes(bytes(panda.virtual_memory_read(cpu, sp, 4)), "little")
            return a, r[0] & M32
        if abi == ABI_AARCH64:
            return [env.xregs[i] for i in range(6)], env.xregs[0]
        if abi == ABI_RISCV64:
            return [env.gpr[10 + i] for i in range(6)], env.gpr[10]
        if abi in (ABI_O32, ABI_N64):
            g, m = env.active_tc.gpr, mask_of(abi)
            a = [g[4 + i] & m for i in range(6)]
            if abi == ABI_O32:
                # arguments 5 and 6 on the user stack, 16 and 20 bytes above $sp
                for k, off in ((4, 16), (5, 20)):
                    try:
                        a[k] = int.from_bytes(bytes(panda.virtual_memory_read(cpu, (g[29] + off) & M32, 4)),
                                              BYTE_ORDER)
                    except ValueError:
                        # not in the TLB (MIPS reaches user memory through it only): unknown,
                        # as for syscalls2 and the engine, which read it the same way
                        a[k] = None
            ret = g[2] & m
            return a, (-ret & m) if g[7] & m else ret   # $a3: an error, $v0 its number
        if abi in (ABI_PPC32, ABI_PPC64):
            m = mask_of(abi)
            ret = env.gpr[3] & m
            return [env.gpr[3 + i] & m for i in range(6)], (-ret & m) if env.crf[0] & 1 else ret
        return [env.regs[i] & M32 for i in range(6)], env.regs[0] & M32

    def record(self, kind, cpu, pc, ctx, on_all_no=None):
        t = self.tls
        if not hasattr(t, "idp"):
            t.idp, t.flp = self.ffi.new("uint64_t *"), self.ffi.new("uint32_t *")
        if not self.lib.syscalls2_current_call(
                self.ffi.cast("void *", int(panda.ffi.cast("uintptr_t", cpu))), t.idp, t.flp):
            return
        if t.flp[0] & CHILD:
            self.children += 1
            return
        if ctx is None:
            # the callback's target_ulong number is syscall_ctx_t.no (an int) sign-extended
            bits = 32 if arch in ("i386", "arm", "mipsel", "mips", "ppc") else 64
            no = on_all_no - (1 << bits) if on_all_no >= 1 << (bits - 1) else on_all_no
            abi, vdso = self.on_all_abi(kind, cpu, pc, no)
            if abi is None:
                return
            arg6 = None
        else:
            abi = PROFILE_ABI.get(ctx.profile)
            if abi is None:
                return
            no = ctx.no
            vdso = arch in ("x86_64", "i386") and ctx.profile == PROFILE_VDSO
            # syscalls2's own decoding of mmap2's argument 6 (the one in ebp, or on the stack)
            arg6 = int.from_bytes(bytes(list(ctx.args[5])[:4]), "little") \
                if kind == ENTER and abi == ABI_I386 and ctx.no == MMAP2 else None
        args, ret = self.regs(cpu, abi, pc, vdso and kind == ENTER)
        self.events.append(Event(kind, abi, cpu.cpu_index, vdso, t.idp[0], no,
                                 args if kind == ENTER else [0] * 6, pc, ret if kind == RETURN else 0,
                                 arg6))

    def start(self):
        self.events = []
        self.on = True

    def stop(self):
        self.on = False
        return self.events, 0

    def stats(self):
        s = self.ffi.new("PandaSyscallStats *")
        self.lib.panda_syscall_stats(s)
        return {k: getattr(s, k) for k in ("entries", "kept", "paired", "restarts", "children",
                                           "ended", "elsewhere", "misses", "evicted", "not_kept",
                                           "resolves", "call_key", "pending")}


trace = Sc2Trace()

LINE = re.compile(r"^\s*(.*)-(\d+)\s+\[(\d+)\]\s+\S+\s+(\d+\.\d+): sys_(enter|exit): NR (-?\d+) "
                  r"(?:\(([0-9a-f, ]*)\)|= (-?\d+))$")


def program(name, build):
    return base64.b64decode(importlib.import_module(f"{name}_programs").PROGRAMS[build])


def mask_of(abi):
    return M32 if abi in (ABI_I386, ABI_ARM, ABI_O32, ABI_PPC32) else M64


def run(name, progs, cmd, ncpu):
    """Run cmd inside both windows; (ftrace events per CPU, syscalls2 events, lost, overruns, rc)."""
    for prog, build in progs:
        agent.put(f"/tmp/{prog}", program(prog, build))
    trace.start()
    progress(f"{name}: {cmd[:60]}")
    status, out = agent.run(f"echo > {T}/trace; echo 1 > {T}/tracing_on; {cmd}; rc=$?; "
                            f"echo 0 > {T}/tracing_on; echo rc=$rc")
    rc = out.decode(errors="replace").rsplit("rc=", 1)[-1].strip()
    trap, lost = trace.stop()
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
            evs.append(dict(comm=comm.strip(), pid=int(pid), cpu=VCPU[int(cpu)], ts=float(ts),
                            kind=kind, nr=int(nr), args=[int(a, 16) for a in args.split(", ")] if args else None,
                            ret=int(ret) if ret is not None else None, i=len(evs)))
        ftrace[VCPU[c]] = evs
    return ftrace, trap, lost, overrun, rc


def same_enter(f, t):
    """Is the kernel's enter f syscalls2's enter t?"""
    mask = mask_of(t.abi)
    return ((f["nr"] - t.num) & mask) == 0 and all(b is None or ((a - b) & mask) == 0
                                                   for a, b in zip(f["args"], t.args))


def align(F, Tc):
    """Walk the kernel's enters F against syscalls2's Tc from the first offset where they agree;
    ({index in F: index in Tc}, [differences])."""
    def agree(i, j, k=16):
        return all(same_enter(F[i + d], Tc[j + d]) for d in range(min(k, len(F) - i, len(Tc) - j)))

    start = next((j for j in range(len(Tc)) if same_enter(F[0], Tc[j]) and agree(0, j)), None)
    if start is None:
        return {}, [("no offset in syscalls2's enters matches the kernel's first ones",
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
            diffs.append((f"syscalls2 has {extra} enter(s) the kernel doesn't", ctx))
            j += extra
        elif missing is not None:
            diffs.append((f"syscalls2 is missing {missing} enter(s)", ctx))
            i += missing
        else:
            diffs.append(("different", ctx))
            i, j = i + 1, j + 1
    if i < len(F):
        diffs.append((f"syscalls2's enters end {len(F) - i} before the kernel's",
                      [fmt_f(x) for x in F[i:i + 3]]))
    return pairs, diffs


def fmt_f(f):
    if f["kind"] == "enter":
        return f"{f['comm']}-{f['pid']} enter {f['nr']} ({', '.join(hex(a) for a in f['args'])})"
    return f"{f['comm']}-{f['pid']} exit {f['nr']} = {f['ret']}"


def fmt_t(t):
    kind = {ENTER: "enter", RETURN: "return"}[t.kind]
    return (f"#{t.id} {kind} abi={t.abi} nr={t.num} "
            f"({', '.join('?' if a is None else hex(a) for a in t.args)}) pc={t.pc:#x}"
            + (f" = {t.ret}" if t.kind == RETURN else ""))


def check(name, ftrace, trap, lost, overrun):
    p = f"{name}: "
    C.check(p + "nothing was lost (ftrace overruns)",
            lost == 0 and not any(overrun.values()), dict(lost=lost, overrun=overrun))
    enters = defaultdict(list)                  # cpu: [enter]
    by_id = defaultdict(list)                   # id: [return]
    for e in trap:
        if e.kind == ENTER:
            enters[e.cpu].append(e)
        else:
            by_id[e.id].append(e)

    # Each task's events in time order.
    tasks = defaultdict(list)
    for c, evs in ftrace.items():
        for f in evs:
            tasks[f["pid"]].append(f)
    for evs in tasks.values():
        evs.sort(key=lambda f: (f["ts"], f["cpu"], f["i"]))
        if arch == "arm":
            # A 32-bit ARM kernel restarts a call that has no signal handler to run without
            # returning to user mode: its -ERESTART* exit is followed at once by the restart's
            # enter (restart_syscall, 0, for -ERESTART_RESTARTBLOCK), which no trap makes. The
            # task sees one call, which returns the restart's value.
            for k in range(len(evs) - 1):
                f, g = evs[k], evs[k + 1]
                if (f["kind"] == "exit" and f["ret"] in RESTART and g["kind"] == "enter" and
                        g["nr"] == (0 if f["ret"] == -516 else f["nr"])):
                    g["krestart"] = True

    # Enters, CPU by CPU.
    trap_of = {}                                # (cpu, ftrace index): enter
    n_enters, enter_diffs = 0, []
    for c, evs in ftrace.items():
        F = [f for f in evs if f["kind"] == "enter" and not f.get("krestart")]
        if not F:
            continue
        n_enters += len(F)
        pairs, diffs = align(F, enters.get(c, []))
        enter_diffs += [(c, *d) for d in diffs]
        for i, j in pairs.items():
            trap_of[(c, F[i]["i"])] = enters[c][j]
    C.check(p + "on every vCPU the kernel's enters are syscalls2's, in order",
            n_enters and not enter_diffs, enter_diffs[:10])
    if name == "i386 program":
        # 200 rounds of 6 calls and a fork through __kernel_vsyscall, under syscalls2's i386 profile
        vdso = [e for e in trap if e.kind == ENTER and e.vdso]
        C.check(p + "syscalls2 reports the calls made through the vDSO", len(vdso) >= 1200,
                len(vdso))
        bad6 = []
        for (c, i), t in trap_of.items():
            if t.sc2arg6 is not None:
                f = ftrace[c][i]
                if t.sc2arg6 != f["args"][5]:
                    bad6.append((fmt_f(f), t.sc2arg6))
        n6 = sum(1 for t in trap_of.values() if t.sc2arg6 is not None)
        if not ON_ALL:
            C.check(p + "mmap2's argument 6, as syscalls2 decodes it, is the kernel's",
                    n6 >= 400 and not bad6, dict(calls=n6, bad=bad6[:5]))

    # Returns, call by call.
    bad, counted = defaultdict(list), defaultdict(int)
    vdso_restarts = 0                           # vDSO calls the kernel ended with -ERESTART*
    restarted = 0                               # calls restarted: -ERESTART*, and no return
    for pid, evs in tasks.items():
        for k, f in enumerate(evs):
            if f["kind"] != "enter":
                continue
            t = trap_of.get((f["cpu"], f["i"]))
            if t is None:
                continue                        # an enter syscalls2 missed: counted above
            abi, mask = t.abi, mask_of(t.abi)
            nr = t.num & mask
            vfork = nr in VFORK[abi] or (nr == CLONE[abi] and t.args[0] & CLONE_VFORK)
            if vfork:
                group = "vfork"
            elif nr == CLONE3:
                group = "clone3"            # its flags (CLONE_VFORK or not) are in memory
            elif abi == ABI_I386 and name == "x86_64 program":
                group = "int 0x80 from a 64-bit task"
            elif t.vdso:
                group = "i386 vDSO"
            else:
                group = ABI_GROUP[abi]
            counted[group] += 1
            nxt = evs[k + 1] if k + 1 < len(evs) else None
            # A task's calls are serial: its next event is this call's exit. The exit's number can
            # differ: -1 for rt_sigreturn, the i386 one after an execve of a 32-bit program.
            exit_ = nxt if nxt and nxt["kind"] == "exit" else None
            j = k + 2
            while exit_ and exit_["ret"] in RESTART and j + 1 < len(evs) and \
                    evs[j].get("krestart") and evs[j + 1]["kind"] == "exit":
                exit_, j = evs[j + 1], j + 2            # restarted in the kernel: its value
            last = j >= len(evs)
            rets = by_id.get(t.id, [])
            got = [r.ret for r in rets]
            if exit_ is None:
                if nr in NO_EXIT[abi]:
                    ok, want = not rets, "no return (it ends the task)"
                elif nxt is None:
                    continue                    # the task's last call in the window
                else:
                    ok, want = False, f"the kernel has no exit before {fmt_f(nxt)}"
            elif nr in SIGRETURN[abi] or (nr in EXECVE[abi] and exit_["ret"] == 0):
                ok, want = not rets, "no return"
            elif nr in SC2_NORETURN[abi]:
                ok, want = not rets, "no return (syscalls2's tables: panda_noreturn)"
            elif exit_["ret"] in RESTART:
                vdso_restarts += t.vdso
                restarted += not rets and not last
                ok = len(rets) <= 1 and (not rets or ((rets[0].ret + EINTR) & mask) == 0)
                want = f"none (a restart) or -EINTR ({exit_['ret']})"
            elif not rets and last:
                continue                        # the task may have died before user mode (a group
                                                # exit), or not got there by the window's end
            elif not rets and EVICT:
                MISSING[0] += 1                 # evicted from a full set: no return, never a wrong one
                continue
            else:
                ok = len(rets) == 1 and ((rets[0].ret - exit_["ret"]) & mask) == 0
                want = exit_["ret"]
            if not ok:
                bad[group].append((fmt_f(f), dict(want=want, syscalls2=got)))
    # How spread the workload was: enters per vCPU, and calls that returned on another vCPU.
    cpu_of = {e.id: e.cpu for e in trap if e.kind == ENTER}
    moved = sum(1 for e in trap if e.kind == RETURN and cpu_of.get(e.id, e.cpu) != e.cpu)
    for group, n in counted.items():
        C.check(p + f"{group}: each call's return is the kernel's", not bad[group],
                f"{len(bad[group])} of {n} calls: {bad[group][:5]}")
    if name == "i386 program":
        # the reads ended by signals (EINTR), the wait4 restarted at the vDSO's int $0x80
        C.check(p + "signals interrupted calls made through the vDSO", vdso_restarts >= 11,
                vdso_restarts)
    return n_enters, dict(sorted((c, len(v)) for c, v in enters.items())), moved, restarted


R = {}
VCPU = {}                                       # the kernel's CPU number: its vCPU's index


@panda.queue_blocking
def drive():
    machine.revert(panda, agent)
    R["cpu"] = agent.run("grep -m1 vendor_id /proc/cpuinfo; uname -m")[1].decode()
    ncpu = int(agent.run("nproc")[1])
    R["ncpu"] = ncpu
    # Linux numbers its CPUs from the one it booted on. On riscv64 that is the hart OpenSBI's
    # boot-hart lottery picked, so CPU 0 can be any hart; a vCPU's index is its hart id.
    VCPU.update((c, c) for c in range(ncpu))
    if arch == "riscv64":
        info = agent.run("cat /proc/cpuinfo")[1].decode()
        VCPU.update((int(c), int(h)) for c, h in
                    re.findall(r"^processor\s*:\s*(\d+)\s*\nhart\s*:\s*(\d+)", info, re.M))
        R["harts"] = dict(VCPU)
    agent.run(f"echo 0 > {T}/tracing_on; echo {32768 // ncpu} > {T}/buffer_size_kb; "
              f"echo global > {T}/trace_clock; echo 8192 > {T}/saved_cmdlines_size; "
              f"echo 1 > {T}/events/raw_syscalls/enable")
    for name, progs, cmd in WORKLOADS[arch]:
        st0 = trace.stats()
        R[name] = run(name, progs, cmd, ncpu)
        st1 = trace.stats()
        R[name + " stats"] = {k: st1[k] - st0[k] for k in st1}
    R["stats"] = trace.stats()
    panda.end_analysis()


panda.run()

summary = []
for name, _, _ in WORKLOADS[arch]:
    if name not in R:
        C.check(f"{name}: ran", False, "the guest stopped before it")
        continue
    ftrace, trap, lost, overrun, rc = R[name]
    want = "3" if name.endswith("threads") else "0"
    C.check(f"{name}: the workload exited {want}", rc == want, rc)
    n, per_cpu, moved, restarted = check(name, ftrace, trap, lost, overrun)
    if not EVICT:
        # each call the kernel restarted (no return reported) was popped as a restart, not left
        # pending: at its restart address (the trap, or the vDSO's int $0x80 after it)
        d = R[name + " stats"]
        C.check(f"{name}: the engine saw each restart", d["restarts"] >= restarted,
                dict(restarted=restarted, engine=d["restarts"]))
    summary.append(f"{name.replace(' ', '_')}={n}")
    print(f"{name}: kernel enters {n}; syscalls2 enters per vCPU {per_cpu}; returns on another "
          f"vCPU {moved}")
print("guest:", " ".join(R.get("cpu", "?").split()), "vCPUs:", R.get("ncpu", "?"),
      *(["CPU: hart", R["harts"]] if "harts" in R else []))
print("child returns:", trace.children, "engine:", R.get("stats"))
if EVICT:
    st = R.get("stats", {})
    C.check("the engine evicted calls from its full sets", st.get("evicted", 0) > 0, st)
    # a full set evicts its oldest call, or keeps no new one while every other way is busy or
    # being delivered on another vCPU (not_kept)
    dropped = st.get("evicted", 0) + st.get("not_kept", 0)
    C.check("every call with no return was evicted or not kept (no return lost otherwise)",
            MISSING[0] <= dropped, dict(missing=MISSING[0], evicted=st.get("evicted"),
                                        not_kept=st.get("not_kept")))
sys.exit(C.report(f"SYSCALL_TRUTH arch={arch} smp={opt.smp} " + " ".join(summary)))
