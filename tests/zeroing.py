"""Memory callbacks see the stores of a helper that fills memory: AArch64 DC ZVA.

Run: panda-python tests/zeroing.py <busybox-aarch64.json>
(an image from tests/mkbusybox.py; the machine is tests/testmachine.py's)

DC ZVA zeroes a whole block (DCZID_EL0's size, 64 bytes on QEMU's CPUs) from a helper that
writes through a host pointer, not through the store helpers; the same goes for the FEAT_MOPS
SET* instructions, PowerPC dcbz and RISC-V cbo.zero. Memory callbacks used to see none of those
stores: an aarch64 kernel's memset zeroes large ranges with DC ZVA, so a store trace of it missed
them. tests/programs/zeroing_aarch64.S fills a buffer with 0xa5 and, between hypercalls 1 and 2,
runs one DC ZVA at an address inside the buffer's second 256-byte block. The memory callbacks
(virt/phys x before/after write) are registered disabled and enabled only in that window.
Checks, against the program's own ground truth:
- DCZID_EL0 permits DC ZVA at EL0 (the precondition), and the block it names is zeroed in guest
  memory while the rest of the buffer still holds 0xa5;
- the window's only stores are DC ZVA's, at its pc;
- after-write events (virtual and physical) cover the aligned block exactly once, in address
  order, in naturally aligned pieces of at most 8 bytes, each with the value 0;
- each piece's before-write events precede its after-write events, and a virtual before-write
  sees the piece still holding 0xa5;
- physical addresses are the virtual ones translated (virt_to_phys at hypercall 2).
"""
import base64
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs"))
from testlib import Checks, MAGIC, hypercall_args, in_program, progress, read_quads
from zeroing_programs import PROGRAMS

PANDA_TEST = dict(area="engine", tier="guest", fixture="testmachine", arches=["aarch64"], timeout=600)

LABELS = ["zbuf", "m0", "mz", "hc2"]
FILL = 0xa5
BUF = 1024
TARGET = 264        # the program's DC ZVA operand, from zbuf

from testmachine import Agent, Machine  # noqa: E402

m = Machine(sys.argv[1])
arch = m.arch
if arch != "aarch64":
    print(f"SKIP zeroing: DC ZVA is aarch64's (arch {arch})")
    sys.exit(0)
panda = m.panda()
agent = Agent(panda)
ffi = panda.ffi
C = Checks(arch)

st = {"window": False, "ev": [], "early": 0, "labels": {}}


def mem_event(cpu, kind, pc, addr, size, buf):
    if not in_program(pc):
        return
    if not st["window"]:
        st["early"] += 1
        return
    before = bytes(panda.virtual_memory_read(cpu, addr, size)) if kind == "vbw" else None
    st["ev"].append((kind, pc, addr, size, bytes(ffi.buffer(buf, size)), before))


@panda.cb_virt_mem_before_write(name="vmbw", enabled=False)
def vmbw(cpu, pc, addr, size, buf):
    mem_event(cpu, "vbw", pc, addr, size, buf)


@panda.cb_virt_mem_after_write(name="vmaw", enabled=False)
def vmaw(cpu, pc, addr, size, buf):
    mem_event(cpu, "vaw", pc, addr, size, buf)


@panda.cb_phys_mem_before_write(name="pmbw", enabled=False)
def pmbw(cpu, pc, addr, size, buf):
    mem_event(cpu, "pbw", pc, addr, size, buf)


@panda.cb_phys_mem_after_write(name="pmaw", enabled=False)
def pmaw(cpu, pc, addr, size, buf):
    mem_event(cpu, "paw", pc, addr, size, buf)


MEMCBS = ["vmbw", "vmaw", "pmbw", "pmaw"]


@panda.hypercall(MAGIC)
def hypercall(cpu):
    op, table, _, pc = hypercall_args(panda, cpu)
    progress(f"hypercall {op}")
    if op == 1:
        st["labels"] = dict(zip(LABELS, read_quads(panda, cpu, table, len(LABELS))))
        st["dczid"] = panda.arch.get_reg(cpu, "X26")
        st["window"] = True
        for name in MEMCBS:
            panda.enable_callback(name)
    elif op == 2:
        for name in MEMCBS:
            panda.disable_callback(name)
        st["window"] = False
        zbuf = st["labels"]["zbuf"]
        st["mem"] = bytes(panda.virtual_memory_read(cpu, zbuf, BUF))
        st["phys"] = {a: panda.virt_to_phys(cpu, a) for a in range(zbuf, zbuf + BUF, 8)}


@panda.queue_blocking
def drive():
    try:
        m.revert(panda, agent)
        agent.put("/tmp/zeroing", base64.b64decode(PROGRAMS[arch]))
        st["output"] = agent.run("/tmp/zeroing; echo rc=$?", timeout=120)[1].decode(errors="replace")
    except Exception as e:
        st["output"] = repr(e)
    panda.end_analysis()


panda.run()

L = st["labels"]
C.check("the program ran both hypercalls and exited 0", "rc=0" in st.get("output", ""), repr(st.get("output")))
C.check("nothing fires in the program while the callbacks are disabled", st["early"] == 0, st["early"])
dczid = st.get("dczid", 1 << 4)
block = 4 << (dczid & 0xf)
C.check("DCZID_EL0 permits DC ZVA at EL0", not dczid & (1 << 4), hex(dczid))
zbuf = L.get("zbuf", 0)
lo = (zbuf + TARGET) & ~(block - 1)
hi = lo + block
want_mem = bytes(FILL if not lo <= zbuf + i < hi else 0 for i in range(BUF))
C.check(f"guest memory: the {block}-byte block at the operand is zero, the rest of the buffer 0xa5",
        st.get("mem") == want_mem, f"block {lo - zbuf:#x}..{hi - zbuf:#x}")
# Only the program's own window: busybox's static text sits near the programs' addresses, and an
# interrupt can switch to another process.
ev = [e for e in st["ev"] if L["m0"] <= e[1] <= L["hc2"]]
C.check("the window's stores are all DC ZVA's", ev and all(e[1] == L["mz"] for e in ev),
        sorted({hex(e[1]) for e in ev}))
bad = []
for kind in ("vaw", "paw", "vbw", "pbw"):
    pieces = [e for e in ev if e[0] == kind]
    addr = lo
    for e in pieces:
        vaddr = e[2] if kind[0] == "v" else None
        if kind[0] == "p":
            # the piece's physical address: map it back through the page it is on
            off = (addr - zbuf) & ~7
            want_p = st["phys"].get(zbuf + off, -1) + ((addr - zbuf) & 7)
            if e[2] != want_p:
                bad.append(f"{kind} at {addr:#x}: phys {e[2]:#x} want {want_p:#x}")
        elif vaddr != addr:
            bad.append(f"{kind}: address {vaddr:#x} want {addr:#x} (out of order, a gap or a repeat)")
            break
        if e[3] not in (1, 2, 4, 8) or addr % e[3]:
            bad.append(f"{kind} at {addr:#x}: size {e[3]} not a naturally aligned piece of at most 8")
        if e[4] != bytes(e[3]):
            bad.append(f"{kind} at {addr:#x}: value {e[4].hex()} not zero")
        if kind == "vbw" and e[5] != bytes([FILL]) * e[3]:
            bad.append(f"vbw at {addr:#x}: memory already {e[5].hex()} before the store")
        addr += e[3]
    if addr != hi:
        bad.append(f"{kind}: {len(pieces)} events cover {lo:#x}..{addr:#x}, want ..{hi:#x}")
C.check("before and after write events (virtual and physical) cover the block exactly, aligned pieces of value 0",
        not bad, bad[:6])
# per piece: its before events come before its after events
pos = {}
for i, e in enumerate(ev):
    pos.setdefault((e[0], e[2]), i)
mis = [k for k in pos if k[0] in ("vbw", "pbw") and
       pos[k] > min(pos.get(("vaw", k[1]) if k[0] == "vbw" else ("paw", k[1]), 1 << 30),
                    1 << 30)]
C.check("each piece's before-write events precede its after-write events", not mis, mis[:4])
sys.exit(C.report(f"ZEROING arch={arch} block={block} events={len(ev)}"))
