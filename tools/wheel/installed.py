#!/usr/bin/env python3
"""Installed-artifact acceptance, run with the installed Python outside the checkout.

Usage: python installed.py wheel|deb /path/to/tests /path/to/evidence
Optional PANDA_INSTALLED_IMAGES is an os.pathsep-separated list of guest JSONs.
Selected guest coverage requires PR2's tests/run_results.py and inventory.py to land
first; without that runner, the planned guest check fails rather than being skipped.
"""
import hashlib
import json
import os
import signal
from pathlib import Path
import subprocess
import sys
import time


def within(path, root):
    return os.path.commonpath((str(path), str(root))) == str(root)


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def probe(kind, arch):
    import pandare2
    from pandare2 import Panda

    root = Path(sys.prefix).resolve()
    package = Path(pandare2.__file__).resolve()
    if kind == "wheel":
        assert within(package, root), (package, root)
    args = ["-M", "virt", "-cpu", "max"] if arch == "aarch64" else []
    p = Panda(arch=arch, mem="128M", os_version="linux-64-generic",
              extra_args=args + ["-display", "none", "-nodefaults"])
    p.load_plugin("cskeleton")
    blocks = 0
    mapped = []

    @p.cb_before_block_exec
    def block(cpu, tb):
        nonlocal blocks, mapped
        blocks += 1
        if blocks == 20000:
            mapped = sorted({line.split(maxsplit=5)[-1].strip()
                             for line in Path("/proc/self/maps").read_text().splitlines()
                             if "/" in line and "libpanda" in line})
            p.end_analysis()

    p.run()
    assert blocks >= 20000
    engines = [x for x in mapped if f"libpanda-{arch}-softmmu" in x]
    plugins = [x for x in mapped if "libpanda-cskeleton_" in x]
    assert engines and plugins, mapped
    for path in mapped:
        assert not path.endswith(" (deleted)"), path
        resolved = Path(path).resolve()
        if kind == "wheel":
            assert within(resolved, root), (resolved, root)
        else:
            assert str(resolved).startswith("/nix/store/"), resolved
    print(json.dumps({"schema_version": 1, "kind": kind, "arch": arch,
                      "python": sys.executable, "prefix": str(root),
                      "package": str(package), "blocks": blocks,
                      "engine_selected": str(p.libpanda_path),
                      "plugin_selected": str(p.plugin_path),
                      "mapped": [{"path": x, "sha256": digest(x)} for x in mapped]},
                     sort_keys=True))


def kill_group(process, sig):
    try:
        os.killpg(process.pid, sig)
    except ProcessLookupError:
        pass


def run_check(argv, log_path, env, timeout, grace=30):
    start = time.monotonic()
    process = None
    result = {"status": "ERROR", "returncode": None}
    try:
        with log_path.open("w") as log:
            process = subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT,
                                       env=env, start_new_session=True)
            try:
                rc = process.wait(timeout=timeout)
                result.update(status="PASS" if rc == 0 else "FAIL", returncode=rc)
            except subprocess.TimeoutExpired:
                result.update(status="TIMEOUT", reason="watchdog deadline exceeded")
                kill_group(process, signal.SIGTERM)
                try:
                    process.wait(timeout=grace)
                except subprocess.TimeoutExpired:
                    pass
    except OSError as e:
        result.update(status="ERROR", reason="{}: {}".format(type(e).__name__, e))
    finally:
        # Even if the leader exited on TERM, descendants may still ignore it.
        if process is not None:
            kill_group(process, signal.SIGKILL)
            process.wait()
            result["returncode"] = process.returncode
        result["seconds"] = time.monotonic() - start
    return result


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "--probe":
        probe(sys.argv[2], sys.argv[3])
        return 0
    kind, tests_arg, out_arg = sys.argv[1:]
    if kind not in ("wheel", "deb"):
        raise ValueError("artifact kind must be wheel or deb")
    tests, out = Path(tests_arg).resolve(), Path(out_arg).resolve()
    out.mkdir(parents=True, exist_ok=False)
    for name in ("PANDA_LIB", "PANDA_PLUGIN_DIR", "PANDA_QEMU_LIBDIR", "PYTHONPATH"):
        if os.environ.get(name):
            raise ValueError(f"clear development override {name}")
    if within(Path.cwd(), tests.parent):
        raise ValueError("run installed acceptance outside the source checkout")
    python = sys.executable
    commands = [("provenance-" + a, [python, str(Path(__file__).resolve()), "--probe", kind, a])
                for a in ("x86_64", "aarch64")]
    commands += [("construct", [python, str(tests / "construct.py")]),
                 ("multi", [python, str(tests / "multi.py")]),
                 ("machine-builder", [python, str(tests / "machine_builder.py")]),
                 ("machine", [python, str(tests / "machine.py"), "aarch64", "riscv64"])]
    images = [x for x in os.environ.get("PANDA_INSTALLED_IMAGES", "").split(os.pathsep) if x]
    if images:
        # Stock deb hosts need not have a system python3. Use this artifact's
        # interpreter for the runner as well as for each selected guest test.
        commands.append(("guests", [python, str(tests / "run_results.py"), *images]))
    records = [{"name": name, "argv": argv, "status": "PLANNED", "returncode": None}
               for name, argv in commands]

    def publish():
        temporary = out / "results.json.tmp"
        temporary.write_text(json.dumps({"schema_version": 1, "kind": kind,
            "python": python, "guest_coverage": "selected" if images else "UNTESTED: no fixtures",
            "results": records}, indent=2) + "\n")
        temporary.replace(out / "results.json")

    publish()
    for record in records:
        name = record["name"]
        env = dict(os.environ, PANDA_PYTHON=python, TESTS="guest api callbacks syscalls",
                   OUT=str(out / "guests"), PYTHONNOUSERSITE="1")
        record["status"] = "RUNNING"
        publish()
        try:
            record.update(run_check(record["argv"], out / (name + ".log"), env,
                                    7200 if name == "guests" else 900))
        finally:
            publish()
        print(f"{record['status']} installed {kind} {name}", flush=True)
    return int(any(r["status"] != "PASS" for r in records))


if __name__ == "__main__":
    sys.exit(main())
