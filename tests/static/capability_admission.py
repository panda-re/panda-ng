"""Admission without capabilities: AST-extracted pandare2 and compiled panda/admission.h; no engine
or guest.

Run: panda-python tests/static/capability_admission.py

Plugins, callbacks and queued main-loop work need no startup capability: a run with none admits
them. pandare2 seals the capabilities before it opens the PANDA bridge, and always opens it. In the
engine, panda_admit refuses only an event that names a startup capability the run lacks (or before
the capabilities are sealed); callbacks name none.
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

PANDA_TEST = dict(area="capabilities", tier="static", needs=["cc"],
                  nix_check="capability-admission")

ROOT = Path(__file__).resolve().parents[2]
PYTHON = ROOT / 'python/core/pandare2/panda.py'
TREE = ast.parse(PYTHON.read_text())
PANDA = next(n for n in TREE.body if isinstance(n, ast.ClassDef) and n.name == 'Panda')
METHODS = {n.name: n for n in PANDA.body if isinstance(n, ast.FunctionDef)}
CAPABILITIES = ('record', 'taint')


def extracted_python():
    names = ('load_plugin', 'register_callback', 'queue_main_loop_wait_fn')
    cls = ast.ClassDef(name='Admission', bases=[], keywords=[], decorator_list=[],
                       body=[copy.deepcopy(METHODS[n]) for n in names])
    ns = {'CAPABILITIES': CAPABILITIES, 'debug': False, 'ABORTS': set(),
          'tcg_only': lambda name: False}
    exec(compile(ast.fix_missing_locations(ast.Module(body=[cls], type_ignores=[])),
                 str(PYTHON), 'exec'), ns)
    return ns


class CapabilityAdmission(unittest.TestCase):
    def test_no_capability_needed(self):
        self.assertNotIn('_require_analysis', METHODS)
        p = extracted_python()['Admission']()
        p.capabilities = ()
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
        p._bind_plugin = lambda name: events.append(('bind', name))
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

    def test_constructor_seals_before_bridge(self):
        init = METHODS['__init__']
        start = next(i for i, n in enumerate(init.body)
                     if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and
                         t.id == 'requested' for t in n.targets))
        stop = next(i for i in range(start, len(init.body))
                    if isinstance(init.body[i], ast.Assign) and
                    ast.unparse(init.body[i].targets[0]) == 'rr_disk')
        block = copy.deepcopy(init.body[start:stop])
        for capabilities, accepted in [((), True), (('record',), True), (('record', 'taint'), True),
                                       (('taint',), False)]:
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
                    want = sum(1 << CAPABILITIES.index(c) for c in capabilities)
                    self.assertEqual(events, [('seal', want), ('bridge', 'own-bridge')])
                    self.assertEqual('-capabilities' in p.panda_args, bool(capabilities))
                else:
                    with self.assertRaises(ValueError):
                        exec(code, ns)
                    self.assertEqual([e[0] for e in events], ['seal'])

    def test_native_admission(self):
        text = (ROOT / 'engine/src/panda/src/callbacks.c').read_text()
        match = re.search(r'static bool callback_mode_valid\([^;]*?\)\s*\{', text)
        self.assertIsNotNone(match)
        policy = text[match.start():text.index('\n}\n', match.end()) + 3]
        admit = text[text.index('static PandaAdmit callback_admit('):match.start()]
        admission = (ROOT / 'engine/src/include/panda/admission.h').read_text()
        load = text[text.index('static bool _panda_load_plugin('):]
        load = load[:load.index('\n}\n')]
        self.assertNotIn('capabilit', load)
        registration = text[text.rindex('static bool register_callback_with_context('):]
        self.assertLess(registration.index('!callback_mode_valid('), registration.index('g_new0('))
        caps = (ROOT / 'engine/src/include/system/capabilities.h').read_text()
        rec = int(re.search(r'STARTUP_CAP_RECORD = 1u << (\d+)', caps).group(1))
        c = r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#define PANDA_MODE_KVM 8
#define PANDA_MODE_RR 4
#define REPLAY_MODE_NONE 0
typedef int panda_cb_type;
#define PANDA_CB_GUEST_HYPERCALL 20
#define RECORD (1u << ''' + str(rec) + r''')
static bool frozen, kvm;
static uint32_t chosen;
static int replay_mode;
static bool startup_caps_is_frozen(void) {return frozen;}
static bool startup_caps_has(uint32_t caps) {return (chosen & caps) == caps;}
static bool kvm_enabled(void) {return kvm;}
static uint32_t panda_callback_supported_modes(panda_cb_type type) {(void)type;return 7;}
'''
        c += admission + admit + policy + r'''
int main(void) {
    /* Callbacks need no capability, sealed or not, with any capabilities. */
    for(int f=0;f<2;f++)for(uint32_t caps=0;caps<4;caps++) {
        frozen=f;chosen=caps;
        assert(callback_mode_valid(0,7,true));
        assert(callback_mode_valid(0,7,false));
        assert(panda_admit(7,0,false,7,true)==PANDA_ADMIT_OK);
        /* An event that names a capability: only once sealed with it. */
        assert((panda_admit(7,RECORD,false,7,true)==PANDA_ADMIT_OK)==(f && (caps & RECORD)));
    }
    frozen=true;chosen=0;
    replay_mode=1;assert(!callback_mode_valid(0,7,false));assert(callback_mode_valid(0,7,true));
    /* A hypercall answer is logged, so a mutating guest_hypercall callback is admitted. */
    assert(callback_mode_valid(PANDA_CB_GUEST_HYPERCALL,7,false));
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
