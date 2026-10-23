#!/usr/bin/env python3
"""Turn the pure-Python pandare2 wheel into a self-contained manylinux wheel.

What it adds to the wheel (under pandare2/data/):
  lib/             libpanda-<arch>-softmmu.so for each target, libpanda_plugin_interface.so
  plugins/         the PANDA plugins (the layout pandare2 and syscalls2 expect)
  share/qemu/      firmware, keymaps
  bin/panda-qemu   the qemu-system-<arch> launcher (console scripts exec it)

Then, like auditwheel repair, it copies every shared library those need, except glibc's, into
pandare2.libs/ under a content-hashed name (so it never clashes with a library the host process
already has loaded), points each ELF's DT_NEEDED at the copies and its RUNPATH at $ORIGIN, sets the
launcher's interpreter to the standard one, and checks that nothing refers to /nix/store any more.
Unlike auditwheel it also bundles libstdc++ and libgcc_s: nix's gcc is newer than the hosts'.

The platform tag is manylinux_2_<N>_<arch>, N being the highest GLIBC_2.N symbol version any ELF in
the wheel needs (it is checked against --max-glibc).

Run (the flake's .#wheel does this):
  repair.py --wheel dist/pandare2-*.whl --engine ENGINE --plugins PLUGINDIR --launcher BIN --out DIR
"""
import argparse
import hashlib
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

from elftools.elf.dynamic import DynamicSection
from elftools.elf.elffile import ELFFile
from elftools.elf.gnuversions import GNUVerNeedSection

# glibc's own libraries: always the host's.
GLIBC = re.compile(r"^(libc|libm|libdl|libpthread|librt|libresolv|libutil|libanl|libmvec|"
                   r"libBrokenLocale|libnsl|libcrypt)\.so\.\d+$|^ld-linux.*\.so\.\d+$")
INTERP = {"aarch64": "/lib/ld-linux-aarch64.so.1", "x86_64": "/lib64/ld-linux-x86-64.so.2"}
MACHINE = {"EM_AARCH64": "aarch64", "EM_X86_64": "x86_64"}


def run(*cmd):
    subprocess.run([str(c) for c in cmd], check=True)


def is_elf(path):
    with open(path, "rb") as f:
        return f.read(4) == b"\x7fELF"


def elf_info(path):
    """(machine, needed, runpath, interp, glibc versions needed)"""
    with open(path, "rb") as f:
        elf = ELFFile(f)
        needed, runpath, interp, versions = [], [], None, set()
        for sec in elf.iter_sections():
            if isinstance(sec, DynamicSection):
                for tag in sec.iter_tags():
                    if tag.entry.d_tag == "DT_NEEDED":
                        needed.append(tag.needed)
                    elif tag.entry.d_tag in ("DT_RUNPATH", "DT_RPATH"):
                        runpath += tag.runpath.split(":") if hasattr(tag, "runpath") else tag.rpath.split(":")
            elif isinstance(sec, GNUVerNeedSection):
                for _verneed, auxs in sec.iter_versions():
                    for aux in auxs:
                        versions.add(aux.name)
        for seg in elf.iter_segments():
            if seg.header.p_type == "PT_INTERP":
                interp = seg.get_interp_name()
        return MACHINE.get(elf["e_machine"], elf["e_machine"]), needed, runpath, interp, versions


def resolve(name, runpath, origin):
    for d in runpath:
        d = d.replace("$ORIGIN", str(origin)).replace("${ORIGIN}", str(origin))
        p = Path(d) / name
        if p.exists():
            return p.resolve()
    return None


def hashed_name(path):
    h = hashlib.sha256(path.read_bytes()).hexdigest()[:8]
    name = path.name
    stem, _, rest = name.partition(".so")
    return f"{stem}-{h}.so{rest}"


def glibc_minor(versions):
    minors = [int(m.group(1)) for v in versions if (m := re.match(r"GLIBC_2\.(\d+)", v))]
    return max(minors, default=17)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wheel", required=True, help="the pure pandare2 wheel")
    ap.add_argument("--engine", required=True, help="engine prefix (lib/, share/qemu/)")
    ap.add_argument("--plugins", required=True, help="plugin dir (lib/panda/plugins)")
    ap.add_argument("--launcher", required=True, help="the panda-qemu launcher binary")
    ap.add_argument("--arches", required=True, help="comma-separated targets, e.g. x86_64,aarch64")
    ap.add_argument("--max-glibc", type=int, default=38, help="fail if anything needs GLIBC_2.N, N > this")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    arches = a.arches.split(",")

    work = Path(tempfile.mkdtemp())
    run(sys.executable, "-m", "wheel", "unpack", "-d", work, a.wheel)
    (root,) = list(work.iterdir())
    pkg = root / "pandare2"
    data = pkg / "data"
    libs = root / "pandare2.libs"
    for d in (data / "lib", data / "bin", data / "share", libs):
        d.mkdir(parents=True, exist_ok=True)

    engine = Path(a.engine)
    for arch in arches:
        shutil.copy(engine / "lib" / f"libpanda-{arch}-softmmu.so", data / "lib")
    shutil.copy(engine / "lib" / "libpanda_plugin_interface.so", data / "lib")
    shutil.copytree(a.plugins, data / "plugins", symlinks=False)
    shutil.copytree(engine / "share" / "qemu", data / "share" / "qemu", symlinks=False)
    shutil.copy(a.launcher, data / "bin" / "panda-qemu")
    for p in [data, *data.rglob("*")]:
        p.chmod(p.stat().st_mode | stat.S_IWUSR)

    # Plugins for targets the wheel doesn't ship.
    for p in list((data / "plugins").rglob("*-softmmu.so")):
        if not any(p.name.endswith(f"_{arch}-softmmu.so") for arch in arches):
            p.unlink()

    # share/qemu has guest firmware ELFs: leave those alone.
    elfs = [p for d in ("lib", "plugins", "bin") for p in (data / d).rglob("*")
            if p.is_file() and not p.is_symlink() and is_elf(p)]
    bundled = {}  # source path -> bundled path
    machines = set()
    queue = list(elfs)
    deps = {}  # elf -> [(needed name, source path)]
    while queue:
        f = queue.pop()
        machine, needed, runpath, _interp, _v = elf_info(f)
        machines.add(machine)
        origin = f.parent
        # A bundled copy keeps resolving against where it came from.
        src_origin = next((s.parent for s, b in bundled.items() if b == f), origin)
        deps[f] = []
        for n in needed:
            if GLIBC.match(n):
                continue
            src = resolve(n, runpath, src_origin)
            if src is None:
                # e.g. the engine's own libraries, already next to it
                if (origin / n).exists():
                    continue
                sys.exit(f"repair: {f}: cannot find {n} in {runpath}")
            if src not in bundled:
                dst = libs / hashed_name(src)
                shutil.copy(src, dst)
                dst.chmod(0o755)
                bundled[src] = dst
                queue.append(dst)
            deps[f].append((n, bundled[src]))

    (machine,) = machines
    for f, ds in deps.items():
        args = []
        for n, b in ds:
            args += ["--replace-needed", n, b.name]
        if f.parent == libs:
            args += ["--set-soname", f.name]
        rel = os.path.relpath(libs, f.parent)
        args += ["--set-rpath", "$ORIGIN" if rel == "." else f"$ORIGIN/{rel}"]
        if f.name == "panda-qemu":
            args += ["--set-interpreter", INTERP[machine]]
        run("patchelf", *args, f)

    # Check: nothing points into the nix store, and the glibc floor.
    worst = 0
    for f in list(deps):
        _m, needed, runpath, interp, versions = elf_info(f)
        bad = [r for r in runpath if "/nix/store" in r] + ([interp] if interp and "/nix/store" in interp else [])
        if bad:
            sys.exit(f"repair: {f} still refers to {bad}")
        n = glibc_minor(versions)
        if n > a.max_glibc:
            sys.exit(f"repair: {f} needs GLIBC_2.{n} (> 2.{a.max_glibc}): {sorted(versions)}")
        worst = max(worst, n)
    plat = f"manylinux_2_{worst}_{machine}"
    print(f"repair: {len(deps)} ELF files, {len(bundled)} libraries bundled, needs glibc 2.{worst}: {plat}")

    # WHEEL: platform-specific now. Console scripts: qemu-system-<arch>.
    (dist_info,) = list(root.glob("*.dist-info"))
    wheel_file = dist_info / "WHEEL"
    lines = [l for l in wheel_file.read_text().splitlines()
             if l.strip() and not l.startswith(("Tag:", "Root-Is-Purelib:"))]
    lines += ["Root-Is-Purelib: false", f"Tag: py3-none-{plat}"]
    wheel_file.write_text("\n".join(lines) + "\n")
    eps = dist_info / "entry_points.txt"
    text = eps.read_text() if eps.exists() else ""
    if "[console_scripts]" not in text:
        text += "[console_scripts]\n"
    text += "".join(f"qemu-system-{arch} = pandare2.qemu:main\n" for arch in arches)
    eps.write_text(text)

    os.makedirs(a.out, exist_ok=True)
    run(sys.executable, "-m", "wheel", "pack", "-d", a.out, root)
    shutil.rmtree(work)


if __name__ == "__main__":
    main()
