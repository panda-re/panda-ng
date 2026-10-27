#!/usr/bin/env python3
"""Test repair.py on a small synthetic engine (no nix build needed):

  - plugin a needs plugin b through RUNPATH $ORIGIN, and libpanda_plugin_interface.so through an
    absolute RUNPATH to the engine: both are shipped, so neither may be copied to pandare2.libs;
  - plugin b and the engine need libdep.so from outside: it is bundled once;
  - plugin b and the per-arch plugin interface need the engine (through an absolute RUNPATH and
    $ORIGIN): it is shipped, so it is not bundled, and their RUNPATHs reach it in the wheel;
  - every RUNPATH starts with $ORIGIN; the licenses and License-Expression are written;
  - loading a (which loads b) from the unpacked wheel maps b once.

Needs cc, readelf, patchelf and a Python with wheel, pyelftools and packaging, e.g.
  nix shell --impure --expr "with import <nixpkgs> {}; [ patchelf gcc binutils
    (python3.withPackages (ps: [ ps.wheel ps.pyelftools ps.packaging ])) ]" -c python3 tools/wheel/test_repair.py
"""
import ctypes
import json
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent


def sh(*cmd, **kw):
    r = subprocess.run([str(c) for c in cmd], text=True, capture_output=True, **kw)
    if r.returncode:
        sys.exit(f"TEST_REPAIR FAIL: {cmd[:3]}... rc={r.returncode}\n{r.stdout}\n{r.stderr}")
    return r.stdout


def cc(out, src, *args):
    c = out.with_suffix(".c")
    c.write_text(src)
    sh("cc", "-shared", "-fPIC", "-o", out, c, *args)
    c.unlink()


def dyn(path):
    return sh("readelf", "-d", path)


def main():
    t = Path(tempfile.mkdtemp())
    deps = t / "store" / "dep-1.0" / "lib"
    eng = t / "store" / "engine"
    plug = t / "store" / "plugins"
    for d in (deps, eng / "lib", eng / "share" / "qemu", plug, t / "lic" / "dep", t / "lic" / "qemu-panda-engine"):
        d.mkdir(parents=True)
    cc(deps / "libdep.so.1", "int dep(void){return 40;}", "-Wl,-soname,libdep.so.1")
    (deps / "libdep.so").symlink_to("libdep.so.1")
    cc(eng / "lib" / "libpanda_plugin_interface.so", "int iface(void){return 1;}")
    cc(eng / "lib" / "libpanda-x86_64-softmmu.so", "int dep(void); int eng(void){return dep();}",
       f"-L{deps}", "-ldep", f"-Wl,-rpath,{deps}")
    cc(eng / "lib" / "libpanda_plugin_interface-x86_64-softmmu.so", "int eng(void); int iface2(void){return eng();}",
       f"-L{eng}/lib", "-l:libpanda-x86_64-softmmu.so", "-Wl,-rpath,$ORIGIN")
    (eng / "share" / "qemu" / "bios.bin").write_bytes(b"x")
    cc(plug / "libpanda-b_x86_64-softmmu.so", "int dep(void); int eng(void); int b_value(void){return dep()+1+eng()-eng();}",
       f"-L{deps}", "-ldep", f"-Wl,-rpath,{deps}", f"-L{eng}/lib", "-l:libpanda-x86_64-softmmu.so",
       f"-Wl,-rpath,{eng}/lib")
    cc(plug / "libpanda-a_x86_64-softmmu.so", "int b_value(void); int iface(void); int a_value(void){return b_value()+iface();}",
       f"-L{plug}", "-lpanda-b_x86_64-softmmu", f"-L{eng}/lib", "-lpanda_plugin_interface",
       "-Wl,-rpath,$ORIGIN", f"-Wl,-rpath,{eng}/lib")
    launcher = t / "panda-qemu"
    (t / "l.c").write_text("int main(void){return 0;}")
    sh("cc", "-o", launcher, t / "l.c")

    (t / "lic" / "dep" / "COPYING").write_text("dep license")
    (t / "lic" / "qemu-panda-engine" / "COPYING").write_text("GPLv2")
    (t / "lic" / "panda-ng" / "LICENSES").mkdir(parents=True)
    (t / "lic" / "panda-ng" / "LICENSE").write_text("MIT")
    (t / "lic" / "panda-ng" / "LICENSES" / "GPL-2.0-only.txt").write_text("GPLv2")
    (t / "lic" / "manifest.json").write_text(json.dumps({
        "engine": {"name": "qemu-panda-engine", "spdx": "GPL-2.0-only", "source": ["engine src"]},
        "panda-ng": {"name": "panda-ng", "spdx": "MIT AND GPL-2.0-only AND GPL-2.0-or-later",
                     "source": ["panda-ng src"]},
        "thirdParty": [{"name": "dep", "version": "1.0", "spdx": "BSD-3-Clause OR GPL-2.0-only",
                        "urls": ["https://example.invalid/dep.tar.gz"], "prefixes": [str(deps.parent)]}],
    }))

    pure = t / "pure" / "pandare2-0.0.0"
    (pure / "pandare2").mkdir(parents=True)
    (pure / "pandare2" / "__init__.py").write_text("")
    di = pure / "pandare2-0.0.0.dist-info"
    di.mkdir()
    (di / "METADATA").write_text("Metadata-Version: 2.1\nName: pandare2\nVersion: 0.0.0\nLicense: MIT\n"
                                 "Classifier: License :: OSI Approved :: MIT License\n\nbody\n")
    (di / "WHEEL").write_text("Wheel-Version: 1.0\nGenerator: test\nRoot-Is-Purelib: true\nTag: py3-none-any\n")
    (di / "RECORD").write_text("")
    (t / "purewhl").mkdir()
    sh(sys.executable, "-m", "wheel", "pack", "-d", t / "purewhl", pure)
    (whl,) = (t / "purewhl").glob("*.whl")

    out = sh(sys.executable, HERE / "repair.py", "--wheel", whl, "--engine", eng, "--plugins", plug,
             "--launcher", launcher, "--arches", "x86_64", "--licenses", t / "lic", "--max-glibc", "99",
             "--out", t / "out")
    print(out)
    (res,) = (t / "out").glob("*.whl")
    x = t / "x"
    zipfile.ZipFile(res).extractall(x)
    libs = sorted(p.name for p in (x / "pandare2.libs").iterdir())
    names = zipfile.ZipFile(res).namelist()
    fails = []
    if len(libs) != 1 or not libs[0].startswith("libdep-"):
        fails.append(f"pandare2.libs should hold only libdep-<hash>: {libs}")
    pa = x / "pandare2" / "data" / "plugins" / "libpanda-a_x86_64-softmmu.so"
    da = dyn(pa)
    print(da)
    if "[libpanda-b_x86_64-softmmu.so]" not in da or "[libpanda_plugin_interface.so]" not in da:
        fails.append("plugin a must keep needing its in-wheel siblings by name")
    if "Library runpath: [$ORIGIN:$ORIGIN/../../../pandare2.libs:$ORIGIN/../lib]" not in da:
        fails.append("plugin a's RUNPATH should be $ORIGIN, the libs dir and ../lib")
    db = dyn(x / "pandare2" / "data" / "plugins" / "libpanda-b_x86_64-softmmu.so")
    if ("[libpanda-x86_64-softmmu.so]" not in db
            or "Library runpath: [$ORIGIN:$ORIGIN/../../../pandare2.libs:$ORIGIN/../lib]" not in db):
        fails.append(f"plugin b must need the shipped engine by name, through ../lib: {db}")
    di = dyn(x / "pandare2" / "data" / "lib" / "libpanda_plugin_interface-x86_64-softmmu.so")
    if "[libpanda-x86_64-softmmu.so]" not in di:
        fails.append(f"the per-arch interface must need the shipped engine by name: {di}")
    for p in x.rglob("*"):
        if p.is_file() and not p.is_symlink() and p.read_bytes()[:4] == b"\x7fELF":
            for l in dyn(p).splitlines():
                if "runpath" in l and "[$ORIGIN" not in l:
                    fails.append(f"{p.name}: {l.strip()}")
    md = next(n for n in names if n.endswith(".dist-info/METADATA"))
    meta = zipfile.ZipFile(res).read(md).decode()
    print(meta)
    want = ("License-Expression: MIT AND GPL-2.0-only AND GPL-2.0-or-later AND "
            "(BSD-3-Clause OR GPL-2.0-only)")
    if want not in meta or "License: MIT" in meta or "License ::" in meta or "Metadata-Version: 2.4" not in meta:
        fails.append("METADATA license fields")
    for f in ("licenses/dep/COPYING", "licenses/qemu-panda-engine/COPYING", "licenses/SOURCES.txt",
              "licenses/panda-ng/LICENSE", "licenses/panda-ng/LICENSES/GPL-2.0-only.txt"):
        if not any(n.endswith(".dist-info/" + f) for n in names):
            fails.append(f"missing {f}")
        if f"License-File: {f.removeprefix('licenses/')}" not in meta:
            fails.append(f"no License-File: {f}")
    # Load a: b (and libdep) must be mapped once.
    if len(sys.argv) > 1:  # keep the tree for debugging
        print("TREE", x)
        sys.exit(0)
    lib = ctypes.CDLL(str(pa))
    if lib.a_value() != 42:
        fails.append(f"a_value() = {lib.a_value()}")
    maps = Path("/proc/self/maps").read_text()
    nb = len({l.split()[-1] for l in maps.splitlines() if "libpanda-b_" in l})
    nd = len({l.split()[-1] for l in maps.splitlines() if "libdep" in l})
    print(f"mapped: libpanda-b x{nb}, libdep x{nd}")
    if nb != 1 or nd != 1:
        fails.append(f"libpanda-b mapped {nb}x, libdep {nd}x")
    if fails:
        print("TEST_REPAIR FAIL:\n  " + "\n  ".join(fails))
        sys.exit(1)
    print("TEST_REPAIR PASS")


if __name__ == "__main__":
    main()
