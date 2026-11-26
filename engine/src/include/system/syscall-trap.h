/*
 * Syscall events at the trap (system mode, TCG)
 *
 * The target calls syscall_trap_enter() when a user task traps for a
 * syscall, and syscall_trap_return() on every return to user mode while a
 * call is pending.  The core filters the traps by ABI and number, hands
 * them to one consumer (the TCG plugin API),
 * and pairs each call with its return.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef SYSTEM_SYSCALL_TRAP_H
#define SYSTEM_SYSCALL_TRAP_H

#include "qemu/atomic.h"
#include "exec/vaddr.h"

/* the same values as enum qemu_plugin_syscall_abi */
enum {
    SYSCALL_ABI_X86_64,
    SYSCALL_ABI_I386,
    SYSCALL_ABI_AARCH64,
    SYSCALL_ABI_ARM,
    SYSCALL_ABI_ARM_OABI,
    SYSCALL_ABI_MIPS,
    SYSCALL_ABI_PPC64,
    SYSCALL_ABI_PPC,
    SYSCALL_ABI_RISCV64,
    SYSCALL_ABI_RISCV32,
    SYSCALL_ABI_LOONGARCH64,
    SYSCALL_ABI_COUNT
};

#define SYSCALL_TRAP_NUMS   8192        /* numbers a consumer can watch */
#define SYSCALL_TRAP_ENTER  1           /* watch flags */
#define SYSCALL_TRAP_RETURN 2

typedef struct SyscallTrap {
    int abi;
    int64_t num;
    uint64_t args[6];
    uint64_t pc;            /* the address past the trap instruction */
    int64_t ret;            /* return: the value; enter: with skip */
    /*
     * The kernel's per-CPU base while the task is in user mode (x86: the
     * kernel GS base; AArch64: TPIDR_EL1), or, for kernels that find the
     * task from the stack (AArch32), the kernel stack pointer.
     */
    uint64_t percpu;
    bool skip;              /* enter: do not enter the kernel; return ret */
    bool want_return;       /* enter: report the call's return */
    bool restart;           /* return: the kernel is restarting the call */
    bool keep;              /* return: not the caller's; keep it pending */
    uint64_t task;          /* the consumer's, kept with a pending call */
    void *data;             /* the consumer's, released through drop() */
} SyscallTrap;

typedef struct SyscallTrapOps {
    /* the vCPU is in user mode; CPUState.syscall_kmmu is set */
    void (*enter)(CPUState *cpu, SyscallTrap *sc, void *opaque);
    void (*ret)(CPUState *cpu, SyscallTrap *sc, void *opaque);
    /* a pending call is discarded without its return (may be NULL) */
    void (*drop)(SyscallTrap *sc, void *opaque);
} SyscallTrapOps;

/* one consumer at a time; false if another one is set */
bool syscall_trap_set_consumer(const SyscallTrapOps *ops, void *opaque);
/* Main thread/BQL, vCPUs stopped: owner-checked drain before detaching. */
bool syscall_trap_clear_consumer(const SyscallTrapOps *ops, void *opaque);
void syscall_trap_watch(int abi, int64_t num, unsigned flags); /* num -1: all */
void syscall_trap_unwatch_all(void);
void syscall_trap_stats(uint64_t *pending, uint64_t *full, uint64_t *stale);

/* Stopped-vCPU restore/reset boundary: abandon pending calls. Drops run
 * outside sc_lock. Watches and the consumer
 * remain installed; a return whose entry predates the snapshot is not rebuilt. */
unsigned syscall_trap_cancel_all(void);

/*
 * Read guest virtual memory as the vCPU sees it: its TLB at the current
 * privilege, then the kernel's (CPUState.syscall_kmmu), then a page walk.
 */
bool syscall_trap_read(CPUState *cpu, vaddr addr, void *buf, size_t len);

/*
 * Read or write it with the vCPU's permissions, user mode's first, then
 * the kernel's: a write never reaches a page the guest could not write
 * (read-only, copy-on-write).  True if all of it was accessed; otherwise
 * *@bad is the first page that was not, and nothing was written.
 */
bool syscall_trap_access(CPUState *cpu, vaddr addr, void *buf, size_t len,
                         bool write, vaddr *bad);

/* for the targets */
#define SYSCALL_TRAP_ARGS 1     /* the consumer changed the arguments */
#define SYSCALL_TRAP_SKIP 2     /* do not enter the kernel; return *ret */
#define SYSCALL_TRAP_RET 4      /* the consumer changed the return value */

extern bool syscall_trap_on;
extern uint32_t syscall_trap_pending;

static inline bool syscall_trap_enabled(void)
{
    return unlikely(qatomic_read(&syscall_trap_on));
}

static inline bool syscall_trap_waiting(void)
{
    return unlikely(qatomic_read(&syscall_trap_pending) != 0);
}

/*
 * @k1/@k2 identify the task: the same at the trap and at the call's
 * return, different for any other task.  @len is the trap instruction's
 * length.  @kmmu: the kernel's MMU indexes, for syscall_trap_read() in the
 * callbacks.
 */
int syscall_trap_enter(CPUState *cpu, int abi, int64_t num, uint64_t args[6],
                       uint64_t pc, unsigned len, uint64_t k1, uint64_t k2,
                       uint64_t percpu, uint32_t kmmu, uint64_t *ret);
int syscall_trap_return(CPUState *cpu, uint64_t pc, uint64_t k1, uint64_t k2,
                        uint64_t percpu, uint32_t kmmu, uint64_t *ret);

#endif /* SYSTEM_SYSCALL_TRAP_H */
