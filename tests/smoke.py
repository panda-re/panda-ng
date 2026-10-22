"""Runtime smoke: libpanda boots the firmware, C plugins load, a Python callback runs.

Run: panda-python tests/smoke.py <arch>
"""
import sys
from pandare2 import Panda
arch = sys.argv[1] if len(sys.argv) > 1 else "x86_64"
bits = 64 if "64" in arch else 32
machine = {"aarch64": ["-M", "virt", "-cpu", "max"], "arm": ["-M", "virt"],
           "riscv32": ["-M", "virt"], "riscv64": ["-M", "virt"], "loongarch64": ["-M", "virt"],
           "ppc64": ["-M", "pseries"]}.get(arch, [])
panda = Panda(arch=arch, mem="256M", os_version=f"linux-{bits}-generic",
              extra_args=machine + ["-display", "none", "-nodefaults"])
# syscalls2 has tables for x86, arm and mips only.
plugins = ["cskeleton"] + (["syscalls2"] if arch.startswith(("x86_64", "i386", "arm", "aarch64", "mips")) else [])
for p in plugins:
    panda.load_plugin(p)
blocks = 0
@panda.cb_before_block_exec
def bbe(cpu, tb):
    global blocks
    blocks += 1
    if blocks == 20000:
        panda.end_analysis()
panda.run()
loaded = [p for p in plugins if panda.libpanda.panda_get_plugin_by_name(p.encode()) != panda.ffi.NULL]
print(f"PANDA_SMOKE arch={arch} blocks={blocks} plugins={loaded}")
assert blocks >= 20000 and loaded == plugins
