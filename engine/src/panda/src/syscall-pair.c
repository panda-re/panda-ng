/*
 * System calls paired in the engine (panda/syscall-pair.h): declared ABIs, task keys, the
 * general cases of entries and returns, and the translated-code parts
 * (panda/syscall-pair-tcg.h). The pending set's layout and its concurrency rules are in
 * syscall-pair-int.h; the functions translated code calls first are in syscall-pair-fast.c.
 *
 * Arming
 * ------
 * panda_sc_forms (declared forms) and panda_sc_armed (some return wanted) are read at
 * translation time; each change flushes translations. A run without a declaration translates
 * exactly as before.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/lockable.h"
#include "qemu/memalign.h"
#include "qemu/rcu.h"
#include "qemu/error-report.h"
#include "panda/plugin.h"
#include "system/tcg.h"
#include "hw/core/boards.h"
#include "tcg/tcg.h"
#include "tcg/tcg-op.h"
#include "exec/helper-head.h.inc"
#include "syscall-pair-int.h"

uint32_t panda_sc_forms;
bool panda_sc_armed;

#ifdef SC_TARGET

ScState *sc;
const uint64_t sc_zero_word;

/* --- Task keys ----------------------------------------------------------------------------- */

#if defined(TARGET_X86_64)
/* The host address of guest RAM at @pa (8 bytes, not crossing a page), or NULL */
static const uint64_t *sc_map_word(uint64_t pa)
{
    hwaddr l = 8, xlat;
    MemoryRegion *mr;

    if (pa & 7) {
        return NULL;
    }
    RCU_READ_LOCK_GUARD();
    mr = address_space_translate(&address_space_memory, pa, &xlat, &l, false,
                                 MEMTXATTRS_UNSPECIFIED);
    if (l < 8 || !memory_region_is_ram(mr) || memory_region_is_ram_device(mr) ||
        !mr->ram_block) {
        return NULL;
    }
    return qemu_map_ram_ptr(mr->ram_block, xlat);
}

/* WORD, the cache missed: resolve for the current KERNEL_GS_BASE. The word, or NULL (the call
 * key is used meanwhile; the resolver runs again after SC_RETRY misses or a new base). */
static const uint64_t *sc_word_slow(ScState *P, CPUState *cpu, CPUArchState *env)
{
    ScVcpu *v = sc_vcpu(P, cpu);
    ScKey *k = &P->kc[cpu->cpu_index & (SC_VCPUS - 1)];
    uint64_t base = env->kernelgsbase, pa;
    const uint64_t *w;

    if (base == v->fail_base && v->retry) {
        v->retry--;
        return NULL;
    }
    v->st.resolves++;
    pa = P->resolve ? P->resolve(P->rctx, cpu, base) : 0;
    w = pa ? sc_map_word(pa) : NULL;
    if (!w) {
        v->fail_base = base;
        v->retry = SC_RETRY;
        return NULL;
    }
    qatomic_set(&k->kword, w);
    qatomic_set(&k->kval, base);
    return w;
}
#endif

/*
 * The key at the boundary: PANDA_TASK_KEY_REG/WORD (*kb 0), CALL (*kb the user SP, never 0 at a
 * boundary), or NONE (nothing is kept: a WORD key whose word holds 0, or no user SP).
 */
static uint32_t sc_key(ScState *P, CPUState *cpu, CPUArchState *env, uint64_t *ka,
                       uint64_t *kb)
{
#if defined(TARGET_X86_64)
    if (P->key_form == PANDA_TASK_KEY_WORD) {
        ScKey *k = &P->kc[cpu->cpu_index & (SC_VCPUS - 1)];
        const uint64_t *w = k->kword;

        if (k->kval != env->kernelgsbase) {
            w = sc_word_slow(P, cpu, env);
        }
        if (w) {
            *ka = *w;
            *kb = 0;
            return *ka ? PANDA_TASK_KEY_WORD : PANDA_TASK_KEY_NONE;
        }
    }
#else
    if (P->key_form == PANDA_TASK_KEY_REG) {
        *ka = env->sp_el[1];
        *kb = 0;
        return PANDA_TASK_KEY_REG;
    }
#endif
    *ka = sc_root(env);
    *kb = sc_user_sp(env);
    return *kb ? PANDA_TASK_KEY_CALL : PANDA_TASK_KEY_NONE;
}

uint32_t panda_task_key(CPUState *cpu, uint64_t *key)
{
    ScState *P = qatomic_read(&sc);
    uint64_t ka, kb;
    uint32_t t;

    if (!P || !sc_at_user(cpu_env(cpu))) {
        return PANDA_TASK_KEY_NONE;
    }
    t = sc_key(P, cpu, cpu_env(cpu), &ka, &kb);
    *key = t == PANDA_TASK_KEY_CALL ? ka ^ kb : ka;
    return t;
}

/* --- Child watches --------------------------------------------------------------------- */

void sc_watch_add(ScState *P, ScRec *r, CPUArchState *env)
{
    QEMU_LOCK_GUARD(&P->lock);
    for (unsigned i = 0; i < SC_WATCH; i++) {
        if (!P->watch[i].r) {
            P->watch[i] = (ScWatch) {
                .r = r, .id = r->c.id, .root = sc_root(env), .retpc = r->c.pc + r->c.len,
            };
            qatomic_inc(P->watch_n);
            return;
        }
    }
    /* full: this call's child returns are not delivered */
}

static void sc_watch_del(ScState *P, ScRec *r, uint64_t id)
{
    QEMU_LOCK_GUARD(&P->lock);
    for (unsigned i = 0; i < SC_WATCH; i++) {
        if (P->watch[i].r == r && P->watch[i].id == id) {
            P->watch[i].r = NULL;
            qatomic_dec(P->watch_n);
            return;
        }
    }
}

/* --- Entries ------------------------------------------------------------------------------- */

uint64_t sc_arg_mem(ScAbi *A, CPUArchState *env, unsigned r)
{
    uint64_t v = 0;
    unsigned n = A->mask == UINT32_MAX ? 4 : 8;

    if (cpu_memory_rw_debug(env_cpu(env), sc_reg(env, A->abi.form, r) & A->mask, &v, n,
                            false) != 0) {
        return 0;
    }
    return n == 4 ? le32_to_cpu((uint32_t)v) : le64_to_cpu(v);
}

/* A record another vCPU's entry delivery may still be writing: not to be evicted */
static bool sc_pinned(ScState *P, const ScRec *r, int self)
{
    unsigned n = current_machine ? current_machine->smp.max_cpus : SC_VCPUS;

    for (unsigned c = 0; c < n && c < SC_VCPUS; c++) {
        if (c != (unsigned)self && qatomic_read(&P->v[c].dlv) == r) {
            return true;
        }
    }
    return false;
}

/* The result of sc_claim: the way, and the depth bits to publish */
#define SC_CLAIM(way, low) ((int)(way) | ((int)(low) << 8))

/*
 * A key's entry into a set whose way 0 was not free at once: a free way and the key's depth;
 * if the set is full, its oldest record without BUSY is evicted. SC_CLAIM(way, depth << 1),
 * claimed with BUSY and counted, or -1 (every way busy or pinned).
 */
static int sc_claim(ScState *P, CPUState *cpu, unsigned s, uint64_t tb, uint64_t ka,
                    uint64_t kb)
{
    uint64_t *T = P->sets[s].tag;
    ScVcpu *v = sc_vcpu(P, cpu);

    for (unsigned tries = 0; tries < 8; tries++) {
        unsigned depth = 0;
        int free_way = -1, victim = -1;
        uint64_t vt = 0, vage = UINT64_MAX, tag;

        for (unsigned w = 0; w < SC_WAYS; w++) {
            uint64_t t = qatomic_load_acquire(&T[w]);
            ScRec *r = sc_rec(P, s, w);

            if (!t) {
                if (free_way < 0) {
                    free_way = w;
                }
                continue;
            }
            if ((t & ~SC_TAG_LOW) == tb && r->ka == ka && r->kb == kb) {
                unsigned d = (t >> 1) & SC_DEPTH_MAX;
                depth = d > depth ? d : depth;
            }
            if (!(t & SC_BUSY) && (r->c.id >> 8) < vage &&
                !sc_pinned(P, r, cpu->cpu_index)) {
                vage = r->c.id >> 8;            /* the vCPUs' entry counts: an age */
                victim = w;
                vt = t;
            }
        }
        depth = depth < SC_DEPTH_MAX ? depth + 1 : SC_DEPTH_MAX;
        tag = tb | (depth << 1);
        if (free_way >= 0) {
            if (qatomic_cmpxchg(&T[free_way], 0, tag | SC_BUSY) == 0) {
                qatomic_inc(&P->sets[s].cnt);
                return SC_CLAIM(free_way, depth << 1);
            }
        } else if (victim >= 0) {
            ScRec *r = sc_rec(P, s, victim);
            uint64_t id = r->c.id;

            if (qatomic_cmpxchg(&T[victim], vt, tag | SC_BUSY) == vt) {
                v->st.evicted++;                /* taken over: the count is unchanged */
                if (r->rflags) {
                    sc_watch_del(P, r, id);
                }
                return SC_CLAIM(victim, depth << 1);
            }
        } else {
            break;
        }
    }
    v->st.not_kept++;
    return -1;
}

/* The set and way of a record */
static void sc_where(ScState *P, const ScRec *r, unsigned *s, unsigned *w)
{
    size_t off = ((const uint8_t *)r - P->recs) / P->stride;

    *s = off / SC_WAYS;
    *w = off % SC_WAYS;
}

/*
 * Keep the call in the vCPU's slot (not kept at the entry; the consumer asked for it): claim a
 * record, copy, publish. False if no record could be taken.
 */
static bool sc_keep_late(ScState *P, CPUState *cpu, CPUArchState *env, ScAbi *A,
                         PandaSyscallCall *slot)
{
    uint64_t ka, kb, hm, tb;
    uint32_t kt = sc_key(P, cpu, env, &ka, &kb);
    ScVcpu *v = sc_vcpu(P, cpu);
    unsigned s;
    int cl;
    ScRec *r;

    if (kt == PANDA_TASK_KEY_NONE) {
        return false;
    }
    hm = sc_mix(ka, kb);
    s = sc_set(P, hm);
    tb = sc_tag(hm);
    cl = sc_claim(P, cpu, s, tb, ka, kb);
    if (cl < 0) {
        return false;
    }
    r = sc_rec(P, s, cl & 0xff);
    r->ka = ka;
    r->kb = kb;
    r->rflags = sc_bit(A->abi.shared_child, sc_index(&A->abi, slot->nr)) ? SC_W_CHILD : 0;
    memcpy(&r->c, slot, P->slot_stride);
    r->c.key = kt == PANDA_TASK_KEY_CALL ? ka ^ kb : ka;
    r->c.keytype = kt;
    SC_COUNT(v, kept);
    if (P->key_form == PANDA_TASK_KEY_WORD && kt == PANDA_TASK_KEY_CALL) {
        v->st.call_key++;
    }
    if (r->rflags) {
        sc_watch_add(P, r, env);
    }
    qatomic_store_release(&P->sets[s].tag[cl & 0xff], tb | (cl >> 8));
    return true;
}

/* Pop a published record of this vCPU's task (the entry's consumer said NOKEEP) */
static void sc_unkeep(ScState *P, ScRec *r)
{
    unsigned s, w;
    uint64_t t;

    sc_where(P, r, &s, &w);
    t = qatomic_read(&P->sets[s].tag[w]);
    if (r->rflags) {
        sc_watch_del(P, r, r->c.id);
    }
    if (t && !(t & SC_BUSY) && qatomic_cmpxchg(&P->sets[s].tag[w], t, 0) == t) {
        qatomic_dec(&P->sets[s].cnt);
    }
}

void panda_syscall_keep(CPUState *cpu, PandaSyscallCall *c, bool keep)
{
    ScState *P = qatomic_read(&sc);
    ScVcpu *v;

    if (!P || keep == (c->keytype != PANDA_TASK_KEY_NONE)) {
        return;
    }
    v = sc_vcpu(P, cpu);
    if (!keep) {
        ScRec *r = container_of(c, ScRec, c);

        if (v->dlv == r) {              /* the record this vCPU's entry is delivering */
            sc_unkeep(P, r);
            qatomic_set(&v->dlv, NULL);
        }
    } else if (c == v->slot) {
        ScAbi *A = P->by_id[c->abi];

        if (A && !sc_bit(A->abi.noreturn, sc_index(&A->abi, c->nr))) {
            sc_keep_late(P, cpu, cpu_env(cpu), A, c);    /* never a noreturn call */
        }
    }
}

/*
 * An entry whose number is wanted (w != 0), any case: kept in its task's set if its return is
 * wanted, delivered if its entry is.
 */
static void sc_enter(ScState *P, ScAbi *A, CPUState *cpu, CPUArchState *env, unsigned w,
                     int64_t nr, uint64_t pc, uint32_t len)
{
    int ci = cpu->cpu_index;
    ScVcpu *v = &P->v[ci & (SC_VCPUS - 1)];
    PandaSyscallCall *c;

    if (w & SC_W_RET) {
        uint64_t ka, kb, hm, tb, tag;
        uint32_t kt = sc_key(P, cpu, env, &ka, &kb);
        unsigned s;
        int cl;
        ScRec *r;

        if (unlikely(kt == PANDA_TASK_KEY_NONE)) {
            goto not_kept;
        }
        hm = sc_mix(ka, kb);
        s = sc_set(P, hm);
        tb = sc_tag(hm);
        cl = sc_claim(P, cpu, s, tb, ka, kb);
        if (cl < 0) {
            goto not_kept;
        }
        tag = tb | (cl >> 8);
        r = sc_rec(P, s, cl & 0xff);
        r->ka = ka;
        r->kb = kb;
        r->rflags = w & SC_W_CHILD;
        c = &r->c;
        sc_fill(false, A, v, ci, env, c, nr, pc, len);
        c->key = kt == PANDA_TASK_KEY_CALL ? ka ^ kb : ka;
        c->keytype = kt;
        SC_COUNT(v, kept);
        if (P->key_form == PANDA_TASK_KEY_WORD && kt == PANDA_TASK_KEY_CALL) {
            v->st.call_key++;
        }
        if (r->rflags) {
            sc_watch_add(P, r, env);
        }
        qatomic_set(&v->dlv, r);
        qatomic_store_release(&P->sets[s].tag[cl & 0xff], tag);
        if (w & SC_W_ENTRY) {
            P->fn(P->ctx, cpu, c, 0, 0);
        }
        return;
    }
not_kept:
    c = v->slot;
    sc_fill(false, A, v, ci, env, c, nr, pc, len);
    if (w & SC_W_ENTRY) {
        P->fn(P->ctx, cpu, c, 0, 0);
    }
}

void sc_enter_full(CPUArchState *env, uint32_t wl, int64_t nr)
{
    ScState *P = sc;
    uint32_t len = (wl >> 8) & 0xff;

    sc_enter(P, P->abi[wl >> 16], env_cpu(env), env, wl & 0xff, nr, sc_tcg_pc(env, len), len);
}

void panda_sc_enter_c(CPUState *cpu, unsigned form, uint64_t pc, uint32_t len)
{
    ScState *P = qatomic_read(&sc);
    CPUArchState *env = cpu_env(cpu);
    ScAbi *A;
    unsigned w;
    int64_t nr;

    if (!P || form >= PANDA_SYSCALL_FORM_COUNT || !(A = P->abi[form])) {
        return;
    }
    nr = sc_nr(A, env);
    w = qatomic_read(&A->want[sc_index(&A->abi, nr)]);
    if (w) {
        sc_enter(P, A, cpu, env, w, nr, pc, len);
    }
}

/* --- Returns ------------------------------------------------------------------------------- */

/* The key's newest record in set @s, or -1 */
static int sc_find(ScState *P, unsigned s, uint64_t tb, uint64_t ka, uint64_t kb)
{
    uint64_t *T = P->sets[s].tag;
    unsigned bd = 0;
    int best = -1;

    for (unsigned w = 0; w < SC_WAYS; w++) {
        uint64_t t = qatomic_read(&T[w]);
        ScRec *r = sc_rec(P, s, w);

        if ((t & ~SC_TAG_LOW) != tb || r->ka != ka || r->kb != kb) {
            continue;
        }
        if (best < 0 || ((t >> 1) & SC_DEPTH_MAX) > bd) {
            best = w;
            bd = (t >> 1) & SC_DEPTH_MAX;
        }
    }
    return best;
}

/* Claim record @w of set @s (tag @t without BUSY) for its owner: a child's copy may hold BUSY
 * for a moment. False if it is gone (evicted). */
static bool sc_take(ScState *P, unsigned s, unsigned w, uint64_t t)
{
    uint64_t *T = &P->sets[s].tag[w];

    for (unsigned spin = 0; spin < 100000; spin++) {
        uint64_t cur = qatomic_cmpxchg(T, t, t | SC_BUSY);

        if (cur == t) {
            return true;
        }
        if (cur != (t | SC_BUSY)) {
            return false;
        }
        cpu_relax();
    }
    return false;
}

static PandaSyscallCall *sc_child_slot(ScState *P, CPUState *cpu)
{
    return (PandaSyscallCall *)(P->cslots +
                                (size_t)(cpu->cpu_index & (SC_VCPUS - 1)) * P->slot_stride);
}

/*
 * A child's first return (a 0 at a watched call's return address, in its address space): the
 * caller's call, copied under BUSY into this vCPU's child slot, delivered CHILD. True if
 * delivered.
 */
static bool sc_child(ScState *P, CPUState *cpu, CPUArchState *env, uint64_t pc)
{
    uint64_t root = sc_root(env);
    PandaSyscallCall *c = sc_child_slot(P, cpu);

    for (unsigned i = 0; i < SC_WATCH; i++) {
        ScWatch wt;
        ScRec *r;
        ScAbi *A;
        unsigned s, w;
        uint64_t t;

        qemu_mutex_lock(&P->lock);
        wt = P->watch[i];
        qemu_mutex_unlock(&P->lock);
        r = wt.r;
        if (!r || wt.retpc != pc || wt.root != root) {
            continue;
        }
        A = P->by_id[r->c.abi];
        if (!A || sc_retval(A, env) != 0) {
            return false;
        }
        sc_where(P, r, &s, &w);
        t = qatomic_load_acquire(&P->sets[s].tag[w]);
        if (!t || (t & SC_BUSY) || r->c.id != wt.id ||
            qatomic_cmpxchg(&P->sets[s].tag[w], t, t | SC_BUSY) != t) {
            return false;
        }
        if (r->c.id != wt.id) {
            qatomic_store_release(&P->sets[s].tag[w], t);
            return false;
        }
        memcpy(c, &r->c, P->slot_stride);
        qatomic_store_release(&P->sets[s].tag[w], t);
        sc_vcpu(P, cpu)->st.children++;
        P->fn(P->ctx, cpu, c, 0, sc_ret_flags(A, cpu, c, 0) | PANDA_SYSCALL_CHILD);
        return true;
    }
    return false;
}

void sc_return_other(CPUArchState *env, unsigned s, unsigned w, uint64_t bt, ScRec *r)
{
    ScState *P = sc;
    CPUState *cpu = env_cpu(env);
    ScVcpu *v = sc_vcpu(P, cpu);
    uint64_t *T = &P->sets[s].tag[w];
    uint64_t pc = sc_dest_pc(env);
    ScAbi *A = P->by_id[r->c.abi];
    uint64_t i = sc_index(&A->abi, r->c.nr);
    int64_t ret = sc_retval(A, env);
    uint32_t flags = sc_ret_flags(A, cpu, &r->c, ret);

    if (pc == r->c.pc + r->c.len) {
        if (ret == 0 && r->rflags) {
            /* the child, under the caller's key (a call key on its stack) */
            PandaSyscallCall *c = sc_child_slot(P, cpu);

            memcpy(c, &r->c, P->slot_stride);
            qatomic_store_release(T, bt);
            v->st.children++;
            P->fn(P->ctx, cpu, c, 0, flags | PANDA_SYSCALL_CHILD);
            return;
        }
    } else if (pc == r->c.pc + (A->abi.ret_skip ? r->c.len - A->abi.ret_skip : 0)) {
        flags |= PANDA_SYSCALL_RESTART;
        v->st.restarts++;
    } else if (sc_bit(A->abi.ctx_replace, i)) {
        /* it succeeded (execve): no return */
        v->st.ended++;
        if (r->rflags) {
            sc_watch_del(P, r, r->c.id);
        }
        P->fn(P->ctx, cpu, &r->c, 0, PANDA_SYSCALL_RETURN | PANDA_SYSCALL_ENDED);
        qatomic_store_release(T, 0);
        qatomic_dec(&P->sets[s].cnt);
        return;
    } else {
        qatomic_store_release(T, bt);           /* elsewhere: a signal handler; still pending */
        v->st.elsewhere++;
        return;
    }
    SC_COUNT(v, paired);
    if (r->rflags) {
        sc_watch_del(P, r, r->c.id);
    }
    P->fn(P->ctx, cpu, &r->c, ret, flags);
    qatomic_store_release(T, 0);                /* the dispatch is final: popped */
    qatomic_dec(&P->sets[s].cnt);
}

void sc_return_full(CPUArchState *env, uint64_t ka, uint64_t kb)
{
    ScState *P = sc;
    CPUState *cpu = env_cpu(env);
    uint64_t hm = sc_mix(ka, kb), tb = sc_tag(hm), bt;
    unsigned s = sc_set(P, hm);
    uint64_t *T = P->sets[s].tag;
    ScVcpu *v = sc_vcpu(P, cpu);
    ScRec *r;
    int best;

    if (!qatomic_read(&P->sets[s].cnt)) {
        goto child;
    }
    best = sc_find(P, s, tb, ka, kb);
    if (best < 0) {
        v->st.misses++;
        goto child;
    }
    bt = qatomic_read(&T[best]) & ~SC_BUSY;
    r = sc_rec(P, s, best);
    if (!sc_take(P, s, best, bt)) {
        v->st.misses++;
        goto child;
    }
    if (r->ka != ka || r->kb != kb) {
        qatomic_store_release(&T[best], bt);    /* another key's record with the same tag */
        v->st.misses++;
        goto child;
    }
    sc_return_other(env, s, best, bt, r);
    return;
child:
    if (qatomic_read(P->watch_n)) {
        sc_child(P, cpu, env, sc_dest_pc(env));
    }
}

/* From translated code: the key could not be read inline (WORD: the cache missed), or a
 * return to AArch32 EL0 */
static void sc_ret_slow_helper(CPUArchState *env)
{
    ScState *P = sc;
    CPUState *cpu = env_cpu(env);
    uint64_t ka, kb;

    if (!sc_at_user(env) || !sc_key(P, cpu, env, &ka, &kb)) {
        return;
    }
    if (qatomic_read(&P->sets[sc_set(P, sc_mix(ka, kb))].cnt) || qatomic_read(P->watch_n)) {
        sc_return_full(env, ka, kb);
    }
}

/* --- Translated code ----------------------------------------------------------------------- */

/* Named panda_sc_* in -d op (tests/syscall_pair_tcg.py looks for them) */
#define SC_ENTER_INFO(fn, nm) {                                                                \
    .func = fn, .name = nm,                                                                    \
    .flags = 0,         /* the callback may write registers */                                 \
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) | dh_typemask(i32, 2) |            \
                dh_typemask(i64, 3),                                                           \
}
/* [task key declared][the ABI's standard layout] */
static TCGHelperInfo sc_enter_info[2][2] = {
    { SC_ENTER_INFO(sc_enter_h_call, "panda_sc_enter_call"),
      SC_ENTER_INFO(sc_enter_h_call_s, "panda_sc_enter_call_s") },
    { SC_ENTER_INFO(sc_enter_h_task, "panda_sc_enter_task"),
      SC_ENTER_INFO(sc_enter_h_task_s, "panda_sc_enter_task_s") },
};
static TCGHelperInfo sc_ret_task_info = {
    .func = sc_ret_h_task, .name = "panda_sc_return_task",
    .flags = 0,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) | dh_typemask(i64, 2),
};
static TCGHelperInfo sc_ret_call_info = {
    .func = sc_ret_h_call, .name = "panda_sc_return_call",
    .flags = 0,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) | dh_typemask(i64, 2) |
                dh_typemask(i64, 3),
};
static TCGHelperInfo sc_ret_slow_info = {
    .func = sc_ret_slow_helper, .name = "panda_sc_return_slow",
    .flags = 0,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1),
};

/* @p + @off as a pointer for a load: one displacement if it fits the host's */
static TCGv_ptr sc_gen_addr(TCGv_ptr p, intptr_t *off)
{
    if (*off != (int32_t)*off) {
        tcg_gen_addi_ptr(p, p, *off);
        *off = 0;
    }
    return p;
}

void panda_sc_gen_enter(unsigned form, uint32_t len, TCGv_i64 nr)
{
    ScState *P = sc;
    ScAbi *A = P->abi[form];
    TCGLabel *skip = gen_new_label();
    TCGv_i64 i = tcg_temp_new_i64(), n = nr;
    TCGv_i32 w = tcg_temp_new_i32();
    TCGv_ptr p = tcg_temp_new_ptr();
    intptr_t off = (intptr_t)A->want;
    TCGHelperInfo *info = &sc_enter_info[P->key_form != PANDA_TASK_KEY_CALL][A->std];

    /* the number as the ABI takes it, and its want byte: want[min(nr, nbits)] (unsigned: a
     * negative number is nbits) */
    if (A->abi.nr_width == 32) {
        n = tcg_temp_new_i64();
        tcg_gen_ext32s_i64(n, nr);
    }
    tcg_gen_umin_i64(i, n, tcg_constant_i64(A->abi.nbits));
    tcg_gen_trunc_i64_ptr(p, i);
    tcg_gen_ld8u_i32(w, sc_gen_addr(p, &off), off);
    tcg_gen_brcondi_i32(TCG_COND_EQ, w, 0, skip);
    tcg_gen_ori_i32(w, w, len << 8 | form << 16);
    tcg_gen_call3(info->func, info, NULL, tcgv_ptr_temp(tcg_env), tcgv_i32_temp(w),
                  tcgv_i64_temp(n));
    gen_set_label(skip);
}

void panda_sc_gen_return(void)
{
    ScState *P = sc;
    TCGLabel *done = gen_new_label(), *slow = NULL;
    TCGv_i64 ka = tcg_temp_new_i64(), kb = NULL, h = tcg_temp_new_i64();
    TCGv_i32 c = tcg_temp_new_i32();
    TCGv_ptr p = tcg_temp_new_ptr();
    intptr_t off = (intptr_t)P->sets + offsetof(ScSet, cnt);

#if defined(TARGET_X86_64)
    /* to user mode (CPL 3; CPL 1 goes on to the C function, which tests it) */
    TCGv_i32 hf = tcg_temp_new_i32();

    tcg_gen_ld_i32(hf, tcg_env, offsetof(CPUX86State, hflags));
    tcg_gen_brcondi_i32(TCG_COND_TSTEQ, hf, 1, done);
    if (P->key_form == PANDA_TASK_KEY_WORD) {
        /* the vCPU's ScKey, valid while KERNEL_GS_BASE is the value it was resolved for (a
         * word that cannot be resolved leaves it invalid: the C function, the call key) */
        TCGv_ptr v = tcg_temp_new_ptr();
        TCGv_i64 g = tcg_temp_new_i64();
        intptr_t kv = (intptr_t)P->kc + offsetof(ScKey, kval), kw;

        slow = gen_new_label();
        tcg_gen_ld32u_i64(h, tcg_env, (int)(offsetof(ArchCPU, parent_obj.cpu_index) -
                                           offsetof(ArchCPU, env)));
        tcg_gen_shli_i64(h, h, 4);
        tcg_gen_trunc_i64_ptr(v, h);
        tcg_gen_ld_i64(h, sc_gen_addr(v, &kv), kv);
        kw = kv + offsetof(ScKey, kword) - offsetof(ScKey, kval);
        tcg_gen_ld_i64(g, tcg_env, offsetof(CPUX86State, kernelgsbase));
        tcg_gen_brcond_i64(TCG_COND_NE, h, g, slow);
        tcg_gen_ld_ptr(v, v, kw);
        tcg_gen_ld_i64(ka, v, 0);
        tcg_gen_muli_i64(h, ka, SC_HASH_K);
    } else {
        kb = tcg_temp_new_i64();
        tcg_gen_ld_i64(ka, tcg_env, offsetof(CPUX86State, cr[3]));
        tcg_gen_andi_i64(ka, ka, 0x000ffffffffff000ull);
        tcg_gen_ld_i64(kb, tcg_env, offsetof(CPUX86State, regs[R_ESP]));
        tcg_gen_xor_i64(h, ka, kb);
        tcg_gen_muli_i64(h, h, SC_HASH_K);
    }
#else
    /* to EL0 in AArch64 (PSTATE.M 0: EL0t; a return to AArch32 leaves PSTATE at EL1h) */
    TCGv_i32 ps = tcg_temp_new_i32();

    tcg_gen_ld_i32(ps, tcg_env, offsetof(CPUARMState, pstate));
    if (P->a32) {
        slow = gen_new_label();
        tcg_gen_brcondi_i32(TCG_COND_TSTNE, ps, PSTATE_M, slow);
    } else {
        tcg_gen_brcondi_i32(TCG_COND_TSTNE, ps, PSTATE_M, done);
    }
    if (P->key_form == PANDA_TASK_KEY_REG) {
        tcg_gen_ld_i64(ka, tcg_env, offsetof(CPUARMState, sp_el[1]));
        tcg_gen_muli_i64(h, ka, SC_HASH_K);
    } else {
        kb = tcg_temp_new_i64();
        tcg_gen_ld_i64(ka, tcg_env, offsetof(CPUARMState, cp15.ttbr0_el[1]));
        tcg_gen_andi_i64(ka, ka, 0x0000fffffffffffeull);
        tcg_gen_ld_i64(kb, tcg_env, offsetof(CPUARMState, xregs[31]));
        tcg_gen_xor_i64(h, ka, kb);
        tcg_gen_muli_i64(h, h, SC_HASH_K);
    }
#endif
    tcg_gen_shri_i64(h, h, 64 - P->sets_log - SC_SET_SHIFT);
    tcg_gen_andi_i64(h, h, ~(uint64_t)((1u << SC_SET_SHIFT) - 1));
    tcg_gen_trunc_i64_ptr(p, h);
    tcg_gen_ld8u_i32(c, sc_gen_addr(p, &off), off);
    if (P->child) {
        TCGv_i32 wn = tcg_temp_new_i32();

        tcg_gen_ld8u_i32(wn, tcg_constant_ptr(P->watch_n), 0);
        tcg_gen_or_i32(c, c, wn);
    }
    tcg_gen_brcondi_i32(TCG_COND_EQ, c, 0, done);
    if (kb) {
        tcg_gen_call3(sc_ret_call_info.func, &sc_ret_call_info, NULL, tcgv_ptr_temp(tcg_env),
                      tcgv_i64_temp(ka), tcgv_i64_temp(kb));
    } else {
        tcg_gen_call2(sc_ret_task_info.func, &sc_ret_task_info, NULL, tcgv_ptr_temp(tcg_env),
                      tcgv_i64_temp(ka));
    }
    if (slow) {
        tcg_gen_br(done);
        gen_set_label(slow);
#if defined(TARGET_AARCH64)
        /* not EL0t: to AArch32 EL0 (env->aarch64 0, CPSR mode USR) goes on, in C. A temp of
         * its own: reusing c would make it live across the branch (a spill on the fast path) */
        TCGv_i32 m = tcg_temp_new_i32();

        tcg_gen_ld_i32(m, tcg_env, offsetof(CPUARMState, aarch64));
        tcg_gen_brcondi_i32(TCG_COND_NE, m, 0, done);
        tcg_gen_ld_i32(m, tcg_env, offsetof(CPUARMState, uncached_cpsr));
        tcg_gen_andi_i32(m, m, CPSR_M);
        tcg_gen_brcondi_i32(TCG_COND_NE, m, ARM_CPU_MODE_USR, done);
#endif
        tcg_gen_call1(sc_ret_slow_info.func, &sc_ret_slow_info, NULL,
                      tcgv_ptr_temp(tcg_env));
    }
    gen_set_label(done);
}

/* --- Declarations and wants ---------------------------------------------------------------- */

static void *sc_alloc_low(size_t bytes)
{
#if defined(__x86_64__) && defined(MAP_32BIT)
    /* below 2 GiB: translated code reaches it with one displacement */
    void *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT,
                   -1, 0);

    if (p != MAP_FAILED) {
        return p;
    }
#endif
    return g_malloc0(bytes);
}

static void sc_flush(void)
{
    panda_do_flush_tb();
}

bool panda_syscall_consumer(void *owner, PandaSyscallFn fn, void *ctx, uint32_t data_size)
{
    ScState *P;
    const char *e = getenv("PANDA_SYSCALL_PAIR_SETS_LOG");
    unsigned sets_log = SC_SETS_LOG;

    /* (plugins load before the accelerator: under KVM nothing ever reaches the sites) */
    if (!owner || !fn || data_size > SC_DATA_MAX) {
        return false;
    }
    if (current_machine && current_machine->smp.max_cpus > SC_VCPUS) {
        error_report("syscall pairing: more than %d vCPUs", SC_VCPUS);
        return false;
    }
    P = qatomic_read(&sc);
    if (P && P->owner) {
        return P->owner == owner && P->fn == fn && P->ctx == ctx && P->data_size == data_size;
    }
    if (e && *e) {
        /* a smaller pending set (tests: a set full of calls, eviction) */
        sets_log = MIN(MAX(atoi(e), 0), 16);
    }
    /*
     * 64-byte aligned, so that v[] (QEMU_ALIGNED) starts a line: with malloc's 16 bytes, v[0]'s
     * first line also held fields every entry and return reads, and vCPU 0's writes took that
     * line from every other vCPU on each call.
     */
    P = qemu_memalign(64, sizeof(ScState));
    memset(P, 0, sizeof(*P));
    qemu_mutex_init(&P->lock);
    P->owner = owner;
    P->fn = fn;
    P->ctx = ctx;
    P->data_size = data_size;
    P->slot_stride = sizeof(PandaSyscallCall) + ROUND_UP(data_size, 8);
    P->stride = ROUND_UP(sizeof(ScRec) + ROUND_UP(data_size, 8), 64);
    P->key_form = PANDA_TASK_KEY_CALL;
    P->sets_log = sets_log;
    P->sshift = 64 - SC_SET_SHIFT - sets_log;
    P->rmul = P->stride * SC_WAYS >> SC_SET_SHIFT;
    P->sets = sc_alloc_low(sizeof(ScSet) << sets_log);
    memset(P->sets, 0, sizeof(ScSet) << sets_log);
    P->recs = qemu_memalign(64, P->stride * SC_WAYS << sets_log);
    memset(P->recs, 0, P->stride * SC_WAYS << sets_log);
    P->slots = g_malloc0(P->slot_stride * SC_VCPUS);
    P->cslots = g_malloc0(P->slot_stride * SC_VCPUS);
    P->kc = sc_alloc_low(sizeof(ScKey) * SC_VCPUS);
    P->watch_n = sc_alloc_low(64);
    *P->watch_n = 0;
    for (unsigned k = 0; k < SC_VCPUS; k++) {
        P->kc[k].kval = SC_KVAL_NONE;
        P->kc[k].kword = &sc_zero_word;
        P->v[k].slot = (PandaSyscallCall *)(P->slots + k * P->slot_stride);
    }
    qatomic_store_release(&sc, P);
    return true;
}

bool panda_task_key_declare(void *owner, const PandaTaskKeyDecl *d)
{
    ScState *P = qatomic_read(&sc);

    if (!P || P->owner != owner || !d || d->size < sizeof(*d) || panda_sc_forms) {
        return false;
    }
#if defined(TARGET_X86_64)
    if (d->form != PANDA_TASK_KEY_WORD || d->reg != PANDA_TASK_REG_X86_KERNEL_GS_BASE ||
        !d->resolve) {
        return false;
    }
#else
    if (d->form != PANDA_TASK_KEY_REG || d->reg != PANDA_TASK_REG_ARM_SP_EL1) {
        return false;
    }
#endif
    P->key_form = d->form;
    P->key_reg = d->reg;
    P->resolve = d->resolve;
    P->rctx = d->ctx;
    return true;
}

static uint8_t *sc_set_copy(const uint8_t *set, uint32_t nbits)
{
    return set ? g_memdup2(set, nbits / 8 + 1) : NULL;
}

int panda_syscall_abi_declare(void *owner, const PandaSyscallAbi *a)
{
    static const uint8_t std_args[6] =
#if defined(TARGET_X86_64)
        { 7, 6, 2, 10, 8, 9 };          /* RDI RSI RDX R10 R8 R9 */
#else
        { 0, 1, 2, 3, 4, 5 };
#endif
    ScState *P = qatomic_read(&sc);
    unsigned form;
    ScAbi *A;
    uint32_t forms;

    if (!P || P->owner != owner || !a || a->size < sizeof(*a) ||
        a->form >= PANDA_SYSCALL_FORM_COUNT || !a->nbits || a->nbits > 65536 ||
        a->nargs > 6 || a->id > 255 || (a->nr_width != 32 && a->nr_width != 64) ||
        a->ret_skip > 15) {
        return -1;
    }
    form = a->form;
#if defined(TARGET_X86_64)
    if (form != PANDA_SYSCALL_FORM_X86_SYSCALL && form != PANDA_SYSCALL_FORM_X86_INT80 &&
        form != PANDA_SYSCALL_FORM_X86_SYSENTER && form != PANDA_SYSCALL_FORM_X86_SYSCALL32) {
        return -1;
    }
#else
    if (form != PANDA_SYSCALL_FORM_A64_SVC && form != PANDA_SYSCALL_FORM_A32_SVC) {
        return -1;
    }
#endif
    QEMU_LOCK_GUARD(&P->lock);
    if (P->abi[form] || P->by_id[a->id]) {
        return -1;
    }
    A = g_new0(ScAbi, 1);
    A->abi = *a;
    A->abi.noreturn = sc_set_copy(a->noreturn, a->nbits);
    A->abi.ctx_replace = sc_set_copy(a->ctx_replace, a->nbits);
    A->abi.shared_child = sc_set_copy(a->shared_child, a->nbits);
    A->want = sc_alloc_low(a->nbits + 1);
    memset(A->want, 0, a->nbits + 1);
    A->mask = (form == PANDA_SYSCALL_FORM_X86_SYSCALL || form == PANDA_SYSCALL_FORM_A64_SVC) ?
              UINT64_MAX : UINT32_MAX;
    A->std = A->mask == UINT64_MAX && a->nargs == 6 && !memcmp(a->arg_reg, std_args, 6) &&
             a->nr_reg == SC_STD_NR && a->ret_reg == SC_STD_RET && !a->ret_skip;
    if (a->shared_child) {
        P->child = true;
    }
    if (form == PANDA_SYSCALL_FORM_A32_SVC) {
        P->a32 = true;
    }
    P->by_id[a->id] = A;
    qatomic_store_release(&P->abi[form], A);
    forms = panda_sc_forms | (1u << form);
    qatomic_set(&panda_sc_forms, forms);
    sc_flush();
    return form;
}

void panda_syscall_want(int abi, int64_t nr, uint32_t want)
{
    ScState *P = qatomic_read(&sc);
    ScAbi *A;
    uint64_t i;
    unsigned old;
    bool arm = false, disarm = false;

    if (!P || abi < 0 || abi >= PANDA_SYSCALL_FORM_COUNT || !(A = qatomic_read(&P->abi[abi]))) {
        return;
    }
    i = sc_index(&A->abi, nr);
    want &= SC_W_ENTRY | SC_W_RET;
    if (sc_bit(A->abi.noreturn, i)) {
        want &= ~SC_W_RET;
    }
    if (want && sc_bit(A->abi.shared_child, i)) {
        want |= SC_W_CHILD;
    }
    old = qatomic_read(&A->want[i]);
    if (old == want) {
        return;
    }
    qemu_mutex_lock(&P->lock);
    old = A->want[i];
    qatomic_set(&A->want[i], want);
    if ((want & SC_W_RET) && !(old & SC_W_RET)) {
        arm = P->nret++ == 0;
    } else if (!(want & SC_W_RET) && (old & SC_W_RET)) {
        disarm = --P->nret == 0;
    }
    if (arm || disarm) {
        qatomic_set(&panda_sc_armed, arm);
    }
    qemu_mutex_unlock(&P->lock);
    if (arm || disarm) {
        sc_flush();
    }
}

void panda_syscall_consumer_release(void *owner)
{
    ScState *P = qatomic_read(&sc);

    if (!P || P->owner != owner) {
        return;
    }
    qatomic_set(&panda_sc_forms, 0);
    qatomic_set(&panda_sc_armed, false);
    /* the old state stays allocated: code translated before the flush may still read it */
    qatomic_store_release(&sc, NULL);
    sc_flush();
}

/* Reset, snapshot load, fast restore (vCPUs stopped): every pending call and key cache goes */
void panda_syscall_pair_reset(void)
{
    ScState *P = qatomic_read(&sc);

    if (!P) {
        return;
    }
    QEMU_LOCK_GUARD(&P->lock);
    memset(P->sets, 0, sizeof(ScSet) << P->sets_log);
    memset(P->watch, 0, sizeof(P->watch));
    qatomic_set(P->watch_n, 0);
    for (unsigned k = 0; k < SC_VCPUS; k++) {
        qatomic_set(&P->kc[k].kword, &sc_zero_word);
        qatomic_set(&P->kc[k].kval, SC_KVAL_NONE);
        P->v[k].dlv = NULL;
        P->v[k].fail_base = 0;
        P->v[k].retry = 0;
    }
}

void panda_syscall_stats(PandaSyscallStats *out)
{
    ScState *P = qatomic_read(&sc);
    uint64_t *o = (uint64_t *)out;

    memset(out, 0, sizeof(*out));
    if (!P) {
        return;
    }
    for (unsigned c = 0; c < SC_VCPUS; c++) {
        const uint64_t *s = (const uint64_t *)&P->v[c].st;

        for (unsigned k = 0; k < sizeof(*out) / sizeof(uint64_t); k++) {
            o[k] += s[k];
        }
        out->entries += qatomic_read(&P->v[c].ids);
    }
    out->pending = 0;
    for (unsigned s = 0; s < 1u << P->sets_log; s++) {
        out->pending += qatomic_read(&P->sets[s].cnt);
    }
}

#else /* !SC_TARGET */

bool panda_syscall_consumer(void *owner, PandaSyscallFn fn, void *ctx, uint32_t data_size)
{
    return false;
}
void panda_syscall_consumer_release(void *owner) {}
int panda_syscall_abi_declare(void *owner, const PandaSyscallAbi *abi) { return -1; }
void panda_syscall_want(int abi, int64_t nr, uint32_t want) {}
bool panda_task_key_declare(void *owner, const PandaTaskKeyDecl *decl) { return false; }
uint32_t panda_task_key(CPUState *cpu, uint64_t *key) { return PANDA_TASK_KEY_NONE; }
void panda_syscall_stats(PandaSyscallStats *out) { memset(out, 0, sizeof(*out)); }
void panda_sc_enter_c(CPUState *cpu, unsigned form, uint64_t pc, uint32_t len) {}
void panda_syscall_pair_reset(void) {}

#endif /* SC_TARGET */
