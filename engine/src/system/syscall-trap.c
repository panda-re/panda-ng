/*
 * Syscall events at the trap (system mode, TCG)
 *
 * The target reports a user task's syscall trap and every return to user
 * mode while a call is pending; this file filters the traps by number,
 * calls the consumer, and pairs each call with its return.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qemu/atomic.h"
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

bool syscall_trap_on;
uint32_t syscall_trap_pending;
uint32_t syscall_trap_epoch;

static const SyscallTrapOps *sc_ops;
static uint32_t sc_held;        /* calls held for a fault */
static uint64_t sc_faults;
static __thread uint64_t sc_fault_addr;
static __thread bool sc_fault_write;
static void *sc_opaque;
static bool sc_detaching;

static uint8_t sc_watch[SYSCALL_ABI_COUNT][SYSCALL_TRAP_NUMS];
static uint8_t sc_watch_all[SYSCALL_ABI_COUNT];

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
    /* SC_PENDING: its task's first return after it went to a signal handler */
    bool signalled;
    uint64_t seq;               /* when it was added */
    SyscallTrap sc;
    int64_t num;                /* SC_INJECT*: the registers it replaced */
    uint64_t args[6];
} SCSlot;

static SCSlot sc_slots[SC_SLOTS];
static QemuMutex sc_lock;
static uint64_t sc_full, sc_stale, sc_seq;

static void __attribute__((constructor)) sc_init(void)
{
    qemu_mutex_init(&sc_lock);
}

static void sc_drop(SyscallTrap *sc)
{
    if (sc->data && sc_ops->drop) {
        sc_ops->drop(sc, sc_opaque);
    }
    sc->data = NULL;
}

/* calls discarded under sc_lock, dropped once it is released */
static __thread GArray *sc_dropped;

static void sc_drop_later(SyscallTrap *sc)
{
    if (sc->data) {
        if (!sc_dropped) {
            sc_dropped = g_array_new(false, false, sizeof(SyscallTrap));
        }
        g_array_append_val(sc_dropped, *sc);
        sc->data = NULL;
    }
}

static void sc_unlock(void)
{
    qemu_mutex_unlock(&sc_lock);
    while (sc_dropped && sc_dropped->len) {
        SyscallTrap sc = g_array_index(sc_dropped, SyscallTrap,
                                       sc_dropped->len - 1);
        g_array_set_size(sc_dropped, sc_dropped->len - 1);
        sc_drop(&sc);
    }
}

/* sc_lock held: forget slot @s */
static void sc_forget(SCSlot *s)
{
    if (s->state != SC_PENDING) {
        qatomic_dec(&sc_held);
    }
}

/* by task only: all of a task's calls are in the cluster at its home slot */
static inline unsigned sc_home(uint64_t k1)
{
    uint64_t h = k1;

    h = (h ^ h >> 30) * 0xbf58476d1ce4e5b9ull;
    h = (h ^ h >> 27) * 0x94d049bb133111ebull;
    return (h ^ h >> 31) & (SC_SLOTS - 1);
}

/* the slot holding the call, or the empty slot where it would go */
static SCSlot *sc_find(uint64_t k1, uint64_t k2, uint64_t pc)
{
    unsigned i = sc_home(k1);

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
        unsigned h = sc_home(sc_slots[j].k1);
        if ((j > i && (h <= i || h > j)) || (j < i && h <= i && h > j)) {
            sc_slots[i] = sc_slots[j];
            sc_slots[j].pc = 0;
            i = j;
        }
    }
}

/*
 * Calls whose return never comes back (a handler that does not return, a
 * task that dies in a call) stay pending.  When the table is 3/4 full, drop
 * the calls added before the latest SC_SLOTS / 2.  Called with sc_lock held.
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
                sc_drop_later(&sc_slots[i].sc);
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

/* sc_lock held; NULL if the call could not be kept (dropped) */
static SCSlot *sc_put(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                      SyscallTrap *sc, int state)
{
    if (qatomic_read(&syscall_trap_pending) >= SC_SLOTS * 3 / 4) {
        sc_evict();
    }
    SCSlot *s = sc_find(k1, k2, pc);
    if (!s) {
        sc_full++;
        sc_drop_later(sc);
    } else {
        if (s->pc) {
            sc_stale++;         /* its return was lost */
            sc_forget(s);
            sc_drop_later(&s->sc);
        } else {
            qatomic_inc(&syscall_trap_pending);
        }
        s->k1 = k1;
        s->k2 = k2;
        s->pc = pc;
        s->len = len;
        s->state = state;
        s->signalled = false;
        s->seq = ++sc_seq;
        s->sc = *sc;
        if (state != SC_PENDING) {
            qatomic_inc(&sc_held);
        }
    }
    return s;
}

static void sc_add(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                   SyscallTrap *sc, int state)
{
    qemu_mutex_lock(&sc_lock);
    sc_put(k1, k2, pc, len, sc, state);
    sc_unlock();
}

/* sc_lock held: discard the call at @s, without its return */
static void sc_remove(SCSlot *s)
{
    sc_forget(s);
    sc_drop_later(&s->sc);
    sc_delete(s);
    qatomic_dec(&syscall_trap_pending);
}

/*
 * sc_lock held: discard task @k1's calls waiting for their return for which
 * @gone(slot, @arg) holds; the ones it returns false for may be changed by
 * it.  Returns how many were discarded.
 */
static unsigned sc_task_calls(uint64_t k1, bool (*gone)(SCSlot *, uint64_t),
                              uint64_t arg)
{
    unsigned i = sc_home(k1), n = 0;

    if (k1 & SYSCALL_TRAP_KEY_AS) {
        return 0;               /* not a task: its calls are not known */
    }
    for (unsigned seen = 0; sc_slots[i].pc && seen < SC_SLOTS; seen++) {
        SCSlot *s = &sc_slots[i];

        if (s->k1 == k1 && s->state == SC_PENDING && gone(s, arg)) {
            sc_remove(s);       /* may move a later call into slot i */
            n++;
        } else {
            i = (i + 1) & (SC_SLOTS - 1);
        }
    }
    return n;
}

static bool sc_no_first_return(SCSlot *s, uint64_t arg)
{
    return !s->signalled;
}

static bool sc_newer(SCSlot *s, uint64_t seq)
{
    return s->seq > seq;
}

/*
 * A call's entry, waiting for its return.  The task is in user mode: a call
 * of its own still waiting for its first return never got it (the task
 * died meanwhile, and this is a new task on its kernel stack).
 */
static void sc_add_call(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                        SyscallTrap *sc)
{
    qemu_mutex_lock(&sc_lock);
    sc_stale += sc_task_calls(k1, sc_no_first_return, 0);
    sc_put(k1, k2, pc, len, sc, SC_PENDING);
    sc_unlock();
}

/* hold the call until the guest has handled the fault it asked for */
static int sc_hold(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                   SyscallTrap *sc, int state)
{
    sc_fault_addr = sc->fault_addr;
    sc_fault_write = sc->fault_write;
    sc->fault = false;
    sc_add(k1, k2, pc, len, sc, state);
    return SYSCALL_TRAP_FAULT;
}

/*
 * Run the call the consumer asked for in place of this one: keep the
 * registers it replaces with the held call; @args and *@ret become the
 * injected call's.  False if the table cannot keep it (the call is
 * dropped, and runs as it is).
 */
static bool sc_inject(CPUState *cpu, uint64_t k1, uint64_t k2, uint64_t pc,
                      unsigned len, SyscallTrap *sc, int64_t num,
                      uint64_t args[6], uint64_t *ret)
{
    int64_t inum = sc->inject_num;
    uint64_t iargs[6];
    SCSlot *s;

    memcpy(iargs, sc->inject_args, sizeof(iargs));
    sc->inject = false;
    sc->injected = false;
    /* the target puts the registers back for these ABIs only */
    if (!(cpu->syscall_inject_abis & (1u << sc->abi))) {
        sc_drop(sc);
        return false;
    }
    qemu_mutex_lock(&sc_lock);
    s = sc_put(k1, k2, pc, len, sc, SC_INJECT);
    if (s) {
        s->num = num;
        memcpy(s->args, args, sizeof(s->args));
    }
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
 * SC_HELD_ENTER, taken; SC_INJECT, a restart of the injected call (left
 * alone); or -1.
 */
static int sc_take_held(uint64_t k1, uint64_t k2, uint64_t pc,
                        SyscallTrap *sc)
{
    int found = -1;

    qemu_mutex_lock(&sc_lock);
    SCSlot *s = sc_find(k1, k2, pc);
    if (s && s->pc && s->state == SC_HELD_ENTER) {
        *sc = s->sc;
        sc_forget(s);
        sc_delete(s);
        qatomic_dec(&syscall_trap_pending);
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
    const SyscallTrapOps *ops = qatomic_read(&sc_ops);
    unsigned w = sc_watch_all[abi];
    SyscallTrap sc;
    int r = 0;

    if (!ops) {
        return 0;
    }
    int held = qatomic_read(&sc_held) ? sc_take_held(k1, k2, pc, &sc) : -1;

    if (held == SC_INJECT) {
        return 0;               /* the kernel restarts the injected call */
    }
    if (held == SC_HELD_ENTER) {
        if (sc.abi == abi && sc.num == num) {
            /* back after the fault: the registers are as they were */
            sc.resumed = true;
            sc.percpu = percpu;
            memcpy(sc.args, args, sizeof(sc.args));
            w = SYSCALL_TRAP_ENTER;
            goto deliver;
        }
        sc_drop(&sc);           /* another call at the same address */
    }
    if (num >= 0 && num < SYSCALL_TRAP_NUMS) {
        w |= sc_watch[abi][num];
    }
    if (!w) {
        return 0;
    }

    sc = (SyscallTrap) {
        .abi = abi, .num = num, .pc = pc, .percpu = percpu,
        .want_return = (w & SYSCALL_TRAP_RETURN) && ops->ret,
    };
    memcpy(sc.args, args, sizeof(sc.args));
deliver:
    if ((w & SYSCALL_TRAP_ENTER) && ops->enter) {
        cpu->syscall_kmmu = kmmu;
        ops->enter(cpu, &sc, sc_opaque);
        cpu->syscall_kmmu = 0;
        if (!replay_analysis_write_allowed()) {
            /* Deliver observations, but never skip/inject/rewrite the real call. */
            sc.fault = sc.inject = sc.skip = false;
            memcpy(sc.args, args, sizeof(sc.args));
        }
        if (sc.fault) {
            return sc_hold(k1, k2, pc, len, &sc, SC_HELD_ENTER);
        }
        if (sc.inject) {
            return sc_inject(cpu, k1, k2, pc, len, &sc, num, args, ret)
                   ? SYSCALL_TRAP_INJECT : 0;
        }
        if (memcmp(sc.args, args, sizeof(sc.args))) {
            memcpy(args, sc.args, sizeof(sc.args));
            r |= SYSCALL_TRAP_ARGS;
        }
        if (sc.skip) {
            sc_drop(&sc);
            *ret = sc_is32[abi] ? (uint32_t)sc.ret : (uint64_t)sc.ret;
            return r | SYSCALL_TRAP_SKIP;
        }
    }
    if (sc.want_return && ops->ret) {
        sc_add_call(k1, k2, pc, len, &sc);
    } else {
        sc_drop(&sc);
    }
    return r;
}

/*
 * A return run that asked for an injected call (sc->inject), or one that is
 * back from one (@restore: the task has the injected call's registers).
 * Inject: hold the return as SC_INJECT_RET with the call's own number and
 * arguments (@onum, @oargs) and send the task back to the trap instruction
 * with the injected call's (REWIND).  Otherwise, with @restore, the task
 * gets its own call's registers back and the run's return value (RESTORE,
 * RET), and is held for a fault or done.
 */
static int sc_ret_after(CPUState *cpu, uint64_t k1, uint64_t k2,
                        uint64_t key, unsigned len, SyscallTrap *sc,
                        int64_t onum, const uint64_t oargs[6], bool restore,
                        uint64_t *ret, SyscallTrapRewind *rw)
{
    int r = 0;

    if (sc->inject) {
        SCSlot *s = NULL;

        sc->inject = false;
        sc->injected = false;
        if (!sc->restart && (cpu->syscall_inject_abis & (1u << sc->abi))) {
            qemu_mutex_lock(&sc_lock);
            s = sc_put(k1, k2, key, len, sc, SC_INJECT_RET);
            if (s) {
                s->num = onum;
                memcpy(s->args, oargs, sizeof(s->args));
            }
            sc_unlock();
        }
        if (s) {
            rw->abi = sc->abi;
            rw->num = sc->inject_num;
            memcpy(rw->args, sc->inject_args, sizeof(rw->args));
            rw->len = len;
            return SYSCALL_TRAP_REWIND;
        }
        sc_drop(sc);            /* the run is aborted */
    } else if (sc->fault) {
        r = sc_hold(k1, k2, key, len, sc, SC_HELD_RETURN);
    } else {
        sc_drop(sc);
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

/*
 * The calls that replace the task's context (sigreturn, execve): when they
 * do not fail, the task's next return to user mode is not past the call.
 */
static bool sc_replaces_context(int abi, int64_t num)
{
    switch (abi) {
    case SYSCALL_ABI_X86_64:
        /* rt_sigreturn, execve, execveat; and x32's */
        return num == 15 || num == 59 || num == 322 ||
               num == 0x40000000 + 513 || num == 0x40000000 + 520 ||
               num == 0x40000000 + 545;
    case SYSCALL_ABI_I386:
        return num == 119 || num == 173 || num == 11 || num == 358;
    case SYSCALL_ABI_AARCH64:
        return num == 139 || num == 221 || num == 281;
    case SYSCALL_ABI_ARM:
    case SYSCALL_ABI_ARM_OABI:
        return num == 119 || num == 173 || num == 11 || num == 387;
    default:
        return false;
    }
}

/*
 * A return to user mode that is none of the task's calls': the first return
 * of its call waiting for one, if it has one, went elsewhere.
 */
static bool sc_first_return_elsewhere(SCSlot *s, uint64_t arg)
{
    if (s->signalled) {
        return false;
    }
    if (sc_replaces_context(s->sc.abi, s->sc.num)) {
        return true;            /* it did not return */
    }
    s->signalled = true;        /* a signal handler runs first */
    return false;
}

int syscall_trap_return(CPUState *cpu, uint64_t pc, uint64_t k1, uint64_t k2,
                        uint64_t percpu, uint32_t kmmu, uint64_t *ret,
                        SyscallTrapRewind *rw)
{
    const SyscallTrapOps *ops = qatomic_read(&sc_ops);
    SyscallTrap sc;
    SCSlot *s;
    uint64_t key;
    unsigned len = 0;
    bool restart = false, resumed;

    qemu_mutex_lock(&sc_lock);
    s = sc_find(k1, k2, pc);
    if (s && !s->pc) {
        s = NULL;
    }
    if (!replay_analysis_write_allowed() && s && s->pc &&
        (s->state == SC_INJECT || s->state == SC_INJECT_RET)) {
        /* Host-side state from before a snapshot/replay transition must not
         * rewind a task in the replay. */
        sc_drop_later(&s->sc);
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
        sc_task_calls(k1, sc_newer, s->seq);
        sc_unlock();
        return SYSCALL_TRAP_REWIND;
    }
    if (s && s->pc && s->state == SC_INJECT_RET) {
        /*
         * A call injected at a return is back: the held return run gets its
         * answer, and the task its own call's registers and return value.
         */
        int64_t onum = s->num;
        uint64_t oargs[6], seq = s->seq;

        sc = s->sc;
        memcpy(oargs, s->args, sizeof(oargs));
        len = s->len;
        key = s->pc;
        sc_forget(s);
        sc_delete(s);
        qatomic_dec(&syscall_trap_pending);
        sc_task_calls(k1, sc_newer, seq);
        sc_unlock();
        if (!ops || !ops->ret) {
            sc_drop(&sc);
            return 0;
        }
        sc.injected = true;
        sc.inject_ret = sc_is32[sc.abi] ? (int32_t)*ret : (int64_t)*ret;
        sc.resumed = true;
        sc.percpu = percpu;
        cpu->syscall_kmmu = kmmu;
        ops->ret(cpu, &sc, sc_opaque);
        cpu->syscall_kmmu = 0;
        return sc_ret_after(cpu, k1, k2, key, len, &sc, onum, oargs, true,
                            ret, rw);
    }
    if (!s || s->state == SC_HELD_ENTER) {
        /*
         * The kernel rewound the task to the trap: a restart, or the
         * return from a fault raised at the trap (that call is held).
         */
        static const unsigned lens[] = { 2, 4 };
        bool held = s != NULL;

        s = NULL;
        for (int i = 0; i < ARRAY_SIZE(lens) && !s; i++) {
            SCSlot *t = sc_find(k1, k2, pc + lens[i]);
            if (t && t->pc && t->len == lens[i]) {
                if (t->state == SC_HELD_ENTER) {
                    held = true;
                } else if (t->state != SC_INJECT && t->state != SC_INJECT_RET) {
                    s = t;
                }
            }
        }
        if (!s) {
            if (!held) {
                /* none of the task's calls returns here */
                sc_task_calls(k1, sc_first_return_elsewhere, 0);
            }
            sc_unlock();
            return 0;
        }
        restart = true;
    }
    resumed = s->state == SC_HELD_RETURN;
    sc = s->sc;
    len = s->len;
    key = s->pc;
    {
        uint64_t seq = s->seq;

        sc_forget(s);
        sc_delete(s);
        qatomic_dec(&syscall_trap_pending);
        /* the calls the task made after this one did not return */
        sc_task_calls(k1, sc_newer, seq);
    }
    sc_unlock();
    if (!ops || !ops->ret) {
        sc_drop(&sc);
        return 0;
    }

    int64_t v = sc_is32[sc.abi] ? (int32_t)*ret : (int64_t)*ret;
    sc.ret = v;
    sc.percpu = percpu;
    if (!resumed) {
        sc.restart = restart;
    }
    restart = sc.restart;
    sc.resumed = resumed;
    sc.keep = false;
    cpu->syscall_kmmu = kmmu;
    ops->ret(cpu, &sc, sc_opaque);
    cpu->syscall_kmmu = 0;
    if (!replay_analysis_write_allowed()) {
        sc.fault = sc.inject = false;
        sc.ret = v;
    }
    if (sc.inject) {
        return sc_ret_after(cpu, k1, k2, key, len, &sc, sc.num, sc.args,
                            false, ret, rw);
    }
    if (sc.fault) {
        return sc_hold(k1, k2, key, len, &sc, SC_HELD_RETURN);
    }
    if (sc.keep && !restart) {
        sc.ret = 0;
        sc_add(k1, k2, key, len, &sc, SC_PENDING);
        return 0;
    }
    sc_drop(&sc);
    if (!restart && sc.ret != v) {
        *ret = sc_is32[sc.abi] ? (uint32_t)sc.ret : (uint64_t)sc.ret;
        return SYSCALL_TRAP_RET;
    }
    return 0;
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

        if (s->pc && s->sc.task == task) {
            sc_forget(s);
            sc_drop_later(&s->sc);
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
            sc_drop_later(&sc_slots[i].sc);
            n++;
        }
    }
    memset(sc_slots, 0, sizeof(sc_slots));
    qatomic_set(&syscall_trap_pending, 0);
    qatomic_set(&sc_held, 0);
    qatomic_inc(&syscall_trap_epoch);   /* the guest kernel may be another */
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

bool syscall_trap_set_consumer(const SyscallTrapOps *ops, void *opaque)
{
    if (sc_detaching || (sc_ops && (sc_ops != ops || sc_opaque != opaque))) {
        return false;
    }
    sc_opaque = opaque;
    qatomic_set(&sc_ops, ops);
    return true;
}

bool syscall_trap_clear_consumer(const SyscallTrapOps *ops, void *opaque)
{
    /* Caller owns main-thread/BQL quiescence; never detach another consumer.
     * Unlike rewind cancellation, live detach cannot abandon injected register
     * restoration: the task must finish its injected call under the old owner. */
    if (sc_detaching || !ops || sc_ops != ops || sc_opaque != opaque) {
        return false;
    }
    qemu_mutex_lock(&sc_lock);
    for (unsigned i = 0; i < SC_SLOTS; i++) {
        if (sc_slots[i].pc && (sc_slots[i].state == SC_INJECT ||
                              sc_slots[i].state == SC_INJECT_RET)) {
            qemu_mutex_unlock(&sc_lock);
            return false; /* no watch, owner, counter or slot changes */
        }
    }
    sc_detaching = true;
    qemu_mutex_unlock(&sc_lock);
    syscall_trap_unwatch_all();
    qatomic_set(&syscall_trap_on, false);
    syscall_trap_cancel_all(); /* old drop callback remains mapped until drained */
    sc_opaque = NULL;
    qatomic_set(&sc_ops, NULL);
    sc_detaching = false;
    return true;
}

void syscall_trap_watch(int abi, int64_t num, unsigned flags)
{
    if (sc_detaching || abi < 0 || abi >= SYSCALL_ABI_COUNT || num < -1 ||
        num >= SYSCALL_TRAP_NUMS) {
        return;
    }
    flags &= SYSCALL_TRAP_ENTER | SYSCALL_TRAP_RETURN;
    if (num < 0) {
        sc_watch_all[abi] = flags;
    } else {
        sc_watch[abi][num] = flags;
    }
    if (flags) {
        qatomic_set(&syscall_trap_on, true);
    }
}

void syscall_trap_unwatch_all(void)
{
    memset(sc_watch, 0, sizeof(sc_watch));
    memset(sc_watch_all, 0, sizeof(sc_watch_all));
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
