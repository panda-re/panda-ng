/*
 * System calls paired in the engine: ABI declarations, task keys and return pairing.
 *
 * A consumer (one per run; the syscalls2 plugin) declares, as data, the system-call ABIs the
 * guest kernel serves. The engine then recognizes those traps, decodes their number and
 * arguments, keeps each call whose return is wanted in a pending set keyed by the calling
 * task, and pairs it with the return to user mode that completes it. The engine holds no
 * operating-system recipe: what it knows about a particular kernel (which numbers never
 * return, where a task's key lives) comes from the declarations.
 *
 * Pay for use. Nothing here runs, and translated code is unchanged, until an ABI is declared.
 * After a declaration, the trap sites of the declared forms test the number against the want
 * map in translated code (a byte load); the return-to-user sites check for a pending call
 * inline only while some number's return is wanted (arming and disarming flush translations).
 *
 * Pairing. Every return to user mode (x86 SYSRET, SYSEXIT and IRET; AArch64 ERET) is a
 * candidate. For the returning task's newest pending call:
 *   - at the call's return address (pc + len): its return;
 *   - at its restart address (the trap instruction, pc; for an ABI with ret_skip, the
 *     instruction it skips): its return, flagged RESTART (the kernel restarts it);
 *   - elsewhere, for a ctx_replace call (execve): the call ends (ENDED, no return value);
 *   - elsewhere, otherwise: a signal handler runs first; the call stays pending, and the
 *     handler's own calls stack above it.
 * A 0 return at the return address of a pending shared_child call (vfork, clone), made in the
 * caller's address space by another task (or under the caller's call key), is that child's
 * first return: it is delivered flagged CHILD and the caller's call stays pending.
 * noreturn calls (exit, exit_group, rt_sigreturn) are never kept. The set is bounded: a full
 * set evicts its oldest call (one left by a killed task). Reset, snapshot load and fast
 * restore empty it.
 *
 * These are the rules of POSIX kernels. An OS that calls back into user mode from inside a
 * system call and returns into the kernel (Windows user callbacks) needs further rule kinds.
 *
 * Phase 1: x86_64 and aarch64 targets, TCG only. See docs/design/syscall-pairing.md.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_SYSCALL_PAIR_H
#define PANDA_SYSCALL_PAIR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CPUState;

/* --- ABI declarations -------------------------------------------------------------------- */

/* PandaSyscallAbi.form: which traps are this ABI's */
enum {
    PANDA_SYSCALL_FORM_X86_SYSCALL,     /* SYSCALL from 64-bit code at CPL 3 */
    PANDA_SYSCALL_FORM_X86_INT80,       /* INT 0x80 at CPL 3 (protected or long mode) */
    PANDA_SYSCALL_FORM_A64_SVC,         /* SVC from AArch64 EL0 */
    PANDA_SYSCALL_FORM_A32_SVC,         /* SVC from AArch32 EL0, under an AArch64 EL1 */
    PANDA_SYSCALL_FORM_X86_SYSENTER,    /* SYSENTER at CPL 3 */
    PANDA_SYSCALL_FORM_X86_SYSCALL32,   /* SYSCALL from 32-bit code in long mode at CPL 3 */
    PANDA_SYSCALL_FORM_COUNT
};

/*
 * Registers are numbered in the form's instruction set: x86 as in ModRM (0 RAX, 1 RCX, 2 RDX,
 * 3 RBX, 4 RSP, 5 RBP, 6 RSI, 7 RDI, 8-15 R8-R15); AArch64 X0-X30; AArch32 R0-R14. The 32-bit
 * forms (X86_INT80, X86_SYSENTER, X86_SYSCALL32, A32_SVC) deliver 32-bit numbers, arguments and
 * return values (the return sign-extended). An argument register PANDA_SYSCALL_ARG_MEM(r) is
 * the word (of the form's width) in user memory at register r's value, read at the entry (0 if
 * it cannot be read): Linux's 32-bit vDSO passes argument 6 on the stack.
 *
 * ret_skip: the kernel returns past that many bytes after the trap, and restarts the call at the
 * instruction there. Linux's 32-bit vDSO (__kernel_vsyscall) follows its SYSENTER or SYSCALL with
 * an INT 0x80 (2 bytes): the kernel returns past it, and a restart runs it.
 *
 * Number sets are bitmaps of nbits + 1 bits (bit i: number i; bit nbits: every number not
 * below nbits, and negative ones). The engine copies them.
 */
typedef struct PandaSyscallAbi {
    uint32_t size;              /* sizeof(PandaSyscallAbi) */
    uint32_t id;                /* the consumer's (0-255), delivered as PandaSyscallCall.abi */
    uint32_t form;              /* PANDA_SYSCALL_FORM_* */
    uint32_t nbits;             /* 1 to 65536 */
    uint8_t nr_reg;             /* the number */
    uint8_t nr_width;           /* the number's width in the register: 32 (sign-extended,
                                 * as Linux reads it) or 64 */
    uint8_t ret_reg;            /* the return value */
    uint8_t nargs;              /* 0 to 6 */
    uint8_t arg_reg[6];
    uint8_t ret_skip;           /* 0, or the bytes after the trap the call returns past */
    int64_t err_min;            /* a return in [err_min, -1] is an error; 0: no error range */
    const uint8_t *noreturn;    /* never pending: no return (exit) or not one (rt_sigreturn) */
    const uint8_t *ctx_replace; /* a return elsewhere ends the call (a successful execve) */
    const uint8_t *shared_child;/* may start a child at the return address (vfork, clone) */
} PandaSyscallAbi;

#define PANDA_SYSCALL_ARG_MEM(r) (0x80 | (r))

/* --- Calls ------------------------------------------------------------------------------- */

/*
 * A call, as the consumer gets it at its entry and at its return. At the entry it is the
 * pending record itself (or, for a call not kept, the vCPU's delivery slot); at the return,
 * the record until the callback returns. data[] is the consumer's (panda_syscall_consumer's
 * data_size bytes): written at the entry, kept with the pending call, given back at its return.
 */
typedef struct PandaSyscallCall {
    uint64_t id;                /* unique in the run */
    int64_t nr;
    uint64_t args[6];
    uint64_t pc;                /* the trap instruction; the call returns to pc + len */
    uint64_t key;               /* the task key at the entry (keytype), or 0 */
    uint32_t len;               /* the trap's length, and the ABI's ret_skip */
    uint16_t vcpu;              /* the entry's */
    uint8_t abi;                /* PandaSyscallAbi.id */
    uint8_t keytype;            /* PANDA_TASK_KEY_* the call was kept under; 0 if not kept */
    uint64_t data[];
} PandaSyscallCall;

/* Delivery flags */
enum {
    PANDA_SYSCALL_RETURN  = 1u << 0,    /* a return (else the entry) */
    PANDA_SYSCALL_RESTART = 1u << 1,    /* returned to the restart address: the kernel restarts
                                         * the call (it traps again, a new call) */
    PANDA_SYSCALL_ERROR   = 1u << 2,    /* the value is in the ABI's error range */
    PANDA_SYSCALL_MOVED   = 1u << 3,    /* returned on another vCPU than it entered */
    PANDA_SYSCALL_CHILD   = 1u << 4,    /* a shared_child call's child: the call stays pending */
    PANDA_SYSCALL_ENDED   = 1u << 5,    /* a ctx_replace call ended elsewhere: no return value */
};

/*
 * The consumer's callback, on the vCPU's thread (vCPUs call it concurrently). At an entry
 * (flags 0) the registers are those of the trapping code, before the trap; @ret is 0. At a
 * return (flags RETURN | ...) the return has completed (user mode, at its destination) and
 * @ret is the return value. The callback may read guest memory and registers, and change
 * registers (at the entry, before the kernel reads them). Returns are delivered for the calls
 * kept: the numbers whose return is wanted at the entry, or the calls their entry callback
 * keeps with panda_syscall_keep(). The engine calls it last (a tail call): an entry's callback
 * decides with panda_syscall_keep(), not a return value.
 */
typedef void (*PandaSyscallFn)(void *ctx, struct CPUState *cpu, PandaSyscallCall *call,
                               int64_t ret, uint32_t flags);

/*
 * From an entry's callback, for the call it was given: deliver its return (@keep) or not,
 * whatever its number's want said when it entered (a noreturn call is never kept). Nothing to do
 * when the call is kept already, or not (PandaSyscallCall.keytype): call it only to change
 * that. The call's data[] is kept as it is then: call it after writing them, and do not use
 * @call after it.
 */
void panda_syscall_keep(struct CPUState *cpu, PandaSyscallCall *call, bool keep);

/*
 * Become the consumer (one per run), before declaring: @data_size bytes (up to 1024) of
 * PandaSyscallCall.data per call. False if another owner is the consumer, or not TCG.
 * panda_syscall_consumer_release(), on the main thread with the vCPUs stopped (a plugin's
 * uninit), drops every pending call, the declarations and the wants; the trap and return sites
 * are translated again without them.
 */
bool panda_syscall_consumer(void *owner, PandaSyscallFn fn, void *ctx, uint32_t data_size);
void panda_syscall_consumer_release(void *owner);

/* Declare an ABI (one per form). Returns its handle (>= 0), or -1. */
int panda_syscall_abi_declare(void *owner, const PandaSyscallAbi *abi);

/* panda_syscall_want() */
enum {
    PANDA_SYSCALL_WANT_ENTRY  = 1u << 0,
    PANDA_SYSCALL_WANT_RETURN = 1u << 1,
};

/*
 * What the consumer wants of number @nr (any number not below nbits, or negative: index nbits)
 * of ABI @abi: its entry, its return, both or neither. Any thread, any time; cheap enough to
 * call from the callback. A noreturn number's return is never wanted.
 */
void panda_syscall_want(int abi, int64_t nr, uint32_t want);

/* --- Task keys --------------------------------------------------------------------------- */

/*
 * A task key is a value that is the same for every system call one task makes and differs
 * between tasks alive at the same time, read at the user/kernel boundary. No architecture has
 * a "current task" register; a key is where the OS keeps a per-task value:
 *   REG   the value of a register at the boundary (AArch64 Linux: SP_EL1, the task's kernel
 *         stack, banked while it runs at EL0)
 *   WORD  a word in guest RAM that the consumer's resolver locates for the current value of a
 *         base register (x86_64 Linux: the per-CPU top-of-stack word, at an offset from
 *         KERNEL_GS_BASE). The engine maps the word once per vCPU and base value, and reads it
 *         through that mapping; a different base value, a reset or a snapshot load makes it
 *         resolve again.
 * Without a declaration, or while the resolver cannot locate the word (0), pairing uses the
 * call key: the address-space root (x86 CR3, AArch64 TTBR0) with the user stack pointer. It
 * pairs a call with its return, but it is not a task identity (a task has several).
 *
 * Keys are reused once a task exits; a key identifies a live task. A call pending when its
 * task is killed stays until its set evicts it.
 */
enum {
    PANDA_TASK_KEY_NONE,
    PANDA_TASK_KEY_CALL,        /* (address-space root, user stack pointer) */
    PANDA_TASK_KEY_REG,
    PANDA_TASK_KEY_WORD,
};

enum {
    PANDA_TASK_REG_ARM_SP_EL1,          /* REG */
    PANDA_TASK_REG_X86_KERNEL_GS_BASE,  /* WORD's base */
};

/* WORD: the guest physical address of the key word for base value @base on @cpu, or 0 if it
 * cannot be located yet (early boot). Called on the vCPU's thread, at the boundary. */
typedef uint64_t (*PandaTaskKeyResolveFn)(void *ctx, struct CPUState *cpu, uint64_t base);

typedef struct PandaTaskKeyDecl {
    uint32_t size;              /* sizeof(PandaTaskKeyDecl) */
    uint32_t form;              /* PANDA_TASK_KEY_REG or _WORD */
    uint32_t reg;               /* PANDA_TASK_REG_*: REG's key, WORD's base */
    PandaTaskKeyResolveFn resolve;
    void *ctx;
} PandaTaskKeyDecl;

/* The consumer declares the task key, before its first ABI. False if invalid for the target. */
bool panda_task_key_declare(void *owner, const PandaTaskKeyDecl *decl);

/* The current task's key, valid at the boundary and in the callback: its type (0: none). */
uint32_t panda_task_key(struct CPUState *cpu, uint64_t *key);

/* --- Counters ---------------------------------------------------------------------------- */

typedef struct PandaSyscallStats {
    uint64_t entries;           /* entries that reached the engine (number wanted) */
    uint64_t kept;              /* calls kept (return wanted) */
    uint64_t paired;            /* returns delivered (RESTART included) */
    uint64_t restarts;
    uint64_t children;          /* CHILD returns delivered */
    uint64_t ended;             /* ctx_replace calls ended elsewhere */
    uint64_t elsewhere;         /* returns elsewhere that left the call pending (signals) */
    uint64_t misses;            /* returns that found calls in their set but none of the key */
    uint64_t evicted;           /* calls evicted from a full set */
    uint64_t not_kept;          /* calls that could not be kept (every way busy) */
    uint64_t resolves;          /* WORD resolver calls */
    uint64_t call_key;          /* calls kept under the call key while a WORD key was declared */
    uint64_t pending;           /* calls pending now */
} PandaSyscallStats;

void panda_syscall_stats(PandaSyscallStats *out);

#ifdef __cplusplus
}
#endif

#endif /* PANDA_SYSCALL_PAIR_H */
