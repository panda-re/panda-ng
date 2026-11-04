#!/usr/bin/env python3
"""The `syscalls` plugin's syscalls2 callback declarations equal syscalls2's. No engine, no guest.

    python3 tests/static/syscalls_compat.py
    (nix build .#checks.x86_64-linux.syscalls-compat-test)

plugins/syscalls/compat/gen_compat.py writes, from the syscalls tables, the callback typedefs
(syscalls_ext_typedefs_<arch>.h) and the PPP extern, boilerplate and registration lists
(syscall_ppp_{extern,boilerplate,register}_{enter,return}) that syscalls2 generates. Regenerated
into a temporary directory, they must equal the committed plugins/syscalls/compat/generated, and
each must equal syscalls2's plugins/syscalls2/generated file of the same name, byte for byte: the
same callbacks, names, parameter types and names, and target conditions.

Needs jinja2 (gen_tables.py imports syscalls2's generator).
"""
import filecmp
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / "plugins/syscalls/compat/gen_compat.py"
COMMITTED = ROOT / "plugins/syscalls/compat/generated"
S2 = ROOT / "plugins/syscalls2/generated"


class Compat(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.tmp.name)
        subprocess.run([sys.executable, str(GEN), "--outdir", str(cls.out)], check=True,
                       stdout=subprocess.DEVNULL)
        cls.names = sorted(os.listdir(cls.out))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_files(self):
        """the generator writes one file per syscalls2 arch and the six PPP lists"""
        self.assertEqual(sum(n.startswith("syscalls_ext_typedefs_") for n in self.names), 10)
        self.assertEqual(sum(n.startswith("syscall_ppp_") for n in self.names), 6)

    def test_committed(self):
        """the committed files are what the generator writes"""
        self.assertEqual(sorted(os.listdir(COMMITTED)), self.names)
        for n in self.names:
            with self.subTest(n):
                self.assertTrue(filecmp.cmp(self.out / n, COMMITTED / n, shallow=False),
                                f"plugins/syscalls/compat/generated/{n} is stale: run gen_compat.py")

    def test_syscalls2(self):
        """every file equals syscalls2's"""
        for n in self.names:
            with self.subTest(n):
                self.assertTrue(filecmp.cmp(self.out / n, S2 / n, shallow=False),
                                f"{n} differs from plugins/syscalls2/generated/{n}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
