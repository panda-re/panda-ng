#!/usr/bin/env python3
"""Offline protocol tests; no QEMU, builds, timing benchmarks or guest fixtures.
Run: python3 -I tests/perf/test_perf.py
"""
import copy
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import analyze
import run
spec = importlib.util.spec_from_file_location("bench_cb", HERE.parent / "bench_cb.py")
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)

GOOD = "BENCH sort_ms=10 copy_mbs=20\nCHECK cpu PASS\nEXIT iw-cpu:1 0\nALLDONE\nRC 0 WALL 2\n"


def parse(text):
    return analyze.parse(text, ["sort_ms", "copy_mbs"], ["iw-cpu:1"], ["CHECK cpu PASS"])


class ParserTests(unittest.TestCase):
    def test_valid(self):
        self.assertEqual(parse(GOOD), {"wall": 2, "sort_ms": 10, "copy_mbs": 20})
        self.assertEqual(parse(GOOD.replace("\n", "\r\n"))["wall"], 2)

    def test_strict_rejections(self):
        bad = [GOOD.replace("RC 0", "RC 1"), GOOD.replace("RC 0", "RC -9"),
               GOOD.replace("RC 0 WALL 2\n", ""), GOOD + "RC 0 WALL 1\n",
               GOOD.replace(" WALL 2", ""), GOOD.replace("WALL 2", "WALL nan"),
               GOOD.replace("sort_ms=10", ""), GOOD.replace("sort_ms=10", "sort_ms=10 sort_ms=11"),
               GOOD.replace("ALLDONE\n", ""), GOOD.replace("ALLDONE", "ALLDONE\nALLDONE"),
               GOOD.replace("ALLDONE", "not ALLDONE"), GOOD.replace("EXIT iw-cpu:1 0", "EXIT iw-cpu:1 1"),
               GOOD.replace("EXIT iw-cpu:1 0\n", ""), GOOD.replace("EXIT iw-cpu:1 0", "EXIT wrong 0"),
               GOOD.replace("CHECK cpu PASS", "CHECK cpu FAIL"), GOOD.replace("sort_ms=10", "sort_ms="),
               GOOD.replace("ALLDONE", "ALLDONE\nEXIT iw-cpu:1 0"), GOOD + "junk\n"]
        bad += [GOOD.replace("sort_ms=10", f"sort_ms={v}") for v in ("nan", "NaN", "inf", "-inf", "1e999", "0", "-1", "no")]
        for text in bad:
            with self.subTest(text=text), self.assertRaises(analyze.Invalid):
                parse(text)

    def test_bootstrap_endpoints(self):
        self.assertEqual(analyze.bootstrap([1, 2, 3]), [1, 3])
        self.assertEqual(analyze.bootstrap([1, 2]), None)
        self.assertEqual(analyze.bootstrap([.9, 1, 1.1, 1.2]), [.9, 1.2])

    def test_duplicate_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "bad.json"
            p.write_text('{"a": 1, "a": 2}')
            with self.assertRaises(analyze.Invalid):
                analyze.load(p)


class ExperimentTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.c = {"variants": {"vanilla": {"kind": "vanilla"},
                               "shared": {"kind": "shared", "engine": "/pinned/libpanda-x86_64-softmmu.so"},
                               "shared-aa": {"kind": "shared", "engine": "/pinned/libpanda-x86_64-softmmu.so"}},
                  "reference": "vanilla", "aa": ["shared", "shared-aa"], "repeats": 3,
                  "metrics": ["sort_ms", "copy_mbs"], "higher_better": ["copy_mbs"],
                  "exits": ["iw-cpu:1"], "correctness": ["CHECK cpu PASS"]}
        config_path = self.root / "config.json"
        config_path.write_text(json.dumps(self.c))
        artifact = self.root / "engine.so"
        artifact.write_text("synthetic pinned engine")
        self.m = {"schema": 1, "state": "complete", "config": self.c, "contamination": "", "runs": [],
                  "config_path": str(config_path), "config_sha256": analyze.digest(config_path),
                  "artifacts": [{"path": str(artifact), "resolved": str(artifact.resolve()),
                                 "sha256": analyze.digest(artifact)}]}
        for phase, count in (("preflight", 1), ("sample", 3)):
            for rep in range(count):
                for name in analyze.order(list(self.c["variants"]), rep):
                    log = f"{phase}-{rep}-{name}.con"
                    (self.root / log).write_text(GOOD)
                    cell = {"phase": phase, "rep": rep, "variant": name, "log": log,
                            "sha256": analyze.digest(self.root / log), "rc": 0, "timeout": False}
                    if name != "vanilla":
                        maps = log + ".maps"
                        (self.root / maps).write_text("7f-8f r-xp 0 00:00 1 /pinned/libpanda-x86_64-softmmu.so\n")
                        cell.update(maps=maps, maps_sha256=analyze.digest(self.root / maps))
                    self.m["runs"].append(cell)
        self.save()

    def save(self):
        (self.root / "manifest.json").write_text(json.dumps(self.m))

    def test_complete_raw_pairs_and_rotation(self):
        report = analyze.analyze(self.root)
        self.assertEqual(len(report["raw_samples"]["shared"]), 3)
        self.assertTrue(all(r["paired_n"] == 3 and r["ci95"] == [1, 1] for r in report["rows"]))
        self.assertEqual(analyze.order(["a", "b", "c"], 1), ["b", "c", "a"])
        self.assertEqual(report, analyze.analyze(self.root))

    def test_incomplete_duplicate_reordered_and_contaminated(self):
        original = copy.deepcopy(self.m)
        for mutate in (lambda m: m["runs"].pop(),
                       lambda m: m["runs"].append(m["runs"][0]),
                       lambda m: m["runs"].reverse(),
                       lambda m: m.update(contamination="build overlapped"),
                       lambda m: m.update(state="running"),
                       lambda m: m["runs"][0].update(rc=9),
                       lambda m: m["runs"][0].update(timeout=True)):
            self.m = copy.deepcopy(original)
            mutate(self.m)
            self.save()
            with self.assertRaises(analyze.Invalid):
                analyze.analyze(self.root)

    def test_reanalysis_artifact_and_config_identity(self):
        for path in (self.root / "engine.so", self.root / "config.json"):
            original = path.read_bytes()
            path.write_bytes(original + b"changed")
            with self.assertRaises(analyze.Invalid):
                analyze.analyze(self.root)
            path.write_bytes(original)
        self.m["config"]["metrics"] = ["different_metric"]
        self.save()
        with self.assertRaisesRegex(analyze.Invalid, "configuration"):
            analyze.analyze(self.root)

    def test_log_and_maps_tamper(self):
        for key in ("log", "maps"):
            cell = self.m["runs"][1]
            path = self.root / cell[key]
            old = path.read_text()
            path.write_text(old + "tampered\n")
            with self.assertRaises(analyze.Invalid):
                analyze.analyze(self.root)
            path.write_text(old)

    def test_missing_engine_and_nonzero_rc_never_dropped(self):
        cell = self.m["runs"][1]
        path = self.root / cell["maps"]
        path.write_text("unrelated library\n")
        cell["maps_sha256"] = analyze.digest(path)
        self.save()
        with self.assertRaises(analyze.Invalid):
            analyze.analyze(self.root)
        cell = self.m["runs"][0]
        path = self.root / cell["log"]
        path.write_text(GOOD.replace("RC 0", "RC 7"))
        cell["sha256"] = analyze.digest(path)
        self.save()
        with self.assertRaises(analyze.Invalid):
            analyze.analyze(self.root)


class RunnerTests(unittest.TestCase):
    def test_fake_process_success_failure_crash_timeout(self):
        # Synthetic child commands exercise containment only, never timing workloads.
        with tempfile.TemporaryDirectory() as tmp:
            cpu = [min(os.sched_getaffinity(0))]
            for i, (code, expected, timeout) in enumerate([
                ("print('fake')", 0, False), ("raise SystemExit(7)", 7, False),
                ("import os,signal; os.kill(os.getpid(),signal.SIGKILL)", -9, False),
                ("import time; time.sleep(30)", -9, True)]):
                result, _ = run.run_process([sys.executable, "-I", "-c", code], os.environ.copy(),
                                             Path(tmp) / str(i), .15 if timeout else 5, cpu)
                self.assertEqual(result["rc"], expected)
                self.assertEqual(result["timeout"], timeout)

    def test_config_and_full_synthetic_protocol(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            exe = root / "qemu-system-x86_64"
            exe.symlink_to(sys.executable)
            paths = {}
            for name in ("kernel", "initrd", "panda-qemu.c", "build.txt", "libpanda-x86_64-softmmu.so"):
                paths[name] = root / name
                paths[name].write_text("synthetic input")
            shared = {"kind": "shared", "executable": str(exe), "engine": str(paths["libpanda-x86_64-softmmu.so"]),
                      "source_revision": "test-only", "patch_tree": "test-only", "build_class": "diagnostic",
                      "shim": "none", "qemu_version": "test-only", "toolchain": "test-only",
                      "build_records": [str(paths["build.txt"])]}
            vanilla = dict(shared, kind="vanilla")
            vanilla.pop("engine")
            c = {"schema": 1, "host_target": "linux-x86_64", "guest_target": "x86_64", "quiet_window": "synthetic only",
                 "repeats": 3, "timeout": 60, "cpus": [min(os.sched_getaffinity(0))],
                 "argv": ["-accel", "tcg", "-kernel", str(paths["kernel"]), "-initrd", str(paths["initrd"])],
                 "launcher_source": str(paths["panda-qemu.c"]), "inputs": [str(paths["kernel"]), str(paths["initrd"])],
                 "metrics": ["sort_ms", "copy_mbs"], "higher_better": ["copy_mbs"],
                 "exits": ["iw-cpu:1"], "correctness": ["CHECK cpu PASS"],
                 "variants": {"vanilla": vanilla, "shared": shared, "shared-aa": dict(shared)},
                 "reference": "vanilla", "aa": ["shared", "shared-aa"]}
            run.validate(c)
            for edit, message in ((lambda d: d["variants"]["shared-aa"].update(toolchain="other"), "toolchain"),
                                  (lambda d: d["variants"]["shared-aa"].update(source_revision="other"), "identical")):
                bad = copy.deepcopy(c)
                edit(bad)
                with self.assertRaisesRegex(analyze.Invalid, message):
                    run.validate(bad)
            config_path = root / "config.json"
            config_path.write_text(json.dumps(c))

            def fake(argv, env, path, timeout, cpus, engine):
                path.write_text(GOOD)
                return {"rc": 0, "timeout": False, "wall": 2}, (f"7f-8f r-xp 0 00:00 1 {engine}\n" if engine else "")

            with mock.patch.object(run, "run_process", side_effect=fake), mock.patch.dict(os.environ, {}, clear=True):
                run.execute(c, root / "out", config_path)
            self.assertEqual(analyze.load(root / "out/manifest.json")["state"], "complete")
            self.assertEqual(len(analyze.analyze(root / "out")["rows"]), 9)

            def fail(*args):
                result, maps = fake(*args)
                result["rc"] = 7
                return result, maps
            with mock.patch.object(run, "run_process", side_effect=fail), mock.patch.dict(os.environ, {}, clear=True):
                with self.assertRaises(analyze.Invalid):
                    run.execute(c, root / "failed", config_path)
            manifest = analyze.load(root / "failed/manifest.json")
            self.assertEqual(manifest["state"], "invalid")
            self.assertEqual(len(manifest["runs"]), 1)  # No timed samples after failed correctness preflight.

    def test_no_output_reuse(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(run, "validate", return_value=[]), \
                mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaises(FileExistsError):
                run.execute({}, tmp, "not-read")

    def test_early_config_rejections(self):
        c = {"schema": 1, "host_target": "linux-x86_64", "guest_target": "x86_64", "quiet_window": "reserved",
             "repeats": 3, "timeout": 60, "cpus": [min(os.sched_getaffinity(0))],
             "argv": ["-accel", "tcg", "-icount", "shift=1"]}
        with self.assertRaisesRegex(analyze.Invalid, "icount"):
            run.validate(c)
        for key, value in (("repeats", 0), ("repeats", True), ("timeout", float("inf")), ("timeout", 601)):
            with self.subTest(key=key, value=value), self.assertRaises(analyze.Invalid):
                run.validate(dict(c, **{key: value}))


class CallbackTests(unittest.TestCase):
    def test_positive_counts_and_all_repeat_accounting(self):
        for value in (0, -1):
            with self.assertRaises(ValueError):
                bench.positive_int(value)
            with self.assertRaises(ValueError):
                bench.sample_record("callback", 0, 1, value, True)
        samples = [bench.sample_record("callback", i, dt, 10, True) for i, dt in enumerate([2, 3, 9])]
        baseline = [bench.sample_record("baseline", i, 1, 0, False) for i in range(3)]
        result = bench.summarize(samples, baseline)
        self.assertEqual(result["paired_n"], 3)
        self.assertEqual(result["ns_per_call_all"], [1e8, 2e8, 8e8])
        self.assertEqual(result["median_ns_per_call"], 2e8)
        with self.assertRaises(ValueError):
            bench.summarize(samples[:-1], baseline)

    def test_private_fixture(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "source.qcow2").write_bytes(b"snapshot fixture")
            (root / "isf.json").write_text("{}")
            image = root / "image.json"
            image.write_text(json.dumps({"arch": "x86_64", "qcow": "source.qcow2", "isf": "isf.json"}))
            fake = mock.Mock(stdout='{"format":"qcow2"}')
            with mock.patch.object(bench.subprocess, "run", return_value=fake):
                with bench.private_image(image) as private:
                    data = json.loads(Path(private).read_text())
                    disk = Path(private).parent / data["qcow"]
                    self.assertEqual(disk.read_bytes(), b"snapshot fixture")
                    disk.write_bytes(b"changed")
                    self.assertEqual((root / "source.qcow2").read_bytes(), b"snapshot fixture")
                self.assertFalse(Path(private).exists())
            fake.stdout = '{"format":"qcow2", "backing-filename":"shared"}'
            with mock.patch.object(bench.subprocess, "run", return_value=fake), self.assertRaises(ValueError):
                with bench.private_image(image):
                    self.fail("shared backing chain accepted")


if __name__ == "__main__":
    unittest.main()
