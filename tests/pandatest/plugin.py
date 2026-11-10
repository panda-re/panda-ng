"""The pytest plugin behind tests/run: collects one item per (test, arch, variant) from each test
file's PANDA_TEST metadata, selects them, runs each as its own process (one Panda per process)
and writes JUnit and JSON results. See tests/README.md, "Running tests".

Static and harness items run in parallel (--jobs); guest items run one at a time, each after a
resource guard. Every item runs pinned to --cpus with taskset.
"""
import concurrent.futures
import fcntl
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

import pytest

from . import fixtures, meta

PARALLEL = ("static", "harness")
ORDER = {t: i for i, t in enumerate(meta.TIERS)}
NO_META = "no metadata: not run by tests/run"


def pytest_addoption(parser):
    g = parser.getgroup("panda", "panda-ng tests (tests/run)")
    g.addoption("--panda-select", action="append", default=[], metavar="TERM",
                help="an area, test name or item id glob (repeatable; default: everything)")
    g.addoption("--arch", default="", help="comma-separated arches (engine target names, or mkbusybox's)")
    g.addoption("--variants", choices=("all", "default"), default="all",
                help="every combination of the variants (default), or each at its default value")
    g.addoption("--no-wait", action="store_true",
                help="skip a guest item at once when the resource guard fails (default: wait up to "
                     "$PANDATEST_GUARD_WAIT seconds, 1800, re-checking)")
    g.addoption("--tier", default="", help=f"comma-separated tiers: {', '.join(meta.TIERS)}")
    g.addoption("--list", action="store_true", help="list the selected items and stop")
    g.addoption("--explain", action="store_true", help="with each item, why it is selected or skipped")
    g.addoption("--build-fixtures", action="store_true", help="build missing fixtures (default: skip)")
    g.addoption("--cpus", default=os.environ.get("PANDATEST_CPUS") or
                ("64-79" if (os.cpu_count() or 0) >= 80 else ""),
                help="taskset cpu list for every item (default $PANDATEST_CPUS, or 64-79)")
    g.addoption("--jobs", type=int, default=8, help="static and harness items at once (default 8)")
    g.addoption("--out", default=None, help="results, logs and work dirs (default $TMPDIR/panda-tests-<time>)")
    g.addoption("--build", default=None, help="the engine build: a tools/dev/dev.sh directory")
    g.addoption("--keep", action="store_true", help="keep every item's work directory")


class State:
    def __init__(self, config):
        o = config.option
        self.config = config
        self.engine = fixtures.Engine(o.build)
        self.out = os.path.abspath(o.out or os.path.join(
            tempfile.gettempdir(), time.strftime("panda-tests-%Y%m%d-%H%M%S")))
        self.cpus = o.cpus
        self.fixtures = {}
        self.records = []
        self.pool = None

    def fixture(self, name):
        if name not in self.fixtures:
            self.fixtures[name] = fixtures.resolve(name, self.engine)
        return self.fixtures[name]


def pytest_configure(config):
    config._panda = State(config)


def pytest_collect_file(file_path, parent):
    p = str(file_path)
    if p.endswith((".py", ".sh")) and os.path.basename(p) != "conftest.py" and \
            os.path.dirname(p) in (fixtures.TESTS, os.path.join(fixtures.TESTS, "static")):
        return PandaFile.from_parent(parent, path=file_path)
    return None


class PandaFile(pytest.File):
    def collect(self):
        path = str(self.path)
        rel = os.path.splitext(os.path.relpath(path, fixtures.TESTS))[0]
        stem = os.path.basename(rel)
        try:
            found, m = meta.read(path)
        except (meta.MetaError, SyntaxError, ValueError) as e:
            yield PandaItem.from_parent(self, name=stem, rel=rel, error=f"PANDA_TEST: {e}")
            return
        if not found:
            yield PandaItem.from_parent(self, name=stem, rel=rel)
            return
        if m is None:
            return
        for arch, params in meta.expand(m, self.config.option.variants == "default"):
            yield PandaItem.from_parent(self, name=meta.item_name(stem, arch, params), rel=rel,
                                        m=m, arch=arch, params=params)


class PandaItem(pytest.Item):
    def __init__(self, *, rel, m=None, arch=None, params=None, error=None, **kw):
        super().__init__(**kw)
        self.rel, self.meta, self.arch, self.params, self.error = rel, m, arch, params or {}, error
        self.id = os.path.dirname(rel) + "/" + self.name if os.path.dirname(rel) else self.name
        self.tier = m["tier"] if m else None
        self.area = m["area"] if m else None
        self.panda_arch = meta.panda_arch(arch)
        self.fixture = None
        self.skip_reason = None
        self.why = "everything"
        self.future = None
        self.result = None
        self.xfail = meta.xfail_for(m, arch, self.params) if m else None
        if self.xfail:
            issue = f" ({self.xfail['issue']})" if self.xfail.get("issue") else ""
            self.add_marker(pytest.mark.xfail(reason=self.xfail["reason"] + issue, strict=True))

    def reportinfo(self):
        return self.path, 0, self.id

    # Everything that decides whether the item can run, before anything runs.
    def prepare(self, st):
        if self.error:
            return
        if not self.meta:
            self.skip_reason = NO_META
            self.add_marker(pytest.mark.skip(reason=NO_META))
            return
        m, eng = self.meta, st.engine
        self.fixture = st.fixture(meta.fixture_name(m, self.arch, self.params))
        if self.tier not in ("static",) and str(self.path).endswith(".py") and not eng.python:
            self.skip_reason = "no PANDA_PYTHON (--build, or PANDA_PYTHON)"
        elif self.tier != "static" and not eng.has_target(self.panda_arch):
            self.skip_reason = f"the engine has no {self.panda_arch} target ({eng.lib})"
        elif missing := [p for p in m.get("plugins", []) if not p.startswith("test:")
                         and not eng.has_plugin(p, self.panda_arch)]:
            self.skip_reason = f"plugin {missing[0]} not in {eng.plugins}"
        elif missing := [n for n in m.get("needs", []) if not have(n, eng)]:
            self.skip_reason = (f"needs an engine build with all {len(meta.TARGETS)} targets"
                                if missing[0] == "all-targets" else f"needs {missing[0]}, not found")
        elif not self.fixture.ready and not (st.config.option.build_fixtures and self.fixture.builds
                                             and self.tier not in PARALLEL):
            self.skip_reason = self.fixture.reason
        else:
            try:
                self.command(st, dry=True)
            except KeyError as e:
                self.skip_reason = f"needs {{{e.args[0]}}}: not known for this engine build"
        if self.skip_reason:
            self.add_marker(pytest.mark.skip(reason=self.skip_reason))

    def values(self, st, work, fixture_path):
        eng = st.engine
        v = dict(fixture=fixture_path, arch=self.arch, panda_arch=self.panda_arch, work=work,
                 repo=fixtures.REPO, tests=fixtures.TESTS, engine_src=eng.src,
                 engine_prefix=eng.prefix, plugin_dir=eng.plugins)
        return {k: x for k, x in v.items() if x is not None}

    def command(self, st, work="", fixture_path=None, dry=False):
        m = self.meta
        vals = self.values(st, work, fixture_path or (self.fixture.path if self.fixture else None))
        if dry and "fixture" not in vals and self.fixture and self.fixture.kind != "none":
            vals["fixture"] = "<fixture>"
        args = m.get("args", ["{fixture}"] if m.get("fixture", "none") != "none" else [])
        args = [a.format_map(vals) for a in args] + meta.variant_args(self.params)
        env = {k: str(x).format_map(vals) for k, x in m.get("env", {}).items()}
        env.update(meta.variant_env(self.params))
        script = str(self.path)
        python = st.engine.python if (self.tier != "static" or st.engine.python) else sys.executable
        return (["bash"] if script.endswith(".sh") else [python]) + [script] + args, env

    def execute(self, st):
        """Run the test in its own process; (rc, seconds, log path)."""
        safe = self.id.replace("/", "_").replace("[", "-").replace("]", "").replace(",", "-")
        work = os.path.join(st.out, "work", safe)
        log = os.path.join(st.out, "logs", safe + ".log")
        shutil.rmtree(work, ignore_errors=True)
        os.makedirs(work)
        os.makedirs(os.path.dirname(log), exist_ok=True)
        # Short: a UNIX socket's path is limited to 108 bytes.
        tmp = tempfile.mkdtemp(prefix="pt.")
        fx = self.fixture
        path = private_copy(fx.path, os.path.join(work, "img")) if fx.copy and fx.path else fx.path
        argv, env = self.command(st, work, path)
        full = dict(os.environ, **st.engine.env(), **fx.env, **env, TMPDIR=tmp)
        timeout = self.meta.get("timeout", 1800 if self.tier not in PARALLEL else 900)
        pre = (["taskset", "-c", st.cpus] if st.cpus else []) + ["timeout", "-k", "30", str(timeout)]
        t0 = time.monotonic()
        with open(log, "w") as f:
            f.write("# " + " ".join(f"{k}={v}" for k, v in sorted(env.items())) + " " +
                    " ".join(argv) + "\n")
            f.flush()
            rc = subprocess.run(pre + argv, cwd=work, env=full, stdout=f, stderr=subprocess.STDOUT,
                                stdin=subprocess.DEVNULL).returncode
        secs = time.monotonic() - t0
        with open(log, "a") as f:
            f.write(f"# rc={rc} {secs:.1f}s\n")
        shutil.rmtree(tmp, ignore_errors=True)
        if rc == 0 and not st.config.option.keep:
            shutil.rmtree(work, ignore_errors=True)
        return rc, secs, log

    def runtest(self):
        st = self.config._panda
        if self.error:
            raise PandaFailure(self.error)
        if self.future:
            self.result = self.future.result()
        else:
            reason = wait_for_guard(st, self)
            if reason:
                pytest.skip(f"resource guard: {reason}")
            if not self.fixture.ready:
                self.fixture = st.fixture(self.fixture.name)    # an earlier item may have built it
            if not self.fixture.ready:
                build_fixture(st, self)
            self.result = self.execute(st)
        rc, secs, log = self.result
        if rc:
            raise PandaFailure(f"rc={rc}{' (timeout)' if rc in (124, 137) else ''} after {secs:.0f}s, "
                               f"log: {log}\n" + tail(log))

    def repr_failure(self, excinfo):
        if isinstance(excinfo.value, PandaFailure):
            return str(excinfo.value)
        return super().repr_failure(excinfo)


class PandaFailure(Exception):
    pass


def tail(path, n=30):
    with open(path, errors="replace") as f:
        return "".join(f.readlines()[-n:])


def have(need, eng):
    if need == "pandare2":
        return bool(eng.python)
    if need == "all-targets":
        return all(eng.has_target(t) for t in meta.TARGETS)
    if need == "cc":
        return bool(shutil.which(os.environ.get("CC", "cc").split()[0]))
    if need == "c++":
        return bool(shutil.which(os.environ.get("CXX", "c++").split()[0]))
    if need == "glib":
        return subprocess.run(["pkg-config", "--exists", "glib-2.0"]).returncode == 0
    if need.startswith("py:"):
        return subprocess.run([eng.python or sys.executable, "-c", f"import {need[3:]}"],
                              capture_output=True).returncode == 0
    return bool(shutil.which(need))


def private_copy(json_path, dest):
    """The image's json and qcow2 copied (tests write snapshots), its other files linked."""
    os.makedirs(dest, exist_ok=True)
    src = os.path.dirname(json_path)
    name = os.path.basename(json_path)[:-len(".json")]
    for f in os.listdir(src):
        if f.startswith(name + "."):
            s, d = os.path.join(src, f), os.path.join(dest, f)
            if f.endswith(".qcow2"):
                subprocess.run(["cp", "--reflink=auto", "--sparse=always", s, d], check=True)
            elif f.endswith(".json"):
                shutil.copy(s, d)
            else:
                os.symlink(s, d)
    return os.path.join(dest, name + ".json")


def guard(st):
    """Why a guest item must not start now, or None."""
    gb = lambda n: n / (1 << 30)  # noqa: E731
    path = "/home" if os.path.isdir("/home") else "/"
    free = gb(shutil.disk_usage(path).free)
    if free < float(os.environ.get("PANDATEST_MIN_FREE_GB", 45)):
        return f"{free:.0f} GB free on {path}"
    with open("/proc/meminfo") as f:
        mem = gb(1024 * int(next(ln for ln in f if ln.startswith("MemAvailable")).split()[1]))
    if mem < float(os.environ.get("PANDATEST_MIN_MEM_GB", 8)):
        return f"{mem:.1f} GB MemAvailable"
    for lock in (st.engine.lock, os.environ.get("PANDATEST_BUILD_LOCK")):
        if lock and os.path.exists(lock):
            if lock != st.engine.lock:
                return f"build lock {lock}"
            with open(lock) as f:
                try:
                    fcntl.flock(f, fcntl.LOCK_SH | fcntl.LOCK_NB)
                except BlockingIOError:
                    return f"a build holds {lock}"
    for d in os.listdir("/proc"):
        try:
            comm = open(f"/proc/{d}/comm").read().strip() if d.isdigit() else ""
        except OSError:
            continue
        if comm in ("ninja", "meson", "samu"):
            return f"a build is running ({comm}, pid {d})"
    return None


def wait_for_guard(st, item):
    """None once the guard passes; its reason if it still fails at the deadline (--no-wait: now)."""
    reason = guard(st)
    if not reason or st.config.option.no_wait:
        return reason
    wait = float(os.environ.get("PANDATEST_GUARD_WAIT", 1800))
    deadline = time.monotonic() + wait
    sys.__stderr__.write(f"\npandatest: {item.id}: {reason}; waiting up to {wait:.0f}s\n")
    while time.monotonic() < deadline:
        time.sleep(min(30, max(1, deadline - time.monotonic())))
        reason = guard(st)
        if not reason:
            return None
    return f"{reason} (still, after waiting {wait:.0f}s)"


def build_fixture(st, item):
    fx = item.fixture
    os.makedirs(os.path.join(st.out, "fixtures"), exist_ok=True)
    log = os.path.join(st.out, "fixtures", fx.name + ".log")
    for argv in fx.builds:
        with open(log, "a") as f:
            f.write("# " + " ".join(argv) + "\n")
            f.flush()
            pre = ["taskset", "-c", st.cpus] if st.cpus else []
            rc = subprocess.run(pre + argv, env=dict(os.environ, **st.engine.env(), **fx.env),
                                stdout=f, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL).returncode
        if rc:
            raise PandaFailure(f"building fixture {fx.name} failed (rc={rc}), log: {log}\n" + tail(log))
    st.fixtures.clear()
    item.fixture = st.fixture(fx.name)
    if not item.fixture.ready:
        raise PandaFailure(f"fixture {fx.name} still missing after its build: {item.fixture.reason}")


def glob(pattern, s):
    """* and ? as in a shell; brackets are literal (item ids have them)."""
    rx = re.escape(pattern).replace(r"\*", ".*").replace(r"\?", ".")
    return re.fullmatch(rx, s) is not None


def selected(item, terms, arches, tiers):
    """Why the item is selected, or None."""
    if item.error:
        return "its PANDA_TEST is invalid"
    if tiers and item.tier not in tiers:
        return None
    if arches and item.arch and item.arch not in arches and item.panda_arch not in arches:
        return None
    if not terms:
        return "everything"
    stem = os.path.basename(item.rel)
    for t in terms:
        if t == item.area:
            return f"area {t}"
        if t in (stem, item.rel, item.id) or glob(t, item.id) or glob(t, stem):
            return f"name {t}"
    return None


def pytest_collection_modifyitems(session, config, items):
    st, o = config._panda, config.option
    split = lambda s: [x for x in s.split(",") if x]  # noqa: E731
    arches = [meta.canonical(a) for a in split(o.arch)]
    keep, drop = [], []
    for it in items:
        why = selected(it, o.panda_select, arches, split(o.tier)) if isinstance(it, PandaItem) else None
        (keep if why else drop).append(it)
        if why:
            it.why = why
    if drop:
        config.hook.pytest_deselected(items=drop)
    keep.sort(key=lambda i: (ORDER.get(i.tier, len(ORDER)), i.rel, i.name))
    for it in keep:
        it.prepare(st)
    items[:] = keep


def pytest_runtestloop(session):
    st, o = session.config._panda, session.config.option
    if o.list or o.explain:
        for it in session.items:
            line = f"{it.id:52} {it.tier or '-':8} {it.fixture.name if it.fixture else '-':28}"
            line += " " + ("skip: " + it.skip_reason if it.skip_reason else "error" if it.error else "runs")
            if o.explain:
                line += f"\n    selected: {it.why}"
                line += f"\n    fixture: {it.fixture.name} {it.fixture.identity} {it.fixture.path or ''}" \
                    if it.fixture and it.fixture.kind != "none" else ""
                line += f"\n    xfail: {it.xfail['reason']}" if it.xfail else ""
                if not it.skip_reason and it.meta:
                    argv, env = it.command(st, work="<work>")
                    line += "\n    run: " + " ".join([f"{k}={v}" for k, v in sorted(env.items())] + argv)
            print(line)
        print(f"{len(session.items)} items; engine {st.engine.identity()}")
        return True
    os.makedirs(st.out, exist_ok=True)
    par = [i for i in session.items if i.tier in PARALLEL and not i.skip_reason and not i.error]
    if par:
        st.pool = concurrent.futures.ThreadPoolExecutor(max(1, o.jobs))
        for it in par:
            it.future = st.pool.submit(it.execute, st)
    return None


def pytest_runtest_logreport(report):
    item = getattr(report, "_panda_item", None)
    if item is None or not (report.when == "call" or (report.when == "setup" and not report.passed)):
        return
    st = item.config._panda
    reason = ""
    if hasattr(report, "wasxfail"):
        outcome, reason = ("xfailed", report.wasxfail) if report.skipped else ("xpassed", report.wasxfail)
    elif report.skipped:
        outcome = "skipped"
        reason = report.longrepr[2] if isinstance(report.longrepr, tuple) else str(report.longrepr)
        reason = reason.removeprefix("Skipped: ")
    elif report.failed:
        text = report.longreprtext
        outcome = "xpassed" if "XPASS(strict)" in text else ("error" if report.when == "setup" else "failed")
        reason = text.splitlines()[0] if text else ""
    else:
        outcome = "passed"
    rc, secs, log = item.result or (None, report.duration, None)
    st.records.append(dict(
        id=item.id, nodeid=item.nodeid, file=item.rel, area=item.area, tier=item.tier,
        arch=item.arch, params={k: meta.fmt(v) for k, v in item.params.items()},
        hash=meta.item_hash(str(item.path), item.meta, item.arch, item.params) if item.meta else None,
        outcome=outcome, reason=reason, duration=round(secs, 2), rc=rc, log=log,
        fixture=item.fixture.record() if item.fixture else None, selected=item.why,
        engine=dict(rev=st.engine.rev, dirty=st.engine.dirty)))


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    rep = outcome.get_result()
    if isinstance(item, PandaItem):
        rep._panda_item = item
        if item.fixture and call.when == "call":
            item.user_properties.append(("fixture", f"{item.fixture.name} {item.fixture.identity}"))


def pytest_sessionfinish(session):
    st, o = session.config._panda, session.config.option
    if st.pool:
        st.pool.shutdown(wait=True, cancel_futures=True)
    if o.list or o.explain or not st.records:
        return
    counts = {}
    for r in st.records:
        counts[r["outcome"]] = counts.get(r["outcome"], 0) + 1
    with open(os.path.join(st.out, "results.json"), "w") as f:
        json.dump(dict(engine=st.engine.identity(), cpus=st.cpus, counts=counts,
                       items=st.records), f, indent=1)
    summary = ", ".join(f"{n} {k}" for k, n in sorted(counts.items()))
    print(f"\npandatest: {summary}; results in {st.out}/results.json")
