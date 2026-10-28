"""syscalls2 on the system calls Debian 13's glibc makes that its Linux tables lacked before they
were generated from Linux 6.19: clone3, rseq, close_range, faccessat2, fchmodat2. Also verifies
retired lookup_dcookie/nfsservctl (and x64 _sysctl) still have typed callbacks with ENOSYS returns.

Run: panda-python tests/syscalls.py <dir>/debian-13-x86_64.json   (x86_64, aarch64, riscv64)

tests/programs/syscalls_<arch>.S makes each call once with known arguments and exits 0 if each
returned what it should (see its header). For the program's calls (pc in the program), checks:
- on_sys_<name>_enter fires once with those arguments (a pointer argument: the guest memory it
  points at: clone3's struct clone_args, faccessat2's and fchmodat2's path);
- on_sys_<name>_return fires with the same arguments, and pandare2's syscall return value
  (arch.get_retval(convention="syscall")) is what the program checked; clone3 returns twice, in
  the vfork child (0) and in the parent (the child's pid): it is double-return
  (generated-in/context_target.json);
- on_unknown_sys_enter and on_unknown_sys_return never fire in the program.
On x86_64 the program loads xmm0 and xmm15 with known bytes before close_range: pandare2 reads them
back at that call's enter (CPUX86State.xmm_regs through cffi; tests/static/layout.py checks the
layout itself).
"""
import sys

from testlib import Checks, in_program, progress, upload
from pandare2 import Panda

AT_FDCWD, R_OK, AT_EACCESS, AT_SYMLINK_NOFOLLOW = -100, 4, 0x200, 0x100
RSEQ_SIG, ENOENT = 0x53053053, 2
CLONE_VM, CLONE_VFORK, SIGCHLD = 0x100, 0x4000, 17
MISSING = "/nonexistent-panda-syscalls"
XMM0 = int.from_bytes(bytes(range(16)), "little")
XMM15 = int.from_bytes(bytes(range(0xf0, 0x100)), "little")

panda = Panda(generic=sys.argv[1])
arch = panda.arch_name
C = Checks(arch)
S = {"enter": [], "return": [], "unknown": [], "xmm": None, "on": False}


def signed(v, bits=32):
    """An int argument, whether cffi passed it signed or as its unsigned bits."""
    return v - (1 << bits) if v >= 1 << (bits - 1) else v


def path(cpu, ptr):
    try:
        return panda.read_str(cpu, ptr)
    except ValueError:
        return None


def clone_args(cpu, ptr):
    """(flags, exit_signal, stack, stack_size) of the struct clone_args at ptr."""
    raw = bytes(panda.virtual_memory_read(cpu, ptr, 88))
    q = [int.from_bytes(raw[8 * i:8 * i + 8], "little") for i in range(11)]
    return q[0], q[4], q[5] != 0, q[6]


def described(name, cpu, args):
    """The arguments as the program passed them (pointers: what they point at)."""
    if name == "rseq":
        rseq, length, flags, sig = args
        return (rseq % 32 == 0, length, flags, sig)
    if name == "clone3":
        uargs, size = args
        return (clone_args(cpu, uargs), size)
    if name == "close_range":
        return tuple(args)
    if name in ("faccessat2", "fchmodat2"):
        dfd, filename, mode, flags = args
        return (signed(dfd), path(cpu, filename), mode, flags)
    if name in ("lookup_dcookie", "nfsservctl", "sysctl"):
        return tuple(args)
    raise KeyError(name)


EXPECT = {
    "rseq": (True, 32, 0, RSEQ_SIG),
    "clone3": (((CLONE_VM | CLONE_VFORK), SIGCHLD, True, 4096), 88),
    "close_range": (100, 200, 0),
    "faccessat2": (AT_FDCWD, "/", R_OK, AT_EACCESS),
    "fchmodat2": (AT_FDCWD, MISSING, 0o644, AT_SYMLINK_NOFOLLOW),
    "lookup_dcookie": (0x1122334455667788, 0x5678, 128),
    "nfsservctl": (9, 0x5678, 0x9abc),
}
if arch == "x86_64":
    EXPECT["sysctl"] = (0x1234,)


def hook(name):
    @panda.ppp("syscalls2", f"on_sys_{name}_enter", name=f"enter_{name}")
    def enter(cpu, pc, *args):
        if S["on"] and in_program(pc):
            S["enter"].append((name, described(name, cpu, args)))
            if name == "close_range" and arch == "x86_64":
                S["xmm"] = (panda.arch.get_reg(cpu, "XMM0"), panda.arch.get_reg(cpu, "XMM15"))

    @panda.ppp("syscalls2", f"on_sys_{name}_return", name=f"return_{name}")
    def ret(cpu, pc, *args):
        if S["on"] and in_program(pc):
            S["return"].append((name, described(name, cpu, args),
                                panda.arch.get_retval(cpu, convention="syscall")))


for n in EXPECT:
    hook(n)


@panda.ppp("syscalls2", "on_unknown_sys_enter")
def unknown_enter(cpu, pc, callno):
    if S["on"] and in_program(pc):
        S["unknown"].append(("enter", callno))


@panda.ppp("syscalls2", "on_unknown_sys_return")
def unknown_return(cpu, pc, callno):
    if S["on"] and in_program(pc):
        S["unknown"].append(("return", callno))


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    upload(panda, "syscalls", arch)
    S["on"] = True
    progress("running /tmp/syscalls")
    S["output"] = panda.run_serial_cmd("/tmp/syscalls; echo rc=$?")
    S["on"] = False
    panda.end_analysis()


panda.run()

C.check("the program saw each call return what it should (rc=0; rc=n: the n-th call didn't)",
        "rc=0" in S.get("output", ""), repr(S.get("output")))
for n, want in EXPECT.items():
    enters = [a for name, a in S["enter"] if name == n]
    C.check(f"on_sys_{n}_enter fires once, with the program's arguments", enters == [want], enters)
    rets = [(a, rv) for name, a, rv in S["return"] if name == n]
    if n == "clone3":
        C.check("on_sys_clone3_return fires in the vfork child (0) and the parent (its pid), with the "
                "program's arguments", sorted(rv > 0 for _, rv in rets) == [False, True] and
                any(rv == 0 for _, rv in rets) and all(a == want for a, _ in rets), rets)
    else:
        rv_want = (-38 if n in ("lookup_dcookie", "nfsservctl", "sysctl") else
                   -ENOENT if n == "fchmodat2" else 0)
        C.check(f"on_sys_{n}_return fires once, with the program's arguments and return value {rv_want}",
                rets == [(want, rv_want)], rets)
C.check("on_unknown_sys_enter/return never fire in the program", not S["unknown"], S["unknown"])
if arch == "x86_64":
    C.check("pandare2 reads xmm0 and xmm15 as the program loaded them", S["xmm"] == (XMM0, XMM15),
            [hex(x) for x in S["xmm"]] if S["xmm"] else None)
sys.exit(C.report(f"SYSCALLS arch={arch} enters={len(S['enter'])} returns={len(S['return'])}"))
