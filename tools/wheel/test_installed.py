#!/usr/bin/env python3
"""Stdlib-only installed-check supervisor regressions; no package/container needed."""
import ast
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("installed", HERE / "installed.py")
installed = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installed)


class InstalledTests(unittest.TestCase):
    def test_missing_interpreter_accounts_all_checks(self):
        with tempfile.TemporaryDirectory(prefix="installed-selftest-") as temp:
            root = Path(temp)
            tests = root / "source" / "tests"
            tests.mkdir(parents=True)
            out = root / "evidence"
            with patch.object(sys, "argv", [str(HERE / "installed.py"), "wheel", str(tests), str(out)]), \
                 patch.object(sys, "executable", "/nonexistent/installed-selftest-python"), \
                 patch.dict(os.environ, {}, clear=True):
                self.assertEqual(installed.main(), 1)
            records = json.loads((out / "results.json").read_text())["results"]
            self.assertEqual(len(records), 6)
            self.assertTrue(all(r["status"] == "ERROR" for r in records))
            self.assertTrue(all("FileNotFoundError" in r["reason"] for r in records))
            self.assertTrue(all((out / (r["name"] + ".log")).is_file() for r in records))

    def test_timeout_kills_descendant_after_leader_exits(self):
        with tempfile.TemporaryDirectory(prefix="installed-selftest-") as temp:
            root = Path(temp)
            ready = root / "ready"
            # Child ignores TERM while leader has its default TERM behavior.
            child_code = "import signal,time; signal.signal(signal.SIGTERM,signal.SIG_IGN); time.sleep(30)"
            leader_code = ("import subprocess,sys,time,pathlib; "
                           "p=subprocess.Popen([sys.executable,'-c',sys.argv[2]]); "
                           "pathlib.Path(sys.argv[1]).write_text(str(p.pid)); time.sleep(30)")
            real_popen = subprocess.Popen
            leaders = []
            def started(*args, **kwargs):
                process = real_popen(*args, **kwargs)
                leaders.append(process)
                return process
            try:
                with patch.object(installed.subprocess, "Popen", side_effect=started):
                    result = installed.run_check([sys.executable, "-c", leader_code, str(ready), child_code],
                                                 root / "test.log", dict(os.environ), 0.8, grace=0.2)
                self.assertEqual(result["status"], "TIMEOUT")
                self.assertLess(result["seconds"], 5)
                self.assertTrue(ready.exists())
                pid = int(ready.read_text())
                deadline = time.monotonic() + 3
                while time.monotonic() < deadline:
                    status = Path("/proc/{}/status".format(pid))
                    if not status.exists() or "Z (zombie)" in status.read_text():
                        break
                    time.sleep(0.02)
                else:
                    self.fail("TERM-ignoring descendant survived timeout cleanup")
            finally:
                for leader in leaders:
                    installed.kill_group(leader, signal.SIGKILL)
                    leader.wait()

    def test_guests_use_installed_interpreter_for_runner(self):
        with tempfile.TemporaryDirectory(prefix="installed-runner-") as temp:
            root = Path(temp)
            tests = root / "source" / "tests"
            tests.mkdir(parents=True)
            out = root / "evidence"
            with patch.object(sys, "argv", [str(HERE / "installed.py"), "deb", str(tests), str(out)]), \
                 patch.object(sys, "executable", "/artifact/bin/python"), \
                 patch.dict(os.environ, {"PANDA_INSTALLED_IMAGES": "/fixtures/guest.json"}, clear=True), \
                 patch.object(installed, "run_check", return_value={"status": "PASS", "returncode": 0}):
                self.assertEqual(installed.main(), 0)
            records = json.loads((out / "results.json").read_text())["results"]
            guest = next(r for r in records if r["name"] == "guests")
            self.assertEqual(guest["argv"], ["/artifact/bin/python", str(tests / "run_results.py"), "/fixtures/guest.json"])

    def test_missing_pr2_runner_fails_selected_guests(self):
        with tempfile.TemporaryDirectory(prefix="installed-no-runner-") as temp:
            root = Path(temp)
            tests = root / "source" / "tests"
            tests.mkdir(parents=True)
            out = root / "evidence"
            run_check = installed.run_check

            def check(argv, *args):
                if argv[1] == str(tests / "run_results.py"):
                    return run_check(argv, *args)
                return {"status": "PASS", "returncode": 0}

            with patch.object(sys, "argv", [str(HERE / "installed.py"), "wheel", str(tests), str(out)]), \
                 patch.dict(os.environ, {"PANDA_INSTALLED_IMAGES": "/fixtures/guest.json"}, clear=True), \
                 patch.object(installed, "run_check", side_effect=check):
                self.assertEqual(installed.main(), 1)
            evidence = json.loads((out / "results.json").read_text())
            self.assertEqual(evidence["guest_coverage"], "selected")
            guest = next(r for r in evidence["results"] if r["name"] == "guests")
            self.assertEqual(guest["status"], "FAIL")
            self.assertNotEqual(guest["returncode"], 0)
            self.assertIn("run_results.py", (out / "guests.log").read_text())

    def test_python38_syntax(self):
        ast.parse((HERE / "installed.py").read_text(), feature_version=(3, 8))

    def test_shell_early_evidence_and_trap(self):
        wheel = (HERE / "smoke.sh").read_text()
        self.assertLess(wheel.index('mkdir -p "$EVIDENCE"'), wheel.index("apt-get update"))
        self.assertLess(wheel.index('exec > >(tee "$EVIDENCE/smoke.log")'), wheel.index("apt-get update"))
        self.assertIn('sha256sum "$WHL" > "$EVIDENCE/artifact.sha256"', wheel)
        self.assertIn('> "$EVIDENCE/smoke-$a.log"', wheel)
        self.assertNotIn('/tmp/smoke-$a.log', wheel)
        self.assertIn('"$EVIDENCE/installed"', wheel)
        deb = (HERE.parent / "deb" / "smoke.sh").read_text()
        self.assertLess(deb.index("trap cleanup EXIT"), deb.index('dpkg -i "$DEB"'))
        self.assertIn("dpkg-query -W", deb)
        self.assertIn('[ "$rc" != 0 ] || rc=1', deb)
        cleanup = deb[deb.index("cleanup() {"):deb.index("trap cleanup EXIT")]
        self.assertLess(cleanup.index("hash -r"), cleanup.index("command -v panda-python"))
        subprocess.run(["bash", "-n", str(HERE / "smoke.sh"),
                        str(HERE.parent / "deb" / "smoke.sh")], check=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
