"""Analysis reads across privilege: kernel memory from user mode, user memory from the kernel, and
kernel memory from a thread that is not a vCPU while the vCPUs run.

Run: panda-python tests/kernel_read.py <dir>/busybox-<arch>.json [--smp N] [--intel]   (an image
from tests/mkbusybox.py; the machine is tests/testmachine.py's)

panda_virtual_memory_read translates as the CPU sees the address, else as the guest's kernel does
(cpu_translate_for_debug_kernel; on x86 under KPTI, the process's kernel table), without changing
the CPU's state. A kernel-mode block callback records a kernel text address (its pc, past the
entry code), the bytes there and the kernel stack pointer; a user-mode block callback records a
user text address and its bytes. Checks, sampled while the guest idles (in the agent's polling
loop) and then while it runs a command per vCPU (with several vCPUs, only the kernel address is
sampled):
- user mode reads the kernel text and a kernel stack (of a task of the same process): MIPS user
  mode takes an address error outside useg/xuseg, RISC-V U-mode can't use supervisor pages,
  PowerPC problem state can't read supervisor pages, LoongArch's direct-map windows are PLV0's,
  and with --intel x86's user CR3 (KPTI) maps none of the kernel but its text and entry code;
- the kernel reads the user text of the process it runs for (same address space identifier):
  RISC-V S-mode can't read user pages without SUM;
- another thread (the test's driver: not a vCPU, without the BQL) reads the kernel text while the
  vCPUs run, and the guest's workload runs correctly (a read changes no CPU state);
- every text read gives the bytes the first one saw (the kernel's view adds translations, it doesn't
  move them).
"""
import argparse
import sys
import threading
import time

from testlib import Checks, progress
from testmachine import Agent, Machine

PANDA_TEST = dict(area="mem", tier="guest", fixture="testmachine",
                  arches=["x86_64", "aarch64", "arm", "i386", "riscv64", "mipsel", "mips", "mips64el",
                          "mips64", "ppc", "ppc64"],
                  variants=dict(smp=[1, 4], intel=[False, True]), timeout=600)

N = 16          # bytes compared
SAMPLES = 100   # reads per direction

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
ap.add_argument("--intel", action="store_true", help="x86_64: an Intel model, which Linux runs with KPTI")
opt = ap.parse_args()

m = Machine(opt.image, smp=opt.smp, intel=opt.intel)
panda = m.panda()
arch = m.arch
agent = Agent(panda)
lock = threading.Lock()
seen = {"on": False, "kblocks": 0, "kern": None, "kstack": {}, "user": {}, "cpu": None,
        "u2k": [0, 0, 0], "u2kd": [0, 0, 0], "k2u": [0, 0, 0], "bg": [0, 0, 0]}   # [ok, failed, wrong bytes]


# The stack pointer's name (PowerPC's get_reg takes names only).
SP = next(name for name, idx in panda.arch.registers.items() if idx == panda.arch.reg_sp)


def read(cpu, addr):
    try:
        return bytes(panda.virtual_memory_read(cpu, addr, N))
    except ValueError:
        return None


def tally(key, got, want):
    with lock:
        seen[key][0 if got == want else 1 if got is None else 2] += 1


@panda.cb_before_block_exec(enabled=False)   # from after the revert: it pauses the vCPUs
def block(cpu, tb):
    if not seen["on"]:
        return
    pc = panda.arch.get_pc(cpu)
    if panda.in_kernel_mode(cpu):
        k = seen["kern"]
        if k is None:
            # Not the first: the entry code is mapped in the user tables too (KPTI).
            seen["kblocks"] += 1
            if seen["kblocks"] < 1000:
                return
            b = read(cpu, pc)
            if b is not None:
                seen["kern"] = (pc, b)
                seen["cpu"] = cpu
            return
        # Kernel data too: x86 KPTI without PCID maps the kernel text in the user tables
        # (global), not the rest. A kernel stack per process, which lives as long as the process
        # does (its contents change, so only the read is checked).
        asid = panda.current_asid(cpu)
        if asid not in seen["kstack"]:
            seen["kstack"][asid] = panda.arch.get_reg(cpu, SP)
        u = seen["user"].get(asid)
        if u is not None and sum(seen["k2u"]) < SAMPLES:
            tally("k2u", read(cpu, u[0]), u[1])
    else:
        asid = panda.current_asid(cpu)
        if asid not in seen["user"]:
            b = read(cpu, pc)
            if b is not None:
                seen["user"][asid] = (pc, b)
        k = seen["kern"]
        if k is not None and sum(seen["u2k"]) < SAMPLES:
            tally("u2k", read(cpu, k[0]), k[1])
        ks = seen["kstack"].get(asid)
        if ks is not None and sum(seen["u2kd"]) < SAMPLES:
            tally("u2kd", None if read(cpu, ks) is None else b"", b"")


def background(stop):
    while not stop.is_set():
        k, cpu = seen["kern"], seen["cpu"]
        if k is not None:
            tally("bg", read(cpu, k[0]), k[1])
        time.sleep(0.005)


out = {}


def sampled():
    if opt.smp > 1:
        # A Python callback on every block serializes the vCPUs on the GIL, so much that the
        # guest hardly runs: with several, only the kernel address is taken (the translations
        # don't depend on the vCPU count; the reads from another thread do, and are checked).
        return seen["kern"] is not None
    return all(sum(seen[k]) >= SAMPLES for k in ("u2k", "u2kd", "k2u"))


@panda.queue_blocking
def drive():
    stop = threading.Event()
    try:
        m.revert(panda, agent)
        # The block callback (in Python, on every block) only until the samples are in, while
        # the guest idles in the agent's polling loop (a command in flight would time out: with
        # several vCPUs the callback serializes them on the GIL). The revert pauses the vCPUs,
        # so the callback is turned on after it, and off from this thread.
        seen["on"] = True
        panda.enable_callback("block")
        t0 = time.time()
        while not sampled() and time.time() - t0 < 120:
            time.sleep(0.5)
        seen["on"] = False
        panda.disable_callback("block")
        # One worker per vCPU, while another thread reads kernel memory: each checks what it
        # writes and reads back, so a CPU whose state a read changed would show as BAD or a crash.
        threading.Thread(target=background, args=(stop,), daemon=True).start()
        n = max(opt.smp, 1)
        rc, o = agent.run(f"for c in $(seq 1 {n}); do (i=0; while [ $i -lt 2000 ]; do "
                          "echo $i > /tmp/w$c; read v < /tmp/w$c; [ $v = $i ] || echo BAD; "
                          "i=$((i+1)); done; echo ok$c) & done; wait")
        out["work"] = o.decode(errors="replace")
    except Exception as e:
        out["error"] = repr(e)
    seen["on"] = False
    stop.set()
    panda.end_analysis()


panda.run()

progress(f"{arch} smp{opt.smp}: user->kernel {seen['u2k']}, user->kernel stack {seen['u2kd']}, "
         f"kernel->user {seen['k2u']}, "
         f"background {seen['bg']} [ok, failed, wrong]")
C = Checks(arch)
work = out.get("work", "")
n = max(opt.smp, 1)
C.check("the guest ran the workload on every vCPU, correctly",
        "error" not in out and "BAD" not in work and all(f"ok{c}" in work for c in range(1, n + 1)),
        out.get("error") or work[-300:])
C.check("a kernel and a user address were recorded", seen["kern"] and (seen["user"] or opt.smp > 1),
        (seen["kern"], len(seen["user"])))
checks = [("user mode reads kernel text", "u2k"), ("user mode reads a kernel stack", "u2kd"),
          ("the kernel reads user text", "k2u")] if opt.smp == 1 else []
for name, key in checks + [("another thread reads kernel text while the vCPUs run", "bg")]:
    ok, failed, wrong = seen[key]
    C.check(name, ok and not failed and not wrong, seen[key])
sys.exit(C.report(f"kernel_read {arch} smp{opt.smp}"))
