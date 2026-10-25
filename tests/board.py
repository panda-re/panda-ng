"""An external board .so under pandare2: MachineBuilder.board(path, name) -> -M path:name.

Run: panda-python tests/board.py <at91sam9260.so>

The board is the at91sam9260 from rehosting/qemu_builder's at91sam9260-board branch, built outside the engine with
the engine's SDK (panda-board-cc, or the flake's lib.<system>.mkBoard). tests/programs/cfg_at91.S
prints the DBGU chip id, waits for five PIT ticks through the AIC, and makes hypercall (MAGIC, 1,
chip id, ticks). Checks the standard callbacks on the external board: after_machine_init,
before_block_exec/insn_exec in the program, the hypercall's arguments, and a guest memory read.
"""
import base64
import os
import sys
import tempfile
import threading
from collections import Counter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs"))
from cfg_programs import PROGRAMS  # noqa: E402
from pandare2 import Panda, MachineBuilder  # noqa: E402

MAGIC = 0x50616e64
BASE = 0x20008000
CIDR_AT91SAM9260 = 0x019803a0

tmp = tempfile.mkdtemp(prefix="board-")
elf = os.path.join(tmp, "at91.elf")
with open(elf, "wb") as f:
    f.write(base64.b64decode(PROGRAMS["at91"]))
m = MachineBuilder("arm").board(sys.argv[1], "at91sam9260").kernel(elf)
print("args:", " ".join(m.args()), flush=True)
panda = Panda(arch="arm", mem="64M", machine=m,
              extra_args=["-display", "none", "-serial", "none", "-icount", "shift=0,sleep=off"])

checks, counts, seen = {}, Counter(), {}


def check(name, ok, detail=""):
    checks[name] = (bool(ok), detail)


def in_program(pc):
    return BASE <= pc < BASE + 0x1000


@panda.cb_after_machine_init
def ami(cpu):
    counts["after_machine_init"] += 1
    seen["pc"] = panda.arch.get_pc(cpu)


@panda.cb_before_block_exec(name="b_bbe")
def bbe(cpu, tb):
    if in_program(tb.pc):
        counts["before_block_exec"] += 1


@panda.cb_insn_translate(name="b_it")
def it(cpu, pc):
    return in_program(pc)


@panda.cb_insn_exec(name="b_ie")
def ie(cpu, pc):
    counts["insn_exec"] += 1
    return 0


@panda.hypercall(MAGIC)
def hc(cpu):
    counts["guest_hypercall"] += 1
    _, op, cidr, ticks, _ = [panda.arch.get_arg(cpu, i, convention="syscall") for i in range(1, 6)]
    check("hypercall: the board's DBGU chip id", cidr == CIDR_AT91SAM9260, f"{cidr:#x}")
    check("hypercall: five PIT ticks through the board's AIC", ticks == 5, ticks)
    v = bytes(panda.virtual_memory_read(cpu, BASE, 16))
    p = bytes(panda.physical_memory_read(BASE, 16))
    check("virtual and physical reads of the board's SDRAM agree (MMU off), the program is there",
          v == p and v in base64.b64decode(PROGRAMS["at91"]), v.hex())
    for name in ("b_bbe", "b_it", "b_ie"):   # see tests/machine_sw.py
        panda.disable_callback(name)
    panda.end_analysis()


timer = threading.Timer(120, lambda: (print("TIMEOUT", flush=True), os._exit(2)))
timer.start()
panda.run()
timer.cancel()

check("after_machine_init fires, pc at the ELF entry", counts["after_machine_init"] == 1 and seen.get("pc") == BASE,
      f"{counts['after_machine_init']} pc={seen.get('pc', 0):#x}")
check("before_block_exec sees the program", counts["before_block_exec"] > 5, counts["before_block_exec"])
check("insn_exec sees the program", counts["insn_exec"] > 5, counts["insn_exec"])
check("guest_hypercall: one", counts["guest_hypercall"] == 1, counts["guest_hypercall"])
print(f"BOARD counts={dict(counts)}")
failed = False
for name, (ok, detail) in checks.items():
    print(f"  {'PASS' if ok else 'FAIL'} {name}" + ("" if ok else f": {detail}"))
    failed |= not ok
sys.exit(1 if failed else 0)
