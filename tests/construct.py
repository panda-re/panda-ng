"""Panda() without a guest: the command line each call form builds, which keyword wins over an
image, and the errors. No engine runs (nothing calls run()), so no image or disk is needed: the
images here are written to a temporary directory and name a qcow that doesn't exist. Each case
constructs one Panda in a child process of its own, since a process has one Panda per arch.

Run: panda-python tests/construct.py [case...]      (default: every case)

- legacy: every way tests/*.py constructs a Panda builds the same argv as before (the monitor's
  arguments aside, which `monitor=` changed on purpose), except that extra_args= now go after the
  image's rather than before (no test's arguments conflict with its image's);
- generic: the image is the base and keywords override it; another arch raises ValueError. It
  can be YAML (extra_args as a list here) or a dict, and its isf and kconf_file are kept.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

# Images like the ones pandare2.mkimage writes (tests/README.md), for three arches whose disks
# are attached differently: x86_64 a bare qcow, aarch64 virtio-blk-pci under rr, riscv64 a virtio
# drive and firmware named through {biospath}.
IMAGES = {
    "x86": {"arch": "x86_64", "os": "linux-64-debian:6.12.113+deb13-amd64",
            "prompt": "root@[\\w.-]+:[^#\\n]*# ", "cdrom": None, "snapshot": "root",
            "qcow": "x86.qcow2", "default_mem": "1G",
            "extra_args": "-display none -nic user,model=virtio-net-pci",
            "isf": "x86.isf.json.xz", "kconf_file": "x86.kernelinfo.conf"},
    "arm64": {"arch": "aarch64", "os": "linux-64-debian:6.12.113+deb13-arm64",
              "prompt": "root@[\\w.-]+:[^#\\n]*# ", "cdrom": None, "snapshot": "root",
              "qcow": "arm64.qcow2", "default_mem": "1G",
              "extra_args": "-M virt -cpu max -bios edk2-aarch64-code.fd -display none "
                            "-nic user,model=virtio-net-pci",
              "isf": "arm64.isf.json.xz", "kconf_file": "arm64.kernelinfo.conf"},
    "rv64": {"arch": "riscv64", "os": "linux-64-debian:6.12.113+deb13-riscv64",
             "prompt": "root@[\\w.-]+:[^#\\n]*# ", "cdrom": None, "snapshot": "root",
             "qcow": "rv64.qcow2", "default_mem": "1G",
             "extra_args": "-M virt,pflash0=pf0 -blockdev driver=file,filename={biospath}/edk2-riscv-code.fd,"
                           "node-name=pf0,read-only=on -cpu rv64 -display none",
             "isf": "rv64.isf.json.xz", "kconf_file": "rv64.kernelinfo.conf"},
}

# x86's image in YAML, with extra_args as a list.
X86_YAML = """\
arch: x86_64
os: linux-64-debian:6.12.113+deb13-amd64
prompt: 'root@[\\w.-]+:[^#\\n]*# '
snapshot: root
qcow: x86.qcow2
default_mem: 1G
extra_args: [-display, none, -nic, "user,model=virtio-net-pci"]
isf: x86.isf.json.xz
kconf_file: x86.kernelinfo.conf
"""

PRE = ["{lib}", "-plugin", "{iface}", "-L", "{bios}"]
SERIAL = ["-serial", "unix:{serial},server,nowait"]
X86_IMG = ["{img}/x86.qcow2", "-display", "none", "-nic", "user,model=virtio-net-pci"]
RR = ["-accel", "tcg,thread=single,rr-ready=on"]
X86_RR_DISK = ["-blockdev", "driver=file,filename={img}/x86.qcow2,node-name=panda-disk-file",
               "-blockdev", "driver=qcow2,file=panda-disk-file,node-name=panda-disk-qcow2",
               "-blockdev", "driver=blkreplay,image=panda-disk-qcow2,node-name=panda-disk"]
ARM64_RR_DISK = [a.replace("x86", "arm64") for a in X86_RR_DISK]
ARM64_ARGS = ["-M", "virt", "-cpu", "max", "-bios", "edk2-aarch64-code.fd", "-display", "none",
              "-nic", "user,model=virtio-net-pci"]
RV64_ARGS = ["-M", "virt,pflash0=pf0", "-blockdev",
             "driver=file,filename={bios}/edk2-riscv-code.fd,node-name=pf0,read-only=on",
             "-cpu", "rv64", "-display", "none"]
MACHINE = {"arch": "aarch64", "calls": [["cpu", ["cortex-a53"]], ["ram", [0x40000000, "16M"]]]}

# name -> (Panda keywords, argv without the monitor's arguments). The forms are the ones in
# tests/*.py and examples/; "{img}" is the images' directory.
LEGACY = {
    "generic": ({"generic": "{img}/x86.json"},
                PRE + X86_IMG + ["-m", "1G"] + SERIAL),
    "generic rr": ({"generic": "{img}/x86.json", "rr": True},
                   PRE + X86_RR_DISK + X86_IMG[1:] + ["-device", "ide-hd,bus=ide.0,unit=0,drive=panda-disk"]
                   + RR + ["-m", "1G"] + SERIAL),
    "generic rr aarch64": ({"generic": "{img}/arm64.json", "rr": True},
                           PRE + ARM64_RR_DISK + ARM64_ARGS + ["-device", "virtio-blk-pci,drive=panda-disk"]
                           + RR + ["-m", "1G"] + SERIAL),
    "generic riscv64": ({"generic": "{img}/rv64.json"},
                        PRE + ["-drive", "file={img}/rv64.qcow2,if=virtio"] + RV64_ARGS + ["-m", "1G"] + SERIAL),
    "generic extra_args": ({"generic": "{img}/x86.json", "extra_args": ["-icount", "shift=6,sleep=off"]},
                           PRE + X86_IMG + ["-icount", "shift=6,sleep=off", "-m", "1G"] + SERIAL),
    "generic extra_args str": ({"generic": "{img}/x86.json", "extra_args": "-cpu Skylake-Client -snapshot"},
                               PRE + X86_IMG + ["-cpu", "Skylake-Client", "-snapshot", "-m", "1G"] + SERIAL),
    "generic rr extra_args": ({"generic": "{img}/x86.json", "rr": True, "extra_args": ["-icount", "shift=0"]},
                              PRE + X86_RR_DISK + X86_IMG[1:] + ["-icount", "shift=0"]
                              + ["-device", "ide-hd,bus=ide.0,unit=0,drive=panda-disk"]
                              + RR + ["-m", "1G"] + SERIAL),
    "arch os_version": ({"arch": "x86_64", "mem": "256M", "os_version": "linux-64-generic",
                         "extra_args": ["-display", "none", "-nodefaults"]},
                        PRE + ["-display", "none", "-nodefaults", "-m", "256M"]),
    "arch extra_args": ({"arch": "aarch64", "extra_args": ["-M", "configurable", "-cpu", "cortex-a53",
                                                           "-display", "none", "-serial", "none"]},
                        PRE + ["-M", "configurable", "-cpu", "cortex-a53", "-display", "none",
                               "-serial", "none", "-m", "128M"]),
    "arch mem extra_args": ({"arch": "mips", "mem": "128M", "extra_args": ["-M", "malta", "-display", "none"]},
                            PRE + ["-M", "malta", "-display", "none", "-m", "128M"]),
    "machine": ({"arch": "aarch64", "machine": MACHINE, "extra_args": ["-display", "none", "-serial", "none"]},
                PRE + ["{machine}", "-display", "none", "-serial", "none", "-m", "16384K"]),
}



def error(kind, *words):
    """A check that the constructor raised `kind` with each of `words` in its message."""
    return (f"raises {kind} ({', '.join(words)})",
            lambda r: r.get("error", [None])[0] == kind and all(w in r["error"][1] for w in words))


def attr(name, value):
    return (f"{name} = {value!r}", lambda r: r.get("attrs", {}).get(name) == value)


def argv_has(*args):
    def ok(r):
        a = r.get("argv", [])
        return any(a[i:i + len(args)] == list(args) for i in range(len(a)))
    return ("argv has " + " ".join(args), ok)


# name -> (Panda keywords, [(check name, check(result) -> bool)]).
CASES = {
    "generic mem": ({"generic": "{img}/x86.json", "mem": "2G"}, [attr("mem", "2G"), argv_has("-m", "2G")]),
    "generic os_version": ({"generic": "{img}/x86.json", "os_version": "linux-64-other:6.1"},
                           [attr("os", "linux-64-other:6.1")]),
    "generic qcow": ({"generic": "{img}/x86.json", "qcow": "{img}/other.qcow2"},
                     [attr("qcow", "{img}/other.qcow2"), argv_has("{img}/other.qcow2", "-display")]),
    "generic expect_prompt": ({"generic": "{img}/x86.json", "expect_prompt": "[$] "},
                              [attr("prompt", "[$] ")]),
    "generic image values": ({"generic": "{img}/x86.json"},
                             [attr("arch", "x86_64"), attr("mem", "1G"), attr("qcow", "{img}/x86.qcow2"),
                              attr("os", IMAGES["x86"]["os"]), attr("prompt", IMAGES["x86"]["prompt"])]),
    "generic same arch": ({"generic": "{img}/x86.json", "arch": "x86_64"}, [attr("arch", "x86_64")]),
    "generic other arch": ({"generic": "{img}/x86.json", "arch": "aarch64"},
                           [error("ValueError", "aarch64", "x86_64")]),
    "defaults": ({}, [attr("arch", "i386"), attr("mem", "128M"), attr("isf", None)]),
    "generic symbols": ({"generic": "{img}/x86.json"},
                        [attr("isf", "{img}/x86.isf.json.xz"), attr("kconf_file", "{img}/x86.kernelinfo.conf")]),
    "generic yaml": ({"generic": "{img}/x86.yaml"},
                     [("argv as from the json", lambda r: r.get("argv") == LEGACY["generic"][1]),
                      attr("prompt", IMAGES["x86"]["prompt"]), attr("os", IMAGES["x86"]["os"]),
                      attr("isf", "{img}/x86.isf.json.xz"), attr("kconf_file", "{img}/x86.kernelinfo.conf")]),
    "generic yml": ({"generic": "{img}/x86.yml"}, [("argv as from the json", lambda r: r.get("argv") == LEGACY["generic"][1])]),
    "generic dict": ({"generic": dict(IMAGES["x86"], qcow="{img}/x86.qcow2", isf="{img}/x86.isf.json.xz")},
                     [("argv as from the json", lambda r: r.get("argv") == LEGACY["generic"][1]),
                      attr("isf", "{img}/x86.isf.json.xz")]),
    "generic dict mem": ({"generic": dict(IMAGES["x86"], qcow="{img}/x86.qcow2"), "mem": "512M"},
                         [attr("mem", "512M")]),
    "generic dict no arch": ({"generic": {"qcow": "{img}/x86.qcow2"}}, [error("ValueError", "arch")]),
    "generic other extension": ({"generic": "{img}/x86.txt"}, [error("ValueError", ".yaml")]),
    "generic missing": ({"generic": "{img}/none.json"}, [error("FileNotFoundError", "none.json")]),
}


def normalize(p, img):
    '''p.panda_args with the paths that differ between hosts as placeholders, and the monitor's
    arguments split off.'''
    iface = p.panda_args[p.panda_args.index("-plugin") + 1] if "-plugin" in p.panda_args else None
    subs = [(p.libpanda_path, "{lib}"), (iface, "{iface}"), (p.biospath, "{bios}"), (img, "{img}")]
    argv, monitor = [], []
    args = list(p.panda_args)
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("-monitor", "-mon") or (a == "-chardev" and "pandare2-monitor" in args[i + 1]):
            monitor += [a, args[i + 1]]
            i += 2
            continue
        if a == p.libpanda_path or a == iface:
            argv.append(subs[0][1] if a == p.libpanda_path else "{iface}")
        else:
            for old, new in subs:
                if old:
                    a = a.replace(old, new)
            a = re.sub(r"^unix:[^,]*,server,nowait$", "unix:{serial},server,nowait", a)
            argv.append(a)
        i += 1
    return argv, monitor


def child(spec):
    from pandare2 import Panda, MachineBuilder
    img = spec["img"]
    kwargs = {}
    for k, v in spec["kwargs"].items():
        if k == "machine":
            m = MachineBuilder(v["arch"])
            for name, args in v["calls"]:
                m = getattr(m, name)(*args)
            v = m
        elif isinstance(v, str):
            v = v.replace("{img}", img)
        elif isinstance(v, dict):
            v = {dk: dv.replace("{img}", img) if isinstance(dv, str) else dv for dk, dv in v.items()}
        kwargs[k] = v
    res = {}
    try:
        p = Panda(**kwargs)
    except Exception as e:  # noqa: BLE001 - the case says which it wants
        res["error"] = [type(e).__name__, str(e)]
    else:
        res["argv"], res["monitor"] = normalize(p, img)
        res["attrs"] = {"arch": p.arch_name, "mem": p.mem, "os": p.os, "prompt": p.expect_prompt,
                        "qcow": p.qcow.replace(img, "{img}") if p.qcow else None,
                        "isf": p.isf.replace(img, "{img}") if p.isf else None,
                        "kconf_file": p.kconf_file.replace(img, "{img}") if p.kconf_file else None}
        if "machine" in kwargs:
            margs = kwargs["machine"].args()
            s = " ".join(res["argv"]).replace(" ".join(margs), "{machine}")
            res["argv"] = s.split(" ")
    print("RESULT " + json.dumps(res), flush=True)


def run_case(img, kwargs):
    spec = {"img": img, "kwargs": kwargs}
    r = subprocess.run([sys.executable, "-u", os.path.abspath(__file__), "--child", json.dumps(spec)],
                       capture_output=True, text=True, timeout=120)
    for line in r.stdout.splitlines():
        if line.startswith("RESULT "):
            return json.loads(line[len("RESULT "):]), r
    return None, r


def main():
    if sys.argv[1:2] == ["--child"]:
        child(json.loads(sys.argv[2]))
        return 0
    img = tempfile.mkdtemp(prefix="construct-")
    for name, image in IMAGES.items():
        with open(os.path.join(img, f"{name}.json"), "w") as f:
            json.dump(image, f)
    for ext in ("yaml", "yml"):
        with open(os.path.join(img, f"x86.{ext}"), "w") as f:
            f.write(X86_YAML)
    open(os.path.join(img, "x86.txt"), "w").close()
    only = sys.argv[1:]
    failed = False

    def check(name, ok, detail=""):
        nonlocal failed
        print(f"  {'PASS' if ok else 'FAIL'} {name}" + ("" if ok else f": {detail}"), flush=True)
        failed |= not ok

    for name, (kwargs, want) in LEGACY.items():
        if only and name not in only:
            continue
        res, r = run_case(img, kwargs)
        if res is None or "error" in res:
            check(f"legacy {name}", False, res or (r.stdout + r.stderr)[-2000:])
            continue
        check(f"legacy {name}: argv unchanged", res["argv"] == want, f"\n    got  {res['argv']}\n    want {want}")

    for name, (kwargs, checks) in CASES.items():
        if only and name not in only:
            continue
        res, r = run_case(img, kwargs)
        if res is None:
            check(name, False, (r.stdout + r.stderr)[-2000:])
            continue
        for what, ok in checks:
            check(f"{name}: {what}", ok(res), res)
    shutil.rmtree(img)
    print("CONSTRUCT " + ("FAIL" if failed else "PASS"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
