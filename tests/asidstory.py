"""asidstory's behaviour as checks: a process timeline from asid_changed, named by OSI.

Run: panda-python tests/asidstory.py <dir>/debian-13-x86_64.json

Three copies of tests/programs/procs_<arch>.S run at once. Each waits at a start barrier the shell
releases once all three have exec'd (so they overlap however slow exec is), then makes M=20
hypercalls (MAGIC, 5, its pid from getpid, i, pc) with a 10 ms sleep between them. asidstory kept, per
address space, when it was seen and which process OSI said it was; here asid_changed builds the
timeline and each hypercall says which process is running. Checks:
- each copy's hypercalls arrive in order (i = 0..M-1), from its own pid;
- the address space (the page-table base in the last asid_changed value: CR3, TTBR0, satp's PPN)
  is the same at all of one process's hypercalls and differs between the three;
- OSI (osi_isf) names the current process "procs" with the pid the program got from getpid, and
  its asid (the pgd's physical address) is that page-table base;
- the copies ran at the same time: their hypercall spans overlap in the timeline;
- every one of the copies' address spaces appears in the asid_changed timeline;
- pandare2's procname filter: a before_block_exec registered with procname="procs" runs in the
  program's code and never in another process's user code.
Not checked: current_asid itself (its meaning changes with tbpc; tighten then: it should identify
the process the same way at every hypercall).
"""
import json
import os
import sys
from collections import defaultdict

from testlib import Checks, MAGIC, hypercall_args, in_program, progress, upload
from pandare2 import Panda

M, KIDS = 20, 3

# Guest time from instructions. With time following the host, the hypercall handler's host time
# (OSI) and the per-block procname callback pass as guest time: a copy's 20 sleeps were over before
# the next copy's exec was (aarch64 under load: the copies ran one after another). Under icount
# both the exec and the sleeps are counted in instructions, so their overlap doesn't depend on the
# host; 10 ms sleeps (not 2) leave the overlap a wide margin.
panda = Panda(generic=sys.argv[1], extra_args=["-icount", "shift=6,sleep=off"])
arch = panda.arch_name
with open(sys.argv[1]) as f:
    isf = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), json.load(f)["isf"])
panda.load_plugin("osi", {"disable-autoload": "true"})
panda.load_plugin("osi_isf", {"isf": isf})
C = Checks(arch)


def pgd(value):
    """The page-table base in an asid_changed value: since tbpc the identifier is that base on
    x86_64 (CR3 51:12, PTI's user-half bit dropped), aarch64 (TTBR0 base) and riscv64 (satp.PPN << 12)."""
    return value


S = {"timeline": [], "hcs": [], "on": False, "filtered_prog": 0, "filtered_other_user": []}


@panda.cb_asid_changed
def asid_changed(cpu, old, new):
    if S["on"]:
        S["timeline"].append(pgd(new))
    return False


# pandare2's procname filter: enabled only while OSI says "procs" runs.
@panda.cb_before_block_exec(procname="procs")
def procs_only(cpu, tb):
    pc = panda.arch.get_pc(cpu)
    if in_program(pc):
        S["filtered_prog"] += 1
    elif not panda.in_kernel_mode(cpu):
        S["filtered_other_user"].append(pc)


@panda.hypercall(MAGIC)
def hypercall(cpu):
    op, pid, i, _ = hypercall_args(panda, cpu)
    if op != 5:
        return
    proc = panda.plugins["osi"].get_current_process(cpu)
    name = panda.ffi.string(proc.name).decode() if proc != panda.ffi.NULL and proc.name != panda.ffi.NULL else None
    osi = (name, proc.pid, proc.asid) if proc != panda.ffi.NULL else None
    cur = S["timeline"][-1] if S["timeline"] else None
    S["hcs"].append((pid, i, cur, len(S["timeline"]), osi))
    if i == 0 or i == M - 1:
        progress(f"pid {pid} hypercall {i}")


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    upload(panda, "procs", arch)
    S["on"] = True
    progress(f"running {KIDS} copies of /tmp/procs")
    # Start barrier: each copy writes a byte to the "ready" FIFO (fd 4) and then reads one from the
    # "go" FIFO (fd 3); the shell, holding both open read-write (so neither open blocks nor sees
    # EOF), releases them with KIDS bytes once it has read KIDS ready bytes. All copies have exec'd
    # and sit in that read together, so their loops overlap however long exec takes.
    start = " ".join(f"/tmp/procs & p{k}=$!;" for k in range(KIDS))
    waits = " ".join(f"wait $p{k}; r{k}=$?;" for k in range(KIDS))
    rcs = " ".join(f"$r{k}" for k in range(KIDS))
    panda.run_serial_cmd("rm -f /tmp/go /tmp/ready; mkfifo /tmp/go /tmp/ready; exec 3<>/tmp/go 4<>/tmp/ready")
    S["output"] = panda.run_serial_cmd(f"{start} head -c {KIDS} <&4 >/dev/null; printf %{KIDS}s >&3; {waits} "
                                       f"echo rc={rcs}")
    panda.run_serial_cmd("exec 3>&- 4>&-; rm -f /tmp/go /tmp/ready")
    S["on"] = False
    panda.end_analysis()


panda.run()

hcs = S["hcs"]
by_pid = defaultdict(list)
for h in hcs:
    by_pid[h[0]].append(h)
C.check(f"{KIDS} processes made {M} hypercalls each", len(by_pid) == KIDS and all(len(v) == M for v in by_pid.values()),
        {p: len(v) for p, v in by_pid.items()})
C.check("each process's hypercalls arrive in order", all([h[1] for h in v] == list(range(len(v))) for v in by_pid.values()))
asids = {p: {h[2] for h in v} for p, v in by_pid.items()}
C.check("one address space per process at all its hypercalls (last asid_changed's page-table base)",
        all(len(a) == 1 and None not in a for a in asids.values()),
        {p: [hex(x) for x in a if x is not None] for p, a in asids.items()})
flat = [next(iter(a)) for a in asids.values() if len(a) == 1]
C.check("the processes' address spaces differ", len(set(flat)) == len(by_pid), [hex(x) for x in flat if x])
bad_osi = [(h[0], h[4]) for h in hcs if h[4] is None or h[4][0] != "procs" or h[4][1] != h[0]]
C.check("OSI names the current process 'procs' with the pid getpid gave", not bad_osi, bad_osi[:4])
bad_pgd = [(h[0], hex(h[2] or 0), hex(h[4][2])) for h in hcs if h[4] is not None and h[4][2] != h[2]]
C.check("OSI's process asid is the page-table base from asid_changed", not bad_pgd, bad_pgd[:4])
spans = sorted((v[0][3], v[-1][3]) for v in by_pid.values())
C.check("processes ran at the same time (two copies' hypercall spans in the timeline overlap)",
        len(spans) == KIDS and any(spans[k + 1][0] < spans[k][1] for k in range(len(spans) - 1)), spans)
seen = set(S["timeline"])
C.check("asid_changed's timeline switches to every process's address space", all(a in seen for a in flat),
        f"{len(S['timeline'])} switches")
switches = sum(1 for a, b in zip(hcs, hcs[1:]) if a[0] != b[0])
C.check("the hypercalls interleave across processes", switches >= KIDS, switches)
C.check("procname filter: a callback with procname='procs' runs in the program's code",
        S["filtered_prog"] >= M * KIDS, S["filtered_prog"])
C.check("procname filter: ... and never in another process's user code", not S["filtered_other_user"],
        [hex(x) for x in S["filtered_other_user"][:6]])
C.check("the shell saw all copies exit 0 (2: the start barrier's read failed)",
        "rc=" + " ".join(["0"] * KIDS) in S.get("output", ""), repr(S.get("output")))
sys.exit(C.report(f"ASIDSTORY arch={arch} pids={sorted(by_pid)} asid_changed={len(S['timeline'])} "
                  f"interleavings={switches}"))
