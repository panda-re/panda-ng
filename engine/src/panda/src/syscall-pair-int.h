/*
 * System calls paired in the engine (panda/syscall-pair.h): the state shared by
 * syscall-pair.c (declarations, wants, translated code, the general cases) and
 * syscall-pair-fast.c (the entry and return functions translated code calls, compiled with
 * their own code generation flags).
 *
 * Layout
 * ------
 * The pending set: 2^sets_log sets (2048) of SC_WAYS (4) records, a record per pending call,
 * the set chosen by a multiplicative hash of the task key. Lock-free:
 *   tag[w]  per record: 0 (free) or the key's tag (the hash above SC_TAG_LOW) with the record's
 *           depth (bits 1-6: LIFO order among one key's records) and BUSY (bit 0: being written,
 *           delivered or copied; nobody else may take it)
 *   cnt     per set: the records in use, exact (incremented after a claim, decremented after a
 *           pop); translated code reads it to skip the pairing function
 * A set's tags and count are one cache line, written only by the vCPUs running the tasks whose
 * keys hash there. A record is claimed by a CAS of its tag from 0 to tag|BUSY, written, and
 * published by clearing BUSY. Only the key's own task pops its records, and the guest orders a
 * task's entry before its return (on any vCPU). The races are with another key's entry evicting
 * a record of a full set, and with a child's return copying its caller's record: both take only
 * a record without BUSY, by CAS, and the owner sets BUSY by CAS before it reads a record it may
 * pop.
 *
 * An entry is delivered from the record itself (the consumer writes PandaSyscallCall.data
 * there), published before the delivery. The vCPU's `dlv` names the record it last kept, and
 * other vCPUs do not evict it, so that no evictor takes it while the consumer writes its data.
 * The pin stays until the vCPU's next kept entry: each vCPU pins at most one record.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_SYSCALL_PAIR_INT_H
#define PANDA_SYSCALL_PAIR_INT_H

#include "qemu/osdep.h"
#include "qemu/atomic.h"
#include "qemu/thread.h"
#include "panda/common.h"
#include "panda/syscall-pair.h"
#include "panda/syscall-pair-tcg.h"

#if defined(TARGET_X86_64) || defined(TARGET_AARCH64)
#define SC_TARGET 1
#endif

#ifdef SC_TARGET

#define SC_SETS_LOG 11
#define SC_WAYS 4
#define SC_VCPUS 256
#define SC_BUSY 1ull
#define SC_TAG_LOW 0x7full
#define SC_DEPTH_MAX 63
#define SC_HASH_K 0x9e3779b97f4a7c15ull
#define SC_WATCH 32
#define SC_DATA_MAX 1024
#define SC_KVAL_NONE 0x8000000000000000ull     /* no KERNEL_GS_BASE value is this */
#define SC_RETRY 4096                           /* slow returns between resolver retries */

/*
 * The counters of the common cases (kept, paired) cost a store on every call: counted only in a
 * build with PANDA_SYSCALL_PAIR_STATS defined. The rare cases are always counted.
 */
#ifdef PANDA_SYSCALL_PAIR_STATS
#define SC_COUNT(v, field) ((v)->st.field++)
#else
#define SC_COUNT(v, field) ((void)(v))
#endif

/* A set's tags and count: one cache line of its own */
typedef struct ScSet {
    uint64_t tag[SC_WAYS];
    uint8_t cnt;
} QEMU_ALIGNED(64) ScSet;
#define SC_SET_SHIFT 6
QEMU_BUILD_BUG_ON(sizeof(ScSet) != 1u << SC_SET_SHIFT);

typedef struct ScRec {
    uint64_t ka, kb;                    /* the key (kb: 0 for a task key; the user SP for a
                                         * call key) */
    uint64_t rflags;                    /* SC_W_CHILD: a shared_child number */
    PandaSyscallCall c;                 /* then the consumer's data */
} ScRec;

/* WORD key, per vCPU: the base register's value it was resolved for, and the word. kval starts
 * at SC_KVAL_NONE and kword at a zero word: translated code never reads through NULL. Read by
 * translated code (16 bytes, indexed by cpu_index). While the word cannot be resolved, kval
 * stays a value KERNEL_GS_BASE does not have, so that every return goes to the C function. */
typedef struct ScKey {
    uint64_t kval;
    const uint64_t *kword;
} ScKey;
QEMU_BUILD_BUG_ON(sizeof(ScKey) != 16);

typedef struct ScVcpu {
    uint64_t ids;                       /* entries filled (the calls' ids) */
    const void *dlv;                    /* the record this vCPU last kept (see above) */
    PandaSyscallCall *slot;             /* its delivery slot for entries not kept */
    uint64_t fail_base;                 /* WORD: the base the resolver last failed for */
    uint64_t retry;                     /* WORD: slow returns before the resolver runs again */
    PandaSyscallStats st;
} QEMU_ALIGNED(256) ScVcpu;             /* a power of two: indexed with a shift */

/* The want byte of a number: the consumer's PANDA_SYSCALL_WANT_*, and, while it is not 0, the
 * engine's own bits for the number */
enum {
    SC_W_ENTRY = PANDA_SYSCALL_WANT_ENTRY,
    SC_W_RET = PANDA_SYSCALL_WANT_RETURN,
    SC_W_CHILD = 1u << 2,               /* a shared_child number */
};

typedef struct ScAbi {
    PandaSyscallAbi abi;                /* the number sets copied */
    uint8_t *want;                      /* [nbits + 1], read by translated code */
    uint64_t mask;                      /* the form's register width */
    bool std;                           /* the target's 64-bit layout (x86_64, AArch64) */
} ScAbi;

typedef struct ScWatch {
    ScRec *r;                           /* a pending shared_child call */
    uint64_t id;
    uint64_t root;                      /* its address space */
    uint64_t retpc;
} ScWatch;

typedef struct ScState {
    void *owner;
    PandaSyscallFn fn;
    void *ctx;
    ScSet *sets;
    uint8_t *recs;
    unsigned sshift;                    /* a key's set: (hash >> sshift) & ~63 is its offset */
    size_t rmul;                        /* ... and that offset * rmul its records' */
    size_t stride;                      /* a record (ScRec + data), a multiple of 64 */
    uint8_t *slots;                     /* [SC_VCPUS] delivery slots of entries not kept */
    size_t slot_stride;                 /* a delivery slot (PandaSyscallCall + data) */
    ScKey *kc;                          /* [SC_VCPUS] */
    unsigned sets_log;
    uint32_t key_form;                  /* PANDA_TASK_KEY_* (CALL if none declared) */
    ScAbi *abi[PANDA_SYSCALL_FORM_COUNT];
    ScAbi *by_id[256];
    uint8_t *watch_n;                   /* the watches in use, read by translated code */
    uint8_t *cslots;                    /* [SC_VCPUS] delivery slots of child returns */
    uint32_t data_size;
    uint32_t key_reg;
    PandaTaskKeyResolveFn resolve;
    void *rctx;
    bool child;                         /* some ABI has a shared_child set */
    bool a32;                           /* A32_SVC declared: returns to AArch32 EL0 checked */
    unsigned nret;                      /* (ABI, number)s whose return is wanted */
    QemuMutex lock;                     /* wants, watches, declarations */
    ScWatch watch[SC_WATCH];
    ScVcpu v[SC_VCPUS];
} ScState;

extern ScState *sc G_GNUC_INTERNAL;
extern const uint64_t sc_zero_word G_GNUC_INTERNAL;

static inline uint64_t sc_mix(uint64_t ka, uint64_t kb)
{
    return (ka ^ kb) * SC_HASH_K;
}

/* The offset of a hash's set in sets[] (sshift is 58 - sets_log; sets_log may be 0) */
static inline size_t sc_set_off(ScState *P, uint64_t hm)
{
    return (hm >> P->sshift) & ~(uint64_t)((1u << SC_SET_SHIFT) - 1);
}

static inline unsigned sc_set(ScState *P, uint64_t hm)
{
    return sc_set_off(P, hm) >> SC_SET_SHIFT;
}

static inline uint64_t sc_tag(uint64_t hm)
{
    return (hm | (SC_TAG_LOW + 1)) & ~SC_TAG_LOW;
}

static inline ScRec *sc_rec(ScState *P, unsigned s, unsigned w)
{
    return (ScRec *)(P->recs + ((size_t)s * SC_WAYS + w) * P->stride);
}


static inline ScVcpu *sc_vcpu(ScState *P, CPUState *cpu)
{
    return &P->v[cpu->cpu_index & (SC_VCPUS - 1)];
}

static inline bool sc_bit(const uint8_t *set, uint64_t i)
{
    return set && ((set[i >> 3] >> (i & 7)) & 1);
}

static inline uint64_t sc_index(const PandaSyscallAbi *a, int64_t nr)
{
    return (uint64_t)nr < a->nbits ? (uint64_t)nr : a->nbits;
}

/* --- Architecture ------------------------------------------------------------------------ */

#if defined(TARGET_X86_64)
#define SC_STD_NR 0
#define SC_STD_RET 0
#define SC_TCG_FORM PANDA_SYSCALL_FORM_X86_SYSCALL

static inline uint64_t sc_reg(CPUArchState *env, unsigned form, unsigned r)
{
    return env->regs[r & 15];
}

static inline void sc_std_args_read(CPUArchState *env, uint64_t *a)
{
    a[0] = env->regs[R_EDI];
    a[1] = env->regs[R_ESI];
    a[2] = env->regs[R_EDX];
    a[3] = env->regs[10];
    a[4] = env->regs[8];
    a[5] = env->regs[9];
}

static inline uint64_t sc_dest_pc(CPUArchState *env)
{
    return env->segs[R_CS].base + env->eip;
}

static inline bool sc_at_user(CPUArchState *env)
{
    return (env->hflags & HF_CPL_MASK) == 3;
}

static inline uint64_t sc_root(CPUArchState *env)
{
    return env->cr[3] & 0x000ffffffffff000ull;
}

static inline uint64_t sc_user_sp(CPUArchState *env)
{
    return env->regs[R_ESP];
}

/* Where translated code's entry finds the trap: EIP at the SYSCALL or SYSENTER */
static inline uint64_t sc_tcg_pc(CPUArchState *env, uint32_t len)
{
    return env->segs[R_CS].base + env->eip;
}

/* The trap faults (#UD, #GP) instead of entering the kernel: no call */
static inline bool sc_x86_faults(CPUArchState *env, unsigned form)
{
    if (form == PANDA_SYSCALL_FORM_X86_SYSENTER) {
        return env->sysenter_cs == 0;
    }
    return !(env->efer & MSR_EFER_SCE);
}
#else
#define SC_STD_NR 8
#define SC_STD_RET 0
#define SC_TCG_FORM PANDA_SYSCALL_FORM_A64_SVC

static inline uint64_t sc_reg(CPUArchState *env, unsigned form, unsigned r)
{
    return form == PANDA_SYSCALL_FORM_A32_SVC ? env->regs[r & 15] : env->xregs[r & 31];
}

static inline void sc_std_args_read(CPUArchState *env, uint64_t *a)
{
    memcpy(a, env->xregs, 6 * sizeof(uint64_t));
}

static inline uint64_t sc_dest_pc(CPUArchState *env)
{
    return env->aarch64 ? env->pc : env->regs[15];
}

static inline bool sc_at_user(CPUArchState *env)
{
    if (env->aarch64) {
        return (env->pstate & PSTATE_M) == 0;          /* EL0t */
    }
    return (env->uncached_cpsr & CPSR_M) == ARM_CPU_MODE_USR;
}

static inline uint64_t sc_root(CPUArchState *env)
{
    return env->cp15.ttbr0_el[1] & 0x0000fffffffffffeull;  /* ASID and CnP dropped */
}

static inline uint64_t sc_user_sp(CPUArchState *env)
{
    return env->aarch64 ? env->xregs[31] : env->regs[13];
}

/* Where translated code's entry finds the trap: the PC is past the SVC */
static inline uint64_t sc_tcg_pc(CPUArchState *env, uint32_t len)
{
    return env->pc - len;
}
#endif

static inline int64_t sc_nr(ScAbi *A, CPUArchState *env)
{
    uint64_t r = sc_reg(env, A->abi.form, A->abi.nr_reg);

    return A->abi.nr_width == 32 ? (int64_t)(int32_t)r : (int64_t)r;
}

static inline int64_t sc_retval(ScAbi *A, CPUArchState *env)
{
    uint64_t r = sc_reg(env, A->abi.form, A->abi.ret_reg);

    return A->mask == UINT32_MAX ? (int64_t)(int32_t)r : (int64_t)r;
}

static inline uint32_t sc_ret_flags(ScAbi *A, CPUState *cpu, PandaSyscallCall *c, int64_t ret)
{
    uint32_t flags = PANDA_SYSCALL_RETURN;

    if (ret < 0 && ret >= A->abi.err_min) {
        flags |= PANDA_SYSCALL_ERROR;
    }
    if (c->vcpu != (uint32_t)cpu->cpu_index) {
        flags |= PANDA_SYSCALL_MOVED;
    }
    return flags;
}

/* --- The general cases (syscall-pair.c) ------------------------------------------------- */

/* An argument PANDA_SYSCALL_ARG_MEM(r): the word at register r in user memory, or 0 */
uint64_t sc_arg_mem(ScAbi *A, CPUArchState *env, unsigned r) G_GNUC_INTERNAL;

/* Fill the call from the registers at the trap */
static inline QEMU_ALWAYS_INLINE void sc_fill(const bool std, ScAbi *A, ScVcpu *v, int ci,
                                              CPUArchState *env, PandaSyscallCall *c,
                                              int64_t nr, uint64_t pc, uint32_t len)
{
    c->len = std ? len : len + A->abi.ret_skip;
    c->vcpu = ci;
    c->abi = A->abi.id;
    c->id = (++v->ids << 8) | (ci & 0xff);
    c->nr = nr;
    c->pc = pc;
    if (std) {
        sc_std_args_read(env, c->args);
    } else {
        unsigned form = A->abi.form;

        for (unsigned k = 0; k < 6; k++) {
            unsigned r = A->abi.arg_reg[k];

            c->args[k] = k >= A->abi.nargs ? 0 :
                         r & 0x80 ? sc_arg_mem(A, env, r & 0x7f) :
                         sc_reg(env, form, r) & A->mask;
        }
    }
}

/* A kept shared_child call's watch (its child's first return), added before it is published */
void sc_watch_add(ScState *P, ScRec *r, CPUArchState *env) G_GNUC_INTERNAL;

/* An entry the fast functions do not handle (any key, any set, a key to resolve again, a
 * shared_child number): @wl and @nr as the translated code's (the want byte, the trap's length
 * << 8, the form << 16; the number as the ABI takes it) */
void sc_enter_full(CPUArchState *env, uint32_t wl, int64_t nr) G_GNUC_INTERNAL;

/* A return the fast functions do not handle: the key's set holds other calls, or none and a
 * child watch is set */
void sc_return_full(CPUArchState *env, uint64_t ka, uint64_t kb) G_GNUC_INTERNAL;

/* A paired record (way @w of set @s, BUSY, tag @bt without BUSY) whose return is not the plain
 * one: a restart, a child's, an execve's end, a signal handler */
void sc_return_other(CPUArchState *env, unsigned s, unsigned w, uint64_t bt, ScRec *r)
    G_GNUC_INTERNAL;

/* --- The fast functions (syscall-pair-fast.c) -------------------------------------------- */

/* From translated code, by the task key's form (TASK: the declared REG or WORD key, else CALL)
 * and the ABI's layout (_s: the target's standard one) */
void sc_enter_h_task_s(CPUArchState *env, uint32_t wl, int64_t nr) G_GNUC_INTERNAL;
void sc_enter_h_task(CPUArchState *env, uint32_t wl, int64_t nr) G_GNUC_INTERNAL;
void sc_enter_h_call_s(CPUArchState *env, uint32_t wl, int64_t nr) G_GNUC_INTERNAL;
void sc_enter_h_call(CPUArchState *env, uint32_t wl, int64_t nr) G_GNUC_INTERNAL;
void sc_ret_h_task(CPUArchState *env, uint64_t ka) G_GNUC_INTERNAL;
void sc_ret_h_call(CPUArchState *env, uint64_t ka, uint64_t kb) G_GNUC_INTERNAL;

#endif /* SC_TARGET */

#endif /* PANDA_SYSCALL_PAIR_INT_H */
