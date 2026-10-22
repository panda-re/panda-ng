'''
Build a generic PANDA image: a distribution's cloud image with a "root" snapshot taken at a
logged-in root shell on the serial console, so revert_sync("root") and run_serial_cmd work
straight away.

    panda-python -m pandare2.mkimage debian-13-x86_64 -o ~/.panda

writes debian-13-x86_64.qcow2 and debian-13-x86_64.json to the output directory, with the guest
kernel's symbols: Volatility 3 ISF (.isf.json.xz) and osi_linux kernelinfo (.kernelinfo.conf), both
from the distribution's kernel debug package. Use it with
Panda(generic="<dir>/debian-13-x86_64.json").

The snapshot is taken by this PANDA with the arguments pandare2 restores it with, so build images
with the PANDA that will run them.
'''

import argparse
import hashlib
import json
import lzma
import shutil
import subprocess
import tarfile
import urllib.request
from os import makedirs, path, remove
from time import monotonic

from .panda import Panda
from .panda_expect import TimeoutExpired

DEBIAN_13 = "https://cloud.debian.org/images/cloud/trixie/20261021-2638/"

# Debian's nocloud images log root in without a password and run a getty on the serial console.
RECIPES = {
    "debian-13-x86_64": dict(
        arch="x86_64",
        url=DEBIAN_13 + "debian-13-nocloud-amd64-20261021-2638.qcow2",
        sha512="4eca654417db771d215f7f8d3fbb4fe3c24cd4971f861ec0805339f0e462408f0627e28ece38f281ea7d9bcedea2e48b9708e8abc24afcf8708a126b183a9ca0",
        mem="1G",
        extra_args="-display none -nic user,model=virtio-net-pci",
    ),
    "debian-13-aarch64": dict(
        arch="aarch64",
        url=DEBIAN_13 + "debian-13-nocloud-arm64-20261021-2638.qcow2",
        sha512="7ea0733e425ba5ab80ca4860936eaae63187f6104c5c1514663bf885e9d30157388a491c017853d6d03e20a28936797da58f748d74e79a9da81f66fed267e242",
        mem="1G",
        extra_args="-M virt -cpu max -bios edk2-aarch64-code.fd -display none -nic user,model=virtio-net-pci",
    ),
    "debian-13-ppc64": dict(
        arch="ppc64",
        url=DEBIAN_13 + "debian-13-nocloud-ppc64el-20261021-2638.qcow2",
        sha512="0983aa6a5fffbdd37b8405fad2d43bc92bac6f34802206e6e09d1412ea157f87323ab763f82a933d1971b774ebedf2d27ba530e430083565a37fc50853733bf0",
        mem="1G",
        extra_args="-M pseries -display none -nic user,model=virtio-net-pci",
    ),
}

PROMPT = r"root@[\w.-]+:[^#\n]*# "
FIRSTBOOT_PASSWORD = "panda"

DEBIAN_ARCH = {"x86_64": "amd64", "aarch64": "arm64", "ppc64": "ppc64el"}
DEBIAN_ARCHIVES = [
    ("https://deb.debian.org/debian/", "trixie"),
    ("https://deb.debian.org/debian/", "trixie-updates"),
    ("https://security.debian.org/debian-security/", "trixie-security"),
]


def download(url, sha512, dest):
    if path.isfile(dest) and _sha512(dest) == sha512:
        return
    print(f"Downloading {url}")
    with urllib.request.urlopen(url) as r, open(dest + ".tmp", "wb") as f:
        shutil.copyfileobj(r, f)
    got = _sha512(dest + ".tmp")
    if got != sha512:
        remove(dest + ".tmp")
        raise RuntimeError(f"{url}: sha512 {got}, expected {sha512}")
    shutil.move(dest + ".tmp", dest)


def _sha512(p):
    h = hashlib.sha512()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def debian_vmlinux(arch, kernel, cache_dir):
    '''Fetch the -dbg package for a Debian kernel release and return its vmlinux (with DWARF).'''
    vmlinux = path.join(cache_dir, f"vmlinux-{kernel}")
    if path.isfile(vmlinux):
        return vmlinux
    package = f"linux-image-{kernel}-dbg"
    for base, suite in DEBIAN_ARCHIVES:
        index = f"{base}dists/{suite}/main/binary-{DEBIAN_ARCH[arch]}/Packages.xz"
        with urllib.request.urlopen(index) as r:
            stanzas = lzma.decompress(r.read()).decode().split("\n\n")
        fields = [dict(l.split(": ", 1) for l in st.splitlines() if ": " in l and not l.startswith(" "))
                  for st in stanzas if st.startswith(f"Package: {package}\n")]
        if fields:
            break
    else:
        raise RuntimeError(f"{package} is in none of the Debian archives")
    deb = path.join(cache_dir, path.basename(fields[0]["Filename"]))
    if not path.isfile(deb):
        print(f"Downloading {package} for kernelinfo")
        with urllib.request.urlopen(base + fields[0]["Filename"]) as r, open(deb + ".tmp", "wb") as f:
            shutil.copyfileobj(r, f)
        shutil.move(deb + ".tmp", deb)
    # A .deb is an ar archive; vmlinux is in data.tar.*.
    with open(deb, "rb") as f:
        assert f.read(8) == b"!<arch>\n"
        while True:
            header = f.read(60)
            member, size = header[:16].decode().strip().rstrip("/"), int(header[48:58])
            if member.startswith("data.tar"):
                break
            f.seek(size + size % 2, 1)
        with tarfile.open(fileobj=f, mode="r|*") as tar:
            for entry in tar:
                if entry.name.endswith((f"/vmlinux-{kernel}", f"/{kernel}/vmlinux")):
                    with tar.extractfile(entry) as src, open(vmlinux + ".tmp", "wb") as dst:
                        shutil.copyfileobj(src, dst)
                    break
            else:
                raise RuntimeError(f"no vmlinux in {deb}")
    shutil.move(vmlinux + ".tmp", vmlinux)
    return vmlinux


def kernelinfo(vmlinux, group, out):
    '''Write an osi_linux kernelinfo.conf for vmlinux with PANDA's gdb extractor.'''
    gdb = next((g for g in [shutil.which("gdb-multiarch"), shutil.which("gdb")]
                if g and path.isfile(g)), None)
    if gdb is None:
        raise RuntimeError("kernelinfo needs gdb (or gdb-multiarch)")
    # Packaged next to this module; in a source checkout it's in the osi_linux plugin.
    here = path.dirname(path.abspath(__file__))
    script = next(p for p in [path.join(here, "extract_kernelinfo.py"),
                              path.join(here, "../../../plugins/osi_linux/utils/kernelinfo_gdb/extract_kernelinfo.py")]
                  if path.isfile(p))
    subprocess.run([gdb, "-batch", "-nx", vmlinux, "-ex", f"source {script}",
                    "-ex", f"kernel_info {out}.body"], check=True, stdout=subprocess.DEVNULL)
    with open(out, "w") as f, open(out + ".body") as body:
        f.write(f"[{group}]\n" + body.read())
    remove(out + ".body")


def isf(vmlinux, out):
    '''Write the kernel's Volatility 3 ISF (symbols and types from its DWARF), xz-compressed.'''
    dwarf2json = shutil.which("dwarf2json")
    if dwarf2json is None:
        raise RuntimeError("ISF generation needs dwarf2json")
    # Through a pipe: given the LZMAFile as stdout, subprocess would write to its raw fileno.
    with lzma.open(out, "wb") as f:
        proc = subprocess.Popen([dwarf2json, "linux", "--elf", vmlinux], stdout=subprocess.PIPE)
        shutil.copyfileobj(proc.stdout, f)
        if proc.wait() != 0:
            raise subprocess.CalledProcessError(proc.returncode, proc.args)


def add_symbols(name, out_dir, cache_dir):
    '''Generate the image kernel's ISF and osi_linux kernelinfo and record them in its json.'''
    json_path = path.join(out_dir, name + ".json")
    with open(json_path) as f:
        image = json.load(f)
    # os is linux-<bits>-<distro>:<kernel>; osi_linux's default group for it is <distro>:<kernel>:<bits>.
    variant, kernel = image["os"].split(":", 1)
    _, bits, distro = variant.split("-")
    vmlinux = debian_vmlinux(image["arch"], kernel, cache_dir)
    image["isf"] = name + ".isf.json.xz"
    isf(vmlinux, path.join(out_dir, image["isf"]))
    image["kconf_file"] = name + ".kernelinfo.conf"
    kernelinfo(vmlinux, f"{distro}:{kernel}:{bits}", path.join(out_dir, image["kconf_file"]))
    with open(json_path, "w") as f:
        json.dump(image, f, indent=2)


def build(name, out_dir, cache_dir, boot_timeout):
    recipe = RECIPES[name]
    makedirs(out_dir, exist_ok=True)
    makedirs(cache_dir, exist_ok=True)
    base = path.join(cache_dir, recipe["url"].split("/")[-1])
    download(recipe["url"], recipe["sha512"], base)
    qcow = path.join(out_dir, name + ".qcow2")
    shutil.copyfile(base, qcow)

    panda = Panda(arch=recipe["arch"], qcow=qcow, mem=recipe["mem"],
                  extra_args=recipe["extra_args"].split(), expect_prompt=PROMPT)
    info = {}

    def boot_to_shell():
        console = panda.serial_console
        # Expect re-renders everything it has read through its ANSI emulator at each line with an
        # escape, which is quadratic over a coloured boot log. Read the boot as plain text.
        console.use_unansi = False
        # The first boot of a cloud image runs systemd-firstboot on the console, which waits for a
        # key and then asks questions. Enter skips each one, except the root password: skipping
        # that locks root, so give it a temporary one.
        console.update_expectation(r"(?:login: |new root password[^\n]*: )")
        deadline = monotonic() + boot_timeout
        while True:
            try:
                console.expect(timeout=20)
            except TimeoutExpired:
                if monotonic() > deadline:
                    raise
                console.send(b"\r")
                continue
            if "root password" not in console.last_prompt:
                break
            console.sendline(FIRSTBOOT_PASSWORD.encode())
        console.update_expectation(f"(?:{PROMPT}|Password: )")
        console.sendline(b"root")
        console.expect(timeout=120)
        if "Password" in console.last_prompt:
            console.update_expectation(PROMPT)
            console.sendline(FIRSTBOOT_PASSWORD.encode())
            console.expect(timeout=120)
        console.update_expectation(PROMPT)
        # bash wraps each prompt in bracketed-paste escapes, which end up in command output.
        console.sendline(b"bind 'set enable-bracketed-paste off'")
        console.expect(timeout=120)
        console.use_unansi = True

    @panda.queue_blocking
    def login():
        boot_to_shell()
        # Back to the image's own passwordless root.
        panda.run_serial_cmd("passwd -d root")
        # Kernel symbols (kernelinfo, ISF) are link-time addresses: boot without KASLR.
        panda.run_serial_cmd("echo 'GRUB_CMDLINE_LINUX=\"$GRUB_CMDLINE_LINUX nokaslr\"' > /etc/default/grub.d/99-panda.cfg")
        panda.run_serial_cmd("update-grub", timeout=boot_timeout)
        panda.serial_console.sendline(b"reboot")
        boot_to_shell()
        if "nokaslr" not in panda.run_serial_cmd("cat /proc/cmdline"):
            raise RuntimeError("the guest kernel did not boot with nokaslr")
        # Let boot finish so the snapshot is quiet, and keep kernel messages off the console.
        panda.run_serial_cmd("systemctl is-system-running --wait", timeout=boot_timeout)
        panda.run_serial_cmd("dmesg -n 1")
        info["kernel"] = panda.run_serial_cmd("uname -r").strip()
        info["os_release"] = panda.run_serial_cmd(". /etc/os-release; echo $PRETTY_NAME").strip()
        panda.run_serial_cmd("sync")
        panda.run_monitor_cmd("savevm root")
        panda.end_analysis()

    panda.run()

    bits = 64 if "64" in recipe["arch"] else 32
    image = dict(
        arch=recipe["arch"],
        os=f"linux-{bits}-{name.split('-')[0]}:{info['kernel']}",
        prompt=PROMPT,
        cdrom=None,
        snapshot="root",
        url=None,
        alternate_urls=None,
        extra_files=None,
        qcow=path.basename(qcow),
        default_mem=recipe["mem"],
        extra_args=recipe["extra_args"],
        hashes=None,
        source=recipe["url"],
        description=info["os_release"],
    )
    with open(path.join(out_dir, name + ".json"), "w") as f:
        json.dump(image, f, indent=2)
    add_symbols(name, out_dir, cache_dir)
    print(f"Built {name}: {info['os_release']}, kernel {info['kernel']}, in {out_dir}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("images", nargs="+", choices=sorted(RECIPES))
    p.add_argument("-o", "--out-dir", default=path.expanduser("~/.panda"))
    p.add_argument("--cache-dir", default=path.expanduser("~/.cache/panda-images"),
                   help="where downloaded cloud images and kernel debug packages are kept")
    p.add_argument("--boot-timeout", type=int, default=3600,
                   help="seconds to wait for the login prompt and for boot to finish")
    p.add_argument("--symbols-only", action="store_true",
                   help="regenerate the kernel ISF and osi_linux kernelinfo of images already built")
    args = p.parse_args()
    for name in args.images:
        if args.symbols_only:
            add_symbols(name, args.out_dir, args.cache_dir)
        else:
            build(name, args.out_dir, args.cache_dir, args.boot_timeout)


if __name__ == "__main__":
    main()
