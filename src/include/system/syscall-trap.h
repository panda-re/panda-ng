/*
 * Syscall events at the trap (system mode, TCG)
 *
 * The target calls syscall_trap_enter() when a user task traps for a
 * syscall, and syscall_trap_return() on every return to user mode while a
 * call is pending.  The core filters the traps by ABI and number, hands
 * them to one consumer (the TCG plugin API or Penguin's syscall hooks),
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
    /*
     * Set by the core: the consumer asked for a fault at this trap, and
     * this is the same call coming back after the guest handled it.
     */
    bool resumed;
    /*
     * Set by the consumer: make the guest take the fault a user access to
     * fault_addr would take, then deliver this event again (resumed) once
     * the task comes back to the same trap.  The call is held meanwhile,
     * with task and data.
     */
    bool fault;
    bool fault_write;
    uint64_t fault_addr;
    /*
     * Set by the consumer at the entry: run this call in the task first
     * (inject_num, of the event's ABI), then deliver this event again
     * (resumed, injected set, inject_ret its return value) once the task is
     * back at the trap with its registers as they were.  The call is held
     * meanwhile, with task and data.  The injected call reaches no consumer:
     * not its entry, not its return, not a restart of it.
     */
    bool inject;
    int64_t inject_num;
    uint64_t inject_args[6];
    bool injected;
    int64_t inject_ret;
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
void syscall_trap_watch(int abi, int64_t num, unsigned flags); /* num -1: all */
void syscall_trap_unwatch_all(void);
void syscall_trap_stats(uint64_t *pending, uint64_t *full, uint64_t *stale);
uint64_t syscall_trap_faults(void);    /* faults raised for consumers */

/*
 * The task is gone: drop its calls, pending or held (those whose
 * SyscallTrap.task is @task, not 0).  Returns how many.
 */
unsigned syscall_trap_forget_task(uint64_t task);

/*
 * Read guest virtual memory as the vCPU sees it: its TLB at the current
 * privilege, then the kernel's (CPUState.syscall_kmmu), then a page walk.
 */
bool syscall_trap_read(CPUState *cpu, vaddr addr, void *buf, size_t len);

/*
 * The host address of the guest page at @page (page-aligned) if the vCPU's
 * TLB has it, at the current privilege or the kernel's; else NULL.  Valid
 * while the vCPU does not run.
 */
const void *syscall_trap_host_page(CPUState *cpu, vaddr page);

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
/*
 * The consumer wants a fault taken first: the target puts the task back
 * where a fault would leave it (enter: at the trap instruction, as if it
 * had not run; return: at the user address the call returns to, as after
 * the return) and calls syscall_trap_fault().  Nothing else changed.
 */
#define SYSCALL_TRAP_FAULT 8
/*
 * Enter: run another call instead: its number is *ret, its arguments
 * args[]; the target puts them in the registers and lets the trap go on.
 */
#define SYSCALL_TRAP_INJECT 16
/*
 * Return: that call is back; *ret was its value.  The target puts the
 * registers of the call it replaced back (SyscallTrapRewind) and the task
 * back at the trap instruction, len bytes before the return address, so
 * that the call traps again.
 */
#define SYSCALL_TRAP_REWIND 32

typedef struct SyscallTrapRewind {
    int abi;                /* the call's: x86_64's or AArch64's only */
    int64_t num;
    uint64_t args[6];
    unsigned len;
} SyscallTrapRewind;

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
                        uint64_t percpu, uint32_t kmmu, uint64_t *ret,
                        SyscallTrapRewind *rw);

/*
 * Raise the fault the consumer asked for, as a user-mode load or store at
 * the current PC would.  @ra: the helper's return address when the state
 * has to be restored from it (0 if the CPU state is exact).  Does not
 * return, unless the page became accessible meanwhile: then the target
 * runs the trap again (the held call is resumed at once).
 */
void syscall_trap_fault(CPUState *cpu, uintptr_t ra);

#endif /* SYSTEM_SYSCALL_TRAP_H */
