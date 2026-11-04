"""Boot a MIPS busybox guest that runs sct (o32, and on mips64el n32 and n64 too) under the
syscall trap's self-test consumer, and keep the console and the trap log.

  python3 run.py <mipsel|mips64el|mipseb|mips64eb> <out dir>
Environment: QBUILD (engine build dir), ZIG (zig binary), KERNELS (linux_builder 6.13 dir),
BUSYBOX (rehosting/busybox dist dir).
"""
import os
import subprocess
import sys
from os import path

HERE = path.dirname(path.abspath(__file__))
sys.path.insert(0, path.join(HERE, "..", "..", "..", "..", "tests"))
from mkbusybox import cpio_newc  # noqa: E402

# arch: (qemu, cpu, zig targets, MIPS_SC_TRACE pid offset,thread size)
ARCH = {
    "mipsel": ("mipsel", None, {"o32": "mipsel-linux-musleabi"}, "832,8192"),
    "mipseb": ("mips", None, {"o32": "mips-linux-musleabi"}, "832,8192"),
    "mips64el": ("mips64el", "MIPS64R2-generic",
                 {"o32": "mipsel-linux-musleabi", "n64": "mips64el-linux-muslabi64",
                  "n32": "mips64el-linux-muslabin32"}, "1024,16384"),
    "mips64eb": ("mips64", "MIPS64R2-generic",
                 {"o32": "mips-linux-musleabi", "n64": "mips64-linux-muslabi64",
                  "n32": "mips64-linux-muslabin32"}, "1024,16384"),
}

INIT = """#!/bin/sh
/bin/busybox mkdir -p /proc /tmp
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t tmpfs tmp /tmp
/bin/busybox echo "T cpus $(/bin/busybox grep -c ^processor /proc/cpuinfo)"
for a in {abis}; do /sct-$a {test}; done
/bin/busybox reboot -f
"""


def main():
    arch, out = sys.argv[1], path.abspath(sys.argv[2])
    test = sys.argv[3] if len(sys.argv) > 3 else "all"
    qemu, cpu, targets, trace = ARCH[arch]
    os.makedirs(out, exist_ok=True)
    zig = os.environ["ZIG"]
    entries = [("bin", 0o40755, b""), ("proc", 0o40755, b""), ("tmp", 0o40755, b"")]
    for abi, zt in targets.items():
        exe = path.join(out, f"sct-{abi}")
        subprocess.run([zig, "cc", "-target", zt, "-static", "-O1", "-pthread",
                        path.join(HERE, "sct.c"), "-o", exe], check=True)
        entries.append((f"sct-{abi}", 0o100755, open(exe, "rb").read()))
    bb = open(path.join(os.environ["BUSYBOX"], arch, "busybox"), "rb").read()
    entries += [("bin/busybox", 0o100755, bb), ("bin/sh", 0o120777, b"busybox"),
                ("init", 0o100755, INIT.format(abis=" ".join(targets), test=test).encode())]
    initrd = path.join(out, "initrd")
    with open(initrd, "wb") as f:
        f.write(cpio_newc(entries))
    kernel = path.join(os.environ["KERNELS"], f"vmlinux.{arch}")
    cmd = [path.join(os.environ["QBUILD"], f"qemu-system-{qemu}"), "-M", "malta", "-m", "512M",
           "-kernel", kernel, "-initrd", initrd,
           "-append", "console=ttyS0 rdinit=/init panic=-1 quiet",
           "-display", "none", "-monitor", "none", "-serial", f"file:{out}/console.txt",
           "-net", "none", "-no-reboot", "-d", "unimp", "-D", f"{out}/qemu.log"]
    if cpu:
        cmd[3:3] = ["-cpu", cpu]
    cmd += os.environ.get("QEMU_EXTRA", "").split()   # e.g. "-smp 2 -cpu I6400"
    env = dict(os.environ, SYSCALL_TRAP_SELFTEST=f"{out}/trap.log", MIPS_SC_TRACE=trace)
    r = subprocess.run(cmd, env=env, timeout=900)
    print(f"{arch}: qemu exit {r.returncode}; {out}/console.txt {out}/trap.log")


if __name__ == "__main__":
    main()
