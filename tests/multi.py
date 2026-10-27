"""More than one Panda in one process. No guest: firmware boots, 20k blocks each. Every case runs
in a child process of its own.

Run: panda-python tests/multi.py [case...]     (default: every case)

- thread: run() on a thread other than the main one; the process exits on its own after it (the
  engine's exit notifiers take the BQL, which panda_run no longer keeps);
- seq: x86_64 then aarch64, one after the other in one process (the first from a thread), each
  with C plugins that call each other (proc_start_linux -> syscalls2 -> hooks, hw_proc_id;
  hypersyscalls -> hypercaller). LD_DEBUG=bindings shows that every symbol the plugin interface
  and plugins of an arch take from an engine comes from that arch's engine, never the other's.
"""
import os
import re
import subprocess
import sys
import tempfile
import threading

A, B = "x86_64", "aarch64"
MACHINE = {"aarch64": ["-M", "virt", "-cpu", "max"]}
PLUGINS = ["proc_start_linux", "hypersyscalls"]
# Loaded through those, by the plugins themselves (panda_require), and called through dlsym.
REQUIRED = ["syscalls2", "hooks", "hw_proc_id", "hypercaller"]
BLOCKS = 20000


def make(arch, plugins=()):
    """A Panda that ends its run at BLOCKS blocks."""
    from pandare2 import Panda
    # syscalls2 needs an OS profile, even with no OS
    p = Panda(arch=arch, mem="128M", os_version="linux-64-generic",
              extra_args=MACHINE.get(arch, []) + ["-display", "none", "-nodefaults"])
    for pl in plugins:
        p.load_plugin(pl)
    p.blocks = 0
    p.loaded = []

    @p.cb_before_block_exec
    def bbe(cpu, tb):
        p.blocks += 1
        if p.blocks >= BLOCKS and not p.loaded:
            p.loaded = [n for n in list(plugins) + (REQUIRED if plugins else [])
                        if p.libpanda.panda_get_plugin_by_name(n.encode()) != p.ffi.NULL] or ["-"]
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
    elif case == "seq":
        a = make(A, PLUGINS)
        t = threading.Thread(target=a.run)
        t.start()
        t.join()
        b = make(B, PLUGINS)
        b.run()
        for p in (a, b):
            check(f"{p.arch_name} runs with its plugins loaded", p.blocks >= BLOCKS and
                  sorted(p.loaded) == sorted(PLUGINS + REQUIRED), (p.blocks, p.loaded))
    print(f"MULTI {case} done", flush=True)


def cross_bindings(prefix):
    """LD_DEBUG=bindings lines where an object of one arch binds to another arch's engine."""
    obj = re.compile(r"libpanda(?:_plugin_interface-|-[a-z0-9_]+_)(" + A + "|" + B + r")-softmmu\.so")
    eng = re.compile(r"libpanda-(" + A + "|" + B + r")-softmmu\.so")
    bad, good = [], 0
    for f in os.listdir(os.path.dirname(prefix)):
        if not f.startswith(os.path.basename(prefix)):
            continue
        for line in open(os.path.join(os.path.dirname(prefix), f), errors="replace"):
            m = re.search(r"binding file (\S+) \[\d+\] to (\S+) \[\d+\]", line)
            if not m:
                continue
            frm, to = obj.search(os.path.basename(m.group(1))), eng.fullmatch(os.path.basename(m.group(2)))
            if frm and to:
                if frm.group(1) != to.group(1):
                    bad.append(line.strip())
                else:
                    good += 1
    return bad, good


def main():
    if sys.argv[1:2] == ["--child"]:
        child(sys.argv[2])
        return 0
    cases = sys.argv[1:] or ["thread", "seq"]
    failed = False
    for case in cases:
        env = dict(os.environ)
        tmp = tempfile.mkdtemp(prefix="multi-")
        if case == "seq":
            env["LD_DEBUG"] = "bindings"
            env["LD_DEBUG_OUTPUT"] = os.path.join(tmp, "ld")
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
        if case == "seq" and rc == 0:
            bad, good = cross_bindings(env["LD_DEBUG_OUTPUT"])
            okb = not bad and good > 0
            print(f"  {'PASS' if okb else 'FAIL'} seq: every binding of an arch's interface and plugins "
                  f"to an engine is to its own ({good} bindings, {len(bad)} cross)" + "".join(f"\n    {b}" for b in bad[:5]))
            ok = ok and okb
        if not ok:
            print(f"  FAIL {case}: rc={rc}\n" + "\n".join("    " + l for l in out.splitlines()[-25:]))
        failed |= not ok
    print("MULTI " + ("FAIL" if failed else "PASS"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
