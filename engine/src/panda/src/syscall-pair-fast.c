/*
 * System calls paired in the engine: the entry and return functions translated code calls
 * (panda/syscall-pair-tcg.h), one per task key form and register layout, chosen when the
 * trap or return site is translated. They handle the common cases (an entry delivered from the
 * vCPU's slot; a call kept in an empty set; a return that pairs its set's only call) and pass
 * everything else, before changing anything, to the general functions in syscall-pair.c.
 *
 * This file is built with its own code generation flags (panda_hot_ss): no call-used register
 * zeroing on return (-fzero-call-used-regs=skip) and no frame pointer, so that these functions
 * are leaves or end in a tail call (an entry's delivery is one). It takes no address of a local
 * (no stack protector canary).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "syscall-pair-int.h"

#ifdef SC_TARGET

/* An entry whose return is not wanted: written into the vCPU's slot, delivered */
static inline QEMU_ALWAYS_INLINE void sc_enter_slot(const bool std, CPUArchState *env,
                                                    uint32_t wl, int64_t nr)
{
    ScState *P = sc;
    ScAbi *A = std ? P->abi[SC_TCG_FORM] : P->abi[wl >> 16];
    CPUState *cpu = env_cpu(env);
    int ci = cpu->cpu_index;
    ScVcpu *v = &P->v[ci & (SC_VCPUS - 1)];
    PandaSyscallCall *c = v->slot;
    uint32_t len = (wl >> 8) & 0xff;

    sc_fill(std, A, v, ci, env, c, nr, sc_tcg_pc(env, len), len);
    P->fn(P->ctx, cpu, c, 0, 0);
}

/*
 * An entry whose return is wanted, of a task whose key is at hand (a WORD key whose word is
 * resolved for this KERNEL_GS_BASE, a REG key, or a call key) and whose set is empty: way 0,
 * claimed at once. The record is published before the delivery (dlv names it meanwhile).
 */
static inline QEMU_ALWAYS_INLINE void sc_keep_k(const bool task, const bool std,
                                                CPUArchState *env, uint32_t wl, int64_t nr)
{
    ScState *P = sc;
    CPUState *cpu = env_cpu(env);
    uint64_t ka, kb, hm, tag;
    PandaSyscallCall *c;
    uint32_t kt, len;
    size_t so;
    ScSet *S;
    ScRec *r;
    ScVcpu *v;
    ScAbi *A;
    int ci;

    if (unlikely((wl & (SC_W_RET | SC_W_CHILD)) != SC_W_RET)) {
        goto full;                      /* a shared_child number: its watch */
    }
    if (task) {
#ifdef SC_KEY_WORD
        ScKey *k = &P->kc[cpu->cpu_index & (SC_VCPUS - 1)];

        if (unlikely(k->kval != sc_word_base(env))) {
            goto full;                  /* to resolve (again) */
        }
        ka = sc_word_read(k->kword);
        if (unlikely(!ka)) {
            goto full;
        }
        kt = PANDA_TASK_KEY_WORD;
#elif defined(SC_KEY_REG)
        ka = sc_task_reg(env);
        kt = PANDA_TASK_KEY_REG;
#else
        goto full;                      /* no task key on this target */
#endif
        kb = 0;
    } else {
        ka = sc_root(env);
        kb = sc_user_sp(env);
        kt = PANDA_TASK_KEY_CALL;
        if (unlikely(!kb)) {
            goto full;
        }
    }
    hm = sc_mix(ka, kb);
    so = sc_set_off(P, hm);
    S = (ScSet *)((uint8_t *)P->sets + so);
    tag = sc_tag(hm) | (1 << 1);
    if (unlikely(qatomic_read(&S->cnt)) ||
        unlikely(qatomic_cmpxchg(&S->tag[0], 0, tag | SC_BUSY) != 0)) {
        goto full;
    }
    qatomic_inc(&S->cnt);
    r = (ScRec *)(P->recs + so * P->rmul);
    r->ka = ka;
    r->kb = kb;
    r->rflags = 0;
    r->usp = sc_user_sp(env);
    c = &r->c;
    ci = cpu->cpu_index;
    v = &P->v[ci & (SC_VCPUS - 1)];
    A = std ? P->abi[SC_TCG_FORM] : P->abi[wl >> 16];
    len = (wl >> 8) & 0xff;
    sc_fill(std, A, v, ci, env, c, nr, sc_tcg_pc(env, len), len);
    c->key = task ? ka : ka ^ kb;
    c->keytype = kt;
    SC_COUNT(v, kept);
    qatomic_set(&v->dlv, r);
    qatomic_store_release(&S->tag[0], tag);
    if (wl & SC_W_ENTRY) {
        P->fn(P->ctx, cpu, c, 0, 0);
    }
    return;
full:
    sc_enter_full(env, wl, nr);
}

/*
 * The entry functions. @wl: the number's want byte (not 0), the trap's length << 8, the form
 * << 16; @nr the number (as the ABI takes it). An entry alone is delivered here; the rest goes
 * to the keep function (a tail call).
 */
static inline QEMU_ALWAYS_INLINE void sc_enter_k(const bool std,
                                                 void (*keep)(CPUArchState *, uint32_t,
                                                              int64_t),
                                                 CPUArchState *env, uint32_t wl, int64_t nr)
{
#if defined(TARGET_I386)
    if (unlikely(std ? !(env->efer & MSR_EFER_SCE) : sc_x86_faults(env, wl >> 16))) {
        return;                         /* #UD or #GP: no system call */
    }
#endif
    if (likely(!(wl & SC_W_RET))) {
        sc_enter_slot(std, env, wl, nr);
        return;
    }
    keep(env, wl, nr);
}

#define SC_ENTER_FNS(name, task, std)                                                          \
static void __attribute__((noinline)) sc_keep_##name(CPUArchState *env, uint32_t wl,         \
                                                     int64_t nr)                               \
{                                                                                              \
    sc_keep_k(task, std, env, wl, nr);                                                         \
}                                                                                              \
void sc_enter_h_##name(CPUArchState *env, uint32_t wl, int64_t nr)                             \
{                                                                                              \
    sc_enter_k(std, sc_keep_##name, env, wl, nr);                                              \
}

SC_ENTER_FNS(task_s, true, true)
SC_ENTER_FNS(task, true, false)
SC_ENTER_FNS(call_s, false, true)
SC_ENTER_FNS(call, false, false)

/*
 * A return to user mode whose set holds calls (or while a child watch is set): the common case,
 * the set's only call is the key's, in way 0, and this return completes it at its return
 * address. Delivered, then popped (the record is BUSY meanwhile).
 */
static inline QEMU_ALWAYS_INLINE void sc_ret_k(CPUArchState *env, uint64_t ka, uint64_t kb)
{
    ScState *P = sc;
    uint64_t hm = sc_mix(ka, kb), bt;
    size_t so = sc_set_off(P, hm);
    ScSet *S = (ScSet *)((uint8_t *)P->sets + so);
    CPUState *cpu;
    ScRec *r;
    ScAbi *A;
    int64_t ret;

    bt = qatomic_read(&S->tag[0]);
    if (unlikely((bt & ~(SC_TAG_LOW & ~SC_BUSY)) != sc_tag(hm)) ||
        unlikely(qatomic_read(&S->cnt) != 1) ||
        unlikely(qatomic_cmpxchg(&S->tag[0], bt, bt | SC_BUSY) != bt)) {
        sc_return_full(env, ka, kb);
        return;
    }
    r = (ScRec *)(P->recs + so * P->rmul);
    if (unlikely(r->ka != ka || r->kb != kb)) {
        qatomic_store_release(&S->tag[0], bt);  /* another key's call with the same tag */
        sc_return_full(env, ka, kb);
        return;
    }
    if (unlikely(sc_dest_pc(env) != r->c.pc + r->c.len) || unlikely(r->rflags)) {
        sc_return_other(env, so >> SC_SET_SHIFT, 0, bt, r);
        return;
    }
    A = P->by_id[r->c.abi];
    ret = sc_retval(A, env);
    cpu = env_cpu(env);
    SC_COUNT(&P->v[cpu->cpu_index & (SC_VCPUS - 1)], paired);
    P->fn(P->ctx, cpu, &r->c, ret, sc_ret_flags(A, cpu, &r->c, ret));
    qatomic_store_release(&S->tag[0], 0);       /* the dispatch is final: popped */
    qatomic_dec(&S->cnt);
}

void sc_ret_h_task(CPUArchState *env, uint64_t ka)
{
    sc_ret_k(env, ka, 0);
}

void sc_ret_h_call(CPUArchState *env, uint64_t ka, uint64_t kb)
{
    sc_ret_k(env, ka, kb);
}

#endif /* SC_TARGET */
