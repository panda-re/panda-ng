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
#define PSC_FAULTS 4
#define PSC_TRIES 3
#define PENGUIN_SC_IO_MAX 4096
#define PENGUIN_SC_STR_MAX 1024
#define DEVICE_NATIVE_ENDIAN 0
#define g_new0(t,n) ((t *)alloc_zero(sizeof(t) * (n)))
enum { REPLAY_MODE_NONE, REPLAY_MODE_RECORD, REPLAY_MODE_PLAY };
static int replay_mode;
typedef uint64_t target_ulong;
typedef uint64_t vaddr;
typedef struct { uint64_t regs[8]; } CPUArchState;
typedef struct { CPUArchState env; } CPUState;
static int env_calls, syncs, writes, allocations, duplicates, inits, mappings;
static int restores, observers, jumps, arch_calls, reads, refusals, reg_sets;
static int hooks_checks, executions, holds, callbacks;
static CPUArchState *cpu_env(CPUState *cpu) { env_calls++; return &cpu->env; }
static target_ulong regs[] = {0, 1, 2};
static void cpu_synchronize_state(CPUState *cpu) { (void)cpu; syncs++; }
static int gdb_write_register(CPUState *cpu, uint8_t *buf, int n) {
    writes++; cpu->env.regs[n] = *buf; return 1;
}
static void *alloc_zero(size_t n) { allocations++; return calloc(1, n); }
static char *g_strdup(const char *s) { duplicates++; return strdup(s); }
static void *g_realloc(void *p, size_t n) { allocations++; return realloc(p, n); }
typedef uint64_t (*penguin_mmio_read_cb_t)(uint64_t, unsigned, void *);
typedef void (*penguin_mmio_write_cb_t)(uint64_t, uint64_t, unsigned, void *);
typedef struct {
    penguin_mmio_read_cb_t read;
    penguin_mmio_write_cb_t write;
    int endianness;
    struct { int min_access_size, max_access_size; } valid, impl;
} Ops;
typedef struct {
    penguin_mmio_read_cb_t read_cb;
    penguin_mmio_write_cb_t write_cb;
    void *opaque;
    char *name;
    Ops ops;
    int mr;
} PenguinMmioRegion;
static PenguinMmioRegion *mapped;
static uint64_t penguin_mmio_read(uint64_t a, unsigned n, void *p) {
    (void)a; (void)n; (void)p; return 0;
}
static void penguin_mmio_write(uint64_t a, uint64_t v, unsigned n, void *p) {
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
static void tcg_hook_restore_state(CPUState *cpu) { (void)cpu; restores++; }
static void tcg_hook_set_pc(CPUState *cpu, uint64_t pc) { (void)cpu; (void)pc; jumps++; }
typedef struct { uint64_t (*cb)(CPUState *, uint64_t, void *); void *opaque; } PKHook;
static uint64_t hook_observer(CPUState *cpu, uint64_t pc, void *opaque) {
    (void)cpu; assert(opaque == &observers); observers++; return pc + 4;
}
static bool psc_x86 = true, psc_arm32;
static int psc_arch(void) { arch_calls++; return 1; }
static uint64_t psc_reg(CPUState *cpu, int n) { (void)cpu; (void)n; reads++; return 16; }
static int cpu_memory_rw_debug(CPUState *cpu, uint64_t a, uint8_t *b, int n, bool w) {
    (void)cpu; (void)a; (void)w; reads++; memset(b, 0, n); b[0] = 32; return 0;
}
static bool target_big_endian(void) { return false; }
static uint32_t ldl_be_p(uint8_t *p) { return *p; }
static uint32_t ldl_le_p(uint8_t *p) { return *p; }
static uint64_t ldq_le_p(uint8_t *p) { return *p; }
static bool psc_fault_refuse(uint64_t a) { (void)a; refusals++; return true; }
static void psc_set_reg(CPUState *cpu, int n, uint64_t v) {
    (void)cpu; (void)n; (void)v; reg_sets++;
}
enum { PENGUIN_SC_OP_NONE, PENGUIN_SC_OP_READ, PENGUIN_SC_OP_READ_STR,
       PENGUIN_SC_OP_WRITE, PENGUIN_SC_OP_SYSCALL, PENGUIN_SC_FAIL = -1 };
struct penguin_sc_cmd {
    int op, status;
    uint64_t addr, size;
    void *data, *buf;
    size_t len;
    int64_t num, ret;
};
typedef struct {
    uint64_t args[6], hook, call;
    int64_t retval;
    bool skip_syscall;
} Event;
typedef struct {
    struct penguin_sc_cmd cmd;
    unsigned total, budget, streak, nbad;
    uint64_t fpage, bad[4];
    void *wdata;
    unsigned i, n;
    uint64_t ids[1], args[6];
    int phase;
    bool running, skip;
    int64_t ret;
    Event ev;
} PRun;
typedef struct { uint64_t args[6]; } Trap;
typedef struct { PRun *r; void *t; CPUState *cpu; Trap *sc; } PCtx;
typedef struct { bool dead; uint64_t id; } PHook;
typedef void (*penguin_syscall_cb_t)(CPUState *, Event *, struct penguin_sc_cmd *, void *);
static penguin_syscall_cb_t psc_cb;
static void *psc_cb_opaque;
static unsigned psc_events, psc_next_call;
static bool psc_guarded = true, psc_consumer = true;
static bool psc_hooks_ok(void) { hooks_checks++; return true; }
static size_t qemu_target_page_size(void) { return 4096; }
static PHook test_hook = { .id = 1 };
static const PHook *psc_find(void *t, uint64_t id) { (void)t; (void)id; return &test_hook; }
static void psc_fill(PCtx *c, Event *e, const uint64_t *a, bool enter) {
    (void)c; (void)enter; memset(e, 0, sizeof(*e)); memcpy(e->args, a, sizeof(e->args));
}
static bool psc_cmd(PCtx *c) { (void)c; executions++; return true; }
static void psc_hold(PCtx *c, int phase) { (void)c; (void)phase; holds++; }
static int command_op;
static uint8_t payload = 7;
static void command_observer(CPUState *cpu, Event *e, struct penguin_sc_cmd *cmd, void *p) {
    (void)cpu; (void)e; (void)p;
    if (callbacks++ == 0) {
        *cmd = (struct penguin_sc_cmd){ .op = command_op, .size = 1,
                                      .data = &payload, .num = 1, .ret = 99 };
    } else {
        if (replay_mode != REPLAY_MODE_NONE) {
            assert(cmd->status == PENGUIN_SC_FAIL);
            assert(cmd->data == &payload);
            if (command_op == PENGUIN_SC_OP_SYSCALL) assert(cmd->ret == 0);
        }
        cmd->op = PENGUIN_SC_OP_NONE;
    }
}
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
        assert(env_calls == (live ? 2 : 0));
        assert(penguin_write_guest_reg(&cpu, 2, &payload, 1) == (live ? 0 : -1));
        assert(syncs == live && writes == live);
        assert(cpu.env.regs[2] == (live ? payload : 0));
    } else if (!strcmp(argv[2], "mmio")) {
        assert(penguin_qemu_add_mmio_region(4096, 4096, "test", penguin_mmio_read,
                                          NULL, NULL) == (live ? 0 : -1));
        assert(allocations == live && duplicates == live);
        assert(inits == live && mappings == live);
        if (mapped) { free(mapped->name); free(mapped); }
    } else if (!strcmp(argv[2], "hook")) {
        PKHook hook = { hook_observer, &observers };
        pkh_fn(&cpu, 4096, &hook);
        assert(restores == 1 && observers == 1);
        assert(jumps == live);
    } else if (!strcmp(argv[2], "guard")) {
        /* Detached internal hooks remain inert even outside record/replay. */
        psc_consumer = false;
        psc_guard_cb(&cpu, 4096, NULL);
        psc_guard_cb(&cpu, 4096, (void *)1);
        assert(!arch_calls && !reads && !refusals && !reg_sets && !jumps);
        psc_consumer = true;
        /* Both signal forms must reject before any guest lookup/bookkeeping. */
        psc_guard_cb(&cpu, 4096, NULL);
        psc_guard_cb(&cpu, 4096, (void *)1);
        assert(arch_calls == (live ? 2 : 0));
        assert(reads == (live ? 7 : 0));
        assert(refusals == (live ? 2 : 0));
        assert(reg_sets == (live ? 4 : 0));
        assert(jumps == (live ? 2 : 0));
    } else if (!strcmp(argv[2], "fault")) {
        PCtx c = {0};
        assert(psc_may_fault(&c, 4096) == live);
        assert(hooks_checks == live);
        PRun r = { .budget = 4 };
        c.r = &r;
        assert(psc_may_fault(&c, 4096) == live);
        r.nbad = 1; r.bad[0] = 4096;
        assert(!psc_may_fault(&c, 4096));
    } else if (!strcmp(argv[2], "commands")) {
        /* Exercise the real psc_run caller, not a reimplementation of its branch. */
        for (int op = PENGUIN_SC_OP_WRITE; op <= PENGUIN_SC_OP_SYSCALL; op++) {
            PRun r = { .n = 1, .ids = {1} };
            Trap trap = {0};
            PCtx c = { .r = &r, .cpu = &cpu, .sc = &trap };
            command_op = op;
            callbacks = executions = allocations = holds = 0;
            psc_cb = command_observer;
            assert(psc_run(&c, true));
            assert(callbacks == 2 && holds == 0);
            assert(executions == live);
            assert(allocations == (live && op == PENGUIN_SC_OP_WRITE));
            free(r.wdata);
        }
    } else { assert(!"unknown case"); }
    return 0;
}
'''


def harness():
    definitions = [
        ('panda/src/callbacks.c', 'panda_guest_mutation_allowed',
         'static bool panda_guest_mutation_allowed(void)'),
        ('panda/src/panda_arch.c', 'panda_set_retval',
         'static void panda_set_retval(CPUState *cpu, target_ulong value)'),
        ('panda/src/panda_arch.c', 'panda_set_syscall_arg',
         'static void panda_set_syscall_arg(CPUState *cpu, int arg, target_ulong value)'),
        ('system/penguin.c', 'penguin_write_guest_reg',
         'static int penguin_write_guest_reg(CPUState *cs, int regnum, const uint8_t *buf, int len)'),
        ('system/penguin.c', 'penguin_qemu_add_mmio_region',
         'static int penguin_qemu_add_mmio_region(uint64_t base, uint64_t size, const char *name, '
         'penguin_mmio_read_cb_t read_cb, penguin_mmio_write_cb_t write_cb, void *opaque)'),
        ('system/penguin-syscalls.c', 'pkh_fn',
         'static void pkh_fn(CPUState *cpu, vaddr pc, void *opaque)'),
        ('system/penguin-syscalls.c', 'psc_guard_cb',
         'static void psc_guard_cb(CPUState *cpu, vaddr pc, void *opaque)'),
        ('system/penguin-syscalls.c', 'psc_may_fault',
         'static bool psc_may_fault(PCtx *c, uint64_t page)'),
        ('system/penguin-syscalls.c', 'psc_cmd_take',
         'static bool psc_cmd_take(PRun *r)'),
        ('system/penguin-syscalls.c', 'psc_run',
         'static bool psc_run(PCtx *c, bool is_enter)'),
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
                                      '-Werror=implicit-function-declaration',
                                      str(source), '-o', str(executable)], check=True)
            for mode, name in enumerate(('NONE', 'RECORD', 'PLAY')):
                for case in ('setters', 'mmio', 'hook', 'guard', 'fault', 'commands'):
                    with self.subTest(mode=name, case=case):
                        subprocess.run([str(executable), str(mode), case], check=True)


if __name__ == '__main__':
    unittest.main()
