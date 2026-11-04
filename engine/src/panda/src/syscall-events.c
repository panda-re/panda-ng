/*
 * syscall.enter and syscall.return (include/panda/syscall-events.h): the syscall trap's
 * consumers for PANDA's event registry.
 *
 * The registry keeps the subscriptions, their states and admission; this file keeps their
 * watches, and dispatches itself so that a subscription runs only for the numbers it watches.
 * Each subscription kind (early observers, mutators, observers) is one trap consumer, which
 * exists only while that kind has an active subscription. The active subscriptions of a kind
 * are an RCU snapshot; a call keeps the snapshot it started with, with one byte per entry (does
 * it want the return) and the entries' instance data, until its return. Call records come from
 * a per-thread pool, so a watched call does not allocate once the pool is warm.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/atomic.h"
#include "qemu/rcu.h"
#include "qemu/thread.h"
#include "qemu/lockable.h"
#include "qemu/error-report.h"
#include "hw/core/cpu.h"
#include "system/kvm.h"
#include "system/replay.h"
#include "system/replay-analysis.h"
#include "system/syscall-trap.h"
#include "panda/plugin.h"
#include "panda/events-internal.h"
#include "panda/syscall-events.h"

QEMU_BUILD_BUG_ON(sizeof(PandaSyscall) != 96);
QEMU_BUILD_BUG_ON(offsetof(PandaSyscall, flags) != 88);
QEMU_BUILD_BUG_ON(PANDA_SC_ENTER != SYSCALL_TRAP_ENTER || PANDA_SC_RETURN != SYSCALL_TRAP_RETURN);

enum { K_ORIG, K_MUT, K_OBS, K_N };

#define L1_WHAT (PANDA_SC_ENTER | PANDA_SC_RETURN | PANDA_SC_NORETURN_TOO)
#define L1_POOL_MAX 64

typedef struct L1Sub {
    struct rcu_head rcu;
    panda_sub sub;
    panda_event_id ev;
    bool ret_only;              /* a syscall.return subscription */
    int kind;
    int priority;
    uint64_t seq;
    panda_event_fn fn;
    void *ctx;
    size_t data_size;
    bool active;                /* in its kind's published snapshot */
    bool seen;                  /* rebuild scratch */
    int refs;                   /* the table's, and each snapshot's */
    uint8_t all[SYSCALL_ABI_COUNT];
    uint8_t *nums[SYSCALL_ABI_COUNT];   /* SYSCALL_TRAP_NUMS each, once watched */
} L1Sub;

typedef struct L1Entry {
    L1Sub *s;
    size_t off;                 /* its instance data in a call record */
    size_t size;                /* and its size, as of the snapshot */
} L1Entry;

typedef struct L1Snap {
    struct rcu_head rcu;
    int refs;                   /* published, and one per call */
    unsigned n;
    size_t block;               /* the size of a call record */
    L1Entry e[];
} L1Snap;

typedef struct L1Call {
    struct L1Call *next;        /* in a pool */
    size_t cap;
    L1Snap *snap;
    int holder;                 /* the entry that holds the call, or -1 */
    uint8_t want[];             /* per entry: deliver the return; instance data follows */
} L1Call;

/* the dispatch in progress on this thread: what the mutator calls act on */
typedef struct L1Ctx {
    PandaSyscall a;
    SyscallTrap *sc;
    CPUState *cpu;
    L1Call *call;
    L1Sub *s;
    unsigned i;
    int kind;
    bool at_return;
    bool skipped;               /* the return of a skipped call */
    bool held;                  /* this callback asked for a fault or an injection */
} L1Ctx;

static panda_event_id l1_ev[2];         /* enter, return */
static QemuMutex l1_lock;
static GHashTable *l1_subs;             /* panda_sub -> L1Sub */
static L1Snap *l1_snap[K_N];            /* RCU */
static SyscallTrapConsumer *l1_cons[K_N];
static uint8_t l1_pall[K_N][SYSCALL_ABI_COUNT];
static uint8_t l1_pnums[K_N][SYSCALL_ABI_COUNT][SYSCALL_TRAP_NUMS];

static __thread L1Ctx *l1_cur;
static __thread const char *l1_why;
static __thread L1Call *l1_pool;
static __thread unsigned l1_pool_n;

/* --- exit calls ------------------------------------------------------------------------ */

/* exit and exit_group: their return is not tracked unless a subscription opts in */
static bool l1_noreturn(int abi, int64_t num)
{
    switch (abi) {
    case SYSCALL_ABI_X86_64:        return num == 60 || num == 231;
    case SYSCALL_ABI_I386:          return num == 1 || num == 252;
    case SYSCALL_ABI_AARCH64:
    case SYSCALL_ABI_RISCV64:
    case SYSCALL_ABI_RISCV32:
    case SYSCALL_ABI_LOONGARCH64:   return num == 93 || num == 94;
    case SYSCALL_ABI_ARM:
    case SYSCALL_ABI_ARM_OABI:      return num == 1 || num == 248;
    case SYSCALL_ABI_MIPS:          return num == 4001 || num == 4246;
    case SYSCALL_ABI_PPC64:
    case SYSCALL_ABI_PPC:           return num == 1 || num == 234;
    }
    return false;
}

static inline unsigned l1_w(const L1Sub *s, int abi, int64_t num)
{
    unsigned w = s->all[abi];

    if (s->nums[abi] && num >= 0 && num < SYSCALL_TRAP_NUMS) {
        w |= s->nums[abi][num];
    }
    return w;
}

static inline bool l1_wants_return(unsigned w, int abi, int64_t num)
{
    return (w & PANDA_SC_RETURN) &&
           ((w & PANDA_SC_NORETURN_TOO) || !l1_noreturn(abi, num));
}

/* --- lifetimes ------------------------------------------------------------------------- */

static void l1_sub_free(L1Sub *s)
{
    for (int abi = 0; abi < SYSCALL_ABI_COUNT; abi++) {
        g_free(s->nums[abi]);
    }
    g_free(s);
}

static void l1_sub_unref(L1Sub *s)
{
    if (qatomic_fetch_dec(&s->refs) == 1) {
        if (panda_events_rcu_ready()) {
            call_rcu(s, l1_sub_free, rcu);
        } else {
            l1_sub_free(s);
        }
    }
}

static void l1_snap_free(L1Snap *snap)
{
    for (unsigned i = 0; i < snap->n; i++) {
        l1_sub_unref(snap->e[i].s);
    }
    g_free(snap);
}

static void l1_snap_unref(L1Snap *snap)
{
    if (snap && qatomic_fetch_dec(&snap->refs) == 1) {
        if (panda_events_rcu_ready()) {
            call_rcu(snap, l1_snap_free, rcu);
        } else {
            l1_snap_free(snap);
        }
    }
}

static L1Call *l1_call_new(L1Snap *snap)
{
    L1Call *c = NULL;

    for (L1Call **pp = &l1_pool; *pp; pp = &(*pp)->next) {
        if ((*pp)->cap >= snap->block) {
            c = *pp;
            *pp = c->next;
            l1_pool_n--;
            break;
        }
    }
    if (!c) {
        c = g_malloc(snap->block);
        c->cap = snap->block;
    }
    memset(c->want, 0, snap->block - offsetof(L1Call, want));
    c->next = NULL;
    c->snap = snap;
    c->holder = -1;
    qatomic_inc(&snap->refs);
    return c;
}

static void l1_call_free(L1Call *c)
{
    l1_snap_unref(c->snap);
    c->snap = NULL;
    if (l1_pool_n < L1_POOL_MAX) {
        c->next = l1_pool;
        l1_pool = c;
        l1_pool_n++;
    } else {
        g_free(c);
    }
}

static inline void *l1_data(L1Call *c, unsigned i)
{
    return c->snap->e[i].size ? (char *)c + c->snap->e[i].off : NULL;
}

/* --- dispatch -------------------------------------------------------------------------- */

static void l1_args(L1Ctx *x, const SyscallTrap *sc)
{
    x->a.size = sizeof(PandaSyscall);
    x->a.abi = sc->abi;
    x->a.num = sc->num;
    memcpy(x->a.args, sc->args, sizeof(x->a.args));
    x->a.pc = sc->pc;
    x->a.call_id = sc->id;
}

static void l1_call_sub(L1Ctx *x, unsigned i, uint32_t flags, int64_t ret)
{
    L1Ctx *prev = l1_cur;
    L1Sub *s = x->call->snap->e[i].s;

    x->s = s;
    x->i = i;
    x->held = false;
    memcpy(x->a.args, x->sc->args, sizeof(x->a.args));
    x->a.ret = ret;
    x->a.flags = flags;
    l1_cur = x;
    s->fn(s->ctx, &x->a, l1_data(x->call, i));
    l1_cur = prev;
}

static void l1_enter(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    int kind = (intptr_t)opaque;
    L1Call *call = sc->resumed ? sc->data : NULL;
    L1Ctx x QEMU_UNINITIALIZED;
    unsigned from = 0;
    bool want = false;

    if (call) {
        from = call->holder;
        call->holder = -1;
    } else {
        L1Snap *snap = qatomic_rcu_read(&l1_snap[kind]);
        bool any = false;

        if (sc->data) {
            return;             /* not one of ours */
        }
        for (unsigned i = 0; snap && i < snap->n && !any; i++) {
            any = l1_w(snap->e[i].s, sc->abi, sc->num) != 0;
        }
        if (!any) {
            sc->want_return = false;
            return;
        }
        call = l1_call_new(snap);
    }
    x.sc = sc;
    x.cpu = cpu;
    x.call = call;
    x.kind = kind;
    x.at_return = x.skipped = false;
    l1_args(&x, sc);
    for (unsigned i = from; i < call->snap->n; i++) {
        L1Sub *s = call->snap->e[i].s;
        bool me = sc->resumed && call == sc->data && i == from;
        uint32_t flags = sc->skip ? PANDA_SC_SKIPPED : 0;
        int64_t ret = sc->skip ? sc->ret : 0;

        if (!qatomic_read(&s->active)) {
            continue;           /* unsubscribed or disabled since: as if it returned */
        }
        if (me) {
            flags |= PANDA_SC_RESUMED;
            if (sc->injected) {
                flags |= PANDA_SC_INJECTED;
                ret = sc->inject_ret;
            }
        } else {
            unsigned w = l1_w(s, sc->abi, sc->num);

            call->want[i] = l1_wants_return(w, sc->abi, sc->num);
            if (!(w & PANDA_SC_ENTER)) {
                continue;
            }
        }
        l1_call_sub(&x, i, flags, ret);
        if (x.held) {
            call->holder = i;
            sc->data = call;
            sc->want_return = true;
            return;
        }
    }
    for (unsigned i = 0; i < call->snap->n; i++) {
        want |= call->want[i];
    }
    if (want) {
        sc->data = call;
        sc->want_return = true;
    } else {
        l1_call_free(call);
        sc->data = NULL;
        sc->want_return = false;
    }
}

static void l1_ret(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    L1Call *call = sc->data;
    L1Ctx x QEMU_UNINITIALIZED;
    unsigned from = 0;

    if (!call) {
        return;
    }
    if (sc->resumed && call->holder >= 0) {
        from = call->holder;
    }
    call->holder = -1;
    x.sc = sc;
    x.cpu = cpu;
    x.call = call;
    x.kind = (intptr_t)opaque;
    x.at_return = true;
    x.skipped = sc->skip;
    l1_args(&x, sc);
    for (unsigned i = from; i < call->snap->n; i++) {
        L1Sub *s = call->snap->e[i].s;
        bool me = sc->resumed && i == from;
        uint32_t flags = PANDA_SC_AT_RETURN | (sc->restart ? PANDA_SC_RESTART : 0) |
                         (sc->skip ? PANDA_SC_SKIPPED : 0);
        int64_t ret = sc->ret;

        if (!call->want[i] || !qatomic_read(&s->active)) {
            continue;
        }
        if (me) {
            flags |= PANDA_SC_RESUMED;
            if (sc->injected) {
                flags |= PANDA_SC_INJECTED;
                ret = sc->inject_ret;
            }
        }
        l1_call_sub(&x, i, flags, ret);
        if (x.held) {
            call->holder = i;
            return;             /* held: the data stays with the call */
        }
    }
    l1_call_free(call);
    sc->data = NULL;
}

static void l1_drop(SyscallTrap *sc, void *opaque)
{
    if (sc->data) {
        l1_call_free(sc->data);
    }
}

static const SyscallTrapOps l1_ops = {
    .enter = l1_enter,
    .ret = l1_ret,
    .drop = l1_drop,
    .flags = SYSCALL_TRAP_OPS_SKIPPED_RETURN,
};

/* --- mutators -------------------------------------------------------------------------- */

static bool l1_refuse(const char *why)
{
    l1_why = why;
    return false;
}

/* the dispatch @sc belongs to, if the caller may change the call now */
static L1Ctx *l1_mutable(PandaSyscall *sc)
{
    L1Ctx *x = l1_cur;

    l1_why = NULL;
    if (!x || &x->a != sc) {
        l1_why = "not called from a syscall event callback for this call";
    } else if (x->kind != K_MUT) {
        l1_why = "an observer cannot change the call: subscribe with PANDA_SUB_MUTATE";
    } else if (kvm_enabled()) {
        l1_why = "syscall events need TCG: under KVM there are no syscall events";
    } else if (replay_mode != REPLAY_MODE_NONE) {
        l1_why = "record/replay is active, and syscall mutations are not logged";
    } else if (!replay_analysis_write_allowed()) {
        l1_why = "the guest may not be changed now (analysis write policy)";
    } else if (x->held) {
        l1_why = "this callback already asked for a fault or an injection";
    } else {
        return x;
    }
    return NULL;
}

bool panda_syscall_skip(PandaSyscall *sc, int64_t ret)
{
    L1Ctx *x = l1_mutable(sc);

    if (!x) {
        return false;
    }
    if (x->at_return) {
        return l1_refuse("skip is for the entry: the call has run");
    }
    x->sc->skip = true;
    x->sc->ret = ret;
    x->a.ret = ret;
    x->a.flags |= PANDA_SC_SKIPPED;
    return true;
}

bool panda_syscall_set_arg(PandaSyscall *sc, unsigned i, uint64_t value)
{
    L1Ctx *x = l1_mutable(sc);

    if (!x) {
        return false;
    }
    if (x->at_return) {
        return l1_refuse("arguments can be changed at the entry only");
    }
    if (i >= ARRAY_SIZE(sc->args)) {
        return l1_refuse("no such argument (0..5)");
    }
    x->sc->args[i] = value;
    x->a.args[i] = value;
    return true;
}

bool panda_syscall_set_ret(PandaSyscall *sc, int64_t ret)
{
    L1Ctx *x = l1_mutable(sc);

    if (!x) {
        return false;
    }
    if (!x->at_return && !x->sc->skip) {
        return l1_refuse("at the entry, a return value needs a skipped call (panda_syscall_skip)");
    }
    if (x->at_return && x->sc->restart) {
        return l1_refuse("the kernel restarts this call: its return value is not the task's");
    }
    x->sc->ret = ret;
    x->a.ret = ret;
    return true;
}

bool panda_syscall_fault(PandaSyscall *sc, uint64_t addr, bool write)
{
    L1Ctx *x = l1_mutable(sc);

    if (!x) {
        return false;
    }
    if (x->skipped) {
        return l1_refuse("the call was skipped: there is nothing to resume");
    }
    if (!x->at_return && x->sc->skip) {
        return l1_refuse("the call is skipped: a fault would hold a call that does not run");
    }
    x->sc->fault = true;
    x->sc->fault_addr = addr;
    x->sc->fault_write = write;
    x->held = true;
    return true;
}

bool panda_syscall_inject(PandaSyscall *sc, int64_t num, const uint64_t args[6])
{
    L1Ctx *x = l1_mutable(sc);

    if (!x) {
        return false;
    }
    if (x->skipped || (!x->at_return && x->sc->skip)) {
        return l1_refuse("the call is skipped: there is nothing to resume");
    }
    if (x->at_return && x->sc->restart) {
        return l1_refuse("the kernel restarts this call: inject at its next entry");
    }
    if (!(x->cpu->syscall_inject_abis & (1u << x->sc->abi))) {
        return l1_refuse("this target cannot inject calls for this ABI");
    }
    x->sc->inject = true;
    x->sc->inject_num = num;
    if (args) {
        memcpy(x->sc->inject_args, args, sizeof(x->sc->inject_args));
    } else {
        memset(x->sc->inject_args, 0, sizeof(x->sc->inject_args));
    }
    x->held = true;
    return true;
}

bool panda_syscall_want_return(PandaSyscall *sc)
{
    L1Ctx *x = l1_cur;
    unsigned w;

    l1_why = NULL;
    if (!x || &x->a != sc) {
        return l1_refuse("not called from a syscall event callback for this call");
    }
    if (x->at_return) {
        return l1_refuse("this is the return");
    }
    w = l1_w(x->s, sc->abi, sc->num);
    if (l1_noreturn(sc->abi, sc->num) && !(w & PANDA_SC_NORETURN_TOO)) {
        return l1_refuse("an exit call: watch its return with PANDA_SC_NORETURN_TOO to track it");
    }
    x->call->want[x->i] = 1;
    return true;
}

const char *panda_syscall_refusal(void)
{
    return l1_why;
}

/* --- subscriptions and watches --------------------------------------------------------- */

static int l1_kind(uint32_t flags)
{
    return (flags & PANDA_SUB_OBSERVE_ORIGINAL) ? K_ORIG : (flags & PANDA_SUB_MUTATE) ? K_MUT
                                                                                    : K_OBS;
}

/* l1_lock held: the L1Sub of @sub, made if it has none */
static L1Sub *l1_sub_get(panda_sub sub, panda_event_id ev)
{
    L1Sub *s = g_hash_table_lookup(l1_subs, sub);

    if (!s) {
        s = g_new0(L1Sub, 1);
        s->sub = sub;
        s->ev = ev;
        s->ret_only = ev == l1_ev[1];
        s->refs = 1;
        g_hash_table_insert(l1_subs, sub, s);
    }
    return s;
}

/* l1_lock held: the trap's watch of @kind at one entry (num -1: all), from the snapshot */
static void l1_push(int kind, int abi, int64_t num)
{
    L1Snap *snap = l1_snap[kind];
    uint8_t v = 0, *pushed = num < 0 ? &l1_pall[kind][abi] : &l1_pnums[kind][abi][num];

    for (unsigned i = 0; snap && i < snap->n; i++) {
        L1Sub *s = snap->e[i].s;
        v |= num < 0 ? s->all[abi] : s->nums[abi] ? s->nums[abi][num] : 0;
    }
    /* entries reach the consumer, which decides at the entry whether to track the return */
    v = v ? SYSCALL_TRAP_ENTER : 0;
    if (v != *pushed && l1_cons[kind]) {
        syscall_trap_consumer_watch(l1_cons[kind], abi, num, v);
        *pushed = v;
    }
}

static void l1_push_all(int kind)
{
    for (int abi = 0; abi < SYSCALL_ABI_COUNT; abi++) {
        l1_push(kind, abi, -1);
        for (int64_t num = 0; num < SYSCALL_TRAP_NUMS; num++) {
            l1_push(kind, abi, num);
        }
    }
}

static gint l1_order(gconstpointer a, gconstpointer b)
{
    const L1Sub *x = *(L1Sub * const *)a, *y = *(L1Sub * const *)b;

    if (x->kind == K_MUT && x->priority != y->priority) {
        return x->priority < y->priority ? -1 : 1;
    }
    return x->seq < y->seq ? -1 : x->seq > y->seq;
}

static const int l1_trap_order[K_N] = {
    [K_ORIG] = SYSCALL_TRAP_OBSERVE_ORIGINAL,
    [K_MUT] = SYSCALL_TRAP_MUTATE,
    [K_OBS] = SYSCALL_TRAP_OBSERVE,
};

/* registry locked: the active subscriptions changed */
static void l1_rebuild(panda_event_id changed, void *opaque)
{
    GPtrArray *kinds[K_N];
    GHashTableIter it;
    gpointer key, val;

    if (!l1_ev[0] || !l1_ev[1]) {
        return;                 /* still declaring */
    }
    QEMU_LOCK_GUARD(&l1_lock);
    g_hash_table_iter_init(&it, l1_subs);
    while (g_hash_table_iter_next(&it, &key, &val)) {
        ((L1Sub *)val)->seen = false;
    }
    for (int k = 0; k < K_N; k++) {
        kinds[k] = g_ptr_array_new();
    }
    for (int e = 0; e < 2; e++) {
        unsigned n = panda_event_active_subs(l1_ev[e], NULL, 0);
        g_autofree panda_sub *subs = g_new(panda_sub, MAX(n, 1));

        panda_event_active_subs(l1_ev[e], subs, n);
        for (unsigned i = 0; i < n; i++) {
            L1Sub *s = l1_sub_get(subs[i], l1_ev[e]);
            PandaSubInfo info;

            panda_sub_info(subs[i], &info);
            s->fn = info.fn;
            s->ctx = info.ctx;
            s->kind = l1_kind(info.flags);
            s->priority = info.priority;
            s->seq = info.seq;
            s->data_size = info.data_size;
            s->seen = true;
            g_ptr_array_add(kinds[s->kind], s);
        }
    }
    g_hash_table_iter_init(&it, l1_subs);
    while (g_hash_table_iter_next(&it, &key, &val)) {
        L1Sub *s = val;

        if (!s->seen) {
            qatomic_set(&s->active, false);
        }
        if (!panda_event_has_sub(s->ev, s->sub)) {
            g_hash_table_iter_remove(&it);      /* unsubscribed: its watches go */
            l1_sub_unref(s);
        }
    }
    for (int k = 0; k < K_N; k++) {
        GPtrArray *a = kinds[k];
        L1Snap *snap = NULL, *old = l1_snap[k];

        g_ptr_array_sort(a, l1_order);
        if (a->len) {
            size_t off;

            snap = g_malloc(sizeof(*snap) + a->len * sizeof(snap->e[0]));
            snap->refs = 1;
            snap->n = a->len;
            off = ROUND_UP(offsetof(L1Call, want) + a->len, 16);
            for (unsigned i = 0; i < a->len; i++) {
                L1Sub *s = g_ptr_array_index(a, i);

                qatomic_inc(&s->refs);
                qatomic_set(&s->active, true);
                snap->e[i].s = s;
                snap->e[i].off = off;
                snap->e[i].size = s->data_size;
                off += ROUND_UP(s->data_size, 16);
            }
            snap->block = off;
        }
        qatomic_rcu_set(&l1_snap[k], snap);
        l1_snap_unref(old);
        g_ptr_array_free(a, true);

        if (snap && !l1_cons[k]) {
            l1_cons[k] = syscall_trap_consumer_add(&l1_ops, (void *)(intptr_t)k,
                                                   l1_trap_order[k], 0);
            memset(l1_pall[k], 0, sizeof(l1_pall[k]));
            memset(l1_pnums[k], 0, sizeof(l1_pnums[k]));
        }
        if (l1_cons[k]) {
            l1_push_all(k);
        }
        /* no subscription of this kind: no consumer (refused while it holds a call that
         * runs an injected one: then it stays, watching nothing, until the next change) */
        if (!snap && l1_cons[k] && syscall_trap_consumer_remove(l1_cons[k])) {
            l1_cons[k] = NULL;
        }
    }
}

typedef struct L1Watch {
    int abi;
    int64_t num;
    unsigned what;
    bool on;
    bool ok;
} L1Watch;

static void l1_watch_locked(panda_sub sub, panda_event_id ev, void *opaque)
{
    L1Watch *w = opaque;
    L1Sub *s;

    if (ev != l1_ev[0] && ev != l1_ev[1]) {
        l1_why = "not a subscription to syscall.enter or syscall.return";
        return;
    }
    if (ev == l1_ev[1] && (w->what & PANDA_SC_ENTER) && w->on) {
        l1_why = "a syscall.return subscription watches returns only";
        return;
    }
    QEMU_LOCK_GUARD(&l1_lock);
    s = l1_sub_get(sub, ev);
    if (w->num < 0) {
        s->all[w->abi] = w->on ? s->all[w->abi] | w->what : s->all[w->abi] & ~w->what;
    } else {
        if (!s->nums[w->abi]) {
            if (!w->on) {
                w->ok = true;
                return;
            }
            s->nums[w->abi] = g_new0(uint8_t, SYSCALL_TRAP_NUMS);
        }
        uint8_t *p = &s->nums[w->abi][w->num];
        *p = w->on ? *p | w->what : *p & ~w->what;
    }
    if (qatomic_read(&s->active) && l1_cons[s->kind]) {
        if (w->num < 0) {
            l1_push(s->kind, w->abi, -1);
        } else {
            l1_push(s->kind, w->abi, w->num);
        }
    }
    w->ok = true;
}

static bool l1_watch(panda_sub sub, int abi, int64_t num, unsigned what, bool on)
{
    L1Watch w = { .abi = abi, .num = num, .what = what, .on = on };

    l1_why = NULL;
    panda_syscall_event(PANDA_SC_ENTER);
    if (abi < 0 || abi >= SYSCALL_ABI_COUNT) {
        return l1_refuse("no such ABI");
    }
    if (num < -1 || num >= SYSCALL_TRAP_NUMS) {
        return l1_refuse("the number is out of range");
    }
    if (!what || (what & ~L1_WHAT)) {
        return l1_refuse("watch PANDA_SC_ENTER and/or PANDA_SC_RETURN (and PANDA_SC_NORETURN_TOO)");
    }
    if (!panda_sub_with_lock(sub, l1_watch_locked, &w)) {
        return l1_refuse("not a live subscription");
    }
    return w.ok;
}

bool panda_syscall_watch(panda_sub sub, int abi, int64_t num, unsigned what)
{
    return l1_watch(sub, abi, num, what, true);
}

bool panda_syscall_unwatch(panda_sub sub, int abi, int64_t num, unsigned what)
{
    return l1_watch(sub, abi, num, what, false);
}

/* --- the events ------------------------------------------------------------------------ */

static void l1_declare(void)
{
    static const char *const names[2] = { "syscall.enter", "syscall.return" };

    qemu_mutex_init(&l1_lock);
    l1_subs = g_hash_table_new(NULL, NULL);
    for (int e = 0; e < 2; e++) {
        panda_event_desc desc = {
            .size = sizeof(desc), .ns = &PANDA_EVENT_NS, .name = names[e],
            .signature = PANDA_SYSCALL_SIGNATURE, .args_size = sizeof(PandaSyscall),
            .modes = PANDA_MODE_ANALYSIS | PANDA_MODE_FUZZ | PANDA_MODE_RR,
            .capability = 0,
            /* mutators are refused under record/replay: their effects are not logged */
            .flags = PANDA_EVENT_MUTABLE | PANDA_EVENT_INSTANCES,
        };
        l1_ev[e] = panda_event_declare(NULL, &desc);
        if (!l1_ev[e]) {
            error_report("PANDA syscall events: %s could not be declared", names[e]);
        }
    }
    for (int e = 0; e < 2; e++) {
        if (l1_ev[e]) {
            panda_event_set_watcher(l1_ev[e], l1_rebuild, NULL);
        }
    }
}

panda_event_id panda_syscall_event(unsigned which)
{
    static gsize done;

    if (g_once_init_enter(&done)) {
        l1_declare();
        g_once_init_leave(&done, 1);
    }
    return which == PANDA_SC_ENTER ? l1_ev[0] : which == PANDA_SC_RETURN ? l1_ev[1] : NULL;
}

/* declared at startup, so that a subscriber that finds them by name binds */
static void __attribute__((constructor)) l1_init(void)
{
    panda_syscall_event(PANDA_SC_ENTER);
}
