"""The engine's system-call pairing is pay for use: without a declaration, no translated block calls
it; with one, the trap and return-to-user sites do.

Run: panda-python tests/syscall_pair_tcg.py <images>/busybox-<arch>.json [--declared]
(busybox-x86_64 and -aarch64, images from tests/mkbusybox.py; tests/testmachine.py's machine).

The guest runs a few shell commands (each a fork, an exec and their system calls) with -d op, so
that every block translated meanwhile is logged with its TCG ops. Checks:
- the trap and return instructions were translated (their helpers are in the ops: x86_64 syscall
  and sysret or iret_protected; aarch64 the SVC's exception and exception_return), so the sites
  ran through translation;
- without a declaration (nothing loaded): no op calls the pairing's helpers (panda_sc_*);
- --declared (syscalls2, which declares the Linux ABIs, with on_all_sys_return): the entry helper
  (panda_sc_enter*) is called from translated code, and so is the return check's
  (panda_sc_return*), which only runs when the returning task's set holds a call.
A comparison of the same run's ops on an engine without the pairing is in the pairing's
measurements (docs/design/syscall-pairing.md), not here: it needs two builds.
"""
import argparse
import os
import re
import sys
import tempfile

from testlib import Checks, progress
from testmachine import Agent, Machine

PANDA_TEST = dict(area="syscalls", tier="guest", fixture="testmachine",
                  arches=["x86_64", "aarch64", "arm", "i386", "riscv64", "mipsel", "mips", "mips64el",
                          "mips64", "ppc", "ppc64"],
                  plugins=["syscalls2"], variants=dict(declared=[False, True]), timeout=600)

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--declared", action="store_true")
opt = ap.parse_args()

machine = Machine(opt.image, smp=1)
arch = machine.arch
log = os.path.join(tempfile.mkdtemp(prefix="sc-tcg-"), "op.log")
panda = machine.panda(extra_args=["-d", "op", "-D", log])
agent = Agent(panda)
C = Checks(arch)
R = {"returns": 0}

if opt.declared:
    panda.load_plugin("syscalls2")

    @panda.ppp("syscalls2", "on_all_sys_return")
    def ret(cpu, pc, callno):
        R["returns"] += 1


@panda.queue_blocking
def drive():
    machine.revert(panda, agent)
    progress("shell commands")
    R["out"] = agent.run("for i in 1 2 3; do ls / > /dev/null; cat /proc/version > /dev/null; done; "
                         "echo done")[1]
    panda.end_analysis()


panda.run()

text = open(log, errors="replace").read()
os.unlink(log)
helpers = re.findall(r"\bcall (panda_sc_\w+)", text)
# the trap's and the return's helpers in the ops (an SVC raises its exception through a helper)
TRAP = {"x86_64": r"\bcall syscall,", "aarch64": r"\bcall exception_with_syndrome",
        "arm": r"\bcall exception_with_syndrome", "i386": r"\bcall (raise_interrupt|sysenter),",
        "riscv64": r"\bcall raise_exception,", "ppc": r"\bcall raise_exception_err,",
        "ppc64": r"\bcall raise_exception_err,"}.get(arch, r"\bcall raise_exception_err,")
RET = {"x86_64": r"\bcall (sysret|iret_protected),", "aarch64": r"\bcall exception_return,",
       "arm": r"\bcall cpsr_write_eret,", "i386": r"\bcall (sysexit|iret_protected),",
       "riscv64": r"\bcall sret,", "ppc": r"\bcall rfi,", "ppc64": r"\bcall rfid,"}.get(
    arch, r"\bcall eret,")
C.check("the guest ran its commands", b"done" in R.get("out", b""), R.get("out"))
C.check("trap and return-to-user instructions were translated",
        bool(re.search(TRAP, text) and re.search(RET, text)))
if opt.declared:
    if arch != "arm":                   # an AArch32 kernel's SVC enters from interrupt delivery
        C.check("the trap sites call the pairing's entry helper",
                any(h.startswith("panda_sc_enter") for h in helpers), sorted(set(helpers)))
    C.check("the return sites call the pairing's return check",
            any(h.startswith("panda_sc_return") for h in helpers), sorted(set(helpers)))
    C.check("syscalls2 delivered returns", R["returns"] > 0, R["returns"])
else:
    C.check("no translated block calls the pairing (nothing declared)", not helpers,
            sorted(set(helpers)))
sys.exit(C.report(f"SYSCALL_PAIR_TCG arch={arch} declared={opt.declared} helper_calls={len(helpers)}"))
