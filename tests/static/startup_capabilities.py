#!/usr/bin/env python3
"""The startup capabilities in the patched tree: the real option lookup and prepass compiled with
the policy, and where the record capability acts. No engine build or guest.

Run: python3 -I tests/static/startup_capabilities.py PATCHED_QEMU_TREE

- -capabilities is sealed before option, module and plugin processing; the prepass skips option
  arguments (-append -capabilities), takes the last -capabilities, keeps an embedding's choice
  and refuses a different one, and refuses unknown names before anything is sealed;
- no capability gates remain on guest hypercalls, snapshot formats or cross-mode adaptations
  (the analysis, fuzz, rr, guest-hypercall and handoff capabilities are gone);
- the record capability turns rr-ready on, startup refuses it for arguments that can't be
  recorded, and recording paths (x-rr-begin-record, -icount rr=record) check it; replay start
  consults the replay table.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile

PANDA_TEST = dict(area="capabilities", tier="static", needs=["cc", "glib"], args=["{engine_src}"],
                  nix_check="startup-capabilities")

ROOT = Path(__file__).resolve().parents[2]
TREE = Path(sys.argv[1]).resolve()


def read(path):
    return (TREE / path).read_text()


def function(text, name):
    match = re.search(r'^\w[^\n]*\b' + re.escape(name) + r'\([^;]*?\)\s*\{', text, re.M)
    assert match, name
    return text[match.start():text.index('\n}', match.end()) + 2]


vl = read('system/vl.c')
init = function(vl, 'qemu_init')
assert init.index('seal_startup_capabilities(') < init.index('qemu_add_opts(') < init.index('module_call_init(')
assert 'DEF("capabilities", HAS_ARG, QEMU_OPTION_capabilities' in read('qemu-options.hx')
assert "'capabilities.c'" in read('system/meson.build')
assert "'replay-conflicts.c'" in read('replay/meson.build')

# No capability gates left on what is always available.
OLD = re.compile(r'analysis_policy|ANALYSIS_CAP_|analysis-capabilities|analysis_guest_hypercall_enabled')
for path in ['hw/i386/pc.c', 'hw/intc/arm_gic_common.c', 'replay/replay-snapshot.c', 'replay/replay.c',
             'system/cpu-timers.c', 'target/arm/helper.c', 'target/arm/machine.c',
             'target/i386/kvm/kvm.c', 'target/i386/machine.c', 'target/i386/tcg/system/seg_helper.c',
             'target/arm/tcg/translate-a64.c', 'target/arm/tcg/translate.c',
             'target/loongarch/tcg/translate.c', 'target/loongarch/tcg/insn_trans/trans_extra.c.inc',
             'target/mips/tcg/translate.c', 'target/ppc/translate.c',
             'target/ppc/translate/fixedpoint-impl.c.inc', 'target/riscv/tcg/translate.c',
             'target/riscv/machine.c', 'target/riscv/cpu.c', 'hw/core/cpu-system.c', 'system/vl.c',
             'accel/tcg/tcg-all.c']:
    assert not OLD.search(read(path)), path
# The x86 hypercall port is always there, and riscv accepts version 11 CPU streams (11.1.0's).
assert 'memory_region_add_subregion(isa_bus->address_space_io, 0x88, ioport88_io);' in read('hw/i386/pc.c')
assert re.search(r'\.name = "cpu",\s*\.version_id = 12,(\s*/\*.*?\*/)?\s*\.minimum_version_id = 11,',
                 read('target/riscv/machine.c'), re.S)
assert 'legacy_vmsd_get' not in read('include/hw/core/sysemu-cpu-ops.h')

# record: the capability launches the recordable machine, refusals name it.
tcg = function(read('accel/tcg/tcg-all.c'), 'tcg_init_machine')
assert tcg.index('startup_caps_has(STARTUP_CAP_RECORD)') < tcg.index('if (s->rr_ready)')
assert '"-capabilities record"' in tcg
check = function(vl, 'record_startup_check')
for what in ('tcg_enabled()', 'replay_check_blockers(&err)', 'vhost_has_dev()'):
    assert what in check, what
done = function(vl, 'qemu_machine_creation_done')
assert 'startup_caps_has(STARTUP_CAP_RECORD) && !record_startup_check(errp)' in done
assert done.index('qdev_machine_creation_done()') < done.index('record_startup_check(')
begin = read('replay/replay-begin.c')
assert 'replay_record_check(errp)' in function(begin, 'qmp_x_rr_begin_record')
assert 'replay_start_check(errp)' in function(begin, 'qmp_x_rr_begin_replay')
start = function(read('replay/replay.c'), 'replay_start')
assert 'replay_record_check(&err)' in start and 'replay_start_check(&err)' in start
assert 'startup_caps_has(STARTUP_CAP_TAINT) && !tcg_enabled()' in function(vl, 'configure_accelerators')

lookup = vl[vl.index('static const QEMUOption *lookup_opt('):vl.index('\n}', vl.index('static const QEMUOption *lookup_opt(')) + 2]
with tempfile.TemporaryDirectory(prefix='startup-capabilities-') as temp:
    out = Path(temp)
    for name, text in {
        'qemu/osdep.h': '#include <stdbool.h>\n#include <stdint.h>\n#include <string.h>\n',
        'qemu/typedefs.h': 'typedef struct Error Error;\n',
        'qapi/error.h': '#include "qemu/typedefs.h"\nvoid error_setg(Error **, const char *, ...);\n',
    }.items():
        p = out / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
    stubs = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <glib.h>
#include "system/capabilities.h"
struct Error { char text[160]; };
void error_setg(Error **e,const char *fmt,...) {
    if (!e) return;
    assert(!*e); *e=g_new0(Error,1);
    va_list ap; va_start(ap,fmt); vsnprintf((*e)->text,sizeof((*e)->text),fmt,ap);va_end(ap);
}
#define HAS_ARG 1
#define QEMU_OPTION_capabilities 7
#define loc_set_cmdline(...) ((void)0)
#define error_report(...) ((void)0)
typedef struct QEMUOption { const char *name; int flags,index; } QEMUOption;
static const QEMUOption qemu_options[]={
    {"capabilities",HAS_ARG,QEMU_OPTION_capabilities},
    {"append",HAS_ARG,1},{"device",HAS_ARG,2},{"module",HAS_ARG,3},{"S",0,4},{0}
};
'''
    tests = r'''
int main(int argc,char **argv) {
    assert(argc==2); Error *error=NULL;
    char *bare[]={"qemu"};
    char *args[]={"qemu","-append","--capabilities","--capabilities","taint","-device","-capabilities","-capabilities","record","disk.img","-S"};
    char *both[]={"qemu","-capabilities","taint,record"};
    char *bad[]={"qemu","-module","constructor.so","-capabilities","rr"};
    char *old[]={"qemu","-capabilities","analysis"};
    if (!strcmp(argv[1],"bare")) {
        assert(seal_startup_capabilities(1,bare,&error));
        assert(startup_caps_is_frozen() && !startup_caps());
    } else if (!strcmp(argv[1],"both")) {
        assert(seal_startup_capabilities(3,both,&error));
        assert(startup_caps()==STARTUP_CAP_ALL);
    } else if (!strcmp(argv[1],"repeat")) {
        assert(seal_startup_capabilities(11,args,&error));
        assert(startup_caps()==STARTUP_CAP_RECORD);
    } else if (!strcmp(argv[1],"embedded")) {
        assert(startup_caps_seal(STARTUP_CAP_TAINT,&error));
        assert(seal_startup_capabilities(1,bare,&error));
        assert(startup_caps()==STARTUP_CAP_TAINT);
        assert(!seal_startup_capabilities(3,both,&error));
        assert(error && strstr(error->text,"differ"));
        assert(startup_caps()==STARTUP_CAP_TAINT);
    } else if (!strcmp(argv[1],"identical")) {
        assert(startup_caps_seal(STARTUP_CAP_RECORD,&error));
        assert(seal_startup_capabilities(11,args,&error));
    } else if (!strcmp(argv[1],"unknown")) {
        assert(!seal_startup_capabilities(5,bad,&error));
        assert(error && strstr(error->text,"'rr'") && !startup_caps_is_frozen());
    } else if (!strcmp(argv[1],"old")) {
        assert(!seal_startup_capabilities(3,old,&error));
        assert(error && strstr(error->text,"'analysis'") && !startup_caps_is_frozen());
    }
    g_clear_pointer(&error,g_free);
}
'''
    source = stubs + lookup + '\n' + function(vl, 'seal_startup_capabilities') + tests
    (out / 'test.c').write_text(source)
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'glib-2.0'], text=True))
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-O2', '-I', str(out),
                    '-I', str(ROOT / 'engine/src/include'), str(ROOT / 'engine/src/system/capabilities.c'),
                    str(out / 'test.c'), '-o', str(out / 'test'), *flags], check=True)
    for scenario in ('bare', 'both', 'repeat', 'embedded', 'identical', 'unknown', 'old'):
        subprocess.run([str(out / 'test'), scenario], check=True)
print('PASS: -capabilities sealed before side effects (option arguments skipped, last one wins, '
      'none when bare, embedded choice kept, mismatch and unknown names refused); no capability '
      'gates on hypercalls, snapshots or cross-mode loads; record turns rr-ready on and is '
      'checked at startup and by every recording path; replay start consults the replay table')
