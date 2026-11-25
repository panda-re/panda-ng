/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PANDA_INJECT_H
#define PANDA_INJECT_H
/*
 * System calls injected into a guest task: the task makes a sequence of system calls chosen by
 * the host, one at a time, each chosen after the previous one returned, and then continues as
 * if nothing had happened.
 *
 * A session names a target and a function, next(). The engine serves it at the next user-mode
 * instruction boundary of the target (the CPU loop, between blocks): it saves the thread's
 * registers and calls next(), which returns the first call; the thread enters the kernel as if
 * it had executed a system call instruction there, and when it is back in user mode at that
 * instruction next() gets the return value and returns the next call, or finishes. Then the
 * thread's registers are put back and done() runs. Between calls next() runs on the vCPU's
 * thread with the task current: it can read and write the task's memory (panda_guest_write_*),
 * and the guest's other vCPUs keep running.
 *
 * Targets:
 *   PANDA_INJECT_TARGET_ASID  any thread of the address space (panda_current_asid())
 *   PANDA_INJECT_TARGET_TASK  the thread whose task key (panda_task_key(), declared by the
 *                             syscall consumer: REG or WORD keys) is @target
 *   panda_inject_hold()       the task of a system-call entry callback, before its own call:
 *                             the call does not enter the kernel yet; the session runs at the
 *                             trap instruction, and then the task executes it again, and its
 *                             entry is delivered again flagged PANDA_SYSCALL_RESUMED.
 *
 * A thread that comes back from an injected call is recognized by a token the engine leaves in
 * a register system calls preserve (the marker: x86_64 r12, i386 ebp, AArch64 x28, AArch32 r8,
 * riscv s11, mips s7, ppc r30, loongarch s8), at the call's stack pointer and return address.
 * A signal delivered on the way out of the call runs its handler with the token still set;
 * it is left alone until the thread is back at the call. A thread that comes back elsewhere,
 * without the token (a handler that longjmp'd, an rseq abort), keeps the registers it has (the
 * injected call's arguments and the registers the call clobbers) and the session ends with
 * PANDA_INJECT_EABANDONED. On i386 a call with six arguments has no free register for the token:
 * the thread is then recognized by its stack pointer and return address alone.
 *
 * Guest-visible effects: the system calls themselves (seccomp, ptrace, audit and syscall
 * tracers see them), and whatever they do. The engine knows no system-call numbers: next()
 * gives them for the task's ABI (PandaInjectStep.abi). fork, vfork and clone return twice;
 * the child comes back with the injected registers into an address space the session does not
 * match, so callers must not inject them (pandare2 refuses them by name).
 *
 * Refused at submission, with their own status: during record/replay (a recording would not
 * hold the calls; see docs/design/rr-guest-mutation-api.md), under KVM, on targets without an
 * injector. Starting a recording is refused while any thread is inside an injected call.
 * Sessions queued when a recording starts are cancelled with PANDA_INJECT_ERECORD.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CPUState;
struct PandaSyscallCall;

typedef enum PandaInjectStatus {
    PANDA_INJECT_OK = 0,
    PANDA_INJECT_QUEUED = 1,        /* submit: accepted; done() runs later */
    PANDA_INJECT_ETIMEDOUT = -2,    /* the target did not run in user mode in time */
    PANDA_INJECT_ECANCELED = -3,    /* cancelled, or a snapshot load, reset or shutdown came first */
    PANDA_INJECT_ERECORD = -4,      /* refused: record/replay is active */
    PANDA_INJECT_EACCEL = -5,       /* refused: the accelerator has no safe point (KVM) */
    PANDA_INJECT_EARCH = -6,        /* refused: no injector for this target or task */
    PANDA_INJECT_EINVAL = -7,       /* refused: no next(), bad target */
    PANDA_INJECT_EBUSY = -8,        /* refused: too many sessions outstanding */
    PANDA_INJECT_EABANDONED = -9,   /* the thread came back elsewhere (longjmp, rseq) */
    PANDA_INJECT_ENOTHERE = -10,    /* panda_inject_hold() outside an entry it can hold */
    PANDA_INJECT_EFAILED = -11,     /* next() returned PANDA_INJECT_FAIL */
} PandaInjectStatus;

#define PANDA_INJECT_MAX_OUTSTANDING 256

/* The task's system-call ABI at the boundary: which numbers and argument registers apply */
typedef enum PandaInjectAbi {
    PANDA_INJECT_ABI_X86_64,        /* SYSCALL, asm/unistd_64.h */
    PANDA_INJECT_ABI_I386,          /* INT 0x80, asm/unistd_32.h (i386, x86_64 compat tasks) */
    PANDA_INJECT_ABI_AARCH64,       /* SVC, asm-generic */
    PANDA_INJECT_ABI_ARM_EABI,      /* SVC 0 with r7 (arm, aarch64 compat tasks) */
    PANDA_INJECT_ABI_RISCV64,       /* ECALL, asm-generic */
    PANDA_INJECT_ABI_MIPS_O32,      /* SYSCALL, 4000+; arguments 5 and 6 on the user stack */
    PANDA_INJECT_ABI_MIPS_N64,      /* SYSCALL, 5000+ (any task of a 64-bit kernel) */
    PANDA_INJECT_ABI_PPC,           /* sc, 32-bit */
    PANDA_INJECT_ABI_PPC64,         /* sc, 64-bit tasks */
    PANDA_INJECT_ABI_LOONGARCH64,   /* SYSCALL, asm-generic */
    PANDA_INJECT_ABI_COUNT
} PandaInjectAbi;

enum {
    PANDA_INJECT_TARGET_ASID,
    PANDA_INJECT_TARGET_TASK,
};

typedef struct PandaInjectCall {
    int64_t nr;
    uint64_t args[6];
    int32_t nargs;                  /* arguments used, or -1 if unknown (all six are set) */
} PandaInjectCall;

typedef struct PandaInjectStep {
    uint64_t id;
    int32_t abi;                    /* PandaInjectAbi */
    uint32_t calls;                 /* injected calls that returned since the thread's registers
                                     * were saved (0: a start, or a start again after AGAIN) */
    int64_t ret;                    /* with calls > 0: the last one's value, -errno on failure */
    uint64_t touch;                 /* AGAIN: a user address the thread accesses (and may fault
                                     * on) once its registers are back, or -1 (the default) */
} PandaInjectStep;

/* next()'s answers */
enum {
    PANDA_INJECT_DONE,              /* finished: the registers go back, done(OK) */
    PANDA_INJECT_CALL,              /* inject *call */
    PANDA_INJECT_AGAIN,             /* not now: the registers go back, and the session waits for
                                     * a later boundary of its target (calls stays as it is) */
    PANDA_INJECT_FAIL,              /* give up: the registers go back, done(EFAILED) */
};

/*
 * On the vCPU's thread, at a user-mode boundary of the target, with the task current and its
 * registers saved; the engine's injection lock is not held (next() may submit or cancel). The
 * guest's registers are the injected call's (after the first call): read the task's own state
 * before submitting. *call is zeroed, nargs -1.
 */
typedef int (*panda_inject_next_fn)(void *opaque, struct CPUState *cpu, PandaInjectStep *step,
                                    PandaInjectCall *call);
/* Exactly once per accepted session: on the vCPU thread after the registers went back, or on
 * the thread that cancelled it. */
typedef void (*panda_inject_done_fn)(void *opaque, uint64_t id, int status);

/*
 * Queue a session for @target (PANDA_INJECT_TARGET_*). @timeout_ns: guest (virtual clock)
 * nanoseconds until it has to have finished, 0 for the default (5 s), UINT64_MAX for none.
 * Returns PANDA_INJECT_QUEUED and sets *@id, or a refusal (done() never runs). Any thread.
 */
int panda_inject_submit(int target_kind, uint64_t target, uint64_t timeout_ns,
                        panda_inject_next_fn next, panda_inject_done_fn done, void *opaque,
                        uint64_t *id);

/*
 * From a system-call entry callback (panda_syscall_consumer's, at an entry: flags without
 * PANDA_SYSCALL_RETURN), for @call: run a session before it. The call does not enter the
 * kernel now and its return is not delivered (the engine drops its pending record once the
 * entry's delivery is over; @call stays valid until the callback returns); after the session the
 * task executes its trap again, and that entry is delivered flagged PANDA_SYSCALL_RESUMED. A
 * pandare2 or syscalls2 consumer finds @call with syscalls2_current_call_ptr(). Returns PANDA_INJECT_QUEUED, or a refusal:
 * ENOTHERE outside an entry callback, or at an entry the engine cannot hold.
 */
int panda_inject_hold(struct CPUState *cpu, struct PandaSyscallCall *call, uint64_t timeout_ns,
                      panda_inject_next_fn next, panda_inject_done_fn done, void *opaque,
                      uint64_t *id);

/*
 * A system call the guest kernel skipped (a kernel-side hook that held it: the call did not run,
 * and the kernel is returning to user mode with its own value in the return register), to be made
 * again after a session. The thread of address space @asid with user stack pointer @sp, when it
 * is back in user mode at @ret_pc (the address after its trap instruction), runs the session
 * (@next may be NULL: none); then its registers go back, its PC goes back to the trap
 * (@ret_pc - @rw->len) and @rw's number and arguments go into their registers, so that it makes
 * the call again. Its entry is then delivered flagged PANDA_SYSCALL_RESUMED to a system-call
 * consumer, and panda_inject_rewound() recognizes it at a kernel-side hook. The call is made
 * again whatever the session's outcome (done(), failed, cancelled, timed out) once the thread is
 * back at @ret_pc; a session cancelled or timed out before it is, is gone (the call stays skipped).
 * Any thread or callback; the kernel-side hook typically calls it from its entry hypercall.
 */
typedef struct PandaInjectRewind {
    int64_t nr;
    uint64_t args[6];
    int32_t nargs;                  /* arguments to put back, or -1: all six */
    uint32_t len;                   /* the trap instruction's length */
} PandaInjectRewind;

int panda_inject_rewind(uint64_t asid, uint64_t sp, uint64_t ret_pc, const PandaInjectRewind *rw,
                        uint64_t timeout_ns, panda_inject_next_fn next,
                        panda_inject_done_fn done, void *opaque, uint64_t *id);

/* At a kernel-side system-call hook: is the call of the thread of @asid and user stack pointer @sp
 * at trap address @pc one that panda_inject_rewind() put back? (Consumes the answer.) */
bool panda_inject_rewound(uint64_t asid, uint64_t sp, uint64_t pc);

/* Cancel a session: true if it was found. One that has started ends at its thread's next
 * boundary (its registers go back); done(ECANCELED). */
bool panda_inject_cancel(uint64_t id);

/* The ABI an injection would use for the task on @cpu now (a vCPU callback in user mode or
 * at a system-call entry), or -1 */
int panda_inject_abi(struct CPUState *cpu);

const char *panda_inject_strerror(int status);

typedef struct PandaInjectStats {
    uint64_t submitted;
    uint64_t calls;                 /* system calls injected (restarts not counted) */
    uint64_t restarts;              /* injected calls the kernel restarted */
    uint64_t completed;
    uint64_t abandoned;             /* threads that came back elsewhere, without the token */
    uint64_t stale;                 /* of those, the ones back at the call's own return address
                                     * and stack pointer: a restore there would have been wrong */
    uint64_t overframes;            /* boundaries of a thread running a handler over a call */
    uint64_t holds;                 /* entries held */
    uint64_t rewinds;               /* skipped calls put back at their trap */
    uint64_t resumed;               /* held entries delivered again */
    uint64_t held;                  /* threads inside an injected call now */
    uint64_t queued;                /* sessions waiting now */
} PandaInjectStats;

void panda_inject_stats(PandaInjectStats *out);

#ifdef __cplusplus
}
#endif
#endif
