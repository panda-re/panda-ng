"""Panda() without a guest: the command line each call form builds, which keyword wins over an
image, and the errors. No engine runs (nothing calls run()), so no image or disk is needed: the
images here are written to a temporary directory and name a qcow that doesn't exist. Each case
constructs one Panda in a child process of its own, since a process has one Panda per arch.

Run: panda-python tests/construct.py [case...]      (default: every case)

- legacy: every way tests/*.py constructs a Panda builds the same argv as before, except where it
  changed on purpose: extra_args= now go after the image's rather than before (no test's
  arguments conflict with its image's), and the serial console and the monitor are sockets QEMU
  gets connected (-chardev socket,fd=N) instead of paths it listens on;
- generic: the image is the base and keywords override it; another arch raises ValueError. It
  can be YAML (extra_args as a list here) or a dict, and its isf and kconf_file are kept;
- accel: "tcg" by default, "kvm" checked at construction (host arch, /dev/kvm; the cases expect
  what this host can do), conflicts with -accel in extra_args and with rr=True raise, and the old
  extra_args=["-accel", "kvm"] maps to accel="kvm" with a DeprecationWarning; under KVM the
  TCG-only callbacks, hook and record raise NotImplementedError at the call, and the callbacks
  KVM calls register;
- monitor: QMP by default, HMP or none, as -chardev socket,fd=N on a socket QEMU gets connected
  (no path); raw_monitor=True is monitor=None, with a DeprecationWarning;
- paths: libpanda_path=, plugin_path=, biospath= and $PANDA_LIB that don't exist raise, $PANDA_LIB
  may name the engine's directory, and constructing a Panda doesn't change os.environ;
- quiet: constructing a Panda prints nothing (Python or C, stdout or stderr) and starts no
  thread; what it has to say goes to the pandare2 logger;
- before run: the methods that crashed the process before the engine started raise RuntimeError,
  and deleting or re-registering a callback works.
"""
import ctypes
import json
import logging
import os
import platform
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
import warnings

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
SERIAL = ["-chardev", "socket,id=pandare2-serial,fd={fd}", "-serial", "chardev:pandare2-serial"]
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

HOST = {"x86_64": "x86_64", "amd64": "x86_64", "aarch64": "aarch64", "arm64": "aarch64"}.get(platform.machine().lower())
KVM_OK = HOST is not None and os.access("/dev/kvm", os.R_OK | os.W_OK)
OTHER = "aarch64" if HOST == "x86_64" else "x86_64"     # a KVM arch that isn't this host's


def warned(kind, *words):
    return (f"warns {kind} ({', '.join(words)})",
            lambda r: any(w[0] == kind and all(x in w[1] for x in words) for w in r.get("warnings", [])))


def no_warning():
    return ("no warning", lambda r: "error" not in r and not r.get("warnings"))


def after(i, kind, *words):
    """A check on the i-th of the case's "after" calls: "ok", or the exception it raised."""
    def ok(r):
        a = r.get("after", [])
        return i < len(a) and a[i][0] == kind and all(w in a[i][1] for w in words)
    return (f"after[{i}] " + ("returns" if kind == "ok" else f"raises {kind} ({', '.join(words)})"), ok)


def kvm_or_error(*checks):
    """The checks if this host can run KVM for an x86_64 image, else the ValueError."""
    if HOST == "x86_64" and KVM_OK:
        return list(checks)
    return [error("ValueError", 'accel="kvm"')]


QMP = ["-chardev", "socket,id=pandare2-monitor,fd={fd}", "-object", "monitor-qmp,id=pandare2-qmp,chardev=pandare2-monitor"]
HMP = QMP[:3] + ["monitor-hmp,id=pandare2-hmp,chardev=pandare2-monitor"]
GENERIC_KVM = PRE + X86_IMG + ["-accel", "kvm", "-m", "1G"] + SERIAL
CASES.update({
    "accel default": ({"generic": "{img}/x86.json"}, [attr("accel", "tcg"), no_warning()]),
    "accel tcg": ({"generic": "{img}/x86.json", "accel": "tcg"},
                  [attr("accel", "tcg"), ("argv as without accel=", lambda r: r.get("argv") == LEGACY["generic"][1])]),
    "accel kvm": ({"generic": "{img}/x86.json", "accel": "kvm"},
                  kvm_or_error(attr("accel", "kvm"), ("argv has -accel kvm", lambda r: r.get("argv") == GENERIC_KVM),
                               no_warning())),
    "accel kvm legacy": ({"generic": "{img}/x86.json", "extra_args": ["-accel", "kvm"]},
                         kvm_or_error(attr("accel", "kvm"), ("argv unchanged", lambda r: r.get("argv") == GENERIC_KVM),
                                      warned("DeprecationWarning", 'accel="kvm"'))),
    "accel tcg legacy": ({"generic": "{img}/x86.json", "extra_args": ["-accel", "tcg,thread=single"]},
                         [attr("accel", "tcg"), argv_has("-accel", "tcg,thread=single"), no_warning()]),
    "accel kvm arch": ({"arch": "mips", "accel": "kvm"}, [error("ValueError", 'accel="kvm"', "x86_64 and aarch64")]),
    "accel kvm host": ({"arch": OTHER, "accel": "kvm"}, [error("ValueError", 'accel="kvm"', "host")]),
    "accel bogus": ({"accel": "hvf"}, [error("ValueError", "hvf", "'tcg'")]),
    "accel and -accel": ({"generic": "{img}/x86.json", "accel": "tcg", "extra_args": ["-accel", "kvm"]},
                         [error("ValueError", "accel='tcg'", "-accel")]),
    "accel and -enable-kvm": ({"generic": "{img}/x86.json", "accel": "kvm", "extra_args": ["-enable-kvm"]},
                              [error("ValueError", "-enable-kvm")]),
    "kvm callbacks": ({"generic": "{img}/x86.json", "accel": "kvm",
                       "after": ["p.cb_before_block_exec(name='c0')(lambda cpu, tb: None)",
                                 "p.cb_asid_changed(name='c1')(lambda cpu, old, new: 0)",
                                 "p.cb_guest_hypercall(name='c2')(lambda cpu: False)",
                                 "p.hook(0x1000)(lambda cpu, tb, h: None)",
                                 "p.record('rec')",
                                 "p.cb_main_loop_wait(name='c5')(lambda: None)",
                                 "p.cb_after_loadvm(name='c6')(lambda cpu: None)",
                                 "p.cb_after_machine_init(name='c7')(lambda cpu: None)",
                                 "p.hypercall(0x1234)"]},
                      kvm_or_error(*[after(i, "NotImplementedError", what, "KVM")
                                     for i, what in enumerate(["before_block_exec", "asid_changed",
                                                               "guest_hypercall", "hook", "record"])],
                                   after(5, "ok"), after(6, "ok"), after(7, "ok"),
                                   after(8, "NotImplementedError", "hypercall", "KVM"))),
    "tcg callbacks": ({"generic": "{img}/x86.json",
                       "after": ["p.cb_before_block_exec(name='c0')(lambda cpu, tb: None)",
                                 "p.cb_guest_hypercall(name='c1')(lambda cpu: False)"]},
                      [after(0, "ok"), after(1, "ok")]),
    "serial": ({"generic": "{img}/x86.json"}, [("its fd is an open socket", lambda r: r.get("serial_fd_socket"))]),
    "monitor default": ({"generic": "{img}/x86.json"},
                        [("monitor args: QMP on a connected socket's fd", lambda r: r.get("monitor") == QMP),
                         attr("monitor", "qmp"), ("its fd is an open socket", lambda r: r.get("monitor_fd_socket"))]),
    "monitor hmp": ({"generic": "{img}/x86.json", "monitor": "hmp"},
                    [("monitor args: HMP", lambda r: r.get("monitor") == HMP), attr("monitor", "hmp")]),
    "monitor none": ({"generic": "{img}/x86.json", "monitor": None},
                     [("no monitor args", lambda r: r.get("monitor") == []),
                      ("argv otherwise unchanged", lambda r: r.get("argv") == LEGACY["generic"][1])]),
    "raw_monitor": ({"generic": "{img}/x86.json", "raw_monitor": True},
                    [("no monitor args", lambda r: r.get("monitor") == []), attr("monitor", None),
                     warned("DeprecationWarning", "monitor=None")]),
    "monitor bogus": ({"monitor": "telnet"}, [error("ValueError", "telnet", "'qmp'")]),
    "libpanda_path missing": ({"arch": "x86_64", "libpanda_path": "{img}/none.so"},
                              [error("FileNotFoundError", "libpanda_path", "none.so")]),
    "plugin_path missing": ({"arch": "x86_64", "plugin_path": "{img}/none"}, [error("ValueError", "plugin_path")]),
    "biospath missing": ({"arch": "x86_64", "biospath": "{img}/none"}, [error("ValueError", "biospath")]),
    "PANDA_LIB missing": ({"arch": "x86_64", "env": {"PANDA_LIB": "{img}/none.so"}},
                          [error("FileNotFoundError", "PANDA_LIB")]),
    "before run": ({"generic": "{img}/x86.json",
                    "after": ["p.vm_stop()", "p.cont()", "p.snap('s')", "p.reset()", "p.revert_async('s')",
                              "p.physical_memory_read(0x1000, 4)", "p.physical_memory_write(0x1000, b'x')",
                              "p.record('rec')",
                              "p.cb_before_block_exec(name='c')(lambda cpu, tb: None)",
                              "p.cb_after_block_exec(name='c')(lambda cpu, tb, code: None)",
                              "p.delete_callback('c')", "p.cb_before_block_exec(name='c')(lambda cpu, tb: None)"]},
                   [after(i, "RuntimeError", what, "run()") for i, what in
                    enumerate(["vm_stop", "cont", "snap", "reset", "revert_async", "reading guest memory",
                               "writing guest memory", "record"])]
                   + [after(8, "ok"), after(9, "ok"), after(10, "ok"), after(11, "ok")]),
    "os= is gone": ({"os": "linux"}, [error("TypeError", "os")]),
    "load_plugin_interface= is gone": ({"load_plugin_interface": True}, [error("TypeError", "load_plugin_interface")]),
    "environ": ({"generic": "{img}/x86.json"}, [("os.environ unchanged", lambda r: r.get("environ") == [])]),
    "quiet": ({"generic": "{img}/x86.json", "extra_args": ["-icount", "shift=6"]},
              [("prints nothing", lambda r: "error" not in r and r.get("printed") == ""),
               ("starts no thread", lambda r: r.get("threads") == 0),
               ("logs the argv (info)", lambda r: any(l[0] == "INFO" and "-icount shift=6" in l[1] for l in r.get("logs", []))),
               ("logs no warning", lambda r: not any(l[0] == "WARNING" for l in r.get("logs", [])))]),
    "quiet aarch64": ({"arch": "aarch64", "os_version": "linux-64-generic", "rr": True},
                      [("prints nothing", lambda r: "error" not in r and r.get("printed") == ""),
                       ("starts no thread", lambda r: r.get("threads") == 0)]),
    "missing disk": ({"generic": "{img}/x86.json", "qcow": "{img}/none.qcow2"},
                     [("logs a warning", lambda r: any(l[0] == "WARNING" and "none.qcow2" in l[1] for l in r.get("logs", []))),
                      ("prints nothing", lambda r: r.get("printed") == "")]),
    "rr kvm": ({"generic": "{img}/x86.json", "rr": True, "accel": "kvm"}, [error("ValueError", "rr=True", "TCG")]),
    "rr legacy kvm": ({"generic": "{img}/x86.json", "rr": True, "extra_args": ["-accel", "kvm"]},
                      [error("ValueError", "rr=True")]),
    "rr -accel": ({"generic": "{img}/x86.json", "rr": True, "extra_args": ["-accel", "tcg"]},
                  [error("ValueError", "rr=True", "-accel")]),
})


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
        if a in ("-monitor", "-mon") or (a in ("-chardev", "-object") and "pandare2-monitor" in args[i + 1]):
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
            a = re.sub(r"^socket,id=pandare2-serial,fd=\d+$", "socket,id=pandare2-serial,fd={fd}", a)
            argv.append(a)
        i += 1
    return argv, monitor


def child(spec):
    from pandare2 import Panda, MachineBuilder
    img = spec["img"]
    kwargs = {}
    for k, v in spec["kwargs"].get("env", {}).items():
        os.environ[k] = v.replace("{img}", img)
    for k, v in spec["kwargs"].items():
        if k in ("after", "env"):
            continue
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
    environ = dict(os.environ)
    # What the constructor logs, prints (from Python or C, to fd 1 or 2) and starts.
    logs = []
    handler = logging.Handler()
    handler.emit = lambda rec: logs.append([rec.levelname, rec.getMessage()])
    logging.getLogger("pandare2").addHandler(handler)
    logging.getLogger("pandare2").setLevel(logging.DEBUG)
    threads = threading.active_count()
    out = tempfile.TemporaryFile()
    saved = [os.dup(1), os.dup(2)]
    sys.stdout.flush()
    sys.stderr.flush()
    os.dup2(out.fileno(), 1)
    os.dup2(out.fileno(), 2)
    try:
        with warnings.catch_warnings(record=True) as w:
            warnings.simplefilter("always")
            p = Panda(**kwargs)
        res["threads"] = threading.active_count() - threads
        res["environ"] = sorted(k for k in set(environ) | set(os.environ) if environ.get(k) != os.environ.get(k))
        res["libpanda_path"] = p.libpanda_path
        res["warnings"] = [[x.category.__name__, str(x.message)] for x in w]
    except Exception as e:  # noqa: BLE001 - the case says which it wants
        res["error"] = [type(e).__name__, str(e)]
    finally:
        sys.stdout.flush()
        sys.stderr.flush()
        ctypes.CDLL(None).fflush(None)      # C's stdio buffers
        os.dup2(saved[0], 1)
        os.dup2(saved[1], 2)
        out.seek(0)
        res["printed"] = out.read().decode(errors="replace")
        res["logs"] = logs
    if "error" not in res:
        res["after"] = []
        for call in spec["kwargs"].get("after", []):
            try:
                eval(call, {"p": p})    # noqa: S307 - the cases' own calls
                res["after"].append(["ok", ""])
            except Exception as e:  # noqa: BLE001
                res["after"].append([type(e).__name__, str(e)])
        res["argv"], res["monitor"] = normalize(p, img)
        res["monitor"] = [re.sub(r"fd=\d+", "fd={fd}", a) for a in res["monitor"]]
        fd = getattr(p, "_monitor_fd", None)
        res["monitor_fd_socket"] = fd is not None and stat.S_ISSOCK(os.fstat(fd).st_mode)
        fd = getattr(p, "_serial_fd", None)
        res["serial_fd_socket"] = fd is not None and stat.S_ISSOCK(os.fstat(fd).st_mode)
        res["attrs"] = {"arch": p.arch_name, "mem": p.mem, "os": p.os, "prompt": p.expect_prompt,
                        "qcow": p.qcow.replace(img, "{img}") if p.qcow else None,
                        "accel": p.accel, "monitor": p.monitor,
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
    for f in ["x86.txt"] + [i["qcow"] for i in IMAGES.values()]:
        open(os.path.join(img, f), "w").close()
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
        check(f"legacy {name}: QMP monitor", res["monitor"] == QMP, res["monitor"])

    # $PANDA_LIB naming the directory of the engine a default Panda finds.
    res, _ = run_case(img, {"arch": "x86_64"})
    lib = (res or {}).get("libpanda_path") or "/nonexistent/libpanda-x86_64-softmmu.so"
    CASES["PANDA_LIB dir"] = ({"arch": "x86_64", "env": {"PANDA_LIB": os.path.dirname(lib)}},
                              [("finds the engine there", lambda r: r.get("libpanda_path") == lib)])

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
