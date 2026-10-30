"""Compile actual cancellation/reset-hook code against small host-only stubs.

Run: python3 tests/static/lifecycle.py
No engine, bindings, image or guest required. This tests real extracted C bodies,
not a second implementation of their lifecycle logic.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(text, name):
    """The project's C bodies put their closing brace in column zero."""
    # Avoid matching declarations/comments before the signature.
    match = re.search(r'^\w[^\n]*\b' + re.escape(name) + r'\([^;]*?\)\s*\{',
                      text, re.MULTILINE)
    assert match, name
    start = match.start()
    end = text.index('\n}', match.end()) + 2
    return text[start:end]


core = (ROOT / 'engine/src/system/syscall-trap.c').read_text()
callbacks = (ROOT / 'engine/src/panda/src/callbacks.c').read_text()
penguin = (ROOT / 'engine/src/system/penguin-syscalls.c').read_text()
header = (ROOT / 'engine/src/include/system/syscall-trap.h').read_text()
trap = header[header.index('typedef struct SyscallTrap {'):header.index('/* one consumer')]
slots = core[core.index('#define SC_SLOTS'):core.index('/*\n * Run the call the consumer')]
# Slots/core use precisely these wrappers in QEMU; real host mutexes catch lock re-entry.
stubs = r'''
#include <glib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SYSCALL_ABI_COUNT 10
#define SYSCALL_ABI_I386 1
#define SYSCALL_ABI_ARM 3
#define SYSCALL_ABI_ARM_OABI 4
#define SYSCALL_ABI_PPC 6
#define SYSCALL_ABI_RISCV32 8
#define SYSCALL_TRAP_NUMS 1024
#define SYSCALL_TRAP_FAULT 2
#define qatomic_read(p) (*(p))
#define qatomic_set(p,v) (*(p)=(v))
#define qatomic_inc(p) (++*(p))
#define qatomic_dec(p) (--*(p))
typedef GMutex QemuMutex;
#define qemu_mutex_init g_mutex_init
#define qemu_mutex_lock g_mutex_lock
#define qemu_mutex_unlock g_mutex_unlock
typedef struct CPUState {
    uint64_t syscall_current;
    uint32_t syscall_kmmu;
    uint32_t syscall_inject_abis;
} CPUState;
static CPUState cpus[2], *first_cpu, *current_cpu;
static bool bql = true, main_thread = true, running, cpus_paused = true;
static bool cpu_is_stopped(CPUState *cpu) { return cpus_paused; }
static bool bql_locked(void) { return bql; }
static bool qemu_in_main_thread(void) { return main_thread; }
static bool runstate_is_running(void) { return running; }
#define error_report(...) ((void)0)
static struct { void *plugin; bool unload, exported_symbols; } panda_plugins[] = {{(void *)1, false, false}, {(void *)2, false, false}, {(void *)3, false, false}};
static int nb_panda_plugins = 3;
#define CPU_FOREACH(cpu) for (cpu = cpus; cpu < cpus + 2; cpu++)
'''
# Penguin's existing drop -> ABORT -> free chain, with only its unrelated fields stubbed.
psc_stubs = r'''
#define PENGUIN_SC_OP_ABORT 9
struct penguin_sc_cmd { int op; };
typedef struct { bool running; struct penguin_sc_cmd cmd; int ev; void *wdata, *rbuf; } PRun;
typedef void (*penguin_syscall_cb_t)(CPUState *, int *, struct penguin_sc_cmd *, void *);
static penguin_syscall_cb_t psc_cb;
static void *psc_cb_opaque;
static unsigned aborted, resolved;
static void psc_fault_resolved(PRun *r) { resolved++; }
'''
psc = '\n'.join(function(penguin, n) for n in ['psc_run_free', 'psc_run_abort', 'psc_drop'])
cancel = function(core, 'syscall_trap_cancel_all')
release = ('void syscall_trap_unwatch_all(void);\n' + '\n'.join(
    function(core, n) for n in ['syscall_trap_set_consumer',
                               'syscall_trap_clear_consumer',
                               'syscall_trap_unwatch_all']))
hooks = callbacks[callbacks.index('typedef struct PandaTransientReset'):callbacks.index('static bool panda_on_main_thread')]
# Failed initialization and normal unload share this central removal path.
close = function(callbacks, 'dlclose_plugin')
assert close.index('panda_unregister_transient_reset(plugin)') < close.index('panda_delete_plugin(plugin_idx)') < close.index('dlclose(plugin)')
checks = r'''
static int panda_main_walk_depth;
static unsigned unloads, closed;
static bool unload_requested;
void panda_unregister_transient_reset(void *owner);
static void panda_unload_pending(void);
static void panda_delete_plugin(int i)
{
    assert(!panda_main_walk_depth);
    nb_panda_plugins--;
}
static int dlclose(void *plugin) { closed++; return 0; }
#define LOG_ERROR(...) ((void)0)
'''
# Exercise the actual central removal body, not merely its source ordering.
checks += close + r'''
static void panda_unload_pending(void)
{
    assert(!panda_main_walk_depth);
    unloads++;
    if (unload_requested) {
        unload_requested = false;
        dlclose_plugin(2);
    }
}
'''
tests = r'''
static int core_cookie, callback_cookie;
static const SyscallTrapOps *expected_ops;
static bool releasing;
static unsigned drops;
static void checked_drop(SyscallTrap *sc, void *opaque)
{
    assert(opaque == &core_cookie && sc_opaque == &core_cookie);
    assert(sc_ops == expected_ops && sc_detaching == releasing);
    assert(g_mutex_trylock(&sc_lock));
    g_mutex_unlock(&sc_lock);
    if (releasing) {
        assert(!syscall_trap_on);
        for (unsigned abi = 0; abi < SYSCALL_ABI_COUNT; abi++) {
            assert(!sc_watch_all[abi]);
            for (unsigned n = 0; n < SYSCALL_TRAP_NUMS; n++) {
                assert(!sc_watch[abi][n]);
            }
        }
        assert(!syscall_trap_set_consumer(expected_ops, &core_cookie));
        assert(!syscall_trap_clear_consumer(expected_ops, &core_cookie));
    }
    drops++;
    psc_drop(sc, opaque);
}
static void on_abort(CPUState *cpu, int *ev, struct penguin_sc_cmd *cmd, void *opaque)
{
    assert(!cpu && cmd->op == PENGUIN_SC_OP_ABORT && opaque == &callback_cookie);
    /* Every slot, counter and caller fault scratch is clear before the first ABORT. */
    assert(g_mutex_trylock(&sc_lock));
    static const SCSlot zero = {0};
    for (unsigned i = 0; i < SC_SLOTS; i++) {
        assert(!memcmp(&sc_slots[i], &zero, sizeof(zero)));
    }
    assert(!sc_fault_addr && !sc_fault_write);
    assert(!syscall_trap_pending && !sc_held);
    for (unsigned i = 0; i < 2; i++) {
        assert(!cpus[i].syscall_current && !cpus[i].syscall_kmmu && !cpus[i].syscall_inject_abis);
    }
    g_mutex_unlock(&sc_lock);
    aborted++;
}
static void add_run_at(unsigned index, int state, bool with_data)
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
    SyscallTrap sc = { .data = r };
    sc_add(key, 2, 0x4000, 4, &sc, state);
    assert(sc_slots[index].pc && sc_slots[index].state == state);
}
static unsigned hooks_seen[3];
static void later(void *p)
{
    hooks_seen[2]++;
    assert(panda_main_walk_depth > 0 && !closed);
    /* Model the existing unload request: actual dlclose runs after this walk. */
    unload_requested = true;
}
static void remove_later(void *p)
{
    panda_unregister_transient_reset((void *)2);
    panda_unregister_transient_reset(p);
}
static void self_remove(void *p)
{
    hooks_seen[0]++;
    assert(panda_main_walk_depth > 0);
    panda_unregister_transient_reset(p);
    assert(panda_register_transient_reset((void *)3, later, NULL));
}
static void keep(void *p) { hooks_seen[1]++; }
int main(void)
{
    static const SyscallTrapOps ops = { .drop = checked_drop };
    expected_ops = &ops;
    assert(syscall_trap_set_consumer(&ops, &core_cookie));
    psc_cb = on_abort;
    psc_cb_opaque = &callback_cookie;
    sc_watch[0][9] = 3;
    sc_watch_all[2] = 3;
    syscall_trap_on = true;
    for (int state = SC_PENDING; state <= SC_INJECT_RET; state++) {
        PRun *r = g_new0(PRun, 1);
        r->running = true;
        r->wdata = g_malloc(8);
        r->rbuf = g_malloc(8);
        SyscallTrap sc = { .data = r };
        sc_add(state + 1, 2, 0x1000 + state, 4, &sc, state);
    }
    /* Also pending calls with no host data must disappear. */
    SyscallTrap empty = {0};
    sc_add(99, 2, 0x2000, 4, &empty, SC_PENDING);
    assert(syscall_trap_pending == 6 && sc_held == 4);
    cpus[0].syscall_current = cpus[1].syscall_current = 42;
    cpus[0].syscall_kmmu = cpus[1].syscall_kmmu = 7;
    cpus[0].syscall_inject_abis = cpus[1].syscall_inject_abis = 3;
    sc_fault_addr = 123;
    sc_fault_write = true;
    assert(syscall_trap_cancel_all() == 6);
    assert(drops == 5 && aborted == 5 && resolved == 5);
    assert(!sc_fault_addr && !sc_fault_write);
    assert(syscall_trap_cancel_all() == 0);
    assert(drops == 5 && aborted == 5 && resolved == 5);
    assert(sc_ops == &ops && sc_watch[0][9] == 3 && sc_watch_all[2] == 3 && syscall_trap_on);
    /* Registrations survive; new execution can create ordinary pending work. */
    sc_add(1, 2, 0x3000, 4, &empty, SC_PENDING);
    assert(syscall_trap_pending == 1);
    assert(syscall_trap_cancel_all() == 1);

    /* Put either injection after droppable pending/held slots in scan order.
     * Snapshot the entire table and policy, including opaque-owned run pointers. */
    for (int state = SC_INJECT; state <= SC_INJECT_RET; state++) {
        add_run_at(0, SC_PENDING, true);
        add_run_at(1, SC_HELD_ENTER, true);
        add_run_at(2, SC_HELD_RETURN, true);
        add_run_at(SC_SLOTS - 2, state, true);
        add_run_at(SC_SLOTS - 1, SC_PENDING, false);
        SCSlot before[SC_SLOTS];
        uint8_t watches[sizeof(sc_watch)], all[sizeof(sc_watch_all)];
        memcpy(before, sc_slots, sizeof(before));
        memcpy(watches, sc_watch, sizeof(watches));
        memcpy(all, sc_watch_all, sizeof(all));
        CPUState cpu_before[2];
        cpus[0].syscall_current = cpus[1].syscall_current = 42;
        cpus[0].syscall_kmmu = cpus[1].syscall_kmmu = 7;
        cpus[0].syscall_inject_abis = cpus[1].syscall_inject_abis = 3;
        memcpy(cpu_before, cpus, sizeof(cpus));
        sc_fault_addr = 123;
        sc_fault_write = true;
        unsigned old_drops = drops, old_aborts = aborted, old_resolved = resolved;
        uint64_t seq = sc_seq, full = sc_full, stale = sc_stale;
        for (unsigned retry = 0; retry < 2; retry++) {
            assert(!syscall_trap_clear_consumer(&ops, &core_cookie));
            assert(!memcmp(before, sc_slots, sizeof(before)));
            assert(!memcmp(watches, sc_watch, sizeof(watches)));
            assert(!memcmp(all, sc_watch_all, sizeof(all)));
            assert(!memcmp(cpu_before, cpus, sizeof(cpus)));
            assert(syscall_trap_pending == 5 && sc_held == 3);
            assert(sc_seq == seq && sc_full == full && sc_stale == stale);
            assert(sc_ops == &ops && sc_opaque == &core_cookie && !sc_detaching);
            assert(syscall_trap_on && sc_fault_addr == 123 && sc_fault_write);
            assert(drops == old_drops && aborted == old_aborts && resolved == old_resolved);
            assert(!sc_dropped || !sc_dropped->len);
        }
        assert(syscall_trap_cancel_all() == 5);
        assert(drops == old_drops + 4 && aborted == old_aborts + 4 && resolved == old_resolved + 4);
        assert(syscall_trap_cancel_all() == 0);
        assert(drops == old_drops + 4 && aborted == old_aborts + 4 && resolved == old_resolved + 4);
    }
    assert(!syscall_trap_clear_consumer(&ops, (void *)1));
    add_run_at(0, SC_PENDING, true);
    add_run_at(1, SC_HELD_ENTER, true);
    add_run_at(2, SC_HELD_RETURN, true);
    add_run_at(SC_SLOTS - 1, SC_PENDING, false);
    unsigned old_drops = drops, old_aborts = aborted, old_resolved = resolved;
    releasing = true;
    assert(syscall_trap_clear_consumer(&ops, &core_cookie));
    releasing = false;
    assert(drops == old_drops + 3 && aborted == old_aborts + 3 && resolved == old_resolved + 3);
    assert(!syscall_trap_pending && !sc_held && !sc_ops && !sc_opaque && !sc_detaching);
    assert(!syscall_trap_on && !sc_watch[0][9] && !sc_watch_all[2]);
    assert(!syscall_trap_clear_consumer(&ops, &core_cookie));
    assert(syscall_trap_cancel_all() == 0 && drops == old_drops + 3);
    assert(syscall_trap_set_consumer(&ops, &core_cookie));
    assert(!syscall_trap_set_consumer(&ops, (void *)1));

    assert(!panda_register_transient_reset(NULL, keep, NULL));
    assert(!panda_register_transient_reset((void *)99, keep, NULL));
    assert(!panda_register_transient_reset((void *)1, NULL, NULL));
    /* Plugin handles are published before init_plugin; no CPU/BQL is needed yet. */
    bql = false;
    assert(panda_register_transient_reset((void *)1, keep, NULL));
    panda_unregister_transient_reset((void *)1);
    bql = true;
    first_cpu = cpus;
    running = true;
    cpus_paused = false;
    assert(!panda_register_transient_reset((void *)1, keep, NULL));
    cpus_paused = true;
    assert(panda_register_transient_reset((void *)1, keep, NULL));
    panda_unregister_transient_reset((void *)1);
    running = false;
    bql = false;
    assert(!panda_register_transient_reset((void *)1, keep, NULL));
    bql = true;
    current_cpu = cpus;
    assert(!panda_register_transient_reset((void *)1, keep, NULL));
    current_cpu = NULL;
    assert(panda_register_transient_reset((void *)2, keep, NULL));
    assert(panda_register_transient_reset((void *)1, self_remove, (void *)1));
    assert(!panda_register_transient_reset((void *)1, keep, NULL));
    panda_analysis_reset();
    assert(hooks_seen[0] == 1 && hooks_seen[1] == 1 && hooks_seen[2] == 0);
    panda_analysis_reset();
    assert(hooks_seen[0] == 1 && hooks_seen[1] == 2 && hooks_seen[2] == 1);
    panda_unregister_transient_reset((void *)2);
    panda_unregister_transient_reset((void *)3);
    panda_analysis_reset();
    assert(hooks_seen[1] == 2 && hooks_seen[2] == 1 && !panda_main_walk_depth);
    assert(unloads == 3 && closed == 1);
    /* An earlier hook can unregister a later hook without invalidating the walk. */
    assert(panda_register_transient_reset((void *)2, keep, NULL));
    assert(panda_register_transient_reset((void *)1, remove_later, (void *)1));
    panda_analysis_reset();
    assert(hooks_seen[1] == 2 && !panda_transient_resets);
    puts("PASS: mixed pending/held/injected cancellation; late-injection refusal without partial changes; safe mixed drain with retained core/Penguin opaque outside locks; persistent watches, idempotence, reset hooks and deferred unload");
}
'''
with tempfile.TemporaryDirectory(prefix='lifecycle-static-') as temp:
    path = Path(temp)
    (path / 'test.c').write_text(stubs + trap + slots + psc_stubs + psc + cancel + release + checks + hooks + tests)
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'glib-2.0'], text=True))
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-O1', '-g',
                    str(path / 'test.c'), '-o', str(path / 'test'), *flags], check=True)
    subprocess.run([str(path / 'test')], check=True)

    # Compile the actual fast restore body too: synthetic devices make all
    # rejection, dirty/full/compare, timer-order and partial-failure paths cheap.
    fast = (ROOT / 'engine/src/panda/src/panda_fast.c').read_text()
    fast_stubs = r'''
#include <glib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define TARGET_ARM 1
#define RUN_STATE_RESTORE_VM 1
#define SHUTDOWN_CAUSE_SNAPSHOT_LOAD 2
#define DIV_ROUND_UP(a,b) (((a)+(b)-1)/(b))
#define ARM_CPU(c) (c)
#define error_report(...) ((void)0)
typedef struct CPUState { int unused; } CPUState;
static CPUState cpus[2], *first_cpu = cpus;
#define CPU_FOREACH(cpu) for (cpu = cpus; cpu < cpus + 2; cpu++)
typedef struct FastBlock { void *rb; unsigned char *copy; size_t len; bool tracked; } FastBlock;
static GArray *fast_blocks;
static void *fast_dev;
static const char *fast_why;
static struct {
    const char *last_why, *last_mode;
    uint64_t full_restores, compare_restores, restores, last_pages;
    bool last_full;
    int64_t last_ram_us, last_dev_us;
} fast_stats;
static bool supported = true, layout = true, tracking = true, unlogged_dev, running;
static int arm_rc, load_rc;
static unsigned canceled, notified, reset_count, timer_count;
static bool fast_supported(const char *s) { return supported; }
static bool runstate_is_running(void) { return running; }
static void vm_stop(int state) { running = false; }
static bool fast_layout_same(void) { return layout; }
static void memory_global_dirty_log_sync(bool b) {}
static bool vhost_has_unlogged_dev(void) { return unlogged_dev; }
static bool fast_tracking_ok(void) { return tracking; }
static int fast_arm(void) { return arm_rc; }
static unsigned page_size(void) { return 4096; }
static void *qemu_ram_get_host_addr(void *rb) { return rb; }
static void tb_flush__exclusive_or_serial(void) {}
static void fast_rearm(void) {}
static uint64_t fast_compare_restore(FastBlock *b) { memcpy(b->rb, b->copy, b->len); return 1; }
static uint64_t fast_walk(FastBlock *b, bool restore) { return fast_compare_restore(b); }
static void fast_set_sentinels(void) {}
static void fast_tlb_rearm(void) {}
static void panda_cov_restore(void) {}
static void panda_analysis_reset(void)
{
    FastBlock *b = &g_array_index(fast_blocks, FastBlock, 0);
    assert(b->rb && ((unsigned char *)b->rb)[0] == 1);
    canceled++;
}
static int fast_load_devices(void) { assert(canceled > 0); return load_rc; }
static void qemu_system_reset(int reason) { reset_count++; }
static void arm_gt_recalc_el1_timers(CPUState *cpu) { timer_count++; }
static void panda_callbacks_after_loadvm(CPUState *cpu)
{
    assert(cpu == first_cpu && !load_rc && timer_count == 2);
    FastBlock *b = &g_array_index(fast_blocks, FastBlock, 0);
    assert(!memcmp(b->rb, b->copy, b->len));
    notified++;
}
'''
    fast_tests = r'''
int main(void)
{
    unsigned char ram[4096], saved[4096];
    memset(ram, 1, sizeof(ram));
    memset(saved, 2, sizeof(saved));
    FastBlock block = { .rb = ram, .copy = saved, .len = sizeof(ram), .tracked = true };
    supported = false;
    assert(panda_fast_restore() == -1);
    supported = true;
    assert(panda_fast_restore() == -1);
    fast_blocks = g_array_new(false, false, sizeof(FastBlock));
    g_array_append_val(fast_blocks, block);
    fast_dev = saved;
    layout = false;
    assert(panda_fast_restore() == -1);
    layout = true;
    arm_rc = -1;
    assert(panda_fast_restore() == -1);
    assert(!canceled && !notified && !reset_count);
    arm_rc = 0;
    for (int mode = 0; mode < 3; mode++) {
        canceled = notified = timer_count = 0;
        memset(ram, 1, sizeof(ram));
        tracking = mode != 1;
        unlogged_dev = mode == 2;
        assert(panda_fast_restore() == 0);
        assert(canceled == 1 && notified == 1 && timer_count == 2);
        assert(!strcmp(fast_stats.last_mode, mode == 0 ? "dirty" : mode == 1 ? "full" : "compare"));
    }
    canceled = notified = timer_count = 0;
    memset(ram, 1, sizeof(ram));
    load_rc = -1;
    assert(panda_fast_restore() == -1);
    assert(canceled == 1 && !notified && !timer_count && reset_count == 1);
    g_array_free(fast_blocks, true);
    puts("PASS: fast rejection/no-cancel, dirty/full/compare exactly-once notification after RAM/devices/timers, partial failure/reset without success");
}
'''
    (path / 'fast.c').write_text(fast_stubs + function(fast, 'panda_fast_restore') + fast_tests)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-O1', '-g',
                    str(path / 'fast.c'), '-o', str(path / 'fast'), *flags], check=True)
    subprocess.run([str(path / 'fast')], check=True)
