"""Actual AST/extracted-C capability admission regression; no engine/guest.
Run: panda-python tests/static/capability_admission.py
"""
import ast
import copy
import os
from pathlib import Path
import re
import subprocess
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[2]
PYTHON = ROOT / 'python/core/pandare2/panda.py'
TREE = ast.parse(PYTHON.read_text())
PANDA = next(n for n in TREE.body if isinstance(n, ast.ClassDef) and n.name == 'Panda')
METHODS = {n.name: n for n in PANDA.body if isinstance(n, ast.FunctionDef)}
CAPABILITIES = ('analysis', 'fuzz', 'rr', 'guest-hypercall', 'handoff', 'penguin')


def extracted_python():
    names = ('_require_analysis', 'load_plugin', '_bind_plugin', '_load_plugin_library', 'register_callback',
             'queue_main_loop_wait_fn')
    cls = ast.ClassDef(name='Admission', bases=[], keywords=[], decorator_list=[],
                      body=[copy.deepcopy(METHODS[n]) for n in names])
    select = next(n for n in TREE.body if isinstance(n, ast.FunctionDef)
                  and n.name == '_analysis_capabilities')
    ns = {'CAPABILITIES': CAPABILITIES, 'debug': False, 'ABORTS': set(),
          'tcg_only': lambda name: False}
    exec(compile(ast.fix_missing_locations(ast.Module(body=[copy.deepcopy(select), cls],
                                                      type_ignores=[])), str(PYTHON), 'exec'), ns)
    return ns


class DeniedBoundary:
    def __getattr__(self, name):
        raise AssertionError('denied operation reached FFI/native boundary: ' + name)


class CapabilityAdmission(unittest.TestCase):
    def test_python_denial_precedes_side_effects(self):
        ns = extracted_python()
        p = ns['Admission']()
        p.capabilities = ()
        p.ffi = p.libpanda = DeniedBoundary()
        p.plugin_register_count = 0
        p.main_loop_wait_fnargs = []
        # No plugin/cache/callback metadata attributes needed: capability
        # rejection must happen before even looking them up.
        calls = (lambda: p.load_plugin('osi_linux', {'x': 1}),
                 lambda: p._bind_plugin('plugin'),
                 lambda: p._load_plugin_library('plugin'),
                 lambda: p.register_callback(object(), object(), 'callback'),
                 lambda: p.queue_main_loop_wait_fn(lambda: None))
        for call in calls:
            with self.subTest(call=call), self.assertRaisesRegex(RuntimeError, 'analysis capability'):
                call()
        self.assertEqual(p.plugin_register_count, 0)
        self.assertEqual(p.main_loop_wait_fnargs, [])

    def test_default_preinit_acceptance(self):
        ns = extracted_python()
        self.assertIn('analysis', ns['_analysis_capabilities'](None, False, 'tcg'))
        self.assertEqual(ns['_analysis_capabilities']('vanilla', False, 'tcg'), ())
        p = ns['Admission']()
        p.capabilities = ns['_analysis_capabilities'](None, False, 'tcg')
        events = []
        p.ffi = SimpleNamespace(NULL=None, RTLD_NOW=1, RTLD_LOCAL=2,
            new=lambda *a: events.append(('new', a)) or object(),
            cast=lambda *a: events.append(('cast', a)) or object(),
            dlopen=lambda *a: events.append(('dlopen', a)) or object())
        p.libpanda = SimpleNamespace(
            panda_main_loop_kick=lambda: events.append(('kick',)),
            panda_set_qemu_path=lambda *a: events.append(('path',)),
            panda_require_from_library=lambda *a: events.append(('require',)),
            panda_callback_supported_modes=lambda n: 7,
            panda_register_callback_modes_helper=lambda *a: events.append(('register',)) or True)
        p.kconf_file = p.isf = None
        p.panda = 'engine'; p.plugins = {}; p._engine_global = True
        p._prepare_engine = lambda: events.append(('prepare',))
        p._load_plugin_library = lambda name: events.append(('library', name))
        p.load_plugin('plugin')
        self.assertIn(('require',), events)
        p.callback_dictionary = {1: SimpleNamespace(name='main_loop_wait', number=1)}
        p.plugin_register_count = 0; p.registered_callbacks = {}; p.accel = 'tcg'
        p.register_callback(1, lambda: None, 'preinit', observe_only=True)
        self.assertEqual(p.plugin_register_count, 1)
        self.assertIn('preinit', p.registered_callbacks)
        p.main_loop_wait_fnargs = []
        p.queue_main_loop_wait_fn(lambda: None)
        self.assertEqual(len(p.main_loop_wait_fnargs), 1)
        self.assertIn(('kick',), events)

    def test_constructor_handshake_before_bridge(self):
        init = METHODS['__init__']
        start = next(i for i, n in enumerate(init.body)
                     if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and
                         t.id == 'requested' for t in n.targets))
        stop = next(i for i in range(start, len(init.body))
                    if isinstance(init.body[i], ast.Expr) and
                    isinstance(init.body[i].value, ast.Call) and
                    ast.unparse(init.body[i].value.func) == 'self.panda_args.append')
        block = copy.deepcopy(init.body[start:stop])
        for capabilities, accepted in [((), True), (('analysis',), True), (('analysis',), False)]:
            with self.subTest(capabilities=capabilities, accepted=accepted):
                events = []
                p = SimpleNamespace(capabilities=capabilities, panda='engine',
                    _engine_global=False, arch_name='x86_64', target='softmmu', dll_extension='so')
                p.libpanda = SimpleNamespace(panda_configure_capabilities=lambda mask:
                                             events.append(('seal', mask)) or accepted)
                p.ffi = SimpleNamespace(RTLD_NOW=1, RTLD_LOCAL=2, RTLD_GLOBAL=4,
                    dlopen=lambda path, flags: events.append(('bridge', path)) or object())
                code = compile(ast.fix_missing_locations(ast.Module(body=block, type_ignores=[])),
                               str(PYTHON), 'exec')
                ns = {'self': p, 'CAPABILITIES': CAPABILITIES, 'own_interface': 'own-bridge'}
                if accepted:
                    exec(code, ns)
                    self.assertEqual(events[0][0], 'seal')
                    self.assertEqual(sum(e[0] == 'bridge' for e in events), bool(capabilities))
                    if not capabilities:
                        self.assertIsNone(p.plugin_interface)
                else:
                    with self.assertRaises(ValueError):
                        exec(code, ns)
                    self.assertEqual([e[0] for e in events], ['seal'])
        initialize = METHODS['_initialize_panda']
        guards = [n for n in ast.walk(initialize) if isinstance(n, ast.If) and
                  ast.unparse(n.test) == "'analysis' in self.capabilities"]
        self.assertTrue(any('self.register_callback' in ast.unparse(n) for n in guards))

    def test_native_policy_before_plugin_or_callback_work(self):
        text = (ROOT / 'engine/src/panda/src/callbacks.c').read_text()
        match = re.search(r'static bool callback_mode_valid\([^;]*?\)\s*\{', text)
        self.assertIsNotNone(match)
        start = match.end() - 1
        depth = 0
        for end in range(start, len(text)):
            if text[end] == '{': depth += 1
            elif text[end] == '}':
                depth -= 1
                if not depth: break
        policy = text[match.start():end + 1]
        load_start = text.index('static bool _panda_load_plugin(')
        load_end = text.index('    // static bool libpanda_loaded', load_start)
        load_guard = text[load_start:load_end] + '\n    dlopens++; return true;\n}\n'
        registration = text[text.index('bool panda_register_callback_with_context_modes('):]
        self.assertLess(registration.index('!callback_mode_valid('), registration.index('g_new0('))
        c = r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#define ANALYSIS_CAP_ANALYSIS 1
#define PANDA_MODE_KVM 8
#define PANDA_MODE_RR 4
#define REPLAY_MODE_NONE 0
#define error_report(...) ((void)0)
static bool frozen, analysis, kvm;
static int replay_mode, dlopens;
static bool analysis_policy_is_frozen(void) {return frozen;}
static bool analysis_policy_has(int cap) {(void)cap;return analysis;}
static bool kvm_enabled(void) {return kvm;}
'''
        c += policy + '\n' + load_guard + r'''
int main(void) {
    for(int f=0;f<2;f++)for(int a=0;a<2;a++) {
        frozen=f;analysis=a;
        bool admitted=f && a;
        assert(callback_mode_valid(7,true)==admitted);
        assert(_panda_load_plugin("plugin","plugin",true)==admitted);
        assert(dlopens==(admitted?1:0));dlopens=0;
    }
    frozen=analysis=true;
    assert(callback_mode_valid(7,false)); /* sealed analysis before CPU init */
    /* record/replay admits mutators: dispatch skips them until it ends */
    replay_mode=1;assert(callback_mode_valid(7,false));assert(callback_mode_valid(7,true));
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='capability-admission-') as tmp:
            source = Path(tmp) / 'admission.c'; source.write_text(c)
            exe = Path(tmp) / 'admission'
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Werror',
                            str(source), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()
