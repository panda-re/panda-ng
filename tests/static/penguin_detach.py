#!/usr/bin/env python3
"""Penguin's detach, public hooks and drop chain on the real syscall trap core, offline.

The trap core (tests/static/trapstubs.py) is compiled whole; Penguin's consumer registration,
detach, kernel-hook and configuration bodies are extracted from penguin-syscalls.c. Checks:
- Penguin is one trap consumer among others: it attaches beside a foreign consumer (the
  QEMU-plugin bridge) and its detach removes only itself, leaving the other's watches and calls;
- unsafe detach (no BQL, from a vCPU, a running vCPU, kernel hooks installed) changes nothing;
- a call Penguin holds while it runs an injected call refuses the detach without any change to
  the slots, rows, watches, vCPU fields, counters or Penguin's state, on every retry;
- otherwise the detach drains: every call's Penguin data goes through psc_drop -> ABORT -> free
  exactly once, outside both the core's and Penguin's locks, with Penguin's callback and opaque
  still set and its watches already gone; configuration calls during the drain are refused;
- when Penguin was the last consumer the core is reset (nothing pending or held, the trap off).
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import trapstubs  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
psc = (ROOT / 'engine/src/system/penguin-syscalls.c').read_text()


def function(text, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;]*?\)\s*\{', text, re.M)
    if match and text[match.start():match.end()].startswith(name + '('):
        start = text.rfind('\n', 0, match.start() - 1) + 1
    else:
        start = match.start() if match else 0
    assert match, name
    return text[start:text.index('\n}', match.end()) + 2]


stubs = r'''
#include "trap_core.inc"
#include "qemu/lockable.h"
''' + trapstubs.GLOBALS + r'''
#define PENGUIN_SC_OP_ABORT 9
#define CONFIG_TCG 1     /* for the kernel-hook bodies (after the trap core, which needs the TLB with it) */
static CPUState *current_cpu;
static bool bql = true, main_thread = true, running[NCPUS];
static bool bql_locked(void) { return bql; }
static bool qemu_in_main_thread(void) { return main_thread; }
static bool cpu_is_stopped(CPUState *cpu) { return !running[cpu->cpu_index]; }
struct penguin_sc_event { int unused; };
struct penguin_sc_cmd { int op; };
typedef void (*penguin_syscall_cb_t)(CPUState *, struct penguin_sc_event *, struct penguin_sc_cmd *, void *);
typedef void (*penguin_syscall_boot_cb_t)(CPUState *, void *);
typedef uint64_t (*penguin_kernel_hook_cb_t)(CPUState *, uint64_t, void *);
typedef struct { bool running; struct penguin_sc_event ev; struct penguin_sc_cmd cmd; void *wdata, *rbuf; } PRun;
static unsigned freed;
static void psc_fault_resolved(PRun *r) { freed++; }
static QemuMutex psc_lock, psc_boot_lock;
static bool psc_consumer, psc_detaching, psc_dirty, psc_booted, psc_scope, psc_cur_cpu;
static SyscallTrapConsumer *psc_trap;
static unsigned psc_kernel_hooks;
static penguin_syscall_cb_t psc_cb;
static void *psc_cb_opaque;
static penguin_syscall_boot_cb_t psc_boot_cb;
static void *psc_boot_opaque;
typedef struct TCGHook { int unused; } TCGHook;
static TCGHook one_hook;
static bool insert_fails;
static TCGHook *tcg_hook_insert(uint64_t addr, void (*cb)(CPUState *, vaddr, void *), void *opaque) { return insert_fails ? NULL : &one_hook; }
static void tcg_hook_remove(TCGHook *h) { assert(h == &one_hook); }
static void pkh_fn(CPUState *cpu, vaddr pc, void *opaque) {}
'''
body = '\n'.join(function(psc, n) for n in ('psc_run_free', 'psc_run_abort', 'psc_drop'))
body += '\nstatic const SyscallTrapOps psc_ops = { .drop = psc_drop };\n'
for n in ('psc_mark_dirty', 'set_penguin_syscall_callback', 'set_penguin_syscall_boot_callback',
          'penguin_syscall_set_scope', 'penguin_syscall_set_current_source', 'psc_detach_quiescent',
          'penguin_syscall_detach'):
    body += '\n' + function(psc, n)
start = psc.index('typedef struct PKHook')
body += '\n' + psc[start:psc.index('static void pkh_fn', start)]
body += '\n' + function(psc, 'penguin_kernel_hook') + '\n' + function(psc, 'penguin_kernel_unhook')

tests = r'''
static int old_opaque, aborts;
static SyscallTrapConsumer *foreign;
static unsigned foreign_drops;
static void foreign_drop(SyscallTrap *sc, void *o) { foreign_drops++; g_free(sc->data); }
static const SyscallTrapOps foreign_ops = { .drop = foreign_drop };

static unsigned watches(SyscallTrapConsumer *c)
{
    unsigned n = 0;
    for (unsigned abi = 0; abi < SYSCALL_ABI_COUNT; abi++) {
        n += c ? c->watch_all[abi] : sc_watch_all[abi];
        for (unsigned num = 0; num < SYSCALL_TRAP_NUMS; num++) {
            n += c ? c->watch[abi][num] : sc_watch[abi][num];
        }
    }
    return n;
}

/* a call in the slot at @index, Penguin's data on it (and, with @also, the foreign consumer's) */
static void add_run_at(unsigned index, int state, bool with_data, bool also)
{
    uint64_t key = 1;
    while (sc_home(key, 2, 0x4000) != index) {
        key++;
    }
    assert(!sc_slots[index].pc);
    PRun *r = with_data ? g_new0(PRun, 1) : NULL;
    if (r) {
        r->running = true;
        r->wdata = g_malloc(8);
        r->rbuf = g_malloc(8);
    }
    SCScr e[2] = {
        { psc_trap, psc_trap->gen, { .data = r, .want = true } },
        { foreign, foreign ? foreign->gen : 0, { .data = also ? g_malloc(4) : NULL, .want = also } },
    };
    SCKey hk = sc_key(psc_trap);
    SyscallTrap sc = { 0 };
    qemu_mutex_lock(&sc_lock);
    sc_put(key, 2, 0x4000, 4, &sc, state, e, foreign ? 2 : 1, state == SC_PENDING ? NULL : &hk);
    qemu_mutex_unlock(&sc_lock);
    assert(sc_slots[index].pc && sc_slots[index].state == state);
}

static void boot(CPUState *cpu, void *opaque) {}
static uint64_t kernel(CPUState *cpu, uint64_t pc, void *opaque) { return 0; }
static void callback(CPUState *cpu, struct penguin_sc_event *ev, struct penguin_sc_cmd *cmd, void *opaque)
{
    assert(!cpu && opaque == &old_opaque && cmd->op == PENGUIN_SC_OP_ABORT);
    /* outside both locks, with Penguin's callback and opaque still set, its watches gone */
    assert(!psc_lock.held && !sc_lock.held && !psc_boot_lock.held);
    assert(psc_detaching && psc_consumer && psc_cb == callback && psc_cb_opaque == &old_opaque);
    assert(psc_boot_cb == boot && psc_boot_opaque == &old_opaque && psc_booted);
    assert(psc_trap && !psc_trap->live && !watches(psc_trap));
    assert(!set_penguin_syscall_callback(NULL, NULL));
    assert(!penguin_syscall_detach());
    assert(!penguin_kernel_hook(1, kernel, &old_opaque));
    set_penguin_syscall_boot_callback(boot, NULL);
    assert(psc_boot_opaque == &old_opaque);
    penguin_syscall_set_scope(false);
    penguin_syscall_set_current_source(false);
    assert(psc_scope && psc_cur_cpu);
    psc_mark_dirty();
    assert(!watches(psc_trap));
    aborts++;
}

static void attach(void)
{
    set_penguin_syscall_boot_callback(boot, &old_opaque);
    psc_booted = true;
    penguin_syscall_set_scope(true);
    penguin_syscall_set_current_source(true);
    assert(set_penguin_syscall_callback(callback, &old_opaque));
    assert(psc_consumer && psc_trap && watches(psc_trap));
}

static void detached(void)
{
    assert(!psc_consumer && !psc_trap && !psc_cb && !psc_cb_opaque && !psc_boot_cb);
    assert(!psc_boot_opaque && !psc_booted && !psc_detaching);
}

int main(void)
{
    qemu_mutex_init(&psc_lock);
    qemu_mutex_init(&psc_boot_lock);

    /* beside a foreign consumer: both attach; the detach removes Penguin only */
    foreign = syscall_trap_consumer_add(&foreign_ops, NULL, SYSCALL_TRAP_OBSERVE, INT_MAX);
    syscall_trap_consumer_watch(foreign, 0, 39, SYSCALL_TRAP_ENTER);
    attach();
    assert(syscall_trap_consumers() == 2 && psc_trap->order == SYSCALL_TRAP_MUTATE);
    add_run_at(0, SC_PENDING, true, true);      /* both have data */
    add_run_at(1, SC_HELD_ENTER, true, false);  /* Penguin holds it */
    add_run_at(2, SC_PENDING, true, false);     /* only Penguin wants it */
    assert(penguin_syscall_detach());
    detached();
    assert(aborts == 3 && freed == 3 && foreign_drops == 0);
    assert(syscall_trap_consumers() == 1 && foreign->live && watches(foreign) == 1);
    assert(sc_watch[0][39] == SYSCALL_TRAP_ENTER && watches(NULL) == 1 && syscall_trap_on);
    assert(syscall_trap_pending == 2 && sc_slots[0].pc && sc_slots[1].pc && !sc_slots[2].pc);
    assert(syscall_trap_consumer_remove(foreign));
    assert(foreign_drops == 1 && !syscall_trap_pending && !sc_held && !syscall_trap_on);
    foreign = NULL;
    rcu_flush();
    aborts = freed = 0;

    /* unsafe detaches change nothing */
    attach();
    unsigned old_watches = watches(NULL);
    bql = false; assert(!penguin_syscall_detach()); bql = true;
    current_cpu = cpus; assert(!penguin_syscall_detach()); current_cpu = NULL;
    running[1] = true; assert(!penguin_syscall_detach()); running[1] = false;
    assert(psc_consumer && psc_cb == callback && watches(NULL) == old_watches);
    insert_fails = true; assert(!penguin_kernel_hook(1, kernel, &old_opaque)); assert(!psc_kernel_hooks);
    insert_fails = false;
    uint64_t h = penguin_kernel_hook(1, kernel, &old_opaque);
    assert(h && psc_kernel_hooks == 1);
    assert(!penguin_syscall_detach() && watches(NULL) == old_watches);
    assert(penguin_kernel_unhook(h)); assert(!penguin_kernel_unhook(h) && !psc_kernel_hooks);

    for (int state = SC_INJECT; state <= SC_INJECT_RET; state++) {
        if (!psc_trap) {
            attach();
        }
        add_run_at(0, SC_PENDING, true, false);
        add_run_at(1, SC_HELD_ENTER, true, false);
        add_run_at(2, SC_HELD_RETURN, true, false);
        add_run_at(SC_SLOTS - 2, state, true, false);
        add_run_at(SC_SLOTS - 1, SC_PENDING, false, false);
        cpus[0].syscall_current = cpus[1].syscall_current = 42;
        cpus[0].syscall_kmmu = cpus[1].syscall_kmmu = 7;
        cpus[0].syscall_inject_abis = cpus[1].syscall_inject_abis = 3;
        sc_fault_addr = 123;
        sc_fault_write = true;
        static SCSlot before[SC_SLOTS];
        static uint8_t watch_before[sizeof(sc_watch)];
        CPUState cpu_before[NCPUS];
        size_t rows = (size_t)SC_SLOTS * sc_ncols * sizeof(SCState);
        g_autofree SCState *rows_before = g_memdup2(sc_rows, rows);
        memcpy(before, sc_slots, sizeof(before));
        memcpy(cpu_before, cpus, sizeof(cpus));
        memcpy(watch_before, sc_watch, sizeof(sc_watch));
        int old_aborts = aborts;
        unsigned old_freed = freed;
        bool old_dirty = psc_dirty;
        uint64_t seq = sc_seq, full = sc_full, stale = sc_stale;
        for (unsigned retry = 0; retry < 2; retry++) {
            assert(!penguin_syscall_detach());
            assert(!memcmp(before, sc_slots, sizeof(before)) && !memcmp(cpu_before, cpus, sizeof(cpus)));
            assert(!memcmp(rows_before, sc_rows, rows));
            assert(!memcmp(watch_before, sc_watch, sizeof(sc_watch)));
            assert(syscall_trap_pending == 5 && sc_held == 3 && syscall_trap_on);
            assert(sc_seq == seq && sc_full == full && sc_stale == stale);
            assert(psc_trap->live && !psc_detaching);
            assert(psc_consumer && psc_cb == callback && psc_cb_opaque == &old_opaque);
            assert(psc_boot_cb == boot && psc_boot_opaque == &old_opaque && psc_booted);
            assert(psc_scope && psc_cur_cpu && psc_dirty == old_dirty);
            assert(sc_fault_addr == 123 && sc_fault_write);
            assert(aborts == old_aborts && freed == old_freed && (!sc_dropped || !sc_dropped->len));
        }
        /* the injected call came back: the detach can drain */
        sc_slots[SC_SLOTS - 2].state = SC_HELD_RETURN;
        assert(penguin_syscall_detach());
        assert(aborts == old_aborts + 4 && freed == old_freed + 4);
        /* the last consumer: the core is reset */
        assert(!syscall_trap_pending && !sc_held && !watches(NULL) && !syscall_trap_on && !sc_list);
        assert(!cpus[0].syscall_current && !cpus[1].syscall_inject_abis);
        detached();
        psc_mark_dirty();
        assert(psc_dirty && !watches(NULL));    /* inert persistent state */
        assert(penguin_syscall_detach() && aborts == old_aborts + 4 && freed == old_freed + 4);
        rcu_flush();
    }
    puts("PASS");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='penguin-detach-') as temp:
    out = Path(temp)
    trapstubs.write_stubs(out)
    (out / 'trap_core.inc').write_text(trapstubs.core_source())
    (out / 'test.c').write_text(stubs + body + tests)
    subprocess.run([trapstubs.cc(), str(out / 'test.c'), '-o', str(out / 'test'),
                    *trapstubs.cflags(out)], check=True)
    subprocess.run([str(out / 'test')], check=True, 
                   # penguin_kernel_unhook keeps its few bytes on purpose (a vCPU may be in it)
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
# Every opaque-bearing/public mutation path rejects drain reentry; refresh and
# internal hooks must not revive watches or affect another consumer.
for name in ('set_penguin_syscall_boot_callback', 'penguin_syscall_set_name', 'penguin_syscall_set_layout',
             'penguin_syscall_set_guard', 'penguin_syscall_set_reaper', 'penguin_syscall_set_exec',
             'penguin_syscall_register', 'penguin_syscall_unregister', 'penguin_syscall_set_scope',
             'penguin_syscall_set_current_source'):
    assert 'psc_detaching' in function(psc, name), name
for name in ('psc_refresh', 'psc_guard_cb', 'psc_reap_cb', 'psc_exec_cb'):
    assert 'qatomic_read(&psc_consumer)' in function(psc, name), name
# Penguin's watches go through its own consumer only.
assert 'syscall_trap_watch(' not in psc and 'syscall_trap_set_consumer' not in psc
print('PASS: Penguin beside another trap consumer; its detach on the real core: unsafe and '
      'late-injection refusals without partial changes, an outside-lock drain with callback and '
      'opaque retained, exactly-once ABORT, the last consumer resetting the core')
