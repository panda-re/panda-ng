"""Cost of a callback on panda-ng, per invocation: in C (tests/plugins/cbcount), as a bare cffi
callback (Panda.register_callback, no wrapper) and through pandare2's decorator
(@panda.cb_<name>), in one process.

Run: PANDA_PLUGIN_DIR=$(nix build .#test-plugins --print-out-paths)/lib/panda/plugins \\
         panda-python tests/bench_cb.py <dir>/debian-13-x86_64.json

From the "root" snapshot, a CPU-bound shell loop runs in the guest once per phase (REPEAT times,
the fastest kept); each phase times the serial command and counts callback invocations.
ns/call = (phase - baseline) / calls, against a run with no callback (with memory callbacks on,
for virt_mem_after_read). Guest time comes from -icount, so slow callbacks can't add timer
ticks: every phase runs the same guest instructions. Without cbcount in the plugin directory the
C phases are skipped. Not a pass/fail test: on a loaded host compare the ratios, not the times.
"""
import os
import sys
import time
from os.path import join, isfile

from cffi import FFI
from pandare2 import Panda

img = sys.argv[1]
N_BLOCK = int(os.environ.get("N_BLOCK", "2000"))
N_MEM = int(os.environ.get("N_MEM", "200"))
REPEAT = int(os.environ.get("REPEAT", "2"))
panda = Panda(generic=img, extra_args=["-icount", "shift=6,sleep=off"])
loop = "i=0; while [ $i -lt {n} ]; do i=$((i+1)); done; echo done"
res = {}
cnt = [0]

so = join(panda.get_plugin_path(), f"libpanda-cbcount_{panda.arch_name}-softmmu.so")
cb = None
if isfile(so):
    panda.load_plugin("cbcount")
    cffi = FFI()
    cffi.cdef("uint64_t cbcount_get(int); void cbcount_reset(void); void cbcount_enable(int, int);")
    cb = cffi.dlopen(so)
else:
    print(f"no {so}: skipping the C phases (set PANDA_PLUGIN_DIR, see the docstring)", flush=True)


def timed(tag, n, calls=lambda: cnt[0]):
    best = None
    for _ in range(REPEAT):
        panda.revert_sync("root")
        cnt[0] = 0
        if cb:
            cb.cbcount_reset()
        t = time.perf_counter()
        out = panda.run_serial_cmd(loop.format(n=n), timeout=600)
        dt = time.perf_counter() - t
        assert "done" in out, out
        if best is None or dt < best[0]:
            best = (dt, calls())
    res[tag] = best
    print(f"phase {tag:28} n={n:6} {best[0]:8.3f} s  calls={best[1]:>11,}", flush=True)


def bare_block(cpu, tb):
    cnt[0] += 1


def bare_mem(cpu, pc, addr, size, buf):
    cnt[0] += 1


@panda.queue_blocking
def drive():
    timed("baseline", N_BLOCK)

    if cb:
        cb.cbcount_enable(0, 1)
        timed("bbe C", N_BLOCK, lambda: cb.cbcount_get(0))
        cb.cbcount_enable(0, 0)

    panda.register_callback(panda.callback.before_block_exec,
                            panda.callback.before_block_exec(bare_block), "raw_bbe")
    timed("bbe bare cffi", N_BLOCK)
    panda.delete_callback("raw_bbe")

    @panda.cb_before_block_exec(name="deco_bbe")
    def deco_bbe(cpu, tb):
        cnt[0] += 1
    timed("bbe decorator", N_BLOCK)
    panda.delete_callback("deco_bbe")

    panda.enable_memcb()
    timed("memcb on, no cb", N_MEM)

    if cb:
        cb.cbcount_enable(1, 1)
        timed("vmar C", N_MEM, lambda: cb.cbcount_get(1))
        cb.cbcount_enable(1, 0)

    panda.register_callback(panda.callback.virt_mem_after_read,
                            panda.callback.virt_mem_after_read(bare_mem), "raw_vmar")
    timed("vmar bare cffi", N_MEM)
    panda.delete_callback("raw_vmar")

    @panda.cb_virt_mem_after_read(name="deco_vmar")
    def deco_vmar(cpu, pc, addr, size, buf):
        cnt[0] += 1
    timed("vmar decorator", N_MEM)
    panda.delete_callback("deco_vmar")
    panda.end_analysis()


panda.run()
b = res["baseline"][0]
m = res["memcb on, no cb"][0]
ns = {}
for tag, base in [("bbe C", b), ("bbe bare cffi", b), ("bbe decorator", b),
                  ("vmar C", m), ("vmar bare cffi", m), ("vmar decorator", m)]:
    if tag not in res:
        continue
    dt, c = res[tag]
    ns[tag] = (dt - base) / max(c, 1) * 1e9
    print(f"{tag:16} {ns[tag]:8.1f} ns/call over {c:,} calls (x{dt / base:.2f} the run time)")
for kind in ("bbe", "vmar"):
    d, bare, c = ns.get(f"{kind} decorator"), ns.get(f"{kind} bare cffi"), ns.get(f"{kind} C")
    line = f"{kind}: decorator/bare {d / bare:.2f}, wrapper {d - bare:+.0f} ns"
    if c:
        line += f"; bare/C {bare / c:.1f}, decorator/C {d / c:.1f}"
    print(line)
sys.stdout.flush()
os._exit(0)
