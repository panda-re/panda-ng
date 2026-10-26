"""Every PANDA callback type: registers, and fires (or is a known gap) on a real guest.

Run: panda-python tests/callbacks.py <dir>/debian-13-x86_64.json

Registers a counting callback for every type pandare2 knows (panda_cb's members), except those
whose registration aborts (no trampoline: before_loadvm, replay_*), and except the per-block,
per-instruction and memory ones that tests/api.py, blocks.py and mem.py check against ground
truth (registering those here would only slow the guest). Then drives the guest through what
each callback is about: commands on the serial console, disk reads and writes (O_DIRECT),
unassigned I/O ports (x86_64, /dev/port), a monitor command, a snapshot taken, reverted to and
deleted with pandare2's snap/revert_sync/delvm_sync (and snap/delvm), and the end of the analysis.

Each type that has a call site must fire; each one that has none is a GAP (and fails the run if it
starts firing, so the list stays true). Also checked, from inside the first asid_changed after
the revert (kernel mode): in_kernel / in_kernel_mode / in_kernel_code_linux, a kernel read
(linux_banner), current_ksp is a kernel address; and pandare2 helpers: run_monitor_cmd output,
the snapshot round trip, type_serial_cmd + finish_serial_cmd.
"""
import json
import lzma
import os
import sys
from collections import Counter

from testlib import Checks, progress
from pandare2 import Panda

panda = Panda(generic=sys.argv[1])
arch = panda.arch_name
ffi = panda.ffi
image_dir = os.path.dirname(os.path.abspath(sys.argv[1]))
with open(sys.argv[1]) as f:
    image = json.load(f)
with lzma.open(os.path.join(image_dir, image["isf"])) as f:
    banner_addr = json.load(f)["symbols"]["linux_banner"]["address"]

ABORTS = {"before_loadvm", "replay_hd_transfer", "replay_net_transfer", "replay_serial_receive",
          "replay_serial_read", "replay_serial_send", "replay_serial_write", "replay_before_dma",
          "replay_after_dma", "replay_handle_packet"}
COVERED_ELSEWHERE = {"before_block_exec", "after_block_exec", "start_block_exec", "end_block_exec",
                     "before_block_exec_invalidate_opt", "insn_exec",
                     "virt_mem_before_read", "virt_mem_before_write", "phys_mem_before_read",
                     "phys_mem_before_write", "virt_mem_after_read", "virt_mem_after_write",
                     "phys_mem_after_read", "phys_mem_after_write", "guest_hypercall"}
NO_CALL_SITE = {
    "before_tcg_codegen": "no call site", "after_insn_translate": "no call site",
    "after_insn_exec": "after_insn_translate, which selects its instructions, has none",
    "mmio_after_read": "commented out in the TCG plugin bridge", "mmio_before_write": "no call site",
    "hd_read": "no call site", "hd_write": "no call site", "monitor": "no call site", "qmp": "no call site",
    "cpu_restore_state": "no call site", "after_cpu_exec_enter": "commented out in wrap_ops.c",
    "before_cpu_exec_exit": "commented out in wrap_ops.c", "top_loop": "no call site",
    "during_machine_init": "no call site", "pre_shutdown": "no call site",
    "unassigned_io_read": "no call site", "unassigned_io_write": "no call site",
    "before_handle_interrupt": "the cpu_exec_interrupt wrapper is commented out in wrap_ops.c",
}
EXERCISED_ONLY_ON = {"unassigned_io_read": {"x86_64"}, "unassigned_io_write": {"x86_64"}}
C = Checks(arch, {f"{n} fires": why for n, why in NO_CALL_SITE.items()})

counts = Counter()
S = {"kernel": None}
fields = dict(ffi.typeof("panda_cb").fields)


def read_or_error(cpu, addr, n):
    try:
        return bytes(panda.virtual_memory_read(cpu, addr, n))
    except ValueError as e:
        return repr(e)


def make(name):
    ftype = fields[name].type
    ftype = ftype.item if ftype.kind == "pointer" else ftype
    result = ftype.result.cname
    passthrough = name in ("before_handle_exception", "before_handle_interrupt")

    def fn(*args):
        counts[name] += 1
        if name == "asid_changed" and S["kernel"] is None:
            cpu = args[0]
            S["kernel"] = {
                "in_kernel": bool(panda.in_kernel(cpu)), "in_kernel_mode": bool(panda.in_kernel_mode(cpu)),
                "in_kernel_code_linux": bool(panda.in_kernel_code_linux(cpu)),
                "banner": read_or_error(cpu, banner_addr, 14),
                "ksp": panda.libpanda.panda_current_ksp(cpu), "pc": panda.arch.get_pc(cpu)}
        if passthrough:
            return args[1]
        if result == "void":
            return None
        return 0
    return fn


registered = []
for name in panda.callback._fields:
    if name in ABORTS or name in COVERED_ELSEWHERE:
        continue
    getattr(panda, "cb_" + name)(name=name)(make(name))
    registered.append(name)
progress(f"registered {len(registered)} callback types")


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    S["kernel"] = None          # take the first asid_changed after the revert
    progress("commands")
    S["cat"] = panda.run_serial_cmd("cat /etc/os-release | head -1")
    disk = "$(ls /dev/vda /dev/sda 2>/dev/null | head -1)"
    progress("disk reads and writes")
    S["dd_read"] = panda.run_serial_cmd(f"dd if={disk} of=/dev/null bs=64k count=16 skip=2000 iflag=direct 2>&1 | tail -1")
    S["dd_write"] = panda.run_serial_cmd("dd if=/dev/zero of=/root/cbtest bs=64k count=16 oflag=direct 2>&1 | tail -1;"
                                         " sync; rm -f /root/cbtest")
    if arch == "x86_64":
        progress("unassigned I/O ports")
        S["port"] = panda.run_serial_cmd("dd if=/dev/port bs=1 skip=$((0x2f0)) count=1 2>/dev/null | od -An -tx1;"
                                         " printf '\\x00' | dd of=/dev/port bs=1 seek=$((0x2f0)) 2>/dev/null; echo w=$?")
    progress("monitor")
    S["monitor"] = panda.run_monitor_cmd("info status")
    progress("type_serial_cmd / finish_serial_cmd")
    panda.type_serial_cmd("echo typed-$((6*7))")
    S["typed"] = panda.finish_serial_cmd()
    progress("snapshot: snap, change, revert_sync, delvm_sync")
    try:
        panda.run_serial_cmd("echo before > /root/snapfile")
        panda.snap("cbtest")
        panda.run_serial_cmd("echo after > /root/snapfile", timeout=60)
        panda.revert_sync("cbtest")
        S["snapfile"] = panda.run_serial_cmd("cat /root/snapfile").strip()
        panda.delvm_sync("cbtest")
        panda.snap("cbtest2")
        panda.delvm("cbtest2")
        S["after_delvm"] = panda.run_serial_cmd("echo alive-$((6*7))", timeout=60)
        S["snapshots"] = panda.run_monitor_cmd("info snapshots")
    except Exception as e:  # noqa: BLE001 - report it as a failed check, still end the analysis
        S["snap_error"] = repr(e)[:300]
        progress(f"snapshot step failed: {S['snap_error']}")
    progress("end_analysis")
    panda.end_analysis()


panda.run()
progress("analysis ended")

for name in registered:
    if name in EXERCISED_ONLY_ON and arch not in EXERCISED_ONLY_ON[name]:
        continue
    C.check(f"{name} fires", counts[name] > 0, counts[name])
C.check("after_loadvm: once per revert (root, cbtest)", counts["after_loadvm"] == (1 if "snap_error" in S else 2),
        counts["after_loadvm"])
k = S["kernel"] or {}
C.check("in asid_changed (kernel mode): in_kernel, in_kernel_mode, in_kernel_code_linux",
        k.get("in_kernel") and k.get("in_kernel_mode") and k.get("in_kernel_code_linux"), k)
C.check("in asid_changed: a kernel virtual read (linux_banner)", k.get("banner") == b"Linux version ", k.get("banner"))
C.check("in asid_changed: current_ksp is a kernel address", k.get("ksp", 0) >= 1 << 63, hex(k.get("ksp", 0)))
C.check("guest commands run (cat)", "PRETTY_NAME" in S.get("cat", ""), repr(S.get("cat")))
C.check("O_DIRECT disk read and write complete", "copied" in S.get("dd_read", "") and "copied" in S.get("dd_write", ""),
        (S.get("dd_read"), S.get("dd_write")))
if arch == "x86_64":
    C.check("x86 /dev/port: an unassigned port reads 0xff, a write to it completes",
            "ff" in S.get("port", "") and "w=0" in S.get("port", ""), repr(S.get("port")))
C.check("run_monitor_cmd returns the monitor's output", "running" in S.get("monitor", ""), repr(S.get("monitor")))
C.check("type_serial_cmd + finish_serial_cmd", "typed-42" in (S.get("typed") or ""), repr(S.get("typed")))
C.check("snap / revert_sync restores the snapshot's state", S.get("snapfile") == "before",
        S.get("snap_error") or repr(S.get("snapfile")))
C.check("delvm_sync deletes it", "cbtest" not in S.get("snapshots", "cbtest"), repr(S.get("snapshots")))
C.check("delvm deletes it and the guest keeps running", "alive-42" in S.get("after_delvm", "")
        and "cbtest2" not in S.get("snapshots", "cbtest2"), repr(S.get("after_delvm")))
sys.exit(C.report(f"CALLBACKS arch={arch} " + " ".join(f"{n}={counts[n]}" for n in registered)))
