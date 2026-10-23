"""Boot an arch's firmware under PANDA with the out-of-tree blockcount plugin loaded, stop after
N blocks seen from Python, and check that the plugin counted them too.

Run: count-blocks [arch] [blocks]   (the flake's wrapper sets PANDA_PLUGIN_DIR)
"""
import sys
from os.path import join

from cffi import FFI
from pandare2 import Panda

arch = sys.argv[1] if len(sys.argv) > 1 else "x86_64"
want = int(sys.argv[2]) if len(sys.argv) > 2 else 20000
machine = {"aarch64": ["-M", "virt", "-cpu", "max"], "riscv64": ["-M", "virt"]}.get(arch, [])
bits = 64 if "64" in arch else 32
panda = Panda(arch=arch, mem="256M", os_version=f"linux-{bits}-generic", extra_args=machine + ["-display", "none", "-nodefaults"])
panda.load_plugin("blockcount")
# The plugin's own API: a second handle on the library PANDA loaded (same counter).
ffi = FFI()
ffi.cdef("uint64_t blockcount_get(void);")
blockcount = ffi.dlopen(join(panda.get_plugin_path(), f"libpanda-blockcount_{arch}-softmmu.so"))

seen = 0


@panda.cb_before_block_exec
def bbe(cpu, tb):
    global seen
    seen += 1
    if seen == want:
        panda.end_analysis()


panda.run()
counted = blockcount.blockcount_get()
print(f"COUNT arch={arch} python={seen} plugin={counted}")
assert seen >= want and counted >= want
