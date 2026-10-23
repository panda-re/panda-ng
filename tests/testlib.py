"""Shared pieces of the real-guest tests: guest programs, checks with known gaps, progress lines.

A test makes checks with `Checks.check(name, ok, detail)` and ends with `sys.exit(checks.report())`.
A check listed as a gap (by name, or by (name, arch)) prints GAP while it fails and XPASS (a
failure of the run) once it passes, so the gap lists stay true.
"""
import base64
import importlib
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "programs"))

MAGIC = 0x50616e64
PROGRAM_BASE, PROGRAM_END = 0x400000, 0x402000   # the programs' text is < 4 KiB; other processes run code at 0x41f000 (debian-13 aarch64)
T0 = time.time()


def progress(msg):
    print(f"progress: [{time.time() - T0:6.1f}s] {msg}", flush=True)


def in_program(pc):
    return PROGRAM_BASE <= pc < PROGRAM_END


def upload(panda, name, arch, path=None):
    """Write tests/programs/<name>_<arch>.S's binary to the guest (as <path>, default /tmp/<name>)
    through the serial console, executable. Call from a queue_blocking function."""
    path = path or f"/tmp/{name}"
    data = base64.b64decode(importlib.import_module(f"{name}_programs").PROGRAMS[arch])
    panda.run_serial_cmd(f"rm -f {path}")
    for i in range(0, len(data), 64):
        chunk = "".join(f"\\x{b:02x}" for b in data[i:i + 64])
        panda.run_serial_cmd(f"printf '{chunk}' >> {path}")
    panda.run_serial_cmd(f"chmod +x {path}")
    return path


def hypercall_args(panda, cpu):
    """(op, a, b, pc) of a test program's hypercall (MAGIC, op, a, b, pc)."""
    return [panda.arch.get_arg(cpu, i, convention="syscall") for i in range(2, 6)]


def read_quads(panda, cpu, addr, n):
    raw = bytes(panda.virtual_memory_read(cpu, addr, 8 * n))
    return [int.from_bytes(raw[8 * i:8 * i + 8], "little") for i in range(n)]


class Checks:
    def __init__(self, arch, gaps=None):
        self.arch = arch
        self.gaps = dict(gaps or {})
        self.results = {}

    def check(self, name, ok, detail=""):
        self.results[name] = (bool(ok), detail)

    def report(self, title=""):
        if title:
            print(title)
        failed = False
        for name, (ok, detail) in self.results.items():
            gap = self.gaps.get(name, self.gaps.get((name, self.arch)))
            if gap:
                status = "XPASS" if ok else "GAP"
                detail = "gap closed: take it out of the gaps" if ok else gap
                failed |= ok
            else:
                status = "PASS" if ok else "FAIL"
                failed |= not ok
            print(f"  {status} {name}" + ("" if status == "PASS" else f": {detail}"), flush=True)
        print(f"RESULT {'FAIL' if failed else 'PASS'} ({len(self.results)} checks, {time.time() - T0:.0f}s)",
              flush=True)
        return 1 if failed else 0
