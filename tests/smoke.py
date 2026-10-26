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
# syscalls2 has tables for x86, arm, mips and ppc.
plugins = ["cskeleton"] + (["syscalls2"] if arch.startswith(("x86_64", "i386", "arm", "aarch64", "mips", "ppc")) else [])
for p in plugins:
    panda.load_plugin(p)
blocks = 0
loaded = []
@panda.cb_before_block_exec
def bbe(cpu, tb):
    global blocks, loaded
    blocks += 1
    if blocks == 20000:
        # Before end_analysis: it queues the plugins' unload for the next main-loop turn, which
        # may or may not come before the run stops.
        loaded = [p for p in plugins if panda.libpanda.panda_get_plugin_by_name(p.encode()) != panda.ffi.NULL]
        panda.end_analysis()
panda.run()
print(f"PANDA_SMOKE arch={arch} blocks={blocks} plugins={loaded}")
assert blocks >= 20000 and loaded == plugins
