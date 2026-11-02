"""Test machines with a snapshot, and a guest agent that moves programs and results through
hypercalls and guest memory instead of the serial console.

Two kinds, both from a generic image (pandare2.mkimage) and both with the agent
(tests/programs/agent_<arch>.c) running in the snapshot:
- tiny (the default): the image's own kernel (the same release, so its ISF and kernelinfo apply)
  booted from an initramfs that holds the agent as /init and a static busybox; 256 MiB. It boots
  in seconds.
- full=True: the image's own disk and distribution (python3, coreutils, systemd), on a qcow2
  overlay of it. With one vCPU and the image's CPU it starts from the image's snapshot; otherwise
  it boots the disk (about 75 s). The agent is started from the root shell, which stays usable:
  run_serial_cmd works after a revert, as with the image's own snapshot.
Each is built once per (image, kind, vCPUs, CPU vendor, memory): the snapshot "agent" is taken at
the agent's first hypercall and cached under <image dir>/machines/<key>/. A test copies that
snapshot and reverts to it, so it starts in about a second, on any number of vCPUs or CPU vendor.

    from testmachine import Agent, Machine
    m = Machine(sys.argv[1], smp=4, intel=True)     # builds the snapshot the first time
    panda = m.panda()
    agent = Agent(panda)

    @panda.queue_blocking
    def drive():
        m.revert(panda, agent)
        agent.put("/tmp/prog", data)
        status, out = agent.run("/tmp/prog > /tmp/log")
        log = agent.get("/tmp/log")

Build or check a machine by hand:
    panda-python tests/testmachine.py <image.json> [--smp N] [--intel] [--full]
(prints the snapshot's path, then a smoke check of the agent).

The kernel comes from the distribution's archive (the -unsigned image package of the image's
release, checked against the archive's SHA256), busybox from nixpkgs (pkgsStatic), both cached
under <image dir>/machines/. Only Debian x86_64 for now.
"""
import argparse
import base64
import hashlib
import importlib
import json
import lzma
import os
import shlex
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "programs"))
from testlib import boot_from_disk, upload  # noqa: E402

MAGIC_AGENT = 0x50416774
HC_HELLO, HC_POLL, HC_REPLY = 1, 2, 3
OP_PUT, OP_RUN, OP_GET = 1, 2, 3
REQ = struct.Struct("<IIIIQQ")          # op, mode, flags, pathlen, datalen, offset
SNAPSHOT = "agent"
MEM = "256M"
BUSYBOX = {"x86_64": "nixpkgs#pkgsStatic.busybox"}
DEBIAN_ARCHIVES = [("https://deb.debian.org/debian/", "trixie"),
                   ("https://deb.debian.org/debian/", "trixie-updates"),
                   ("https://security.debian.org/debian-security/", "trixie-security")]
DEBIAN_ARCH = {"x86_64": "amd64", "aarch64": "arm64"}


def debian_kernel(arch, release, cache):
    """The kernel image of Debian's linux-image-<release>-unsigned package (cached)."""
    out = os.path.join(cache, f"vmlinuz-{arch}-{release}")
    if os.path.isfile(out):
        return out
    package = f"linux-image-{release}-unsigned"
    for base, suite in DEBIAN_ARCHIVES:
        index = f"{base}dists/{suite}/main/binary-{DEBIAN_ARCH[arch]}/Packages.xz"
        with urllib.request.urlopen(index) as r:
            stanzas = lzma.decompress(r.read()).decode().split("\n\n")
        found = [dict(l.split(": ", 1) for l in st.splitlines() if ": " in l and not l.startswith(" "))
                 for st in stanzas if st.startswith(f"Package: {package}\n")]
        if found:
            break
    else:
        raise RuntimeError(f"{package} is in none of the Debian archives")
    deb = os.path.join(cache, os.path.basename(found[0]["Filename"]))
    if not os.path.isfile(deb):
        print(f"testmachine: downloading {package}", flush=True)
        h = hashlib.sha256()
        with urllib.request.urlopen(base + found[0]["Filename"]) as r, open(deb + ".tmp", "wb") as f:
            for chunk in iter(lambda: r.read(1 << 20), b""):
                h.update(chunk)
                f.write(chunk)
        if h.hexdigest() != found[0]["SHA256"]:
            os.remove(deb + ".tmp")
            raise RuntimeError(f"{deb}: SHA256 differs from the archive's")
        shutil.move(deb + ".tmp", deb)
    with open(deb, "rb") as f:          # an ar archive; the kernel is in data.tar.*
        assert f.read(8) == b"!<arch>\n"
        while True:
            header = f.read(60)
            member, size = header[:16].decode().strip().rstrip("/"), int(header[48:58])
            if member.startswith("data.tar"):
                break
            f.seek(size + size % 2, 1)
        with tarfile.open(fileobj=f, mode="r|*") as tar:
            for entry in tar:
                if entry.name.endswith(f"boot/vmlinuz-{release}"):
                    with tar.extractfile(entry) as src, open(out + ".tmp", "wb") as dst:
                        shutil.copyfileobj(src, dst)
                    break
            else:
                raise RuntimeError(f"no boot/vmlinuz-{release} in {deb}")
    shutil.move(out + ".tmp", out)
    return out


def busybox(arch):
    """A static busybox for arch, from nixpkgs (substituted from the binary cache, never built)."""
    out = subprocess.run(["nix", "build", "--no-link", "--print-out-paths", "--max-jobs", "0",
                          BUSYBOX[arch]], check=True, capture_output=True, text=True).stdout
    return os.path.join(out.strip().splitlines()[-1], "bin", "busybox")


def cpio(entries):
    """A newc cpio archive of [(path, mode, data or None, (major, minor) or None)]."""
    out = bytearray()

    def add(name, mode, data=b"", rdev=(0, 0)):
        name = name.encode() + b"\0"
        fields = [len(out) // 4 + 1, mode, 0, 0, 1, 0, len(data), 0, 0, rdev[0], rdev[1], len(name), 0]
        out.extend(b"070701" + b"".join(b"%08x" % v for v in fields) + name)
        out.extend(b"\0" * (-len(out) % 4))
        out.extend(data)
        out.extend(b"\0" * (-len(out) % 4))

    for path, mode, data, rdev in entries:
        add(path, mode, data or b"", rdev or (0, 0))
    add("TRAILER!!!", 0)
    return bytes(out)


def initramfs(arch, bb):
    agent = base64.b64decode(importlib.import_module("agent_programs").PROGRAMS[arch])
    with open(bb, "rb") as f:
        bbdata = f.read()
    d = 0o040755
    return cpio([("bin", d, None, None), ("dev", d, None, None), ("proc", d, None, None),
                 ("sys", d, None, None), ("tmp", d | 0o1000, None, None), ("etc", d, None, None),
                 ("root", d, None, None),
                 ("dev/console", 0o020600, None, (5, 1)),
                 ("init", 0o100755, agent, None),
                 ("bin/busybox", 0o100755, bbdata, None)])


class Machine:
    """A small machine on a generic image's kernel, with a cached snapshot at the agent's start."""

    def __init__(self, image, smp=1, intel=False, mem=None, full=False):
        with open(image) as f:
            self.image = json.load(f)
        self.image_dir = os.path.dirname(os.path.abspath(image))
        self.arch = self.image["arch"]
        if self.arch not in BUSYBOX:
            raise NotImplementedError(f"testmachine: no {self.arch} machine yet")
        self.release = self.image["os"].split(":", 1)[1]
        self.smp, self.intel, self.full = smp, intel, full
        self.mem = mem or (self.image["default_mem"] if full else MEM)
        self.cache = os.path.join(self.image_dir, "machines")
        agent = importlib.import_module("agent_programs").PROGRAMS[self.arch]
        key = hashlib.sha256(json.dumps([self.arch, self.release, smp, intel, self.mem, full, agent,
                                         BUSYBOX[self.arch], self.image]).encode()).hexdigest()[:16]
        name = f"{self.arch}-{'full' if full else 'tiny'}-smp{smp}{'-intel' if intel else ''}-{key}"
        self.dir = os.path.join(self.cache, name)
        self.snapshot = os.path.join(self.dir, "snapshot.qcow2")
        self.tmp = None
        if not os.path.isfile(self.snapshot):
            self.build()

    def args(self):
        cpu = ["-cpu", "qemu64,vendor=GenuineIntel"] if self.intel else []
        if self.full:
            image = self.image.get("extra_args") or []
            return (shlex.split(image) if isinstance(image, str) else list(image)) + \
                ["-smp", str(self.smp)] + cpu
        return ["-display", "none", "-nic", "none", "-no-reboot", "-smp", str(self.smp)] + cpu + \
            ["-kernel", os.path.join(self.dir, "vmlinuz"), "-initrd", os.path.join(self.dir, "initrd"),
             "-append", "console=ttyS0 quiet panic=-1"]

    def generic(self, qcow):
        g = dict(arch=self.arch, os=self.image["os"], prompt=self.image["prompt"] if self.full else r"\$ ",
                 qcow=qcow, default_mem=self.mem, extra_args=self.args(), snapshot=SNAPSHOT)
        for k in ("isf", "kconf_file"):
            if self.image.get(k):
                g[k] = os.path.join(self.image_dir, self.image[k])
        return g

    def build(self):
        """Boot the machine in a child process (one Panda per process) and snapshot it."""
        os.makedirs(self.cache, exist_ok=True)
        tmp = self.dir + ".tmp"
        shutil.rmtree(tmp, ignore_errors=True)
        os.makedirs(tmp)
        if not self.full:
            shutil.copy(debian_kernel(self.arch, self.release, self.cache), os.path.join(tmp, "vmlinuz"))
            with open(os.path.join(tmp, "initrd"), "wb") as f:
                f.write(initramfs(self.arch, busybox(self.arch)))
        t0 = time.monotonic()
        print(f"testmachine: booting {os.path.basename(self.dir)} for its snapshot", flush=True)
        subprocess.run([sys.executable, os.path.abspath(__file__), "--snap", tmp,
                        json.dumps([self.image_dir, self.image, self.smp, self.intel, self.mem,
                                    self.full])], check=True)
        os.replace(tmp, self.dir)
        print(f"testmachine: snapshot in {time.monotonic() - t0:.0f}s: {self.snapshot}", flush=True)

    def panda(self, **kwargs):
        """A Panda for this machine, on a private copy of the snapshot (tests can run in parallel)."""
        from pandare2 import Panda
        self.tmp = tempfile.mkdtemp(prefix="testmachine-")
        qcow = os.path.join(self.tmp, "snapshot.qcow2")
        shutil.copy(self.snapshot, qcow)
        extra = kwargs.pop("extra_args", [])
        return Panda(generic=self.generic(qcow), extra_args=extra, **kwargs)

    def revert(self, panda, agent, timeout=60):
        """Revert to the snapshot and wait for the agent. From a queue_blocking function."""
        agent.ready.clear()
        panda.revert_sync(SNAPSHOT)
        if not agent.ready.wait(timeout):
            raise TimeoutError("the agent didn't poll after the revert")

    def __del__(self):
        if self.tmp:
            shutil.rmtree(self.tmp, ignore_errors=True)


class Agent:
    """The host side of the guest agent: register it before panda.run()."""

    def __init__(self, panda):
        self.panda = panda
        self.ready = threading.Event()
        self.done = threading.Event()
        self.pending = None
        self.result = None
        self.size = 0
        self.hello = None
        panda.hypercall(MAGIC_AGENT, mutates=True)(self._hypercall)

    def _hypercall(self, cpu):
        # The "syscall" convention's argument 0 is the number register; the magic is argument 1.
        op, a, b, c = (self.panda.arch.get_arg(cpu, i, convention="syscall") for i in range(2, 6))
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
            status = c - (1 << 64) if c >= 1 << 63 else c
            self.result = (status, data)
            self.done.set()
        self.panda.guest.set_retval(cpu, ret)

    def request(self, op, path, data=b"", mode=0o755, flags=0, offset=0, timeout=600):
        path = path.encode() if isinstance(path, str) else path
        req = REQ.pack(op, mode, flags, len(path), len(data), offset) + path + data
        if len(req) > self.size:
            raise ValueError(f"request of {len(req)} bytes; the agent's buffer is {self.size}")
        self.done.clear()
        self.pending = req
        if not self.done.wait(timeout):
            raise TimeoutError(f"the agent didn't answer op {op} ({path[:60]!r})")
        return self.result

    def put(self, path, data, mode=0o755):
        """Write data to the guest file path (created or truncated)."""
        chunk = self.size - REQ.size - 4096
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


def _snap(tmp, spec):
    """The child process of Machine.build: boot, wait for the agent, snapshot."""
    from pandare2 import Panda
    image_dir, image, smp, intel, mem, full = json.loads(spec)
    m = Machine.__new__(Machine)
    m.image, m.image_dir, m.arch, m.smp, m.intel, m.mem, m.full, m.dir, m.tmp = (
        image, image_dir, image["arch"], smp, intel, mem, full, tmp, None)
    qcow = os.path.join(tmp, "snapshot.qcow2")
    panda = Panda(generic=m.generic(qcow))
    qemu_img = os.path.join(os.path.dirname(os.path.dirname(panda.libpanda_path)), "bin", "qemu-img")
    qemu_img = qemu_img if os.path.isfile(qemu_img) else "qemu-img"
    if full:        # an overlay on the image's disk
        subprocess.run([qemu_img, "create", "-q", "-f", "qcow2", "-F", "qcow2", "-b",
                        os.path.join(image_dir, image["qcow"]), qcow], check=True)
    else:           # only for the snapshot: the guest has no disk
        subprocess.run([qemu_img, "create", "-q", "-f", "qcow2", qcow, "16M"], check=True)
    agent = Agent(panda)
    failed = []

    @panda.queue_blocking
    def drive():
        from pandare2.panda_expect import TimeoutExpired
        if full:
            # A cold boot: the image's snapshots are in the backing file, out of loadvm's reach.
            boot_from_disk(panda, image["prompt"])
            upload(panda, "agent", image["arch"], "/root/panda-agent")
            panda.run_serial_cmd("setsid /root/panda-agent < /dev/null > /dev/null 2>&1 &")
            if agent.ready.wait(600):
                panda.snap(SNAPSHOT)
            else:
                failed.append("the agent never polled")
            panda.end_analysis()
            return
        # Drain the console into boot.log while waiting: nothing else reads it.
        console = panda.serial_console
        console.set_logging(os.path.join(tmp, "boot.log"))
        console.update_expectation("(?!x)x")
        deadline = time.monotonic() + 1800
        while not agent.ready.is_set() and time.monotonic() < deadline:
            try:
                console.expect(timeout=2)
            except TimeoutExpired:
                pass
        if agent.ready.is_set():
            panda.snap(SNAPSHOT)
        else:
            failed.append(f"the agent never polled (console: {tmp}/boot.log)")
        panda.end_analysis()

    panda.run()
    if failed:
        sys.exit(failed[0])


def main():
    if sys.argv[1:2] == ["--snap"]:
        return _snap(sys.argv[2], sys.argv[3])
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--smp", type=int, default=1)
    ap.add_argument("--intel", action="store_true")
    ap.add_argument("--full", action="store_true")
    opt = ap.parse_args()
    m = Machine(opt.image, opt.smp, opt.intel, full=opt.full)
    print(m.snapshot)
    panda = m.panda()
    agent = Agent(panda)
    seen = {}

    @panda.queue_blocking
    def drive():
        t = time.monotonic()
        m.revert(panda, agent)
        seen["revert"] = time.monotonic() - t
        data = os.urandom(3 << 20) * 3
        t = time.monotonic()
        agent.put("/tmp/blob", data)
        seen["put"] = time.monotonic() - t
        seen["run"] = agent.run("nproc; grep -m1 vendor_id /proc/cpuinfo; uname -r; md5sum /tmp/blob")
        t = time.monotonic()
        seen["blob"] = agent.get("/tmp/blob") == data
        seen["get"] = time.monotonic() - t
        panda.end_analysis()

    panda.run()
    print(f"revert {seen.get('revert', 0):.2f}s, put 9 MiB {seen.get('put', 0):.2f}s, "
          f"get {seen.get('get', 0):.2f}s, round trip ok: {seen.get('blob')}")
    print("run:", seen.get("run"))
    ok = seen.get("blob") and seen.get("run", (1,))[0] == 0
    print("TESTMACHINE", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
