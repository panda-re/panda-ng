"""Block-exec callbacks on a target without CF_PCREL (mips, ppc), TB chaining on, bare metal.

Run: panda-python tests/blocks_bare.py mips|ppc

Without CF_PCREL, chained code doesn't store the guest PC (goto_tb leaves that to the unchained
exit), so at a chained block's start the CPU's PC is some earlier block's unless the engine
synchronizes it there. tests/programs/bare_<arch>.S (no OS, no interrupts: mips on malta via
-kernel, ppc on g3beige via -bios) runs blocks_<arch>.S's loop: "loop" N times, "odd" and "even"
N/2 times each, chained to one another, then spins in "done". Every callback is on from the start.
Checks:
- the CPU's PC is the block's own (tb->pc) in before_block_exec, invalidate_opt and
  start_block_exec, for every block (the firmware's too), and every TB is non-PCREL;
- after_block_exec / end_block_exec of a block that ran to its end see the next block's PC;
- invalidate_opt returns true once, at the 500th start of "loop" (entered through a chain): the
  retranslated "loop" starts next, and nothing runs twice: insn_exec ground truth, loop's
  instructions run exactly N, N/2, N times;
- the guest's own result (v0 / r4 at "done") is sum(1..N) + 3 * N/2.
"""
import base64
import os
import sys
import tempfile
from collections import Counter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs"))
from bare_programs import PROGRAMS  # noqa: E402

from pandare2 import Panda  # noqa: E402

N = 1000
CF_PCREL = 0x00020000
TB_EXIT_IDX1 = 1
INVALIDATE_AT = 500  # the 500th start of "loop": by then its predecessors chain into it

arch = sys.argv[1]
prog = PROGRAMS[arch]
L = prog["labels"]
elf = os.path.join(tempfile.mkdtemp(prefix="blocks_bare-"), "prog.elf")
with open(elf, "wb") as f:
    f.write(base64.b64decode(prog["elf"]))
lo, hi = min(L.values()), max(L.values()) + 16
machine = {"mips": ["-M", "malta", "-kernel", elf],
           "ppc": ["-M", "g3beige", "-bios", elf]}[arch]
result_reg = {"mips": "V0", "ppc": "R4"}[arch]

panda = Panda(arch=arch, mem="128M", extra_args=machine + ["-display", "none"])
ffi = panda.ffi

st = {
    "blocks": 0, "pcrel": 0, "pc_wrong": Counter(), "pc_wrong_ex": [],
    "after": [],          # (tb pc, exit code, CPU pc) per after_block_exec
    "end": [],            # (tb pc, CPU pc) per end_block_exec
    "starts": [],         # tb pc per before_block_exec, in order
    "ev": [],             # program events: ("b", pc) ("i", pc) ("a", pc, code) ("v", pc, yes)
    "loop_asks": 0, "invalidated": None, "result": None, "done": False,
}


def in_prog(pc):
    return lo <= pc < hi


def pc_check(where, cpu, tb):
    pc = panda.arch.get_pc(cpu)
    if pc != tb.pc:
        st["pc_wrong"][where] += 1
        if len(st["pc_wrong_ex"]) < 6:
            st["pc_wrong_ex"].append(f"{where}: CPU pc {pc:#x}, tb->pc {tb.pc:#x}")
    return pc


@panda.cb_before_block_exec_invalidate_opt
def inv(cpu, tb):
    pc_check("invalidate_opt", cpu, tb)
    if tb.pc != L["loop"]:
        return False
    st["loop_asks"] += 1
    yes = st["loop_asks"] == INVALIDATE_AT
    if yes:
        st["invalidated"] = "asked"
    st["ev"].append(("v", tb.pc, yes))
    return yes


@panda.cb_before_block_exec
def bbe(cpu, tb):
    st["blocks"] += 1
    if tb.cflags & CF_PCREL:
        st["pcrel"] += 1
    pc_check("before_block_exec", cpu, tb)
    st["starts"].append(tb.pc)
    if in_prog(tb.pc):
        st["ev"].append(("b", tb.pc))
    if tb.pc == L["done"] and not st["done"]:
        st["done"] = True
        st["result"] = panda.arch.get_reg(cpu, result_reg)
        panda.end_analysis()


@panda.cb_start_block_exec
def sbe(cpu, tb):
    pc_check("start_block_exec", cpu, tb)


@panda.cb_after_block_exec
def abe(cpu, tb, code):
    # starts has this block's start last: the next start will be starts[len(starts)]
    st["after"].append((tb.pc, code, panda.arch.get_pc(cpu), len(st["starts"])))
    if in_prog(tb.pc):
        st["ev"].append(("a", tb.pc, code))


@panda.cb_end_block_exec
def ebe(cpu, tb):
    st["end"].append((tb.pc, panda.arch.get_pc(cpu), len(st["starts"])))


@panda.cb_insn_translate
def it(cpu, pc):
    return in_prog(pc)


@panda.cb_insn_exec
def ie(cpu, pc):
    if in_prog(pc):
        st["ev"].append(("i", pc))
    return 0


panda.run()

checks = {}


def check(name, ok, detail=""):
    checks[name] = (bool(ok), detail)


check("the program reached done", st["done"], f"{st['blocks']} blocks")
check("every TB is non-PCREL (this test's point)", st["blocks"] and st["pcrel"] == 0,
      f"{st['pcrel']} of {st['blocks']}")
check("CPU pc == tb->pc in before_block_exec, invalidate_opt, start_block_exec (every block)",
      not st["pc_wrong"], f"{dict(st['pc_wrong'])}: {st['pc_wrong_ex']}")

# after/end_block_exec of a block that ran to its end (exit code 0) see the next block's start.
starts = st["starts"]
bad_after = []
for tbpc, code, pc, nxt in st["after"]:
    if nxt < len(starts) and code <= TB_EXIT_IDX1 and pc != starts[nxt]:
        bad_after.append(f"after {tbpc:#x}: CPU pc {pc:#x}, next block {starts[nxt]:#x}")
check("after_block_exec (ran to its end) sees the next block's pc", not bad_after,
      f"{len(bad_after)} of {len(st['after'])}: {bad_after[:4]}")
# (end_block_exec runs just before the same block's after_block_exec: skip blocks that left early,
# e.g. malta's firmware's I/O stores, which longjmp out to be translated again.)
code_at = {nxt: code for _, code, _, nxt in st["after"]}
bad_end = [f"end {tbpc:#x}: CPU pc {pc:#x}, next {starts[nxt]:#x}"
           for tbpc, pc, nxt in st["end"]
           if nxt < len(starts) and code_at.get(nxt, 0) <= TB_EXIT_IDX1 and pc != starts[nxt]]
check("end_block_exec (ran to its end) sees the next block's pc", not bad_end, f"{len(bad_end)}: {bad_end[:4]}")

# Ground truth, and the invalidation: right after the true, the retranslated "loop" starts.
ev = st["ev"]
insn_at = Counter(e[1] for e in ev if e[0] == "i")
want = {"setup": 1, "loop": N, "odd": N // 2, "even": N}
got = {k: insn_at[L[k]] for k in want}
check(f"instructions run exactly setup 1, loop N={N}, odd N/2, even N times", got == want, got)
blocks = Counter(e[1] for e in ev if e[0] == "b")
got_b = {k: blocks[L[k]] for k in ("setup", "loop", "odd", "even")}
check("blocks start setup 1, loop N, odd N/2, even N/2 times",
      got_b == {"setup": 1, "loop": N, "odd": N // 2, "even": N // 2}, got_b)
after_true = None
for i, e in enumerate(ev):
    if e[0] == "v" and e[2]:
        nxt = [x for x in ev[i + 1:] if x[0] in ("b", "i", "v")][:2]
        after_true = nxt
check(f"invalidate_opt true at the {INVALIDATE_AT}th start of loop, then loop starts (not asked again)",
      after_true is not None and after_true[:1] == [("b", L["loop"])],
      after_true)
# N starts of loop that ran, plus the one left at its start; the retranslated one isn't asked
check(f"invalidate_opt asked at every start of loop but the retranslated one ({N})",
      st["loop_asks"] == N, st["loop_asks"])
want_result = N * (N + 1) // 2 + 3 * (N // 2)
check(f"the guest's result is {want_result}", st["result"] == want_result, st["result"])

print(f"BLOCKS_BARE arch={arch} blocks={st['blocks']} after={len(st['after'])}")
failed = False
for name, (ok, detail) in checks.items():
    failed |= not ok
    print(f"  {'PASS' if ok else 'FAIL'} {name}" + ("" if ok else f": {detail}"))
sys.exit(1 if failed else 0)
