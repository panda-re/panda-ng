"""Guest analysis checks on an image built by pandare2.mkimage.

Run: panda-python tests/guest.py <dir>/debian-13-x86_64.json

Reverts to the root snapshot, runs a known command and a hypercall program on the guest, and checks
what asid_changed, syscalls2, proc_start_linux and hypercaller report.
"""
import json
import os
import struct
import sys

from pandare2 import Panda

MAGIC = 0x50616e64

# A static ELF that makes one PANDA hypercall with MAGIC as its first argument, then exits 0.
CODE = {
    # ioperm(0x88, 4, 1); edi = eax = MAGIC; out 0x88, eax; exit(0)
    "x86_64": (62, bytes.fromhex("b8ad000000" "bf88000000" "be04000000" "ba01000000" "0f05")
               + b"\xbf" + struct.pack("<I", MAGIC) + b"\xb8" + struct.pack("<I", MAGIC)
               + bytes.fromhex("e788" "b83c000000" "31ff" "0f05")),
    # x0 = x8 = MAGIC; the hypercall instruction; exit(0)
    "aarch64": (183, struct.pack("<7I",
                                 0xD2800000 | (MAGIC & 0xffff) << 5,
                                 0xF2A00000 | (MAGIC >> 16) << 5,
                                 0xAA0003E8,  # mov x8, x0
                                 0xD500501F,
                                 0xD2800000,  # mov x0, #0
                                 0xD2800BA8,  # mov x8, #93
                                 0xD4000001)),  # svc #0
    # a0 = a7 = MAGIC; the hypercall instruction; exit(0)
    "riscv64": (243, struct.pack("<7I",
                                 ((MAGIC + 0x800) >> 12) << 12 | 10 << 7 | 0x37,  # lui a0, %hi
                                 (MAGIC & 0xfff) << 20 | 10 << 15 | 10 << 7 | 0x1B,  # addiw a0, a0, %lo
                                 0x00050893,  # mv a7, a0
                                 0x00004013,  # xori x0, x0, 0
                                 0x00000513,  # li a0, 0
                                 0x05D00893,  # li a7, 93
                                 0x00000073)),  # ecall
}


def elf(machine, code, base=0x400000):
    entry = 64 + 56
    size = entry + len(code)
    ehdr = struct.pack("<16sHHIQQQIHHHHHH", b"\x7fELF\x02\x01\x01", 2, machine, 1, base + entry,
                       64, 0, 0, 64, 56, 1, 0, 0, 0)
    phdr = struct.pack("<IIQQQQQQ", 1, 5, 0, base, base, size, size, 0x1000)
    return ehdr + phdr + code


panda = Panda(generic=sys.argv[1])
arch = panda.arch_name
# Needs an IGLOO-instrumented kernel to report syscalls; here it only has to load and register its
# hypercalls with hypercaller next to ours.
panda.load_plugin("hypersyscalls")
with open(sys.argv[1]) as f:
    kconf = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), json.load(f)["kconf_file"])
# osi would load osi_linux itself, without its arguments.
panda.load_plugin("osi", {"disable-autoload": "true"})
panda.load_plugin("osi_linux", {"kconf_file": kconf})
seen = {"asid": 0, "openat": [], "auxv": [], "hypercall": []}


@panda.cb_asid_changed
def asid_changed(cpu, old, new):
    seen["asid"] += 1
    return 0


@panda.ppp("syscalls2", "on_sys_openat_enter")
def openat(cpu, pc, dfd, filename, flags, *mode):
    try:
        seen["openat"].append(panda.read_str(cpu, filename))
    except ValueError:
        pass


@panda.ppp("proc_start_linux", "on_rec_auxv")
def auxv(cpu, tb, av):
    argv = [panda.ffi.string(av.argv[i]).decode() for i in range(av.argc)]
    proc = panda.plugins["osi"].get_current_process(cpu)
    osi = (panda.ffi.string(proc.name).decode(), proc.pid) if proc != panda.ffi.NULL else None
    seen["auxv"].append((panda.ffi.string(av.execfn).decode(), argv, osi))


@panda.hypercall(MAGIC)
def hypercall(cpu):
    seen["hypercall"].append(panda.arch.get_arg(cpu, 1, convention="syscall"))
    # while running: end_analysis unloads the plugins
    seen["hypersyscalls"] = panda.libpanda.panda_get_plugin_by_name(b"hypersyscalls") != panda.ffi.NULL


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    # exec keeps the pid, so cat runs as the PID the shell prints.
    out = panda.run_serial_cmd("sh -c 'echo PID=$$; exec cat /etc/os-release'").strip().splitlines()
    seen["pid"] = int(out[0].split("=")[1])
    seen["os_id"] = next((l.strip() for l in out[1:] if l.startswith("ID=")), "")
    data = elf(*CODE[arch])
    panda.run_serial_cmd("rm -f /tmp/hc")
    for i in range(0, len(data), 64):
        chunk = "".join(f"\\x{b:02x}" for b in data[i:i + 64])
        panda.run_serial_cmd(f"printf '{chunk}' >> /tmp/hc")
    seen["hc"] = panda.run_serial_cmd("chmod +x /tmp/hc && /tmp/hc; echo rc=$?").strip()
    panda.end_analysis()


panda.run()

os_release_opens = [f for f in seen["openat"] if f == "/etc/os-release"]
cat_starts = [a for a in seen["auxv"] if a[1] == ["cat", "/etc/os-release"]]
print(f"GUEST arch={arch} os_id={seen['os_id']!r} pid={seen['pid']} asid_changes={seen['asid']} "
      f"openat_os_release={len(os_release_opens)} cat_auxv={cat_starts[:1]} "
      f"hypercalls={[hex(m) for m in seen['hypercall']]} hc={seen['hc']!r}")
checks = {
    "command output": seen["os_id"].startswith("ID="),
    "asid_changed": seen["asid"] > 0,
    "syscalls2 openat(/etc/os-release)": len(os_release_opens) > 0,
    "proc_start_linux argv": len(cat_starts) > 0 and cat_starts[0][0].endswith("/cat"),
    "osi_linux name and pid": len(cat_starts) > 0 and cat_starts[0][2] == ("cat", seen["pid"]),
    "hypercaller": seen["hypercall"] == [MAGIC] and seen["hc"].endswith("rc=0"),
    "hypersyscalls loaded": seen.get("hypersyscalls", False),
}
for name, ok in checks.items():
    print(f"  {'PASS' if ok else 'FAIL'} {name}")
sys.exit(0 if all(checks.values()) else 1)
