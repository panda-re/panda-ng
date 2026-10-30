"""Offline tests of the real Python observer wrappers, without loading an engine.

Run: python3 tests/static/replay_policy.py
AST extraction avoids importing pandare2 (which requires built native bindings).
Native/replay integration is covered separately by tests/replay_guard.py.
"""
import ast
from pathlib import Path
import threading
from types import SimpleNamespace
import unittest


ROOT = Path(__file__).resolve().parents[2]
tree = ast.parse((ROOT / 'python/core/pandare2/panda.py').read_text())
methods = {'_check_guest_mutation', '_check_rr_observers', '_catching', 'record', 'run_replay'}
panda = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'Panda')
selected = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == '_wrapper_maker']
selected += [ast.ClassDef(name='Policy', bases=[], keywords=[], decorator_list=[],
                          body=[n for n in panda.body if isinstance(n, ast.FunctionDef) and n.name in methods])]
ns = {'_WRAPPER_MAKERS': {}}
exec(compile(ast.fix_missing_locations(ast.Module(body=selected, type_ignores=[])),
             str(ROOT / 'python/core/pandare2/panda.py'), 'exec'), ns)
Policy = ns['Policy']


class ReplayPolicy(unittest.TestCase):
    def setUp(self):
        self.replay = False
        self.record = False
        self.errors = []
        self.p = Policy()
        self.p.libpanda = SimpleNamespace(
            PANDA_MODE_RR=4,
            rr_in_replay=lambda: self.replay,
            panda_guest_mutation_allowed=lambda: not (self.replay or self.record))
        self.p._callback_access = threading.local()
        self.p._callback_raised = self.errors.append
        self.p._exit = False

    def test_live_mutator_and_observer(self):
        self.p._catching(lambda: self.p._check_guest_mutation('write'), 0)()
        self.assertEqual(self.errors, [])
        self.p._catching(lambda: self.p._check_guest_mutation('write'), 0,
                         observe_only=True)()
        self.assertIsInstance(self.errors.pop(), PermissionError)
        self.assertEqual(self.p._callback_access.observers, 0)
        self.p._check_guest_mutation('write after observer')

    def test_nested_mutator_cannot_escape_observer(self):
        inner = self.p._catching(lambda: self.p._check_guest_mutation('write'), 0)
        self.p._catching(inner, 0, observe_only=True)()
        self.assertIsInstance(self.errors.pop(), PermissionError)
        self.assertEqual(self.p._callback_access.observers, 0)

    def test_replay_entry_and_late_registration(self):
        early = self.p._catching(lambda: self.fail('mutator executed'), 0)
        self.replay = True
        early()
        self.assertIsInstance(self.errors.pop(), PermissionError)
        with self.assertRaises(ValueError):
            self.p._catching(lambda: None, 0)
        with self.assertRaises(PermissionError):
            self.p._check_guest_mutation('outside callback')

    def test_recording_rejects_unlogged_mutators(self):
        early = self.p._catching(lambda: self.fail('mutator executed'), 0)
        self.record = True
        early()
        self.assertIsInstance(self.errors.pop(), PermissionError)
        with self.assertRaises(ValueError):
            self.p._catching(lambda: None, 0)
        with self.assertRaises(PermissionError):
            self.p._check_guest_mutation('unlogged write')
        self.p._catching(lambda: None, 0, observe_only=True)()
        self.assertEqual(self.errors, [])

    def test_rr_preflight_checks_python_bridges(self):
        self.p.registered_callbacks = {}
        self.p._ppp_observers = {}
        self.p.hook_list = []
        self.p._hook_observers = {}
        self.p.hypercalls = {}
        self.p._check_rr_observers()
        self.p._ppp_observers['mutator'] = False
        with self.assertRaises(ValueError):
            self.p._check_rr_observers()
        self.p._ppp_observers['mutator'] = True
        self.p._check_rr_observers()
        self.p.registered_callbacks['unsafe'] = dict(enabled=True, observe_only=False, modes=4)
        with self.assertRaises(ValueError):
            self.p._check_rr_observers()
        self.p.registered_callbacks['unsafe']['enabled'] = False
        self.p._check_rr_observers()
        self.p.registered_callbacks['incompatible'] = dict(enabled=True, observe_only=True, modes=1)
        with self.assertRaises(ValueError):
            self.p._check_rr_observers()
        self.p.registered_callbacks.clear()
        hook = type('Hook', (), {'enabled': True})()
        self.p.hook_list = [(hook, None)]
        with self.assertRaises(ValueError):
            self.p._check_rr_observers()
        hook.enabled = False
        self.p._check_rr_observers()
        hook.enabled = True
        self.p._hook_observers[hook] = True
        self.p._check_rr_observers()
        def hypercall():
            pass
        self.p.hypercalls[hypercall] = [None, 123, False]
        with self.assertRaises(ValueError):
            self.p._check_rr_observers()
        self.p.hypercalls[hypercall][2] = True
        self.p._check_rr_observers()

    def test_public_rr_entry_checks_before_native_requests(self):
        self.p._needs_tcg = lambda operation: None
        self.p._needs_engine = lambda operation: None
        self.p.registered_callbacks = {'unsafe': dict(enabled=True, observe_only=False, modes=4)}
        self.p._ppp_observers = {}
        self.p.hook_list = []
        self.p._hook_observers = {}
        self.p.hypercalls = {}
        # No FFI/native request or replay file lookup may happen before rejection.
        with self.assertRaisesRegex(ValueError, 'observe_only'):
            self.p.record('unused')
        with self.assertRaisesRegex(ValueError, 'observe_only'):
            self.p.run_replay('unused')

    def test_observer_results_and_exception_unwind(self):
        self.replay = True
        boolean = SimpleNamespace(cname='_Bool')
        self.assertTrue(self.p._catching(lambda: True, 0, boolean,
                                        observe_only=True)())
        def fail():
            raise RuntimeError('observer failed')
        self.assertIsNone(self.p._catching(fail, 0, observe_only=True)())
        self.assertIsInstance(self.errors.pop(), RuntimeError)
        self.assertEqual(self.p._callback_access.observers, 0)

    def test_exception_passthrough_on_rejected_mutator(self):
        callback = self.p._catching(lambda cpu, index: -1, 2,
                                   SimpleNamespace(cname='int'), passthrough=True)
        self.replay = True
        self.assertEqual(callback(None, 23), 23)
        self.assertIsInstance(self.errors.pop(), PermissionError)


if __name__ == '__main__':
    unittest.main()
