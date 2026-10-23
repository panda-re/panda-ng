"""pandare2.MachineBuilder without an engine run: what it lowers to and what it refuses.

Run: panda-python tests/machine_builder.py
"""
import os
import sys

from pandare2 import MachineBuilder, Panda

checks = []


def check(name, ok, detail=""):
    checks.append((name, bool(ok), detail))


def raises(fn):
    try:
        fn()
    except ValueError as e:
        return str(e)
    return None


m = MachineBuilder("arm").cpu("cortex-a15").ram(0x40000000, "16M")
check("ram(): -m in KiB, exact", m.mem == "16384K", m.mem)
err = raises(lambda: MachineBuilder("arm").ram(0x40000000, 0x1200))
check("ram(): a size that isn't a multiple of 8 KiB is refused (was -m 4K)", err and "8 KiB" in err, err)
check("ram(): size 0 is refused", raises(lambda: MachineBuilder("arm").ram(0, 0)))
m = MachineBuilder("arm").cpu("cortex-a15").rom("boot", 0, 0x1000, file="rel/boot.bin").kernel("rel/k.elf")
arg = " ".join(m.args())
check("region file= is absolute", f"region.0.file={os.path.abspath('rel/boot.bin')}" in arg, arg)
check("kernel() is absolute", f"-kernel {os.path.abspath('rel/k.elf')}" in arg, arg)
check("no ram(): mem is None (Panda passes -m 0)", m.mem is None and not m.is_board)
check("board(): is_board", MachineBuilder("arm").board("/x.so", "b").is_board)
err = raises(lambda: Panda(arch="aarch64", machine=MachineBuilder("arm").cpu("cortex-a15")))
check("Panda(arch=) must match MachineBuilder(arch)", err and "MachineBuilder('arm')" in err, err)

failed = False
for name, ok, detail in checks:
    print(f"  {'PASS' if ok else 'FAIL'} {name}" + ("" if ok else f": {detail}"))
    failed |= not ok
sys.exit(1 if failed else 0)
