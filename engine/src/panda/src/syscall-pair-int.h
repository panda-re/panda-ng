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

#if defined(TARGET_I386) || defined(TARGET_ARM) || defined(TARGET_RISCV64) || \
    defined(TARGET_MIPS) || defined(TARGET_PPC)
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
    uint64_t rflags;                    /* SC_W_CHILD: a shared_child number; SC_R_HANDLER: a
                                         * return went elsewhere (a signal handler runs) */
    uint64_t usp;                       /* the user stack pointer at the entry */
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
    SC_R_HANDLER = 1u << 3,             /* ScRec.rflags only: a signal handler ran */
};

typedef struct ScAbi {
    PandaSyscallAbi abi;                /* the number sets copied */
    uint8_t *want;                      /* [nbits + 1], read by translated code */
    uint64_t mask;                      /* the form's register width */
    bool std;                           /* the target's standard layout (SC_TCG_FORM, SC_STD_*) */
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
/*
 * Per target:
 *   SC_FORMS             the forms it declares (1u << PANDA_SYSCALL_FORM_*)
 *   SC_TCG_FORM          the form whose trap translated code reaches with the target's standard
 *                        register layout (SC_STD_NR, SC_STD_RET, sc_std_args_read), if SC_HAS_STD
 *   SC_TASK_FORM         the task key form it supports (PANDA_TASK_KEY_REG or _WORD), and
 *   SC_TASK_REG          its PANDA_TASK_REG_*: REG's register (SC_TASK_REG_OFF, _BITS in env) or
 *                        WORD's base register (SC_WORD_BASE_OFF, _BITS)
 *   sc_reg               a register of a form's instruction set
 *   sc_dest_pc           where the vCPU is (a return's destination)
 *   sc_at_user           in user mode
 *   sc_root, sc_user_sp  the call key's parts
 *   sc_tcg_pc            the trap's address when translated code calls the entry function
 */
#if defined(TARGET_X86_64) || defined(TARGET_I386)
#define SC_STD_NR 0
#define SC_STD_RET 0
#define SC_TCG_FORM PANDA_SYSCALL_FORM_X86_SYSCALL
#define SC_TASK_FORM PANDA_TASK_KEY_WORD
#if defined(TARGET_X86_64)
#define SC_HAS_STD 1
#define SC_FORMS ((1u << PANDA_SYSCALL_FORM_X86_SYSCALL) | (1u << PANDA_SYSCALL_FORM_X86_INT80) | \
                  (1u << PANDA_SYSCALL_FORM_X86_SYSENTER) | \
                  (1u << PANDA_SYSCALL_FORM_X86_SYSCALL32))
#define SC_TASK_REG PANDA_TASK_REG_X86_KERNEL_GS_BASE
#define SC_WORD_BASE_OFF offsetof(CPUX86State, kernelgsbase)
#define SC_WORD_BASE_BITS 64
#else
#define SC_HAS_STD 0
#define SC_FORMS ((1u << PANDA_SYSCALL_FORM_X86_INT80) | (1u << PANDA_SYSCALL_FORM_X86_SYSENTER))
#define SC_TASK_REG PANDA_TASK_REG_X86_TR_BASE
#define SC_WORD_BASE_OFF offsetof(CPUX86State, tr.base)
#define SC_WORD_BASE_BITS TARGET_LONG_BITS
#endif

static inline uint64_t sc_reg(CPUArchState *env, unsigned form, unsigned r)
{
    return env->regs[r & (CPU_NB_REGS - 1)];
}

static inline void sc_std_args_read(CPUArchState *env, uint64_t *a)
{
#if defined(TARGET_X86_64)
    a[0] = env->regs[R_EDI];
    a[1] = env->regs[R_ESI];
    a[2] = env->regs[R_EDX];
    a[3] = env->regs[10];
    a[4] = env->regs[8];
    a[5] = env->regs[9];
#endif
}

static inline uint64_t sc_word_base(CPUArchState *env)
{
#if defined(TARGET_X86_64)
    return env->kernelgsbase;
#else
    return env->tr.base;
#endif
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

#elif defined(TARGET_AARCH64)
#define SC_STD_NR 8
#define SC_STD_RET 0
#define SC_HAS_STD 1
#define SC_TCG_FORM PANDA_SYSCALL_FORM_A64_SVC
#define SC_FORMS ((1u << PANDA_SYSCALL_FORM_A64_SVC) | (1u << PANDA_SYSCALL_FORM_A32_SVC))
#define SC_TASK_FORM PANDA_TASK_KEY_REG
#define SC_TASK_REG PANDA_TASK_REG_ARM_SP_EL1
#define SC_TASK_REG_OFF offsetof(CPUARMState, sp_el[1])
#define SC_TASK_REG_BITS 64

static inline uint64_t sc_reg(CPUArchState *env, unsigned form, unsigned r)
{
    return form == PANDA_SYSCALL_FORM_A32_SVC ? env->regs[r & 15] : env->xregs[r & 31];
}

static inline void sc_std_args_read(CPUArchState *env, uint64_t *a)
{
    memcpy(a, env->xregs, 6 * sizeof(uint64_t));
}

static inline uint64_t sc_task_reg(CPUArchState *env)
{
    return env->sp_el[1];
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

#elif defined(TARGET_ARM)
/* An AArch32 kernel: SVC is taken in C (no translated entry) */
#define SC_STD_NR 7
#define SC_STD_RET 0
#define SC_HAS_STD 0
#define SC_TCG_FORM PANDA_SYSCALL_FORM_A32_SVC
#define SC_FORMS (1u << PANDA_SYSCALL_FORM_A32_SVC)
#define SC_TASK_FORM PANDA_TASK_KEY_REG
#define SC_TASK_REG PANDA_TASK_REG_ARM_SP_SVC
#define SC_TASK_REG_OFF offsetof(CPUARMState, banked_r13[1])     /* bank_number(SVC) */
#define SC_TASK_REG_BITS 32

static inline uint64_t sc_reg(CPUArchState *env, unsigned form, unsigned r)
{
    return env->regs[r & 15];
}

static inline void sc_std_args_read(CPUArchState *env, uint64_t *a) {}

static inline uint64_t sc_task_reg(CPUArchState *env)
{
    return env->banked_r13[1];
}

static inline uint64_t sc_dest_pc(CPUArchState *env)
{
    return env->regs[15];
}

static inline bool sc_at_user(CPUArchState *env)
{
    return (env->uncached_cpsr & CPSR_M) == ARM_CPU_MODE_USR;
}

static inline uint64_t sc_root(CPUArchState *env)
{
    return env->cp15.ttbr0_el[1] & 0x000000ffffffff80ull;   /* attributes and ASID dropped */
}

static inline uint64_t sc_user_sp(CPUArchState *env)
{
    return env->regs[13];
}

static inline uint64_t sc_tcg_pc(CPUArchState *env, uint32_t len)
{
    return env->regs[15] - len;
}

#elif defined(TARGET_RISCV64)
#define SC_STD_NR 17
#define SC_STD_RET 10
#define SC_HAS_STD 1
#define SC_TCG_FORM PANDA_SYSCALL_FORM_RISCV_ECALL
#define SC_FORMS (1u << PANDA_SYSCALL_FORM_RISCV_ECALL)
#define SC_TASK_FORM PANDA_TASK_KEY_REG
#define SC_TASK_REG PANDA_TASK_REG_RISCV_SSCRATCH
#define SC_TASK_REG_OFF offsetof(CPURISCVState, sscratch)
#define SC_TASK_REG_BITS 64

static inline uint64_t sc_reg(CPUArchState *env, unsigned form, unsigned r)
{
    return env->gpr[r & 31];
}

static inline void sc_std_args_read(CPUArchState *env, uint64_t *a)
{
    memcpy(a, &env->gpr[10], 6 * sizeof(uint64_t));
}

static inline uint64_t sc_task_reg(CPUArchState *env)
{
    return env->sscratch;
}

static inline uint64_t sc_dest_pc(CPUArchState *env)
{
    return env->pc;
}

static inline bool sc_at_user(CPUArchState *env)
{
    return env->priv == PRV_U && !env->virt_enabled;
}

static inline uint64_t sc_root(CPUArchState *env)
{
    return env->satp & ((1ull << 44) - 1);                  /* the root table's PPN */
}

static inline uint64_t sc_user_sp(CPUArchState *env)
{
    return env->gpr[2];
}

/* The trap site stores the ECALL's address in the PC first */
static inline uint64_t sc_tcg_pc(CPUArchState *env, uint32_t len)
{
    return env->pc;
}

#elif defined(TARGET_MIPS)
#define SC_STD_NR 2
#define SC_STD_RET 2
#define SC_TCG_FORM PANDA_SYSCALL_FORM_MIPS_SYSCALL
#define SC_FORMS (1u << PANDA_SYSCALL_FORM_MIPS_SYSCALL)
#define SC_TASK_FORM PANDA_TASK_KEY_WORD
#define SC_TASK_REG PANDA_TASK_REG_MIPS_EBASE
#define SC_WORD_BASE_OFF offsetof(CPUMIPSState, CP0_EBase)
#define SC_WORD_BASE_BITS TARGET_LONG_BITS
#if defined(TARGET_MIPS64)
#define SC_HAS_STD 1                    /* n64: $a0-$a5 */
#else
#define SC_HAS_STD 0                    /* o32: arguments 5 and 6 on the stack */
#endif

static inline uint64_t sc_reg(CPUArchState *env, unsigned form, unsigned r)
{
    return env->active_tc.gpr[r & 31];
}

static inline void sc_std_args_read(CPUArchState *env, uint64_t *a)
{
    for (unsigned k = 0; k < 6; k++) {
        a[k] = env->active_tc.gpr[4 + k];
    }
}

static inline uint64_t sc_word_base(CPUArchState *env)
{
    return env->CP0_EBase;
}

static inline uint64_t sc_dest_pc(CPUArchState *env)
{
    return env->active_tc.PC;
}

static inline bool sc_at_user(CPUArchState *env)
{
    return (env->hflags & MIPS_HFLAG_KSU) == MIPS_HFLAG_UM;
}

static inline uint64_t sc_root(CPUArchState *env)
{
    return env->CP0_EntryHi & env->CP0_EntryHi_ASID_mask;  /* the ASID */
}

static inline uint64_t sc_user_sp(CPUArchState *env)
{
    return env->active_tc.gpr[29];
}

/* The trap site saves the SYSCALL's address in the PC first */
static inline uint64_t sc_tcg_pc(CPUArchState *env, uint32_t len)
{
    return env->active_tc.PC;
}

#elif defined(TARGET_PPC)
#define SC_STD_NR 0
#define SC_STD_RET 3
#define SC_HAS_STD 1
#define SC_TCG_FORM PANDA_SYSCALL_FORM_PPC_SC
#if defined(TARGET_PPC64)
#define SC_FORMS ((1u << PANDA_SYSCALL_FORM_PPC_SC) | (1u << PANDA_SYSCALL_FORM_PPC_SC32) | \
                  (1u << PANDA_SYSCALL_FORM_PPC_SCV))
#define SC_TASK_FORM PANDA_TASK_KEY_WORD
#define SC_TASK_REG PANDA_TASK_REG_PPC_SPRG1
#define SC_WORD_BASE_OFF offsetof(CPUPPCState, spr[SPR_SPRG1])
#define SC_WORD_BASE_BITS 64
#else
#define SC_FORMS (1u << PANDA_SYSCALL_FORM_PPC_SC)
#define SC_TASK_FORM PANDA_TASK_KEY_REG
#define SC_TASK_REG PANDA_TASK_REG_PPC_SPRG3
#define SC_TASK_REG_OFF offsetof(CPUPPCState, spr[SPR_SPRG3])
#define SC_TASK_REG_BITS 32
#endif

static inline uint64_t sc_reg(CPUArchState *env, unsigned form, unsigned r)
{
    return env->gpr[r & 31];
}

static inline void sc_std_args_read(CPUArchState *env, uint64_t *a)
{
    for (unsigned k = 0; k < 6; k++) {
        a[k] = env->gpr[3 + k];
    }
}

static inline uint64_t sc_task_reg(CPUArchState *env)
{
    return env->spr[SPR_SPRG3];
}

static inline uint64_t sc_word_base(CPUArchState *env)
{
    return env->spr[SPR_SPRG1];
}

static inline uint64_t sc_dest_pc(CPUArchState *env)
{
    return env->nip;
}

static inline bool sc_at_user(CPUArchState *env)
{
    return (env->msr >> MSR_PR) & 1;
}

static inline uint64_t sc_root(CPUArchState *env)
{
    return env->spr[SPR_BOOKE_PID];                         /* BookE PID, Book3S PIDR */
}

static inline uint64_t sc_user_sp(CPUArchState *env)
{
    return env->gpr[1];
}

/* The trap site stores the sc's address in NIP first */
static inline uint64_t sc_tcg_pc(CPUArchState *env, uint32_t len)
{
    return env->nip;
}
#endif

/* The task key form, for the preprocessor */
#if defined(SC_WORD_BASE_OFF)
#define SC_KEY_WORD 1
#elif defined(SC_TASK_REG_OFF)
#define SC_KEY_REG 1
#endif

#if !defined(TARGET_I386)
static inline bool sc_x86_faults(CPUArchState *env, unsigned form)
{
    return false;
}
#endif

/* The guest's data byte order (arguments read from memory) */
static inline bool sc_guest_be(CPUArchState *env)
{
#if defined(TARGET_PPC)
    return !(env->msr & 1);                     /* MSR[LE] */
#elif defined(TARGET_MIPS) && TARGET_BIG_ENDIAN
    return true;
#else
    return false;
#endif
}

/* WORD: the key word's width; read through its host mapping as it is in memory (a key is
 * compared, not interpreted) */
#if (defined(TARGET_I386) && !defined(TARGET_X86_64)) || \
    (defined(TARGET_MIPS) && !defined(TARGET_MIPS64))
#define SC_WORD_BITS 32
#else
#define SC_WORD_BITS 64
#endif

static inline uint64_t sc_word_read(const uint64_t *w)
{
#if SC_WORD_BITS == 32
    return *(const uint32_t *)w;
#else
    return *w;
#endif
}

/* A form's register width: 64, or 32 */
static inline bool sc_form_64(unsigned form)
{
    switch (form) {
    case PANDA_SYSCALL_FORM_X86_SYSCALL:
    case PANDA_SYSCALL_FORM_A64_SVC:
    case PANDA_SYSCALL_FORM_RISCV_ECALL:
    case PANDA_SYSCALL_FORM_PPC_SCV:
        return true;
    case PANDA_SYSCALL_FORM_MIPS_SYSCALL:
    case PANDA_SYSCALL_FORM_PPC_SC:
        return TARGET_LONG_BITS == 64;
    default:
        return false;
    }
}

static inline int64_t sc_nr(ScAbi *A, CPUArchState *env)
{
    uint64_t r = sc_reg(env, A->abi.form, A->abi.nr_reg);

    return A->abi.nr_width == 32 ? (int64_t)(int32_t)r : (int64_t)r;
}

/* An error reported by a flag (err_flag): the value is the positive error number */
static inline bool sc_err_flag(ScAbi *A, CPUArchState *env)
{
#if defined(TARGET_PPC)
    if (A->abi.err_flag == PANDA_SYSCALL_ERR_PPC_SO) {
        return env->crf[0] & 1;                 /* CR0.SO */
    }
#endif
    return (A->abi.err_flag & 0x80) && sc_reg(env, A->abi.form, A->abi.err_flag & 0x7f) != 0;
}

static inline int64_t sc_retval(ScAbi *A, CPUArchState *env)
{
    uint64_t r = sc_reg(env, A->abi.form, A->abi.ret_reg);
    int64_t v = A->mask == UINT32_MAX ? (int64_t)(int32_t)r : (int64_t)r;

    if (unlikely(A->abi.err_flag) && sc_err_flag(A, env)) {
        return -v;
    }
    return v;
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

/* An argument PANDA_SYSCALL_ARG_MEM(r): the word at register r + off in user memory, or 0 */
uint64_t sc_arg_mem(ScAbi *A, CPUArchState *env, unsigned r, unsigned off) G_GNUC_INTERNAL;

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
                         r & 0x80 ? sc_arg_mem(A, env, r & 0x7f, A->abi.arg_off[k]) :
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
