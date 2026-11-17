"""Test machines with a snapshot at a guest agent, which moves programs and results through
hypercalls and guest memory instead of the serial console.

Run: panda-python tests/testmachine.py <busybox-<arch>.json> [--smp N] [--intel]
(builds the machine's snapshot the first time, prints its path, then times a revert and a round
trip of a 9 MiB file through the agent; TESTMACHINE PASS at the end).

A machine is a tests/mkbusybox.py image's kernel and machine arguments with an initramfs of its
own: the image's busybox and the agent (tests/programs/agent.c) as /init. The agent mounts /dev,
/proc, /sys, tracefs and /tmp, installs busybox's applets and polls the host; the snapshot "agent"
is taken at its first poll. Building it is one boot (3-6 s under TCG); it is cached under the
image's directory (machines/<arch>-smp<N>-<key>/, the key a hash of everything it was built
from), or $TESTMACHINE_CACHE. A test copies the snapshot and reverts to it (0.3-0.6 s), so it
starts in about a second, and tests can run in parallel.

    from testmachine import Agent, Machine
    m = Machine(sys.argv[1], smp=2)         # builds the snapshot the first time
    panda = m.panda()
    agent = Agent(panda)

    @panda.queue_blocking
    def drive():
        m.revert(panda, agent)
        agent.put("/tmp/prog", data)
        status, out = agent.run("/tmp/prog > /tmp/log")
        log = agent.get("/tmp/log")

The guest console goes to console.log in the machine's private directory (Machine.tmp).

Machines exist for x86_64, i386, aarch64, arm (busybox-armel), riscv64, mipsel, mips, mips64el,
mips64, ppc and ppc64 (big- and little-endian). An arch needs (tests/README.md, "Test machines"):
the agent built for it (AGENT_ARCH, from tests/programs/sys.h's system layer and build.sh), and a
mkbusybox image. The hypercall is penguin's per-arch instruction, which the
engine routes to PANDA's guest_hypercall.
"""
import argparse
import atexit
import gzip
import hashlib
import importlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time

PANDA_TEST = dict(area="testmachine", tier="guest", fixture="testmachine",
                  arches=["x86_64", "aarch64", "arm", "i386", "riscv64", "mipsel", "mips", "mips64el",
                          "mips64", "ppc", "ppc64"],
                  variants=dict(smp=[1, 4], intel=[False, True]), timeout=600)

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "programs"))
sys.path.insert(0, HERE)
from mkbusybox import cpio_newc, empty_qcow2  # noqa: E402

MAGIC_AGENT = 0x50416774
HC_HELLO, HC_POLL, HC_REPLY = 1, 2, 3
OP_PUT, OP_RUN, OP_GET = 1, 2, 3
REQ = "IIIIQQ"                          # op, mode, flags, pathlen, datalen, offset (guest order)
REQ_SIZE = struct.calcsize("<" + REQ)
SNAPSHOT = "agent"
# pandare2 arch: the agent's key in agent_programs.PROGRAMS
AGENT_ARCH = {"x86_64": "x86_64", "aarch64": "aarch64", "arm": "arm", "i386": "i386", "riscv64": "riscv64",
              "mipsel": "mipsel", "mips": "mips", "mips64el": "mips64el", "mips64": "mips64",
              "ppc": "ppc", "ppc64": "ppc64"}


def agent_key(image):
    """The agent build for an image: its pandare2 arch's (ppc64 also runs little-endian kernels)."""
    arch = image["arch"]
    kernel = arg_after(list(image["extra_args"]), "-kernel") or ""
    if arch == "ppc64" and "64le" in os.path.basename(kernel):
        return "ppc64le"
    return AGENT_ARCH[arch]


def agent_binary(image):
    import base64
    return base64.b64decode(importlib.import_module("agent_programs").PROGRAMS[agent_key(image)])


def cpio_members(data):
    """{name: (mode, bytes)} of a (gzipped) newc cpio archive."""
    if data[:2] == b"\x1f\x8b":
        data = gzip.decompress(data)
    out, i = {}, 0
    while data[i:i + 6] == b"070701":
        f = [int(data[i + 6 + 8 * k:i + 14 + 8 * k], 16) for k in range(13)]
        mode, size, namesize = f[1], f[6], f[11]
        name = data[i + 110:i + 110 + namesize - 1].decode()
        i = (i + 110 + namesize + 3) & ~3
        if name == "TRAILER!!!":
            break
        out[name] = (mode, data[i:i + size])
        i = (i + size + 3) & ~3
    return out


def arg_after(args, flag):
    return args[args.index(flag) + 1] if flag in args else None


def machine_dir(image, initrd_src, initrd, smp, intel, mem, append=""):
    """The machine's cache directory: <cache>/<arch>-smp<N>[-intel]-<key>, the key a hash of
    everything the machine is built from (the image's arguments, the initramfs, the agent)."""
    cache = os.environ.get("TESTMACHINE_CACHE") or \
        os.path.join(os.path.dirname(os.path.realpath(initrd_src)), "machines")
    key = hashlib.sha256(json.dumps([image["arch"], list(image["extra_args"]), smp, intel, mem,
                                     image.get("disk_device")] + ([append] if append else [])
                                    ).encode() + initrd +
                         agent_binary(image)).hexdigest()[:16]
    return os.path.join(cache, f"{image['arch']}-smp{smp}{'-intel' if intel else ''}-{key}")


class Machine:
    """A busybox image's machine with the agent as /init, and a cached snapshot at its start."""

    def __init__(self, image, smp=1, intel=False, mem=None, append=""):
        """append: more kernel command line (e.g. nokaslr, for OSI from a kernel's ISF)."""
        with open(image) as f:
            self.image = json.load(f)
        self.arch = self.image["arch"]
        if self.arch not in AGENT_ARCH:
            raise NotImplementedError(f"testmachine: no agent for {self.arch} (tests/README.md, "
                                      "\"Test machines\")")
        args = list(self.image["extra_args"])
        self.initrd_src = arg_after(args, "-initrd")
        if not self.initrd_src or not arg_after(args, "-kernel"):
            raise ValueError(f"{image}: not a tests/mkbusybox.py image (no -kernel and -initrd)")
        self.smp, self.intel, self.append = smp, intel, append
        self.mem = mem or self.image["default_mem"]
        with open(self.initrd_src, "rb") as f:
            initrd = f.read()
        self.dir = machine_dir(self.image, self.initrd_src, initrd, smp, intel, self.mem, append)
        self.snapshot = os.path.join(self.dir, "snapshot.qcow2")
        self.tmp = None
        atexit.register(self.cleanup)
        if not os.path.isfile(self.snapshot):
            self.build(initrd)

    def args(self, initrd, console):
        args = list(self.image["extra_args"])
        args[args.index("-initrd") + 1] = initrd
        if self.append:
            args[args.index("-append") + 1] += " " + self.append
        args += ["-smp", str(self.smp), "-serial", f"file:{console}"]
        if self.intel:
            args += ["-cpu", "qemu64,vendor=GenuineIntel"]
        return args

    def generic(self, qcow, console):
        return dict(arch=self.arch, os=self.image["os"], prompt=None, qcow=qcow,
                    default_mem=self.mem, snapshot=SNAPSHOT, disk_device=self.image.get("disk_device"),
                    extra_args=self.args(os.path.join(self.dir, "initrd"), console))

    def build(self, initrd):
        """Boot the machine in a child process (one Panda per process) and snapshot it."""
        os.makedirs(os.path.dirname(self.dir), exist_ok=True)
        tmp = tempfile.mkdtemp(prefix=os.path.basename(self.dir) + ".", dir=os.path.dirname(self.dir))
        bb = cpio_members(initrd).get("bin/busybox")
        if not bb:
            raise ValueError(f"{self.initrd_src}: no bin/busybox")
        dirs = ["bin", "dev", "proc", "sys", "tmp", "root", "etc", "var", "var/tmp"]
        with open(os.path.join(tmp, "initrd"), "wb") as f:
            f.write(cpio_newc([(d, 0o40755, b"") for d in dirs] + [
                ("bin/busybox", 0o100755, bb[1]), ("bin/sh", 0o120777, b"busybox"),
                ("init", 0o100755, agent_binary(self.image))]))
        empty_qcow2(os.path.join(tmp, "snapshot.qcow2"), 16 << 20)
        t0 = time.monotonic()
        print(f"testmachine: booting {os.path.basename(self.dir)} for its snapshot", flush=True)
        spec = json.dumps(dict(image=self.image, initrd_src=self.initrd_src, smp=self.smp,
                               intel=self.intel, mem=self.mem, append=self.append, dir=tmp))
        rc = subprocess.run([sys.executable, os.path.abspath(__file__), "--snap", spec]).returncode
        if rc:
            raise RuntimeError(f"testmachine: no snapshot (rc {rc}; console: {tmp}/console.log)")
        try:
            os.rename(tmp, self.dir)
        except OSError:             # another process built it first
            shutil.rmtree(tmp, ignore_errors=True)
        print(f"testmachine: snapshot in {time.monotonic() - t0:.1f}s: {self.snapshot}", flush=True)

    def panda(self, **kwargs):
        """A Panda for this machine, on a private copy of the snapshot."""
        from pandare2 import Panda
        sweep_tmp()
        self.cleanup()
        self.tmp = tempfile.mkdtemp(prefix=f"testmachine-{os.getpid()}-")
        qcow = os.path.join(self.tmp, "snapshot.qcow2")
        shutil.copy(self.snapshot, qcow)
        extra = kwargs.pop("extra_args", [])
        return Panda(generic=self.generic(qcow, os.path.join(self.tmp, "console.log")),
                     extra_args=extra, **kwargs)

    def revert(self, panda, agent, timeout=60):
        """Revert to the snapshot and wait for the agent. From a queue_blocking function."""
        agent.ready.clear()
        panda.revert_sync(SNAPSHOT)
        if not agent.ready.wait(timeout):
            raise TimeoutError("the agent didn't poll after the revert")

    def cleanup(self):
        """Remove this machine's snapshot copy and console log (kept with $TESTMACHINE_KEEP)."""
        if self.tmp and not os.environ.get("TESTMACHINE_KEEP"):
            shutil.rmtree(self.tmp, ignore_errors=True)
            self.tmp = None

    def __del__(self):
        self.cleanup()


def sweep_tmp():
    """Remove the snapshot copies of test processes that have exited without cleaning up (killed,
    or timed out): each copy's directory is named after its process."""
    if os.environ.get("TESTMACHINE_KEEP"):
        return
    root = tempfile.gettempdir()
    for name in os.listdir(root):
        parts = name.split("-")
        if len(parts) != 3 or parts[0] != "testmachine" or not parts[1].isdigit():
            continue
        path = os.path.join(root, name)
        try:
            os.kill(int(parts[1]), 0)
            continue                    # still running
        except ProcessLookupError:
            pass
        except PermissionError:
            continue
        if os.path.isdir(path) and os.stat(path).st_uid == os.getuid():
            shutil.rmtree(path, ignore_errors=True)


class Agent:
    """The host side of the guest agent: create it before panda.run()."""

    def __init__(self, panda):
        self.panda = panda
        self.ready = threading.Event()
        self.done = threading.Event()
        self.pending = None
        self.result = None
        self.size = 0
        self.hello = None
        self.order = None                   # the guest's byte order, at its first hypercall
        panda.hypercall(MAGIC_AGENT, mutates=True)(self._hypercall)

    def _byte_order(self, cpu):
        arch = self.panda.arch_name
        if arch == "ppc64":
            return ">" if not self.panda.cpu_env(cpu).msr & 1 else "<"    # MSR[LE]
        return ">" if arch in ("mips", "mips64", "ppc") else "<"

    def _hypercall(self, cpu):
        # The "syscall" convention's argument 0 is the number register; the magic is argument 1.
        op, a, b, c = (self.panda.arch.get_arg(cpu, i, convention="syscall") for i in range(2, 6))
        if self.order is None:
            self.order = self._byte_order(cpu)
        ret = 0
        if op == HC_HELLO:
            self.hello = c
        elif op == HC_POLL:
            self.size = b
            self.ready.set()
            if self.pending is not None:
                req, self.pending = self.pending, None
                self.panda.guest.write_virt(cpu, a, req)
                ret = len(req)
        elif op == HC_REPLY:
            data = bytes(self.panda.virtual_memory_read(cpu, a, b)) if b else b""
            bits = self.panda.bits
            c &= (1 << bits) - 1
            status = c - (1 << bits) if c >> (bits - 1) else c
            self.result = (status, data)
            self.done.set()
        self.panda.guest.set_retval(cpu, ret)

    def request(self, op, path, data=b"", mode=0o755, flags=0, offset=0, timeout=600):
        path = path.encode() if isinstance(path, str) else path
        req = struct.pack((self.order or "<") + REQ, op, mode, flags, len(path), len(data),
                          offset) + path + data
        if len(req) > self.size:
            raise ValueError(f"request of {len(req)} bytes; the agent's buffer is {self.size}")
        self.done.clear()
        self.pending = req
        if not self.done.wait(timeout):
            raise TimeoutError(f"the agent didn't answer op {op} ({path[:60]!r})")
        return self.result

    def put(self, path, data, mode=0o755):
        """Write data to the guest file path (created or truncated)."""
        chunk = self.size - REQ_SIZE - 4096
        for i in range(0, max(len(data), 1), chunk):
            status, _ = self.request(OP_PUT, path, data[i:i + chunk], mode, flags=int(i > 0))
            if status:
                raise OSError(-status, f"agent put {path}")

    def run(self, cmd, timeout=600):
        """Run /bin/sh -c cmd; (exit status, or -signal, and its stdout and stderr)."""
        status, out = self.request(OP_RUN, cmd, timeout=timeout)
        if len(out) == self.size:
            out += self.get("/tmp/.agent-out", offset=len(out))
        code = (status >> 8) & 0xff if status & 0x7f == 0 else -(status & 0x7f)
        return code, out

    def get(self, path, offset=0):
        """The guest file path's bytes from offset."""
        out = b""
        while True:
            n, data = self.request(OP_GET, path, offset=offset + len(out))
            if n < 0:
                raise OSError(-n, f"agent get {path}")
            out += data
            if n < self.size:
                return out


def _snap(spec):
    """The child process of Machine.build: boot, wait for the agent's first poll, savevm."""
    from pandare2 import Panda
    s = json.loads(spec)
    m = Machine.__new__(Machine)
    m.image, m.initrd_src, m.arch, m.smp, m.intel, m.mem, m.append, m.dir, m.tmp = (
        s["image"], s["initrd_src"], s["image"]["arch"], s["smp"], s["intel"], s["mem"], s["append"],
        s["dir"], None)
    panda = Panda(generic=m.generic(os.path.join(m.dir, "snapshot.qcow2"),
                                    os.path.join(m.dir, "console.log")))
    agent = Agent(panda)
    failed = []

    @panda.queue_blocking
    def drive():
        if agent.ready.wait(600):
            panda.snap(SNAPSHOT)
        else:
            failed.append(f"the agent never polled (console: {m.dir}/console.log)")
        panda.end_analysis()

    panda.run()
    if failed:
        sys.exit(failed[0])


def main():
    if sys.argv[1:2] == ["--snap"]:
        return _snap(sys.argv[2])
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--smp", type=int, default=1)
    ap.add_argument("--intel", action="store_true")
    opt = ap.parse_args()
    t = time.monotonic()
    m = Machine(opt.image, opt.smp, opt.intel)
    print(m.snapshot)
    panda = m.panda()
    agent = Agent(panda)
    seen = {"machine": time.monotonic() - t}

    @panda.queue_blocking
    def drive():
        t = time.monotonic()
        m.revert(panda, agent)
        seen["revert"] = time.monotonic() - t
        data = os.urandom(3 << 20) * 3
        t = time.monotonic()
        agent.put("/tmp/blob", data)
        seen["put"] = time.monotonic() - t
        seen["run"] = agent.run("nproc; uname -m; md5sum /tmp/blob; ls /sys/kernel/tracing | wc -l")
        t = time.monotonic()
        seen["blob"] = agent.get("/tmp/blob") == data
        seen["get"] = time.monotonic() - t
        panda.end_analysis()

    panda.run()
    print(f"machine {seen['machine']:.1f}s, revert {seen.get('revert', 0):.2f}s, "
          f"put 9 MiB {seen.get('put', 0):.2f}s, get {seen.get('get', 0):.2f}s, "
          f"round trip ok: {seen.get('blob')}")
    print("run:", seen.get("run"))
    ok = seen.get("blob") and seen.get("run", (1,))[0] == 0
    print("TESTMACHINE", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
