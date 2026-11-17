"""Exercise Python capability selection without loading an engine."""
import ast
from pathlib import Path
import unittest

PANDA_TEST = dict(area="capabilities", tier="static", nix_check="capabilities")

ROOT = Path(__file__).resolve().parents[2]
tree = ast.parse((ROOT / 'python/core/pandare2/panda.py').read_text())
selected = [node for node in tree.body
            if isinstance(node, ast.FunctionDef) and node.name == '_startup_capabilities'
            or isinstance(node, ast.Assign) and any(isinstance(target, ast.Name) and
                target.id == 'CAPABILITIES' for target in node.targets)]
ns = {}
exec(compile(ast.Module(body=selected, type_ignores=[]), '<capabilities>', 'exec'), ns)
choose = ns['_startup_capabilities']


class Capabilities(unittest.TestCase):
    def test_bits_match_the_engine(self):
        # pandare2 passes 1 << CAPABILITIES.index(name): the engine's PANDA_CAP_* values.
        header = (ROOT / 'engine/src/include/panda/panda_api.h').read_text()
        self.assertIn('PANDA_CAP_RECORD = 1,', header)
        self.assertIn('PANDA_CAP_TAINT = 2,', header)
        self.assertEqual(ns['CAPABILITIES'], ('record', 'taint'))

    def test_none_by_default(self):
        self.assertEqual(choose(None, 'tcg'), ())
        self.assertEqual(choose([], 'tcg'), ())
        self.assertEqual(choose(None, 'kvm'), ())

    def test_selection(self):
        self.assertEqual(choose(['record'], 'tcg'), ('record',))
        self.assertEqual(choose('taint,record', 'tcg'), ('record', 'taint'))
        self.assertEqual(choose(('taint',), 'tcg'), ('taint',))

    def test_invalid(self):
        for names, accel in [('vanilla', 'tcg'), ('analysis', 'tcg'), ('rr', 'tcg'), ('fuzz', 'tcg'),
                             ('guest-hypercall', 'tcg'), ('handoff', 'tcg'), ('record,', 'tcg'),
                             ('record', 'kvm'), ('taint', 'kvm')]:
            with self.subTest(names=names, accel=accel):
                with self.assertRaises(ValueError):
                    choose(names, accel)


if __name__ == '__main__':
    unittest.main()
