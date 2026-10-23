#!/usr/bin/env python3
"""Turn the pure-Python pandare2 wheel into a self-contained manylinux wheel.

What it adds to the wheel (under pandare2/data/):
  lib/             libpanda-<arch>-softmmu.so for each target, libpanda_plugin_interface.so
  plugins/         the PANDA plugins (the layout pandare2 and syscalls2 expect)
  share/qemu/      firmware, keymaps
  bin/panda-qemu   the qemu-system-<arch> launcher (console scripts exec it)

Then, like auditwheel repair, it copies every shared library those need, except glibc's, into
pandare2.libs/ under a content-hashed name (so it never clashes with a library the host process
already has loaded), points each ELF's DT_NEEDED at the copies and its RUNPATH at
$ORIGIN:$ORIGIN/<to pandare2.libs> (plus the directory of any in-wheel sibling it needs), sets the
launcher's interpreter to the standard one, and checks that nothing refers to /nix/store any more.
Unlike auditwheel it also bundles libstdc++ and libgcc_s: nix's gcc is newer than the hosts'.
A library the wheel already ships (an engine library, a plugin) is never copied a second time, and
no soname is bundled twice: two copies would load as two instances with separate globals.

Licenses (--licenses, the flake's wheelLicenses): every bundled library must come from a package
in its manifest.json. The license files of the engine, panda-ng and each package used go into
pandare2-*.dist-info/licenses/<name>/, SOURCES.txt there says where each one's corresponding source
is, and METADATA gets a License-Expression for the combined wheel (Metadata-Version 2.4).

The platform tag is manylinux_2_<N>_<arch>, N being the highest GLIBC_2.N symbol version any ELF in
the wheel needs (it is checked against --max-glibc).

Run (the flake's .#wheel does this):
  repair.py --wheel dist/pandare2-*.whl --engine ENGINE --plugins PLUGINDIR --launcher BIN \
    --arches x86_64,... --licenses LICDIR --out DIR
"""
import argparse
import hashlib
import json
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

# glibc's own libraries: always the host's. (Not libcrypt or libnsl: glibc no longer builds them,
# so hosts may lack them; if anything needs them they are bundled like any other library.)
GLIBC = re.compile(r"^(libc|libm|libdl|libpthread|librt|libresolv|libutil|libanl|libmvec|"
                   r"libBrokenLocale)\.so\.\d+$|^ld-linux.*\.so\.\d+$")
INTERP = {"aarch64": "/lib/ld-linux-aarch64.so.1", "x86_64": "/lib64/ld-linux-x86-64.so.2"}
MACHINE = {"EM_AARCH64": "aarch64", "EM_X86_64": "x86_64"}


def run(*cmd):
    subprocess.run([str(c) for c in cmd], check=True)


def is_elf(path):
    with open(path, "rb") as f:
        return f.read(4) == b"\x7fELF"


def elf_info(path):
    """(machine, needed, runpath, interp, glibc versions needed, soname)"""
    with open(path, "rb") as f:
        elf = ELFFile(f)
        needed, runpath, interp, versions, soname = [], [], None, set(), None
        for sec in elf.iter_sections():
            if isinstance(sec, DynamicSection):
                for tag in sec.iter_tags():
                    if tag.entry.d_tag == "DT_NEEDED":
                        needed.append(tag.needed)
                    elif tag.entry.d_tag == "DT_SONAME":
                        soname = tag.soname
                    elif tag.entry.d_tag in ("DT_RUNPATH", "DT_RPATH"):
                        runpath += tag.runpath.split(":") if hasattr(tag, "runpath") else tag.rpath.split(":")
            elif isinstance(sec, GNUVerNeedSection):
                for _verneed, auxs in sec.iter_versions():
                    for aux in auxs:
                        versions.add(aux.name)
        for seg in elf.iter_segments():
            if seg.header.p_type == "PT_INTERP":
                interp = seg.get_interp_name()
        return MACHINE.get(elf["e_machine"], elf["e_machine"]), needed, runpath, interp, versions, soname


def dynsyms(path):
    """The dynamic symbol names (patchelf must leave them alone)."""
    with open(path, "rb") as f:
        sec = ELFFile(f).get_section_by_name(".dynsym")
        return [s.name for s in sec.iter_symbols()] if sec else []


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


def spdx_join(exprs):
    """AND of the distinct expressions, compound ones parenthesized."""
    out = []
    for e in exprs:
        e = f"({e})" if (" OR " in e or " AND " in e) else e
        if e not in out:
            out.append(e)
    expr = " AND ".join(out)
    try:
        from packaging.licenses import canonicalize_license_expression
    except ImportError:  # packaging < 24.2: no validation
        return expr
    return canonicalize_license_expression(expr)


def nix_store_strings(path):
    """The /nix/store/... strings in a file (up to the next NUL or whitespace)."""
    data = path.read_bytes()
    return {m.group(0).decode(errors="replace") for m in re.finditer(rb"/nix/store/[^\x00\s'\"]*", data)}


def write_licenses(dist_info, licdir, used, bundled_by_entry):
    """dist-info/licenses/: each component's license files and SOURCES.txt. Returns the License-File
    paths (relative to licenses/) and the SPDX expressions."""
    man = json.loads((licdir / "manifest.json").read_text())
    dst = dist_info / "licenses"
    dst.mkdir(exist_ok=True)
    comps = [man["panda-ng"], man["engine"]] + [e for e in man["thirdParty"] if e["name"] in used]
    lines = [
        "pandare2: what this wheel contains, under which license, and where the source is.",
        "",
        "The wheel combines pandare2 and the PANDA plugins (panda-ng), the PANDA engine (QEMU, GPL-2.0-only)",
        "and the libraries below, which are bundled in pandare2.libs/. Each component's license files",
        "are in the directory of the same name next to this file.",
        "",
    ]
    for c in comps:
        if c["name"] == "panda-ng":
            src = None  # no license file in panda-ng yet; setup.cfg says MIT
        else:
            src = licdir / c["name"]
            if not src.is_dir() or not any(src.iterdir()):
                sys.exit(f"repair: no license files for {c['name']} in {licdir}")
            shutil.copytree(src, dst / c["name"], dirs_exist_ok=True)
        lines.append(f"{c['name']} {c.get('version', '')}".rstrip() + f": {c['spdx']}")
        lines += [f"  {s}" for s in c.get("source", [])]
        if c.get("urls"):
            lines.append(f"  source: {' '.join(u for u in c['urls'] if u)}")
        if c["name"] in bundled_by_entry:
            lines.append(f"  bundled: {' '.join(sorted(bundled_by_entry[c['name']]))}")
        lines.append("")
    (dst / "SOURCES.txt").write_text("\n".join(lines))
    files = sorted(str(p.relative_to(dst)) for p in dst.rglob("*") if p.is_file())
    for p in dst.rglob("*"):
        p.chmod(0o755 if p.is_dir() else 0o644)
    return files, [c["spdx"] for c in comps]


def rewrite_metadata(dist_info, expr, license_files):
    """License-Expression and License-File (PEP 639, Metadata-Version 2.4) instead of License and
    the License :: classifiers, which described only the Python package."""
    md = dist_info / "METADATA"
    text = md.read_text()
    head, sep, body = text.partition("\n\n")
    out = []
    for l in head.splitlines():
        if l.startswith(("License:", "License-File:", "License-Expression:", "Classifier: License ::")):
            continue
        if l.startswith("Metadata-Version:"):
            l = "Metadata-Version: 2.4"
        out.append(l)
    out.append(f"License-Expression: {expr}")
    out += [f"License-File: {f}" for f in license_files]
    md.write_text("\n".join(out) + sep + body)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wheel", required=True, help="the pure pandare2 wheel")
    ap.add_argument("--engine", required=True, help="engine prefix (lib/, share/qemu/)")
    ap.add_argument("--plugins", required=True, help="plugin dir (lib/panda/plugins)")
    ap.add_argument("--launcher", required=True, help="the panda-qemu launcher binary")
    ap.add_argument("--arches", required=True, help="comma-separated targets, e.g. x86_64,aarch64")
    ap.add_argument("--licenses", required=True, help="license dir: manifest.json, <name>/ (the flake's wheelLicenses)")
    ap.add_argument("--max-glibc", type=int, default=38, help="fail if anything needs GLIBC_2.N, N > this")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    with tempfile.TemporaryDirectory() as work:
        repair(a, Path(work))


def repair(a, work):
    arches = a.arches.split(",")
    licdir = Path(a.licenses)
    manifest = json.loads((licdir / "manifest.json").read_text())

    run(sys.executable, "-m", "wheel", "unpack", "-d", work, a.wheel)
    (root,) = list(work.iterdir())
    # py3-none is honest only because the cffi bindings are ABI mode: no compiled module.
    if compiled := [str(p.relative_to(root)) for p in root.rglob("*") if p.is_file() and is_elf(p)]:
        sys.exit(f"repair: the pure wheel already has compiled files: {compiled}")
    pkg = root / "pandare2"
    data = pkg / "data"
    libs = root / "pandare2.libs"
    for d in (data / "lib", data / "bin", data / "share", libs):
        d.mkdir(parents=True, exist_ok=True)

    # What the wheel ships, by the real path of its source and of itself: a dependency that
    # resolves to one of these is a sibling in the wheel, never a library to bundle.
    shipped = {}
    engine = Path(a.engine)
    for name in [f"libpanda-{arch}-softmmu.so" for arch in arches] + ["libpanda_plugin_interface.so"]:
        shutil.copy(engine / "lib" / name, data / "lib")
        shipped[(engine / "lib" / name).resolve()] = data / "lib" / name
    shutil.copytree(a.plugins, data / "plugins", symlinks=False)
    for p in (data / "plugins").rglob("*"):
        if p.is_file():
            shipped[(Path(a.plugins) / p.relative_to(data / "plugins")).resolve()] = p
    shutil.copytree(engine / "share" / "qemu", data / "share" / "qemu", symlinks=False)
    shutil.copy(a.launcher, data / "bin" / "panda-qemu")
    shipped[Path(a.launcher).resolve()] = data / "bin" / "panda-qemu"
    for p in [data, *data.rglob("*")]:
        p.chmod(p.stat().st_mode | stat.S_IWUSR)

    # Plugins for targets the wheel doesn't ship.
    for p in list((data / "plugins").rglob("*-softmmu.so")):
        if not any(p.name.endswith(f"_{arch}-softmmu.so") for arch in arches):
            p.unlink()
    shipped = {s: d for s, d in shipped.items() if d.exists()}
    shipped.update({d.resolve(): d for d in list(shipped.values())})

    # share/qemu has guest firmware ELFs: leave those alone.
    elfs = [p for d in ("lib", "plugins", "bin") for p in (data / d).rglob("*")
            if p.is_file() and not p.is_symlink() and is_elf(p)]
    bundled = {}  # source path -> bundled path
    bundled_soname = {}  # original soname -> source path
    machines = set()
    queue = list(elfs)
    deps = {}  # elf -> [(needed name, path in the wheel)]
    rpath_dirs = {}  # elf -> extra in-wheel dirs (siblings it needs)
    while queue:
        f = queue.pop()
        machine, needed, runpath, _interp, _v, _so = elf_info(f)
        machines.add(machine)
        # A bundled copy keeps resolving against where it came from.
        src_origin = next((s.parent for s, b in bundled.items() if b == f), f.parent)
        deps[f], rpath_dirs[f] = [], set()
        for n in needed:
            if GLIBC.match(n):
                continue
            if "/" in n:
                sys.exit(f"repair: {f}: DT_NEEDED {n} is a path")
            src = resolve(n, runpath + ["$ORIGIN"], src_origin)
            if src is None:
                sys.exit(f"repair: {f}: cannot find {n} in {runpath} or next to it")
            if src in shipped:
                # A sibling in the wheel (another plugin, the plugin interface library).
                dst = shipped[src]
                if dst.parent != f.parent:
                    rpath_dirs[f].add(dst.parent)
                if dst.name != n:
                    deps[f].append((n, dst))
                continue
            if src not in bundled:
                soname = elf_info(src)[5] or src.name
                if soname in bundled_soname and bundled_soname[soname] != src:
                    sys.exit(f"repair: two different libraries with soname {soname}: "
                             f"{bundled_soname[soname]} and {src}; the wheel would load both")
                bundled_soname[soname] = src
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
        rpath = ["$ORIGIN"]
        for d in [libs, *sorted(rpath_dirs[f])]:
            rel = os.path.relpath(d, f.parent)
            if rel != ".":
                rpath.append(f"$ORIGIN/{rel}")
        args += ["--set-rpath", ":".join(rpath)]
        if f.name == "panda-qemu":
            args += ["--set-interpreter", INTERP[machine]]
        before = dynsyms(f)
        run("patchelf", *args, f)
        # patchelf blanks the old RUNPATH string with 'X's; a symbol name the linker tail-merged
        # into it (e.g. "b" into ".../lib") would be renamed.
        if dynsyms(f) != before:
            changed = [(x, y) for x, y in zip(before, dynsyms(f)) if x != y]
            sys.exit(f"repair: patchelf changed dynamic symbol names in {f}: {changed[:5]}")

    # Checks: nothing loads from the nix store, no library is in the wheel twice, the glibc floor.
    worst = 0
    store_refs = {}  # string -> files
    for f in list(deps):
        _m, needed, runpath, interp, versions, _so = elf_info(f)
        bad = ([r for r in runpath if "/nix/store" in r or not r.startswith("$ORIGIN")]
               + [n for n in needed if "/" in n]
               + ([interp] if interp and "/nix/store" in interp else []))
        if bad:
            sys.exit(f"repair: {f} still refers to {bad}")
        n = glibc_minor(versions)
        if n > a.max_glibc:
            sys.exit(f"repair: {f} needs GLIBC_2.{n} (> 2.{a.max_glibc}): {sorted(versions)}")
        worst = max(worst, n)
        for s in nix_store_strings(f):
            store_refs.setdefault(re.sub(r"^/nix/store/[a-z0-9]{32}-", "/nix/store/*-", s), set()).add(f.name)
    # A bundled library must not be one the wheel ships itself (the dependency resolution above
    # never copies those; this checks it).
    shipped_names = set()
    for f in deps:
        if f.parent != libs:
            shipped_names |= {f.name, elf_info(f)[5] or f.name}
    if again := sorted(set(bundled_soname) & shipped_names):
        sys.exit(f"repair: bundled a second copy of {again}")
    # Compiled-in paths (QEMU's CONFIG_QEMU_*DIR, the libraries' data and config paths): the
    # engine finds its firmware relative to itself and pandare2 passes -L, so these are only
    # fallbacks that point nowhere on the host. Listed so the set is known.
    for s, fs in sorted(store_refs.items()):
        print(f"repair: warning: compiled-in {s} in {len(fs)} file(s), e.g. {sorted(fs)[0]}")
    plat = f"manylinux_2_{worst}_{machine}"
    print(f"repair: {len(deps)} ELF files, {len(bundled)} libraries bundled, needs glibc 2.{worst}: {plat}")

    # Which package each bundled library comes from (for its license and source).
    bundled_by_entry = {}
    for src, dst in bundled.items():
        owner = next((e["name"] for e in manifest["thirdParty"]
                      if any(str(src).startswith(p + "/") for p in e["prefixes"])), None)
        if owner is None:
            sys.exit(f"repair: {src} (bundled as {dst.name}) is from no package in the license manifest; "
                     f"add it to wheelThirdParty in flake.nix")
        bundled_by_entry.setdefault(owner, set()).add(dst.name)
    unused = [e["name"] for e in manifest["thirdParty"] if e["name"] not in bundled_by_entry]
    if unused:
        print(f"repair: license manifest entries not bundled (left out): {' '.join(unused)}")

    (dist_info,) = list(root.glob("*.dist-info"))
    license_files, exprs = write_licenses(dist_info, licdir, set(bundled_by_entry), bundled_by_entry)
    expr = spdx_join(exprs)
    rewrite_metadata(dist_info, expr, license_files)
    print(f"repair: License-Expression: {expr}")

    # WHEEL: platform-specific now. Console scripts: qemu-system-<arch>.
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


if __name__ == "__main__":
    main()
