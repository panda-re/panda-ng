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
#include "accel/tcg/probe.h"
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
#if defined(TARGET_MIPS)
#include "target/mips/internal.h"
#endif
#if defined(TARGET_PPC)
#include "target/ppc/internal.h"
#endif

#define MADV_POPULATE_READ 22
#define MADV_POPULATE_WRITE 23
#define PAGEIN_TIMEOUT_NS (5 * NANOSECONDS_PER_SECOND)
#define PAGEIN_MAX_ROUNDS 6         /* injections per request (page size probes, reclaim) */

/*
 * The guest kernel's page size, as a shift, once known: madvise wants a start aligned to it,
 * and it may be larger than the target's (16 or 64 KiB on arm64, ppc64, mips, loongarch). Until
 * a call proves it, a request answered EINVAL is tried again with the start aligned to 16, then
 * 64 KiB; the first alignment that works, from a start not aligned to the next one, is it.
 */
static int guest_page_shift;

typedef struct PageinReq {
    QTAILQ_ENTRY(PageinReq) next;
    uint64_t id, asid, addr;
    size_t len;
    uint8_t *buf;               /* read: filled at completion; write: the bytes */
    bool write, inflight;
    size_t done_len;            /* copied so far, page by page (after the call) */
    bool populated;             /* the last call succeeded: what's missing is a TLB miss away */
    int rounds, touches;
    int shift, first_errno;     /* the start's alignment for the next call; its first answer */
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
    /* the thread's registers: general purpose ones, and what the target keeps besides */
    uint64_t r[32];
    uint64_t x[12];
} PageinRegs;

static QemuMutex lock;
static bool inited;
static bool debug;              /* PANDA_PAGEIN_DEBUG: log each call and its answer */
static QTAILQ_HEAD(, PageinReq) reqs = QTAILQ_HEAD_INITIALIZER(reqs);
static QTAILQ_HEAD(, PageinRegs) held = QTAILQ_HEAD_INITIALIZER(held);
static unsigned nreqs;
static uint64_t next_id G_GNUC_UNUSED = 1;    /* unused without an injector */
static uint64_t st_submitted, st_injected, st_completed, st_abandoned, st_status[11];

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
            debug = getenv("PANDA_PAGEIN_DEBUG") != NULL;
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
                        uint64_t *abandoned, uint64_t by_status[11])
{
    pagein_init();
    qemu_mutex_lock(&lock);
    *abandoned = st_abandoned;
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
/*
 * pagein_user(): 1 if the vCPU is at a user-mode boundary we can inject at (*sp set), 0 if not
 * (kernel mode, or a moment we can't use: a branch delay slot), -1 if it is a user task we can't
 * serve (another ABI). pagein_retval(): the call's result, -errno on failure.
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
    for (int i = 0; i < CPU_NB_REGS; i++) {
        h->r[i] = env->regs[i];
    }
    h->x[0] = cpu_compute_eflags(env);
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
    for (int i = 0; i < CPU_NB_REGS; i++) {
        env->regs[i] = h->r[i];
    }
    cpu_load_eflags(env, h->x[0], TF_MASK | AC_MASK | ID_MASK | NT_MASK | RF_MASK);
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
#elif defined(TARGET_ARM)
/*
 * AArch64 tasks (aarch64 target): SVC with x8 = 233. AArch32 tasks (arm target, or a compat
 * task under an AArch64 kernel): EABI SVC 0 with r7 = 220. Either way the exception's return
 * address is the current PC (QEMU's SVC has already stepped past the instruction). An AArch32
 * task in ARM state on an AArch32 kernel is served only right after an SVC #0 (pagein_user).
 */
static int pagein_user(CPUState *cs, uint64_t *sp)
{
    CPUARMState *env = cpu_env(cs);
    if (arm_current_el(env) != 0) {
        return 0;
    }
    if (!is_a64(env) && !env->thumb && !arm_el_is_aa64(env, 1)) {
        /*
         * An AArch32 kernel with OABI compatibility reads the SVC instruction before the return
         * address and takes a non-zero immediate as an old-ABI call number: inject only where
         * that word is an SVC #0 (right after one of the thread's own system calls). Thumb
         * entries and AArch64 kernels take the number from r7 alone.
         */
        GByteArray *b = g_byte_array_new();
        bool svc0 = qemu_plugin_read_memory_vaddr_fault(env->regs[15] - 4, b, 4,
                                                        QEMU_PLUGIN_VADDR_USER, false) ==
                    QEMU_PLUGIN_VADDR_OK &&
                    (ldl_le_p(b->data) & 0x0fffffff) == 0x0f000000;
        g_byte_array_unref(b);
        if (!svc0) {
            return 0;
        }
    }
    *sp = is_a64(env) ? env->xregs[31] : env->regs[13];
    return 1;
}
static void pagein_save(CPUState *cs, PageinRegs *h)
{
    CPUARMState *env = cpu_env(cs);
    h->x[0] = is_a64(env);
    if (h->x[0]) {
        for (int i = 0; i < 31; i++) {
            h->r[i] = env->xregs[i];
        }
        h->pc = env->pc;
        h->len = 4;
    } else {
        for (int i = 0; i < 15; i++) {
            h->r[i] = env->regs[i];
        }
        h->pc = env->regs[15];
        h->len = env->thumb ? 2 : 4;
    }
}
static int64_t pagein_retval(CPUState *cs)
{
    CPUARMState *env = cpu_env(cs);
    return is_a64(env) ? (int64_t)env->xregs[0] : (int64_t)(int32_t)env->regs[0];
}
static void pagein_restore(CPUState *cs, PageinRegs *h)
{
    CPUARMState *env = cpu_env(cs);
    if (h->x[0]) {
        for (int i = 0; i < 31; i++) {
            env->xregs[i] = h->r[i];
        }
        if (env->pc == h->pc || env->pc == h->pc - h->len) {
            env->pc = h->pc;
        }
    } else {
        for (int i = 0; i < 15; i++) {
            env->regs[i] = h->r[i];
        }
        if (env->regs[15] == h->pc || env->regs[15] == h->pc - h->len) {
            env->regs[15] = h->pc;
        }
    }
}
static void pagein_inject(CPUState *cs, uint64_t a0, uint64_t a1, uint64_t a2)
{
    CPUARMState *env = cpu_env(cs);

    if (is_a64(env)) {
        env->xregs[8] = 233;
        env->xregs[0] = a0;
        env->xregs[1] = a1;
        env->xregs[2] = a2;
        env->exception.syndrome = syn_aa64_svc(0);
    } else {
        env->regs[7] = 220;
        env->regs[0] = a0;
        env->regs[1] = a1;
        env->regs[2] = a2;
        env->exception.syndrome = syn_aa32_svc(0, env->thumb);
    }
    env->exception.target_el = 1;
    panda_pagein_injecting = true;      /* not the syscall trap's: the program didn't make it */
    cs->exception_index = EXCP_SWI;
    cpu_loop_exit(cs);
}
#elif defined(TARGET_RISCV64)
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
    for (int i = 0; i < 32; i++) {
        h->r[i] = env->gpr[i];
    }
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
    for (int i = 1; i < 32; i++) {
        env->gpr[i] = h->r[i];
    }
    if (env->pc == h->pc || env->pc == h->pc - h->len) {
        env->pc = h->pc;
    }
}
/* An ECALL 4 bytes before the current PC: the kernel returns past it, to the PC */
static void pagein_inject(CPUState *cs, uint64_t a0, uint64_t a1, uint64_t a2)
{
    CPURISCVState *env = cpu_env(cs);

    env->gpr[17] = 233;
    env->gpr[10] = a0;
    env->gpr[11] = a1;
    env->gpr[12] = a2;
    env->pc -= 4;
    cs->exception_index = RISCV_EXCP_U_ECALL;
    cpu_loop_exit(cs);
}
#elif defined(TARGET_MIPS)
/*
 * A SYSCALL 4 bytes before the current PC (the kernel steps EPC past it): o32 madvise (4218) on
 * 32-bit targets, n64 madvise (5027) on 64-bit ones, where the kernel takes the ABI from the
 * number's range, so o32 and n32 tasks are served too. Not in a delay slot, not MIPS16 or
 * microMIPS, and not at the start of a page (the CPU may read the instruction at EPC into
 * BadInstr).
 */
static int pagein_user(CPUState *cs, uint64_t *sp)
{
    CPUMIPSState *env = cpu_env(cs);
    if ((env->hflags & MIPS_HFLAG_MODE) != MIPS_HFLAG_UM) {
        return 0;
    }
    if (env->hflags & MIPS_HFLAG_M16) {
        return -1;
    }
    if ((env->hflags & MIPS_HFLAG_BMASK) ||
        (env->active_tc.PC & (qemu_target_page_size() - 1)) < 4) {
        return 0;
    }
    *sp = env->active_tc.gpr[29];
    return 1;
}
static void pagein_save(CPUState *cs, PageinRegs *h)
{
    CPUMIPSState *env = cpu_env(cs);
    for (int i = 0; i < 32; i++) {
        h->r[i] = env->active_tc.gpr[i];
    }
    h->x[0] = env->active_tc.HI[0];
    h->x[1] = env->active_tc.LO[0];
    h->pc = env->active_tc.PC;
    h->len = 4;
}
/* v0 is the errno when a3 is set */
static int64_t pagein_retval(CPUState *cs)
{
    CPUMIPSState *env = cpu_env(cs);
    int64_t v0 = (target_long)env->active_tc.gpr[2];
    return env->active_tc.gpr[7] ? -v0 : v0;
}
static void pagein_restore(CPUState *cs, PageinRegs *h)
{
    CPUMIPSState *env = cpu_env(cs);
    for (int i = 1; i < 32; i++) {
        env->active_tc.gpr[i] = h->r[i];
    }
    env->active_tc.HI[0] = h->x[0];
    env->active_tc.LO[0] = h->x[1];
    if (env->active_tc.PC == h->pc || env->active_tc.PC == h->pc - h->len) {
        env->active_tc.PC = h->pc;
    }
}
static void pagein_inject(CPUState *cs, uint64_t a0, uint64_t a1, uint64_t a2)
{
    CPUMIPSState *env = cpu_env(cs);

#if defined(TARGET_MIPS64)
    env->active_tc.gpr[2] = 5027;
#else
    env->active_tc.gpr[2] = 4218;
#endif
    env->active_tc.gpr[4] = a0;
    env->active_tc.gpr[5] = a1;
    env->active_tc.gpr[6] = a2;
    env->active_tc.PC -= 4;
    cs->exception_index = EXCP_SYSCALL;
    env->error_code = 0;
    cpu_loop_exit(cs);
}
#elif defined(TARGET_PPC)
/*
 * sc with r0 = 205, from 4 bytes before the current NIP (the exception entry steps SRR0 past
 * it). On ppc64, 64-bit tasks only (MSR.SF).
 */
static int pagein_user(CPUState *cs, uint64_t *sp)
{
    CPUPPCState *env = cpu_env(cs);
    if (!FIELD_EX64(env->msr, MSR, PR)) {
        return 0;
    }
#if defined(TARGET_PPC64)
    if (!FIELD_EX64(env->msr, MSR, SF)) {
        return -1;
    }
#endif
    *sp = env->gpr[1];
    return 1;
}
static void pagein_save(CPUState *cs, PageinRegs *h)
{
    CPUPPCState *env = cpu_env(cs);
    for (int i = 0; i < 32; i++) {
        h->r[i] = env->gpr[i];
    }
    for (int i = 0; i < 8; i++) {
        h->x[i] = env->crf[i];
    }
    /* sc may change the volatile special registers too (Linux zeroes some on the way out) */
    h->x[8] = env->lr;
    h->x[9] = env->ctr;
    h->x[10] = cpu_read_xer(env);
    h->pc = env->nip;
    h->len = 4;
}
/* r3 is the errno when CR0.SO is set */
static int64_t pagein_retval(CPUState *cs)
{
    CPUPPCState *env = cpu_env(cs);
    int64_t r3 = (target_long)env->gpr[3];
    return (env->crf[0] & 1) ? -r3 : r3;
}
static void pagein_restore(CPUState *cs, PageinRegs *h)
{
    CPUPPCState *env = cpu_env(cs);
    for (int i = 0; i < 32; i++) {
        env->gpr[i] = h->r[i];
    }
    for (int i = 0; i < 8; i++) {
        env->crf[i] = h->x[i];
    }
    env->lr = h->x[8];
    env->ctr = h->x[9];
    cpu_write_xer(env, h->x[10]);
    if (env->nip == h->pc || env->nip == h->pc - h->len) {
        env->nip = h->pc;
    }
}
static void pagein_inject(CPUState *cs, uint64_t a0, uint64_t a1, uint64_t a2)
{
    CPUPPCState *env = cpu_env(cs);

    env->gpr[0] = 205;
    env->gpr[3] = a0;
    env->gpr[4] = a1;
    env->gpr[5] = a2;
    env->nip -= 4;
    cs->exception_index = POWERPC_EXCP_SYSCALL;
    env->error_code = 0;
    cpu_loop_exit(cs);
}
#elif defined(TARGET_LOONGARCH64)
/* SYSCALL with a7 = 233, 4 bytes before the current PC (the kernel steps ERA past it) */
static int pagein_user(CPUState *cs, uint64_t *sp)
{
    CPULoongArchState *env = cpu_env(cs);
    if (FIELD_EX64(env_sys(env)->CSR_CRMD, CSR_CRMD, PLV) != 3) {
        return 0;
    }
    *sp = env->gpr[3];
    return 1;
}
static void pagein_save(CPUState *cs, PageinRegs *h)
{
    CPULoongArchState *env = cpu_env(cs);
    for (int i = 0; i < 32; i++) {
        h->r[i] = env->gpr[i];
    }
    h->pc = env->pc;
    h->len = 4;
}
static int64_t pagein_retval(CPUState *cs)
{
    return (int64_t)cpu_env(cs)->gpr[4];
}
static void pagein_restore(CPUState *cs, PageinRegs *h)
{
    CPULoongArchState *env = cpu_env(cs);
    for (int i = 1; i < 32; i++) {
        env->gpr[i] = h->r[i];
    }
    if (env->pc == h->pc || env->pc == h->pc - h->len) {
        env->pc = h->pc;
    }
}
static void pagein_inject(CPUState *cs, uint64_t a0, uint64_t a1, uint64_t a2)
{
    CPULoongArchState *env = cpu_env(cs);

    env->gpr[11] = 233;
    env->gpr[4] = a0;
    env->gpr[5] = a1;
    env->gpr[6] = a2;
    env->pc -= 4;
    cs->exception_index = EXCCODE_SYS;
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
static int pagein_copy_range(PageinReq *r, uint64_t off, size_t len)
{
    GByteArray *b = g_byte_array_new();
    int ret;

    if (r->write) {
        g_byte_array_append(b, r->buf + off, len);
        ret = qemu_plugin_write_memory_vaddr_fault(r->addr + off, b, QEMU_PLUGIN_VADDR_USER,
                                                   false);
    } else {
        ret = qemu_plugin_read_memory_vaddr_fault(r->addr + off, b, len, QEMU_PLUGIN_VADDR_USER,
                                                  false);
        if (ret == QEMU_PLUGIN_VADDR_OK) {
            memcpy(r->buf + off, b->data, len);
        }
    }
    g_byte_array_unref(b);
    return ret;
}

/*
 * At a user-mode boundary of r's address space: copy without faulting, all of it or nothing.
 * After the system call, @partial: as much as the CPU can reach now, a page at a time, from where
 * the last one stopped. A software-refilled TLB (MIPS, BookE PowerPC) may hold fewer pages than
 * the range spans, so such a range is copied over several boundaries; a write may then land in
 * part if the process goes away in between.
 */
static int pagein_copy(PageinReq *r, bool partial)
{
    uint64_t page = qemu_target_page_size();
    int ret;

    if (!partial && r->done_len == 0) {
        return pagein_copy_range(r, 0, r->len);
    }
    while (r->done_len < r->len) {
        uint64_t a = r->addr + r->done_len;
        size_t chunk = MIN(r->len - r->done_len, page - (a & (page - 1)));

        ret = pagein_copy_range(r, r->done_len, chunk);
        if (ret != QEMU_PLUGIN_VADDR_OK) {
            return ret;
        }
        r->done_len += chunk;
    }
    return QEMU_PLUGIN_VADDR_OK;
}
#endif

#ifndef PAGEIN_NO_INJECTOR
/* The first page of r the CPU can't reach without a fault, or -1 */
static uint64_t pagein_first_missing(PageinReq *r)
{
    uint64_t page = qemu_target_page_size();
    GByteArray *b = g_byte_array_new();
    uint64_t a, bad = (uint64_t)-1;

    for (a = (r->addr + r->done_len) & ~(page - 1); a < r->addr + r->len; a += page) {
        uint64_t at = MAX(a, r->addr + r->done_len);
        int ret = r->write ?
            (g_byte_array_set_size(b, 1), memcpy(b->data, r->buf + (at - r->addr), 1),
             qemu_plugin_write_memory_vaddr_fault(at, b, QEMU_PLUGIN_VADDR_USER, false)) :
            qemu_plugin_read_memory_vaddr_fault(at, b, 1, QEMU_PLUGIN_VADDR_USER, false);
        if (ret != QEMU_PLUGIN_VADDR_OK) {
            bad = at;
            break;
        }
    }
    g_byte_array_unref(b);
    return bad;
}

/*
 * The system call populated the range, but the CPU still can't reach a page of it: CPUs whose
 * TLB the kernel refills in software (MIPS, BookE PowerPC) only see a page table entry after a
 * TLB miss. Take that miss here, as the thread's own user-mode access at this boundary: the
 * page is in a mapping that allows the access (the call just succeeded), so the kernel's refill
 * or fault handler resolves it and returns to this instruction. (A thread of the same process
 * unmapping the range in between is the one way this could still signal.) Returns only if the
 * page became reachable meanwhile.
 */
static void pagein_touch(CPUState *cs, uint64_t addr, bool write)
{
    if (debug) {
        fprintf(stderr, "pagein: vcpu %d touch %#" PRIx64 " %s at %#" PRIx64 "\n", cs->cpu_index,
                addr, write ? "write" : "read", (uint64_t)cs->cc->get_pc(cs));
    }
    if (!cs->cc->tcg_ops->user_mmu_index) {
        return;
    }
    probe_access(cpu_env(cs), addr, 1, write ? MMU_DATA_STORE : MMU_DATA_LOAD,
                 cs->cc->tcg_ops->user_mmu_index(cs), 0);
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
    uint64_t touch = (uint64_t)-1;

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
        uint64_t pc = cs->cc->get_pc(cs);

        if (pc != h->pc && pc != h->pc - h->len) {
            /*
             * Same thread, somewhere else: a signal handler that longjmp'd, an rseq abort. Its
             * registers are its own now: leave them, and let the request go round again.
             */
            QTAILQ_REMOVE(&held, h, next);
            if (h->req) {
                h->req->inflight = false;
            }
            if (debug) {
                fprintf(stderr, "pagein: vcpu %d asid %#" PRIx64 " request %" PRIu64
                        " abandoned: back at %#" PRIx64 ", not %#" PRIx64 "\n", cs->cpu_index,
                        asid, h->req ? h->req->id : 0, pc, h->pc);
            }
            g_free(h);
            st_abandoned++;
            goto look_for_request;
        }
    }
    if (h) {
        int64_t ret = pagein_retval(cs);
        QTAILQ_REMOVE(&held, h, next);
        if (debug) {
            fprintf(stderr, "pagein: vcpu %d asid %#" PRIx64 " request %" PRIu64 " back at %#"
                    PRIx64 ": %" PRId64 " (shift %d, round %d)\n", cs->cpu_index, asid,
                    h->req ? h->req->id : 0, h->pc, ret, h->req ? h->req->shift : 0,
                    h->req ? h->req->rounds : 0);
        }
        pagein_restore(cs, h);
        r = h->req;
        g_free(h);
        if (r && ndone < ARRAY_SIZE(done)) {
            r->inflight = false;
            int shift = r->shift ? r->shift : TARGET_PAGE_BITS;
            uint64_t start = r->addr & ~((1ull << shift) - 1);

            if (ret == -EINVAL && !guest_page_shift && shift < 16) {
                r->first_errno = r->first_errno ? r->first_errno : EINVAL;
                r->shift = shift + 2;       /* maybe a larger page: try again aligned to it */
            } else if (ret < 0 && ret > -4096) {
                pagein_finish(r, PANDA_PAGEIN_EGUEST,
                              r->first_errno ? r->first_errno : (int)-ret, &done[ndone++]);
            } else {
                if (!guest_page_shift && (start & ((1ull << (shift + 2)) - 1))) {
                    guest_page_shift = shift;   /* worked, and no larger alignment did it */
                }
                r->populated = true;
                int c = pagein_copy(r, true);
                if (c == QEMU_PLUGIN_VADDR_OK) {
                    pagein_finish(r, PANDA_PAGEIN_OK, 0, &done[ndone++]);
                } else if (c == QEMU_PLUGIN_VADDR_IO) {
                    pagein_finish(r, PANDA_PAGEIN_EIO, 0, &done[ndone++]);
                } else if (r->rounds >= PAGEIN_MAX_ROUNDS) {
                    pagein_finish(r, PANDA_PAGEIN_ERECLAIMED, 0, &done[ndone++]);
                } else if (r->touches < 2 * (r->len / qemu_target_page_size()) + 8) {
                    touch = pagein_first_missing(r);
                    r->touches++;
                }
                /* else it waits for another round */
            }
        }
        pagein_update_loop_bit();
        qemu_mutex_unlock(&lock);
        pagein_complete(done, ndone);
        if (touch != (uint64_t)-1) {
            pagein_touch(cs, touch, r->write);
        }
        cpu_loop_exit_noexc(cs);    /* the registers changed: look the block up again */
    }

look_for_request:
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
    {
        /* already there (or now): no system call */
        int c = pagein_copy(r, r->populated);
        if (c == QEMU_PLUGIN_VADDR_OK || c == QEMU_PLUGIN_VADDR_IO) {
            if (ndone < ARRAY_SIZE(done)) {
                pagein_finish(r, c == QEMU_PLUGIN_VADDR_OK ? PANDA_PAGEIN_OK : PANDA_PAGEIN_EIO,
                              0, &done[ndone++]);
            }
            goto out;
        }
        if (r->populated && r->touches < 2 * (r->len / qemu_target_page_size()) + 8) {
            /* populated, but the CPU's TLB doesn't have the next page yet: its miss, here */
            uint64_t touch_addr = pagein_first_missing(r);
            r->touches++;
            pagein_update_loop_bit();
            qemu_mutex_unlock(&lock);
            pagein_complete(done, ndone);
            if (touch_addr != (uint64_t)-1) {
                pagein_touch(cs, touch_addr, r->write);
            }
            return;
        }
        r->populated = false;
    }
    if (r->rounds >= PAGEIN_MAX_ROUNDS) {
        if (ndone < ARRAY_SIZE(done)) {
            pagein_finish(r, PANDA_PAGEIN_ERECLAIMED, 0, &done[ndone++]);
        }
        goto out;
    }
    {
        int shift = r->shift ? r->shift : guest_page_shift ? guest_page_shift : TARGET_PAGE_BITS;
        uint64_t page = qemu_target_page_size();
        uint64_t start = r->addr & ~((1ull << shift) - 1);
        uint64_t end = ROUND_UP(r->addr + r->len, page);

        r->shift = shift;

        h = g_new0(PageinRegs, 1);
        h->asid = asid;
        h->sp = sp;
        h->req = r;
        pagein_save(cs, h);
        QTAILQ_INSERT_TAIL(&held, h, next);
        r->inflight = true;
        r->rounds++;
        st_injected++;
        if (debug) {
            fprintf(stderr, "pagein: vcpu %d asid %#" PRIx64 " request %" PRIu64 " call at %#"
                    PRIx64 " sp %#" PRIx64 ": [%#" PRIx64 ", %#" PRIx64 ") %s\n", cs->cpu_index,
                    asid, r->id, h->pc, sp, start, end, r->write ? "write" : "read");
        }
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
    Done d = { 0 };
    PageinReq *r;
    PageinRegs *h;

    if (!qatomic_read(&inited)) {
        return;
    }
    for (;;) {
        qemu_mutex_lock(&lock);
        guest_page_shift = 0;               /* maybe another guest now */
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
