/*
 * Page-in requests (include/panda/pagein.h) for this target.
 *
 * A request waits in a queue until a vCPU reaches a user-mode instruction boundary of the
 * request's address space (the CPU loop calls panda_pagein_service() there while anything is
 * queued: PANDA_LOOP_PAGEIN). If the range is already accessible it completes at once. Otherwise
 * the thread's registers are saved, the thread is made to enter the kernel as if it had executed
 * a system call instruction just before its current PC, with madvise(MADV_POPULATE_READ/WRITE)
 * in the argument registers, and the kernel returns to that PC. The first user-mode boundary of
 * the same address space with the same stack pointer is that return (or a restart of it): the
 * registers are put back, the range is accessed without faulting, and the request completes.
 *
 * Only system calls the guest could have made itself; never a fault the guest would not take.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/timer.h"
#include "qemu/rcu.h"
#include "hw/core/cpu.h"
#include "exec/cpu-common.h"
#include "accel/tcg/cpu-ops.h"
#include "accel/tcg/cpu-mmu-index.h"
#include "accel/tcg/cpu-loop.h"
#include "system/tcg.h"
#include "system/replay-analysis.h"
#include "panda/common.h"
#include "panda/hooks.h"
#include "panda/pagein.h"
#include "plugins/qemu-plugin.h"
#if defined(TARGET_I386)
#include "target/i386/tcg/helper-tcg.h"
#endif
#if defined(TARGET_ARM)
#include "target/arm/internals.h"
#include "target/arm/syndrome.h"
#endif

#define MADV_POPULATE_READ 22
#define MADV_POPULATE_WRITE 23
#define PAGEIN_TIMEOUT_NS (5 * NANOSECONDS_PER_SECOND)
#define PAGEIN_MAX_ROUNDS 4         /* injections per request (reclaimed again in between) */

typedef struct PageinReq {
    QTAILQ_ENTRY(PageinReq) next;
    uint64_t id, asid, addr;
    size_t len;
    uint8_t *buf;               /* read: filled at completion; write: the bytes */
    bool write, inflight;
    int rounds;
    int64_t deadline;
    panda_pagein_cb_t cb;
    void *opaque;
} PageinReq;

/* A thread that is in the kernel on our behalf: its registers, to put back */
typedef struct PageinRegs {
    QTAILQ_ENTRY(PageinRegs) next;
    uint64_t asid, sp, pc;
    unsigned len;               /* the system call instruction's length, for a restart */
    PageinReq *req;             /* NULL once the request is gone (timeout, cancel) */
#if defined(TARGET_I386)
    target_ulong regs[CPU_NB_REGS];
    target_ulong eflags;
#elif defined(TARGET_AARCH64)
    uint64_t xregs[31];
#elif defined(TARGET_RISCV)
    target_ulong gpr[32];
#endif
} PageinRegs;

static QemuMutex lock;
static bool inited;
static QTAILQ_HEAD(, PageinReq) reqs = QTAILQ_HEAD_INITIALIZER(reqs);
static QTAILQ_HEAD(, PageinRegs) held = QTAILQ_HEAD_INITIALIZER(held);
static unsigned nreqs;
static uint64_t next_id = 1;
static uint64_t st_submitted, st_injected, st_completed, st_status[11];

/* Completions to run after the lock is dropped */
typedef struct Done {
    PageinReq *req;
    int status, guest_errno;
} Done;

static void pagein_init(void)
{
    if (!qatomic_read(&inited)) {
        static QemuMutex init_lock;
        static gsize once;
        if (g_once_init_enter(&once)) {
            qemu_mutex_init(&init_lock);
            qemu_mutex_init(&lock);
            qatomic_set(&inited, true);
            g_once_init_leave(&once, 1);
        }
    }
}

/* lock held: what the CPU loop has to look at */
static void pagein_update_loop_bit(void)
{
    if (QTAILQ_EMPTY(&reqs) && QTAILQ_EMPTY(&held)) {
        qatomic_and(&panda_loop_hooks, ~PANDA_LOOP_PAGEIN);
    } else {
        qatomic_or(&panda_loop_hooks, PANDA_LOOP_PAGEIN);
    }
}

/* lock held */
static void pagein_finish(PageinReq *r, int status, int guest_errno, Done *d)
{
    QTAILQ_REMOVE(&reqs, r, next);
    nreqs--;
    PageinRegs *h;
    QTAILQ_FOREACH(h, &held, next) {
        if (h->req == r) {
            h->req = NULL;      /* the registers still go back */
        }
    }
    st_completed++;
    if (status <= 0 && status >= -10) {
        st_status[-status]++;
    }
    d->req = r;
    d->status = status;
    d->guest_errno = guest_errno;
}

static void pagein_complete(Done *d, int n)
{
    for (int i = 0; i < n; i++) {
        PageinReq *r = d[i].req;
        bool data = d[i].status == PANDA_PAGEIN_OK;
        r->cb(r->id, d[i].status, d[i].guest_errno, data ? r->buf : NULL, data ? r->len : 0,
              r->opaque);
        g_free(r->buf);
        g_free(r);
    }
}

bool panda_pagein_can_wait(void)
{
    return current_cpu == NULL && !bql_locked();
}

const char *panda_pagein_strerror(int status)
{
    switch (status) {
    case PANDA_PAGEIN_OK: return "ok";
    case PANDA_PAGEIN_QUEUED: return "queued";
    case PANDA_PAGEIN_EGUEST: return "the guest kernel refused the range";
    case PANDA_PAGEIN_ETIMEDOUT: return "the process did not run in user mode in time";
    case PANDA_PAGEIN_ECANCELED: return "cancelled by a snapshot load or reset";
    case PANDA_PAGEIN_ERECORD: return "refused: record/replay is active";
    case PANDA_PAGEIN_EACCEL: return "refused: not supported under this accelerator";
    case PANDA_PAGEIN_EARCH: return "refused: no system-call injector for this target or task";
    case PANDA_PAGEIN_EINVAL: return "refused: bad range or no completion";
    case PANDA_PAGEIN_EBUSY: return "refused: too many requests outstanding";
    case PANDA_PAGEIN_ERECLAIMED: return "the guest reclaimed the range again before it was read";
    case PANDA_PAGEIN_EIO: return "the range is not RAM";
    }
    return "unknown";
}

void panda_pagein_stats(uint64_t *submitted, uint64_t *injected, uint64_t *completed,
                        uint64_t by_status[11])
{
    pagein_init();
    qemu_mutex_lock(&lock);
    *submitted = st_submitted;
    *injected = st_injected;
    *completed = st_completed;
    memcpy(by_status, st_status, sizeof(st_status));
    qemu_mutex_unlock(&lock);
}

/*
 * Target pieces: is this vCPU at a user-mode boundary of a task we can inject into, its stack
 * pointer, the system call's return register, and the injection.
 */
#if defined(TARGET_X86_64)
#define PAGEIN_NR_MADVISE 28
static int pagein_user(CPUState *cs, uint64_t *sp)
{
    CPUX86State *env = cpu_env(cs);
    if ((env->hflags & HF_CPL_MASK) != 3) {
        return 0;
    }
    *sp = env->regs[R_ESP];
    /* a 32-bit task: madvise is another number, entered another way */
    return (env->hflags & HF_CS64_MASK) ? 1 : -1;
}
static void pagein_save(CPUState *cs, PageinRegs *h)
{
    CPUX86State *env = cpu_env(cs);
    memcpy(h->regs, env->regs, sizeof(h->regs));
    h->eflags = cpu_compute_eflags(env);
    h->pc = env->eip;
    h->len = 2;
}
static int64_t pagein_retval(CPUState *cs)
{
    return (int64_t)cpu_env(cs)->regs[R_EAX];
}
static void pagein_restore(CPUState *cs, PageinRegs *h)
{
    CPUX86State *env = cpu_env(cs);
    memcpy(env->regs, h->regs, sizeof(h->regs));
    cpu_load_eflags(env, h->eflags, TF_MASK | AC_MASK | ID_MASK | NT_MASK | RF_MASK);
    if (env->eip == h->pc || env->eip == h->pc - h->len) {
        env->eip = h->pc;
    }
}
/* What SYSCALL does in long mode (helper_syscall), returning to the current RIP */
static void pagein_inject(CPUState *cs, uint64_t a0, uint64_t a1, uint64_t a2)
{
    CPUX86State *env = cpu_env(cs);
    int selector = (env->star >> 32) & 0xffff;

    env->regs[R_EAX] = PAGEIN_NR_MADVISE;
    env->regs[R_EDI] = a0;
    env->regs[R_ESI] = a1;
    env->regs[R_EDX] = a2;
    env->regs[R_ECX] = env->eip;
    env->regs[11] = cpu_compute_eflags(env) & ~RF_MASK;
    env->eflags &= ~(env->fmask | RF_MASK);
    cpu_load_eflags(env, env->eflags, 0);
    cpu_x86_load_seg_cache(env, R_CS, selector & 0xfffc, 0, 0xffffffff,
                           DESC_G_MASK | DESC_P_MASK | DESC_S_MASK | DESC_CS_MASK |
                           DESC_R_MASK | DESC_A_MASK | DESC_L_MASK);
    cpu_x86_load_seg_cache(env, R_SS, (selector + 8) & 0xfffc, 0, 0xffffffff,
                           DESC_G_MASK | DESC_B_MASK | DESC_P_MASK | DESC_S_MASK |
                           DESC_W_MASK | DESC_A_MASK);
    env->eip = env->lstar;
    cpu_loop_exit_noexc(cs);
}
#elif defined(TARGET_AARCH64)
#define PAGEIN_NR_MADVISE 233
static int pagein_user(CPUState *cs, uint64_t *sp)
{
    CPUARMState *env = cpu_env(cs);
    if (arm_current_el(env) != 0) {
        return 0;
    }
    if (!is_a64(env)) {
        return -1;
    }
    *sp = env->xregs[31];
    return 1;
}
static void pagein_save(CPUState *cs, PageinRegs *h)
{
    CPUARMState *env = cpu_env(cs);
    memcpy(h->xregs, env->xregs, sizeof(h->xregs));
    h->pc = env->pc;
    h->len = 4;
}
static int64_t pagein_retval(CPUState *cs)
{
    return (int64_t)cpu_env(cs)->xregs[0];
}
static void pagein_restore(CPUState *cs, PageinRegs *h)
{
    CPUARMState *env = cpu_env(cs);
    memcpy(env->xregs, h->xregs, sizeof(h->xregs));
    if (env->pc == h->pc || env->pc == h->pc - h->len) {
        env->pc = h->pc;
    }
}
/* An SVC whose return address (ELR) is the current PC */
static void pagein_inject(CPUState *cs, uint64_t a0, uint64_t a1, uint64_t a2)
{
    CPUARMState *env = cpu_env(cs);

    env->xregs[8] = PAGEIN_NR_MADVISE;
    env->xregs[0] = a0;
    env->xregs[1] = a1;
    env->xregs[2] = a2;
    env->exception.syndrome = syn_aa64_svc(0);
    env->exception.target_el = 1;
    panda_pagein_injecting = true;      /* not the syscall trap's: the program didn't make it */
    cs->exception_index = EXCP_SWI;
    cpu_loop_exit(cs);
}
#elif defined(TARGET_RISCV64)
#define PAGEIN_NR_MADVISE 233
static int pagein_user(CPUState *cs, uint64_t *sp)
{
    CPURISCVState *env = cpu_env(cs);
    if (env->priv != PRV_U || env->virt_enabled) {
        return 0;
    }
    if (env->xl != MXL_RV64) {
        return -1;
    }
    *sp = env->gpr[2];
    return 1;
}
static void pagein_save(CPUState *cs, PageinRegs *h)
{
    CPURISCVState *env = cpu_env(cs);
    memcpy(h->gpr, env->gpr, sizeof(h->gpr));
    h->pc = env->pc;
    h->len = 4;
}
static int64_t pagein_retval(CPUState *cs)
{
    return (int64_t)cpu_env(cs)->gpr[10];
}
static void pagein_restore(CPUState *cs, PageinRegs *h)
{
    CPURISCVState *env = cpu_env(cs);
    memcpy(env->gpr, h->gpr, sizeof(h->gpr));
    if (env->pc == h->pc || env->pc == h->pc - h->len) {
        env->pc = h->pc;
    }
}
/* An ECALL 4 bytes before the current PC: the kernel returns past it, to the PC */
static void pagein_inject(CPUState *cs, uint64_t a0, uint64_t a1, uint64_t a2)
{
    CPURISCVState *env = cpu_env(cs);

    env->gpr[17] = PAGEIN_NR_MADVISE;
    env->gpr[10] = a0;
    env->gpr[11] = a1;
    env->gpr[12] = a2;
    env->pc -= 4;
    cs->exception_index = RISCV_EXCP_U_ECALL;
    cpu_loop_exit(cs);
}
#else
#define PAGEIN_NO_INJECTOR 1
#endif

__thread bool panda_pagein_injecting;

int panda_pagein_submit(uint64_t asid, uint64_t addr, size_t len, const uint8_t *wdata,
                        uint64_t timeout_ns, panda_pagein_cb_t cb, void *opaque, uint64_t *id)
{
    pagein_init();
    if (!replay_analysis_write_allowed()) {
        return PANDA_PAGEIN_ERECORD;     /* the system call would not be in the recording */
    }
    if (!tcg_enabled()) {
        return PANDA_PAGEIN_EACCEL;
    }
#ifdef PAGEIN_NO_INJECTOR
    return PANDA_PAGEIN_EARCH;
#else
    if (!cb || len == 0 || len > PANDA_PAGEIN_MAX_LEN || addr + len < addr) {
        return PANDA_PAGEIN_EINVAL;
    }
    PageinReq *r = g_new0(PageinReq, 1);
    r->asid = asid;
    r->addr = addr;
    r->len = len;
    r->write = wdata != NULL;
    r->buf = wdata ? g_memdup2(wdata, len) : g_malloc0(len);
    r->cb = cb;
    r->opaque = opaque;
    r->deadline = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                  (timeout_ns ? timeout_ns : PAGEIN_TIMEOUT_NS);
    qemu_mutex_lock(&lock);
    if (nreqs >= PANDA_PAGEIN_MAX_OUTSTANDING) {
        qemu_mutex_unlock(&lock);
        g_free(r->buf);
        g_free(r);
        return PANDA_PAGEIN_EBUSY;
    }
    r->id = next_id++;
    if (id) {
        *id = r->id;
    }
    QTAILQ_INSERT_TAIL(&reqs, r, next);
    nreqs++;
    st_submitted++;
    pagein_update_loop_bit();
    qemu_mutex_unlock(&lock);

    /* Every vCPU back to its loop, where the request is looked at */
    CPUState *cs;
    WITH_RCU_READ_LOCK_GUARD() {
        CPU_FOREACH(cs) {
            cpu_exit(cs);
        }
    }
    return PANDA_PAGEIN_QUEUED;
#endif
}

#ifndef PAGEIN_NO_INJECTOR
/* At a user-mode boundary of r's address space: copy without faulting */
static int pagein_copy(PageinReq *r)
{
    GByteArray *b = g_byte_array_new();
    int ret;

    if (r->write) {
        g_byte_array_append(b, r->buf, r->len);
        ret = qemu_plugin_write_memory_vaddr_fault(r->addr, b, QEMU_PLUGIN_VADDR_USER, false);
    } else {
        ret = qemu_plugin_read_memory_vaddr_fault(r->addr, b, r->len, QEMU_PLUGIN_VADDR_USER,
                                                  false);
        if (ret == QEMU_PLUGIN_VADDR_OK) {
            memcpy(r->buf, b->data, r->len);
        }
    }
    g_byte_array_unref(b);
    return ret;
}
#endif

void panda_pagein_service(CPUState *cs)
{
#ifndef PAGEIN_NO_INJECTOR
    Done done[8];
    int ndone = 0;
    uint64_t sp = 0, asid;
    int user;
    int64_t now;
    PageinReq *r, *rn;
    PageinRegs *h;

    pagein_init();
    user = pagein_user(cs, &sp);
    now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    qemu_mutex_lock(&lock);

    /*
     * A recording or replay started after these were queued: serving them now would put a system
     * call in the recording that its replay never makes. Refuse them; a thread that is in the
     * kernel on our behalf keeps the registers it has (they can't be put back unlogged).
     */
    if (!replay_analysis_write_allowed()) {
        while ((h = QTAILQ_FIRST(&held))) {
            QTAILQ_REMOVE(&held, h, next);
            g_free(h);
        }
        QTAILQ_FOREACH_SAFE(r, &reqs, next, rn) {
            if (ndone < ARRAY_SIZE(done)) {
                pagein_finish(r, PANDA_PAGEIN_ERECORD, 0, &done[ndone++]);
            }
        }
        goto out;
    }

    /* Requests whose process never came back in time; their held registers stay */
    QTAILQ_FOREACH_SAFE(r, &reqs, next, rn) {
        if (ndone < ARRAY_SIZE(done) && now > r->deadline) {
            pagein_finish(r, PANDA_PAGEIN_ETIMEDOUT, 0, &done[ndone++]);
        }
    }
    if (user == 0) {
        goto out;
    }
    asid = panda_current_asid(cs);

    /* A thread back from our system call: its registers go back first */
    QTAILQ_FOREACH(h, &held, next) {
        if (h->asid == asid && h->sp == sp) {
            break;
        }
    }
    if (h) {
        int64_t ret = pagein_retval(cs);
        QTAILQ_REMOVE(&held, h, next);
        pagein_restore(cs, h);
        r = h->req;
        g_free(h);
        if (r && ndone < ARRAY_SIZE(done)) {
            r->inflight = false;
            if (ret < 0 && ret > -4096) {
                pagein_finish(r, PANDA_PAGEIN_EGUEST, (int)-ret, &done[ndone++]);
            } else {
                int c = pagein_copy(r);
                if (c == QEMU_PLUGIN_VADDR_OK) {
                    pagein_finish(r, PANDA_PAGEIN_OK, 0, &done[ndone++]);
                } else if (c == QEMU_PLUGIN_VADDR_IO) {
                    pagein_finish(r, PANDA_PAGEIN_EIO, 0, &done[ndone++]);
                } else if (r->rounds >= PAGEIN_MAX_ROUNDS) {
                    pagein_finish(r, PANDA_PAGEIN_ERECLAIMED, 0, &done[ndone++]);
                }
                /* else it waits for another round */
            }
        }
        pagein_update_loop_bit();
        qemu_mutex_unlock(&lock);
        pagein_complete(done, ndone);
        cpu_loop_exit_noexc(cs);    /* the registers changed: look the block up again */
    }

    /* A request for this address space */
    QTAILQ_FOREACH(r, &reqs, next) {
        if (!r->inflight && r->asid == asid) {
            break;
        }
    }
    if (!r) {
        goto out;
    }
    if (user < 0) {
        if (ndone < ARRAY_SIZE(done)) {
            pagein_finish(r, PANDA_PAGEIN_EARCH, 0, &done[ndone++]);
        }
        goto out;
    }
    if (r->rounds == 0) {
        int c = pagein_copy(r);         /* already there: no system call */
        if (c == QEMU_PLUGIN_VADDR_OK || c == QEMU_PLUGIN_VADDR_IO) {
            if (ndone < ARRAY_SIZE(done)) {
                pagein_finish(r, c == QEMU_PLUGIN_VADDR_OK ? PANDA_PAGEIN_OK : PANDA_PAGEIN_EIO,
                              0, &done[ndone++]);
            }
            goto out;
        }
    }
    {
        uint64_t page = qemu_target_page_size();
        uint64_t start = r->addr & ~(page - 1);
        uint64_t end = ROUND_UP(r->addr + r->len, page);

        h = g_new0(PageinRegs, 1);
        h->asid = asid;
        h->sp = sp;
        h->req = r;
        pagein_save(cs, h);
        QTAILQ_INSERT_TAIL(&held, h, next);
        r->inflight = true;
        r->rounds++;
        st_injected++;
        pagein_update_loop_bit();
        qemu_mutex_unlock(&lock);
        pagein_complete(done, ndone);
        pagein_inject(cs, start, end - start,
                      r->write ? MADV_POPULATE_WRITE : MADV_POPULATE_READ);
        g_assert_not_reached();
    }
out:
    pagein_update_loop_bit();
    qemu_mutex_unlock(&lock);
    pagein_complete(done, ndone);
#endif
}

/* A snapshot load or reset: the state the held registers belong to is gone */
void panda_pagein_cancel_all(void)
{
    Done d;
    PageinReq *r;
    PageinRegs *h;

    if (!qatomic_read(&inited)) {
        return;
    }
    for (;;) {
        qemu_mutex_lock(&lock);
        while ((h = QTAILQ_FIRST(&held))) {
            QTAILQ_REMOVE(&held, h, next);
            g_free(h);
        }
        r = QTAILQ_FIRST(&reqs);
        if (r) {
            pagein_finish(r, PANDA_PAGEIN_ECANCELED, 0, &d);
        }
        pagein_update_loop_bit();
        qemu_mutex_unlock(&lock);
        if (!r) {
            return;
        }
        pagein_complete(&d, 1);
    }
}
