"""The process identifier under KPTI (x86_64), on a test machine.

Run: panda-python tests/kpti.py <dir>/busybox-x86_64.json [--intel] [--isf <kernel ISF>]
(a tests/mkbusybox.py image; the machine is tests/testmachine.py's, booted with nokaslr: built
the first time, in seconds)

With --intel the machine's CPU is GenuineIntel, which Linux treats as Meltdown-affected: page-table
isolation on, so each process has a kernel and a user CR3 (bit 12 apart) and the kernel switches
between them on every entry and exit. panda_current_asid must give one identifier for both, equal
to OSI's (the physical address of mm->pgd), and asid_changed must not fire for the switches. (QEMU's
TCG has no PCID, so the PCID bits current_asid drops aren't exercised here.) Without --intel (an
AMD model, not isolated) the same checks run with one CR3 for both modes, as the control.

OSI reads the kernel's symbols from the ISF linux_builder ships next to the kernel
(cosi.x86_64.json.xz beside the image's -kernel), or --isf.
"""
import argparse
import os
import sys

from testlib import Checks, progress
from testmachine import Agent, Machine

PANDA_TEST = dict(area="asid", tier="guest", fixture="busybox", arches=["x86_64"],
                  plugins=["osi", "osi_isf", "syscalls2"], variants=dict(intel=[False, True]),
                  timeout=600)

SYSCALLS = 20000
MARKER = "/tmp/kpti-marker"

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--intel", action="store_true")
ap.add_argument("--isf")
opt = ap.parse_args()

m = Machine(opt.image, intel=opt.intel, append="nokaslr")   # OSI reads the ISF's addresses
args = m.image["extra_args"]
isf = opt.isf or os.path.join(os.path.dirname(args[args.index("-kernel") + 1]), "cosi.x86_64.json.xz")
if not os.path.isfile(isf):
    sys.exit(f"kpti: no kernel ISF at {isf} (linux_builder's cosi.x86_64.json.xz; or --isf)")
panda = m.panda()
agent = Agent(panda)
panda.load_plugin("syscalls2", {"load-info": False})
panda.load_plugin("osi", {"disable-autoload": "true"})
panda.load_plugin("osi_isf", {"isf": isf})
seen = {"changes": 0, "opens": 0, "armed": False}


def cr3(cpu):
    return panda.cpu_env(cpu).cr[3]


@panda.cb_asid_changed
def asid_changed(cpu, old, new):
    seen["changes"] += 1
    return False


def opened(cpu, filename):
    # syscalls2 calls this at the system call instruction: still user mode, on the user CR3.
    seen["opens"] += 1
    if not seen["armed"] or "user" in seen:
        return
    try:
        if panda.read_str(cpu, filename) != MARKER:
            return
    except ValueError:
        return
    seen["user"] = (panda.in_kernel_mode(cpu), panda.current_asid(cpu), cr3(cpu),
                    panda.current_asid_field(cpu))
    panda.enable_callback("kernel_block")


# busybox is static musl, which opens with open(2); a glibc program would use openat(2).
@panda.ppp("syscalls2", "on_sys_open_enter")
def sys_open(cpu, pc, filename, flags, mode):
    opened(cpu, filename)


@panda.ppp("syscalls2", "on_sys_openat_enter")
def sys_openat(cpu, pc, dfd, filename, flags, mode):
    opened(cpu, filename)


@panda.cb_before_block_exec(enabled=False)
def kernel_block(cpu, tb):
    # The same system call in the kernel, past the entry code's switch to the kernel CR3 (with PTI).
    # OSI reads kernel memory, which the user table doesn't map, so it is asked here.
    if not panda.in_kernel_mode(cpu) or (opt.intel and cr3(cpu) & 0x1000):
        return
    proc = panda.plugins["osi"].get_current_process(cpu)
    name = panda.ffi.string(proc.name).decode() if proc != panda.ffi.NULL else None
    seen["kernel"] = (name, proc.asid if name else None, panda.current_asid(cpu), cr3(cpu),
                      panda.current_asid_field(cpu))
    panda.disable_callback("kernel_block")


out = {}


@panda.queue_blocking
def drive():
    try:
        m.revert(panda, agent)
        rc, o = agent.run("cat /sys/devices/system/cpu/vulnerabilities/meltdown")
        out["meltdown"] = o.decode().strip()
        agent.put(MARKER, b"kpti\n", mode=0o644)
        progress(f"cat {MARKER}")
        seen["armed"] = True        # not the agent's own open of it, for the put
        out["cat"] = agent.run(f"exec cat {MARKER}")
        # System calls without process switches: with PTI, two CR3 switches each.
        progress(f"dd: {SYSCALLS} reads and writes")
        before = seen["changes"]
        out["dd"] = agent.run(f"dd if=/dev/zero of=/dev/null bs=1 count={SYSCALLS} 2>&1")
        out["changes_dd"] = seen["changes"] - before
    except Exception as e:
        out["error"] = repr(e)
    panda.end_analysis()


panda.run()

k, u = seen.get("kernel"), seen.get("user")
hx = lambda t: t and tuple(hex(v) if isinstance(v, int) and not isinstance(v, bool) else v for v in t)
print(f"meltdown={out.get('meltdown')!r} kernel(name, osi asid, current_asid, cr3, field)={hx(k)} "
      f"user(in_kernel, current_asid, cr3, field)={hx(u)} opens={seen['opens']} "
      f"asid_changed during {SYSCALLS} dd syscalls: {out.get('changes_dd')}")
C = Checks("x86_64")
pti = opt.intel
C.check("the guest ran the commands", "error" not in out and out.get("cat") == (0, b"kpti\n")
        and out.get("dd", (1,))[0] == 0, out.get("error") or (out.get("cat"), out.get("dd")))
C.check(f"the guest {'isolates' if pti else 'does not isolate'} page tables (PTI)",
        ("PTI" in out.get("meltdown", "")) == pti, out.get("meltdown"))
C.check("the open of the marker file was seen in user mode", u is not None and not u[0], hx(u))
if pti:
    C.check("user mode (the syscall instruction): CR3 is the user table (bit 12)",
            u is not None and u[2] & 0x1000, hx(u))
    C.check("kernel mode: CR3 is the same process's kernel table",
            k is not None and u is not None and k[3] == u[2] ^ 0x1000, (hx(k), hx(u)))
else:
    C.check("kernel mode: CR3 is the user mode one",
            k is not None and u is not None and k[3] == u[2], (hx(k), hx(u)))
C.check("kernel mode: current_asid is OSI's asid (cat)",
        k is not None and k[0] == "cat" and k[1] == k[2], hx(k))
C.check("user mode: current_asid is the kernel-mode one",
        k is not None and u is not None and u[1] == k[2], (hx(k), hx(u)))
C.check("current_asid is page aligned; current_asid_field is 0 (no PCID)",
        k is not None and u is not None and k[2] & 0xfff == 0 and k[4] == 0 and u[3] == 0,
        (hx(k), hx(u)))
# One dd: a couple of switches to and from it, nowhere near two per syscall.
C.check("asid_changed doesn't fire for KPTI's CR3 switches",
        out.get("changes_dd") is not None and out["changes_dd"] < SYSCALLS // 10, out.get("changes_dd"))
sys.exit(C.report(f"kpti x86_64{' intel' if pti else ''}"))
