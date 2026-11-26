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

static const SyscallTrapOps *sc_ops;
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

typedef struct {
    uint64_t k1, k2, pc;        /* pc 0: empty */
    uint8_t len;
    uint64_t seq;               /* when it was added */
    SyscallTrap sc;
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
            if (sc_slots[i].seq + SC_SLOTS / 2 >= sc_seq) {
                keep[n++] = sc_slots[i];
            } else {
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

/* sc_lock held; the call is dropped if it cannot be kept */
static void sc_put(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                   SyscallTrap *sc)
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
            sc_drop_later(&s->sc);
        } else {
            qatomic_inc(&syscall_trap_pending);
        }
        s->k1 = k1;
        s->k2 = k2;
        s->pc = pc;
        s->len = len;
        s->seq = ++sc_seq;
        s->sc = *sc;
    }
}

static void sc_add(uint64_t k1, uint64_t k2, uint64_t pc, unsigned len,
                   SyscallTrap *sc)
{
    qemu_mutex_lock(&sc_lock);
    sc_put(k1, k2, pc, len, sc);
    sc_unlock();
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
    if ((w & SYSCALL_TRAP_ENTER) && ops->enter) {
        cpu->syscall_kmmu = kmmu;
        ops->enter(cpu, &sc, sc_opaque);
        cpu->syscall_kmmu = 0;
        if (!replay_analysis_write_allowed()) {
            /* Deliver observations, but never skip or rewrite the real call. */
            sc.skip = false;
            memcpy(sc.args, args, sizeof(sc.args));
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
        sc_add(k1, k2, pc, len, &sc);
    } else {
        sc_drop(&sc);
    }
    return r;
}

int syscall_trap_return(CPUState *cpu, uint64_t pc, uint64_t k1, uint64_t k2,
                        uint64_t percpu, uint32_t kmmu, uint64_t *ret)
{
    const SyscallTrapOps *ops = qatomic_read(&sc_ops);
    SyscallTrap sc;
    SCSlot *s;
    uint64_t key;
    unsigned len = 0;
    bool restart = false;

    qemu_mutex_lock(&sc_lock);
    s = sc_find(k1, k2, pc);
    if (!s || !s->pc) {
        /* the kernel rewound the task to the trap: a restart */
        static const unsigned lens[] = { 2, 4 };
        s = NULL;
        for (int i = 0; i < ARRAY_SIZE(lens) && !s; i++) {
            SCSlot *t = sc_find(k1, k2, pc + lens[i]);
            if (t && t->pc && t->len == lens[i]) {
                s = t;
            }
        }
        if (!s) {
            sc_unlock();
            return 0;
        }
        restart = true;
    }
    sc = s->sc;
    len = s->len;
    key = s->pc;
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
    sc.restart = restart;
    sc.keep = false;
    cpu->syscall_kmmu = kmmu;
    ops->ret(cpu, &sc, sc_opaque);
    cpu->syscall_kmmu = 0;
    if (!replay_analysis_write_allowed()) {
        sc.ret = v;
    }
    if (sc.keep && !restart) {
        sc.ret = 0;
        sc_add(k1, k2, key, len, &sc);
        return 0;
    }
    sc_drop(&sc);
    if (!restart && sc.ret != v) {
        *ret = sc_is32[sc.abi] ? (uint32_t)sc.ret : (uint64_t)sc.ret;
        return SYSCALL_TRAP_RET;
    }
    return 0;
}

unsigned syscall_trap_cancel_all(void)
{
    CPUState *cpu;
    unsigned n = 0;

    /* Callers have stopped the vCPUs before replacing guest execution. Watch
     * policy stays. */
    qemu_mutex_lock(&sc_lock);
    for (unsigned i = 0; i < SC_SLOTS; i++) {
        if (sc_slots[i].pc) {
            sc_drop_later(&sc_slots[i].sc);
            n++;
        }
    }
    memset(sc_slots, 0, sizeof(sc_slots));
    qatomic_set(&syscall_trap_pending, 0);
    CPU_FOREACH(cpu) {
        cpu->syscall_kmmu = 0;
    }
    sc_unlock();  /* A consumer's drop chain runs outside the lock. */
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
    /* Caller owns main-thread/BQL quiescence; never detach another consumer. */
    if (sc_detaching || !ops || sc_ops != ops || sc_opaque != opaque) {
        return false;
    }
    sc_detaching = true;
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
