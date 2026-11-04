/* Records system call events for the syscall tests (tests/syscall_cover.py, tests/syscall_truth.py):
 * every call whose pc is in a range, its enter and its return, in a buffer Python reads through the
 * functions below (cffi on this library, which QEMU loaded with -plugin: the same handle).
 *
 * It is a QEMU TCG plugin and observes the engine's syscall trap through the plugin syscall API
 * (qemu_plugin_register_syscall_event_cb, qemu_plugin_syscall_watch): it changes nothing in the
 * calls (arguments, return values, want_return and keep stay as the trap set them). Only one
 * plugin may hold the syscall callbacks; loading this one fails if another has them.
 * Built by the flake as packages.<system>.sctrace (in test-plugins), linked against each target's
 * engine so that it binds to the engine pandare2 loads. */
#include <qemu-plugin.h>

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

/* What Python reads: one per event. */
typedef struct SctraceEvent {
    uint32_t kind;          /* 1 enter, 2 return */
    int32_t abi;            /* enum qemu_plugin_syscall_abi */
    int32_t cpu;            /* vCPU index */
    uint32_t flags;         /* return: 1 restart */
    uint64_t id;            /* the call's: its enter's, at its return; from 1 */
    int64_t num;
    uint64_t args[6];
    uint64_t pc;            /* past the trap instruction */
    int64_t ret;            /* return: the value */
} SctraceEvent;

static qemu_plugin_id_t self;
static bool registered;
static SctraceEvent *events;
static atomic_uint_fast64_t n_events, n_lost, next_id;
static uint64_t capacity, pc_lo, pc_hi;
static unsigned abis;       /* the ABIs watched, a bit each */
static atomic_bool on;

static void record(uint32_t kind, unsigned vcpu, const struct qemu_plugin_syscall *sc, uint64_t id,
                   uint32_t flags)
{
    uint64_t i = atomic_fetch_add(&n_events, 1);
    SctraceEvent *e;

    if (i >= capacity) {
        atomic_fetch_add(&n_lost, 1);
        return;
    }
    e = &events[i];
    e->kind = kind;
    e->abi = sc->abi;
    e->cpu = vcpu;
    e->flags = flags;
    e->id = id;
    e->num = sc->num;
    memcpy(e->args, sc->args, sizeof(e->args));
    e->pc = sc->pc;
    e->ret = sc->ret;
}

static void on_enter(qemu_plugin_id_t id, unsigned vcpu, struct qemu_plugin_syscall *sc, void *u)
{
    if (!atomic_load(&on) || sc->pc < pc_lo || sc->pc >= pc_hi) {
        return;
    }
    sc->cookie = atomic_fetch_add(&next_id, 1) + 1;
    record(1, vcpu, sc, sc->cookie, 0);
}

static void on_return(qemu_plugin_id_t id, unsigned vcpu, struct qemu_plugin_syscall *sc, void *u)
{
    if (!atomic_load(&on) || !sc->cookie) {
        return;                 /* a call from before the window, or out of the range */
    }
    record(2, vcpu, sc, sc->cookie, sc->restart ? 1 : 0);
}

static void watch(unsigned flags)
{
    for (int abi = 0; abi < QEMU_PLUGIN_SYSCALL_ABI_COUNT; abi++) {
        if (abis & (1u << abi)) {
            qemu_plugin_syscall_watch(self, abi, -1, flags);
        }
    }
}

/* Record the calls with pc in [lo, hi) of the ABIs in abi_mask (bit per enum
 * qemu_plugin_syscall_abi), up to cap events. 0, or -1 (not registered), -2 (no memory). */
QEMU_PLUGIN_EXPORT int sctrace_start(uint64_t lo, uint64_t hi, uint64_t cap, unsigned abi_mask)
{
    if (!registered) {
        return -1;
    }
    atomic_store(&on, false);
    free(events);
    events = calloc(cap ? cap : 1, sizeof(*events));
    if (!events) {
        capacity = 0;
        return -2;
    }
    capacity = cap;
    atomic_store(&n_events, 0);
    atomic_store(&n_lost, 0);
    pc_lo = lo;
    pc_hi = hi;
    abis = abi_mask;
    watch(QEMU_PLUGIN_SYSCALL_ENTER | QEMU_PLUGIN_SYSCALL_RETURN);
    atomic_store(&on, true);
    return 0;
}

/* Stop recording and watching; the buffer stays readable. */
QEMU_PLUGIN_EXPORT void sctrace_stop(void)
{
    atomic_store(&on, false);
    watch(0);
}

QEMU_PLUGIN_EXPORT uint64_t sctrace_count(void)
{
    uint64_t n = atomic_load(&n_events);
    return n < capacity ? n : capacity;
}

QEMU_PLUGIN_EXPORT uint64_t sctrace_lost(void)
{
    return atomic_load(&n_lost);
}

/* Copy events [first, first + n) to out; how many were copied. */
QEMU_PLUGIN_EXPORT uint64_t sctrace_read(uint64_t first, uint64_t n, SctraceEvent *out)
{
    uint64_t have = sctrace_count();

    if (first >= have) {
        return 0;
    }
    if (n > have - first) {
        n = have - first;
    }
    memcpy(out, events + first, n * sizeof(*out));
    return n;
}

/* The trap's bookkeeping (qemu_plugin_syscall_stats). */
QEMU_PLUGIN_EXPORT void sctrace_stats(uint64_t *pending, uint64_t *full, uint64_t *stale)
{
    qemu_plugin_syscall_stats(pending, full, stale);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc,
                                           char **argv)
{
    self = id;
    registered = qemu_plugin_register_syscall_event_cb(id, on_enter, on_return, NULL);
    return registered ? 0 : -1;
}
