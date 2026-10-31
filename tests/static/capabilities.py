"""Exercise Python capability selection without loading an engine."""
import ast
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
tree = ast.parse((ROOT / 'python/core/pandare2/panda.py').read_text())
selected = [node for node in tree.body
            if isinstance(node, ast.FunctionDef) and node.name == '_analysis_capabilities'
            or isinstance(node, ast.Assign) and any(isinstance(target, ast.Name) and
                target.id == 'CAPABILITIES' for target in node.targets)]
ns = {}
exec(compile(ast.Module(body=selected, type_ignores=[]), '<capabilities>', 'exec'), ns)
choose = ns['_analysis_capabilities']


class Capabilities(unittest.TestCase):
    def test_compatibility_default(self):
        self.assertEqual(choose(None, False, 'tcg'), ('analysis', 'guest-hypercall'))
        self.assertEqual(choose(None, True, 'tcg'), ('analysis', 'rr', 'guest-hypercall'))

    def test_explicit_zero_and_combinations(self):
        self.assertEqual(choose('vanilla', False, 'tcg'), ())
        self.assertEqual(choose([], False, 'tcg'), ())
        self.assertEqual(choose(['fuzz', 'handoff'], False, 'tcg'), ('analysis', 'fuzz', 'handoff'))
        self.assertEqual(choose('analysis', False, 'kvm'), ('analysis',))

    def test_invalid(self):
        for names, rr, accel in [('vanilla,analysis', False, 'tcg'),
                                 ('unknown', False, 'tcg'), ('analysis', True, 'tcg'),
                                 ('rr', False, 'tcg'), ('rr', True, 'kvm')]:
            with self.subTest(names=names, rr=rr, accel=accel):
                with self.assertRaises(ValueError):
                    choose(names, rr, accel)


if __name__ == '__main__':
    unittest.main()
