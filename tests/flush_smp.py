"""A TB flush requested from one vCPU while another vCPU's request is still pending.

Run: panda-python tests/flush_smp.py <dir>/busybox-<arch>.json   (an image from tests/mkbusybox.py)

The image is booted afresh with two vCPUs (its snapshot has one). tests/programs/flushsmp_<arch>.S
runs pinned to each vCPU: a loop of a hypercall and the block after it ("after", whose second
instruction is "mark"). insn_translate asks for insn_exec at the addresses in a set that starts as
{after}. Once both loops have run a while:
- A, vCPU 0: insn_exec at "after" calls flush_tb(), then stays in the callback until vCPU 1 makes
  its next hypercall, or WAIT seconds. Until it returns, vCPU 0 has not left its block, so the
  flush A asked for has not run.
- B, vCPU 1, at its second hypercall after that (so that its blocks were translated after the last
  flush): adds "mark" to the set and calls flush_tb(), while A's flush is pending.
Checks:
- B made its request while A's was pending (the case under test);
- B ran no more of the program before the flush: its next hypercall comes after A's callback
  returned (B waited for the flush; without it, B goes on at once with its old blocks);
- B's next block is translated again: insn_exec fires at "mark" on vCPU 1 before B's next hypercall;
- A's blocks are translated again too: insn_exec fires at "mark" on vCPU 0 after A's callback.
No per-block Python callback is used: with two vCPUs calling into Python on every block, the guest
stalls.
"""
import sys
import threading
import time

from testlib import Checks, MAGIC, hypercall_args, progress, upload
from pandare2 import Panda

PANDA_TEST = dict(area="engine", tier="guest", fixture="busybox", arches=["x86_64", "aarch64", "riscv64"],
                  plugins=["hypercaller"], timeout=600)

EXIT_REG = {"x86_64": "R15", "aarch64": "X25", "riscv64": "S6"}
WARM = 50       # hypercalls on each vCPU before the test starts
WAIT = 5.0      # seconds A stays in its callback

panda = Panda(generic=sys.argv[1], extra_args=["-smp", "2"])
arch = panda.arch_name
C = Checks(arch)

st = dict(phase="off", ticks=[0, 0], after=None, mark=None, a_req=False, a_waiting=False, a_waited=None,
          a_calls=0, b_calls=0, b_req_during_a=None, b_next=None, a_next=None, stop=[False, False])
instrumented = set()
ie_log = []     # (vCPU, pc) of insn_exec
release = threading.Event()


@panda.cb_insn_translate(name="it", enabled=False)
def it(cpu, pc):
    return pc in instrumented


@panda.cb_insn_exec(name="ie", enabled=False)
def ie(cpu, pc):
    ie_log.append((cpu.cpu_index, pc))
    if st["phase"] != "arm" or cpu.cpu_index != 0 or pc != st["after"]:
        return 0
    # A
    st["phase"] = "a"
    st["a_waiting"] = True
    panda.flush_tb()
    st["a_req"] = True
    t = time.time()
    release.wait(WAIT)
    st["a_waited"] = time.time() - t
    st["a_mark"] = len(ie_log)
    st["a_waiting"] = False
    return 0


def stop(cpu, i):
    st["stop"][i] = True
    panda.arch.set_reg(cpu, EXIT_REG[arch], 1)


@panda.hypercall(MAGIC)
def hypercall(cpu):
    i = cpu.cpu_index
    if i > 1:
        return
    _, st["after"], st["mark"], _ = hypercall_args(panda, cpu)
    if st["stop"][i]:
        stop(cpu, i)
    elif st["phase"] == "warm":
        st["ticks"][i] += 1
        if i == 0 and min(st["ticks"]) >= WARM:
            st["phase"] = "arm"
            instrumented.add(st["after"])
            panda.enable_callback("it")
            panda.enable_callback("ie")
            panda.flush_tb()    # vCPU 0's own request: done before its next block, "after"
    elif st["phase"] == "a" and i == 1:
        st["b_calls"] += 1
        if st["b_calls"] == 2:
            # B
            st["b_req_during_a"] = st["a_waiting"] and st["a_req"]
            st["b_mark"] = len(ie_log)
            instrumented.add(st["mark"])
            panda.flush_tb()
        elif st["b_calls"] == 3:
            st["b_next"] = dict(a_waiting=st["a_waiting"],
                                marks=sum(1 for c, pc in ie_log[st["b_mark"]:] if c == 1 and pc == st["mark"]))
            release.set()
            stop(cpu, 1)
    elif st["phase"] == "a" and i == 0 and not st["a_waiting"]:
        # A's next hypercall ends the block A waited in; the one after that follows a whole new "after"
        st["a_calls"] += 1
        if st["a_calls"] == 2:
            st["a_next"] = sum(1 for c, pc in ie_log[st["a_mark"]:] if c == 0 and pc == st["mark"])
            stop(cpu, 0)


out = {}


@panda.queue_blocking
def drive():
    panda.serial_console.expect(timeout=300)
    out["nproc"] = panda.run_serial_cmd("nproc").strip()
    upload(panda, "flushsmp", arch)
    st["phase"] = "warm"
    progress("running the program on vCPUs 0 and 1")
    out["run"] = panda.run_serial_cmd("taskset -c 0 /tmp/flushsmp & taskset -c 1 /tmp/flushsmp & wait; echo done",
                                      timeout=300)
    panda.end_analysis()


panda.run()

C.check("the guest has two CPUs", out.get("nproc", "").endswith("2"), out.get("nproc"))
C.check("both copies of the program ran and were stopped", out.get("run", "").strip().endswith("done"),
        out.get("run"))
C.check("A (vCPU 0) asked for a flush, and stayed in its callback", st["a_req"] and st["a_waited"] is not None,
        st["a_waited"])
C.check("B (vCPU 1) asked for a flush while A's was pending", st["b_req_during_a"], st["b_req_during_a"])
bn = st["b_next"] or {}
C.check("B ran no more of the program until the flush: its next hypercall came after A's callback returned",
        bn and not bn["a_waiting"], "B's next hypercall came while A was in its callback" if bn else "no next hypercall")
C.check("B's next block is translated again: insn_exec fires at mark on vCPU 1 before its next hypercall",
        bn.get("marks"), bn)
C.check("A's blocks are translated again: insn_exec fires at mark on vCPU 0 after its callback",
        st["a_next"], st["a_next"])
sys.exit(C.report(f"flush_smp {arch}"))
