"""Actual block edges and late enable/disable on already translated guest code.

Run: panda-python tests/block_activation.py <image.json>
Warm the program with both callbacks disabled, then select entry only, end
only, both, neither, and both again. No caller requests a TB flush. End-only
must not require entry instrumentation. Faulting blocks have no end event;
normal end observers see the outgoing PC. The enabled-shape query verifies
that disabled callbacks leave neither a block helper nor a gate requested.
"""
import base64
from collections import Counter
import cffi
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs"))
from blocks_programs import PROGRAMS
from pandare2 import Panda

panda = Panda(generic=sys.argv[1], extra_args=["-icount", "shift=6,sleep=off"])
arch = panda.arch_name
probe = cffi.FFI()
probe.cdef("unsigned panda_bridge_wants(void);")
engine = probe.dlopen(panda.libpanda_path)
LABELS = ["setup", "loop", "odd", "even", "fault", "fault_store", "sys"]
MODES = [(False, False), (True, False), (False, True), (True, True),
         (False, False), (True, True)]
runs = []
current = None
checks = []


def check(name, ok, detail=""):
    checks.append((name, bool(ok), detail))


@panda.cb_start_block_exec(name="entry", enabled=False)
def entry(cpu, tb):
    pc = panda.tb_pc(cpu, tb)
    current["starts"][pc] += 1


@panda.cb_end_block_exec(name="end", enabled=False)
def end(cpu, tb):
    pc = panda.tb_pc(cpu, tb)
    current["ends"][pc] += 1
    if pc == current["labels"].get("loop"):
        current["loop_destinations"].add((tb.size, panda.arch.get_pc(cpu)))


@panda.hypercall(0x50616e64)
def hypercall(cpu):
    op = panda.arch.get_arg(cpu, 2, convention="syscall")
    if op == 1:
        table = panda.arch.get_arg(cpu, 3, convention="syscall")
        data = bytes(panda.virtual_memory_read(cpu, table, 8 * len(LABELS)))
        current["labels"] = {name: int.from_bytes(data[i * 8:i * 8 + 8], "little")
                             for i, name in enumerate(LABELS)}
        start_on, end_on = current["mode"]
        for name, on in (("entry", start_on), ("end", end_on)):
            (panda.enable_callback if on else panda.disable_callback)(name)
        current["shape"] = engine.panda_bridge_wants() & 6
    elif op == 2:
        panda.disable_callback("entry")
        panda.disable_callback("end")
        current["closed_shape"] = engine.panda_bridge_wants() & 6
        current["closed_counts"] = (sum(current["starts"].values()),
                                     sum(current["ends"].values()))


@panda.queue_blocking
def drive():
    global current
    panda.revert_sync("root")
    data = base64.b64decode(PROGRAMS[arch])
    panda.run_serial_cmd("rm -f /tmp/block-activation")
    for i in range(0, len(data), 64):
        chunk = "".join(f"\\x{b:02x}" for b in data[i:i + 64])
        panda.run_serial_cmd(f"printf '{chunk}' >> /tmp/block-activation")
    panda.run_serial_cmd("chmod +x /tmp/block-activation")
    for mode in MODES:
        current = {"mode": mode, "starts": Counter(), "ends": Counter(),
                   "labels": {}, "loop_destinations": set()}
        runs.append(current)
        current["output"] = panda.run_serial_cmd("/tmp/block-activation; echo rc=$?")
        current["final_counts"] = (sum(current["starts"].values()),
                                    sum(current["ends"].values()))
    panda.end_analysis()


panda.run()
for i, run in enumerate(runs):
    prefix = f"run {i} {run['mode']}"
    start_on, end_on = run["mode"]
    labels = run["labels"]
    check(prefix + " guest completes", "rc=0" in run["output"])
    check(prefix + " independent enabled shape", run.get("shape") ==
          (2 * start_on | 4 * end_on), run.get("shape"))
    check(prefix + " disabling removes both shapes", run.get("closed_shape") == 0)
    check(prefix + " no callbacks after disable", run.get("closed_counts") == run["final_counts"])
    if start_on:
        check(prefix + " all 1000 loop entries observed after late enable",
              run["starts"][labels["loop"]] == 1000, run["starts"][labels["loop"]])
    else:
        check(prefix + " no entry callbacks", not run["starts"])
    if end_on:
        check(prefix + " all 1000 normal loop exits observed",
              run["ends"][labels["loop"]] == 1000, run["ends"][labels["loop"]])
        # An icount budget can end a TB before the loop's conditional branch.
        # Such a TB completes normally at its next sequential instruction.
        loop_size = labels["odd"] - labels["loop"]
        destinations = {pc for size, pc in run["loop_destinations"]
                        if size == loop_size}
        split_pcs_valid = all(0 < size < loop_size and pc == labels["loop"] + size
                              for size, pc in run["loop_destinations"]
                              if size != loop_size)
        check(prefix + " outgoing PC committed before end callback",
              destinations == {labels["odd"], labels["even"]} and split_pcs_valid,
              sorted(run["loop_destinations"]))
        check(prefix + " faulted block has no completion event",
              run["ends"][labels["fault"]] == 0)
        check(prefix + " retried faulting store completes",
              run["ends"][labels["fault_store"]] == 1,
              run["ends"][labels["fault_store"]])
        check(prefix + " exception syscall has no normal completion",
              run["ends"][labels["sys"]] == (1 if arch == "x86_64" else 0),
              run["ends"][labels["sys"]])
    else:
        check(prefix + " no end callbacks", not run["ends"])

for name, ok, detail in checks:
    print(f"{'PASS' if ok else 'FAIL'} {name}" + (f": {detail}" if not ok else ""))
sys.exit(0 if all(ok for _, ok, _ in checks) else 1)
