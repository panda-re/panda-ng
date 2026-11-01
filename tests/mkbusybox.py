"""Small guest images for the arches no distribution cloud image covers: a linux_builder kernel, a
busybox initramfs and an empty qcow2 disk, ext2-formatted and mounted on /root, with a `root`
snapshot at the shell. They boot in seconds, so tests that only need a shell, a disk and
record/replay (taint, rr) can run on every arch.

Run: panda-python tests/mkbusybox.py <arch>... -o <dir> --kernels <dir> --busybox <dir>
  arch       x86_64 aarch64 armel mipsel mipseb mips64el mips64eb powerpc powerpc64 powerpc64le
             riscv64 loongarch64, or "all"
  --kernels  linux_builder's kernels, extracted (contains 6.13/); default $KERNEL_DIR
  --busybox  rehosting/busybox #dist (<arch>/busybox); default $BUSYBOX_DIST. It has no 32-bit
             powerpc: put Debian's (busybox-static, debian-ports powerpc) at powerpc/busybox.
Writes busybox-<arch>.json, .qcow2 and .initrd to <dir>. The pins are engine/tests/guest/prepare.sh's
(linux_builder v4.0.1, rehosting/busybox eeba468aa), which also fetches them. As with
pandare2.mkimage, build the images with the PANDA that will run them: it takes the snapshot.
"""
import argparse
import gzip
import io
import json
import os
import struct
import sys
from os import path

# arch: (pandare2 arch, kernel, console, machine args, word size)
ARCHES = {
    "x86_64": ("x86_64", "bzImage.x86_64", "ttyS0", ["-M", "pc"], 64),
    "aarch64": ("aarch64", "zImage.arm64", "ttyAMA0", ["-M", "virt", "-cpu", "cortex-a57"], 64),
    "armel": ("arm", "zImage.armel", "ttyAMA0", ["-M", "virt"], 32),
    "mipsel": ("mipsel", "vmlinux.mipsel", "ttyS0", ["-M", "malta"], 32),
    "mipseb": ("mips", "vmlinux.mipseb", "ttyS0", ["-M", "malta"], 32),
    "mips64el": ("mips64el", "vmlinux.mips64el", "ttyS0", ["-M", "malta", "-cpu", "MIPS64R2-generic"], 64),
    "mips64eb": ("mips64", "vmlinux.mips64eb", "ttyS0", ["-M", "malta", "-cpu", "MIPS64R2-generic"], 64),
    "powerpc64": ("ppc64", "vmlinux.powerpc64", "hvc0", ["-M", "pseries", "-cpu", "power9"], 64),
    "powerpc64le": ("ppc64", "vmlinux.powerpc64le", "hvc0", ["-M", "pseries", "-cpu", "power9"], 64),
    "riscv64": ("riscv64", "Image.riscv64", "ttyS0", ["-M", "virt"], 64),
    "powerpc": ("ppc", "vmlinux.powerpc", "ttyS0", ["-M", "ppce500", "-cpu", "e500mc"], 32),
    "loongarch64": ("loongarch64", "vmlinuz.efi.loongarch64", "ttyS0",
                    ["-M", "virt", "-cpu", "la464", "-bios", "{biospath}/edk2-loongarch64-code.fd"], 64),
}

# The rr disk device, where pandare2's default for the arch doesn't fit the kernel: linux_builder's
# x86_64 kernel has no ATA driver.
DISK = {"x86_64": "virtio-blk-pci", "powerpc": "virtio-blk-pci"}

INIT = """#!/bin/sh
/bin/busybox --install -s /bin
export PATH=/bin HOME=/root
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev 2>/dev/null
mkdir -p /dev/shm && mount -t tmpfs shm /dev/shm
cd /root
export PS1='root@{arch}:~# '
while true; do sh -i </dev/console >/dev/console 2>&1; done
"""


def cpio_newc(entries):
    """A gzipped newc cpio archive of (name, mode, data) entries (data: bytes, or a link target)."""
    out = io.BytesIO()
    for ino, (name, mode, data) in enumerate(entries + [("TRAILER!!!", 0, b"")], 1):
        nb = name.encode() + b"\0"
        hdr = b"070701" + b"".join(b"%08X" % v for v in (
            ino, mode, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(nb), 0))
        out.write(hdr + nb + b"\0" * (-(len(hdr) + len(nb)) % 4))
        out.write(data + b"\0" * (-len(data) % 4))
    return gzip.compress(out.getvalue(), 9)


def empty_qcow2(file, size):
    """An empty qcow2 (version 3, 64 KiB clusters): header, refcount table, refcount block, L1."""
    c = 1 << 16
    l1_size = -(-size // (c * (c // 8)))
    hdr = struct.pack(">4sIQIIQIIQQIIQQQQII", b"QFI\xfb", 3, 0, 0, 16, size, 0, l1_size, 3 * c,
                      c, 1, 0, 0, 0, 0, 0, 4, 104)
    with open(file, "wb") as f:
        f.write(hdr + b"\0" * 8)                     # no header extensions
        f.seek(c)
        f.write(struct.pack(">Q", 2 * c))            # refcount table -> block at cluster 2
        f.seek(2 * c)
        f.write(struct.pack(">4H", 1, 1, 1, 1))      # clusters 0-3 in use
        f.truncate(4 * c)                            # L1 at cluster 3: all zero


def build(arch, out_dir, kernels, busybox):
    parch, kfile, console, machine, bits = ARCHES[arch]
    name = f"busybox-{arch}"
    kernel = path.join(kernels, "6.13", kfile)
    bb = open(path.join(busybox, arch, "busybox"), "rb").read()
    dirs = ["bin", "dev", "proc", "sys", "tmp", "root", "var", "var/tmp"]
    initrd = path.abspath(path.join(out_dir, name + ".initrd"))
    with open(initrd, "wb") as f:
        f.write(cpio_newc([(d, 0o40755, b"") for d in dirs] + [
            ("bin/busybox", 0o100755, bb), ("bin/sh", 0o120777, b"busybox"),
            ("init", 0o100755, INIT.format(arch=arch).encode())]))
    qcow = path.join(out_dir, name + ".qcow2")
    empty_qcow2(qcow, 64 << 20)
    desc = {
        "arch": parch, "os": f"linux-{bits}-generic", "prompt": f"root@{arch}:~# ", "cdrom": None,
        "snapshot": "root", "url": None, "alternate_urls": None, "extra_files": None,
        "qcow": path.basename(qcow), "default_mem": "512M",
        "extra_args": machine + ["-kernel", path.abspath(kernel), "-initrd", initrd,
                                 "-append", f"console={console} rdinit=/init panic=-1 quiet",
                                 "-display", "none", "-net", "none", "-no-reboot"],
        "hashes": None, "source": None, "isf": None, "kconf_file": None, "disk_device": DISK.get(arch),
        "description": f"linux_builder 6.13 {arch} kernel, busybox initramfs, /root on ext2",
    }
    jpath = path.join(out_dir, name + ".json")
    with open(jpath, "w") as f:
        json.dump(desc, f, indent=1)
    return jpath


def snapshot(jpath):
    """In a child process (one Panda per process): format the disk, mount it on /root, savevm."""
    from pandare2 import Panda
    panda = Panda(generic=jpath, rr=True)

    @panda.queue_blocking
    def snap():
        panda.serial_console.expect(timeout=300)
        out = panda.run_serial_cmd(
            "for d in vda sda; do [ -b /dev/$d ] && break; done; "
            "mke2fs -q /dev/$d && mount -t ext2 /dev/$d /root && cd /root && "
            "echo 1 > /proc/sys/kernel/printk && sync && grep -c ' /root ext2' /proc/mounts", timeout=120)
        ok = out.strip().endswith("1")
        if ok:
            panda.run_monitor_cmd("savevm root")
        print(f"{jpath}: {'snapshot root' if ok else 'disk not mounted: ' + out}", flush=True)
        panda.end_analysis()
        if not ok:
            os._exit(1)

    panda.run()


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("arch", nargs="+")
    p.add_argument("-o", "--out", required=True)
    p.add_argument("--kernels", default=os.environ.get("KERNEL_DIR"))
    p.add_argument("--busybox", default=os.environ.get("BUSYBOX_DIST"))
    p.add_argument("--snapshot-only", help=argparse.SUPPRESS)
    a = p.parse_args()
    if a.snapshot_only:
        snapshot(a.snapshot_only)
        return
    if not a.kernels or not a.busybox:
        p.error("--kernels and --busybox (or KERNEL_DIR and BUSYBOX_DIST) are required")
    arches = list(ARCHES) if a.arch == ["all"] else a.arch
    os.makedirs(a.out, exist_ok=True)
    import subprocess
    failed = []
    for arch in arches:
        jpath = build(arch, a.out, a.kernels, a.busybox)
        rc = subprocess.run([sys.executable, __file__, "x", "-o", a.out, "--snapshot-only", jpath]).returncode
        if rc:
            failed.append(arch)
    if failed:
        sys.exit(f"no snapshot for: {' '.join(failed)}")


if __name__ == "__main__":
    main()
