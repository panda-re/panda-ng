"""SmallWorld's way of using the configurable machine, unchanged: `-M configurable -cpu X`, no memory
map, memory mapped and code written from Python (panda.map_memory, physical_memory_write) in
after_machine_init, pc set there, then run. (smallworld/emulators/panda/panda.py does this.)

Run: panda-python tests/machine_sw.py <arch> [<arch> ...]   (aarch64, riscv64)
"""
import os
import struct
import subprocess
import sys
import threading

MAGIC = 0x50616e64
BASE = 0x1000_0000
# MAGIC in x0/a0 and x8/a7, 7 in x1/a1, the hypercall, then a branch to itself.
CODE = {
    "aarch64": ("cortex-a53", [0xd28dcc80, 0xf2aa0c20, 0xaa0003e8, 0xd28000e1, 0xd500501f, 0x14000000]),
    "riscv64": ("rv64", [0x50617537, 0xe6450513, 0x00050893, 0x00700593, 0x00004013, 0x0000006f]),
}


def run(arch):
    from pandare2 import Panda

    cpu_type, words = CODE[arch]
    # --8<-- [start:panda]
    panda = Panda(arch=arch, extra_args=["-M", "configurable", "-cpu", cpu_type, "-display", "none",
                                         "-serial", "none"])
    # --8<-- [end:panda]
    seen = {"insns": set()}

    # --8<-- [start:setup]
    @panda.cb_after_machine_init
    def setup(cpu):
        panda.map_memory("code", 0x10000, BASE)
        panda.physical_memory_write(BASE, struct.pack(f"<{len(words)}I", *words))
        panda.arch.set_pc(cpu, BASE)
    # --8<-- [end:setup]

    @panda.cb_insn_translate(name="sw_insn_translate")
    def it(cpu, pc):
        return BASE <= pc < BASE + 0x100

    @panda.cb_insn_exec(name="sw_insn_exec")
    def ie(cpu, pc):
        seen["insns"].add(pc)
        return 0

    # --8<-- [start:hypercall]
    @panda.hypercall(MAGIC)
    def hc(cpu):
        seen["arg"] = panda.arch.get_arg(cpu, 2, convention="syscall")
        seen["pc"] = panda.arch.get_pc(cpu)
        # Off before ending: the program spins on, and a vCPU still running insn_exec while the
        # main thread tears the callback lists down segfaults (panda_cb_list_next, REPORT.md).
        panda.disable_callback("sw_insn_exec")
        panda.disable_callback("sw_insn_translate")
        panda.end_analysis()
    # --8<-- [end:hypercall]

    timer = threading.Timer(60, lambda: (print("TIMEOUT", flush=True), os._exit(2)))
    timer.start()
    panda.run()
    timer.cancel()
    checks = {
        "the hypercall ran with the program's argument": seen.get("arg") == 7,
        # The TB's start, not the hypercall's own pc: the precise-pc gap tests/api.py lists.
        "pc in the program": BASE <= seen.get("pc", 0) <= BASE + 16,
        "insn_exec saw the program's instructions": {BASE + 4 * i for i in range(5)} <= seen["insns"],
    }
    for name, ok in checks.items():
        print(f"  {'PASS' if ok else 'FAIL'} {name}" + ("" if ok else f": {seen}"))
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    if len(sys.argv) == 2:
        sys.exit(run(sys.argv[1]))
    rc = 0
    for a in sys.argv[1:] or list(CODE):
        r = subprocess.run([sys.executable, os.path.abspath(__file__), a])
        print(f"== {a}: {'PASS' if r.returncode == 0 else 'FAIL'} (rc={r.returncode})", flush=True)
        rc |= r.returncode
    sys.exit(rc)
