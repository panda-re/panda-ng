#!/usr/bin/env python3
"""Result schema 1 and process supervisor for run.sh (stdlib only).

The coordinator owns the denominator and final reconciliation. Image workers never
append to a shared result file. Every state transition is an atomic JSON replacement.
"""
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import selectors
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import time

import inventory

SCHEMA = 1
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
LOG_LIMIT = 1024 * 1024
STOP = None


def interrupted(signum, _frame):
    global STOP
    STOP = signum


def write_json(path, value):
    path = Path(path)
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    tmp.replace(path)


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def identity(path):
    p = Path(path)
    result = {"path": str(p), "resolved_path": str(p.resolve())}
    try:
        result.update(size=p.stat().st_size, sha256=digest(p))
    except OSError as e:
        result["error"] = str(e)
    return result


def git(*args):
    p = subprocess.run(["git", "-C", str(ROOT), *args], stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, timeout=30)
    if p.returncode:
        raise RuntimeError(p.stderr.decode(errors="replace"))
    return p.stdout


def source_identity():
    try:
        patches = git("ls-files", "--cached", "--others", "--exclude-standard", "-z", "engine/patches").decode().split("\0")
        tree = [{"path": p, "sha256": digest(ROOT / p)} for p in patches if p]
        return {"root": str(ROOT), "revision": git("rev-parse", "HEAD").decode().strip(),
                "status": git("status", "--porcelain").decode(),
                "diff_sha256": hashlib.sha256(git("diff", "HEAD", "--binary")).hexdigest(),
                "patch_tree_sha256": hashlib.sha256(json.dumps(tree, sort_keys=True).encode()).hexdigest(),
                "runner": [identity(HERE / p) for p in ("run.sh", "run_results.py", "testlib.py", "inventory.py")]}
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as e:
        return {"root": str(ROOT), "error": str(e)}


def fixture_identity(path):
    """Inventory before any preparation; missing/invalid fixtures remain planned cells."""
    p = Path(path)
    result = {"json": str(p), "files": []}
    files = {p}
    try:
        data = json.loads(p.read_text())
        result["arch"] = data["arch"]
        if not data.get("qcow"):
            raise ValueError("image JSON has no qcow")
        result["references"] = {}
        for key in ("qcow", "isf", "kconf_file"):
            if data.get(key):
                ref = (p.parent / data[key]).absolute()
                result["references"][key] = str(ref)
                files.add(ref)
        files.update(p.parent.glob(p.stem + ".*"))
    except (OSError, ValueError, KeyError, TypeError) as e:
        result["error"] = str(e)
    result["files"] = [identity(f) for f in sorted(files) if not f.is_dir()]
    return result


def signal_group(proc, sig):
    try:
        os.killpg(proc.pid, sig)
    except ProcessLookupError:
        pass


def process_evidence(pid, loaded):
    """Linux best effort: actual mappings, not loader search paths or guessed selection."""
    evidence = []
    pending = [pid]
    seen = set()
    while pending and len(seen) < 64:
        p = pending.pop()
        if p in seen:
            continue
        seen.add(p)
        base = Path("/proc") / str(p)
        item = {"pid": p}
        for name in ("cmdline", "status", "wchan", "stack"):
            try:
                item[name] = (base / name).read_bytes()[:8192].decode(errors="replace").replace("\0", " ")
            except OSError:
                pass
        try:
            pending.extend(int(x) for x in (base / "task" / str(p) / "children").read_text().split())
        except OSError:
            pass
        try:
            for line in (base / "maps").read_text().splitlines():
                fields = line.split(None, 5)
                if len(fields) == 6 and fields[5].startswith("/"):
                    path = re.sub(r"\\([0-7]{3})", lambda m: chr(int(m[1], 8)), fields[5])
                    # Keep engine/plugin/Python mappings, not all host libc mappings.
                    if any(s in path.lower() for s in ("panda", "qemu", "plugin", "python")) and len(loaded) < 1024:
                        if path not in loaded:
                            loaded[path] = identity(path)
        except OSError:
            pass
        evidence.append(item)
    return evidence


def execute(argv, cwd, env, log, timeout, grace, started=None):
    """Bounded log + explicit watchdog bit. A plain SIGKILL is never a timeout."""
    start = time.monotonic()
    proc = subprocess.Popen(argv, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, start_new_session=True)
    if started:
        started(proc.pid)
    sel = selectors.DefaultSelector()
    sel.register(proc.stdout, selectors.EVENT_READ)
    head = bytearray()
    tail = bytearray()
    count = 0
    line_buffer = b""
    skip_lines = []
    timed_out = False
    stopped = False
    term_at = None
    loaded = {}
    evidence = []
    next_sample = 0
    with open(log, "wb") as f:
        while sel.get_map() or proc.poll() is None:
            now = time.monotonic()
            if now >= next_sample:
                sample = process_evidence(proc.pid, loaded)
                if sample:
                    evidence = sample
                next_sample = now + 0.25
            if term_at is None and (STOP or now - start >= timeout):
                stopped = bool(STOP)
                timed_out = not stopped
                term_at = now
                signal_group(proc, signal.SIGTERM)
            if term_at is not None and now - term_at >= grace:
                signal_group(proc, signal.SIGKILL)
                # An escaped/inherited pipe must not keep the supervisor alive indefinitely.
                if now - term_at >= grace + 1:
                    break
            for key, _ in sel.select(0.05):
                data = os.read(key.fileobj.fileno(), 65536)
                if not data:
                    sel.unregister(key.fileobj)
                    continue
                count += len(data)
                lines = (line_buffer + data).split(b"\n")
                line_buffer = lines.pop()[-8192:]
                for line in lines:
                    text_line = line.decode(errors="replace")
                    if re.search(r"(^SKIP\b|:\s*SKIP\b)", text_line) and len(skip_lines) < 32:
                        skip_lines.append(text_line[:4096])
                room = LOG_LIMIT // 2 - len(head)
                if room > 0:
                    part = data[:room]
                    head.extend(part)
                    f.write(part)
                    f.flush()
                    data = data[room:]
                tail.extend(data)
                if len(tail) > LOG_LIMIT // 2:
                    del tail[:-LOG_LIMIT // 2]
        signal_group(proc, signal.SIGKILL)  # no orphan children after a successful leader exit
        proc.wait(timeout=5)
        if count > len(head) + len(tail):
            f.write(b"\n[runner: log truncated; retaining head and tail]\n")
        f.write(tail)
    sel.close()
    proc.stdout.close()
    last_line = line_buffer.decode(errors="replace")
    if re.search(r"(^SKIP\b|:\s*SKIP\b)", last_line):
        skip_lines.append(last_line[:4096])
    if stopped:
        status, reason = "CANCELLED", "runner received signal"
    elif timed_out:
        status, reason = "TIMEOUT", "watchdog deadline exceeded"
    elif proc.returncode < 0:
        status, reason = "CRASH", signal.Signals(-proc.returncode).name
    elif proc.returncode == 77:
        status, reason = "SKIP", "test exited 77 (unsupported)"
    elif proc.returncode:
        status, reason = "FAIL", "test returned nonzero"
    elif skip_lines:
        # Legacy scripts print skip and exit zero, including partial subtest exclusions.
        status, reason = "SKIP", "; ".join(skip_lines)[:4096]
    else:
        status, reason = "PASS", "test exited zero"
    return {"argv": argv, "cwd": str(cwd), "status": status, "reason": reason, "returncode": proc.returncode,
            "signal": -proc.returncode if proc.returncode < 0 else None,
            "watchdog_fired": timed_out, "seconds": round(time.monotonic() - start, 3),
            "log_bytes": count, "log_truncated": count > LOG_LIMIT,
            "loaded_paths": list(loaded.values()), "process_evidence": evidence,
            "loaded_paths_note": "observed /proc mappings only; sampling may miss short-lived loads; no observation is not proof of absence"}


def prepare(fixture, work):
    if fixture.get("error"):
        raise ValueError(fixture["error"])
    img = work / "img"
    img.mkdir()
    # Copy JSON and qcow2 (never symlink writable inputs). Symbol files stay read-only links.
    refs = fixture["references"]
    destinations = {}
    for index, item in enumerate(fixture["files"]):
        if item.get("error"):
            raise ValueError(item["error"])
        src = Path(item["path"])
        dest = img / f"{index}-{src.name}"
        if src.suffix in (".json", ".qcow2") or str(src) == refs["qcow"]:
            subprocess.run(["cp", "--reflink=auto", "--sparse=always", "--", str(src), str(dest)], check=True)
        else:
            dest.symlink_to(src)
        destinations[str(src)] = dest
    original = Path(fixture["json"])
    data = json.loads(original.read_text())
    for key, ref in refs.items():
        data[key] = destinations[ref].name
    copied = destinations[str(original)]
    write_json(copied, data)
    return copied


def worker(out, image_id):
    global STOP
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    plan = json.loads((out / "plan.json").read_text())
    fixture = next(i for i in plan["images"] if i["id"] == image_id)
    for cell in (c for c in plan["cells"] if c["image_id"] == image_id):
        cell_dir = out / cell["id"]
        for attempt in range(1, plan["retries"] + 2):
            if STOP:
                return 1
            work = cell_dir / f"attempt-{attempt:03d}"
            work.mkdir()
            repro_env = {k: os.environ[k] for k in ("PANDA_LIB", "PANDA_PLUGIN_DIR", "PANDA_QEMU_LIBDIR", "PYTHONPATH", "LD_LIBRARY_PATH", "LD_PRELOAD", "RRCB_NO_ICOUNT") if k in os.environ}
            repro_env.update(PANDA_PYTHON=plan["python"], TESTS=cell["test"], TIMEOUT=str(plan["timeout"]),
                             KILL_GRACE=str(plan["kill_grace"]), RETRIES="0", JOBS="1")
            result = {"schema_version": SCHEMA, "cell_id": cell["id"], "attempt": attempt,
                      "status": "RUNNING", "started": time.time(),
                      "reproduce": "env -u OUT " + " ".join(shlex.quote(k + "=" + v) for k, v in repro_env.items()) +
                                   " " + shlex.join([str(HERE / "run.sh"), fixture["json"]]),
                      "reproduce_note": "uses new output and fresh fixture copies; requires recorded source, interpreter and fixtures unchanged"}
            write_json(work / "result.json", result)
            tmp = None
            try:
                if fixture.get("error") or any(f.get("error") for f in fixture["files"]):
                    raise ValueError("invalid/missing fixture; see plan.json")
                requirements = cell["requirements"]
                script = HERE / (requirements["script"] + ".py")
                if not script.is_file():
                    raise ValueError(f"missing test script: {script}")
                result["requirements"] = requirements
                reasons = inventory.unmet(requirements, fixture["arch"])
                if reasons:
                    result.update(status="SKIP", reason="; ".join(reasons), requirements=requirements)
                    write_json(work / "result.json", result)
                    write_json(cell_dir / "result.json", result)
                    break
                tmp = tempfile.mkdtemp(prefix="pt.", dir="/tmp")
                result["temporary_directory"] = tmp
                write_json(work / "result.json", result)
                copied = prepare(fixture, work)
                argv = [plan["python"], str(script), str(copied), *requirements["args"]]
                if requirements["script"] in ("rr", "rrcb", "rrstop", "rrload", "rr_blocks"):
                    (work / "rr").mkdir()
                    argv.append(str(work / "rr"))
                env = dict(os.environ, TMPDIR=tmp, PYTHONUNBUFFERED="1")
                if fixture["arch"] != "x86_64":
                    env["RRCB_NO_ICOUNT"] = "1"
                result.update(argv=argv, cwd=str(work), environment={k: env.get(k) for k in
                              ("TMPDIR", "RRCB_NO_ICOUNT", "PANDA_LIB", "PANDA_PLUGIN_DIR", "PANDA_QEMU_LIBDIR", "PYTHONPATH")},
                              test_source=identity(script), fixture_json=str(copied))
                write_json(work / "result.json", result)
                def started(pid):
                    result["process_group"] = pid
                    write_json(work / "result.json", result)
                result.update(execute(argv, work, env, work / "test.log", plan["timeout"], plan["kill_grace"], started))
            except Exception as e:
                result.update(status="ERROR", reason=f"setup/supervisor failure: {type(e).__name__}: {e}")
            finally:
                if tmp:
                    shutil.rmtree(tmp)
            result["finished"] = time.time()
            write_json(work / "result.json", result)
            if attempt == 1:
                # First outcome is authoritative, even if a retry passes.
                write_json(cell_dir / "result.json", result)
            if result["status"] == "PASS" and not plan["keep"]:
                shutil.rmtree(work / "img")
                if (work / "rr").exists():
                    shutil.rmtree(work / "rr")
            if result["status"] in ("PASS", "SKIP", "CANCELLED"):
                break
    return 0


def cleanup_abandoned(result):
    if result.get("process_group"):
        try:
            os.killpg(result["process_group"], signal.SIGKILL)
        except ProcessLookupError:
            pass
    tmp = Path(result.get("temporary_directory", "/nonexistent"))
    if tmp.parent == Path("/tmp") and tmp.name.startswith("pt.") and tmp.is_dir() and not tmp.is_symlink():
        shutil.rmtree(tmp)


def reconcile(out, plan, reason=None):
    records = []
    for cell in plan["cells"]:
        path = out / cell["id"] / "result.json"
        result = json.loads(path.read_text())
        if result["status"] == "PLANNED":
            attempts = sorted((out / cell["id"]).glob("attempt-*/result.json"))
            if attempts:
                result = json.loads(attempts[0].read_text())
            if result["status"] in ("PLANNED", "RUNNING"):
                cleanup_abandoned(result)
                result.update(status="CANCELLED" if STOP else "ERROR",
                              reason=reason or "image worker died before publishing result")
                if attempts:
                    write_json(attempts[0], result)
            write_json(path, result)
        # Reconcile interrupted retries too, without replacing the original outcome.
        for attempt in sorted((out / cell["id"]).glob("attempt-*/result.json")):
            data = json.loads(attempt.read_text())
            if data["status"] == "RUNNING":
                cleanup_abandoned(data)
                data.update(status="CANCELLED" if STOP else "ERROR",
                            reason=reason or "image worker died during retry")
                write_json(attempt, data)
        records.append(dict(cell, result=result))
    write_json(out / "results.json", {"schema_version": SCHEMA, "cells": records})
    with open(out / "results", "w") as f:
        for cell in records:
            r = cell["result"]
            f.write(f"{r['status']} {r.get('seconds', 0)}s {cell['image_id']} {cell['test']}\n")
    return records


def main(args):
    global STOP
    if args and args[0] == "--worker":
        return worker(Path(args[1]), args[2])
    if not args:
        print("usage: tests/run.sh IMAGE.json|DIRECTORY ...", file=sys.stderr)
        return 2
    tier = os.environ.get("TIER") or None
    selection = inventory.select(tier, os.environ.get("TESTS"))
    tests = [row["name"] for row in selection]
    if not tests or any(not re.fullmatch(r"[A-Za-z0-9_]+", t) for t in tests):
        raise ValueError("TESTS must contain simple test names")
    timeout = float(os.environ.get("TIMEOUT", "1800"))
    grace = float(os.environ.get("KILL_GRACE", "30"))
    retries = int(os.environ.get("RETRIES", "0"))
    if not (0 < timeout < float("inf") and 0 <= grace < float("inf") and 0 <= retries <= 100):
        raise ValueError("invalid TIMEOUT, KILL_GRACE or RETRIES")
    paths = []
    for a in args:
        p = Path(a).absolute()
        paths.extend(sorted(p.glob("*.json")) if p.is_dir() else [p])
    if not paths:
        raise ValueError("no image JSONs selected")
    jobs = int(os.environ.get("JOBS", str(len(paths))))
    if jobs < 1:
        raise ValueError("JOBS must be positive")
    python = os.environ.get("PANDA_PYTHON", "panda-python")
    python = shutil.which(python) or str(Path(python).absolute())
    if "OUT" in os.environ:
        out = Path(os.environ["OUT"]).absolute()
        out.mkdir()  # Deliberately exclusive, including an empty preexisting directory.
    else:
        out = Path(tempfile.mkdtemp(prefix="panda-tests-", dir=os.environ.get("TMPDIR", "/tmp")))
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    plan = {"schema_version": SCHEMA, "images": [], "cells": [], "python": python,
            "timeout": timeout, "kill_grace": grace, "retries": retries,
            "keep": os.environ.get("KEEP", "0") == "1", "jobs": jobs,
            "tier": tier, "inventory_schema_version": inventory.SCHEMA_VERSION}
    # Persist the full denominator BEFORE reading or hashing any fixture or probing Python.
    for index, p in enumerate(paths):
        image_id = f"{index + 1:03d}-" + re.sub(r"[^A-Za-z0-9_.-]", "_", p.stem)[:100]
        plan["images"].append({"id": image_id, "json": str(p)})
        for n, test in enumerate(tests):
            cell_id = f"{image_id}/{n + 1:03d}-{test}"
            cell = {"id": cell_id, "image_id": image_id, "test": test, "requirements": selection[n]}
            plan["cells"].append(cell)
            d = out / cell_id
            d.mkdir(parents=True)
            write_json(d / "result.json", {"schema_version": SCHEMA, "cell_id": cell_id, "status": "PLANNED"})
    write_json(out / "plan.json", plan)
    print(f"run.sh: {len(paths)} image(s) x {len(tests)} test(s); results in {out}", flush=True)
    active = {}
    worker_exits = {}
    fatal = None
    try:
        write_json(out / "provenance.json", {"schema_version": SCHEMA, "source": source_identity(),
                   "host": {"system": platform.platform(), "machine": platform.machine()},
                   "selection": {k: os.environ.get(k) for k in ("TESTS", "PANDA_LIB", "PANDA_PLUGIN_DIR", "PANDA_QEMU_LIBDIR", "PYTHONPATH")},
                   "interpreter": identity(python),
                   "exclusions": [row["name"] for row in inventory.INVENTORY if row["name"] not in tests],
                   "selection_note": "exclusions are not selected/untested, not passed; prerequisites are evaluated per planned cell",
                   "note": "selection paths are not evidence of loaded libraries; see per-attempt observed loaded_paths"})
        for image in plan["images"]:
            image.update(fixture_identity(image["json"]))
            write_json(out / "plan.json", plan)
        probe = execute([python, "-c", "import pandare2; print(pandare2.__file__)"], out,
                        dict(os.environ), out / "python-probe.log", min(timeout, 30), grace)
        write_json(out / "python-probe.json", probe)
        if probe["status"] != "PASS":
            raise RuntimeError("PANDA_PYTHON preflight failed; see python-probe.log/json")
        pending = list(plan["images"])
        cancelling_at = None
        while pending or active:
            if STOP:
                pending.clear()
                if cancelling_at is None:
                    cancelling_at = time.monotonic()
                    for proc in active:
                        proc.terminate()
                elif time.monotonic() - cancelling_at > grace + 3:
                    for proc in active:
                        proc.kill()
            while pending and len(active) < jobs and not STOP:
                image = pending.pop(0)
                log = open(out / image["id"] / "worker.log", "wb")
                proc = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "--worker", str(out), image["id"]],
                                        stdout=log, stderr=subprocess.STDOUT)
                active[proc] = (image["id"], log)
            for proc in list(active):
                if proc.poll() is not None:
                    image_id, log = active.pop(proc)
                    log.close()
                    worker_exits[image_id] = proc.returncode
                    if proc.returncode:
                        for attempt in (out / image_id).glob("*/attempt-*/result.json"):
                            unfinished = json.loads(attempt.read_text())
                            if unfinished["status"] == "RUNNING":
                                cleanup_abandoned(unfinished)
            if pending or active:
                time.sleep(0.05)
    except Exception as e:
        fatal = f"coordinator failure: {type(e).__name__}: {e}"
        print(fatal, file=sys.stderr)
    finally:
        for proc in active:
            proc.terminate()
        for proc, (image_id, log) in active.items():
            try:
                proc.wait(timeout=grace + 3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
            log.close()
            worker_exits[image_id] = proc.returncode
        records = reconcile(out, plan, fatal)
        changes = []
        for image in plan["images"]:
            for item in image.get("files", []):
                after = identity(item["path"])
                if after != item:
                    changes.append({"before": item, "after": after})
        write_json(out / "completion.json", {"schema_version": SCHEMA, "cancel_signal": STOP,
                   "fatal": fatal, "worker_exits": worker_exits, "fixture_changes": changes})
    statuses = [c["result"]["status"] for c in records]
    print(f"run.sh: {statuses.count('PASS')}/{len(statuses)} passed, {statuses.count('SKIP')} skipped ({out / 'results'})")
    if STOP:
        return 128 + STOP
    return int(bool(fatal or changes or any(rc != 0 for rc in worker_exits.values()) or
                    any(s not in ("PASS", "SKIP") for s in statuses)))


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except (ValueError, OSError) as e:
        print(f"run.sh: {e}", file=sys.stderr)
        sys.exit(2)
