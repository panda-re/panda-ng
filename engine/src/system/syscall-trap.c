/*
 * Syscall events at the trap (system mode, TCG)
 *
 * The target reports a user task's syscall trap and every return to user
 * mode while a call is pending; this file filters the traps by number,
 * hands each call to the consumers that watch it, and pairs it with its
 * return.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qemu/atomic.h"
#include "qemu/rcu.h"
#include "qemu/lockable.h"
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
#define SC_PARTS 8              /* parts on the stack while a call is delivered */

bool syscall_trap_on;
uint32_t syscall_trap_pending;

struct SyscallTrapConsumer {
    struct rcu_head rcu;        /* first: g_free_rcu */
    const SyscallTrapOps *ops;
    void *opaque;
    unsigned flags;
    uint64_t order;             /* modifiers first, then observers, each as added */
    bool removing;
    uint8_t watch_all[SYSCALL_ABI_COUNT];
    uint8_t watch[SYSCALL_ABI_COUNT][SYSCALL_TRAP_NUMS];
};

/* the consumers in delivery order, replaced whole */
typedef struct SCList {
    struct rcu_head rcu;
    unsigned n;
    SyscallTrapConsumer *c[];
} SCList;

static SCList *sc_list;
static QemuMutex sc_cons_lock;  /* changes to sc_list and to the watches */
static uint64_t sc_order;
static uint32_t sc_held;        /* calls held for a fault or an injected call */
static uint64_t sc_faults;
static __thread uint64_t sc_fault_addr;
static __thread bool sc_fault_write;

/* the union of the consumers' watches */
static uint8_t sc_watch[SYSCALL_ABI_COUNT][SYSCALL_TRAP_NUMS];
static uint8_t sc_watch_all[SYSCALL_ABI_COUNT];

static const bool sc_is32[SYSCALL_ABI_COUNT] = {
    [SYSCALL_ABI_I386] = true,
    [SYSCALL_ABI_ARM] = true,
    [SYSCALL_ABI_ARM_OABI] = true,
    [SYSCALL_ABI_PPC] = true,
    [SYSCALL_ABI_RISCV32] = true,
};

#define SC_X32 0x40000000

/*
 * fork, vfork, clone and clone3: a 0 return is the child's.  A vfork child
 * runs on the caller's stack, so its return matches the caller's call.
 */
static bool sc_forks(int abi, int64_t num)
{
    switch (abi) {
    case SYSCALL_ABI_X86_64:
        num &= ~(int64_t)SC_X32;
        return num == 56 || num == 57 || num == 58 || num == 435;
    case SYSCALL_ABI_I386:
    case SYSCALL_ABI_ARM:
    case SYSCALL_ABI_ARM_OABI:
        return num == 2 || num == 120 || num == 190 || num == 435;
    case SYSCALL_ABI_AARCH64:
        return num == 220 || num == 435;
    }
    return false;
}

static inline int64_t sc_sext(int abi, uint64_t v)
{
    return sc_is32[abi] ? (int32_t)v : (int64_t)v;
}

static inline uint64_t sc_trunc(int abi, int64_t v)
{
    return sc_is32[abi] ? (uint32_t)v : (uint64_t)v;
}

/*
 * SC_INJECT: held while the task runs a call in its place (inject);
 * SC_INJECT_RET: the same at its return, the task's return held
 */
enum { SC_PENDING, SC_HELD_ENTER, SC_HELD_RETURN, SC_INJECT, SC_INJECT_RET };

/* a consumer's part of a call */
typedef struct SCPart {
    SyscallTrapConsumer *c;
    uint64_t task;
    void *data;
    bool want_return;           /* its return is still to be delivered */
    bool keep;
} SCPart;

/* a call: what its consumers share, and their parts */
typedef struct SCCall {
    SyscallTrap sc;             /* as the modifiers have changed it so far */
    uint64_t regs[6];           /* the arguments in the registers at the trap */
    uint64_t holder;            /* held: the order of the consumer that asked */
    unsigned n, cap;
    bool heap;                  /* part is ours to free */
    SCPart *part;               /* in delivery order */
} SCCall;

typedef struct {
    uint64_t k1, k2, pc;        /* pc 0: empty */
    uint8_t len;
    uint8_t state;              /* waiting for its return, or held */
    uint64_t seq;               /* when it was added */
    SCCall call;
    int64_t num;                /* SC_INJECT*: the registers it replaced */
    uint64_t args[6];
} SCSlot;

static SCSlot sc_slots[SC_SLOTS];
static QemuMutex sc_lock;
static uint64_t sc_full, sc_stale, sc_seq;

static void __attribute__((constructor)) sc_init(void)
{
    qemu_mutex_init(&sc_lock);
    qemu_mutex_init(&sc_cons_lock);
}

/* @p's view of @call: what they share, and its own part */
static SyscallTrap sc_view(const SCCall *call, const SCPart *p)
{
    SyscallTrap v = call->sc;

    v.task = p->task;
    v.data = p->data;
    v.want_return = p->want_return;
    v.keep = v.fault = v.inject = v.resumed = false;
    return v;
}

static void sc_drop_part(SCCall *call, SCPart *p)
{
    if (p->data && p->c->ops->drop) {
        SyscallTrap v = sc_view(call, p);

        p->c->ops->drop(&v, p->c->opaque);
    }
    p->data = NULL;
}

static void sc_free_parts(SCCall *call)
{
    if (call->heap) {
        g_free(call->part);
    }
    call->part = NULL;
    call->n = call->cap = 0;
    call->heap = false;
}

/* the call ends: release every part */
static void sc_drop(SCCall *call)
{
    for (unsigned i = 0; i < call->n; i++) {
        sc_drop_part(call, &call->part[i]);
    }
    sc_free_parts(call);
}

/* parts discarded under sc_lock, dropped once it is released */
typedef struct {
    SyscallTrapConsumer *c;
    SyscallTrap sc;
} SCDropped;

static __thread GArray *sc_dropped;

static void sc_drop_part_later(SCCall *call, SCPart *p)
{
    if (p->data && p->c->ops->drop) {
        SCDropped d = { p->c, sc_view(call, p) };

        if (!sc_dropped) {
            sc_dropped = g_array_new(false, false, sizeof(SCDropped));
        }
        g_array_append_val(sc_dropped, d);
    }
    p->data = NULL;
}

static void sc_drop_later(SCCall *call)
{
    for (unsigned i = 0; i < call->n; i++) {
        sc_drop_part_later(call, &call->part[i]);
    }
    sc_free_parts(call);
}

/* run the drops queued by this thread; no lock held */
static void sc_drain(void)
{
    while (sc_dropped && sc_dropped->len) {
        SCDropped d = g_array_index(sc_dropped, SCDropped, sc_dropped->len - 1);

        g_array_set_size(sc_dropped, sc_dropped->len - 1);
        d.c->ops->drop(&d.sc, d.c->opaque);
    }
}

static void sc_unlock(void)
{
    qemu_mutex_unlock(&sc_lock);
    sc_drain();
}

/* room for @extra more parts */
static void sc_reserve(SCCall *call, unsigned extra)
{
    unsigned cap = call->n + extra;

    if (cap <= call->cap) {
        return;
    }
    if (call->heap) {
        call->part = g_renew(SCPart, call->part, cap);
    } else {
        SCPart *p = g_new(SCPart, cap);

        if (call->n) {
            memcpy(p, call->part, call->n * sizeof(SCPart));
        }
        call->part = p;
        call->heap = true;
    }
    call->cap = cap;
}

static SCPart *sc_part_of(SCCall *call, uint64_t order)
{
    for (unsigned i = 0; i < call->n; i++) {
        if (call->part[i].c->order == order) {
            return &call->part[i];
        }
    }
    return NULL;
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

/* remove the call at @s, shifting later calls of the cluster back */
static void sc_delete(SCSlot *s)
{
    unsigned i = s - sc_slots, j = i;

    sc_slots[i].pc = 0;
    for (;;) {
        j = (j + 1) & (SC_SLOTS - 1);
        if (!sc_slots[j].pc) {
            return;
        }
        unsigned h = sc_home(sc_slots[j].k1, sc_slots[j].k2, sc_slots[j].pc);
        if ((j > i && (h <= i || h > j)) || (j < i && h <= i && h > j)) {
            sc_slots[i] = sc_slots[j];
            sc_slots[j].pc = 0;
            i = j;
        }
    }
}

/* sc_lock held: take the call out of @s */
static SCCall sc_take(SCSlot *s)
{
    SCCall call = s->call;

    sc_forget(s);
    sc_delete(s);
    qatomic_dec(&syscall_trap_pending);
    return call;
}

/*
 * Calls whose return never comes back to the same task and address (a
 * successful execve) stay pending.  When the table is 3/4 full, drop the
 * calls added before the latest SC_SLOTS / 2.  Called with sc_lock held.
 */
static void sc_evict(void)
{
    g_autofree SCSlot *keep = g_new(SCSlot, SC_SLOTS);
    unsigned n = 0;

    for (unsigned i = 0; i < SC_SLOTS; i++) {
        if (sc_slots[i].pc) {
            /* a task running an injected call needs its registers back */
            if (sc_slots[i].seq + SC_SLOTS / 2 >= sc_seq ||
                sc_slots[i].state == SC_INJECT ||
                sc_slots[i].state == SC_INJECT_RET) {
                keep[n++] = sc_slots[i];
            } else {
                sc_forget(&sc_slots[i]);
                sc_drop_later(&sc_slots[i].call);
                sc_stale++;
            }
        }
    }
    memset(sc_slots, 0, sizeof(sc_slots));
    for (unsigned i = 0; i < n; i++) {
        *sc_find(keep[i].k1, keep[i].k2, keep[i].pc) = keep[i];
    }
    qatomic_set(&syscall_trap_pending, n);
}

/*
 * sc_lock held: the table takes @call (its parts too); NULL if it could not
 * keep it (dropped)
 */
static SCSlot *sc_put(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                      SCCall *call, int state)
{
    if (!call->heap && call->n) {
        call->part = g_memdup2(call->part, call->n * sizeof(SCPart));
        call->heap = true;
    }
    call->cap = call->n;
    if (qatomic_read(&syscall_trap_pending) >= SC_SLOTS * 3 / 4) {
        sc_evict();
    }
    SCSlot *s = sc_find(k1, k2, pc);
    if (!s) {
        sc_full++;
        sc_drop_later(call);
    } else {
        if (s->pc) {
            sc_stale++;         /* its return was lost */
            sc_forget(s);
            sc_drop_later(&s->call);
        } else {
            qatomic_inc(&syscall_trap_pending);
        }
        s->k1 = k1;
        s->k2 = k2;
        s->pc = pc;
        s->len = len;
        s->state = state;
        s->seq = ++sc_seq;
        s->call = *call;
        if (state != SC_PENDING) {
            qatomic_inc(&sc_held);
        }
    }
    return s;
}

static void sc_add(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                   SCCall *call, int state)
{
    qemu_mutex_lock(&sc_lock);
    sc_put(k1, k2, pc, len, call, state);
    sc_unlock();
}

/*
 * The call goes on waiting for the return of the parts that ask for it
 * (@kept: those that kept it at a return); the others are done.
 */
static void sc_pend(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                    SCCall *call, bool kept)
{
    unsigned n = 0;

    for (unsigned i = 0; i < call->n; i++) {
        SCPart *p = &call->part[i];

        if (kept ? p->keep : p->want_return) {
            p->want_return = true;
            p->keep = false;
            call->part[n++] = *p;
        } else {
            sc_drop_part(call, p);
        }
    }
    call->n = n;
    if (n) {
        sc_add(k1, k2, pc, len, call, SC_PENDING);
    } else {
        sc_free_parts(call);
    }
}

/*
 * Give @p's consumer its view of @call, at the return if @ret, else at the
 * entry.  A modifier's changes go into @call, for the consumers after it.
 * True if it asked for a fault or an injected call (@call->holder): the
 * call is to be held, and nothing else of it applies.
 */
static bool sc_deliver(CPUState *cpu, SCCall *call, SCPart *p, bool ret,
                       bool resumed, uint32_t kmmu)
{
    SyscallTrapConsumer *c = p->c;
    SyscallTrap v = sc_view(call, p);

    v.resumed = resumed;
    cpu->syscall_kmmu = kmmu;
    (ret ? c->ops->ret : c->ops->enter)(cpu, &v, c->opaque);
    cpu->syscall_kmmu = 0;
    p->task = v.task;
    p->data = v.data;
    if (ret) {
        p->keep = v.keep;
    } else {
        p->want_return = v.want_return && c->ops->ret;
    }
    if ((c->flags & SYSCALL_TRAP_OBSERVER) || !replay_analysis_write_allowed()) {
        /* deliver observations, but never skip/inject/rewrite the real call */
        return false;
    }
    if (v.fault || v.inject) {
        call->sc.fault = v.fault;
        call->sc.fault_write = v.fault_write;
        call->sc.fault_addr = v.fault_addr;
        call->sc.inject = v.inject;
        call->sc.inject_num = v.inject_num;
        memcpy(call->sc.inject_args, v.inject_args, sizeof(v.inject_args));
        call->holder = c->order;
        return true;
    }
    call->sc.ret = v.ret;
    if (!ret) {
        memcpy(call->sc.args, v.args, sizeof(v.args));
        call->sc.skip = v.skip;
    }
    return false;
}

/*
 * Deliver the entry of @call: to its holder again if @resumed, then to each
 * consumer after @after that watches the call.  True if one asked for a
 * hold.
 */
static bool sc_enter_run(CPUState *cpu, SCList *l, SCCall *call,
                         uint64_t after, bool resumed, uint32_t kmmu)
{
    int abi = call->sc.abi;
    int64_t num = call->sc.num;

    if (resumed) {
        SCPart *p = sc_part_of(call, after);
        bool held = p && p->c->ops->enter &&
                    sc_deliver(cpu, call, p, false, true, kmmu);

        call->sc.injected = false;      /* the holder's answer only */
        call->sc.inject_ret = 0;
        if (held) {
            return true;
        }
    }
    sc_reserve(call, l->n);
    for (unsigned i = 0; i < l->n; i++) {
        SyscallTrapConsumer *c = l->c[i];
        unsigned w = c->watch_all[abi];

        if (c->order <= after) {
            continue;
        }
        if (num >= 0 && num < SYSCALL_TRAP_NUMS) {
            w |= c->watch[abi][num];
        }
        if (!w) {
            continue;
        }
        SCPart *p = &call->part[call->n++];
        *p = (SCPart) {
            .c = c,
            .want_return = (w & SYSCALL_TRAP_RETURN) && c->ops->ret,
        };
        if ((w & SYSCALL_TRAP_ENTER) && c->ops->enter &&
            sc_deliver(cpu, call, p, false, false, kmmu)) {
            return true;
        }
    }
    return false;
}

/*
 * Deliver the return of @call to each part waiting for it, the holder's
 * first again if @resumed.  True if one asked for a hold.
 */
static bool sc_ret_run(CPUState *cpu, SCCall *call, bool resumed,
                       uint32_t kmmu)
{
    for (unsigned i = 0; i < call->n; i++) {
        SCPart *p = &call->part[i];
        bool again = resumed && p->c->order == call->holder;

        if (!p->want_return) {
            continue;
        }
        bool held = sc_deliver(cpu, call, p, true, again, kmmu);

        if (again) {
            call->sc.injected = false;
            call->sc.inject_ret = 0;
        }
        if (held) {
            return true;                /* it gets this return again */
        }
        p->want_return = false;
    }
    return false;
}

/* a skipped call: its return, for the consumers that ask for it */
static void sc_skipped(CPUState *cpu, SCCall *call, uint32_t kmmu)
{
    call->sc.skipped = true;
    for (unsigned i = 0; i < call->n; i++) {
        SCPart *p = &call->part[i];
        SyscallTrapConsumer *c = p->c;

        if (p->want_return && c->ops->ret &&
            (c->flags & SYSCALL_TRAP_SKIPPED)) {
            SyscallTrap v = sc_view(call, p);

            cpu->syscall_kmmu = kmmu;
            c->ops->ret(cpu, &v, c->opaque);
            cpu->syscall_kmmu = 0;
            p->task = v.task;
            p->data = v.data;
        }
    }
}

/* hold the call until the guest has handled the fault its holder asked for */
static int sc_hold(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                   SCCall *call, int state)
{
    sc_fault_addr = call->sc.fault_addr;
    sc_fault_write = call->sc.fault_write;
    call->sc.fault = false;
    sc_add(k1, k2, pc, len, call, state);
    return SYSCALL_TRAP_FAULT;
}

/*
 * Run the call the holder asked for in place of this one: keep the
 * registers it replaces with the held call; @args and *@ret become the
 * injected call's.  0 if the target cannot (the call goes on), -1 if the
 * table cannot keep it (the call is dropped, and runs as it is).
 */
static int sc_inject(CPUState *cpu, uint64_t k1, uint64_t k2, uint64_t pc,
                     unsigned len, SCCall *call, int64_t num,
                     uint64_t args[6], uint64_t *ret)
{
    int64_t inum = call->sc.inject_num;
    uint64_t iargs[6];
    SCSlot *s;

    memcpy(iargs, call->sc.inject_args, sizeof(iargs));
    call->sc.inject = false;
    call->sc.injected = false;
    /* the target puts the registers back for these ABIs only */
    if (!(cpu->syscall_inject_abis & (1u << call->sc.abi))) {
        return 0;
    }
    qemu_mutex_lock(&sc_lock);
    s = sc_put(k1, k2, pc, len, call, SC_INJECT);
    if (s) {
        s->num = num;
        memcpy(s->args, args, sizeof(s->args));
    }
    sc_unlock();
    if (!s) {
        return -1;
    }
    *ret = inum;
    memcpy(args, iargs, sizeof(iargs));
    return 1;
}

/*
 * The call held at the trap, if this is the task coming back to it:
 * SC_HELD_ENTER, taken; SC_INJECT, a restart of the injected call (left
 * alone); or -1.
 */
static int sc_take_held(uint64_t k1, uint64_t k2, uint64_t pc, SCCall *call)
{
    int found = -1;

    qemu_mutex_lock(&sc_lock);
    SCSlot *s = sc_find(k1, k2, pc);
    if (s && s->pc && s->state == SC_HELD_ENTER) {
        *call = sc_take(s);
        found = SC_HELD_ENTER;
    } else if (s && s->pc && (s->state == SC_INJECT ||
                              s->state == SC_INJECT_RET)) {
        found = SC_INJECT;
    }
    sc_unlock();
    return found;
}

int syscall_trap_enter(CPUState *cpu, int abi, int64_t num, uint64_t args[6],
                       uint64_t pc, unsigned len, uint64_t k1, uint64_t k2,
                       uint64_t percpu, uint32_t kmmu, uint64_t *ret)
{
    SCPart buf[SC_PARTS];
    SCCall call;
    uint64_t after = 0;
    bool resumed = false;
    SCList *l;
    int r = 0;

    RCU_READ_LOCK_GUARD();
    l = qatomic_rcu_read(&sc_list);
    if (!l) {
        return 0;
    }
    int held = qatomic_read(&sc_held) ? sc_take_held(k1, k2, pc, &call) : -1;

    if (held == SC_INJECT) {
        return 0;               /* the kernel restarts the injected call */
    }
    if (held == SC_HELD_ENTER) {
        if (call.sc.abi == abi && call.sc.num == num &&
            !memcmp(call.regs, args, sizeof(call.regs))) {
            /* back after the fault or the injected call */
            call.sc.percpu = percpu;
            after = call.holder;
            resumed = true;
        } else {
            sc_drop(&call);     /* another call at the same address */
        }
    }
    if (!resumed) {
        unsigned w = sc_watch_all[abi];

        if (num >= 0 && num < SYSCALL_TRAP_NUMS) {
            w |= sc_watch[abi][num];
        }
        if (!w) {
            return 0;
        }
        call = (SCCall) {
            .sc = { .abi = abi, .num = num, .pc = pc, .percpu = percpu },
            .part = buf, .cap = ARRAY_SIZE(buf),
        };
        memcpy(call.sc.args, args, sizeof(call.sc.args));
        memcpy(call.regs, args, sizeof(call.regs));
    }
    while (sc_enter_run(cpu, l, &call, after, resumed, kmmu)) {
        if (call.sc.fault) {
            return sc_hold(k1, k2, pc, len, &call, SC_HELD_ENTER);
        }
        int i = sc_inject(cpu, k1, k2, pc, len, &call, num, args, ret);
        if (i) {
            return i > 0 ? SYSCALL_TRAP_INJECT : 0;
        }
        /* refused: the holder's part ends, the others go on */
        SCPart *p = sc_part_of(&call, call.holder);
        sc_drop_part(&call, p);
        p->want_return = false;
        after = call.holder;
        resumed = false;
    }
    if (memcmp(call.sc.args, args, sizeof(call.sc.args))) {
        memcpy(args, call.sc.args, sizeof(call.sc.args));
        r |= SYSCALL_TRAP_ARGS;
    }
    if (call.sc.skip) {
        *ret = sc_trunc(abi, call.sc.ret);
        sc_skipped(cpu, &call, kmmu);
        sc_drop(&call);
        return r | SYSCALL_TRAP_SKIP;
    }
    sc_pend(k1, k2, pc, len, &call, false);
    return r;
}

/*
 * The holder of a return asked for an injected call: hold the return as
 * SC_INJECT_RET with the call's own number and arguments (@onum, @oargs)
 * and send the task back to the trap instruction with the injected call's
 * (REWIND).  0 if the target cannot (the run goes on), -1 if the table
 * cannot keep it (dropped).
 */
static int sc_inject_ret(CPUState *cpu, uint64_t k1, uint64_t k2,
                         uint64_t key, unsigned len, SCCall *call,
                         int64_t onum, const uint64_t oargs[6],
                         SyscallTrapRewind *rw)
{
    int abi = call->sc.abi;
    int64_t inum = call->sc.inject_num;
    uint64_t iargs[6];
    SCSlot *s;

    memcpy(iargs, call->sc.inject_args, sizeof(iargs));
    call->sc.inject = false;
    call->sc.injected = false;
    if (call->sc.restart || !(cpu->syscall_inject_abis & (1u << abi))) {
        return 0;
    }
    qemu_mutex_lock(&sc_lock);
    s = sc_put(k1, k2, key, len, call, SC_INJECT_RET);
    if (s) {
        s->num = onum;
        memcpy(s->args, oargs, sizeof(s->args));
    }
    sc_unlock();
    if (!s) {
        return -1;
    }
    rw->abi = abi;
    rw->num = inum;
    memcpy(rw->args, iargs, sizeof(rw->args));
    rw->len = len;
    return 1;
}

/* back from a call injected at the return: the task's own registers */
static int sc_restore(int abi, int64_t onum, const uint64_t oargs[6],
                      int64_t v, uint64_t *ret, SyscallTrapRewind *rw)
{
    rw->abi = abi;
    rw->num = onum;
    memcpy(rw->args, oargs, sizeof(rw->args));
    rw->len = 0;
    *ret = sc_trunc(abi, v);
    return SYSCALL_TRAP_RESTORE | SYSCALL_TRAP_RET;
}

int syscall_trap_return(CPUState *cpu, uint64_t pc, uint64_t k1, uint64_t k2,
                        uint64_t percpu, uint32_t kmmu, uint64_t *ret,
                        SyscallTrapRewind *rw)
{
    SCCall call;
    SCSlot *s;
    uint64_t key, oargs[6];
    int64_t onum, v;
    unsigned len;
    bool restart = false, resumed = false, restore = false;
    int r = 0;

    RCU_READ_LOCK_GUARD();
    qemu_mutex_lock(&sc_lock);
    s = sc_find(k1, k2, pc);
    if (s && !s->pc) {
        s = NULL;
    }
    if (!replay_analysis_write_allowed() && s &&
        (s->state == SC_INJECT || s->state == SC_INJECT_RET)) {
        /* Host-side state from before a snapshot/replay transition must not
         * rewind a task in the replay. */
        call = sc_take(s);
        sc_drop_later(&call);
        sc_unlock();
        return 0;
    }
    if (s && s->state == SC_INJECT) {
        /*
         * The injected call is back (after any signal handler it ran into:
         * sigreturn brings the task back here).  The task goes back to the
         * trap, its own call held there with the answer.
         */
        s->call.sc.injected = true;
        s->call.sc.inject_ret = sc_sext(s->call.sc.abi, *ret);
        s->state = SC_HELD_ENTER;
        rw->abi = s->call.sc.abi;
        rw->num = s->num;
        memcpy(rw->args, s->args, sizeof(rw->args));
        rw->len = s->len;
        sc_unlock();
        return SYSCALL_TRAP_REWIND;
    }
    if (s && s->state == SC_INJECT_RET) {
        /*
         * A call injected at a return is back: the held return run gets its
         * answer, and the task its own call's registers and return value.
         */
        s->call.sc.injected = true;
        s->call.sc.inject_ret = sc_sext(s->call.sc.abi, *ret);
        restore = true;
    } else if (!s || s->state == SC_HELD_ENTER) {
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
            sc_unlock();
            return 0;
        }
        restart = true;
    } else if (s->state == SC_PENDING &&
               sc_forks(s->call.sc.abi, s->call.sc.num) &&
               sc_sext(s->call.sc.abi, *ret) == 0) {
        /* a vfork child, on its parent's stack: the parent returns later */
        sc_unlock();
        return 0;
    }
    resumed = s->state != SC_PENDING;
    onum = s->num;
    memcpy(oargs, s->args, sizeof(oargs));
    len = s->len;
    key = s->pc;
    call = sc_take(s);
    sc_unlock();

    int abi = call.sc.abi;
    v = sc_sext(abi, *ret);
    if (!restore) {
        onum = call.sc.num;
        memcpy(oargs, call.sc.args, sizeof(oargs));
    }
    if (!resumed) {
        call.sc.ret = v;
        call.sc.restart = restart;
    }
    restart = call.sc.restart;
    call.sc.percpu = percpu;
    call.sc.skipped = false;
    while (sc_ret_run(cpu, &call, resumed, kmmu)) {
        resumed = false;
        if (call.sc.inject) {
            int i = sc_inject_ret(cpu, k1, k2, key, len, &call, onum, oargs,
                                  rw);
            if (i > 0) {
                return SYSCALL_TRAP_REWIND;
            }
            if (i < 0) {
                return restore ? sc_restore(abi, onum, oargs, call.sc.ret,
                                            ret, rw) : 0;
            }
            /* refused: the holder's run ends, the others go on */
            SCPart *p = sc_part_of(&call, call.holder);
            sc_drop_part(&call, p);
            p->want_return = false;
            continue;
        }
        r = sc_hold(k1, k2, key, len, &call, SC_HELD_RETURN);
        if (restore) {
            r |= sc_restore(abi, onum, oargs, call.sc.ret, ret, rw);
        }
        return r;
    }
    if (restore) {
        r = sc_restore(abi, onum, oargs, call.sc.ret, ret, rw);
    } else if (!restart && call.sc.ret != v) {
        *ret = sc_trunc(abi, call.sc.ret);
        r = SYSCALL_TRAP_RET;
    }
    if (restart || restore) {
        for (unsigned i = 0; i < call.n; i++) {
            call.part[i].keep = false;
        }
    }
    call.sc.ret = 0;
    call.sc.restart = false;
    sc_pend(k1, k2, key, len, &call, true);
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

unsigned syscall_trap_forget_task(uint64_t task)
{
    unsigned n = 0;

    if (!task || !syscall_trap_waiting()) {
        return 0;
    }
    qemu_mutex_lock(&sc_lock);
    for (unsigned i = 0; i < SC_SLOTS;) {
        SCSlot *s = &sc_slots[i];
        bool mine = false;

        for (unsigned j = 0; s->pc && j < s->call.n && !mine; j++) {
            mine = s->call.part[j].task == task;
        }
        if (mine) {
            SCCall call = sc_take(s);   /* may move a later call into slot i */

            sc_drop_later(&call);
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
            sc_drop_later(&sc_slots[i].call);
            n++;
        }
    }
    memset(sc_slots, 0, sizeof(sc_slots));
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
    sc_unlock();  /* Penguin's drop -> ABORT -> free chain runs outside the lock. */
    return n;
}

/* sc_cons_lock held: publish @l in place of the list */
static void sc_publish(SCList *l)
{
    SCList *old = sc_list;

    qatomic_rcu_set(&sc_list, l);
    if (old) {
        g_free_rcu(old, rcu);
    }
}

/* sc_cons_lock held: the union of the watches for @num (-1: all) of @abi */
static void sc_union(int abi, int64_t num)
{
    uint8_t w = 0;

    for (unsigned i = 0; sc_list && i < sc_list->n; i++) {
        SyscallTrapConsumer *c = sc_list->c[i];
        w |= num < 0 ? c->watch_all[abi] : c->watch[abi][num];
    }
    if (num < 0) {
        qatomic_set(&sc_watch_all[abi], w);
    } else {
        qatomic_set(&sc_watch[abi][num], w);
    }
}

/* sc_cons_lock held */
static void sc_union_all(void)
{
    for (int abi = 0; abi < SYSCALL_ABI_COUNT; abi++) {
        sc_union(abi, -1);
        for (int64_t num = 0; num < SYSCALL_TRAP_NUMS; num++) {
            sc_union(abi, num);
        }
    }
}

SyscallTrapConsumer *syscall_trap_add_consumer(const SyscallTrapOps *ops,
                                               void *opaque, unsigned flags)
{
    SyscallTrapConsumer *c = g_new0(SyscallTrapConsumer, 1);
    unsigned n, at;
    SCList *l;

    c->ops = ops;
    c->opaque = opaque;
    c->flags = flags;
    QEMU_LOCK_GUARD(&sc_cons_lock);
    c->order = ++sc_order;
    if (flags & SYSCALL_TRAP_OBSERVER) {
        c->order |= 1ull << 63;
    }
    n = sc_list ? sc_list->n : 0;
    l = g_malloc(sizeof(SCList) + (n + 1) * sizeof(l->c[0]));
    l->n = n + 1;
    for (at = 0; at < n && sc_list->c[at]->order < c->order; at++) {
        l->c[at] = sc_list->c[at];
    }
    l->c[at] = c;
    for (unsigned i = at; i < n; i++) {
        l->c[i + 1] = sc_list->c[i];
    }
    sc_publish(l);
    return c;
}

bool syscall_trap_remove_consumer(SyscallTrapConsumer *c)
{
    unsigned n = 0;
    SCList *l;
    bool last;

    /* Caller owns main-thread/BQL quiescence: no vCPU is inside a callback.
     * Live removal cannot abandon injected register restoration: the task
     * must finish its injected call under its holder. */
    qemu_mutex_lock(&sc_cons_lock);
    for (unsigned i = 0; sc_list && i < sc_list->n; i++) {
        n += sc_list->c[i] == c;
    }
    if (!c || !n || c->removing) {
        qemu_mutex_unlock(&sc_cons_lock);
        return false;
    }
    qemu_mutex_lock(&sc_lock);
    for (unsigned i = 0; i < SC_SLOTS; i++) {
        if (sc_slots[i].pc && sc_slots[i].call.holder == c->order &&
            (sc_slots[i].state == SC_INJECT ||
             sc_slots[i].state == SC_INJECT_RET)) {
            qemu_mutex_unlock(&sc_lock);
            qemu_mutex_unlock(&sc_cons_lock);
            return false; /* no watch, list, counter or slot changes */
        }
    }
    c->removing = true;
    /* its parts go; a held call stays, for the consumers after its holder */
    for (unsigned i = 0; i < SC_SLOTS;) {
        SCSlot *s = &sc_slots[i];
        unsigned k = 0;

        if (!s->pc) {
            i++;
            continue;
        }
        for (unsigned j = 0; j < s->call.n; j++) {
            if (s->call.part[j].c == c) {
                sc_drop_part_later(&s->call, &s->call.part[j]);
            } else {
                s->call.part[k++] = s->call.part[j];
            }
        }
        s->call.n = k;
        if (!k && s->state == SC_PENDING) {
            SCCall call = sc_take(s);   /* may move a later call into slot i */

            sc_free_parts(&call);
        } else {
            i++;
        }
    }
    qemu_mutex_unlock(&sc_lock);

    l = g_malloc(sizeof(SCList) + sc_list->n * sizeof(l->c[0]));
    l->n = 0;
    for (unsigned i = 0; i < sc_list->n; i++) {
        if (sc_list->c[i] != c) {
            l->c[l->n++] = sc_list->c[i];
        }
    }
    if (!l->n) {
        g_free(l);
        l = NULL;
    }
    sc_publish(l);
    sc_union_all();
    last = !l;
    if (last) {
        qatomic_set(&syscall_trap_on, false);
    }
    qemu_mutex_unlock(&sc_cons_lock);
    /* its drops run outside both locks; it may call in (and is refused) */
    if (last) {
        syscall_trap_cancel_all();
    }
    sc_drain();
    g_free_rcu(c, rcu);
    return true;
}

bool syscall_trap_watch(SyscallTrapConsumer *c, int abi, int64_t num,
                        unsigned flags)
{
    if (!c || abi < 0 || abi >= SYSCALL_ABI_COUNT || num < -1 ||
        num >= SYSCALL_TRAP_NUMS) {
        return false;
    }
    flags &= SYSCALL_TRAP_ENTER | SYSCALL_TRAP_RETURN;
    QEMU_LOCK_GUARD(&sc_cons_lock);
    if (c->removing) {
        return false;
    }
    if (num < 0) {
        c->watch_all[abi] = flags;
    } else {
        c->watch[abi][num] = flags;
    }
    sc_union(abi, num);
    if (flags) {
        qatomic_set(&syscall_trap_on, true);
    }
    return true;
}

void syscall_trap_unwatch_all(SyscallTrapConsumer *c)
{
    QEMU_LOCK_GUARD(&sc_cons_lock);
    memset(c->watch, 0, sizeof(c->watch));
    memset(c->watch_all, 0, sizeof(c->watch_all));
    sc_union_all();
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
