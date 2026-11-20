"""Block gates and the bridge's translation declarations: what a callback sees and changes.

Run: panda-python tests/tbgate.py <dir>/busybox-<arch>.json [--case NAME]
(an image from tests/mkbusybox.py; the machine is tests/testmachine.py's)

Each case runs in a child process and runs tests/programs/blocks_<arch>.S (a loop whose block
"loop" runs N times, counting a register down, then two stores) through the guest agent:
- start, end: a start_block_exec (end_block_exec) callback sets the loop's counter register to K
  at the first entry (the end) of block "loop" in a run: the loop then runs exactly K times in all.
  The block calls the callback behind its gate, after which the block must use the register the
  callback wrote, not a copy of the old value. Run 1 translates the program with the callbacks
  enabled (the gate open: the call placed inline, on hosts that place it). After a flush, run 2
  translates it with them disabled (the gate closed: the call placed after the block's code), and
  run 3 runs those blocks with the callbacks enabled.
- mem: a block_translate callback registers a memory callback through QEMU's plugin API
  (qemu_plugin_register_vcpu_mem_cb) on the program's instructions. While no PANDA memory callback
  is registered, the bridge declares that it registers none and blocks are translated without
  memory instrumentation: the callback is ignored, with a warning, and never called. Once a PANDA
  memory callback is registered (disabled), blocks are translated with it again, and the raw
  callback sees the program's two stores.
"""
import argparse
import base64
import os
import subprocess
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "programs"))
from blocks_programs import PROGRAMS  # noqa: E402
from testlib import MAGIC, PROGRAM_BASE, PROGRAM_END, Checks, progress  # noqa: E402

PANDA_TEST = dict(area="engine", tier="guest", fixture="testmachine", arches=["x86_64", "aarch64"],
                  timeout=900)

N, K = 1000, 10
LABELS = ["setup", "loop", "odd", "even", "fault", "fault_store", "sys"]
COUNTER = {"x86_64": "RCX", "aarch64": "X9"}
CASES = ["start", "end", "mem"]
WARNING = "memory callback ignored"

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--case", choices=CASES)
opt = ap.parse_args()

if opt.case is None:
    failed = []
    for case in CASES:
        progress(f"case {case}")
        cmd = [sys.executable, os.path.abspath(__file__), opt.image, "--case", case]
        r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=600)
        print("\n".join(f"  [{case}] {line}" for line in r.stdout.splitlines()), flush=True)
        warned = WARNING in r.stdout
        if r.returncode or warned != (case == "mem"):
            if not r.returncode:
                print(f"  [{case}] FAIL the warning ({WARNING!r}) {'printed' if warned else 'missing'}")
            failed.append(case)
    print(f"TBGATE {'FAIL ' + ' '.join(failed) if failed else 'PASS'}")
    sys.exit(1 if failed else 0)

from testmachine import Agent, Machine  # noqa: E402

m = Machine(opt.image)
panda = m.panda()
arch = m.arch
C = Checks(arch)
agent = Agent(panda)
ffi, lib = panda.ffi, panda.libpanda
st = {"run": 0, "labels": {}, "asid": None, "written": Counter(), "loops": Counter(),
      "translated": Counter(), "raw": Counter(), "hc": [], "window": False}


def ours(cpu, pc):
    return PROGRAM_BASE <= pc < PROGRAM_END and panda.current_asid(cpu) == st["asid"]


def write_counter(cpu, tb):
    if panda.tb_pc(cpu, tb) == st["labels"].get("loop") and ours(cpu, panda.tb_pc(cpu, tb)) \
            and not st["written"][st["run"]]:
        panda.arch.set_reg(cpu, COUNTER[arch], K)
        st["written"][st["run"]] += 1


def on_start(cpu, tb):
    pc = panda.tb_pc(cpu, tb)
    if pc == st["labels"].get("loop") and ours(cpu, pc):
        if opt.case == "start":
            write_counter(cpu, tb)
        st["loops"][st["run"]] += 1


def on_end(cpu, tb):
    write_counter(cpu, tb)


def on_translate(cpu, pc):
    if pc == st["labels"].get("loop"):
        st["translated"][st["run"]] += 1


@ffi.callback("void(unsigned int, qemu_plugin_meminfo_t, uint64_t, void *)")
def raw_mem(vcpu, info, vaddr, udata):
    # the program's range is other processes' too: count the program's stores, from hypercall 1 to 2
    if st["window"] and panda.current_asid(panda.get_cpu()) == st["asid"]:
        st["raw"][st["run"]] += 1


def on_block_translate(cpu, tb):
    for i in range(lib.qemu_plugin_tb_n_insns(tb)):
        insn = lib.qemu_plugin_tb_get_insn(tb, i)
        if PROGRAM_BASE <= lib.qemu_plugin_insn_vaddr(insn) < PROGRAM_END:
            lib.qemu_plugin_register_vcpu_mem_cb(insn, raw_mem, lib.QEMU_PLUGIN_CB_NO_REGS,
                                                 lib.QEMU_PLUGIN_MEM_W, ffi.NULL)


def on_after_write(cpu, pc, addr, size, buf):
    pass


if opt.case in ("start", "end"):
    panda.cb_start_block_exec(name="s", enabled=False)(on_start)
    if opt.case == "end":
        panda.cb_end_block_exec(name="e", enabled=False)(on_end)
    panda.cb_before_block_translate(name="bbt")(on_translate)
else:
    panda.cb_block_translate(name="bt")(on_block_translate)


@panda.hypercall(MAGIC, observe_only=True)
def hypercall(cpu):
    op = panda.arch.get_arg(cpu, 2, convention="syscall")
    st["hc"].append((st["run"], op))
    if op == 1:
        table = panda.arch.get_arg(cpu, 3, convention="syscall")
        raw = bytes(panda.virtual_memory_read(cpu, table, 8 * len(LABELS)))
        st["labels"] = {k: int.from_bytes(raw[8 * i:8 * i + 8], "little") for i, k in enumerate(LABELS)}
        st["asid"] = panda.current_asid(cpu)
    st["window"] = op == 1
    if opt.case in ("start", "end") and st["run"] in (1, 3):
        # on from hypercall 1 to hypercall 2 (the program's blocks run in between)
        for name in ["s", "e"] if opt.case == "end" else ["s"]:
            (panda.enable_callback if op == 1 else panda.disable_callback)(name)


out = {}


@panda.queue_blocking
def drive():
    try:
        m.revert(panda, agent)
        agent.put("/tmp/blocks", base64.b64decode(PROGRAMS[arch]))
        out["runs"] = {}
        for run in range(1, 4 if opt.case != "mem" else 3):
            st["run"] = run
            if opt.case != "mem" and run == 2:
                panda.flush_tb()
            if opt.case == "mem" and run == 2:
                panda.cb_virt_mem_after_write(name="w", enabled=False)(on_after_write)
            out["runs"][run] = agent.run("/tmp/blocks; echo rc=$?", timeout=300)
            agent.run("true")
            progress(f"run {run}: {out['runs'][run][1].strip()!r}, loop entries {st['loops'][run]}, "
                     f"loop translated {st['translated'][run]}, raw store calls {st['raw'][run]}")
    except Exception as e:
        out["error"] = repr(e)
    panda.end_analysis()


panda.run()

runs = out.get("runs", {})
C.check("no error in the driver", "error" not in out, out.get("error"))
C.check("the program ran each time, exit 0",
        runs and all(rc == 0 and b"rc=0" in o for rc, o in runs.values()),
        {r: o[-120:] for r, (rc, o) in runs.items()})
if opt.case in ("start", "end"):
    C.check("runs 1 and 2 translate the loop (callbacks enabled, then disabled), run 3 does not",
            st["translated"][1] >= 1 and st["translated"][2] >= 1 and st["translated"][3] == 0,
            dict(st["translated"]))
    C.check("the callback wrote the counter once in runs 1 and 3, never in run 2",
            st["written"][1] == 1 and st["written"][3] == 1 and not st["written"][2], dict(st["written"]))
    C.check(f"the loop ran K={K} times in all, not N={N}: the block used the written register",
            st["loops"][1] == K and st["loops"][3] == K and not st["loops"][2], dict(st["loops"]))
else:
    C.check("no PANDA memory callback: the raw one is never called", st["raw"][1] == 0, dict(st["raw"]))
    C.check("a PANDA memory callback registered: the raw one sees the program's 2 stores",
            st["raw"][2] == 2, dict(st["raw"]))
sys.exit(C.report(f"TBGATE {opt.case} arch={arch} loops={dict(st['loops'])} raw={dict(st['raw'])}"))
