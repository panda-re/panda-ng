"""Callbacks registered after the code they watch was translated still see it run.

Run: panda-python tests/late.py <dir>/debian-13-x86_64.json

What the TCG bridge instruments is decided when a block is translated, from the callbacks
registered then. tests/programs/blocks_<arch>.S (tests/blocks.py's program) runs three times from
the same file, so its blocks stay translated from one run to the next (same physical pages):
1. nothing registered: the program's code is translated, uninstrumented;
2. at its hypercall 1, insn_translate (true for the program's instructions), insn_exec and
   virt_mem_after_write are registered; between hypercalls 1 and 2 the program's loop must be
   seen executing exactly N, N/2, N times and its two stores written once each. At hypercall 2
   the instruction callbacks are deleted and virt_mem_after_write is disabled (still registered):
   enabled, it is a Python call for every store the guest makes, and the shell starting run 3
   under it took ~55 s on ubuntu-24.04-aarch64 (HZ=1000) on igloo, past run_serial_cmd's 30 s.
3. at hypercall 1, virt_mem_after_write is enabled again and virt_mem_before_write is added (a
   memory callback's other side): both sides must see the two stores. At hypercall 2 every
   callback goes.
Each registration happens from a callback on the vCPU, right before the already-translated code
runs again.
"""
import base64
import os
import sys
from collections import Counter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs"))
from blocks_programs import PROGRAMS  # noqa: E402

from pandare2 import Panda  # noqa: E402

MAGIC = 0x50616e64
N = 1000
PROGRAM_BASE, PROGRAM_END = 0x400000, 0x420000
LABELS = ["setup", "loop", "odd", "even", "fault", "fault_store", "sys"]

panda = Panda(generic=sys.argv[1])
arch = panda.arch_name
st = {"run": 0, "labels": {}, "insn": Counter(), "after_w": Counter(), "before_w": Counter(),
      "between": False, "hc": []}


def in_program(pc):
    return PROGRAM_BASE <= pc < PROGRAM_END


def on_insn_translate(cpu, pc):
    return in_program(pc)


def on_insn_exec(cpu, pc):
    if st["between"]:
        st["insn"][(st["run"], pc)] += 1
    return 0


def on_after_write(cpu, pc, addr, size, buf):
    if st["between"] and in_program(pc):
        st["after_w"][(st["run"], pc)] += 1


def on_before_write(cpu, pc, addr, size, buf):
    if st["between"] and in_program(pc):
        st["before_w"][(st["run"], pc)] += 1


# --8<-- [start:late]
@panda.hypercall(MAGIC)
def hypercall(cpu):
    op = panda.arch.get_arg(cpu, 2, convention="syscall")
    table = panda.arch.get_arg(cpu, 3, convention="syscall")
    run = st["run"]
    st["hc"].append((run, op))
    if op == 1:
        raw = bytes(panda.virtual_memory_read(cpu, table, 8 * len(LABELS)))
        st["labels"] = {k: int.from_bytes(raw[8 * i:8 * i + 8], "little") for i, k in enumerate(LABELS)}
        if run == 2:
            panda.cb_insn_translate(name="it")(on_insn_translate)
            panda.cb_insn_exec(name="ie")(on_insn_exec)
            panda.cb_virt_mem_after_write(name="aw")(on_after_write)
        elif run == 3:
            panda.enable_callback("aw")
            panda.cb_virt_mem_before_write(name="bw")(on_before_write)
        st["between"] = True
    elif op == 2:
        st["between"] = False
        if run == 2:
            panda.delete_callback("it")
            panda.delete_callback("ie")
            panda.disable_callback("aw")
        elif run == 3:
            panda.delete_callback("aw")
            panda.delete_callback("bw")
# --8<-- [end:late]


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    data = base64.b64decode(PROGRAMS[arch])
    panda.run_serial_cmd("rm -f /tmp/late")
    for i in range(0, len(data), 64):
        chunk = "".join(f"\\x{b:02x}" for b in data[i:i + 64])
        panda.run_serial_cmd(f"printf '{chunk}' >> /tmp/late")
    panda.run_serial_cmd("chmod +x /tmp/late")
    for run in (1, 2, 3):
        st["run"] = run
        st[f"out{run}"] = panda.run_serial_cmd("/tmp/late; echo rc=$?").strip()
        print(f"progress: run {run}: {st[f'out{run}']!r}", flush=True)
    panda.end_analysis()


panda.run()

L = st["labels"]
stores = None
checks = {}
if L:
    ins2 = {pc: n for (r, pc), n in st["insn"].items() if r == 2}
    loop = (ins2.get(L["loop"], 0), ins2.get(L["odd"], 0), ins2.get(L["even"], 0))
    aw2 = {pc: n for (r, pc), n in st["after_w"].items() if r == 2}
    aw3 = {pc: n for (r, pc), n in st["after_w"].items() if r == 3}
    bw3 = {pc: n for (r, pc), n in st["before_w"].items() if r == 3}
    print(f"LATE arch={arch} run2 loop insns={loop} insns={sum(ins2.values())} "
          f"after_write2={ {hex(k): v for k, v in aw2.items()} } after_write3={ {hex(k): v for k, v in aw3.items()} } "
          f"before_write3={ {hex(k): v for k, v in bw3.items()} } hypercalls={st['hc']}")
    store_pcs = sorted(aw2)
    checks = {
        "the program ran three times, exit 0": all("rc=0" in st.get(f"out{r}", "") for r in (1, 2, 3)),
        "hypercalls 1 and 2 on each run": st["hc"] == [(1, 1), (1, 2), (2, 1), (2, 2), (3, 1), (3, 2)],
        f"run 2: insn_exec registered late sees the loop exactly N={N}, N/2, N times": loop == (N, N // 2, N),
        "run 2: insn_exec sees the stores' block": L["fault_store"] in ins2,
        "run 2: virt_mem_after_write registered late sees the program's two stores, once each":
            len(store_pcs) == 2 and all(v == 1 for v in aw2.values()) and store_pcs[0] == L["fault_store"],
        "run 3: the after side still sees them": len(aw3) == 2 and aw3 == aw2,
        "run 3: virt_mem_before_write added late sees both stores":
            len(bw3) == 2 and sorted(bw3) == store_pcs and all(v >= 1 for v in bw3.values()),
        "run 3: no instruction callbacks after their deletion": not any(r == 3 for r, _ in st["insn"]),
    }
else:
    checks = {"hypercall 1 read the program's labels": False}
for k, ok in checks.items():
    print(f"  {'PASS' if ok else 'FAIL'} {k}")
sys.exit(0 if all(checks.values()) else 1)
