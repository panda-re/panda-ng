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
#ifdef CONFIG_TCG
#include "accel/tcg/probe.h"
#include "accel/tcg/cpu-mmu-index.h"
#include "exec/tlb-flags.h"
#endif

#define SC_SLOTS 4096           /* pending calls; a power of two */

bool syscall_trap_on;
uint32_t syscall_trap_pending;

static const SyscallTrapOps *sc_ops;
static uint32_t sc_held;        /* calls held for a fault */
static uint64_t sc_faults;
static __thread uint64_t sc_fault_addr;
static __thread bool sc_fault_write;
static void *sc_opaque;

static uint8_t sc_watch[SYSCALL_ABI_COUNT][SYSCALL_TRAP_NUMS];
static uint8_t sc_watch_all[SYSCALL_ABI_COUNT];

static const bool sc_is32[SYSCALL_ABI_COUNT] = {
    [SYSCALL_ABI_I386] = true,
    [SYSCALL_ABI_ARM] = true,
    [SYSCALL_ABI_ARM_OABI] = true,
    [SYSCALL_ABI_PPC] = true,
    [SYSCALL_ABI_RISCV32] = true,
};

/* SC_INJECT: held while the task runs a call in its place (inject) */
enum { SC_PENDING, SC_HELD_ENTER, SC_HELD_RETURN, SC_INJECT };

typedef struct {
    uint64_t k1, k2, pc;        /* pc 0: empty */
    uint8_t len;
    uint8_t state;              /* waiting for its return, or held */
    uint64_t seq;               /* when it was added */
    SyscallTrap sc;
    int64_t num;                /* SC_INJECT: the registers it replaced */
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
                sc_slots[i].state == SC_INJECT) {
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
    } else if (s && s->pc && s->state == SC_INJECT) {
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
        sc_add(k1, k2, pc, len, &sc, SC_PENDING);
    } else {
        sc_drop(&sc);
    }
    return r;
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
                t->state != SC_HELD_ENTER && t->state != SC_INJECT) {
                s = t;
            }
        }
        if (!s) {
            sc_unlock();
            return 0;
        }
        restart = true;
    }
    resumed = s->state == SC_HELD_RETURN;
    sc = s->sc;
    len = s->len;
    key = s->pc;
    sc_forget(s);
    sc_delete(s);
    qatomic_dec(&syscall_trap_pending);
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

bool syscall_trap_set_consumer(const SyscallTrapOps *ops, void *opaque)
{
    if (sc_ops && sc_ops != ops) {
        return false;
    }
    sc_opaque = opaque;
    qatomic_set(&sc_ops, ops);
    return true;
}

void syscall_trap_watch(int abi, int64_t num, unsigned flags)
{
    if (abi < 0 || abi >= SYSCALL_ABI_COUNT || num < -1 ||
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

bool syscall_trap_access(CPUState *cpu, vaddr addr, void *buf, size_t len,
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
