"""Boot a busybox guest (tests/mkbusybox.py's kernels and busybox) with sct in the initramfs, under
a dev-tree qemu-system-ppc{,64} with the prototype's test consumer (SCTRAP_LOG), and print what
the guest did and what the trap saw.

  python3 tests/ppc-sctrap/run.py --qemu-dir <qemu build dir> --bins <dir with sct32/sct64/sct64le>
      --arch powerpc|powerpc64|powerpc64le [--smp N] [--log file] [--off]
"""
import argparse
import os
import subprocess
import sys
from os import path

HERE = path.dirname(path.abspath(__file__))
sys.path.insert(0, path.dirname(HERE))
import mkbusybox  # noqa: E402

# ISF offsets (linux_builder 6.13 cosi.*.json): task_struct.pid, task_struct.thread,
# paca_struct.__current. Used only by the test consumer's pid cross-check.
DIAG = {"powerpc": "1024,1984,-1", "powerpc64": "2008,-1,2312", "powerpc64le": "2008,-1,2312"}

INIT = """#!/bin/sh
/bin/busybox --install -s /bin
export PATH=/bin
mount -t proc proc /proc
mount -t devtmpfs dev /dev 2>/dev/null
echo SCT-INIT start
{runs}
echo SCT-INIT done
poweroff -f
"""


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--qemu-dir", required=True)
    p.add_argument("--bins", required=True)
    p.add_argument("--arch", required=True, choices=list(DIAG))
    p.add_argument("--dl", default=path.expanduser("~/.cache/panda-busybox"))
    p.add_argument("--smp", default="2")
    p.add_argument("--log", default=None)
    p.add_argument("--off", action="store_true", help="consumer installed, nothing watched")
    p.add_argument("--no-trap", action="store_true", help="no consumer at all")
    p.add_argument("--extra", default="", help="extra init commands")
    p.add_argument("--timeout", type=int, default=300)
    a = p.parse_args()

    parch, kfile, console, machine, bits = mkbusybox.ARCHES[a.arch]
    bins = {"powerpc": ["sct32"], "powerpc64": ["sct64", "sct32"], "powerpc64le": ["sct64le"]}[a.arch]
    runs = "\n".join(f"echo SCT-INIT run {b}; /{b}; echo SCT-INIT rc $?" for b in bins) + "\n" + a.extra
    bb = open(path.join(a.dl, "busybox", a.arch, "busybox"), "rb").read()
    entries = [(d, 0o40755, b"") for d in ["bin", "dev", "proc", "sys", "tmp"]]
    entries += [("bin/busybox", 0o100755, bb), ("bin/sh", 0o120777, b"busybox"),
                ("init", 0o100755, INIT.format(runs=runs).encode())]
    for b in bins:
        entries.append((b, 0o100755, open(path.join(a.bins, b), "rb").read()))
    out = path.dirname(path.abspath(a.log)) if a.log else HERE
    initrd = path.join(out, f"sct-{a.arch}.initrd")
    with open(initrd, "wb") as f:
        f.write(mkbusybox.cpio_newc(entries))

    q = path.join(a.qemu_dir, "qemu-system-" + parch)
    cmd = [q, *machine, "-m", "512M", "-smp", a.smp, "-L", path.join(path.dirname(a.qemu_dir), "pc-bios"),
           "-kernel", path.join(a.dl, "kernels", "6.13", kfile), "-initrd", initrd,
           "-append", f"console={console} rdinit=/init panic=-1 quiet",
           "-display", "none", "-monitor", "none", "-serial", "stdio", "-net", "none", "-no-reboot"]
    env = dict(os.environ)
    log = a.log or path.join(out, f"sct-{a.arch}.log")
    if not a.no_trap:
        env["SCTRAP_LOG"] = log
        env["SCTRAP_DIAG"] = DIAG[a.arch]
    if a.off:
        env["SCTRAP_OFF"] = "1"
    serial = log + ".serial"
    with open(serial, "wb") as so:
        try:
            r = subprocess.run(cmd, env=env, stdout=so, stderr=subprocess.STDOUT, timeout=a.timeout)
            rc = r.returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
    for line in open(serial, errors="replace").read().splitlines():
        if "SCT" in line or "error" in line.lower() or "Oops" in line or "panic" in line.lower():
            print("guest:", line)
    if not a.no_trap and path.exists(log):
        for line in open(log):
            if line.startswith(("SUMMARY", "PID-MISMATCH", "CHECK-BAD", "INJECT", "FAULT")):
                print("trap: ", line.rstrip())
    print("qemu rc", rc)


if __name__ == "__main__":
    main()
