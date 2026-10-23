"""The process identifier under KPTI and PCID (x86_64), on an image built by pandare2.mkimage.

Run: panda-python tests/kpti.py <dir>/debian-13-x86_64.json

The images' snapshots run on qemu64, an AMD model the kernel doesn't isolate. This boots the image
from its disk instead (-snapshot: nothing is written back), as a GenuineIntel CPU with PCID, which
Linux treats as Meltdown-affected: page-table isolation on, so each process has a kernel and a
user CR3 (bit 12 apart, PCIDs in the low bits) and the kernel switches between them on every
entry and exit. panda_current_asid must give one identifier for both, equal to OSI's (the
physical address of mm->pgd), and asid_changed must not fire for the switches.
"""
import json
import os
import sys

from pandare2 import Panda

CPU = "qemu64,vendor=GenuineIntel,+pcid,+invpcid"
panda = Panda(generic=sys.argv[1], extra_args=f"-cpu {CPU} -snapshot")
with open(sys.argv[1]) as f:
    image = json.load(f)
isf = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), image["isf"])
PROMPT = image["prompt"]
panda.load_plugin("osi", {"disable-autoload": "true"})
panda.load_plugin("osi_isf", {"isf": isf})
seen = {"changes": 0, "cr3s": set(), "user": None}
SYSCALLS = 20000


def cr3(cpu):
    return panda.cpu_env(cpu).cr[3]


@panda.cb_asid_changed
def asid_changed(cpu, old, new):
    seen["changes"] += 1
    return False


@panda.ppp("syscalls2", "on_sys_openat_enter")
def openat(cpu, pc, dfd, filename, flags, *mode):
    if "kernel" in seen:
        return
    try:
        if panda.read_str(cpu, filename) != "/etc/os-release":
            return
    except ValueError:
        return
    proc = panda.plugins["osi"].get_current_process(cpu)
    if proc == panda.ffi.NULL:
        return
    seen["kernel"] = (panda.ffi.string(proc.name).decode(), proc.asid, panda.current_asid(cpu), cr3(cpu),
                      panda.current_asid_field(cpu))
    panda.enable_callback("user_block")


@panda.cb_before_block_exec(enabled=False)
def user_block(cpu, tb):
    # The same process back in user mode, on its user CR3.
    if panda.in_kernel_mode(cpu):
        return
    seen["user"] = (panda.current_asid(cpu), cr3(cpu), panda.current_asid_field(cpu))
    panda.disable_callback("user_block")


@panda.queue_blocking
def drive():
    console = panda.serial_console
    console.use_unansi = False
    console.update_expectation(r"login: ")
    console.expect(timeout=3000)   # a full TCG boot, with OSI and syscalls2 loaded
    console.update_expectation(f"(?:{PROMPT}|Password: )")
    console.sendline(b"root")
    console.expect(timeout=300)
    console.update_expectation(PROMPT)
    console.sendline(b"bind 'set enable-bracketed-paste off'")
    console.expect(timeout=120)
    console.use_unansi = True
    panda.run_serial_cmd("dmesg -n 1")
    seen["meltdown"] = panda.run_serial_cmd("cat /sys/devices/system/cpu/vulnerabilities/meltdown").strip()
    seen["pcid"] = "pcid" in panda.run_serial_cmd("grep -o -w pcid /proc/cpuinfo | head -1")
    panda.run_serial_cmd("cat /etc/os-release > /dev/null")
    # Syscalls without switches: KPTI switches CR3 twice for each.
    seen["changes_before"] = seen["changes"]
    panda.run_serial_cmd(f"dd if=/dev/zero of=/dev/null bs=1 count={SYSCALLS} 2>/dev/null", timeout=600)
    seen["changes_dd"] = seen["changes"] - seen["changes_before"]
    panda.end_analysis()


panda.run()

k, u = seen.get("kernel"), seen.get("user")
print(f"KPTI meltdown={seen.get('meltdown')!r} pcid={seen.get('pcid')} "
      f"kernel(name, osi asid, current_asid, cr3, field)={k and (k[0],) + tuple(hex(v) for v in k[1:])} "
      f"user(current_asid, cr3, field)={u and tuple(hex(v) for v in u)} "
      f"asid_changed during {SYSCALLS} dd syscalls: {seen.get('changes_dd')}")
checks = {
    "the guest isolates page tables (PTI)": "PTI" in seen.get("meltdown", ""),
    "the guest uses PCID": seen.get("pcid"),
    "kernel mode: current_asid is OSI's asid": k is not None and k[0] == "cat" and k[1] == k[2],
    "user mode: CR3 is the other table (bit 12)": k is not None and u is not None
        and (u[1] ^ k[3]) & 0x1000 == 0x1000,
    "user mode: current_asid is the kernel-mode one": k is not None and u is not None and u[0] == k[2],
    "current_asid drops the PCID; current_asid_field is it": k is not None and u is not None
        and k[2] & 0xfff == 0 and u[2] == u[1] & 0xfff and u[2] != 0,
    # One dd: a couple of switches to and from it, nowhere near two per syscall.
    "asid_changed doesn't fire for KPTI's CR3 switches": seen.get("changes_dd") is not None
        and seen["changes_dd"] < SYSCALLS // 10,
}
failed = False
for name, ok in checks.items():
    print(f"  {'PASS' if ok else 'FAIL'} {name}")
    failed |= not ok
sys.exit(1 if failed else 0)
