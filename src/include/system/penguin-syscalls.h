/*
 * Penguin syscall hooks at the trap (system mode, TCG)
 *
 * The hook model of igloo_driver's syscall hooks (syscalls_hc.h), served
 * from QEMU's syscall trap events (system/syscall-trap.c) instead of a
 * kernel patch and module: hooks are registered at run time by name, with
 * the same pid, comm, scope, argument and return-value filters, and each
 * matching hook gets one callback per event with a mutable event.
 *
 * Dispatch, as igloo's:
 * - on_all hooks first, then on_unknown hooks, then hooks on the name, each
 *   in registration order; the set is chosen before the first callback;
 * - entry: a hook's argument changes reach the call (and the later hooks);
 *   the first hook that sets skip_syscall ends the entry: the call does not
 *   run, its return value is the event's retval, and the return hooks still
 *   see it;
 * - return: retval chains from hook to hook and the last value is returned.
 *
 * A callback reads and writes guest memory the way igloo's portal lets
 * Penguin's coroutines do: it returns a command (read, read a string,
 * write) and is called again with the answer, until it says it is done.
 * The engine carries the commands out with the task's permissions.  When a
 * page is not mapped (not faulted in yet, swapped out, copy-on-write), the
 * task takes the fault a user access there would take: the call is held,
 * the guest pages the memory in and comes back to the same trap, and the
 * engine retries the command and answers it.  A callback therefore runs
 * once, however many faults its commands need; its state stays with the
 * consumer, keyed by the event's call id.  The string filters page their
 * memory in the same way, before any callback runs.  Faults are raised only
 * once penguin_syscall_set_guard() has hooked the kernel's signal path: a
 * fault the kernel refuses (no mapping there) must not kill the task.  The
 * answer is then a failure, as the portal's.
 *
 * Differences from igloo:
 * - the filters apply to on_all hooks too;
 * - a restarted call's return event has restart set and retval -ERESTARTSYS;
 * - exit, exit_group, rt_sigreturn and sigreturn have no return event, nor
 *   has a successful execve;
 * - regs is 0: the registers are the CPU's (the user's), live at both events;
 * - a return hook's filters other than the return value's are also checked
 *   at the entry: a call is followed to its return only if one can match.
 *
 * Every function may be called from any thread.  Callbacks run on the vCPU
 * thread, in user mode, without the BQL.  The C ABI below is for Penguin's
 * CFFI layer: plain types, literal array sizes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef SYSTEM_PENGUIN_SYSCALLS_H
#define SYSTEM_PENGUIN_SYSCALLS_H

#include "qemu/typedefs.h"

/* igloo's enum value_filter_type */
enum penguin_sc_filter_type {
    PENGUIN_SC_FILTER_EXACT = 0,
    PENGUIN_SC_FILTER_GREATER,
    PENGUIN_SC_FILTER_GREATER_EQUAL,
    PENGUIN_SC_FILTER_LESS,
    PENGUIN_SC_FILTER_LESS_EQUAL,
    PENGUIN_SC_FILTER_NOT_EQUAL,
    PENGUIN_SC_FILTER_RANGE,            /* [min_value, max_value] */
    PENGUIN_SC_FILTER_SUCCESS,          /* >= 0 */
    PENGUIN_SC_FILTER_ERROR,            /* < 0 */
    PENGUIN_SC_FILTER_BITMASK_SET,      /* all of bitmask set */
    PENGUIN_SC_FILTER_BITMASK_CLEAR,    /* all of bitmask clear */
    PENGUIN_SC_FILTER_STR_EXACT,
    PENGUIN_SC_FILTER_STR_CONTAINS,
    PENGUIN_SC_FILTER_STR_STARTSWITH,
    PENGUIN_SC_FILTER_STR_ENDSWITH,
};

/* what a filter looks at */
enum penguin_sc_subject {
    PENGUIN_SC_SUBJECT_VALUE = 0,       /* the value, or the string it points to */
    PENGUIN_SC_SUBJECT_FD_PATH,         /* the name of the task's file at fd value */
};

/*
 * Values compare as signed; for a 32-bit ABI as the 32-bit kernel's long
 * (the low 32 bits, sign-extended), arguments and filter values alike.
 * String filters read the user string the argument points to (at most
 * 4096 bytes); with PENGUIN_SC_SUBJECT_FD_PATH, the d_path() name of the
 * file the argument is the fd of (penguin-osi.h; no fd there: no match).
 */
struct penguin_sc_filter {
    bool enabled;
    int32_t type;                       /* enum penguin_sc_filter_type */
    int64_t value;
    int64_t min_value;
    int64_t max_value;
    uint64_t bitmask;
    const char *pattern;                /* copied at registration */
    uint32_t pattern_len;               /* without a NUL */
    int32_t subject;                    /* enum penguin_sc_subject */
};

struct penguin_sc_hook {
    bool enabled;
    bool on_enter;
    bool on_return;
    bool on_all;                        /* every syscall */
    bool on_unknown;                    /* every syscall without a name */
    char name[48];                      /* normalized as igloo does */
    bool pid_filter_enabled;
    int64_t filter_pid;                 /* task pid (thread id) */
    bool comm_filter_enabled;
    char comm_filter[16];
    bool scope_filter_enabled;          /* see penguin_syscall_set_scope() */
    struct penguin_sc_filter arg_filters[6];
    struct penguin_sc_filter retval_filter;
    /*
     * A string filter on the task's executable: the d_path() name of its
     * mm->exe_file (penguin-osi.h).  Checked after the argument filters.
     */
    struct penguin_sc_filter exe_filter;
};

/* igloo's struct syscall_event, and the trap's facts */
struct penguin_sc_event {
    uint64_t call;                      /* this callback run, unique */
    uint64_t args[6];
    uint64_t pc;                        /* the user address past the trap */
    uint64_t hook;                      /* penguin_syscall_register()'s id */
    uint64_t task;                      /* the task_struct, or 0 */
    uint64_t regs;                      /* 0: the registers are the CPU's */
    int64_t retval;
    uint64_t pid;
    uint64_t create_time;               /* task->start_time */
    uint32_t argc;
    bool skip_syscall;
    char syscall_name[48];              /* normalized; "" if unknown */
    int32_t abi;                        /* enum qemu_plugin_syscall_abi */
    int64_t num;
    bool is_enter;
    bool restart;
};

/* a callback's command; the HYPER_OP_* of igloo's portal it stands for */
enum penguin_sc_op {
    PENGUIN_SC_OP_NONE,     /* to the callback: a new run; from it: done */
    PENGUIN_SC_OP_READ,     /* READ: size bytes at addr */
    PENGUIN_SC_OP_READ_STR, /* READ_STR: the string at addr, < size bytes */
    PENGUIN_SC_OP_WRITE,    /* WRITE: size bytes from data, at addr */
    PENGUIN_SC_OP_ABORT,    /* to the callback: its call is gone; drop it */
    PENGUIN_SC_OP_SYSCALL,  /* run syscall num(args) in the task: see below */
};

/* the answer: igloo's HYPER_RESP_READ_OK/_PARTIAL/_FAIL, WRITE_OK/_FAIL */
enum penguin_sc_status {
    PENGUIN_SC_OK,
    PENGUIN_SC_PARTIAL,     /* READ: the first len bytes only */
    PENGUIN_SC_FAIL,
};

#define PENGUIN_SC_IO_MAX (1 << 20)     /* READ and WRITE */
#define PENGUIN_SC_STR_MAX 4096         /* READ_STR */

/*
 * SYSCALL: the task makes system call num, of the event's ABI, with args,
 * before its own call; the answer is its return value, ret (status OK).
 * The engine puts the arguments in the registers, lets the task enter the
 * kernel, catches the call's return to user mode, and puts the task back
 * at its own call's trap with its own registers; the run is resumed there.
 * So the call runs as the task's: its credentials, namespaces, files,
 * signals.  Pointer arguments point into the task's memory: the callback
 * gets some first (an injected mmap) and fills it with WRITE.  It is a
 * kernel call like any other: a signal can interrupt it (-EINTR, or the
 * kernel restarts it), a handler can run before it comes back (seeing the
 * injected call's registers in its frame), the task can block in it, and
 * the task's other threads see what it does (a new fd, a mapping) while it
 * lasts.  Hooks see neither the injected call nor its restarts.
 * Status FAIL, nothing run: not at an entry event, not a call of an ABI
 * the target can put the registers back for (x86_64, AArch64, and an
 * AArch32 EABI task on an AArch64 kernel; num is then the EABI number, the
 * arguments 32 bits), or a call that would not come back to the trap
 * (exit, exit_group, the sigreturns, and fork, vfork, clone, clone3, whose
 * child would run on in the task's code; execve and execveat without
 * penguin_syscall_set_exec()).
 * execve, execveat with the exec hook: if the exec fails, the answer is
 * its error; if it succeeds, there is no answer: the task runs the new
 * image, its own call is gone, and the run is aborted (ABORT).
 */
struct penguin_sc_cmd {
    /* from the callback */
    int32_t op;
    uint64_t addr;
    uint64_t size;
    const void *data;                   /* WRITE: copied at once */
    /* to the callback, with op the command answered */
    int32_t status;
    const void *buf;                    /* READ, READ_STR: valid until it returns */
    uint64_t len;                       /* READ_STR: without a NUL */
    /* SYSCALL */
    int64_t num;
    uint64_t args[6];
    int64_t ret;                        /* to the callback */
};

/*
 * Where the kernel keeps what the filters and events need.  The current
 * task, at the user boundary: *(ptr_size *)((percpu & cur_mask) + cur_offset)
 * where percpu is x86's kernel GS base, AArch64's TPIDR_EL1, or an AArch32
 * kernel's stack pointer (cur_mask: ~(THREAD_SIZE - 1), cur_offset:
 * thread_info.task).  Offsets of -1 are unknown; cur_mask 0: no current.
 */
struct penguin_sc_layout {
    uint64_t cur_mask;
    uint64_t cur_offset;
    int32_t ptr_size;
    int32_t task_pid;
    int32_t task_comm;
    int32_t task_start_time;
    int32_t task_nsproxy;
    int32_t nsproxy_uts_ns;
    uint64_t init_uts_ns;               /* the address of init_uts_ns */
};

struct penguin_sc_stats {
    uint64_t enters;                    /* traps with hooks on the number */
    uint64_t events;                    /* callback runs */
    uint64_t commands;                  /* commands carried out */
    uint64_t faults;                    /* faults raised */
    uint64_t refused;                   /* faults the guest refused */
    uint64_t kept;                      /* returns that were not the caller's */
    uint64_t pending;                   /* calls waiting for their return */
    uint64_t full;                      /* calls not kept: the table was full */
    uint64_t stale;                     /* calls dropped without their return */
    uint64_t reaped;                    /* calls dropped with their task, or at its exec */
    uint64_t injected;                  /* SYSCALL commands carried out */
};

/*
 * @cmd->op on the way in: PENGUIN_SC_OP_NONE for a new run of the hook on
 * @ev; a command, with the answer; or PENGUIN_SC_OP_ABORT (@cs NULL): the
 * run will not continue.  On the way out: the next command, or
 * PENGUIN_SC_OP_NONE when done (then @ev's changes count).  @ev stays at
 * the same address until the run is done or aborted; @cmd's buf only until
 * the callback returns.
 */
typedef void (*penguin_syscall_cb_t)(CPUState *cs,
                                     struct penguin_sc_event *ev,
                                     struct penguin_sc_cmd *cmd,
                                     void *opaque);
typedef void (*penguin_syscall_boot_cb_t)(CPUState *cs, void *opaque);

/*
 * Take the syscall trap events (false if a TCG plugin has them) and call
 * @cb for every event.  TCG only: under KVM no event is ever delivered.
 */
bool set_penguin_syscall_callback(penguin_syscall_cb_t cb, void *opaque);

/*
 * Called once, at the first syscall trap, before any hook runs: where the
 * names can be filled from the guest (syscall_metadata.syscall_nr is not
 * set until the kernel's initcalls have run).
 */
void set_penguin_syscall_boot_callback(penguin_syscall_boot_cb_t cb,
                                       void *opaque);

/* the name and argument count of a syscall; name NULL forgets it */
bool penguin_syscall_set_name(int32_t abi, int64_t num, const char *name,
                              int32_t nargs);
bool penguin_syscall_set_layout(const struct penguin_sc_layout *layout);

/*
 * Let the engine raise faults: the kernel's force_sig_fault(sig, code,
 * addr), or before 4.20 force_sig_info(sig, info, task) (either may be 0),
 * is hooked so that the signal for a fault the guest refuses (no mapping
 * there) is dropped, as pagein's guard does.  x86_64 and AArch64 kernels (not yet
 * 32-bit ARM: refused there).
 */
bool penguin_syscall_set_guard(uint64_t force_sig_fault,
                               uint64_t force_sig_info);

/*
 * Drop a task's calls when it goes: the kernel's release_task(p) is hooked.
 * Without it, the call of a task killed while waiting for its return or
 * for a fault stays in the table until the table evicts it.  x86_64 and
 * AArch64 kernels.
 */
bool penguin_syscall_set_reaper(uint64_t release_task);

/*
 * Drop a task's calls when it execs: the kernel's __set_task_comm(task,
 * name, exec) is hooked, for exec true.  The calls cannot come back to the
 * image the task left; counted as reaped.  Lets SYSCALL run execve and
 * execveat.  x86_64 and AArch64 kernels (not yet 32-bit ARM).
 */
bool penguin_syscall_set_exec(uint64_t set_task_comm);

/* scope filters on: only tasks outside the initial UTS namespace match */
void penguin_syscall_set_scope(bool enabled);

/*
 * Where an event's task comes from when the layout has no per-CPU slot
 * (cur_mask 0, no __entry_task or current_task symbol): with @from_cpu,
 * the target's CPUState.syscall_current (AArch64: SP_EL0 as the kernel
 * last set it, Linux 4.10+).  Off by default.
 */
void penguin_syscall_set_current_source(bool from_cpu);

/* returns the hook's id, or 0 */
uint64_t penguin_syscall_register(const struct penguin_sc_hook *hook);
bool penguin_syscall_unregister(uint64_t id);

/*
 * Guest memory as the vCPU sees it, never faulting: fast in a callback,
 * and false where a command would have to fault.  A write needs the
 * guest's permission, user mode's or the kernel's: it never reaches a
 * read-only or copy-on-write page (a debug write would, into the page
 * cache or the zero page); all or nothing.
 */
bool penguin_syscall_read_memory(CPUState *cs, uint64_t addr, void *buf,
                                 size_t len);
bool penguin_syscall_write_memory(CPUState *cs, uint64_t addr,
                                  const void *buf, size_t len);

/*
 * gdb registers @first..@first + @n - 1 of @cs (the gdbstub's numbering, at
 * most 8 bytes each) into @out, as integers: in a callback, the task's
 * registers at the trap.  Returns how many were read.
 */
int penguin_syscall_regs(CPUState *cs, int first, int n, uint64_t *out);

void penguin_syscall_stats(struct penguin_sc_stats *stats);

#endif /* SYSTEM_PENGUIN_SYSCALLS_H */
