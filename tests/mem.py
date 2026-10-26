"""Memory and instruction callbacks, register access and exceptions against a guest program.

Run: panda-python tests/mem.py <dir>/debian-13-x86_64.json

tests/programs/mem_<arch>.S, between its hypercalls 1 and 2, runs straight-line code that stores
1, 2, 4, 8 (and, on x86_64/aarch64, 16) bytes of known values and loads them back; each access is
a label the program passes in a table. The memory callbacks (virt/phys x before/after x
read/write) and insn_translate/insn_exec are registered disabled and enabled only in that window.
Checks, each against the program's own ground truth:
- every access in the window, in order: the accessing instruction's pc, the virtual address, the
  size, the value (stores: before and after; loads: after), the physical address (= virt_to_phys);
  before comes before after, and a before-write sees memory still holding the old value;
- nothing else in the program touches memory in the window, and nothing fires while disabled;
- insn_exec: each instruction between the window's first block and hypercall 2 runs exactly once,
  in address order, and those are the instructions insn_translate was asked about;
- at hypercall 2: arch.get_reg reads the loaded values, current_sp / get_reg(SP), and set_reg sets
  the register the program exits with (the shell sees that exit code);
- between hypercalls 3 and 4: before_handle_exception sees the page fault of the store to a fresh
  page, with the CPU's pc at the store;
- delete_callback: a deleted callback never fires again; is_callback_enabled follows
  enable/disable_callback;
- flush_tb: the program runs twice. The second run finds its blocks already translated (no
  before_block_translate for the window's first block), then flushes at hypercall 2: the blocks
  after it are translated again.
  A run that had to translate the window's first block again doesn't flush; the program runs once
  more (at most RUNS runs in all). On x86 this happens when an interrupt is delivered right at m0:
  QEMU returns from a hardware interrupt with EFLAGS.RF=1, RF is part of the TB key, and run 1's
  RF=0 block doesn't match (ubuntu-24.04 x86_64, HZ=1000: ~1 run in 4-9). After that run both keys
  are translated, so the next run finds one. The checks of the extra run are its own ("[run 3] ...").
cpu_restore_state is registered too: it has no call site yet (a gap).
"""
import os
import sys
from collections import Counter

from testlib import Checks, MAGIC, hypercall_args, in_program, progress, read_quads, upload
from pandare2 import Panda

LABELS = ["buf", "src16", "mw1", "mw2", "mw3", "mw4", "mr5", "mw5", "mr1", "mr2", "mr3", "mr4",
          "mpf", "fresh", "m0", "hc2"]
SRC16 = b"0123456789abcdef"
LOADED = {"x86_64": ["R11", "R12", "R13", "R14"], "aarch64": ["X21", "X22", "X23", "X24"],
          "riscv64": ["S2", "S3", "S4", "S5"]}
EXIT_REG = {"x86_64": "R15", "aarch64": "X25", "riscv64": "S6"}
SP_REG = {"x86_64": "RSP", "aarch64": "SP", "riscv64": "SP"}
PAGE_FAULT = {"x86_64": 14, "aarch64": 4, "riscv64": 15}   # EXCP0E_PAGE, EXCP_DATA_ABORT, store page fault
EXIT_CODE = 42
RUNS = 3    # run 1 translates; runs 2.. reuse; the first run that found m0 translated flushes

panda = Panda(generic=sys.argv[1])
arch = panda.arch_name
ffi = panda.ffi
C = Checks(arch, {"cpu_restore_state fires on the page fault":
                  "no call site for panda_callbacks_cpu_restore_state",
                  **{f"[run {n}] 16-byte accesses get before events":
                     "128-bit TCG accesses have no before events (patch 0047)" for n in range(1, RUNS + 1)}})


class Run:
    def __init__(self, n):
        self.n = n
        self.ev = []            # memory events in program code: (kind, pc, addr, size, value, mem_before)
        self.insns = []         # insn_exec pcs in program code
        self.translated = []    # insn_translate pcs in program code
        self.bbt = []           # before_block_translate pcs in program code
        self.exc = []           # (index, pc) between hypercalls 3 and 4
        self.restore = 0
        self.early = Counter()  # callbacks seen in the program before hypercall 1
        self.window = False
        self.labels = {}
        self.regs = None
        self.retry = False      # m0 translated again: no flush, the program runs once more


runs = []
cur = None
MEMCBS = ["vmbr", "vmbw", "vmar", "vmaw", "pmbr", "pmbw", "pmar", "pmaw", "it", "ie"]


def mem_event(cpu, kind, pc, addr, size, buf=None):
    r = cur
    if r is None or not in_program(pc):
        return
    if not r.window:
        r.early[kind] += 1
        return
    value = bytes(ffi.buffer(buf, size)) if buf is not None else None
    mem_before = None
    if kind == "vbw":
        mem_before = bytes(panda.virtual_memory_read(cpu, addr, size))
    r.ev.append((kind, pc, addr, size, value, mem_before))


@panda.cb_virt_mem_before_read(name="vmbr", enabled=False)
def vmbr(cpu, pc, addr, size):
    mem_event(cpu, "vbr", pc, addr, size)


@panda.cb_virt_mem_before_write(name="vmbw", enabled=False)
def vmbw(cpu, pc, addr, size, buf):
    mem_event(cpu, "vbw", pc, addr, size, buf)


@panda.cb_virt_mem_after_read(name="vmar", enabled=False)
def vmar(cpu, pc, addr, size, buf):
    mem_event(cpu, "var", pc, addr, size, buf)


@panda.cb_virt_mem_after_write(name="vmaw", enabled=False)
def vmaw(cpu, pc, addr, size, buf):
    mem_event(cpu, "vaw", pc, addr, size, buf)


@panda.cb_phys_mem_before_read(name="pmbr", enabled=False)
def pmbr(cpu, pc, addr, size):
    mem_event(cpu, "pbr", pc, addr, size)


@panda.cb_phys_mem_before_write(name="pmbw", enabled=False)
def pmbw(cpu, pc, addr, size, buf):
    mem_event(cpu, "pbw", pc, addr, size, buf)


@panda.cb_phys_mem_after_read(name="pmar", enabled=False)
def pmar(cpu, pc, addr, size, buf):
    mem_event(cpu, "par", pc, addr, size, buf)


@panda.cb_phys_mem_after_write(name="pmaw", enabled=False)
def pmaw(cpu, pc, addr, size, buf):
    mem_event(cpu, "paw", pc, addr, size, buf)


@panda.cb_insn_translate(name="it", enabled=False)
def it(cpu, pc):
    if in_program(pc) and cur is not None:
        cur.translated.append(pc)
        return True
    return False


@panda.cb_insn_exec(name="ie", enabled=False)
def ie(cpu, pc):
    if in_program(pc) and cur is not None:
        cur.insns.append(pc)
    return 0


@panda.cb_insn_exec(name="victim", enabled=False)
def victim(cpu, pc):
    if in_program(pc) and cur is not None:
        cur.early["deleted callback"] += 1
    return 0


@panda.cb_before_block_translate
def bbt(cpu, pc):
    if in_program(pc) and cur is not None:
        cur.bbt.append(pc)


@panda.cb_before_handle_exception(name="exc", enabled=False)
def exc(cpu, index):
    if cur is not None:
        cur.exc.append((index, panda.arch.get_pc(cpu)))
    return index


@panda.cb_cpu_restore_state(name="restore", enabled=False)
def restore(cpu, tb):
    if cur is not None:
        cur.restore += 1


enabled_states = []


@panda.hypercall(MAGIC)
def hypercall(cpu):
    r = cur
    op, table, _, pc = hypercall_args(panda, cpu)
    progress(f"run {r.n}: hypercall {op}")
    if op == 1:
        r.labels = dict(zip(LABELS, read_quads(panda, cpu, table, len(LABELS))))
        r.window = True
        if r.n == 1:
            panda.enable_callback("victim")
            panda.delete_callback("victim")
        for name in MEMCBS:
            panda.enable_callback(name)
        enabled_states.append(all(panda.is_callback_enabled(n) for n in MEMCBS))
    elif op == 2:
        for name in MEMCBS:
            panda.disable_callback(name)
        enabled_states.append(not any(panda.is_callback_enabled(n) for n in MEMCBS))
        r.window = False
        r.regs = {reg: panda.arch.get_reg(cpu, reg) for reg in LOADED[arch]}
        L = r.labels
        r.phys = {a: panda.virt_to_phys(cpu, a) for a in (L["buf"], L["buf"] + 2, L["buf"] + 4, L["buf"] + 8,
                                                         L["buf"] + 16, L["src16"])}
        r.sp = (panda.current_sp(cpu), panda.arch.get_reg(cpu, SP_REG[arch]))
        r.exit_reg_before = panda.arch.get_reg(cpu, EXIT_REG[arch])
        panda.arch.set_reg(cpu, EXIT_REG[arch], EXIT_CODE)
        if r.n >= 2:
            if L["m0"] in r.bbt and r.n < RUNS:
                r.retry = True
                progress(f"run {r.n}: m0 was translated again in this run (x86: an interrupt at m0, "
                         f"EFLAGS.RF=1 in the TB key); no flush, running the program again")
            else:
                r.bbt_at_flush = len(r.bbt)
                panda.flush_tb()
    elif op == 3:
        panda.enable_callback("exc")
        panda.enable_callback("restore")
    elif op == 4:
        panda.disable_callback("exc")
        panda.disable_callback("restore")


@panda.queue_blocking
def drive():
    global cur
    panda.revert_sync("root")
    upload(panda, "mem", arch)
    n = 0
    while n < 2 or runs[-1].retry:
        n += 1
        cur = Run(n)
        runs.append(cur)
        progress(f"run {n}: /tmp/mem")
        cur.output = panda.run_serial_cmd("/tmp/mem; echo rc=$?")
        cur = None
    panda.end_analysis()


panda.run()


def expected(L):
    """The program's accesses in order: (label, kind 'w'/'r', address, size, value)."""
    buf = L["buf"]
    acc = [("mw1", "w", buf, 1, b"\x11"), ("mw2", "w", buf + 2, 2, (0x2233).to_bytes(2, "little")),
           ("mw3", "w", buf + 4, 4, (0x44556677).to_bytes(4, "little")),
           ("mw4", "w", buf + 8, 8, (0x8899aabbccddeeff).to_bytes(8, "little"))]
    if L["mr5"]:
        acc += [("mr5", "r", L["src16"], 16, SRC16), ("mw5", "w", buf + 16, 16, SRC16)]
    acc += [("mr1", "r", buf, 1, b"\x11"), ("mr2", "r", buf + 2, 2, (0x2233).to_bytes(2, "little")),
            ("mr3", "r", buf + 4, 4, (0x44556677).to_bytes(4, "little")),
            ("mr4", "r", buf + 8, 8, (0x8899aabbccddeeff).to_bytes(8, "little"))]
    return acc


def analyse(r):
    p = f"[run {r.n}] "
    L = r.labels
    C.check(p + "the program ran its four hypercalls and exited with the code set_reg gave it",
            f"rc={EXIT_CODE}" in r.output, repr(r.output))
    C.check(p + "nothing fires in the program while the callbacks are disabled (incl. a deleted one)",
            not r.early, dict(r.early))
    # Only the program's own window: other code can sit at these addresses in another process that
    # an interrupt switches to (seen on aarch64: a process with code at 0x41fxxx).
    r.ev = [e for e in r.ev if L["m0"] <= e[1] <= L["hc2"]]
    if arch == "x86_64":
        # The hypercall (out 0x88) reads the TSS's I/O permission bitmap: two 2-byte kernel reads,
        # reported with the out's pc (the CPU's own accesses on behalf of the instruction).
        tss = [e for e in r.ev if e[1] == L["hc2"]]
        C.check(p + "x86 out: its implicit I/O-bitmap reads are reported (kernel address, 2 bytes)",
                tss and all(e[3] == 2 and (e[0][0] == "p" or e[2] >= 1 << 63) for e in tss), [(e[0], hex(e[2]), e[3]) for e in tss])
        r.ev = [e for e in r.ev if e[1] != L["hc2"]]
    by_pc = {}
    for e in r.ev:
        by_pc.setdefault(e[1], []).append(e)
    acc = expected(L)
    C.check(p + "memory events come only from the program's own accesses",
            sorted(by_pc) == sorted(L[a[0]] for a in acc),
            sorted(hex(pc) for pc in by_pc))
    seq = []    # accessing pcs in the order of their first event
    for e in r.ev:
        if not seq or seq[-1] != e[1]:
            seq.append(e[1])
    C.check(p + "accesses happen in program order, each once", seq == [L[a[0]] for a in acc],
            [hex(x) for x in seq])
    bad = []
    before16 = []
    for label, rw, addr, size, value in acc:
        evs = by_pc.get(L[label], [])
        kinds = [e[0] for e in evs]
        paddr = r.phys.get(addr)
        want = {"vb" + rw: (addr, size), "pb" + rw: (paddr, size), "va" + rw: (addr, size),
                "pa" + rw: (paddr, size)}
        if size == 16:
            # 128-bit accesses have no before events yet (patch 0047): a gap of its own.
            before16.append(sorted(k for k in kinds if k[1] == "b"))
            want = {k: v for k, v in want.items() if k[1] == "a"}
        if sorted(kinds) != sorted(want):
            bad.append(f"{label}: events {kinds}")
            continue
        if size != 16 and max(kinds.index("vb" + rw), kinds.index("pb" + rw)) > min(
                kinds.index("va" + rw), kinds.index("pa" + rw)):
            bad.append(f"{label}: an after event before a before event: {kinds}")
        for e in evs:
            if (e[2], e[3]) != want[e[0]]:
                bad.append(f"{label} {e[0]}: addr/size {e[2]:#x}/{e[3]} want {want[e[0]][0]:#x}/{size}")
            has_value = rw == "w" or e[0][1] == "a"
            if has_value and e[4] != value:
                bad.append(f"{label} {e[0]}: value {e[4]!r} want {value!r}")
            if e[0] == "vbw" and e[5] != bytes(size):
                bad.append(f"{label} vbw: memory already {e[5]!r} before the store")
    C.check(p + "each access: pc, virtual and physical address, size, value; before precedes after",
            not bad, bad[:6])
    if before16:
        C.check(p + "16-byte accesses get before events", all(b for b in before16), before16)
    m0, hc2 = L["m0"], L["hc2"]
    run_insns = [pc for pc in r.insns if m0 <= pc <= hc2]
    C.check(p + "insn_exec: every instruction from the window's first block to hypercall 2 runs once, in order",
            run_insns and run_insns == sorted(set(run_insns)) and run_insns[0] == m0 and run_insns[-1] == hc2
            and (arch == "x86_64" or len(run_insns) == (hc2 - m0) // 4 + 1),
            f"{len(run_insns)} insns {[hex(x) for x in run_insns[:4]]}..")
    if r.n == 1:    # run 2 reuses run 1's translations
        C.check(p + "insn_exec fires for exactly the instructions insn_translate was asked about",
                set(run_insns) == {pc for pc in r.translated if m0 <= pc <= hc2},
                f"exec {len(set(run_insns))} translate {len({pc for pc in r.translated if m0 <= pc <= hc2})}")
    C.check(p + "arch.get_reg reads the values the program loaded",
            r.regs and list(r.regs.values()) == [0x11, 0x2233, 0x44556677, 0x8899aabbccddeeff],
            {k: hex(v) for k, v in (r.regs or {}).items()})
    C.check(p + "current_sp is get_reg(SP)", r.sp[0] == r.sp[1] and r.sp[0] != 0, [hex(x) for x in r.sp])
    C.check(p + "the exit register held 0 before set_reg", r.exit_reg_before == 0, r.exit_reg_before)
    pf = [e for e in r.exc if e[1] == L["mpf"]]
    C.check(p + f"before_handle_exception: the page fault ({PAGE_FAULT[arch]}) at the store's pc",
            any(i == PAGE_FAULT[arch] for i, _ in pf), [(i, hex(pc)) for i, pc in r.exc[:6]])
    if r.n == 1:
        C.check("cpu_restore_state fires on the page fault", r.restore > 0, r.restore)
    return [(e[0], e[1], e[3], e[4]) for e in r.ev]


results = [analyse(r) for r in runs]
if os.environ.get("MEMDEBUG"):
    for r in runs:
        print(f"run {r.n} labels", {k: hex(v) for k, v in r.labels.items()})
        for e in r.ev:
            print("  ", e[0], hex(e[1]), hex(e[2]), e[3], e[4], e[5])
C.check("is_callback_enabled follows enable_callback/disable_callback", enabled_states and all(enabled_states),
        enabled_states)
if len(runs) >= 2:
    rf = runs[-1]   # the run that flushed
    L = rf.labels
    cached = L["m0"] not in rf.bbt[:getattr(rf, "bbt_at_flush", 0)]
    after = [pc for pc in rf.bbt[getattr(rf, "bbt_at_flush", 0):] if pc > L["hc2"]]
    C.check("the flushing run found the program's blocks still translated from the run before (precondition for flush_tb)",
            cached, f"run {rf.n} of {RUNS}: {[hex(x) for x in rf.bbt]}")
    C.check("flush_tb at hypercall 2: the blocks after it are translated again", after,
            [hex(x) for x in rf.bbt])
    C.check("every run sees the same memory events as run 1 (pcs, sizes, values)",
            all([(k, pc - runs[0].labels["m0"], s, v) for k, pc, s, v in results[0]]
                == [(k, pc - r.labels["m0"], s, v) for k, pc, s, v in res] for r, res in zip(runs[1:], results[1:])))
sys.exit(C.report(f"MEM arch={arch} events={[len(r.ev) for r in runs]} insns={[len(r.insns) for r in runs]}"))
