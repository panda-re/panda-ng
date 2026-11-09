"""Compile extracted native RR guards against side-effect-counting stubs; no engine/guest.

Run: panda-python tests/static/replay_native.py
Uses CC (default cc), a temporary directory, and the actual C bodies, not copies
of their policy decisions. The architecture setters are compiled for x86 here.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'engine/src'


def body(text, name):
    """Extract a definition's body, balancing braces outside comments/strings."""
    match = re.search(r'\b' + re.escape(name) + r'\s*\([^;{}]*\)\s*\{', text)
    if match is None:
        raise AssertionError(f'function definition not found: {name}')
    start = match.end() - 1
    tokens = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    depth = 0
    for token in tokens.finditer(text, start):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:token.end()]
    raise AssertionError(f'unclosed function body: {name}')


def function(path, name, signature):
    return signature + '\n' + body((SRC / path).read_text(), name) + '\n'


STUBS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TARGET_I386 1
#define R_EAX 0
#define GPR(x) env->regs[(x)]
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define qatomic_read(p) (*(p))
#define qatomic_inc(p) (++*(p))
#define qatomic_fetch_inc(p) ((*(p))++)
#define DEVICE_NATIVE_ENDIAN 0
#define g_new0(t,n) ((t *)alloc_zero(sizeof(t) * (n)))
enum { REPLAY_MODE_NONE, REPLAY_MODE_RECORD, REPLAY_MODE_PLAY };
static int replay_mode;
typedef uint64_t target_ulong;
typedef uint64_t vaddr;
typedef struct { uint64_t regs[8]; } CPUArchState;
typedef struct CPUState { CPUArchState env; } CPUState;
static int env_calls, syncs, writes, allocations, duplicates, inits, mappings;
static CPUArchState *cpu_env(CPUState *cpu) { env_calls++; return &cpu->env; }
static target_ulong regs[] = {0, 1, 2};
static void cpu_synchronize_state(CPUState *cpu) { (void)cpu; syncs++; }
static int gdb_write_register(CPUState *cpu, uint8_t *buf, int n) {
    writes++; cpu->env.regs[n] = *buf; return 1;
}
/* Inside a recorded hypercall answer the logged write APIs are admitted. */
#include "panda/guest-write.h"
#define HOST_BIG_ENDIAN 0
static void stq_le_p(void *p, uint64_t v) { memcpy(p, &v, 8); }
static bool in_hypercall;
static int logged;
static int replay_guest_write_admitted(CPUState *cpu, size_t n) {
    (void)cpu; (void)n; return in_hypercall ? GUEST_WRITE_OK : GUEST_WRITE_EPERM;
}
static void replay_guest_write_log_reg(CPUState *cpu, int reg, const uint8_t *buf, int len,
                                       int result) { (void)cpu; (void)reg; (void)buf; (void)len; (void)result; logged++; }
/* The env write itself (replay-effect.c): bytes at the offset, logged */
static int replay_guest_write_env(CPUState *cpu, const uint32_t *d, unsigned n, uint32_t off,
                                  const void *buf, uint32_t len) {
    assert(!n && !d && off + len <= sizeof(cpu->env));
    memcpy((uint8_t *)&cpu->env + off, buf, len); logged++; return GUEST_WRITE_OK;
}
static void *alloc_zero(size_t n) { allocations++; return calloc(1, n); }
static char *g_strdup(const char *s) { duplicates++; return strdup(s); }
static void *g_realloc(void *p, size_t n) { allocations++; return realloc(p, n); }
typedef uint64_t (*panda_mmio_read_cb_t)(uint64_t, unsigned, void *);
typedef void (*panda_mmio_write_cb_t)(uint64_t, uint64_t, unsigned, void *);
typedef struct {
    panda_mmio_read_cb_t read;
    panda_mmio_write_cb_t write;
    int endianness;
    struct { int min_access_size, max_access_size; } valid, impl;
} Ops;
typedef struct {
    panda_mmio_read_cb_t read_cb;
    panda_mmio_write_cb_t write_cb;
    void *opaque;
    char *name;
    Ops ops;
    int mr;
} PandaMmioRegion;
static PandaMmioRegion *mapped;
static uint64_t panda_mmio_read(uint64_t a, unsigned n, void *p) {
    (void)a; (void)n; (void)p; return 0;
}
static void panda_mmio_write(uint64_t a, uint64_t v, unsigned n, void *p) {
    (void)a; (void)v; (void)n; (void)p;
}
static void memory_region_init_io(int *mr, void *owner, Ops *ops, void *opaque,
                                  const char *name, uint64_t size) {
    (void)mr; (void)owner; (void)ops; (void)name; (void)size;
    inits++; mapped = opaque;
}
static void *get_system_memory(void) { return NULL; }
static void memory_region_add_subregion_overlap(void *s, uint64_t a, int *mr, int p) {
    (void)s; (void)a; (void)mr; (void)p; mappings++;
}
static uint8_t payload = 7;
'''


CHECKS = r'''
int main(int argc, char **argv) {
    assert(argc == 3);
    replay_mode = atoi(argv[1]);
    bool live = replay_mode == REPLAY_MODE_NONE;
    CPUState cpu = {0};
    if (!strcmp(argv[2], "setters")) {
        panda_set_retval(&cpu, 41);
        panda_set_syscall_arg(&cpu, 1, 42);
        assert(cpu.env.regs[0] == (live ? 41 : 0));
        assert(cpu.env.regs[1] == (live ? 42 : 0));
        assert(logged == (live ? 2 : 0));
        assert(panda_write_guest_reg(&cpu, 2, &payload, 1) == (live ? 0 : -1));
        assert(syncs == live && writes == live);
        assert(cpu.env.regs[2] == (live ? payload : 0));
    } else if (!strcmp(argv[2], "hypercall")) {
        /* A hypercall answer's writes go ahead in every mode, and the API one is logged. */
        in_hypercall = true;
        panda_set_retval(&cpu, 41);
        panda_set_syscall_arg(&cpu, 1, 42);
        assert(cpu.env.regs[0] == 41 && cpu.env.regs[1] == 42);
        assert(panda_write_guest_reg(&cpu, 2, &payload, 1) == 0);
        assert(writes == 1 && logged == 3 && cpu.env.regs[2] == payload);
    } else if (!strcmp(argv[2], "mmio")) {
        assert(panda_add_mmio_region(4096, 4096, "test", panda_mmio_read,
                                          NULL, NULL) == (live ? 0 : -1));
        assert(allocations == live && duplicates == live);
        assert(inits == live && mappings == live);
        if (mapped) { free(mapped->name); free(mapped); }
    } else { assert(!"unknown case"); }
    return 0;
}
'''


def harness():
    definitions = [
        ('panda/src/callbacks.c', 'panda_guest_mutation_allowed',
         'static bool panda_guest_mutation_allowed(void)'),
        ('panda/src/guest-write.c', 'guest_admit',
         'static int guest_admit(CPUState *cpu, size_t len)'),
        ('panda/src/guest-write.c', 'panda_guest_write_env_path',
         'GuestWriteResult panda_guest_write_env_path(CPUState *cpu, const uint32_t *derefs, '
         'size_t n, size_t offset, const void *buf, size_t len)'),
        ('panda/src/guest-write.c', 'panda_guest_write_env',
         'GuestWriteResult panda_guest_write_env(CPUState *cpu, size_t offset, '
         'const void *buf, size_t len)'),
        ('panda/src/guest-write.c', 'guest_retval_field',
         'static size_t guest_retval_field(CPUState *cpu, size_t *len)'),
        ('panda/src/guest-write.c', 'panda_guest_set_retval',
         'GuestWriteResult panda_guest_set_retval(CPUState *cpu, uint64_t value)'),
        ('panda/src/panda_arch.c', 'panda_set_retval',
         'static void panda_set_retval(CPUState *cpu, target_ulong value)'),
        ('panda/src/panda_arch.c', 'panda_set_syscall_arg',
         'static void panda_set_syscall_arg(CPUState *cpu, int arg, target_ulong value)'),
        ('system/panda-embed.c', 'panda_write_guest_reg',
         'static int panda_write_guest_reg(CPUState *cs, int regnum, const uint8_t *buf, int len)'),
        ('system/panda-embed.c', 'panda_add_mmio_region',
         'static int panda_add_mmio_region(uint64_t base, uint64_t size, const char *name, '
         'panda_mmio_read_cb_t read_cb, panda_mmio_write_cb_t write_cb, void *opaque)'),
    ]
    # replay-analysis.h is generated by the patch pipeline, not present in src.
    patch = (ROOT / 'engine/patches/11.1.2/0084-replay-guard-guest-state-against-analysis-mutations.patch').read_text()
    added = '\n'.join(line[1:] for line in patch.splitlines()
                      if line.startswith('+') and not line.startswith('+++'))
    policy = 'static bool replay_analysis_write_allowed(void)\n' + body(added, 'replay_analysis_write_allowed')
    return STUBS + policy + '\n' + ''.join(function(*d) for d in definitions) + CHECKS


class ReplayNative(unittest.TestCase):
    def test_native_guards(self):
        with tempfile.TemporaryDirectory(prefix='replay-native-') as directory:
            directory = Path(directory)
            source = directory / 'guards.c'
            executable = directory / 'guards'
            source.write_text(harness())
            command = shlex.split(os.environ.get('CC', 'cc'))
            subprocess.run(command + ['-std=gnu11', '-O0', '-Wall',
                                      '-Werror=implicit-function-declaration', '-I', str(SRC / 'include'),
                                      str(source), '-o', str(executable)], check=True)
            for mode, name in enumerate(('NONE', 'RECORD', 'PLAY')):
                for case in ('setters', 'hypercall', 'mmio'):
                    with self.subTest(mode=name, case=case):
                        subprocess.run([str(executable), str(mode), case], check=True)


if __name__ == '__main__':
    unittest.main()
