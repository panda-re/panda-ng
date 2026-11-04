/*
 * Syscall events at the trap (system mode, TCG)
 *
 * The target reports a user task's syscall trap and every return to user
 * mode while a call is pending; this file filters the traps by number,
 * calls the consumers in order, and pairs each call with its return.
 *
 * Consumers are published as an RCU array in dispatch order.  Each pending
 * or held call keeps the call's shared state in its slot and one SCState per
 * consumer (its data, task, want_return and keep) in a row of sc_rows, one
 * column per consumer.  A dispatch copies a slot's row out into per-thread
 * scratch, runs the consumers without the lock, and puts what is still
 * needed back.  Drops never run under sc_lock.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qemu/atomic.h"
#include "qemu/rcu.h"
#include "qemu/host-utils.h"
#include "hw/core/cpu.h"
#include "exec/target_page.h"
#include "system/tcg.h"
#include "system/syscall-trap.h"
#include "system/replay-analysis.h"
#include "system/replay-effect.h"
#include "panda/guest-write.h"
#include "system/address-spaces.h"
#ifdef CONFIG_TCG
#include "accel/tcg/probe.h"
#include "accel/tcg/cpu-mmu-index.h"
#include "exec/tlb-flags.h"
#endif

#define SC_SLOTS 4096           /* pending calls; a power of two */
#define SC_SCRATCH 16           /* consumers a dispatch serves without allocating */

bool syscall_trap_on;
uint32_t syscall_trap_pending;

struct SyscallTrapConsumer {
    struct rcu_head rcu;
    const SyscallTrapOps *ops;
    void *opaque;
    int order;
    int priority;
    uint64_t seq;               /* unique: the consumer's identity in a held call */
    uint32_t gen;               /* bumped when the struct is reused */
    unsigned col;               /* its column in sc_rows */
    bool live;
    SyscallTrapConsumer *next_free;
    uint8_t watch_all[SYSCALL_ABI_COUNT];
    uint8_t watch[SYSCALL_ABI_COUNT][SYSCALL_TRAP_NUMS];
};

typedef struct SCList {
    struct rcu_head rcu;
    unsigned n;
    SyscallTrapConsumer *c[];
} SCList;

/* a consumer's part of one call */
typedef struct SCState {
    void *data;
    uint64_t task;
    bool want;                  /* deliver the return to it */
    bool keep;                  /* it kept the call pending (vfork child) */
} SCState;

/* the holder of a held call: its place in the order */
typedef struct SCKey {
    int order;
    int priority;
    uint64_t seq;
} SCKey;

/* one consumer's entry in a dispatch */
typedef struct SCScr {
    SyscallTrapConsumer *c;
    uint32_t gen;
    SCState st;
} SCScr;

static SCList *sc_list;         /* RCU; NULL without consumers */
static SyscallTrapConsumer **sc_cols;   /* the owner of each column */
static unsigned sc_ncols;
static SCState *sc_rows;        /* [SC_SLOTS][sc_ncols] */
static SyscallTrapConsumer *sc_free;
static uint64_t sc_cseq;
static uint64_t sc_ids;         /* SyscallTrap.id */

static uint32_t sc_held;        /* calls held for a fault */
static uint64_t sc_faults;
static __thread uint64_t sc_fault_addr;
static __thread bool sc_fault_write;

static uint8_t sc_watch[SYSCALL_ABI_COUNT][SYSCALL_TRAP_NUMS];
static uint8_t sc_watch_all[SYSCALL_ABI_COUNT];
static unsigned sc_watch_n;     /* entries of the two set */

static const bool sc_is32[SYSCALL_ABI_COUNT] = {
    [SYSCALL_ABI_I386] = true,
    [SYSCALL_ABI_ARM] = true,
    [SYSCALL_ABI_ARM_OABI] = true,
    [SYSCALL_ABI_PPC] = true,
    [SYSCALL_ABI_RISCV32] = true,
};

/*
 * SC_INJECT: held while the task runs a call in its place (inject);
 * SC_INJECT_RET: the same at its return, the task's return held
 */
enum { SC_PENDING, SC_HELD_ENTER, SC_HELD_RETURN, SC_INJECT, SC_INJECT_RET };

typedef struct {
    uint64_t k1, k2, pc;        /* pc 0: empty */
    uint8_t len;
    uint8_t state;              /* waiting for its return, or held */
    uint64_t seq;               /* when it was added */
    SyscallTrap sc;             /* the shared part */
    int64_t num;                /* held: the registers it replaced or had */
    uint64_t args[6];
    SCKey holder;               /* held: the consumer that holds it */
    uint64_t arch;              /* the target's word (syscall_trap_set_arch) */
} SCSlot;

static SCSlot sc_slots[SC_SLOTS];
static QemuMutex sc_lock;
static uint64_t sc_full, sc_stale, sc_seq;

/*
 * The consumers of the dispatch in flight on this thread.  A consumer that
 * leaves its callback by longjmp (qemu_plugin_set_pc from a syscall entry)
 * abandons the dispatch: the next one on the thread drops what the
 * consumers had on that call.
 */
static __thread SCScr sc_scr_inline[SC_SCRATCH];
static __thread SCScr *sc_scr_heap;
static __thread unsigned sc_scr_cap, sc_scr_n;
static __thread bool sc_busy;

/* the target's word: set for the next entry, the dispatched call's, the last return's */
static __thread uint64_t sc_arch_in, sc_arch_cur, sc_arch_out;

void syscall_trap_set_arch(uint64_t word)
{
    sc_arch_in = word;
}

uint64_t syscall_trap_arch(void)
{
    return sc_arch_out;
}

static void __attribute__((constructor)) sc_init(void)
{
    qemu_mutex_init(&sc_lock);
}

static inline SCState *sc_row(unsigned i)
{
    return &sc_rows[(size_t)i * sc_ncols];
}

static inline SCKey sc_key(const SyscallTrapConsumer *c)
{
    return (SCKey) { c->order, c->priority, c->seq };
}

/* <0: @c runs before the holder @k; 0: it is the holder; >0: after it */
static int sc_key_cmp(const SyscallTrapConsumer *c, const SCKey *k)
{
    if (c->seq == k->seq) {
        return 0;
    }
    if (c->order != k->order) {
        return c->order < k->order ? -1 : 1;
    }
    if (c->priority != k->priority) {
        return c->priority < k->priority ? -1 : 1;
    }
    return c->seq < k->seq ? -1 : 1;
}

/* the same consumer as when the dispatch began (removed or not) */
static inline bool sc_same(const SCScr *e)
{
    return e->c->gen == e->gen;
}

static inline bool sc_alive(const SCScr *e)
{
    return qatomic_read(&e->c->live) && sc_same(e);
}

static SCScr *sc_scratch(unsigned n)
{
    if (n <= SC_SCRATCH) {
        return sc_scr_inline;
    }
    if (n > sc_scr_cap) {
        sc_scr_heap = g_renew(SCScr, sc_scr_heap, n);
        sc_scr_cap = n;
    }
    return sc_scr_heap;
}

/* a fresh dispatch over @l: no consumer has anything on the call yet */
static SCScr *sc_scratch_fresh(SCList *l)
{
    SCScr *scr = sc_scratch(l->n);

    for (unsigned j = 0; j < l->n; j++) {
        scr[j].c = l->c[j];
        scr[j].gen = l->c[j]->gen;
        scr[j].st = (SCState) { 0 };
    }
    sc_scr_n = l->n;
    return scr;
}

static void sc_view(SyscallTrap *view, const SyscallTrap *sc, const SCState *st)
{
    *view = *sc;
    view->data = st->data;
    view->task = st->task;
    view->want_return = st->want;
    view->keep = st->keep;
}

/* release a consumer's data on a call; no lock held */
static void sc_drop_one(SyscallTrapConsumer *c, const SyscallTrap *sc, SCState *st)
{
    if (st->data && c->ops->drop) {
        SyscallTrap view QEMU_UNINITIALIZED;

        sc_view(&view, sc, st);
        c->ops->drop(&view, c->opaque);
    }
    *st = (SCState) { 0 };
}

/* every consumer's, at the end of a dispatch */
static void sc_drop_all(const SyscallTrap *sc, SCScr *scr, unsigned n)
{
    for (unsigned j = 0; j < n; j++) {
        if (scr[j].st.data && sc_same(&scr[j])) {
            sc_drop_one(scr[j].c, sc, &scr[j].st);
        }
        scr[j].st = (SCState) { 0 };
    }
}

/* the dispatch is over (or the data it had is now kept elsewhere) */
static inline void sc_done(void)
{
    sc_busy = false;
}

/* the last dispatch on this thread did not finish: drop what it had */
static void sc_abandoned(void)
{
    SCScr *scr = sc_scr_n <= SC_SCRATCH ? sc_scr_inline : sc_scr_heap;
    SyscallTrap sc = { 0 };

    sc_busy = false;
    for (unsigned j = 0; j < sc_scr_n; j++) {
        if (scr[j].st.data && sc_same(&scr[j])) {
            sc_drop_one(scr[j].c, &sc, &scr[j].st);
        }
    }
    sc_scr_n = 0;
}

/* calls discarded under sc_lock, dropped once it is released */
typedef struct SCDrop {
    SyscallTrapConsumer *c;
    SyscallTrap sc;
} SCDrop;

static __thread GArray *sc_dropped;

static void sc_drop_later(SyscallTrapConsumer *c, const SyscallTrap *sc, SCState *st)
{
    if (st->data) {
        SCDrop d;

        if (!sc_dropped) {
            sc_dropped = g_array_new(false, false, sizeof(SCDrop));
        }
        d.c = c;
        sc_view(&d.sc, sc, st);
        g_array_append_val(sc_dropped, d);
    }
    *st = (SCState) { 0 };
}

/* sc_lock held: every consumer's data on slot @i goes */
static void sc_slot_drop_later(unsigned i)
{
    SCState *row = sc_row(i);

    for (unsigned k = 0; k < sc_ncols; k++) {
        if (row[k].data && sc_cols[k]) {
            sc_drop_later(sc_cols[k], &sc_slots[i].sc, &row[k]);
        }
        row[k] = (SCState) { 0 };
    }
}

static void sc_unlock(void)
{
    qemu_mutex_unlock(&sc_lock);
    while (sc_dropped && sc_dropped->len) {
        SCDrop d = g_array_index(sc_dropped, SCDrop, sc_dropped->len - 1);

        g_array_set_size(sc_dropped, sc_dropped->len - 1);
        if (d.c->ops->drop) {
            d.c->ops->drop(&d.sc, d.c->opaque);
        }
    }
}

/* sc_lock held: forget slot @s */
static void sc_forget(SCSlot *s)
{
    if (s->state != SC_PENDING) {
        qatomic_dec(&sc_held);
    }
}

static inline unsigned sc_home(uint64_t k1, uint64_t k2, uint64_t pc)
{
    uint64_t h = (k1 ^ (k2 * 0x9e3779b97f4a7c15ull) ^ (pc * 0xc2b2ae3d27d4eb4full));
    return (h ^ h >> 29 ^ h >> 41) & (SC_SLOTS - 1);
}

/* the slot holding the call, or the empty slot where it would go */
static SCSlot *sc_find(uint64_t k1, uint64_t k2, uint64_t pc)
{
    unsigned i = sc_home(k1, k2, pc);

    for (unsigned n = 0; n < SC_SLOTS; n++, i = (i + 1) & (SC_SLOTS - 1)) {
        SCSlot *s = &sc_slots[i];
        if (!s->pc || (s->pc == pc && s->k1 == k1 && s->k2 == k2)) {
            return s;
        }
    }
    return NULL;
}

static void sc_move(unsigned to, unsigned from)
{
    sc_slots[to] = sc_slots[from];
    sc_slots[from].pc = 0;
    if (sc_ncols) {
        memcpy(sc_row(to), sc_row(from), sc_ncols * sizeof(SCState));
        memset(sc_row(from), 0, sc_ncols * sizeof(SCState));
    }
}

/* remove the call at @s (its row must hold no data), shifting later calls back */
static void sc_delete(SCSlot *s)
{
    unsigned i = s - sc_slots, j = i;

    sc_slots[i].pc = 0;
    if (sc_ncols) {
        memset(sc_row(i), 0, sc_ncols * sizeof(SCState));
    }
    for (;;) {
        j = (j + 1) & (SC_SLOTS - 1);
        if (!sc_slots[j].pc) {
            return;
        }
        unsigned h = sc_home(sc_slots[j].k1, sc_slots[j].k2, sc_slots[j].pc);
        if ((j > i && (h <= i || h > j)) || (j < i && h <= i && h > j)) {
            sc_move(i, j);
            i = j;
        }
    }
}

/* the held-call count changed, or a watch did */
static void sc_update_on(void)
{
    qatomic_set(&syscall_trap_on, sc_watch_n > 0 || qatomic_read(&sc_held) > 0);
}

/*
 * Calls whose return never comes back to the same task and address (a
 * successful execve) stay pending.  When the table is 3/4 full, drop the
 * calls added before the latest SC_SLOTS / 2.  Called with sc_lock held.
 */
static void sc_evict(void)
{
    g_autofree SCSlot *keep = g_new(SCSlot, SC_SLOTS);
    g_autofree SCState *rows = g_new0(SCState, (size_t)SC_SLOTS * MAX(sc_ncols, 1));
    unsigned n = 0;

    for (unsigned i = 0; i < SC_SLOTS; i++) {
        if (sc_slots[i].pc) {
            /* a task running an injected call needs its registers back */
            if (sc_slots[i].seq + SC_SLOTS / 2 >= sc_seq ||
                sc_slots[i].state == SC_INJECT ||
                sc_slots[i].state == SC_INJECT_RET) {
                if (sc_ncols) {
                    memcpy(&rows[(size_t)n * sc_ncols], sc_row(i),
                           sc_ncols * sizeof(SCState));
                }
                keep[n++] = sc_slots[i];
            } else {
                sc_forget(&sc_slots[i]);
                sc_slot_drop_later(i);
                sc_stale++;
            }
        }
    }
    memset(sc_slots, 0, sizeof(sc_slots));
    if (sc_ncols) {
        memset(sc_rows, 0, (size_t)SC_SLOTS * sc_ncols * sizeof(SCState));
    }
    for (unsigned i = 0; i < n; i++) {
        SCSlot *s = sc_find(keep[i].k1, keep[i].k2, keep[i].pc);
        *s = keep[i];
        if (sc_ncols) {
            memcpy(sc_row(s - sc_slots), &rows[(size_t)i * sc_ncols],
                   sc_ncols * sizeof(SCState));
        }
    }
    qatomic_set(&syscall_trap_pending, n);
}

/*
 * sc_lock held: keep the call, with the consumers' states from @scr (those
 * of consumers gone meanwhile are dropped).  NULL if the call could not be
 * kept: then every consumer's data is dropped.
 */
static SCSlot *sc_put(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                      const SyscallTrap *sc, int state, SCScr *scr, unsigned n,
                      const SCKey *holder)
{
    SCSlot *s;

    if (qatomic_read(&syscall_trap_pending) >= SC_SLOTS * 3 / 4) {
        sc_evict();
    }
    s = sc_find(k1, k2, pc);
    if (!s) {
        sc_full++;
        for (unsigned j = 0; j < n; j++) {
            if (sc_same(&scr[j])) {
                sc_drop_later(scr[j].c, sc, &scr[j].st);
            }
            scr[j].st = (SCState) { 0 };
        }
        return NULL;
    }
    if (s->pc) {
        sc_stale++;             /* its return was lost */
        sc_forget(s);
        sc_slot_drop_later(s - sc_slots);
    } else {
        qatomic_inc(&syscall_trap_pending);
    }
    s->k1 = k1;
    s->k2 = k2;
    s->pc = pc;
    s->len = len;
    s->state = state;
    s->seq = ++sc_seq;
    s->sc = *sc;
    s->sc.data = NULL;
    s->sc.task = 0;
    s->holder = holder ? *holder : (SCKey) { 0 };
    s->arch = sc_arch_cur;
    SCState *row = sc_row(s - sc_slots);
    for (unsigned j = 0; j < n; j++) {
        SyscallTrapConsumer *c = scr[j].c;

        /* a pending call is kept for those that want its return; a held one
         * for everyone with something on it */
        if (state == SC_PENDING ? !scr[j].st.want
                                : !scr[j].st.data && !scr[j].st.want) {
            continue;
        }
        if (sc_alive(&scr[j]) && sc_cols[c->col] == c) {
            row[c->col] = scr[j].st;
            scr[j].st = (SCState) { 0 };
        } else if (sc_same(&scr[j])) {
            sc_drop_later(c, sc, &scr[j].st);   /* removed meanwhile */
        } else {
            scr[j].st = (SCState) { 0 };
        }
    }
    if (state != SC_PENDING) {
        qatomic_inc(&sc_held);
        if (!qatomic_read(&syscall_trap_on)) {
            qatomic_set(&syscall_trap_on, true);
        }
    }
    return s;
}

/*
 * sc_lock held: take the call at @s out into a dispatch over the current
 * consumers.  The slot is deleted.
 */
static SCScr *sc_take(SCSlot *s, SCList **lp, SyscallTrap *sc)
{
    SCList *l = sc_list;
    unsigned n = l ? l->n : 0;
    SCScr *scr = sc_scratch(n);
    SCState *row = sc_row(s - sc_slots);

    *sc = s->sc;
    sc_arch_cur = s->arch;
    for (unsigned j = 0; j < n; j++) {
        SyscallTrapConsumer *c = l->c[j];

        scr[j].c = c;
        scr[j].gen = c->gen;
        scr[j].st = row[c->col];
        row[c->col] = (SCState) { 0 };
    }
    sc_scr_n = n;
    sc_forget(s);
    sc_delete(s);
    qatomic_dec(&syscall_trap_pending);
    *lp = l;
    return scr;
}

/* hold the call until the guest has handled the fault its holder asked for */
static int sc_hold(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                   SyscallTrap *sc, int state, SCScr *scr, unsigned n,
                   SyscallTrapConsumer *holder, int64_t onum,
                   const uint64_t oargs[6])
{
    SCKey hk = sc_key(holder);
    SCSlot *s;

    sc_fault_addr = sc->fault_addr;
    sc_fault_write = sc->fault_write;
    sc->fault = false;
    qemu_mutex_lock(&sc_lock);
    s = sc_put(k1, k2, pc, len, sc, state, scr, n, &hk);
    if (s && oargs) {
        s->num = onum;
        memcpy(s->args, oargs, sizeof(s->args));
    }
    sc_done();
    sc_unlock();
    return SYSCALL_TRAP_FAULT;
}

/*
 * Run the call the holder asked for in place of this one: keep the
 * registers it replaces with the held call; @args and *@ret become the
 * injected call's.  False if the table cannot keep it (the call is
 * dropped, and runs as it is).
 */
static bool sc_inject(CPUState *cpu, uint64_t k1, uint64_t k2, uint64_t pc,
                      unsigned len, SyscallTrap *sc, SCScr *scr, unsigned n,
                      SyscallTrapConsumer *holder, int64_t num,
                      uint64_t args[6], uint64_t *ret)
{
    int64_t inum = sc->inject_num;
    uint64_t iargs[6];
    SCKey hk = sc_key(holder);
    SCSlot *s;

    memcpy(iargs, sc->inject_args, sizeof(iargs));
    sc->inject = false;
    sc->injected = false;
    /* the target puts the registers back for these ABIs only */
    if (!(cpu->syscall_inject_abis & (1u << sc->abi))) {
        sc_drop_all(sc, scr, n);
        sc_done();
        return false;
    }
    qemu_mutex_lock(&sc_lock);
    s = sc_put(k1, k2, pc, len, sc, SC_INJECT, scr, n, &hk);
    if (s) {
        s->num = num;
        memcpy(s->args, args, sizeof(s->args));
    }
    sc_done();
    sc_unlock();
    if (!s) {
        return false;
    }
    *ret = inum;
    memcpy(args, iargs, sizeof(iargs));
    return true;
}

/*
 * The call held at the trap, if this is the task coming back to it:
 * SC_HELD_ENTER, taken into a dispatch; SC_INJECT, a restart of the
 * injected call (left alone); or -1.
 */
static int sc_take_held(uint64_t k1, uint64_t k2, uint64_t pc,
                        SyscallTrap *sc, SCScr **scrp, SCList **lp,
                        SCKey *hk, int64_t *onum, uint64_t oargs[6])
{
    int found = -1;

    qemu_mutex_lock(&sc_lock);
    SCSlot *s = sc_find(k1, k2, pc);
    if (s && s->pc && s->state == SC_HELD_ENTER) {
        *hk = s->holder;
        *onum = s->num;
        memcpy(oargs, s->args, sizeof(s->args));
        *scrp = sc_take(s, lp, sc);
        sc_busy = true;
        found = SC_HELD_ENTER;
    } else if (s && s->pc && (s->state == SC_INJECT ||
                              s->state == SC_INJECT_RET)) {
        found = SC_INJECT;
    }
    sc_unlock();
    return found;
}

static inline unsigned sc_cwatch(const SyscallTrapConsumer *c, int abi, int64_t num)
{
    unsigned w = c->watch_all[abi];

    if (num >= 0 && num < SYSCALL_TRAP_NUMS) {
        w |= c->watch[abi][num];
    }
    return w;
}

int syscall_trap_enter(CPUState *cpu, int abi, int64_t num, uint64_t args[6],
                       uint64_t pc, unsigned len, uint64_t k1, uint64_t k2,
                       uint64_t percpu, uint32_t kmmu, uint64_t *ret)
{
    SCList *l;
    SCScr *scr = NULL;
    SyscallTrap sc QEMU_UNINITIALIZED;
    SCKey hk QEMU_UNINITIALIZED;
    int64_t onum = 0;
    uint64_t oargs[6] QEMU_UNINITIALIZED;
    unsigned w, n;
    bool resuming = false;
    int r = 0;
    uint64_t aw = sc_arch_in;

    sc_arch_in = 0;
    RCU_READ_LOCK_GUARD();
    l = qatomic_rcu_read(&sc_list);
    if (!l) {
        return 0;
    }
    if (unlikely(sc_busy)) {
        sc_abandoned();
    }
    w = sc_watch_all[abi];
    if (num >= 0 && num < SYSCALL_TRAP_NUMS) {
        w |= sc_watch[abi][num];
    }
    if (qatomic_read(&sc_held)) {
        int held = sc_take_held(k1, k2, pc, &sc, &scr, &l, &hk, &onum, oargs);

        if (held == SC_INJECT) {
            return 0;           /* the kernel restarts the injected call */
        }
        if (held == SC_HELD_ENTER) {
            if (sc.abi == abi && sc.num == num) {
                /* back after the fault: the registers are as they were */
                resuming = true;
                sc.percpu = percpu;
                if (memcmp(oargs, args, sizeof(oargs))) {
                    /* not as they were: the holder sees what the task has */
                    memcpy(sc.args, args, sizeof(sc.args));
                }
            } else {
                sc_drop_all(&sc, scr, sc_scr_n);    /* another call at the same address */
                sc_done();
            }
        }
    }
    if (!resuming) {
        if (!w) {
            return 0;
        }
        l = qatomic_rcu_read(&sc_list);
        if (!l) {
            return 0;
        }
        scr = sc_scratch_fresh(l);
        sc = (SyscallTrap) {
            .abi = abi, .num = num, .pc = pc, .percpu = percpu,
            .id = qatomic_fetch_inc(&sc_ids) + 1,
        };
        memcpy(sc.args, args, sizeof(sc.args));
        sc_busy = true;
    }
    sc_arch_cur = aw;           /* this trap's word, also for a resumed call */
    n = sc_scr_n;

    for (unsigned j = 0; j < n; j++) {
        SyscallTrapConsumer *c = scr[j].c;
        SCState *st = &scr[j].st;
        bool me = false;

        if (!sc_alive(&scr[j])) {
            continue;
        }
        if (resuming) {
            int k = sc_key_cmp(c, &hk);
            if (k < 0) {
                continue;       /* it ran before the call was held */
            }
            me = k == 0;
        }
        if (!me) {
            unsigned cw = sc_cwatch(c, abi, num);

            st->want = (cw & SYSCALL_TRAP_RETURN) && c->ops->ret;
            if (!(cw & SYSCALL_TRAP_ENTER) || !c->ops->enter) {
                continue;
            }
            sc.injected = false;
        }
        sc.data = st->data;
        sc.task = st->task;
        sc.want_return = st->want;
        sc.keep = false;
        sc.resumed = me;
        cpu->syscall_kmmu = kmmu;
        c->ops->enter(cpu, &sc, c->opaque);
        cpu->syscall_kmmu = 0;
        st->data = sc.data;
        st->task = sc.task;
        st->want = sc.want_return && c->ops->ret;
        sc.resumed = false;
        sc.injected = false;
        if (!replay_analysis_write_allowed()) {
            /* Deliver observations, but never skip/inject/rewrite the real call. */
            sc.fault = sc.inject = sc.skip = false;
            memcpy(sc.args, args, sizeof(sc.args));
        }
        if (sc.fault) {
            return sc_hold(k1, k2, pc, len, &sc, SC_HELD_ENTER, scr, n, c,
                           num, args);
        }
        if (sc.inject) {
            return sc_inject(cpu, k1, k2, pc, len, &sc, scr, n, c, num, args, ret)
                   ? SYSCALL_TRAP_INJECT : 0;
        }
    }
    if (memcmp(sc.args, args, sizeof(sc.args))) {
        memcpy(args, sc.args, sizeof(sc.args));
        r |= SYSCALL_TRAP_ARGS;
    }
    if (sc.skip) {
        /* the return of a skipped call, to those that asked for it */
        for (unsigned j = 0; j < n; j++) {
            SyscallTrapConsumer *c = scr[j].c;
            SCState *st = &scr[j].st;

            if (!st->want || !c->ops->ret || !sc_alive(&scr[j]) ||
                !(c->ops->flags & SYSCALL_TRAP_OPS_SKIPPED_RETURN)) {
                continue;
            }
            sc.data = st->data;
            sc.task = st->task;
            sc.want_return = true;
            sc.keep = sc.resumed = sc.injected = sc.restart = false;
            cpu->syscall_kmmu = kmmu;
            c->ops->ret(cpu, &sc, c->opaque);
            cpu->syscall_kmmu = 0;
            st->data = sc.data;
            sc.fault = sc.inject = sc.keep = false;
            sc.skip = true;
        }
        sc_drop_all(&sc, scr, n);
        sc_done();
        *ret = sc_is32[abi] ? (uint32_t)sc.ret : (uint64_t)sc.ret;
        return r | SYSCALL_TRAP_SKIP;
    }
    for (unsigned j = 0; j < n; j++) {
        if (scr[j].st.want && sc_alive(&scr[j])) {
            qemu_mutex_lock(&sc_lock);
            sc_put(k1, k2, pc, len, &sc, SC_PENDING, scr, n, NULL);
            sc_unlock();
            break;
        }
    }
    sc_drop_all(&sc, scr, n);
    sc_done();
    return r;
}

/*
 * Run a return through the consumers that want it, from the holder @hk on
 * if it resumes.  The index of the consumer that asked for a fault or an
 * injection (the new holder), or -1.
 */
static int sc_ret_run(CPUState *cpu, SyscallTrap *sc, SCScr *scr, unsigned n,
                      const SCKey *hk, uint32_t kmmu, int64_t v)
{
    for (unsigned j = 0; j < n; j++) {
        SyscallTrapConsumer *c = scr[j].c;
        SCState *st = &scr[j].st;
        bool me = false;

        if (!sc_alive(&scr[j])) {
            continue;
        }
        if (hk) {
            int k = sc_key_cmp(c, hk);
            if (k < 0) {
                continue;
            }
            me = k == 0;
        }
        if (!st->want || !c->ops->ret) {
            continue;
        }
        if (!me) {
            sc->injected = false;
        }
        sc->data = st->data;
        sc->task = st->task;
        sc->want_return = true;
        sc->keep = false;
        sc->resumed = me;
        cpu->syscall_kmmu = kmmu;
        c->ops->ret(cpu, sc, c->opaque);
        cpu->syscall_kmmu = 0;
        st->data = sc->data;
        st->task = sc->task;
        st->keep = sc->keep;
        sc->keep = false;
        sc->resumed = false;
        sc->injected = false;
        if (!replay_analysis_write_allowed()) {
            sc->fault = sc->inject = false;
            sc->ret = v;
        }
        if (sc->fault || sc->inject) {
            return j;
        }
    }
    return -1;
}

/*
 * A return is held at consumer @h: the consumers before it are done with
 * the call (their data goes), except those that keep it pending.
 */
static void sc_ret_held_before(const SyscallTrap *sc, SCScr *scr, int h)
{
    for (int j = 0; j < h; j++) {
        if (!scr[j].st.keep) {
            sc_drop_one(scr[j].c, sc, &scr[j].st);
        }
    }
    scr[h].st.want = true;
}

/*
 * A return run in which consumer @h asked for an injected call (sc->inject),
 * or one that is back from one (@restore: the task has the injected call's
 * registers).  Inject: hold the return as SC_INJECT_RET with the call's own
 * number and arguments (@onum, @oargs) and send the task back to the trap
 * instruction with the injected call's (REWIND).  Otherwise, with @restore,
 * the task gets its own call's registers back and the run's return value
 * (RESTORE, RET), and is held for a fault or done.
 */
static int sc_ret_after(CPUState *cpu, uint64_t k1, uint64_t k2,
                        uint64_t key, unsigned len, SyscallTrap *sc,
                        SCScr *scr, unsigned n, int h,
                        int64_t onum, const uint64_t oargs[6], bool restore,
                        uint64_t *ret, SyscallTrapRewind *rw)
{
    int r = 0;

    if (h >= 0) {
        sc_ret_held_before(sc, scr, h);
    }
    if (sc->inject) {
        SCSlot *s = NULL;
        SCKey hk = sc_key(scr[h].c);

        sc->inject = false;
        sc->injected = false;
        if (!sc->restart && (cpu->syscall_inject_abis & (1u << sc->abi))) {
            qemu_mutex_lock(&sc_lock);
            s = sc_put(k1, k2, key, len, sc, SC_INJECT_RET, scr, n, &hk);
            if (s) {
                s->num = onum;
                memcpy(s->args, oargs, sizeof(s->args));
            }
            sc_done();
            sc_unlock();
        }
        if (s) {
            rw->abi = sc->abi;
            rw->num = sc->inject_num;
            memcpy(rw->args, sc->inject_args, sizeof(rw->args));
            rw->len = len;
            return SYSCALL_TRAP_REWIND;
        }
        sc_drop_all(sc, scr, n);    /* the run is aborted */
        sc_done();
    } else if (sc->fault) {
        r = sc_hold(k1, k2, key, len, sc, SC_HELD_RETURN, scr, n, scr[h].c, 0, NULL);
    } else {
        sc_drop_all(sc, scr, n);
        sc_done();
    }
    if (restore) {
        rw->abi = sc->abi;
        rw->num = onum;
        memcpy(rw->args, oargs, sizeof(rw->args));
        rw->len = 0;
        *ret = sc_is32[sc->abi] ? (uint32_t)sc->ret : (uint64_t)sc->ret;
        r |= SYSCALL_TRAP_RESTORE | SYSCALL_TRAP_RET;
    }
    return r;
}

int syscall_trap_return(CPUState *cpu, uint64_t pc, uint64_t k1, uint64_t k2,
                        uint64_t percpu, uint32_t kmmu, uint64_t *ret,
                        SyscallTrapRewind *rw)
{
    SyscallTrap sc QEMU_UNINITIALIZED;
    SCKey hk QEMU_UNINITIALIZED;
    SCList *l;
    SCScr *scr;
    SCSlot *s;
    uint64_t key;
    unsigned len = 0, n;
    bool restart = false, resumed;
    int h;

    RCU_READ_LOCK_GUARD();
    if (unlikely(sc_busy)) {
        sc_abandoned();
    }
    sc_arch_out = 0;
    qemu_mutex_lock(&sc_lock);
    s = sc_find(k1, k2, pc);
    if (s && s->pc) {
        sc_arch_out = s->arch;
    }
    if (!replay_analysis_write_allowed() && s && s->pc &&
        (s->state == SC_INJECT || s->state == SC_INJECT_RET)) {
        /* Host-side state from before a snapshot/replay transition must not
         * rewind a task in the replay. */
        sc_slot_drop_later(s - sc_slots);
        sc_forget(s);
        sc_delete(s);
        qatomic_dec(&syscall_trap_pending);
        sc_unlock();
        return 0;
    }
    if (s && s->pc && s->state == SC_INJECT) {
        /*
         * The injected call is back (after any signal handler it ran into:
         * sigreturn brings the task back here).  The task goes back to the
         * trap, its own call held there with the answer.
         */
        s->sc.injected = true;
        s->sc.inject_ret = sc_is32[s->sc.abi] ? (int32_t)*ret : (int64_t)*ret;
        s->state = SC_HELD_ENTER;
        rw->abi = s->sc.abi;
        rw->num = s->num;
        memcpy(rw->args, s->args, sizeof(rw->args));
        rw->len = s->len;
        sc_unlock();
        return SYSCALL_TRAP_REWIND;
    }
    if (s && s->pc && s->state == SC_INJECT_RET) {
        /*
         * A call injected at a return is back: the held return run gets its
         * answer, and the task its own call's registers and return value.
         */
        int64_t onum = s->num;
        uint64_t oargs[6];

        hk = s->holder;
        memcpy(oargs, s->args, sizeof(oargs));
        len = s->len;
        key = s->pc;
        scr = sc_take(s, &l, &sc);
        n = sc_scr_n;
        sc_busy = true;
        sc_unlock();
        sc.injected = true;
        sc.inject_ret = sc_is32[sc.abi] ? (int32_t)*ret : (int64_t)*ret;
        sc.percpu = percpu;
        h = sc_ret_run(cpu, &sc, scr, n, &hk, kmmu, sc.ret);
        return sc_ret_after(cpu, k1, k2, key, len, &sc, scr, n, h, onum, oargs,
                            true, ret, rw);
    }
    if (!s || !s->pc || s->state == SC_HELD_ENTER) {
        /*
         * The kernel rewound the task to the trap: a restart, or the
         * return from a fault raised at the trap (that call is held).
         */
        static const unsigned lens[] = { 2, 4 };
        s = NULL;
        for (int i = 0; i < ARRAY_SIZE(lens) && !s; i++) {
            SCSlot *t = sc_find(k1, k2, pc + lens[i]);
            if (t && t->pc && t->len == lens[i] &&
                t->state != SC_HELD_ENTER && t->state != SC_INJECT &&
                t->state != SC_INJECT_RET) {
                s = t;
            }
        }
        if (!s) {
            sc_arch_out = 0;
            sc_unlock();
            return 0;
        }
        restart = true;
        sc_arch_out = s->arch;
    }
    resumed = s->state == SC_HELD_RETURN;
    hk = s->holder;
    len = s->len;
    key = s->pc;
    scr = sc_take(s, &l, &sc);
    n = sc_scr_n;
    sc_busy = true;
    sc_unlock();

    int64_t v = sc_is32[sc.abi] ? (int32_t)*ret : (int64_t)*ret;
    sc.ret = v;
    sc.percpu = percpu;
    if (!resumed) {
        sc.restart = restart;
    }
    restart = sc.restart;
    h = sc_ret_run(cpu, &sc, scr, n, resumed ? &hk : NULL, kmmu, v);
    if (h >= 0 && sc.inject) {
        return sc_ret_after(cpu, k1, k2, key, len, &sc, scr, n, h, sc.num,
                            sc.args, false, ret, rw);
    }
    if (h >= 0) {
        sc_ret_held_before(&sc, scr, h);
        return sc_hold(k1, k2, key, len, &sc, SC_HELD_RETURN, scr, n,
                       scr[h].c, 0, NULL);
    }

    int r = 0;
    if (!restart && sc.ret != v) {
        *ret = sc_is32[sc.abi] ? (uint32_t)sc.ret : (uint64_t)sc.ret;
        r = SYSCALL_TRAP_RET;
    }
    if (!restart) {
        bool kept = false;

        /* not the caller's (vfork child): pending again for those that said so */
        for (unsigned j = 0; j < n; j++) {
            SCState *st = &scr[j].st;

            if (st->keep && sc_alive(&scr[j])) {
                st->keep = false;
                st->want = true;
                kept = true;
            } else {
                sc_drop_one(scr[j].c, &sc, st);
            }
        }
        if (kept) {
            sc.ret = 0;
            sc.resumed = false;
            qemu_mutex_lock(&sc_lock);
            sc_put(k1, k2, key, len, &sc, SC_PENDING, scr, n, NULL);
            sc_unlock();
        }
    }
    sc_drop_all(&sc, scr, n);
    sc_done();
    return r;
}

void syscall_trap_fault(CPUState *cpu, uintptr_t ra)
{
    if (!replay_analysis_write_allowed()) {
        return;
    }
#ifdef CONFIG_TCG
    if (tcg_enabled()) {
        qatomic_inc(&sc_faults);
        probe_access(cpu_env(cpu), sc_fault_addr, 0,
                     sc_fault_write ? MMU_DATA_STORE : MMU_DATA_LOAD,
                     cpu_mmu_index(cpu, false), ra);
    }
#endif
}

uint64_t syscall_trap_faults(void)
{
    return qatomic_read(&sc_faults);
}

/* sc_lock held: does any consumer's task on slot @i equal @task */
static bool sc_slot_has_task(unsigned i, uint64_t task)
{
    SCState *row = sc_row(i);

    for (unsigned k = 0; k < sc_ncols; k++) {
        if (sc_cols[k] && row[k].task == task) {
            return true;
        }
    }
    return false;
}

unsigned syscall_trap_forget_task(uint64_t task)
{
    unsigned n = 0;

    if (!task || !syscall_trap_waiting()) {
        return 0;
    }
    qemu_mutex_lock(&sc_lock);
    for (unsigned i = 0; i < SC_SLOTS;) {
        SCSlot *s = &sc_slots[i];

        if (s->pc && sc_slot_has_task(i, task)) {
            sc_forget(s);
            sc_slot_drop_later(i);
            sc_delete(s);       /* may move a later call into slot i */
            qatomic_dec(&syscall_trap_pending);
            n++;
        } else {
            i++;
        }
    }
    sc_unlock();
    return n;
}

unsigned syscall_trap_cancel_all(void)
{
    CPUState *cpu;
    unsigned n = 0;

    /* Callers have stopped the vCPUs before replacing guest execution. Do not
     * unwind injected registers into the new execution or remove watch policy. */
    qemu_mutex_lock(&sc_lock);
    for (unsigned i = 0; i < SC_SLOTS; i++) {
        if (sc_slots[i].pc) {
            sc_slot_drop_later(i);
            n++;
        }
    }
    memset(sc_slots, 0, sizeof(sc_slots));
    if (sc_ncols) {
        memset(sc_rows, 0, (size_t)SC_SLOTS * sc_ncols * sizeof(SCState));
    }
    qatomic_set(&syscall_trap_pending, 0);
    qatomic_set(&sc_held, 0);
    CPU_FOREACH(cpu) {
        cpu->syscall_current = 0;
        cpu->syscall_kmmu = 0;
        cpu->syscall_inject_abis = 0;
    }
    /* Fault scratch is vCPU TLS, consumed immediately by syscall_trap_fault
     * before the vCPU can become stopped. These assignments clear only the
     * caller's scratch, not other threads; no held continuation retains it. */
    sc_fault_addr = 0;
    sc_fault_write = false;
    sc_unlock();  /* the consumers' drops (Penguin's ABORT -> free) run outside the lock */
    return n;
}

/* --- consumers ------------------------------------------------------------------------ */

/*
 * Before the machine has vCPUs nothing dispatches, and QEMU's RCU thread
 * cannot run callbacks yet (it takes the BQL): free at once.  Plugins add
 * consumers from init_plugin, before qemu_init.
 */
static bool sc_rcu_ready(void)
{
    return first_cpu != NULL;
}

static gint sc_consumer_order(gconstpointer a, gconstpointer b)
{
    const SyscallTrapConsumer *x = *(SyscallTrapConsumer * const *)a;
    SCKey k = sc_key(*(SyscallTrapConsumer * const *)b);

    return sc_key_cmp(x, &k);
}

/* sc_lock held: publish the live consumers, but @gone, in order */
static void sc_publish(SyscallTrapConsumer *add, SyscallTrapConsumer *gone)
{
    SCList *old = sc_list, *l;
    unsigned n = 0, m = old ? old->n : 0;

    l = g_malloc(sizeof(*l) + (m + 1) * sizeof(l->c[0]));
    for (unsigned j = 0; j < m; j++) {
        if (old->c[j] != gone) {
            l->c[n++] = old->c[j];
        }
    }
    if (add) {
        l->c[n++] = add;
    }
    l->n = n;
    qsort(l->c, n, sizeof(l->c[0]), sc_consumer_order);
    if (!n) {
        g_free(l);
        l = NULL;
    }
    qatomic_rcu_set(&sc_list, l);
    if (old && sc_rcu_ready()) {
        g_free_rcu(old, rcu);
    } else {
        g_free(old);
    }
}

/* sc_lock held: the core's watch at one entry, from its consumers' */
static void sc_watch_set(uint8_t *global, uint8_t v)
{
    if (!*global != !v) {
        sc_watch_n += v ? 1 : -1;
    }
    *global = v;
}

static void sc_rewatch(int abi, int64_t num)
{
    SCList *l = sc_list;
    uint8_t v = 0;

    for (unsigned j = 0; l && j < l->n; j++) {
        v |= num < 0 ? l->c[j]->watch_all[abi] : l->c[j]->watch[abi][num];
    }
    sc_watch_set(num < 0 ? &sc_watch_all[abi] : &sc_watch[abi][num], v);
}

static void sc_rewatch_all(void)
{
    for (int abi = 0; abi < SYSCALL_ABI_COUNT; abi++) {
        sc_rewatch(abi, -1);
        for (int64_t num = 0; num < SYSCALL_TRAP_NUMS; num++) {
            sc_rewatch(abi, num);
        }
    }
    sc_update_on();
}

static void sc_recycle(SyscallTrapConsumer *c)
{
    qemu_mutex_lock(&sc_lock);
    c->gen++;
    c->next_free = sc_free;
    sc_free = c;
    qemu_mutex_unlock(&sc_lock);
}

SyscallTrapConsumer *syscall_trap_consumer_add(const SyscallTrapOps *ops, void *opaque,
                                               int order, int priority)
{
    SyscallTrapConsumer *c;
    unsigned col;

    if (!ops || order < SYSCALL_TRAP_OBSERVE_ORIGINAL || order > SYSCALL_TRAP_OBSERVE) {
        return NULL;
    }
    qemu_mutex_lock(&sc_lock);
    c = sc_free;
    if (c) {
        sc_free = c->next_free;
    } else {
        c = g_new0(SyscallTrapConsumer, 1);
    }
    for (col = 0; col < sc_ncols && sc_cols[col]; col++) {
    }
    if (col == sc_ncols) {
        /* a new column for every slot */
        unsigned cols = sc_ncols + 1;
        SCState *rows = g_new0(SCState, (size_t)SC_SLOTS * cols);

        for (unsigned i = 0; i < SC_SLOTS && sc_ncols; i++) {
            memcpy(&rows[(size_t)i * cols], sc_row(i), sc_ncols * sizeof(SCState));
        }
        g_free(sc_rows);
        sc_rows = rows;
        sc_cols = g_renew(SyscallTrapConsumer *, sc_cols, cols);
        sc_cols[sc_ncols] = NULL;
        sc_ncols = cols;
    }
    c->ops = ops;
    c->opaque = opaque;
    c->order = order;
    c->priority = priority;
    c->seq = ++sc_cseq;
    c->col = col;
    c->next_free = NULL;
    memset(c->watch_all, 0, sizeof(c->watch_all));
    memset(c->watch, 0, sizeof(c->watch));
    qatomic_set(&c->live, true);
    sc_cols[col] = c;
    sc_publish(c, NULL);
    qemu_mutex_unlock(&sc_lock);
    return c;
}

bool syscall_trap_consumer_remove(SyscallTrapConsumer *c)
{
    bool last;

    if (!c) {
        return false;
    }
    qemu_mutex_lock(&sc_lock);
    if (!c->live) {
        qemu_mutex_unlock(&sc_lock);
        return false;
    }
    /* live detach cannot abandon injected register restoration */
    for (unsigned i = 0; i < SC_SLOTS; i++) {
        if (sc_slots[i].pc && (sc_slots[i].state == SC_INJECT ||
                               sc_slots[i].state == SC_INJECT_RET) &&
            sc_slots[i].holder.seq == c->seq) {
            qemu_mutex_unlock(&sc_lock);
            return false;       /* no watch, consumer, counter or slot changes */
        }
    }
    qatomic_set(&c->live, false);
    for (unsigned i = 0; i < SC_SLOTS;) {
        SCSlot *s = &sc_slots[i];
        bool want = false;

        if (!s->pc) {
            i++;
            continue;
        }
        SCState *row = sc_row(i);
        sc_drop_later(c, &s->sc, &row[c->col]);
        for (unsigned k = 0; k < sc_ncols; k++) {
            want |= row[k].want;
        }
        if (s->state == SC_PENDING && !want) {
            sc_slot_drop_later(i);
            sc_forget(s);
            sc_delete(s);       /* may move a later call into slot i */
            qatomic_dec(&syscall_trap_pending);
        } else {
            i++;
        }
    }
    sc_cols[c->col] = NULL;
    memset(c->watch_all, 0, sizeof(c->watch_all));
    memset(c->watch, 0, sizeof(c->watch));
    sc_publish(NULL, c);
    last = !sc_list;
    sc_rewatch_all();
    if (last) {
        qatomic_set(&syscall_trap_on, false);
    }
    sc_unlock();
    if (last) {
        syscall_trap_cancel_all();
    }
    if (sc_rcu_ready()) {
        call_rcu(c, sc_recycle, rcu);
    } else {
        sc_recycle(c);
    }
    return true;
}

void syscall_trap_consumer_watch(SyscallTrapConsumer *c, int abi, int64_t num,
                                 unsigned flags)
{
    if (!c || abi < 0 || abi >= SYSCALL_ABI_COUNT || num < -1 ||
        num >= SYSCALL_TRAP_NUMS) {
        return;
    }
    flags &= SYSCALL_TRAP_ENTER | SYSCALL_TRAP_RETURN;
    qemu_mutex_lock(&sc_lock);
    if (c->live) {
        if (num < 0) {
            c->watch_all[abi] = flags;
        } else {
            c->watch[abi][num] = flags;
        }
        sc_rewatch(abi, num);
        sc_update_on();
    }
    qemu_mutex_unlock(&sc_lock);
}

void syscall_trap_consumer_unwatch_all(SyscallTrapConsumer *c)
{
    if (!c) {
        return;
    }
    qemu_mutex_lock(&sc_lock);
    if (c->live) {
        memset(c->watch_all, 0, sizeof(c->watch_all));
        memset(c->watch, 0, sizeof(c->watch));
        sc_rewatch_all();
    }
    qemu_mutex_unlock(&sc_lock);
}

unsigned syscall_trap_consumers(void)
{
    SCList *l;

    RCU_READ_LOCK_GUARD();
    l = qatomic_rcu_read(&sc_list);
    return l ? l->n : 0;
}

void syscall_trap_stats(uint64_t *pending, uint64_t *full, uint64_t *stale)
{
    qemu_mutex_lock(&sc_lock);
    *pending = qatomic_read(&syscall_trap_pending);
    *full = sc_full;
    *stale = sc_stale;
    sc_unlock();
}

bool syscall_trap_read(CPUState *cpu, vaddr addr, void *buf, size_t len)
{
    size_t done = 0;

#ifdef CONFIG_TCG
    /*
     * Pages the vCPU has in its TLB, copied straight from RAM: what the
     * guest itself would read.  In a syscall-event callback the vCPU is in
     * user mode, so also try the kernel's TLBs (same address space).
     */
    if (tcg_enabled()) {
        int idx = cpu_mmu_index(cpu, false);
        uint32_t extra = cpu->syscall_kmmu & ~(1u << idx);

        while (done < len) {
            vaddr a = addr + done;
            size_t chunk = MIN(len - done, qemu_target_page_size() -
                                           (a & ~qemu_target_page_mask()));
            void *h = tlb_vaddr_to_host_lookup(cpu, a, MMU_DATA_LOAD, idx);
            for (uint32_t m = extra; !h && m; m &= m - 1) {
                h = tlb_vaddr_to_host_lookup(cpu, a, MMU_DATA_LOAD, ctz32(m));
            }
            if (!h) {
                break;
            }
            memcpy((uint8_t *)buf + done, h, chunk);
            done += chunk;
        }
    }
#endif
    if (!replay_analysis_write_allowed() && done != len) {
        /* Debug translation does not run a guest fault handler. Check every
         * remaining page before cpu_memory_rw_debug can issue a device read. */
        for (size_t at = done; at < len;) {
            vaddr a = addr + at;
            TranslateForDebugResult tr;
            if (!cpu_translate_for_debug(cpu, a, &tr)) {
                return false;
            }
            uint64_t page = MIN(1ULL << tr.lg_page_size, qemu_target_page_size());
            size_t chunk = MIN(len - at, page - (tr.physaddr & (page - 1)));
            AddressSpace *as = cpu_get_address_space(cpu, cpu_asidx_from_attrs(cpu, tr.attrs));
            if (!replay_analysis_read_allowed(as, tr.physaddr, chunk, tr.attrs)) {
                return false;
            }
            at += chunk;
        }
    }
    return done == len ||
           cpu_memory_rw_debug(cpu, addr + done, (uint8_t *)buf + done,
                               len - done, false) == 0;
}

const void *syscall_trap_host_page(CPUState *cpu, vaddr page)
{
#ifdef CONFIG_TCG
    if (tcg_enabled()) {
        int idx = cpu_mmu_index(cpu, false);
        uint32_t idxs = (1u << idx) | cpu->syscall_kmmu;
        void *h = tlb_vaddr_to_host_lookup(cpu, page, MMU_DATA_LOAD, idx);

        for (idxs &= ~(1u << idx); !h && idxs; idxs &= idxs - 1) {
            h = tlb_vaddr_to_host_lookup(cpu, page, MMU_DATA_LOAD, ctz32(idxs));
        }
        return h;
    }
#endif
    return NULL;
}

#ifdef CONFIG_TCG
/* the host address of a page the vCPU may write, at any of its indexes */
static void *sc_writable(CPUState *cpu, vaddr a, size_t size)
{
    int idx = cpu_mmu_index(cpu, false);
    uint32_t idxs = (1u << idx) | cpu->syscall_kmmu;

    for (; idxs; idxs &= idxs - 1) {
        CPUTLBEntryFull *full;
        void *host;
        int flags = probe_access_full_mmu(cpu_env(cpu), a, size,
                                          MMU_DATA_STORE, ctz32(idxs),
                                          &host, &full);
        if (!(flags & (TLB_INVALID_MASK | TLB_MMIO)) && host) {
            return host;
        }
    }
    return NULL;
}
#endif

static bool sc_access(CPUState *cpu, vaddr addr, void *buf, size_t len,
                      bool write, vaddr *bad)
{
    size_t pg = qemu_target_page_size();

    if (!write) {
        for (size_t done = 0; done < len;) {
            vaddr a = addr + done;
            size_t chunk = MIN(len - done, pg - (a & (pg - 1)));

            if (!syscall_trap_read(cpu, a, (uint8_t *)buf + done, chunk)) {
                *bad = a & ~(vaddr)(pg - 1);
                return false;
            }
            done += chunk;
        }
        return true;
    }
#ifdef CONFIG_TCG
    if (!tcg_enabled()) {
        *bad = addr & ~(vaddr)(pg - 1);
        return false;
    }
    /* every page first: a write is all or nothing */
    for (int copy = 0; copy < 2; copy++) {
        for (size_t done = 0; done < len;) {
            vaddr a = addr + done;
            size_t chunk = MIN(len - done, pg - (a & (pg - 1)));
            /* with a size, also marks the page dirty, drops its code */
            void *h = sc_writable(cpu, a, copy ? chunk : 0);

            if (!h) {
                *bad = a & ~(vaddr)(pg - 1);
                return false;
            }
            if (copy) {
                memcpy(h, (uint8_t *)buf + done, chunk);
            }
            done += chunk;
        }
    }
    return true;
#else
    *bad = addr & ~(vaddr)(pg - 1);
    return false;
#endif
}

/*
 * During record/replay a write goes ahead only inside a hypercall answer, which
 * logs it here: replay repeats this same call (replay-effect.h).
 */
bool syscall_trap_access(CPUState *cpu, vaddr addr, void *buf, size_t len,
                         bool write, vaddr *bad)
{
    bool ok;

    if (write && !replay_analysis_write_allowed() &&
        replay_guest_write_admitted(NULL, len) != GUEST_WRITE_OK) {
        *bad = addr & ~(vaddr)(qemu_target_page_size() - 1);
        return false;
    }
    ok = sc_access(cpu, addr, buf, len, write, bad);
    if (write) {
        replay_guest_write_log_vaddr(cpu, addr, buf, len,
                                     ok ? GUEST_WRITE_OK : GUEST_WRITE_EFAULT);
    }
    return ok;
}
