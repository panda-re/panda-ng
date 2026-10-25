"""Block-exec callbacks against ground truth, with TB chaining on and off.

Run: panda-python tests/blocks.py <dir>/debian-13-x86_64.json

tests/programs/blocks_<arch>.S makes hypercall 1 (passing a table of its blocks' addresses), runs a
loop of N iterations (blocks "loop" N times, "odd" and "even" N/2 times each), a block that faults
on its third instruction (the first write to a .bss page), a block that ends in a system call, then
hypercall 2, and exits. The block-exec callbacks are registered from the start, disabled, and
enabled only between the hypercalls (no TB flush: enabling opens the bridge's gate). The program
runs twice: with TB chaining on (the default) and with it off. Each time:
- before_block_exec / after_block_exec alternate on every block (the kernel's too), each pair on
  the same TB; start_block_exec / end_block_exec alike, and on the same blocks;
- per-instruction ground truth (insn_exec on every program instruction): a block that ran to its
  end (exit code TB_EXIT_IDX0) executed exactly tb->icount instructions, starting at its own pc;
  one that left early (exit code > TB_EXIT_IDX1) fewer or as many;
- the loop's instructions run exactly N, N/2, N times, and its blocks start N, N/2, N/2 times
  (plus one "even" start for each "odd" block icount split);
- the faulting block exits early once, at the store, which then starts a block of its own;
- the system call block: x86_64's syscall is not an exception in QEMU (exit code 0), aarch64's svc
  and riscv64's ecall are (it ran all its instructions, then left by longjmp: exit code 3);
- before_block_exec_invalidate_opt asks at every start of "loop" but the one right after it
  returned true, which is a fresh translation of it (asked too if an interrupt came between);
and both runs execute the same instructions the same number of times. Every block's CPU pc at
before_block_exec is its tb->pc (non-PCREL blocks), and after_block_exec of a program block that ran to its end sees
the pc of the program's next block.

A third run ("hooked page") puts a TCG hook on the program's first page (on its ELF header, never
executed): the engine then translates that page without CF_PCREL, so the program's blocks chain
without storing the pc, as on mips and ppc (tests/blocks_bare.py), and the same checks apply.

Another run ("toggle", before the hooked one) has only before/after_block_exec (and the insn callbacks) on. At the 10th
start of "odd", before_block_exec disables both: no block-exec callback is enabled. insn_exec
re-enables them at "loop"'s first instruction, which "odd" chains into. "odd"'s after_block_exec,
disabled when it ended, must not run late, at the start the gate reopened for.

Guest time comes from -icount, so the slow per-block Python callbacks can't starve the program
under timer ticks.
"""
import base64
import cffi
import json
import os
import sys
from collections import Counter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs"))
from blocks_programs import PROGRAMS  # noqa: E402

from pandare2 import Panda  # noqa: E402

MAGIC = 0x50616e64
N = 1000
PROGRAM_BASE, PROGRAM_END = 0x400000, 0x402000   # the programs' text is < 4 KiB; other processes run code at 0x41f000 (debian-13 aarch64)
LABELS = ["setup", "loop", "odd", "even", "fault", "fault_store", "sys"]
TB_EXIT_IDX1 = 1
CF_PCREL = 0x00020000
TOGGLE_AT = 10

# Guest time from instructions (-icount), not the host clock. Every block in the window, the
# kernel's too, runs Python callbacks; with time following the host, timer ticks then come every
# ~1000 blocks, the tick and the scheduling it triggers take about that long, and the program can
# starve for minutes (seen on debian-13 x86_64 and riscv64: run_serial_cmd timed out). icount may
# split a block when its budget runs out; the checks below allow for that. On every arch: an aarch64
# root snapshot taken without icount loads with it since engine patch 0063 (PMCEID0).
panda = Panda(generic=sys.argv[1], extra_args=["-icount", "shift=6,sleep=off"])
arch = panda.arch_name
ffi = panda.ffi
checks = {}


def check(name, ok, detail=""):
    checks[name] = (bool(ok), detail)


def address_space(cpu):
    """The running process's page-table root. (panda_current_asid is the hardware ASID on aarch64,
    which Linux keeps in TTBR1: 0 for every process. The tbpc lane changes it to the page-table
    address; until then, read TTBR0 here.)"""
    if arch == "aarch64":
        return panda.arch.cpu_env(cpu).cp15.ttbr0_el[1] & ((1 << 48) - 1)
    return panda.current_asid(cpu)


def in_program(pc, cpu=None):
    """In the program's range, and (given the CPU) in its address space: another process may run
    code at those addresses (seen on aarch64: a non-PIE process's blocks at 0x41f3f0)."""
    return PROGRAM_BASE <= pc < PROGRAM_END and (cpu is None or address_space(cpu) == cur.asid)


class Run:
    def __init__(self, name):
        self.name = name
        self.ev = []                 # program events, in order
        self.viol = Counter()        # pairing violations, any block
        self.blocks_all = 0          # before_block_exec calls in the window, any block
        self.be_cur = None           # (tb pointer, pc) of the open before/after pair
        self.se_cur = None
        self.invalidated = False
        self.labels = {}
        self.asid = None             # the program's, from hypercall 1
        self.pc_wrong = []           # before_block_exec where the CPU pc wasn't tb->pc
        self.prog_tbs = Counter()    # program blocks started: PCREL or not
        self.odd_starts = 0          # toggle run
        self.toggled = self.reenabled = False


runs = []
cur = None   # the Run in progress


def tbptr(tb):
    return int(ffi.cast("uintptr_t", tb))


@panda.cb_before_block_exec(name="bbe", enabled=False)
def bbe(cpu, tb):
    r = cur
    pc = panda.arch.get_pc(cpu)
    r.blocks_all += 1
    # (A CF_PCREL TB keeps the pc current; its tb->pc is a binding detail, so only non-PCREL ones.)
    if not tb.cflags & CF_PCREL and pc != tb.pc and len(r.pc_wrong) < 1000:
        r.pc_wrong.append((pc, tb.pc))
    if r.be_cur is not None:
        r.viol["before_block_exec while a block was open"] += 1
    prog = in_program(pc, cpu)
    r.be_cur = (tbptr(tb), pc, prog)
    if prog:
        r.ev.append(("b", pc, tb.icount))
        r.prog_tbs["pcrel" if tb.cflags & CF_PCREL else "not pcrel"] += 1
        if r.name == "toggle" and pc == r.labels["odd"]:
            r.odd_starts += 1
            if r.odd_starts == TOGGLE_AT:
                # the last block-exec callbacks off: this block's after_block_exec won't run
                panda.disable_callback("bbe")
                panda.disable_callback("abe")
                r.toggled = True
                r.be_cur = None
                r.ev.append(("x", pc))


@panda.cb_after_block_exec(name="abe", enabled=False)
def abe(cpu, tb, exit_code):
    r = cur
    if r.be_cur is None or r.be_cur[0] != tbptr(tb):
        r.viol["after_block_exec without its before_block_exec"] += 1
        r.be_cur = None
        if r.name == "toggle" and in_program(tb.pc, cpu):
            r.ev.append(("a", tb.pc, exit_code, panda.arch.get_pc(cpu)))
        return
    pc, prog = r.be_cur[1:]
    r.be_cur = None
    if prog:
        r.ev.append(("a", pc, exit_code, panda.arch.get_pc(cpu)))


@panda.cb_start_block_exec(name="sbe", enabled=False)
def sbe(cpu, tb):
    r = cur
    pc = panda.arch.get_pc(cpu)
    if r.se_cur is not None:
        r.viol["start_block_exec while a block was open"] += 1
    prog = in_program(pc, cpu)
    r.se_cur = (tbptr(tb), pc, prog)
    if prog:
        r.ev.append(("s", pc))


@panda.cb_end_block_exec(name="ebe", enabled=False)
def ebe(cpu, tb):
    r = cur
    if r.se_cur is None or r.se_cur[0] != tbptr(tb):
        r.viol["end_block_exec without its start_block_exec"] += 1
        r.se_cur = None
        return
    pc, prog = r.se_cur[1:]
    r.se_cur = None
    if prog:
        r.ev.append(("e", pc))


@panda.cb_before_block_exec_invalidate_opt(name="inv", enabled=False)
def inv(cpu, tb):
    r = cur
    pc = panda.arch.get_pc(cpu)
    if not in_program(pc, cpu):
        return False
    yes = pc == r.labels.get("loop") and not r.invalidated
    r.invalidated |= yes
    r.ev.append(("v", pc, yes))
    return yes


@panda.cb_before_block_translate(name="bbt", enabled=False)
def bbt(cpu, pc):
    if in_program(pc, cpu):
        cur.ev.append(("t", pc))


@panda.cb_insn_translate(name="it", enabled=False)
def it(cpu, pc):
    return in_program(pc)


@panda.cb_insn_exec(name="ie", enabled=False)
def ie(cpu, pc):
    r = cur
    if in_program(pc, cpu):
        r.ev.append(("i", pc))
        if r.toggled and not r.reenabled and pc == r.labels["loop"]:
            panda.enable_callback("bbe")
            panda.enable_callback("abe")
            r.reenabled = True
            r.ev.append(("y", pc))
    return 0


WINDOW = ["bbe", "abe", "sbe", "ebe", "inv", "bbt", "it", "ie"]


@panda.hypercall(MAGIC)
def hypercall(cpu):
    # (MAGIC, n, &labels, 0, pc), as tests/api.py's program passes them
    op = panda.arch.get_arg(cpu, 2, convention="syscall")
    table = panda.arch.get_arg(cpu, 3, convention="syscall")
    print(f"progress: [{cur.name}] hypercall {op}", flush=True)
    if op == 1:
        cur.asid = address_space(cpu)
        raw = bytes(panda.virtual_memory_read(cpu, table, 8 * len(LABELS)))
        cur.labels = {k: int.from_bytes(raw[8 * i:8 * i + 8], "little") for i, k in enumerate(LABELS)}
        for name in (["bbe", "abe", "it", "ie"] if cur.name == "toggle" else WINDOW):
            panda.enable_callback(name)
    elif op == 2:
        for name in WINDOW:
            panda.disable_callback(name)
        cur.done = True


def analyse(r):
    """Walk the program's events: t* [v] b i* a, with s/e beside b/a. Returns per-block counts."""
    L = r.labels
    p = f"[{r.name}] "
    ev = r.ev
    # The window closes inside the hypercall 2 block: its before/start ran, its after/end didn't.
    while ev and ev[-1][0] in ("b", "s", "i", "v", "t"):
        ev.pop()
    blocks = []          # (pc, icount, exit_code, insn pcs)
    bad_seq = []
    open_b = None
    insns = []
    pending_inv = None   # pc whose invalidate_opt said yes: next must be a translation, then its start
    asked = Counter()
    retrans_ok = None
    b_pcs, s_pcs, a_pcs, e_pcs = [], [], [], []
    for e in ev:
        k = e[0]
        if k == "t":
            if pending_inv is not None and e[1] == pending_inv:
                retrans_ok = "translated"
            continue
        if k == "v":
            asked[e[1]] += 1
            if open_b is not None:
                bad_seq.append(("invalidate_opt inside a block", hex(e[1])))
            if e[2]:
                pending_inv = e[1]
            continue
        if k == "b":
            b_pcs.append(e[1])
            if open_b is not None:
                bad_seq.append(("b inside a block", hex(e[1])))
            if pending_inv is not None:
                if e[1] == pending_inv and retrans_ok == "translated":
                    retrans_ok = "ok"
                else:
                    retrans_ok = f"next start {e[1]:#x} after {retrans_ok}"
                pending_inv = None
            open_b = (e[1], e[2])
            insns = []
        elif k == "i":
            if open_b is None:
                bad_seq.append(("insn outside a block", hex(e[1])))
            insns.append(e[1])
        elif k == "a":
            a_pcs.append(e[1])
            if open_b is None or open_b[0] != e[1]:
                bad_seq.append(("a without b", hex(e[1])))
                open_b = None
                continue
            blocks.append((e[1], open_b[1], e[2], insns))
            open_b = None
        elif k == "s":
            s_pcs.append(e[1])
        elif k == "e":
            e_pcs.append(e[1])
    check(p + "every block's before/after_block_exec pair up, on the same TB (kernel's too)",
          not r.viol and r.blocks_all > 0, f"{dict(r.viol)} of {r.blocks_all} blocks")
    check(p + "program events are b, its instructions, a", not bad_seq, bad_seq[:5])
    check(p + "CPU pc == tb->pc at before_block_exec (every non-PCREL block)", not r.pc_wrong,
          f"{len(r.pc_wrong)}: " + str([(hex(a), hex(b)) for a, b in r.pc_wrong[:4]]))
    # A program block that ran to its end: after_block_exec sees the pc where the program goes on
    # (its next block; kernel blocks, e.g. an interrupt's, may come between). Not "sys": x86_64's
    # syscall jumps to the kernel within the block.
    bad_next = []
    for i, e in enumerate(ev):
        if e[0] == "a" and e[2] <= TB_EXIT_IDX1 and e[1] != L["sys"]:
            nxt = next((x[1] for x in ev[i + 1:] if x[0] == "b"), None)
            if nxt is not None and e[3] != nxt:
                bad_next.append(f"{e[1]:#x}: CPU pc {e[3]:#x}, next block {nxt:#x}")
    check(p + "after_block_exec of a block that ran to its end sees the next block's pc", not bad_next,
          f"{len(bad_next)}: {bad_next[:4]}")
    check(p + "start/end_block_exec see the same blocks as before/after_block_exec",
          s_pcs == b_pcs and e_pcs == a_pcs,
          f"b={len(b_pcs)} s={len(s_pcs)} a={len(a_pcs)} e={len(e_pcs)}")

    wrong = []
    for pc, icount, code, ins in blocks:
        if ins and ins[0] != pc:
            wrong.append(f"{pc:#x}: first insn {ins[0]:#x}")
        if code <= TB_EXIT_IDX1:
            if len(ins) != icount:
                wrong.append(f"{pc:#x}: ran to its end with {len(ins)} insns, icount {icount}")
        elif not 0 < len(ins) <= icount:
            wrong.append(f"{pc:#x}: left early with {len(ins)} insns, icount {icount}")
    check(p + "instruction ground truth: a complete block ran tb->icount instructions from its pc",
          blocks and not wrong, f"{len(blocks)} blocks; {wrong[:4]}")

    count = Counter(pc for pc, *_ in blocks)
    by_label = {k: count[L[k]] for k in LABELS}
    insn_at = Counter(i for b in blocks for i in b[3])
    # Instructions are ground truth: loop's first instruction runs N times, odd's N/2 and even's N
    # (odd falls into it). Blocks: loop and odd start N and N/2 times; even starts N/2 times, plus
    # once for each odd block that icount split before even's instruction.
    split_odd = sum(1 for pc, ic, c, ins in blocks if pc == L["odd"] and c == 0 and L["even"] not in ins)
    check(p + f"loop instructions run exactly N={N}, N/2, N times",
          (insn_at[L["loop"]], insn_at[L["odd"]], insn_at[L["even"]]) == (N, N // 2, N),
          (insn_at[L["loop"]], insn_at[L["odd"]], insn_at[L["even"]]))
    check(p + f"loop blocks fire exactly N={N}, N/2, N/2 (+ icount splits) times",
          (by_label["loop"], by_label["odd"], by_label["even"], by_label["setup"])
          == (N, N // 2, N // 2 + split_odd, 1), (by_label, f"odd blocks split by icount: {split_odd}"))
    early = [(pc, code, ins) for pc, _, code, ins in blocks if code > TB_EXIT_IDX1]
    fault_early = [b for b in early if b[0] == L["fault"]]
    check(p + "the faulting block exits early once, at the faulting store",
          len(fault_early) == 1 and fault_early[0][2][-1:] == [L["fault_store"]],
          [(hex(pc), c, [hex(i) for i in ins]) for pc, c, ins in early])
    check(p + "the faulting store then starts a block that runs to its end",
          [c for pc, _, c, _ in blocks if pc == L["fault_store"]] == [0],
          [(hex(pc), c) for pc, _, c, _ in blocks if pc in (L["fault"], L["fault_store"])])
    sys_codes = [c for pc, _, c, _ in blocks if pc == L["sys"]]
    want = 0 if arch == "x86_64" else 3
    check(p + f"the system call block: exit code {want} ({arch})", sys_codes == [want], sys_codes)
    check(p + "nothing else in the program exits early",
          all(b[0] in (L["fault"], L["sys"]) for b in early), [hex(b[0]) for b in early])
    check(p + "invalidate_opt: true once for loop, then a fresh translation of it starts",
          r.invalidated and retrans_ok == "ok", retrans_ok)
    # The start right after a true isn't asked if it is the retranslated block. When an interrupt
    # comes between (the kernel's blocks start first), it is asked again, once: N + 1.
    check(p + "invalidate_opt asked at every start of loop but the retranslated one",
          asked[L["loop"]] in (N, N + 1), f"asked {asked[L['loop']]} want {N} (or {N + 1})")
    return dict(insn_at), sum(len(b[3]) for b in blocks)


# A TCG hook (accel/tcg/hooks.h) on the program's first page, at its ELF header (never executed):
# the page's code is then translated without CF_PCREL.
hffi = cffi.FFI()
hffi.cdef("void *tcg_hook_insert(uint64_t pc, void (*fn)(void *cpu, uint64_t pc, void *opaque), "
          "void *opaque);")
hook_hits = []


@hffi.callback("void(void *, uint64_t, void *)")
def hook_fn(cpu, pc, opaque):
    hook_hits.append(pc)


def hook_page():
    hlib = hffi.dlopen(panda.libpanda_path)
    assert hlib.tcg_hook_insert(PROGRAM_BASE, hook_fn, hffi.NULL) != hffi.NULL


def analyse_toggle(r):
    p = f"[{r.name}] "
    ev = r.ev
    check(p + f"before_block_exec disabled both at odd's {TOGGLE_AT}th start; insn_exec re-enabled them",
          r.toggled and r.reenabled, (r.toggled, r.reenabled))
    if not (r.toggled and r.reenabled):
        return
    ix = next(i for i, e in enumerate(ev) if e[0] == "x")
    iy = next(i for i, e in enumerate(ev) if e[0] == "y")
    ib = next((i for i in range(iy, len(ev)) if ev[i][0] == "b"), len(ev))
    late = [e for e in ev[ix:ib] if e[0] == "a"]
    check(p + "no after_block_exec for the block running when they were disabled (none late)",
          not late, [(hex(e[1]), e[2], hex(e[3])) for e in late])
    check(p + "before/after_block_exec pair up on the same TB, before and after the toggle",
          not r.viol, dict(r.viol))
    n_after = sum(1 for e in ev[ib:] if e[0] == "b" and e[1] == r.labels["loop"])
    check(p + "the callbacks run again after the re-enable", n_after > 0, n_after)


@panda.queue_blocking
def drive():
    global cur
    panda.revert_sync("root")
    data = base64.b64decode(PROGRAMS[arch])
    panda.run_serial_cmd("rm -f /tmp/blocks")
    for i in range(0, len(data), 64):
        chunk = "".join(f"\\x{b:02x}" for b in data[i:i + 64])
        panda.run_serial_cmd(f"printf '{chunk}' >> /tmp/blocks")
    panda.run_serial_cmd("chmod +x /tmp/blocks")
    # toggle before the hook: its page stays hooked for the rest of the process
    for name, chaining in (("chaining on", True), ("chaining off", False), ("toggle", True),
                           ("hooked page", True)):
        cur = Run(name)
        cur.done = False
        runs.append(cur)
        if chaining:
            panda.enable_tb_chaining()
        else:
            panda.disable_tb_chaining()
        if name == "hooked page":
            hook_page()
        print(f"progress: [{name}] running the program", flush=True)
        cur.output = panda.run_serial_cmd("/tmp/blocks; echo rc=$?")
    panda.enable_tb_chaining()
    panda.end_analysis()


panda.run()

results = []
for r in runs:
    check(f"[{r.name}] the program ran between its hypercalls and exited 0",
          r.done and "rc=0" in r.output, repr(r.output))
    if r.name == "toggle":
        analyse_toggle(r)
        continue
    results.append(analyse(r))
    want = {"not pcrel"} if r.name == "hooked page" else {"pcrel"}
    check(f"[{r.name}] program TBs are " + ("not " if r.name == "hooked page" else "") + "CF_PCREL",
          set(r.prog_tbs) == want, dict(r.prog_tbs))
if len(results) == 3:
    check("chaining on, off, and on a hooked page: the same instructions, each as many times",
          results[0] == results[1] == results[2], results)
check("the hook on the ELF header never ran", not hook_hits, hook_hits[:3])

print(f"BLOCKS arch={arch} " + " ".join(f"[{r.name}] insns={res[1]} all={r.blocks_all}"
                                         for r, res in zip(runs, results)))
failed = False
for name, (ok, detail) in checks.items():
    failed |= not ok
    print(f"  {'PASS' if ok else 'FAIL'} {name}" + ("" if ok else f": {detail}"))
sys.exit(1 if failed else 0)
