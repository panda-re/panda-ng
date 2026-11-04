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


# The syscall trap's own cancellation (pending, held and injected calls, every consumer's drop
# outside the lock, the last consumer's removal) is tested on the real core in
# tests/static/syscall_trap.py, and Penguin's drop -> ABORT -> free chain with it in
# tests/static/penguin_detach.py. Here the reset path only has to call it.
callbacks = (ROOT / 'engine/src/panda/src/callbacks.c').read_text()
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
static unsigned cancels;
static unsigned syscall_trap_cancel_all(void) { cancels++; return 0; }
'''
hooks =callbacks[callbacks.index('typedef struct PandaTransientReset'):callbacks.index('static bool panda_on_main_thread')]
# Failed initialization and normal unload share this central removal path.
close = function(callbacks, 'dlclose_plugin')
assert close.index('panda_unregister_transient_reset(plugin)') < close.index('panda_delete_plugin(plugin_idx)') < close.index('dlclose(plugin)')
# The registry drops the plugin's subscriptions (functions in its code) before the dlclose.
assert close.index('panda_events_owner_gone(plugin)') < close.index('dlclose(plugin)')
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
static void panda_events_owner_gone(void *owner) { (void)owner; }
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
    assert(!panda_register_transient_reset(NULL, keep, NULL));
    assert(!panda_register_transient_reset((void *)99, keep, NULL));
    assert(!panda_register_transient_reset((void *)1, NULL, NULL));
    /* Plugin handles are published before init_plugin; no CPU/BQL is needed yet. */
    bql = false;
    assert(panda_register_transient_reset((void *)1, keep, NULL));
    panda_unregister_transient_reset((void *)1);
    bql = true;
    first_cpu = cpus;
    /* A plugin loaded mid-run registers with vCPUs running, from a vCPU's own
     * callback or off the main thread without the BQL. */
    running = true;
    cpus_paused = false;
    assert(panda_register_transient_reset((void *)1, keep, NULL));
    assert(!panda_register_transient_reset((void *)1, keep, NULL));
    cpus_paused = true;
    panda_unregister_transient_reset((void *)1);
    running = false;
    bql = false;
    assert(panda_register_transient_reset((void *)1, keep, NULL));
    bql = true;
    panda_unregister_transient_reset((void *)1);
    current_cpu = cpus;
    assert(panda_register_transient_reset((void *)1, keep, NULL));
    /* Unregistering stays a main-thread/BQL operation. */
    panda_unregister_transient_reset((void *)1);
    current_cpu = NULL;
    panda_unregister_transient_reset((void *)1);
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
    /* each analysis reset cancels the syscall trap's pending and held calls */
    assert(cancels == 4);
    puts("PASS: reset hooks, self/later removal, deferred unload; the reset cancels the syscall trap");
}
'''
with tempfile.TemporaryDirectory(prefix='lifecycle-static-') as temp:
    path = Path(temp)
    (path / 'test.c').write_text(stubs + checks + hooks + tests)
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
        /* Fast restore cancels transient analysis but is not an after_loadvm. */
        assert(canceled == 1 && !notified && timer_count == 2);
        FastBlock *b = &g_array_index(fast_blocks, FastBlock, 0);
        assert(!memcmp(b->rb, b->copy, b->len));
        assert(!strcmp(fast_stats.last_mode, mode == 0 ? "dirty" : mode == 1 ? "full" : "compare"));
    }
    canceled = notified = timer_count = 0;
    memset(ram, 1, sizeof(ram));
    load_rc = -1;
    assert(panda_fast_restore() == -1);
    assert(canceled == 1 && !notified && !timer_count && reset_count == 1);
    g_array_free(fast_blocks, true);
    puts("PASS: fast rejection/no-cancel, dirty/full/compare cancellation without after_loadvm, RAM/devices/timers restored, partial failure/reset without success");
}
'''
    (path / 'fast.c').write_text(fast_stubs + function(fast, 'panda_fast_restore') + fast_tests)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-O1', '-g',
                    str(path / 'fast.c'), '-o', str(path / 'fast'), *flags], check=True)
    subprocess.run([str(path / 'fast')], check=True)
