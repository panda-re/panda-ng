#!/usr/bin/env python3
"""Offline runner regressions. No pandare2, guest, Nix, or third-party framework.

    python3 tests/run_selftests.py [--out NEW_DIRECTORY]

--out retains all synthetic outcomes, including the intentionally failing cases.
"""
import argparse
import ast
import importlib.util
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

HERE = Path(__file__).resolve().parent
BASE = None
FAKE = r'''#!/usr/bin/env python3
import json, os, signal, sys, time
from pathlib import Path
if sys.argv[1] == "-c":
    print("synthetic pandare2 (no package loaded)")
    sys.exit(int(os.environ.get("FAKE_PREFLIGHT_RC", "0")))
name = Path(sys.argv[1]).stem
image = Path(sys.argv[2])
data = json.loads(image.read_text())
assert len(os.environ["TMPDIR"]) < 30
qcow = image.parent / data["qcow"]
assert qcow.read_bytes() == b"original disk"
assert not qcow.is_symlink()
assert not image.is_symlink()
qcow.write_bytes(b"private write")
image.write_text("private json write")
if name == "assertion":
    assert False, "synthetic assertion"
if name == "crash":
    os.kill(os.getpid(), signal.SIGSEGV)
if name == "sigkill":
    os.kill(os.getpid(), signal.SIGKILL)
if name == "exit137":
    sys.exit(137)
if name == "exit124":
    sys.exit(124)
if name == "skip":
    print("SKIP: synthetic unsupported target")
if name == "skip77":
    sys.exit(77)
if name == "partialskip":
    print("GUARD notcg: SKIP (no /dev/kvm)")
    print("RESULT PASS (other checks)")
if name == "noisyskip":
    print("a" * 600000)
    print("SKIP: must survive log truncation")
    print("b" * 600000)
if name == "provenance":
    import ctypes
    loaded_library = ctypes.CDLL(os.environ["SYNTHETIC_LIBRARY"])
    time.sleep(0.5)
if name == "retry":
    sys.exit(0 if "attempt-002" in str(image) else 1)
if name == "worker":
    os.kill(os.getppid(), signal.SIGKILL)
    time.sleep(20)
if name in ("timeout", "stubborn", "cancel"):
    if name == "stubborn":
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
    print("ready", flush=True)
    time.sleep(20)
if name == "flood":
    print("x" * 3000000)
print("synthetic complete")
'''


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.root = BASE / self._testMethodName
        self.root.mkdir()
        self.here = self.root / "tests"
        self.here.mkdir()
        for name in ("run.sh", "run_results.py", "testlib.py", "inventory.py"):
            shutil.copy2(HERE / name, self.here / name)
        self.python = self.root / "fake-python"
        # Use the invoking interpreter, not PATH's potentially ABI-incompatible Python.
        # The provenance case copies a library mapped by this interpreter.
        self.python.write_text(FAKE.replace("#!/usr/bin/env python3", "#!" + sys.executable, 1))
        self.python.chmod(0o755)
        self.image = self.fixture("a")
        self.env = dict(os.environ, PANDA_PYTHON=str(self.python), OUT=str(self.root / "out"),
                        TIMEOUT="2", KILL_GRACE="0.1", JOBS="2", RETRIES="0", KEEP="0")
        self.env.pop("FAKE_PREFLIGHT_RC", None)
        self.env.pop("TIER", None)
        self.processes = []

    def tearDown(self):
        for proc in self.processes:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
        self.assertEqual((self.image.parent / "same.qcow2").read_bytes(), b"original disk")
        self.assertEqual(json.loads(self.image.read_text())["arch"], "x86_64")

    def fixture(self, directory):
        d = self.root / directory
        d.mkdir()
        (d / "same.qcow2").write_bytes(b"original disk")
        p = d / "same.json"
        p.write_text(json.dumps({"arch": "x86_64", "qcow": "same.qcow2"}))
        return p

    def start(self, tests, images=None, **env):
        for test in (tests or '').split():
            if test != "missing_script":
                (self.here / (test + ".py")).write_text("# synthetic source identity\n")
        self.env.update(env)
        if tests is None:
            self.env.pop('TESTS', None)
        else:
            self.env['TESTS'] = tests
        log = open(self.root / "runner.log", "wb")
        self.addCleanup(log.close)
        p = subprocess.Popen(["bash", str(self.here / "run.sh"), *map(str, images or [self.image])],
                             env=self.env, stdout=log, stderr=subprocess.STDOUT)
        self.processes.append(p)
        return p

    def results(self, proc, count):
        rc = proc.wait(timeout=20)
        out = Path(self.env["OUT"])
        records = json.loads((out / "results.json").read_text())["cells"]
        plan = json.loads((out / "plan.json").read_text())
        self.assertEqual(len(plan["cells"]), count)
        self.assertEqual(len(records), count)
        self.assertEqual(len((out / "results").read_text().splitlines()), count)
        self.assertTrue(all(c["result"]["status"] not in ("RUNNING", "PLANNED") for c in records))
        completion = json.loads((out / "completion.json").read_text())
        self.assertEqual(completion["fixture_changes"], [])
        return rc, records

    def test_verdicts(self):
        names = "pass assertion skip skip77 partialskip crash sigkill exit137 exit124 timeout stubborn"
        rc, records = self.results(self.start(names, TIMEOUT="0.5"), 11)
        self.assertEqual(rc, 1)
        statuses = {c["test"]: c["result"] for c in records}
        expected = dict(pass_="PASS", assertion="FAIL", skip="SKIP", skip77="SKIP", partialskip="SKIP",
                        crash="CRASH", sigkill="CRASH", exit137="FAIL", exit124="FAIL", timeout="TIMEOUT", stubborn="TIMEOUT")
        expected["pass"] = expected.pop("pass_")
        self.assertEqual({k: v["status"] for k, v in statuses.items()}, expected)
        self.assertEqual(statuses["sigkill"]["signal"], signal.SIGKILL)
        self.assertFalse(statuses["sigkill"]["watchdog_fired"])
        self.assertTrue(statuses["stubborn"]["watchdog_fired"])
        self.assertEqual(statuses["stubborn"]["signal"], signal.SIGKILL)
        for c in records:
            r = c["result"]
            self.assertIn("reproduce", r)
            self.assertIn("argv", r)
            self.assertIn("test_source", r)
        provenance = json.loads((Path(self.env["OUT"]) / "provenance.json").read_text())
        self.assertIn("selection paths are not evidence", provenance["note"])
        self.assertTrue(statuses["timeout"]["process_evidence"])

    def test_duplicate_basenames_and_tests(self):
        other = self.fixture("b")
        rc, records = self.results(self.start("pass pass", [self.image, other]), 4)
        self.assertEqual(rc, 0)
        self.assertEqual(len({c["id"] for c in records}), 4)
        self.assertEqual((other.parent / "same.qcow2").read_bytes(), b"original disk")
        self.assertTrue(all(c["result"]["status"] == "PASS" for c in records))

    def test_reuse_refused(self):
        out = Path(self.env["OUT"])
        out.mkdir()
        sentinel = out / "sentinel"
        sentinel.write_text("do not delete")
        self.assertEqual(self.start("pass").wait(timeout=10), 2)
        self.assertEqual(sentinel.read_text(), "do not delete")
        self.assertEqual(sorted(p.name for p in out.iterdir()), ["sentinel"])

    def test_missing_and_malformed_fixtures(self):
        missing = self.root / "missing.json"
        bad = self.root / "bad.json"
        bad.write_text("not json")
        absent_disk = self.root / "no_disk.json"
        absent_disk.write_text(json.dumps({"arch": "x86_64", "qcow": "absent.qcow2"}))
        rc, records = self.results(self.start("pass assertion", [missing, bad, absent_disk]), 6)
        self.assertEqual(rc, 1)
        self.assertTrue(all(c["result"]["status"] == "ERROR" for c in records))

    def test_missing_script(self):
        rc, records = self.results(self.start("missing_script pass"), 2)
        self.assertEqual(rc, 1)
        self.assertEqual([c["result"]["status"] for c in records], ["ERROR", "PASS"])

    def test_preflight_failure(self):
        rc, records = self.results(self.start("pass assertion", FAKE_PREFLIGHT_RC="1"), 2)
        self.assertEqual(rc, 1)
        self.assertTrue(all(c["result"]["status"] == "ERROR" for c in records))

    def test_worker_death(self):
        rc, records = self.results(self.start("worker pass", TIMEOUT="0.5"), 2)
        self.assertEqual(rc, 1)
        self.assertTrue(all(c["result"]["status"] == "ERROR" for c in records))
        r = records[0]["result"]
        if r.get("process_group"):
            # A killed child may briefly remain as a zombie until host init reaps it.
            status = Path(f"/proc/{r['process_group']}/status")
            if status.exists():
                self.assertIn("Z (zombie)", status.read_text())

    def test_cancellation(self):
        other = self.fixture("b")
        proc = self.start("cancel pass", [self.image, other], JOBS="1", TIMEOUT="10")
        out = Path(self.env["OUT"])
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            logs = list(out.glob("*/001-cancel/attempt-*/test.log"))
            if logs and b"ready" in logs[0].read_bytes():
                break
            if proc.poll() is not None:
                self.fail("runner exited before cancellation")
            time.sleep(0.02)
        else:
            self.fail("test did not start")
        proc.send_signal(signal.SIGTERM)
        rc, records = self.results(proc, 4)
        self.assertEqual(rc, 143)
        self.assertTrue(all(c["result"]["status"] == "CANCELLED" for c in records))

    def test_retries_keep_first_failure(self):
        rc, records = self.results(self.start("retry", RETRIES="1"), 1)
        self.assertEqual(rc, 1)
        self.assertEqual(records[0]["result"]["status"], "FAIL")
        d = Path(self.env["OUT"]) / records[0]["id"]
        first = json.loads((d / "attempt-001/result.json").read_text())
        second = json.loads((d / "attempt-002/result.json").read_text())
        self.assertEqual([first["status"], second["status"]], ["FAIL", "PASS"])
        self.assertNotEqual(first["fixture_json"], second["fixture_json"])
        self.assertTrue((d / "attempt-001/img").is_dir())
        self.assertFalse((d / "attempt-002/img").exists())

    def test_bounded_logs_and_skip(self):
        rc, records = self.results(self.start("flood noisyskip"), 2)
        self.assertEqual(rc, 0)
        self.assertEqual([c["result"]["status"] for c in records], ["PASS", "SKIP"])
        for c in records:
            self.assertTrue(c["result"]["log_truncated"])
            log = Path(self.env["OUT"]) / c["id"] / "attempt-001/test.log"
            self.assertLessEqual(log.stat().st_size, 1024 * 1024 + 100)

    def test_source_and_actual_loaded_identity(self):
        # Copy an already loaded host library, no compiler/build or guest required.
        paths = [line.split(None, 5)[5].strip() for line in Path('/proc/self/maps').read_text().splitlines()
                 if len(line.split(None, 5)) == 6 and '/libc.so' in line]
        self.assertTrue(paths, 'Linux /proc libc mapping required for runner provenance selftest')
        library = self.root / 'panda-synthetic.so'
        shutil.copyfile(paths[0], library)
        selection = self.root / 'NOT-loaded-selection.so'
        rc, records = self.results(self.start('provenance', SYNTHETIC_LIBRARY=str(library),
                                             PANDA_LIB=str(selection)), 1)
        self.assertEqual(rc, 0)
        r = records[0]['result']
        observed = {entry['path']: entry for entry in r['loaded_paths']}
        self.assertIn(str(library), observed)
        self.assertEqual(observed[str(library)]['resolved_path'], str(library.resolve()))
        self.assertEqual(observed[str(library)]['sha256'], hashlib.sha256(library.read_bytes()).hexdigest())
        self.assertNotIn(str(selection), observed)
        self.assertEqual(r['environment']['PANDA_LIB'], str(selection))
        self.assertEqual(r['test_source']['sha256'], hashlib.sha256((self.here / 'provenance.py').read_bytes()).hexdigest())
        self.assertIn('PANDA_LIB=', r['reproduce'])
        self.assertIn('env -u OUT', r['reproduce'])
        # This fixture is outside git; real source_identity is tested against this checkout below.
        sys.path.insert(0, str(HERE))
        self.addCleanup(sys.path.remove, str(HERE))
        spec = importlib.util.spec_from_file_location('run_results', HERE / 'run_results.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        source = module.source_identity()
        revision = subprocess.run(['git', '-C', str(HERE.parent), 'rev-parse', 'HEAD'], text=True,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if revision.returncode == 0:
            self.assertEqual(source['revision'], revision.stdout.strip())
            self.assertEqual(len(source['patch_tree_sha256']), 64)
            self.assertEqual(source['runner'][1]['sha256'], hashlib.sha256((HERE / 'run_results.py').read_bytes()).hexdigest())
        else:
            # Nix source snapshots omit .git; missing source identity must be explicit.
            self.assertIn('error', source)

    def test_inventory_requirements(self):
        sys.path.insert(0, str(HERE))
        self.addCleanup(sys.path.remove, str(HERE))
        import inventory
        focused = inventory.select('focused-guest')
        for row in inventory.INVENTORY:
            self.assertTrue((HERE / (row['script'] + '.py')).is_file(), row['script'])
        self.assertIn('rr_blocks', [r['name'] for r in focused])
        self.assertNotIn('fastrestore_vsock', [r['name'] for r in focused])
        self.assertTrue(set(r['name'] for r in focused) <= set(r['name'] for r in inventory.select('broad')))
        self.assertEqual(len({r['name'] for r in inventory.INVENTORY}), len(inventory.INVENTORY))
        (self.here / 'fastrestore_vsock.py').write_text('# fixture source')
        rc, records = self.results(self.start('fastrestore_vsock', VHOST_DEVICE_VSOCK='/nonexistent/vsock'), 1)
        self.assertEqual(rc, 0)
        self.assertEqual(records[0]['result']['status'], 'SKIP')
        self.assertIn('requires executable', records[0]['result']['reason'])
        self.assertNotIn('argv', records[0]['result'])
        self.assertEqual(records[0]['requirements']['modes'], ['tcg', 'restore', 'vhost-user'])
        self.assertIn('unknown', inventory.select('quick', 'unlisted')[0]['coverage'])
        with self.assertRaises(ValueError):
            inventory.select('nonexistent')

    def test_inventory_target_skip(self):
        other = self.fixture('other-target')
        data = json.loads(other.read_text())
        data['arch'] = 'aarch64'
        other.write_text(json.dumps(data))
        rc, records = self.results(self.start('kpti', [other]), 1)
        self.assertEqual(rc, 0)
        self.assertEqual(records[0]['result']['status'], 'SKIP')
        self.assertIn('target aarch64', records[0]['result']['reason'])
        self.assertFalse(list((Path(self.env['OUT']) / records[0]['id']).glob('attempt-*/img')))

    def test_inventory_alias_arguments(self):
        (self.here / 'fastrestore_guards.py').write_text('# synthetic alias')
        rc, records = self.results(self.start('fastrestore_guards_rr'), 1)
        self.assertEqual(rc, 0)
        result = records[0]['result']
        self.assertEqual(Path(result['argv'][1]).name, 'fastrestore_guards.py')
        self.assertEqual(result['argv'][-1], 'rr')
        self.assertEqual(records[0]['requirements']['modes'], ['record', 'replay', 'restore-rejection'])

    def test_empty_explicit_selection_uses_tier(self):
        for name in ('guest', 'api'):
            (self.here / (name + '.py')).write_text('# synthetic tier')
        rc, records = self.results(self.start('', TIER='quick'), 2)
        self.assertEqual(rc, 0)
        self.assertEqual([c['test'] for c in records], ['guest', 'api'])
        plan = json.loads((Path(self.env['OUT']) / 'plan.json').read_text())
        self.assertEqual(plan['tier'], 'quick')

    def test_legacy_default_unchanged(self):
        # Independent literal locks the original run.sh default and order, not just its count.
        expected = 'guest api osi_isf blocks block_activation callbacks calls mem mmio late reset teardown asidstory tbpc hookalias rr rrcb rrstop rrload fuzz fastrestore fastcov syscalls'.split()
        for name in expected:
            (self.here / (name + '.py')).write_text('# synthetic legacy default')
        # Both absent TESTS and empty TESTS use legacy selection when TIER is absent.
        for index, explicit in enumerate((None, '')):
            self.env['OUT'] = str(self.root / f'legacy-{index}')
            proc = self.start(explicit)
            rc, records = self.results(proc, len(expected))
            self.assertEqual(rc, 0)
            self.assertEqual([c['test'] for c in records], expected)
            plan = json.loads((Path(self.env['OUT']) / 'plan.json').read_text())
            self.assertIsNone(plan['tier'])

    def test_docs_default_matches_inventory(self):
        # Exercise the real generator function without importing its full docs dependencies.
        root = HERE.parent
        source = ast.parse((root / 'tools/docs/gen.py').read_text())
        functions = [node for node in source.body
                     if isinstance(node, ast.FunctionDef) and node.name == 'default_tests']
        self.assertEqual(len(functions), 1)
        def err(message):
            self.fail(message)
        namespace = {'ast': ast, 'ROOT': root, 'err': err}
        module = ast.Module(body=functions, type_ignores=[])
        exec(compile(module, str(root / 'tools/docs/gen.py'), 'exec'), namespace)
        inventory_tree = ast.parse((HERE / 'inventory.py').read_text())
        defaults = [ast.literal_eval(node.value) for node in inventory_tree.body
                    if isinstance(node, ast.Assign) and
                    any(isinstance(target, ast.Name) and target.id == 'LEGACY_DEFAULT'
                        for target in node.targets)]
        self.assertEqual(len(defaults), 1)
        self.assertEqual(namespace['default_tests'](), defaults[0].split())

    def test_checks_strict_xpass(self):
        spec = importlib.util.spec_from_file_location("testlib", HERE / "testlib.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        checks = module.Checks("x86_64", {"known": "known gap"})
        checks.check("known", False)
        self.assertEqual(checks.report(), 0)
        checks.check("known", True)
        self.assertEqual(checks.report(), 1)
        checks = module.Checks("x86_64")
        checks.check("assertion", False)
        self.assertEqual(checks.report(), 1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    if args.out:
        args.out.mkdir()
        BASE = args.out.resolve()
    else:
        BASE = Path(tempfile.mkdtemp(prefix="panda-run-selftests-"))
    print(f"selftest evidence: {BASE}", flush=True)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(RunnerTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.wasSuccessful() and not args.out:
        shutil.rmtree(BASE)
    sys.exit(0 if result.wasSuccessful() else 1)
