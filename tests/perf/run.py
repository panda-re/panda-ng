#!/usr/bin/env python3
"""Minimal single-lane shared-engine protocol; no builds or fixture preparation.

Offline: python3 tests/perf/run.py --check CONFIG.json
Reserved quiet window only: ... --execute CONFIG.json NEW_OUT
Analyze: python3 tests/perf/analyze.py NEW_OUT > report.json

CONFIG schema 1 (all paths absolute; argv is identical for every variant):
  host_target, guest_target, quiet_window: nonempty descriptive strings
  repeats: 3..100, timeout: seconds per boot (0 < timeout <= 600), cpus: [int,...]
  argv: QEMU arguments including -accel tcg, -kernel PATH, -initrd PATH; no icount
  metrics: ["sort_ms",...], higher_better: ["copy_mbs",...]
  exits: ["iw-cpu",...], correctness: [exact guest correctness output lines,...]
  reference: "vanilla", aa: ["shared", "shared-aa"]
  launcher_source: PATH (the exact launcher/panda-qemu.c source)
  inputs: [PATH,...] (kernel, initrd, firmware, workload/source/shim/build/toolchain
    records; directories must be represented by a pinned file manifest)
  variants: {name: {kind: "shared"|"vanilla", executable: PATH,
    engine: PATH (shared only), source_revision: STR, patch_tree: STR,
    build_class: "production"|"diagnostic", shim: STR ("none" or disclosure),
    qemu_version: STR (identical upstream version across every variant),
    toolchain: STR (identical pinned compiler/toolchain across variants),
    build_records: [PATH,...] (configure/compiler records)}}

Use installed qemu-system-ARCH links to launcher/panda-qemu.c for shared variants.
The engine must be libpanda-ARCH-softmmu.so; PANDA_QEMU_LIBDIR is set explicitly.
A/A aliases must name the identical shared executable/engine/configuration. Both
aliases run preflight before any samples. /proc maps provide actual loaded-engine
provenance, not just an asserted library path. Other loader/PANDA overrides are
rejected. Configuration and every input/artifact are hashed before and after.

No automatic performance verdict. Quiet-window contamination must be reviewed;
mark manifest contamination nonempty to invalidate it. Cold activation, dispatch,
restore and subscription modes need separate correctness-checked workloads, not
an aggregate idle number. This first protocol only supports read-only initrds and
no writable guest drives. No mandatory shared-host timing thresholds.
"""
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

# Explicit local import also works with python -I (script directory is excluded).
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze import Invalid, analyze, digest, load, order, parse, positive


def require(ok, message):
    if not ok:
        raise Invalid(message)


def file_path(value):
    p = Path(value)
    require(p.is_absolute() and p.is_file(), f"not an absolute file: {value}")
    require(not re.search(r"[\s,]", str(p)), f"unsupported whitespace/comma path: {p}")
    return p


def validate(c):
    require(c["schema"] == 1, "unsupported schema")
    for key in ("host_target", "guest_target", "quiet_window"):
        require(isinstance(c[key], str) and c[key].strip(), f"missing {key}")
    require(type(c["repeats"]) is int and 3 <= c["repeats"] <= 100, "repeats must be 3..100")
    require(positive(c["timeout"]) <= 600, "timeout exceeds 600s")
    require(c["cpus"] and all(type(n) is int and n >= 0 for n in c["cpus"])
            and len(set(c["cpus"])) == len(c["cpus"]), "invalid cpus")
    require(set(c["cpus"]) <= os.sched_getaffinity(0), "CPUs outside available affinity")
    require(not isinstance(c["timeout"], bool), "invalid timeout")
    argv = c["argv"]
    require(isinstance(argv, list) and all(isinstance(a, str) for a in argv), "argv must be strings")
    require(not any("icount" in a for a in argv), "icount is forbidden")
    # Restrict the first protocol to a single TCG boot with no writable fixtures.
    flags = {"-nographic", "-no-reboot", "-nodefaults", "-no-user-config"}
    options = {"-M", "-machine", "-cpu", "-m", "-smp", "-accel", "-kernel", "-initrd", "-append", "-L", "-bios"}
    i = 0
    while i < len(argv):
        flag = argv[i]
        require(flag in flags | options, f"unsupported QEMU option: {flag}")
        i += 1
        if flag in options:
            require(i < len(argv), f"missing value for {flag}")
            i += 1
    require(argv.count("-accel") == 1 and argv[argv.index("-accel") + 1] == "tcg",
            "require explicit -accel tcg")
    for key in ("metrics", "higher_better", "exits", "correctness"):
        require(isinstance(c[key], list) and all(isinstance(x, str) and x for x in c[key])
                and len(set(c[key])) == len(c[key]), f"invalid {key}")
    require(c["metrics"] and all(re.fullmatch(r"[a-z][a-z0-9_]*", m) and m != "wall"
                                for m in c["metrics"]), "invalid metrics")
    require(c["exits"] and c["correctness"], "guest exit and correctness records required")
    require(set(c["higher_better"]) <= set(c["metrics"]), "unknown higher_better metric")
    paths = [file_path(p) for p in c["inputs"]]
    launcher_source = file_path(c["launcher_source"])
    require(launcher_source.name == "panda-qemu.c", "pin launcher/panda-qemu.c source")
    paths.append(launcher_source)
    for flag in ("-kernel", "-initrd"):
        require(argv.count(flag) == 1, f"require one {flag}")
        require(file_path(argv[argv.index(flag) + 1]) in paths, f"unhashed {flag}")
    variants = c["variants"]
    require(isinstance(variants, dict) and len(variants) >= 3, "need vanilla and shared A/A aliases")
    for name, v in variants.items():
        require(re.fullmatch(r"[a-zA-Z0-9_-]+", name), "invalid variant name")
        require(v["kind"] in ("shared", "vanilla"), "invalid variant kind")
        require(v["build_class"] in ("production", "diagnostic"), "invalid build class")
        for key in ("source_revision", "patch_tree", "shim", "qemu_version", "toolchain"):
            require(isinstance(v[key], str) and v[key].strip(), f"missing {key}")
        exe = file_path(v["executable"])
        require(os.access(exe, os.X_OK), "executable not runnable")
        paths.append(exe)
        require(v["build_records"], "configure and compiler records required")
        paths.extend(file_path(p) for p in v["build_records"])
        if v["kind"] == "shared":
            require(Path(v["executable"]).name == f"qemu-system-{c['guest_target']}",
                    "shared executable must be an installed launcher link")
            engine = file_path(v["engine"])
            require(Path(v["engine"]).name == f"libpanda-{c['guest_target']}-softmmu.so",
                    "engine name does not match guest target")
            paths.append(engine)
    for key in ("qemu_version", "toolchain"):
        require(len({v[key] for v in variants.values()}) == 1, f"mismatching {key}")
    require(variants[c["reference"]]["kind"] == "vanilla", "reference must be matching vanilla")
    aa = c["aa"]
    require(len(aa) == 2 and len(set(aa)) == 2, "need distinct A/A aliases")
    a, b = (variants[n] for n in aa)
    require(a["kind"] == b["kind"] == "shared", "A/A must use actual shared launcher")
    require(a == b, "A/A must have identical paths/build configuration, not merely similar builds")
    return sorted(set(paths))


def command(c, v):
    env = {k: os.environ[k] for k in ("PATH", "HOME", "TZ", "TMPDIR") if k in os.environ}
    env["LC_ALL"] = "C"
    if v["kind"] == "shared":
        env["PANDA_QEMU_LIBDIR"] = str(Path(v["engine"]).parent)
    return [v["executable"], *c["argv"]], env


def run_process(argv, env, path, timeout, cpus, engine=None):
    """Bound the entire process group and preserve the first failure console."""
    start = time.monotonic()
    maps = ""
    timed_out = False
    with open(path, "xb") as console:
        p = subprocess.Popen(["taskset", "-c", ",".join(map(str, cpus)), *argv],
                             env=env, stdin=subprocess.DEVNULL, stdout=console,
                             stderr=subprocess.STDOUT, start_new_session=True)
        try:
            while p.poll() is None:
                if engine and not maps:
                    try:
                        text = Path(f"/proc/{p.pid}/maps").read_text()
                        if any(line.split()[-1:] == [str(Path(engine).resolve())] for line in text.splitlines()):
                            maps = text
                    except (OSError, ProcessLookupError):
                        pass
                remaining = timeout - (time.monotonic() - start)
                if remaining <= 0:
                    timed_out = True
                    break
                try:
                    p.wait(timeout=min(.02, remaining))
                except subprocess.TimeoutExpired:
                    pass
        finally:
            # Also reap descendants on success; a stray child cannot contaminate later cells.
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            p.wait()
        wall = time.monotonic() - start
        console.write(f"\nRC {p.returncode} WALL {wall:.9f}\n".encode())
    return {"rc": p.returncode, "timeout": timed_out, "wall": wall}, maps


def save(root, manifest):
    tmp = root / "manifest.tmp"
    tmp.write_text(json.dumps(manifest, indent=2, allow_nan=False) + "\n")
    tmp.replace(root / "manifest.json")


def execute(c, root, config_path):
    paths = validate(c)
    for key in os.environ:
        require(not key.startswith(("PANDA_", "LD_", "QEMU_")), f"unset loader override {key}")
    root = Path(root)
    root.mkdir()  # Never reuse or delete an existing output directory.
    manifest = {"schema": 1, "state": "running", "config": c,
                "config_path": str(Path(config_path).resolve()), "config_sha256": digest(config_path),
                "started": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                "host": {"uname": list(os.uname()), "load": os.getloadavg(),
                         "affinity": sorted(os.sched_getaffinity(0)),
                         "environment": {k: os.environ[k] for k in ("PATH", "LANG", "LC_ALL", "TZ", "TMPDIR")
                                         if k in os.environ}},
                "runner_sha256": digest(__file__),
                "analyzer_sha256": digest(Path(__file__).with_name("analyze.py")),
                "artifacts": [{"path": str(p), "resolved": str(p.resolve()), "sha256": digest(p)} for p in paths],
                "contamination": "", "runs": []}
    save(root, manifest)
    try:
        for phase, count in (("preflight", 1), ("sample", c["repeats"])):
            for rep in range(count):
                for name in order(list(c["variants"]), rep):
                    v = c["variants"][name]
                    log = f"{phase}-{rep:03}-{name}.con"
                    argv, env = command(c, v)
                    cell = {"phase": phase, "rep": rep, "variant": name, "log": log,
                            "argv": argv, "load_before": os.getloadavg()}
                    manifest["runs"].append(cell)
                    save(root, manifest)
                    result, maps = run_process(argv, env, root / log, c["timeout"], c["cpus"], v.get("engine"))
                    cell.update(result, sha256=digest(root / log), load_after=os.getloadavg())
                    if v["kind"] == "shared":
                        cell["maps"] = log + ".maps"
                        (root / cell["maps"]).write_text(maps)
                        cell["maps_sha256"] = digest(root / cell["maps"])
                        require(maps, "shared engine not observed in /proc maps")
                    require(not result["timeout"] and result["rc"] == 0, f"failed cell {log}")
                    parse((root / log).read_text(), c["metrics"], c["exits"], c["correctness"])
                    save(root, manifest)
        require(digest(config_path) == manifest["config_sha256"], "config changed during experiment")
        require(all(digest(a["path"]) == a["sha256"] and str(Path(a["path"]).resolve()) == a["resolved"]
                    for a in manifest["artifacts"]), "artifact changed")
        manifest["state"] = "complete"
        save(root, manifest)
        report = analyze(root)
        (root / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    except BaseException as e:
        manifest.update(state="invalid", failure=f"{type(e).__name__}: {e}")
        save(root, manifest)
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--execute", action="store_true")
    parser.add_argument("config")
    parser.add_argument("out", nargs="?")
    args = parser.parse_args()
    try:
        config = load(args.config)
        validate(config)
        if args.check:
            print("Configuration valid; no guest executed. Artifact provenance and quiet-window review still required.")
        else:
            require(args.out, "NEW_OUT required")
            execute(config, args.out, args.config)
    except (Invalid, OSError, ValueError, KeyError, TypeError, IndexError) as e:
        sys.exit(f"INVALID: {e}")
