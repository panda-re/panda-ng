"""Offline preflight regressions; no tarball, git commits, or build required."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("check_series", Path(__file__).with_name("check-series.py"))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class SeriesTests(unittest.TestCase):
    def check(self, rows, files=("0001-one.patch", "0002-two.patch")):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            version = root / "11.1.2"
            version.mkdir()
            for name in files:
                (version / name).touch()
            (version / "series").write_text(rows)
            return module.check(root, "11.1.2")

    def test_valid(self):
        self.assertEqual(self.check("# comment\n\n11.1.2/0001-one.patch\n11.1.2/0002-two.patch\n"), 2)

    def test_duplicate(self):
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.check("11.1.2/0001-one.patch\n11.1.2/0001-one.patch\n")

    def test_conflict_markers(self):
        with self.assertRaisesRegex(ValueError, "malformed"):
            self.check("<<<<<<< ours\n11.1.2/0001-one.patch\n=======\n>>>>>>> theirs\n")

    def test_gap(self):
        with self.assertRaisesRegex(ValueError, "expected patch"):
            self.check("11.1.2/0002-two.patch\n")

    def test_missing(self):
        with self.assertRaisesRegex(ValueError, "missing"):
            self.check("11.1.2/0001-absent.patch\n")

    def test_traversal(self):
        with self.assertRaisesRegex(ValueError, "malformed"):
            self.check("../11.1.2/0001-one.patch\n")

    def test_overlay_owned_patch(self):
        for marker in ("+++ b/", "--- a/"):
            with self.subTest(marker=marker), tempfile.TemporaryDirectory() as work:
                engine = Path(work)
                patches = engine / "patches"
                version = patches / "11.1.2"
                version.mkdir(parents=True)
                (engine / "src/system").mkdir(parents=True)
                (engine / "src/system/owned.c").touch()
                (version / "series").write_text("11.1.2/0001-one.patch\n")
                (version / "0001-one.patch").write_text(marker + "system/owned.c\n")
                with self.assertRaisesRegex(ValueError, "overlay-owned.*silently overwritten"):
                    module.check(patches, "11.1.2")

    def test_empty(self):
        with self.assertRaisesRegex(ValueError, "empty"):
            self.check("# no patches\n")


if __name__ == "__main__":
    unittest.main()
