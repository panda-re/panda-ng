/*
 * Syscall events at the trap (system mode, TCG)
 *
 * The target calls syscall_trap_enter() when a user task traps for a
 * syscall, and syscall_trap_return() on every return to user mode while a
 * call is pending.  The core filters the traps by ABI and number, hands
 * them to its consumers in order (the PANDA syscall events, Penguin's
 * syscall hooks, the TCG plugin API), and pairs each call with its return.
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

/*
 * One call as a consumer sees it.  The call's own fields are shared: a
 * consumer sees what the consumers before it changed.  want_return, keep,
 * resumed, injected, inject_ret, task and data are each consumer's own,
 * kept for it with a pending or held call.
 */
typedef struct SyscallTrap {
    int abi;
    int64_t num;
    uint64_t args[6];
    uint64_t pc;            /* the address past the trap instruction */
    uint64_t id;            /* the core's: unique per call, the same at its return */
    int64_t ret;            /* return: the value; enter: with skip */
    /*
     * The kernel's per-CPU base while the task is in user mode (x86: the
     * kernel GS base; AArch64: TPIDR_EL1), or, for kernels that find the
     * task from the stack (AArch32), the kernel stack pointer.
     */
    uint64_t percpu;
    bool skip;              /* enter: do not enter the kernel; return ret */
    bool want_return;       /* enter: report the call's return to this consumer */
    bool restart;           /* return: the kernel is restarting the call */
    bool keep;              /* return: not this consumer's; keep it pending for it */
    /*
     * Set by the core for the consumer that asked for a fault at this trap
     * (the holder): this is the same call coming back after the guest
     * handled it.
     */
    bool resumed;
    /*
     * Set by the consumer: make the guest take the fault a user access to
     * fault_addr would take, then deliver this event again (resumed) once
     * the task comes back to the same trap.  The call is held meanwhile,
     * with every consumer's task and data; the consumers after the holder
     * see the call only when it resumes.
     */
    bool fault;
    bool fault_write;
    uint64_t fault_addr;
    /*
     * Set by the consumer at the entry: run this call in the task first
     * (inject_num, of the event's ABI), then deliver this event again
     * (resumed, injected set, inject_ret its return value) once the task is
     * back at the trap with its registers as they were.  The call is held
     * meanwhile, as for a fault.  The injected call reaches no consumer:
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
    /*
     * The consumer's data on a call is released: the call is discarded
     * without the consumer's return, or the consumer is done with it and
     * left data set (may be NULL).  Never called with the core's lock held,
     * and once per consumer and call.
     */
    void (*drop)(SyscallTrap *sc, void *opaque);
    unsigned flags;         /* SYSCALL_TRAP_OPS_* */
} SyscallTrapOps;

/*
 * A skipped call (skip set at the entry, by this consumer or another one)
 * is also reported to this consumer's ret() if it wanted the return, right
 * after the entry: skip is set, ret is the value the task gets, and ret()
 * may change it (nothing else).  Without the flag a skipped call has no
 * return.
 */
#define SYSCALL_TRAP_OPS_SKIPPED_RETURN 1

/*
 * Consumers.  A call reaches the consumers in order: order class, then
 * priority (lower first), then the order they were added in.  A held call
 * resumes at its holder, then the consumers after it.
 */
enum {
    SYSCALL_TRAP_OBSERVE_ORIGINAL,  /* observers that see the call as the task made it */
    SYSCALL_TRAP_MUTATE,            /* consumers that may change, skip or hold it */
    SYSCALL_TRAP_OBSERVE,           /* observers that see the final values */
};

typedef struct SyscallTrapConsumer SyscallTrapConsumer;

/* NULL without ops or on a bad order class. */
SyscallTrapConsumer *syscall_trap_consumer_add(const SyscallTrapOps *ops, void *opaque,
                                               int order, int priority);
/*
 * Its data on every pending or held call is dropped, outside the lock (from
 * its own callback: the call in flight's, once the callback returns); a
 * call it holds for a fault resumes at the consumers after it.  Refused, without changes, while a call it holds
 * runs an injected call: the task needs its registers back through it.
 * When the last consumer goes, the core is reset as by
 * syscall_trap_cancel_all() and no watch is left: then the caller must have
 * stopped the vCPUs (main thread/BQL).
 */
bool syscall_trap_consumer_remove(SyscallTrapConsumer *c);
/* @num -1: all of @abi.  The core watches the OR of its consumers' watches. */
void syscall_trap_consumer_watch(SyscallTrapConsumer *c, int abi, int64_t num,
                                 unsigned flags);
void syscall_trap_consumer_unwatch_all(SyscallTrapConsumer *c);
unsigned syscall_trap_consumers(void);
void syscall_trap_stats(uint64_t *pending, uint64_t *full, uint64_t *stale);
uint64_t syscall_trap_faults(void);    /* faults raised for consumers */

/*
 * The task is gone: drop its calls, pending or held (those where a
 * consumer's SyscallTrap.task is @task, not 0), for every consumer with
 * data on them.  Returns how many.
 */
unsigned syscall_trap_forget_task(uint64_t task);

/* Stopped-vCPU restore/reset boundary: abandon pending/held analysis, without
 * restoring old registers. Drops run outside sc_lock, once per consumer with
 * data. Watches and consumers remain installed; a return whose entry predates
 * the snapshot is not rebuilt. */
unsigned syscall_trap_cancel_all(void);

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
/*
 * Return: a call injected at this call's return is back.  The target puts
 * this call's own registers back as they are at its return: its arguments
 * (SyscallTrapRewind args), its number where the ABI keeps it in a register
 * of its own (AArch64 x8, EABI r7), and *ret (SYSCALL_TRAP_RET, set too);
 * the PC stays.
 */
#define SYSCALL_TRAP_RESTORE 64

typedef struct SyscallTrapRewind {
    int abi;                /* the call's: one the target injects into */
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
 * The target's own word for a call (PPC: sc or scv; MIPS o32: where the
 * stack arguments are).  Set before syscall_trap_enter(), on the same
 * thread: the call keeps it while it is pending or held, and a
 * syscall_trap_return() that finds the call sets syscall_trap_arch() to it
 * (0 when it found none).  Each syscall_trap_enter() consumes the word set
 * before it.
 */
void syscall_trap_set_arch(uint64_t word);
uint64_t syscall_trap_arch(void);

/*
 * Raise the fault the consumer asked for, as a user-mode load or store at
 * the current PC would.  @ra: the helper's return address when the state
 * has to be restored from it (0 if the CPU state is exact).  Does not
 * return, unless the page became accessible meanwhile: then the target
 * runs the trap again (the held call is resumed at once).
 */
void syscall_trap_fault(CPUState *cpu, uintptr_t ra);

#endif /* SYSTEM_SYSCALL_TRAP_H */
