/* Records system call events for the syscall tests (tests/syscall_cover.py, ...): every call whose
 * pc is in a range, its enter and its return, in a buffer Python reads through the functions below
 * (cffi on the plugin's library, as tests/bench_cb.py reads cbcount).
 *
 * The events come from the engine's syscall trap (system/syscall-trap.h), as an observer.
 * The plugin SDK doesn't ship that header: the two structs below are copies of it and must change
 * with it. Built by the flake as packages.<system>.sctrace (in test-plugins). */
#include "panda.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct SyscallTrap {
    int abi;
    int64_t num;
    uint64_t args[6];
    uint64_t pc;
    int64_t ret;
    uint64_t percpu;
    bool skip, want_return, restart, skipped, keep, resumed;
    bool fault, fault_write;
    uint64_t fault_addr;
    bool inject;
    int64_t inject_num;
    uint64_t inject_args[6];
    bool injected;
    int64_t inject_ret;
    uint64_t task;
    void *data;
} SyscallTrap;

typedef struct SyscallTrapOps {
    void (*enter)(CPUState *cpu, SyscallTrap *sc, void *opaque);
    void (*ret)(CPUState *cpu, SyscallTrap *sc, void *opaque);
    void (*drop)(SyscallTrap *sc, void *opaque);
} SyscallTrapOps;

typedef struct SyscallTrapConsumer SyscallTrapConsumer;

SyscallTrapConsumer *syscall_trap_add_consumer(const SyscallTrapOps *ops, void *opaque,
                                               unsigned flags) __attribute__((weak));
bool syscall_trap_remove_consumer(SyscallTrapConsumer *c) __attribute__((weak));
bool syscall_trap_watch(SyscallTrapConsumer *c, int abi, int64_t num, unsigned flags)
    __attribute__((weak));

#define SYSCALL_TRAP_OBSERVER 1

#define SYSCALL_ABI_COUNT 10
#define TRAP_ENTER 1
#define TRAP_RETURN 2

/* What Python reads: one per event. */
typedef struct SctraceEvent {
    uint32_t kind;          /* 1 enter, 2 return, 3 dropped (no return will come) */
    int32_t abi;
    int32_t cpu;
    uint32_t flags;         /* return: 1 restart; enter: 2 resumed */
    uint64_t id;            /* the call's: its enter's, at its return and drop; from 1 */
    int64_t num;
    uint64_t args[6];
    uint64_t pc;
    int64_t ret;            /* return: the value */
} SctraceEvent;

static SctraceEvent *events;
static atomic_uint_fast64_t n_events, n_lost, next_id;
static uint64_t capacity, pc_lo, pc_hi;
static bool on, follow;
static atomic_uint_fast64_t followed;    /* the address space of the last call in range */

static SctraceEvent *slot(void)
{
    uint64_t i = atomic_fetch_add(&n_events, 1);

    if (i >= capacity) {
        atomic_fetch_add(&n_lost, 1);
        return NULL;
    }
    return &events[i];
}

static void record(uint32_t kind, int cpu, const SyscallTrap *sc, uint64_t id, uint32_t flags)
{
    SctraceEvent *e = slot();

    if (!e) {
        return;
    }
    e->kind = kind;
    e->abi = sc->abi;
    e->cpu = cpu;
    e->flags = flags;
    e->id = id;
    e->num = sc->num;
    memcpy(e->args, sc->args, sizeof(e->args));
    e->pc = sc->pc;
    e->ret = sc->ret;
}

static void on_enter(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    uint64_t id;

    if (!on) {
        return;
    }
    if (sc->pc >= pc_lo && sc->pc < pc_hi) {
        if (follow) {
            atomic_store(&followed, panda_current_asid(cpu));
        }
    } else if (!follow || !atomic_load(&followed) ||
               panda_current_asid(cpu) != atomic_load(&followed)) {
        return;
    }
    id = atomic_fetch_add(&next_id, 1) + 1;
    record(1, cpu->cpu_index, sc, id, sc->resumed ? 2 : 0);
    sc->want_return = true;
    sc->data = (void *)(uintptr_t)id;
}

static void on_return(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    if (sc->data) {
        record(2, cpu->cpu_index, sc, (uintptr_t)sc->data, sc->restart ? 1 : 0);
        sc->data = NULL;    /* the trap calls drop() for any data still set after the return */
    }
}

static void on_drop(SyscallTrap *sc, void *opaque)
{
    if (sc->data) {
        record(3, -1, sc, (uintptr_t)sc->data, 0);
    }
}

static const SyscallTrapOps ops = { on_enter, on_return, on_drop };
static SyscallTrapConsumer *consumer;

/* Record the calls with lo <= pc < hi, up to cap events (later ones are counted as lost).
 * follow: also every call in the address space of the last call in range, wherever its pc is
 * (an i386 task's calls through the vDSO). Clears what was recorded. 0 on success; -1 without the
 * trap. */
int sctrace_start(uint64_t lo, uint64_t hi, uint64_t cap, int follow_asid)
{
    if (!syscall_trap_add_consumer || !syscall_trap_watch) {
        return -1;
    }
    if (!consumer) {
        consumer = syscall_trap_add_consumer(&ops, NULL, SYSCALL_TRAP_OBSERVER);
    }
    on = false;
    free(events);
    events = calloc(cap, sizeof(*events));
    capacity = events ? cap : 0;
    atomic_store(&n_events, 0);
    atomic_store(&n_lost, 0);
    pc_lo = lo;
    pc_hi = hi;
    follow = follow_asid;
    atomic_store(&followed, 0);
    for (int abi = 0; abi < SYSCALL_ABI_COUNT; abi++) {
        syscall_trap_watch(consumer, abi, -1, TRAP_ENTER | TRAP_RETURN);
    }
    on = true;
    return 0;
}

/* Stop recording new calls (returns of recorded calls are still recorded). */
void sctrace_stop(void)
{
    on = false;
}

uint64_t sctrace_count(void)
{
    uint64_t n = atomic_load(&n_events);
    return n < capacity ? n : capacity;
}

uint64_t sctrace_lost(void)
{
    return atomic_load(&n_lost);
}

/* Copies events [first, first + n) to out; returns how many. */
uint64_t sctrace_read(uint64_t first, uint64_t n, SctraceEvent *out)
{
    uint64_t count = sctrace_count();

    if (first >= count) {
        return 0;
    }
    if (n > count - first) {
        n = count - first;
    }
    memcpy(out, &events[first], n * sizeof(*out));
    return n;
}

bool init_plugin(void *self)
{
    return true;
}

void uninit_plugin(void *self)
{
    on = false;
    if (consumer && syscall_trap_remove_consumer(consumer)) {
        consumer = NULL;
    }
}
