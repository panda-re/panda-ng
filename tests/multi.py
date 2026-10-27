"""More than one Panda in one process. No guest: firmware boots, 20k blocks each. Every case runs
in a child process of its own.

Run: panda-python tests/multi.py [case...]     (default: every case)

- thread: run() on a thread other than the main one; the process exits on its own after it (the
  engine's exit notifiers take the BQL, which panda_run no longer keeps).
"""
import os
import subprocess
import sys
import tempfile
import threading

A = "x86_64"
MACHINE = {"aarch64": ["-M", "virt", "-cpu", "max"]}
BLOCKS = 20000


def make(arch):
    """A Panda that ends its run at BLOCKS blocks."""
    from pandare2 import Panda
    p = Panda(arch=arch, mem="128M", os_version="linux-64-generic",
              extra_args=MACHINE.get(arch, []) + ["-display", "none", "-nodefaults"])
    p.blocks = 0

    @p.cb_before_block_exec
    def bbe(cpu, tb):
        p.blocks += 1
        if p.blocks == BLOCKS:
            p.end_analysis()
    return p


def child(case):
    """One case, in this (fresh) process. Prints CHECK lines; exits on its own."""
    def check(name, ok, detail=""):
        print(f"CHECK {'PASS' if ok else 'FAIL'} {case}: {name}" + ("" if ok else f": {detail}"), flush=True)

    if case == "thread":
        a = make(A)
        t = threading.Thread(target=a.run)
        t.start()
        t.join()
        check("run() on another thread", a.blocks >= BLOCKS, a.blocks)
    print(f"MULTI {case} done", flush=True)


def main():
    if sys.argv[1:2] == ["--child"]:
        child(sys.argv[2])
        return 0
    cases = sys.argv[1:] or ["thread"]
    failed = False
    for case in cases:
        env = dict(os.environ)
        try:
            r = subprocess.run([sys.executable, "-u", os.path.abspath(__file__), "--child", case],
                               env=env, capture_output=True, text=True, timeout=300)
            out, rc = r.stdout + r.stderr, r.returncode
        except subprocess.TimeoutExpired as e:
            out, rc = (e.stdout or b"").decode(errors="replace") + (e.stderr or b"").decode(errors="replace"), "timeout"
        checks = [l for l in out.splitlines() if l.startswith("CHECK ")]
        for l in checks:
            print("  " + l[len("CHECK "):])
        ok = rc == 0 and checks and all(l.startswith("CHECK PASS") for l in checks) and f"MULTI {case} done" in out
        if case == "thread":
            print(f"  {'PASS' if rc == 0 else 'FAIL'} thread: the process exits after run() on a thread (rc={rc})")
        if not ok:
            print(f"  FAIL {case}: rc={rc}\n" + "\n".join("    " + l for l in out.splitlines()[-25:]))
        failed |= not ok
    print("MULTI " + ("FAIL" if failed else "PASS"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
