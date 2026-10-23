"""Classic plugins' behaviour as checks: callstack_instr, edge_coverage and hooks.

Run: panda-python tests/calls.py <dir>/debian-13-x86_64.json

tests/programs/calls_<arch>.S has a known call graph (see its header): N=20 calls of f, which
calls g on odd i and h always; g calls h; a recursion r of depth D=4. Its label table gives each
block's address. Between hypercalls 1 and 2 the test runs, in Python, what the plugins did:
- callstack_instr: after_block_translate classifies each program block by its last instruction
  (call, return, other: decoded per arch, as callstack_instr did with capstone); after_block_exec
  of a call block pushes its return address (the block's end), and a block starting at the top
  return address pops it. Checks: calls per function, every return matched, the stack empty again,
  the maximum depth, and the call stack seen at each entry to h;
- edge_coverage: (previous block, block) pairs of program blocks from before_block_exec, counted,
  against the edges and counts the program's structure gives;
- hooks (the hooks plugin through Panda.hook): a hook at a block start fires once per execution;
  at an address inside a block too; one that disables itself fires once; a kernel-only hook on user
  code never fires; a before_block_exec-typed hook fires like the default.
Hooks are added at hypercall 1 (the program's code after it isn't translated yet). Not checked: a
hook's asid filter (waits for tbpc's process identifier); after_block_exec-typed hooks (the hooks
plugin calls every hook with the before_tcg_codegen signature, so those would get wrong arguments).
"""
import sys
from collections import Counter

from testlib import Checks, MAGIC, hypercall_args, in_program, progress, read_quads, upload
from pandare2 import Panda

LABELS = ["N", "D", "setup", "loop", "ret_main", "done", "ret_r0", "f", "hook_mid", "f_odd", "f_even",
          "f_ret", "g", "g_ret", "h", "r", "r_rec", "r_ret", "r_base", "fin"]
BLOCKS = [k for k in LABELS[2:] if k != "hook_mid"]

panda = Panda(generic=sys.argv[1])
arch = panda.arch_name
ffi = panda.ffi
C = Checks(arch)
S = {"labels": {}, "kind": {}, "stack": [], "calls": Counter(), "returns": 0, "bad_returns": [],
     "maxdepth": 0, "h_stacks": Counter(), "pending_call": False, "edges": Counter(), "prev": None,
     "hooks": Counter(), "hook_pcs": {}, "done": False, "untracked_calls": 0}


def classify(code):
    """'call', 'ret' or None for a block's last instruction, given the block's bytes."""
    if arch == "x86_64":
        if code[-1:] == b"\xc3":
            return "ret"
        if len(code) >= 5 and code[-5] == 0xe8:
            return "call"
        return None
    insn = int.from_bytes(code[-4:], "little")
    if arch == "aarch64":
        if insn == 0xd65f03c0:
            return "ret"
        if insn >> 26 == 0b100101 or insn & 0xfffffc1f == 0xd63f0000:   # bl, blr
            return "call"
        return None
    # riscv64: ret = jalr x0, 0(ra); call = jal/jalr with rd = ra
    if insn == 0x00008067:
        return "ret"
    if insn & 0x7f in (0x6f, 0x67) and (insn >> 7) & 0x1f == 1:
        return "call"
    return None


def label_of(pc):
    """The labelled block containing pc (icount or an interrupt can split a block)."""
    L = S["labels"]
    best = None
    for k in BLOCKS:
        if L[k] <= pc and (best is None or L[k] > L[best]):
            best = k
    return best


@panda.cb_after_block_translate(name="abt", enabled=False)
def abt(cpu, tb):
    if in_program(tb.pc):
        code = bytes(panda.virtual_memory_read(cpu, tb.pc, tb.size))
        S["kind"][(tb.pc, tb.size)] = (classify(code), tb.pc + tb.size)


@panda.cb_before_block_exec(name="bbe", enabled=False)
def bbe(cpu, tb):
    pc = panda.arch.get_pc(cpu)
    if not in_program(pc):
        return
    L = S["labels"]
    # callstack_instr: a block starting at a return address on the stack is a return.
    st = S["stack"]
    if st and st[-1][0] == pc:
        st.pop()
        S["returns"] += 1
    elif S.get("last_kind") == "ret":
        S["bad_returns"].append(hex(pc))
    S["last_kind"] = None
    if S["pending_call"]:
        S["pending_call"] = False
        callee = label_of(pc)
        st[-1] = (st[-1][0], callee)
        S["calls"][callee] += 1
        S["maxdepth"] = max(S["maxdepth"], len(st))
        if callee == "h":
            S["h_stacks"][tuple(c for _, c in st)] += 1
    # edge_coverage, on labelled blocks (a split block's second half is not a new edge).
    lab = label_of(pc)
    if lab is not None and L[lab] == pc:
        S["edges"][(S["prev"], lab)] += 1
    S["prev"] = lab


@panda.cb_after_block_exec(name="abe", enabled=False)
def abe(cpu, tb, exit_code):
    if not in_program(tb.pc):
        return
    kind = S["kind"].get((tb.pc, tb.size))
    if kind is None:
        S["untracked_calls"] += 1   # executed but never seen translated (window opened after)
        return
    if exit_code > 1:
        return                      # left early: its last instruction didn't run
    if kind[0] == "call":
        S["stack"].append((kind[1], None))
        S["pending_call"] = True
    S["last_kind"] = kind[0]


WINDOW = ["abt", "bbe", "abe"]


def add_hooks(L):
    def counter(key):
        def fn(cpu, tb, h):
            S["hooks"][key] += 1
            S["hook_pcs"].setdefault(key, set()).add(panda.arch.get_pc(cpu))
        return fn

    panda.hook(L["h"], kernel=False)(counter("h (block start)"))
    panda.hook(L["hook_mid"], kernel=False)(counter("hook_mid (inside f's block)"))
    panda.hook(L["g"], kernel=True)(counter("g, kernel-only"))
    panda.hook(L["f_ret"], kernel=False, cb_type="before_block_exec")(counter("f_ret (before_block_exec type)"))

    def once(cpu, tb, h):
        S["hooks"]["r (disables itself)"] += 1
        h.enabled = False
    panda.hook(L["r"], kernel=False)(once)


@panda.hypercall(MAGIC)
def hypercall(cpu):
    op, table, _, pc = hypercall_args(panda, cpu)
    progress(f"hypercall {op}")
    if op == 1:
        S["labels"] = dict(zip(LABELS, read_quads(panda, cpu, table, len(LABELS))))
        add_hooks(S["labels"])
        for name in WINDOW:
            panda.enable_callback(name)
        panda.flush_tb()
    elif op == 2:
        for name in WINDOW:
            panda.disable_callback(name)
        S["done"] = True
        S["stack_at_end"] = list(S["stack"])


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    upload(panda, "calls", arch)
    progress("running /tmp/calls")
    S["output"] = panda.run_serial_cmd("/tmp/calls; echo rc=$?")
    panda.end_analysis()


panda.run()

L = S["labels"]
N, D = L.get("N", 20), L.get("D", 4)
odd, even = N // 2, N - N // 2
C.check("the program ran between its hypercalls and exited 0", S["done"] and "rc=0" in S.get("output", ""),
        repr(S.get("output")))
kinds = Counter(k[0] for k in S["kind"].values())
C.check("callstack_instr: program blocks classified (calls and returns found)",
        kinds["call"] >= 6 and kinds["ret"] >= 5, dict(kinds))
want_calls = {"f": N, "g": odd, "h": N + odd, "r": D + 1}
C.check("callstack_instr: calls per function", dict(S["calls"]) == want_calls,
        f"{dict(S['calls'])} want {want_calls}")
C.check("callstack_instr: every call returned, to its return address",
        S["returns"] == sum(want_calls.values()) and not S["bad_returns"] and not S.get("stack_at_end"),
        f"returns {S['returns']} bad {S['bad_returns'][:4]} left {S.get('stack_at_end')}")
C.check("callstack_instr: maximum depth (r's recursion)", S["maxdepth"] == D + 1, S["maxdepth"])
C.check("callstack_instr: the call stack at each entry to h",
        S["h_stacks"] == Counter({("f", "g", "h"): odd, ("f", "h"): N}), dict(S["h_stacks"]))
C.check("callstack_instr: every executed program block was seen translated", S["untracked_calls"] == 0,
        S["untracked_calls"])
want_edges = Counter({
    (None, "setup"): 1, ("setup", "loop"): 1, ("loop", "f"): N, ("f", "f_odd"): odd, ("f", "f_even"): even,
    ("f_odd", "g"): odd, ("g", "h"): odd, ("h", "g_ret"): odd, ("g_ret", "f_even"): odd,
    ("f_even", "h"): N, ("h", "f_ret"): N, ("f_ret", "ret_main"): N, ("ret_main", "loop"): N - 1,
    ("ret_main", "done"): 1, ("done", "r"): 1, ("r", "r_rec"): D, ("r_rec", "r"): D, ("r", "r_base"): 1,
    ("r_base", "r_ret"): 1, ("r_ret", "r_ret"): D - 1, ("r_ret", "ret_r0"): 1, ("ret_r0", "fin"): 1})
C.check("edge_coverage: the program's edges, each as often as it runs", S["edges"] == want_edges,
        f"extra {dict(S['edges'] - want_edges)} missing {dict(want_edges - S['edges'])}")
want_hooks = {"h (block start)": N + odd, "hook_mid (inside f's block)": N, "f_ret (before_block_exec type)": N,
              "r (disables itself)": 1}
for key, n in want_hooks.items():
    C.check(f"hooks: {key} fires once per execution ({n})", S["hooks"][key] == n, S["hooks"][key])
C.check("hooks: a kernel-only hook on user code never fires", S["hooks"]["g, kernel-only"] == 0,
        S["hooks"]["g, kernel-only"])
sys.exit(C.report(f"CALLS arch={arch} calls={dict(S['calls'])} edges={sum(S['edges'].values())} "
                  f"hooks={dict(S['hooks'])} hook_pcs={ {k: [hex(x) for x in v] for k, v in S['hook_pcs'].items()} }"))
