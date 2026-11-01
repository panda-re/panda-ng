/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PANDA_GUEST_WRITE_H
#define PANDA_GUEST_WRITE_H
/*
 * Guest writes from analysis code: the one API for changing guest state, so that
 * a recording logs every change and its replay repeats it
 * (docs/design/rr-guest-mutation-api.md).
 *
 * Without record/replay each call writes at once. While recording, a call is
 * admitted only inside a mutating callback at a site that logs its writes (a
 * guest hypercall, for now): the write is made and logged in order, and replay
 * repeats it at the same instruction without running the callback. Anywhere
 * else during record/replay a call is refused with GUEST_WRITE_EPERM and
 * changes nothing; it is never dropped silently.
 *
 * Changing guest state any other way (a store through cpu_env(), a raw pointer
 * into guest RAM, a target helper that rewrites env) is unsupported: nothing
 * logs it, so a replay of that recording diverges. Every write can go through
 * these calls, with or without record/replay; without it they cost a check and
 * a copy.
 *
 * Env writes copy bytes into the vCPU's CPUArchState at an offset
 * (offsetof(CPUArchState, ...)) or through pointer fields of it, by path. The
 * engine's layout map (panda/src/guest-layout.gen.h) checks every path step and
 * refuses a range that overlaps a host pointer: a host address means nothing
 * in another process. Replay needs a recording made with the same layout.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GuestWriteResult {
    GUEST_WRITE_OK = 0,
    GUEST_WRITE_EPERM,    /* no logging scope: observer, replay, wrong thread or site */
    GUEST_WRITE_EINVAL,   /* bad vCPU, register, length or path */
    GUEST_WRITE_EFAULT,   /* the address doesn't translate or isn't RAM; NULL on a path */
    GUEST_WRITE_EACCES,   /* the target refused the register write */
    GUEST_WRITE_E2BIG,    /* past the per-site budget (REPLAY_GUEST_WRITE_BYTES_MAX) */
} GuestWriteResult;

#define GUEST_WRITE_PATH_MAX 4   /* pointers one env path may follow */

/* Guest memory through the vCPU's page tables, and guest physical RAM. */
GuestWriteResult panda_guest_write_virt(struct CPUState *cpu, uint64_t addr,
                                        const void *buf, size_t len);
GuestWriteResult panda_guest_write_phys(uint64_t addr, const void *buf, size_t len);
/* A register by gdbstub number (panda_guest_reg_lookup), through the target's
 * setter; len must be the register's size. */
GuestWriteResult panda_guest_write_reg(struct CPUState *cpu, int regnum,
                                       const void *buf, size_t len);
/* The return register of a hypercall or syscall (panda_set_retval's). */
GuestWriteResult panda_guest_set_retval(struct CPUState *cpu, uint64_t value);
/* Bytes at offsetof(CPUArchState, ...) of the vCPU's env. */
GuestWriteResult panda_guest_write_env(struct CPUState *cpu, size_t offset,
                                       const void *buf, size_t len);
/*
 * env->field = value through the API, for code that sees CPUArchState:
 * PANDA_GUEST_ENV_SET(cpu, regs[R_EAX], 0) or (cpu, cp15.c1_sys, x). value is
 * converted to the field's type. Evaluates to the GuestWriteResult.
 */
#define PANDA_GUEST_ENV_SET(cpu, field, value) __extension__ ({                \
    __typeof__(((CPUArchState *)0)->field) panda_guest_env_value_ = (value);   \
    panda_guest_write_env((cpu), offsetof(CPUArchState, field),                \
                          &panda_guest_env_value_,                             \
                          sizeof(panda_guest_env_value_)); })
/* Through pointers: from env, for each derefs[i] follow the pointer field at
 * that offset of the struct reached so far, then write len bytes at offset of
 * the last pointee. n == 0 is panda_guest_write_env. n <= GUEST_WRITE_PATH_MAX. */
GuestWriteResult panda_guest_write_env_path(struct CPUState *cpu, const uint32_t *derefs,
                                            size_t n, size_t offset,
                                            const void *buf, size_t len);
/* Recompute what the target derives from env after raw writes (arm hflags) and
 * flush the vCPU's TLB. Logged like a write. */
GuestWriteResult panda_guest_env_sync(struct CPUState *cpu);
/* gdbstub register number of a register named as in the target's gdb XML
 * ("x0", "rip", "xmm3"); its size in *len. -1 if there is none. */
int panda_guest_reg_lookup(struct CPUState *cpu, const char *name, size_t *len);
/* Name of the result, for messages. */
const char *panda_guest_write_strerror(GuestWriteResult result);

#ifdef __cplusplus
}
#endif
#endif
