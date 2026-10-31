#!/usr/bin/env python3
"""Compile actual RISC-V descriptor selection and core registration bodies.
Run: python3 -I tests/static/riscv_snapshot_gate.py PATCHED_QEMU_TREE
No engine/guest. Tests loader ordering with an explicit boundary stub.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile

TREE = Path(sys.argv[1]).resolve()


def function(text, name):
    match = re.search(r'^\w[^\n]*\b' + re.escape(name) + r'\([^;]*?\)\s*\{', text, re.M)
    assert match, name
    return text[match.start():text.index('\n}', match.end()) + 2]


machine = (TREE/'target/riscv/machine.c').read_text()
core = (TREE/'hw/core/cpu-system.c').read_text()
assert re.search(r'const VMStateDescription vmstate_riscv_cpu = \{\s*\.name = "cpu",\s*\.version_id = 12,\s*\.minimum_version_id = 12,', machine)
assert '.legacy_vmsd_get = riscv_cpu_legacy_vmsd' in (TREE/'target/riscv/cpu.c').read_text()
stubs = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <glib.h>
#define ANALYSIS_CAP_HANDOFF 16
static bool provisioned;
static bool analysis_policy_has(unsigned mask) { return provisioned; }
static bool analysis_policy_is_frozen(void) { return true; }
typedef struct VMStateDescription {
    const char *name;
    int version_id, minimum_version_id;
    void *fields, *subsections, *pre_load, *post_load;
} VMStateDescription;
typedef struct CPUState CPUState;
typedef struct SysemuCPUOps {
    const VMStateDescription *legacy_vmsd;
    const VMStateDescription *(*legacy_vmsd_get)(CPUState *cpu);
} SysemuCPUOps;
typedef struct CPUClass { const SysemuCPUOps *sysemu_ops; } CPUClass;
struct CPUState { const CPUClass *cc; int cpu_index; };
static const VMStateDescription vmstate_cpu_common = {0};
static const VMStateDescription vmstate_riscv_cpu = {
    .name="cpu", .version_id=12, .minimum_version_id=12,
    .fields=(void *)1, .subsections=(void *)2, .pre_load=(void *)3, .post_load=(void *)4
};
#define DEVICE(cpu) (cpu)
static const VMStateDescription *registered, *unregistered;
static const VMStateDescription *qdev_get_vmsd(CPUState *cpu) { return &vmstate_cpu_common; }
static void vmstate_register(void *unused, int instance, const VMStateDescription *vmsd, CPUState *cpu) { registered=vmsd; }
static void vmstate_unregister(void *unused, const VMStateDescription *vmsd, CPUState *cpu) { unregistered=vmsd; }
'''
body = function(machine, 'riscv_cpu_legacy_vmsd') + '\n'
body += '\n'.join(function(core, n) for n in ('cpu_legacy_vmsd','cpu_vmstate_register','cpu_vmstate_unregister'))
tests = r'''
static unsigned preloads, fields;
/* Mirrors generic vmstate's version-before-pre_load/fields boundary. */
static bool load(const VMStateDescription *vmsd, int version) {
    if (version > vmsd->version_id || version < vmsd->minimum_version_id) return false;
    preloads++; fields++; return true;
}
int main(int argc, char **argv) {
    assert(argc==2); provisioned=atoi(argv[1]);
    const SysemuCPUOps ops={.legacy_vmsd_get=riscv_cpu_legacy_vmsd};
    const CPUClass cc={.sysemu_ops=&ops};
    CPUState cpu={.cc=&cc};
    cpu_vmstate_register(&cpu);
    assert(registered->version_id==12);
    assert(registered->minimum_version_id==(provisioned ? 11 : 12));
    assert(registered->fields==vmstate_riscv_cpu.fields);
    assert(registered->subsections==vmstate_riscv_cpu.subsections);
    assert(registered->pre_load==vmstate_riscv_cpu.pre_load && registered->post_load==vmstate_riscv_cpu.post_load);
    assert(load(registered,11)==provisioned);
    assert(fields==provisioned && preloads==provisioned);
    assert(load(registered,12));
    assert(!load(registered,10) && !load(registered,13));
    assert(vmstate_riscv_cpu.minimum_version_id==12);
    assert(riscv_cpu_legacy_vmsd(&cpu)==registered);
    cpu_vmstate_unregister(&cpu); assert(unregistered==registered);
    // Existing targets without selector retain precisely their original descriptor.
    const SysemuCPUOps original={.legacy_vmsd=&vmstate_cpu_common};
    const CPUClass original_cc={.sysemu_ops=&original}; cpu.cc=&original_cc;
    cpu_vmstate_register(&cpu); assert(registered==&vmstate_cpu_common);
    cpu_vmstate_unregister(&cpu); assert(unregistered==registered);
}
'''
with tempfile.TemporaryDirectory(prefix='riscv-snapshot-gate-') as temp:
    out=Path(temp)
    (out/'test.c').write_text(stubs+body+tests)
    flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','glib-2.0'],text=True))
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O2',str(out/'test.c'),'-o',str(out/'test'),*flags],check=True)
    for enabled in ('0','1'):
        subprocess.run([str(out/'test'),enabled],check=True)
print('PASS: actual registration-selected min12/min11 descriptor, stable unregister/save12/field hooks; early rejection boundary and unchanged other-target descriptor fallback')
