/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PANDA_PAGEIN_H
#define PANDA_PAGEIN_H
/*
 * Page-in requests: read or write guest virtual memory of a process whose pages may not be
 * present (never touched, swapped out, a file page not in the page cache), from any callback or
 * thread, without ever making the guest take a fault it would not have taken itself.
 *
 * A request names an address space (panda_current_asid()), a range, and a completion. The
 * engine serves it at the next user-mode instruction boundary of that address space, i.e. the
 * next time any of the process's threads runs in user mode: it has that thread make
 * madvise(addr, len, MADV_POPULATE_READ) (MADV_POPULATE_WRITE for a write), which the guest
 * kernel runs like any other system call, under its own locks, and which fails with an error
 * instead of a signal where the range isn't mapped (no mapping, PROT_NONE, past the end of a
 * file, a kernel address). The thread's registers are put back when it comes back to user mode
 * with the same stack pointer, the range is read or written without faulting, and the completion
 * runs, on that vCPU's thread. The code that made the request never runs again because of it.
 *
 * Guest-visible effects: the system call itself (seccomp, ptrace, audit and syscall tracers see
 * a madvise the program did not make), and those of a page-in: resident memory, fault counters,
 * accessed/dirty bits, page cache and swap-in I/O (the task may sleep, and other tasks run), and
 * for writes copy-on-write breaks and dirty pages. Linux 5.14 or later (MADV_POPULATE_*); an older
 * kernel completes requests with PANDA_PAGEIN_EGUEST and EINVAL.
 *
 * Refused, at submission, with their own status: during record/replay, under KVM, on targets
 * without a system-call injector (x86_64, aarch64, arm, riscv64, mips, mipsel, mips64, mips64el,
 * ppc, ppc64 and loongarch64 have one; i386 doesn't), and for tasks of another ABI (a 32-bit task
 * on an x86_64 or ppc64 kernel, MIPS16).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PandaPageinStatus {
    PANDA_PAGEIN_OK = 0,
    PANDA_PAGEIN_QUEUED = 1,      /* submit: accepted; the completion runs later */
    PANDA_PAGEIN_EGUEST = -1,     /* the guest kernel refused the range: guest_errno says why */
    PANDA_PAGEIN_ETIMEDOUT = -2,  /* no thread of the process reached user mode in time (it
                                     exited, is blocked, or is not scheduled) */
    PANDA_PAGEIN_ECANCELED = -3,  /* a snapshot load, reset or shutdown came first */
    PANDA_PAGEIN_ERECORD = -4,    /* refused: record/replay is active */
    PANDA_PAGEIN_EACCEL = -5,     /* refused: the accelerator has no safe point (KVM) */
    PANDA_PAGEIN_EARCH = -6,      /* refused: no system-call injector for this target or task */
    PANDA_PAGEIN_EINVAL = -7,     /* refused: empty or too long a range, no completion */
    PANDA_PAGEIN_EBUSY = -8,      /* refused: too many requests outstanding */
    PANDA_PAGEIN_ERECLAIMED = -9, /* the guest kept reclaiming the range before it was read */
    PANDA_PAGEIN_EIO = -10,       /* the range is not RAM (a device mapping) */
} PandaPageinStatus;

#define PANDA_PAGEIN_MAX_LEN (1u << 20)
#define PANDA_PAGEIN_MAX_OUTSTANDING 256

/*
 * @status: PANDA_PAGEIN_OK or an error. @guest_errno: with PANDA_PAGEIN_EGUEST, the errno of
 * the guest's madvise (ENOMEM: not mapped; EFAULT, EINVAL, ...). @data/@len: for a read, the bytes
 * (valid during the call); for a write, the bytes written. Runs on a vCPU thread: at the target
 * process's user-mode boundary when @status is OK or EGUEST (its other memory can be read there),
 * anywhere for the others.
 */
typedef void (*panda_pagein_cb_t)(uint64_t id, int status, int guest_errno,
                                  const uint8_t *data, size_t len, void *opaque);

/*
 * Queue a read (@wdata NULL) or a write of @len bytes at @addr in address space @asid.
 * @timeout_ns: guest (virtual clock) nanoseconds to wait; 0 for the default (5 s).
 * Returns PANDA_PAGEIN_QUEUED and sets *@id, or a refusal (the completion never runs).
 * Callable from any thread, any callback, and from the completion itself.
 */
int panda_pagein_submit(uint64_t asid, uint64_t addr, size_t len, const uint8_t *wdata,
                        uint64_t timeout_ns, panda_pagein_cb_t cb, void *opaque, uint64_t *id);

/*
 * Whether this thread may wait for a completion: not a vCPU thread (the guest has to run for the
 * request to be served) and not holding the BQL (the main loop: the guest's devices, its disk
 * I/O among them, would stop).
 */
bool panda_pagein_can_wait(void);

/* Status names for messages */
const char *panda_pagein_strerror(int status);

/* Counters since start: submitted, injected system calls, completed by status (-10..0). */
void panda_pagein_stats(uint64_t *submitted, uint64_t *injected, uint64_t *completed,
                        uint64_t by_status[11]);

#ifdef __cplusplus
}
#endif
#endif
