"""Host stubs for compiling the syscall trap core (system/syscall-trap.c), the PANDA syscall events
(panda/src/syscall-events.c) and the event registry (panda/src/events.c) without QEMU.

The stubs are QEMU's own interfaces reduced to what those files use: atomics as plain accesses (the
tests are single-threaded), a non-recursive lock that fails on re-entry, an RCU whose frees wait
for the test's rcu_flush(), and CPUState with the trap's fields. The real headers come after the
stubs on the include path: system/syscall-trap.h, panda/events.h, panda/events-internal.h,
panda/admission.h and panda/syscall-events.h are the engine's.

    core_source()   the trap core: syscall-trap.c up to its guest-memory helpers, to #include in a
                    test (its statics are then visible to the test)
    write_stubs(d)  the stub headers, in d
    cflags(d)       compiler flags: ASan, UBSan, warnings, both include directories, GLib
"""
import os
import shlex
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "engine/src"

STUBS = {
"qemu/osdep.h": r'''
#pragma once
#include <glib.h>
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ROUND_UP(n, d) ((((n) + (d) - 1) / (d)) * (d))
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define QEMU_BUILD_BUG_ON(x) _Static_assert(!(x), #x)
#define QEMU_UNINITIALIZED
#define unlikely(x) __builtin_expect(!!(x), 0)
#define likely(x) __builtin_expect(!!(x), 1)
typedef struct CPUState CPUState;
''',
"qemu/atomic.h": r'''
#pragma once
#define qatomic_read(p) (*(p))
#define qatomic_set(p, v) (*(p) = (v))
#define qatomic_inc(p) (++*(p))
#define qatomic_dec(p) (--*(p))
#define qatomic_fetch_inc(p) ((*(p))++)
#define qatomic_fetch_dec(p) ((*(p))--)
#define qatomic_rcu_read(p) (*(p))
#define qatomic_rcu_set(p, v) (*(p) = (v))
''',
"qemu/rcu.h": r'''
#pragma once
/* Frees wait for the test's rcu_flush(), so a held snapshot stays readable until then. */
struct rcu_head { struct rcu_head *next; void (*func)(void *); };
extern struct rcu_head *rcu_deferred;
static inline void call_rcu1(struct rcu_head *h, void (*f)(void *))
{
    h->func = f; h->next = rcu_deferred; rcu_deferred = h;
}
#define call_rcu(head, func, field) call_rcu1(&(head)->field, (void (*)(void *))(func))
#define g_free_rcu(obj, field) call_rcu1(&(obj)->field, g_free)
#define RCU_READ_LOCK_GUARD() do { } while (0)
static inline void rcu_flush(void)
{
    while (rcu_deferred) {
        struct rcu_head *h = rcu_deferred;
        rcu_deferred = h->next;
        h->func(h);
    }
}
''',
"qemu/thread.h": r'''
#pragma once
/* Not recursive: taking a lock twice is a deadlock in the engine, an assertion here. */
typedef struct { int held, init; } QemuMutex;
static inline void qemu_mutex_init(QemuMutex *m) { m->held = 0; m->init = 1; }
static inline void qemu_mutex_lock(QemuMutex *m) { g_assert(m->init && !m->held); m->held = 1; }
static inline void qemu_mutex_unlock(QemuMutex *m) { g_assert(m->held); m->held = 0; }
''',
"qemu/lockable.h": r'''
#pragma once
static inline void lock_release_(QemuMutex **m) { qemu_mutex_unlock(*m); }
static inline QemuMutex *lock_take_(QemuMutex *m) { qemu_mutex_lock(m); return m; }
#define QEMU_LOCK_GUARD(m) \
    QemuMutex *lock_guard_ __attribute__((cleanup(lock_release_))) = lock_take_(m)
''',
"qemu/error-report.h": r'''
#pragma once
#define error_report(...) (fprintf(stderr, "error: " __VA_ARGS__), fputc('\n', stderr))
#define warn_report(...) (fprintf(stderr, "warn: " __VA_ARGS__), fputc('\n', stderr))
''',
"qemu/host-utils.h": "#pragma once\n",
"exec/vaddr.h": "#pragma once\ntypedef uint64_t vaddr;\n",
"exec/target_page.h": "#pragma once\n",
"system/tcg.h": "#pragma once\n",
"system/address-spaces.h": "#pragma once\n",
"system/replay-effect.h": "#pragma once\n",
"panda/guest-write.h": "#pragma once\n",
"hw/core/cpu.h": r'''
#pragma once
struct CPUState {
    int cpu_index;
    uint64_t syscall_current;
    uint32_t syscall_kmmu;
    uint32_t syscall_inject_abis;
};
#define NCPUS 2
extern CPUState cpus[NCPUS];
extern CPUState *first_cpu;
#define CPU_FOREACH(cpu) for ((cpu) = cpus; (cpu) < cpus + NCPUS; (cpu)++)
''',
"system/replay-analysis.h": r'''
#pragma once
extern bool env_writes;     /* replay_analysis_write_allowed() */
static inline bool replay_analysis_write_allowed(void) { return env_writes; }
''',
"system/analysis-policy.h": r'''
#pragma once
#define ANALYSIS_CAP_ANALYSIS 1u
extern bool env_frozen, env_analysis;
static inline bool analysis_policy_is_frozen(void) { return env_frozen; }
static inline bool analysis_policy_has(uint32_t c) { return env_analysis && c == ANALYSIS_CAP_ANALYSIS; }
''',
"system/kvm.h": r'''
#pragma once
extern bool env_kvm;
static inline bool kvm_enabled(void) { return env_kvm; }
''',
"system/replay.h": r'''
#pragma once
enum { REPLAY_MODE_NONE, REPLAY_MODE_RECORD, REPLAY_MODE_PLAY };
extern int replay_mode;
''',
"panda/plugin.h": r'''
#pragma once
enum { PANDA_MODE_ANALYSIS = 1, PANDA_MODE_FUZZ = 2, PANDA_MODE_RR = 4, PANDA_MODE_KVM = 8 };
''',
}

# Definitions the stubs declare; one test translation unit includes this.
GLOBALS = r'''
struct rcu_head *rcu_deferred;
CPUState cpus[NCPUS] = { { .cpu_index = 0 }, { .cpu_index = 1 } };
CPUState *first_cpu = cpus;
bool env_writes = true, env_frozen = true, env_analysis = true, env_kvm;
int replay_mode;
'''


def core_source():
    """syscall-trap.c without its guest-memory helpers (they need the TLB)."""
    text = (SRC / "system/syscall-trap.c").read_text()
    return text[:text.index("bool syscall_trap_read(CPUState *cpu")]


def write_stubs(d):
    d = Path(d)
    for name, body in STUBS.items():
        (d / name).parent.mkdir(parents=True, exist_ok=True)
        (d / name).write_text(body)


def cflags(d, sanitize=True):
    glib = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0"],
                                               text=True))
    san = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"] if sanitize else []
    return ["-std=gnu11", "-O1", "-g", "-Wall", "-Wno-unused-function", "-Wno-unused-variable",
            *san, "-I", str(d), "-I", str(SRC / "include"), *glib]


def cc():
    return os.environ.get("CC", "cc")
