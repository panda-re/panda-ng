/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Taint: a TCG pass that adds a bit-precise shadow computation after translation.
 *
 * The pass runs once per translated block with CF_TAINT in its cflags, after plugin injection
 * and before tcg_optimize. It walks the op list and emits, next to each guest op, ops that
 * compute the op's output shadow from its inputs' shadows (Memcheck's V-bit rules).
 *
 * Shadow state:
 *   - every TCG global at env+off has a shadow global at senv+off, where senv is a per-vCPU
 *     buffer the size of the CPU state; explicit ld/st against env map to the same offsets;
 *   - every other temp gets a TEMP_TB shadow temp of the same type;
 *   - guest RAM has one shadow byte per byte, indexed by ram_addr_t (one sparse mapping);
 *   - a per-vCPU, per-mmu_idx shadow TLB (page -> shadow addend) sits after the shadow env,
 *     so a guest load/store's shadow access is inline on a hit and a helper call on a miss.
 * Helpers: the result shadow is the OR of the scalar input shadows (H1), except for the hand
 * models below; calls whose inputs are tainted are counted per helper.
 */
#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/madvise.h"
#include "qemu/memalign.h"
#include "qemu/main-loop.h"
#include "qom/object.h"
#include "hw/core/cpu.h"
#include "exec/cpu-common.h"
#include "exec/target_page.h"
#include "exec/translation-block.h"
#include "exec/tb-flush.h"
#include "exec/memop.h"
#include "exec/cputlb.h"
#include "exec/helper-head.h.inc"
#include "accel/tcg/cpu-ldst-common.h"
#include "system/memory.h"
#include "system/ramblock.h"
#include "system/ramlist.h"
#include "system/replay.h"
#include "system/analysis-policy.h"
#include "tcg/tcg.h"
#include "tcg/tcg-temp-internal.h"
#include "tcg/tcg-op-common.h"
#include "tcg/tcg-internal.h"
#include "exec/taint.h"

/* panda/plugin.h is target-specific; these two are not (panda/src/callbacks.c). */
bool panda_register_transient_reset(void *owner, void (*reset)(void *opaque), void *opaque);
void panda_unregister_transient_reset(void *owner);

bool taint_active;

/* ---------------------------------------------------------------- shadow state */

#define STLB_BITS 10
#define TAINT_MAX_ARGS 16   /* more than any non-call op has */
#define STLB_N (1u << STLB_BITS)

typedef struct STLBEntry {
    uint64_t tag;          /* guest page address, or -1 */
    intptr_t saddend;      /* shadow host address - guest vaddr */
} STLBEntry;

static size_t env_size;    /* bytes of CPU state from env on (instance size - env offset) */
static size_t stlb_off;    /* offset of the shadow TLB in the shadow env buffer */
static size_t senv_bytes;
static intptr_t senv_field_off;   /* env-relative offset of CPUState::taint_senv */
static int senv_idx;              /* temp index of the senv pointer global */
static int nb_guest_globals;
static uint16_t shadow_global[TCG_MAX_TEMPS];    /* guest global idx -> shadow global idx */

static uint8_t *ram_shadow;
static uint64_t ram_shadow_size;

static TaintDeviceWriteFn device_write_fn;
static void *device_write_opaque;

static struct {
    uint64_t tbs, ops, untracked_ld, untracked_st, ptr_calls, ld_slow, st_slow,
             ram_oob, unknown_ops, h1_tainted, device_writes, device_bytes;
} stats;
static GHashTable *h1_hits;     /* helper name -> tainted-call count */

static STLBEntry *stlb(CPUState *cpu, int mmu_idx)
{
    return (STLBEntry *)((uint8_t *)cpu->taint_senv + stlb_off) + (size_t)mmu_idx * STLB_N;
}

static void stlb_flush_all(CPUState *cpu)
{
    if (cpu->taint_senv) {
        memset(stlb(cpu, 0), -1, sizeof(STLBEntry) * STLB_N * NB_MMU_MODES);
    }
}

void taint_tlb_flush_mmuidx(CPUState *cpu, int mmu_idx)
{
    if (cpu->taint_senv) {
        memset(stlb(cpu, mmu_idx), -1, sizeof(STLBEntry) * STLB_N);
    }
}

void taint_tlb_flush_page(CPUState *cpu, int mmu_idx, vaddr page)
{
    if (cpu->taint_senv) {
        STLBEntry *e = stlb(cpu, mmu_idx) + ((page >> TARGET_PAGE_BITS) & (STLB_N - 1));
        e->tag = -1;
    }
}

static uint64_t ram_extent(void)
{
    uint64_t end = 0;
    RAMBlock *rb;

    RCU_READ_LOCK_GUARD();
    RAMBLOCK_FOREACH(rb) {
        end = MAX(end, qemu_ram_get_offset(rb) + qemu_ram_get_max_length(rb));
    }
    return end;
}

static void ram_shadow_alloc(void)
{
    uint64_t size = ROUND_UP(ram_extent(), qemu_real_host_page_size());

    if (ram_shadow && size <= ram_shadow_size) {
        return;
    }
    if (ram_shadow) {
        munmap(ram_shadow, ram_shadow_size);
    }
    ram_shadow = mmap(NULL, size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (ram_shadow == MAP_FAILED) {
        error_report("taint: cannot map %" PRIu64 " bytes of RAM shadow", size);
        abort();
    }
    ram_shadow_size = size;
}

static void ram_shadow_clear(void)
{
    if (ram_shadow) {
        qemu_madvise(ram_shadow, ram_shadow_size, QEMU_MADV_DONTNEED);
    }
}

static void senv_clear(CPUState *cpu)
{
    if (cpu->taint_senv) {
        memset(cpu->taint_senv, 0, stlb_off);
        stlb_flush_all(cpu);
    }
}

/* ---------------------------------------------------------------- slow-path helpers */

static uint8_t *ram_shadow_at(CPUState *cpu, vaddr addr, int mmu_idx, MMUAccessType type)
{
    uint64_t ra;

    if (!tlb_peek_ram_addr(cpu, addr, mmu_idx, type, &ra)) {
        return NULL;
    }
    if (ra >= ram_shadow_size) {
        stats.ram_oob++;
        return NULL;
    }
    return ram_shadow + ra;
}

/* Fill the shadow TLB for the page of @addr if it is RAM. */
static void stlb_fill(CPUState *cpu, vaddr addr, int mmu_idx, MMUAccessType type)
{
    vaddr page = addr & TARGET_PAGE_MASK;
    uint8_t *sp = ram_shadow_at(cpu, page, mmu_idx, type);

    if (sp && ram_shadow_at(cpu, page + TARGET_PAGE_SIZE - 1, mmu_idx, type)
              == sp + TARGET_PAGE_SIZE - 1) {
        STLBEntry *e = stlb(cpu, mmu_idx) + ((page >> TARGET_PAGE_BITS) & (STLB_N - 1));
        e->saddend = (intptr_t)sp - (intptr_t)page;
        e->tag = page;
    }
}

static bool guest_big_endian(MemOp mop)
{
    return !!(mop & MO_BSWAP) != HOST_BIG_ENDIAN;
}

static uint64_t taint_ld_slow(CPUArchState *env, uint64_t addr, uint32_t oi)
{
    CPUState *cpu = env_cpu(env);
    MemOp mop = get_memop(oi);
    int mmu_idx = get_mmuidx(oi);
    unsigned size = memop_size(mop);
    uint64_t v = 0;

    stats.ld_slow++;
    for (unsigned i = 0; i < size; i++) {
        uint8_t *sp = ram_shadow_at(cpu, addr + i, mmu_idx, MMU_DATA_LOAD);
        uint64_t b = sp ? *sp : 0;
        unsigned shift = guest_big_endian(mop) ? 8 * (size - 1 - i) : 8 * i;

        v |= b << shift;
    }
    if ((mop & MO_SIGN) && size < 8) {
        v = sextract64(v, 0, size * 8);
    }
    if (((addr & ~TARGET_PAGE_MASK) + size) <= TARGET_PAGE_SIZE) {
        stlb_fill(cpu, addr, mmu_idx, MMU_DATA_LOAD);
    }
    return v;
}

static void taint_st_slow(CPUArchState *env, uint64_t addr, uint32_t oi, uint64_t v)
{
    CPUState *cpu = env_cpu(env);
    MemOp mop = get_memop(oi);
    int mmu_idx = get_mmuidx(oi);
    unsigned size = memop_size(mop);

    stats.st_slow++;
    for (unsigned i = 0; i < size; i++) {
        uint8_t *sp = ram_shadow_at(cpu, addr + i, mmu_idx, MMU_DATA_STORE);
        unsigned shift = guest_big_endian(mop) ? 8 * (size - 1 - i) : 8 * i;

        if (sp) {
            *sp = v >> shift;
        }
    }
    if (((addr & ~TARGET_PAGE_MASK) + size) <= TARGET_PAGE_SIZE) {
        stlb_fill(cpu, addr, mmu_idx, MMU_DATA_STORE);
    }
}

static void taint_h1_hit(const TCGHelperInfo *info)
{
    gpointer n = g_hash_table_lookup(h1_hits, info->name);

    stats.h1_tainted++;
    g_hash_table_insert(h1_hits, (gpointer)info->name, GSIZE_TO_POINTER(GPOINTER_TO_SIZE(n) + 1));
}

static TCGHelperInfo ld_slow_info = {
    .func = taint_ld_slow, .name = "taint_ld_slow", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(i64, 0) | dh_typemask(ptr, 1) | dh_typemask(i64, 2) |
                dh_typemask(i32, 3),
};
static TCGHelperInfo st_slow_info = {
    .func = taint_st_slow, .name = "taint_st_slow", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) | dh_typemask(i64, 2) |
                dh_typemask(i32, 3) | dh_typemask(i64, 4),
};
static TCGHelperInfo h1_hit_info = {
    .func = taint_h1_hit, .name = "taint_h1_hit", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1),
};

/* ---------------------------------------------------------------- init */

void taint_tcg_init(CPUState *cpu)
{
    TCGContext *s = tcg_ctx;
    TCGTemp *env_ts = tcgv_ptr_temp(tcg_env);
    TCGv_ptr senv;

    if (!analysis_policy_has(ANALYSIS_CAP_TAINT)) {
        return;
    }
    env_size = object_class_get_instance_size(OBJECT_CLASS(cpu->cc)) -
               ((uintptr_t)cpu_env(cpu) - (uintptr_t)cpu);
    stlb_off = ROUND_UP(env_size, 64);
    senv_bytes = stlb_off + sizeof(STLBEntry) * STLB_N * NB_MMU_MODES;
    senv_field_off = (uintptr_t)&cpu->taint_senv - (uintptr_t)cpu_env(cpu);

    nb_guest_globals = s->nb_globals;
    senv = tcg_global_mem_new_ptr(tcg_env, senv_field_off, "taint_senv");
    senv_idx = temp_idx(tcgv_ptr_temp(senv));
    for (int i = 0; i < nb_guest_globals; i++) {
        TCGTemp *ts = &s->temps[i];
        TCGTemp *sh;

        if (ts->kind != TEMP_GLOBAL || ts->mem_base != env_ts || ts->indirect_reg) {
            continue;
        }
        if (ts->mem_offset < 0 || ts->mem_offset >= env_size) {
            continue;   /* CPUState fields (negative offsets) carry no guest data */
        }
        if (ts->base_type == TCG_TYPE_I32) {
            sh = tcgv_i32_temp(tcg_global_mem_new_i32(senv, ts->mem_offset,
                                                      g_strdup_printf("t_%s", ts->name)));
        } else {
            sh = tcgv_i64_temp(tcg_global_mem_new_i64(senv, ts->mem_offset,
                                                      g_strdup_printf("t_%s", ts->name)));
        }
        shadow_global[i] = temp_idx(sh);
    }
    h1_hits = g_hash_table_new(g_str_hash, g_str_equal);
}

void taint_vcpu_init(CPUState *cpu)
{
    if (!analysis_policy_has(ANALYSIS_CAP_TAINT) || !senv_bytes) {
        return;
    }
    cpu->taint_senv = qemu_memalign(64, senv_bytes);
    memset(cpu->taint_senv, 0, senv_bytes);
    stlb_flush_all(cpu);
}

/* ---------------------------------------------------------------- the pass */

typedef struct Pass {
    TCGContext *s;
    int nb_temps;           /* temps that existed before the pass */
    TCGTemp **sh;           /* shadow of each non-global temp */
    TCGTemp **sptr;         /* for a ptr temp equal to env + x: senv + x */
    TCGOp *chain;           /* first op of the current carry chain */
    TCGTemp *scarry;        /* 1 if the carry/borrow bit is tainted */
} Pass;

static TCGTemp *zero(TCGType t)
{
    return tcg_constant_internal(t, 0);
}

static TCGTemp *senv_temp(Pass *p)
{
    return &p->s->temps[senv_idx];
}

static bool is_env(TCGTemp *ts)
{
    return ts == tcgv_ptr_temp(tcg_env);
}

/* The shadow of @ts, allocating one for a temp that has none yet. */
static TCGTemp *S(Pass *p, TCGTemp *ts)
{
    size_t i = temp_idx(ts);

    switch (ts->kind) {
    case TEMP_CONST:
    case TEMP_FIXED:
        return zero(ts->type);
    case TEMP_GLOBAL:
        if (i < nb_guest_globals && shadow_global[i]) {
            return &p->s->temps[shadow_global[i]];
        }
        return zero(ts->type);
    default:
        break;
    }
    if (i >= p->nb_temps) {
        return zero(ts->type);
    }
    if (!p->sh[i]) {
        if (ts->base_type == TCG_TYPE_I128) {
            TCGTemp *first = ts - ts->temp_subindex;
            TCGTemp *sh = tcg_temp_new_internal(TCG_TYPE_I128, TEMP_TB);

            p->sh[temp_idx(first)] = sh;
            p->sh[temp_idx(first) + 1] = sh + 1;
        } else {
            p->sh[i] = tcg_temp_new_internal(ts->base_type, TEMP_TB);
        }
    }
    return p->sh[i];
}

/* The shadow to write for output @ts: a scratch temp for an untracked global. */
static TCGTemp *D(Pass *p, TCGTemp *ts)
{
    size_t i = temp_idx(ts);

    if (ts->kind == TEMP_GLOBAL && !(i < nb_guest_globals && shadow_global[i])) {
        return tcg_temp_new_internal(ts->type, TEMP_TB);
    }
    return S(p, ts);
}

/* Type-generic emitters on temps. Vector ops use element size @vece. */
#define I32(t) temp_tcgv_i32(t)
#define I64(t) temp_tcgv_i64(t)
#define VEC(t) temp_tcgv_vec(t)

#define GEN_BIN(name)                                                         \
static void g_##name(TCGType ty, unsigned vece, TCGTemp *d, TCGTemp *a, TCGTemp *b) \
{                                                                             \
    switch (ty) {                                                             \
    case TCG_TYPE_I32: tcg_gen_##name##_i32(I32(d), I32(a), I32(b)); break;   \
    case TCG_TYPE_I64: tcg_gen_##name##_i64(I64(d), I64(a), I64(b)); break;   \
    default: tcg_gen_##name##_vec(vece, VEC(d), VEC(a), VEC(b)); break;       \
    }                                                                         \
}
GEN_BIN(or)
GEN_BIN(and)
GEN_BIN(andc)
GEN_BIN(xor)

static void g_mov(TCGType ty, TCGTemp *d, TCGTemp *a)
{
    if (d == a) {
        return;
    }
    switch (ty) {
    case TCG_TYPE_I32: tcg_gen_mov_i32(I32(d), I32(a)); break;
    case TCG_TYPE_I64: tcg_gen_mov_i64(I64(d), I64(a)); break;
    default: tcg_gen_mov_vec(VEC(d), VEC(a)); break;
    }
}

static void g_not(TCGType ty, TCGTemp *d, TCGTemp *a)
{
    switch (ty) {
    case TCG_TYPE_I32: tcg_gen_not_i32(I32(d), I32(a)); break;
    case TCG_TYPE_I64: tcg_gen_not_i64(I64(d), I64(a)); break;
    default: tcg_gen_not_vec(0, VEC(d), VEC(a)); break;
    }
}

static TCGTemp *tmp(TCGType ty)
{
    return tcg_temp_new_internal(ty, TEMP_TB);
}

/* Left smear: every bit at and above the lowest set bit (carry propagation). */
static void g_lsmear(TCGType ty, unsigned vece, TCGTemp *d, TCGTemp *a)
{
    TCGTemp *t = tmp(ty);

    switch (ty) {
    case TCG_TYPE_I32: tcg_gen_neg_i32(I32(t), I32(a)); break;
    case TCG_TYPE_I64: tcg_gen_neg_i64(I64(t), I64(a)); break;
    default: tcg_gen_neg_vec(vece, VEC(t), VEC(a)); break;
    }
    g_or(ty, vece, d, a, t);
}

/* Pessimistic cast: all ones if any bit of @a is set (per element for vectors). */
static void g_pcast(TCGType ty, unsigned vece, TCGTemp *d, TCGTemp *a)
{
    switch (ty) {
    case TCG_TYPE_I32: tcg_gen_negsetcondi_i32(TCG_COND_NE, I32(d), I32(a), 0); break;
    case TCG_TYPE_I64: tcg_gen_negsetcondi_i64(TCG_COND_NE, I64(d), I64(a), 0); break;
    default:
        tcg_gen_cmp_vec(TCG_COND_NE, vece, VEC(d), VEC(a),
                        tcg_constant_vec(ty, vece, 0));
        break;
    }
}

/* OR of the shadows of @n inputs of type @ty, into a new temp. */
static TCGTemp *or_inputs(Pass *p, TCGType ty, unsigned vece, TCGTemp **in, int n)
{
    TCGTemp *acc = NULL;

    for (int i = 0; i < n; i++) {
        TCGTemp *si = S(p, in[i]);

        if (si->kind == TEMP_CONST) {
            continue;
        }
        if (!acc) {
            acc = si;
        } else {
            TCGTemp *t = tmp(ty);

            g_or(ty, vece, t, acc, si);
            acc = t;
        }
    }
    return acc ? acc : zero(ty);
}

/* Emit a copy of @op with its temp args replaced by @args (constants copied as is). */
static void emit_clone(TCGOp *op, TCGArg *args)
{
    TCGOp *n = tcg_emit_op(op->opc, op->nargs);

    n->param1 = op->param1;
    n->param2 = op->param2;
    memcpy(n->args, args, sizeof(TCGArg) * op->nargs);
}

/* Clone @op onto shadows: outputs and inputs become shadows, constant args stay. */
static void clone_on_shadows(Pass *p, TCGOp *op)
{
    const TCGOpDef *def = &tcg_op_defs[op->opc];
    TCGArg args[TAINT_MAX_ARGS];
    int k = 0;

    for (int i = 0; i < def->nb_oargs; i++, k++) {
        args[k] = temp_arg(D(p, arg_temp(op->args[k])));
    }
    for (int i = 0; i < def->nb_iargs; i++, k++) {
        args[k] = temp_arg(S(p, arg_temp(op->args[k])));
    }
    for (; k < op->nargs; k++) {
        args[k] = op->args[k];
    }
    emit_clone(op, args);
}

/* All outputs := pcast(OR of all inputs). The sound default. */
static void rule_pcast_all(Pass *p, TCGOp *op, TCGType ty, unsigned vece)
{
    const TCGOpDef *def = &tcg_op_defs[op->opc];
    TCGTemp *in[TAINT_MAX_ARGS];
    int ni = 0;

    for (int i = 0; i < def->nb_iargs; i++) {
        TCGTemp *ts = arg_temp(op->args[def->nb_oargs + i]);

        if (ts->type == ty) {
            in[ni++] = ts;
        }
    }
    TCGTemp *acc = or_inputs(p, ty, vece, in, ni);
    for (int i = 0; i < def->nb_oargs; i++) {
        TCGTemp *o = arg_temp(op->args[i]);

        if (o->type == ty) {
            g_pcast(ty, vece, D(p, o), acc);
        } else {
            g_mov(o->type, D(p, o), zero(o->type));
        }
    }
    /* inputs of another type: any taint taints the outputs */
    for (int i = 0; i < def->nb_iargs; i++) {
        TCGTemp *ts = arg_temp(op->args[def->nb_oargs + i]);

        if (ts->type != ty && ts->type <= TCG_TYPE_I64 && ty <= TCG_TYPE_I64) {
            TCGTemp *any = tmp(ty);

            if (ts->type == TCG_TYPE_I32) {
                TCGv_i32 b = tcg_temp_new_i32();

                tcg_gen_negsetcondi_i32(TCG_COND_NE, b, I32(S(p, ts)), 0);
                if (ty == TCG_TYPE_I64) {
                    tcg_gen_ext_i32_i64(I64(any), b);
                } else {
                    tcg_gen_mov_i32(I32(any), b);
                }
            } else {
                TCGv_i64 b = tcg_temp_new_i64();

                tcg_gen_negsetcondi_i64(TCG_COND_NE, b, I64(S(p, ts)), 0);
                tcg_gen_extrl_i64_i32(I32(any), b);
            }
            for (int j = 0; j < def->nb_oargs; j++) {
                TCGTemp *o = arg_temp(op->args[j]);

                if (o->type == ty) {
                    g_or(ty, vece, D(p, o), D(p, o), any);
                }
            }
        }
    }
}

/* and: a bit is clean if both are clean, or either is a clean 0. */
static void rule_and(Pass *p, TCGType ty, unsigned vece, TCGTemp *d,
                     TCGTemp *a, TCGTemp *b, bool not_b)
{
    TCGTemp *sa = S(p, a), *sb = S(p, b);
    TCGTemp *bv = b;

    if (not_b) {
        bv = tmp(ty);
        g_not(ty, bv, b);
    }
    TCGTemp *t1 = tmp(ty), *t2 = tmp(ty), *t3 = tmp(ty);
    g_and(ty, vece, t1, sa, sb);
    g_and(ty, vece, t2, sa, bv);
    g_and(ty, vece, t3, a, sb);
    g_or(ty, vece, t1, t1, t2);
    g_or(ty, vece, D(p, d), t1, t3);
}

/* or: a bit is clean if both are clean, or either is a clean 1. */
static void rule_or(Pass *p, TCGType ty, unsigned vece, TCGTemp *d,
                    TCGTemp *a, TCGTemp *b, bool not_b)
{
    TCGTemp *sa = S(p, a), *sb = S(p, b);
    TCGTemp *t1 = tmp(ty), *t2 = tmp(ty), *t3 = tmp(ty);

    g_and(ty, vece, t1, sa, sb);
    if (not_b) {
        g_and(ty, vece, t2, sa, b);       /* ~(~b) */
    } else {
        g_andc(ty, vece, t2, sa, b);
    }
    g_andc(ty, vece, t3, sb, a);
    g_or(ty, vece, t1, t1, t2);
    g_or(ty, vece, D(p, d), t1, t3);
}

/* d = OP(sa, b) | pcast(sb): a shift/rotate by a tainted amount taints everything. */
static void rule_shift(Pass *p, TCGOp *op, TCGType ty)
{
    TCGTemp *d = arg_temp(op->args[0]), *a = arg_temp(op->args[1]), *b = arg_temp(op->args[2]);
    TCGTemp *t = tmp(ty), *m = tmp(ty);
    TCGArg args[3] = { temp_arg(t), temp_arg(S(p, a)), temp_arg(b) };

    emit_clone(op, args);
    g_pcast(ty, 0, m, S(p, b));
    g_or(ty, 0, D(p, d), t, m);
}

/* ---- guest memory */

static TCGv_ptr senv_ptr(Pass *p)
{
    return temp_tcgv_ptr(senv_temp(p));
}

/* Shadow TLB entry pointer for @a; returns the base offset of the mmu_idx's table. */
static TCGv_ptr stlb_entry(Pass *p, TCGv_i64 a, int mmu_idx, intptr_t *off)
{
    TCGv_i64 idx = tcg_temp_new_i64();
    TCGv_ptr ent = tcg_temp_new_ptr();

    tcg_gen_extract_i64(idx, a, TARGET_PAGE_BITS, STLB_BITS);
    tcg_gen_shli_i64(idx, idx, 4);
    tcg_gen_add_ptr(ent, senv_ptr(p), temp_tcgv_ptr(tcgv_i64_temp(idx)));
    *off = stlb_off + (intptr_t)mmu_idx * STLB_N * sizeof(STLBEntry);
    return ent;
}

static void bswap_shadow(TCGv_i64 v, unsigned size)
{
    switch (size) {
    case 2: tcg_gen_bswap16_i64(v, v, TCG_BSWAP_IZ | TCG_BSWAP_OZ); break;
    case 4: tcg_gen_bswap32_i64(v, v, TCG_BSWAP_IZ | TCG_BSWAP_OZ); break;
    case 8: tcg_gen_bswap64_i64(v, v); break;
    }
}

/* The shadow of a guest load of @oi at @a, as an i64 (sign-extended like the load). */
static TCGv_i64 shadow_load(Pass *p, TCGv_i64 a, MemOpIdx oi)
{
    MemOp mop = get_memop(oi);
    unsigned size = memop_size(mop);
    intptr_t off;
    TCGv_i64 res = tcg_temp_new_i64(), tag = tcg_temp_new_i64(), end = tcg_temp_new_i64();
    TCGv_ptr ent = stlb_entry(p, a, get_mmuidx(oi), &off), hp = tcg_temp_new_ptr();
    TCGLabel *slow = gen_new_label(), *done = gen_new_label();

    tcg_gen_ld_i64(tag, ent, off + offsetof(STLBEntry, tag));
    tcg_gen_addi_i64(end, a, size - 1);
    tcg_gen_andi_i64(end, end, TARGET_PAGE_MASK);
    tcg_gen_brcond_i64(TCG_COND_NE, tag, end, slow);
    tcg_gen_ld_ptr(hp, ent, off + offsetof(STLBEntry, saddend));
    tcg_gen_add_ptr(hp, hp, temp_tcgv_ptr(tcgv_i64_temp(a)));
    switch (size) {
    case 1: tcg_gen_ld8u_i64(res, hp, 0); break;
    case 2: tcg_gen_ld16u_i64(res, hp, 0); break;
    case 4: tcg_gen_ld32u_i64(res, hp, 0); break;
    default: tcg_gen_ld_i64(res, hp, 0); break;
    }
    if (guest_big_endian(mop) != HOST_BIG_ENDIAN) {
        bswap_shadow(res, size);
    }
    if ((mop & MO_SIGN) && size < 8) {
        tcg_gen_sextract_i64(res, res, 0, size * 8);
    }
    tcg_gen_br(done);
    gen_set_label(slow);
    tcg_gen_call3(taint_ld_slow, &ld_slow_info, tcgv_i64_temp(res), tcgv_ptr_temp(tcg_env),
                  tcgv_i64_temp(a), tcg_constant_internal(TCG_TYPE_I32, oi));
    gen_set_label(done);
    return res;
}

static void shadow_store(Pass *p, TCGv_i64 a, MemOpIdx oi, TCGv_i64 sv)
{
    MemOp mop = get_memop(oi);
    unsigned size = memop_size(mop);
    intptr_t off;
    TCGv_i64 tag = tcg_temp_new_i64(), end = tcg_temp_new_i64(), v = tcg_temp_new_i64();
    TCGv_ptr ent = stlb_entry(p, a, get_mmuidx(oi), &off), hp = tcg_temp_new_ptr();
    TCGLabel *slow = gen_new_label(), *done = gen_new_label();

    tcg_gen_ld_i64(tag, ent, off + offsetof(STLBEntry, tag));
    tcg_gen_addi_i64(end, a, size - 1);
    tcg_gen_andi_i64(end, end, TARGET_PAGE_MASK);
    tcg_gen_brcond_i64(TCG_COND_NE, tag, end, slow);
    tcg_gen_ld_ptr(hp, ent, off + offsetof(STLBEntry, saddend));
    tcg_gen_add_ptr(hp, hp, temp_tcgv_ptr(tcgv_i64_temp(a)));
    tcg_gen_mov_i64(v, sv);
    if (guest_big_endian(mop) != HOST_BIG_ENDIAN) {
        bswap_shadow(v, size);
    }
    switch (size) {
    case 1: tcg_gen_st8_i64(v, hp, 0); break;
    case 2: tcg_gen_st16_i64(v, hp, 0); break;
    case 4: tcg_gen_st32_i64(v, hp, 0); break;
    default: tcg_gen_st_i64(v, hp, 0); break;
    }
    tcg_gen_br(done);
    gen_set_label(slow);
    tcg_gen_call4(taint_st_slow, &st_slow_info, NULL, tcgv_ptr_temp(tcg_env),
                  tcgv_i64_temp(a), tcg_constant_internal(TCG_TYPE_I32, oi), tcgv_i64_temp(sv));
    gen_set_label(done);
}

/* The guest address of a qemu_ld/st as an i64 temp (copied, since the op may overwrite it). */
static TCGv_i64 copy_addr(TCGTemp *addr)
{
    TCGv_i64 a = tcg_temp_new_i64();

    if (addr->type == TCG_TYPE_I32) {
        tcg_gen_extu_i32_i64(a, I32(addr));
    } else {
        tcg_gen_mov_i64(a, I64(addr));
    }
    return a;
}

static void set_from_i64(Pass *p, TCGTemp *d, TCGv_i64 v)
{
    TCGTemp *sd = D(p, d);

    if (sd->type == TCG_TYPE_I32) {
        tcg_gen_extrl_i64_i32(I32(sd), v);
    } else {
        tcg_gen_mov_i64(I64(sd), v);
    }
}

static TCGv_i64 as_i64(Pass *p, TCGTemp *v)
{
    TCGTemp *sv = S(p, v);

    if (sv->type == TCG_TYPE_I32) {
        TCGv_i64 t = tcg_temp_new_i64();

        tcg_gen_extu_i32_i64(t, I32(sv));
        return t;
    }
    return I64(sv);
}

static MemOpIdx half_oi(MemOpIdx oi)
{
    return make_memop_idx(MO_64 | (get_memop(oi) & MO_BSWAP), get_mmuidx(oi));
}

/* qemu_ld/st: the address copy goes before the op, the shadow access after it. */
static void taint_qemu_ldst(Pass *p, TCGOp *op, TCGOp *next)
{
    TCGContext *s = p->s;
    bool two = op->opc == INDEX_op_qemu_ld2 || op->opc == INDEX_op_qemu_st2;
    TCGTemp *v0 = arg_temp(op->args[0]);
    TCGTemp *v1 = two ? arg_temp(op->args[1]) : NULL;
    TCGTemp *addr = arg_temp(op->args[two ? 2 : 1]);
    MemOpIdx oi = op->args[two ? 3 : 2];
    TCGv_i64 a;

    s->emit_before_op = op;
    a = copy_addr(addr);
    s->emit_before_op = next;

    if (!two) {
        if (op->opc == INDEX_op_qemu_ld) {
            set_from_i64(p, v0, shadow_load(p, a, oi));
        } else {
            shadow_store(p, a, oi, as_i64(p, v0));
        }
        return;
    }
    /* 128 bits: two 64-bit halves; the low half is at the lower address unless big-endian */
    TCGv_i64 a2 = tcg_temp_new_i64();
    bool be = guest_big_endian(get_memop(oi));
    TCGTemp *lo_ts = v0, *hi_ts = v1;

    tcg_gen_addi_i64(a2, a, 8);
    if (op->opc == INDEX_op_qemu_ld2) {
        set_from_i64(p, lo_ts, shadow_load(p, be ? a2 : a, half_oi(oi)));
        set_from_i64(p, hi_ts, shadow_load(p, be ? a : a2, half_oi(oi)));
    } else {
        shadow_store(p, be ? a2 : a, half_oi(oi), as_i64(p, lo_ts));
        shadow_store(p, be ? a : a2, half_oi(oi), as_i64(p, hi_ts));
    }
}

/* ld/st against host memory: env-relative ones map to the shadow env. */
static void taint_host_ldst(Pass *p, TCGOp *op, bool load)
{
    TCGTemp *v = arg_temp(op->args[0]), *base = arg_temp(op->args[1]);
    TCGTemp *sbase = NULL;
    TCGArg args[TAINT_MAX_ARGS];

    if (is_env(base)) {
        sbase = senv_temp(p);
    } else if (temp_idx(base) < p->nb_temps && p->sptr[temp_idx(base)]) {
        sbase = p->sptr[temp_idx(base)];
    }
    if (!sbase) {
        if (load) {
            stats.untracked_ld++;
            g_mov(v->type, D(p, v), zero(v->type));
        } else {
            stats.untracked_st++;
        }
        return;
    }
    args[0] = temp_arg(load ? D(p, v) : S(p, v));
    args[1] = temp_arg(sbase);
    for (int k = 2; k < op->nargs; k++) {
        args[k] = op->args[k];
    }
    emit_clone(op, args);
}

/* ---- calls */

static const char *const control_prefixes[] = {
    "lookup_tb_ptr", "raise_exception", "exception", "panda_", "plugin_", "debug",
    "wfi", "wfe", "yield", "hlt", "pause", "rechecking_single_step", "check_bxj_trap",
    "exit_atomic", "rr_", "sc_", "tcg_hook", "taint_",
};

static bool is_control(const char *name)
{
    for (int i = 0; i < ARRAY_SIZE(control_prefixes); i++) {
        if (g_str_has_prefix(name, control_prefixes[i])) {
            return true;
        }
    }
    return false;
}

/* Hand models: helpers whose result has known clean bits. x86's lazy flags: the result of
 * cc_compute_all holds only CF PF AF ZF SF OF, and cc_compute_c only CF. */
static uint64_t result_mask(const char *name)
{
    if (!strcmp(name, "cc_compute_all")) {
        return 0x8d5;
    }
    if (!strcmp(name, "cc_compute_c") || !strcmp(name, "cc_compute_nz")) {
        return 1;
    }
    return -1;
}

static void taint_call(Pass *p, TCGOp *op)
{
    const TCGHelperInfo *info = tcg_call_info(op);
    int no = TCGOP_CALLO(op), ni = TCGOP_CALLI(op);
    TCGv_i64 acc = NULL;
    bool control = is_control(info->name);

    if (!control) {
        for (int i = 0; i < ni; i++) {
            TCGTemp *ts = arg_temp(op->args[no + i]);
            TCGv_i64 si;

            if (is_env(ts) || ts->kind == TEMP_CONST) {
                continue;
            }
            if (ts->type == TCG_TYPE_I64 && temp_idx(ts) < p->nb_temps &&
                p->sptr[temp_idx(ts)]) {
                stats.ptr_calls++;      /* points into env: no write-set model yet */
            }
            si = as_i64(p, ts);
            if (!acc) {
                acc = tcg_temp_new_i64();
                tcg_gen_mov_i64(acc, si);
            } else {
                tcg_gen_or_i64(acc, acc, si);
            }
        }
    }
    if (acc) {
        TCGLabel *skip = gen_new_label();

        tcg_gen_brcondi_i64(TCG_COND_EQ, acc, 0, skip);
        tcg_gen_call1(taint_h1_hit, &h1_hit_info, NULL,
                      tcg_constant_internal(TCG_TYPE_PTR, (uintptr_t)info));
        gen_set_label(skip);
        tcg_gen_negsetcondi_i64(TCG_COND_NE, acc, acc, 0);
        tcg_gen_andi_i64(acc, acc, result_mask(info->name));
    }
    for (int i = 0; i < no; i++) {
        TCGTemp *o = arg_temp(op->args[i]);
        TCGTemp *so = D(p, o);

        switch (o->type) {
        case TCG_TYPE_I32:
            if (acc) {
                tcg_gen_extrl_i64_i32(I32(so), acc);
            } else {
                tcg_gen_movi_i32(I32(so), 0);
            }
            break;
        case TCG_TYPE_I64:
            if (acc) {
                tcg_gen_mov_i64(I64(so), acc);
            } else {
                tcg_gen_movi_i64(I64(so), 0);
            }
            break;
        default:
            break;
        }
    }
}

/* ---- the op walk */

static bool is_carry_op(TCGOpcode opc)
{
    switch (opc) {
    case INDEX_op_addco: case INDEX_op_addci: case INDEX_op_addcio: case INDEX_op_addc1o:
    case INDEX_op_subbo: case INDEX_op_subbi: case INDEX_op_subbio: case INDEX_op_subb1o:
        return true;
    default:
        return false;
    }
}

static void taint_carry(Pass *p, TCGOp *op, TCGType ty)
{
    TCGTemp *d = arg_temp(op->args[0]), *a = arg_temp(op->args[1]), *b = arg_temp(op->args[2]);
    bool cin = op->opc == INDEX_op_addci || op->opc == INDEX_op_addcio ||
               op->opc == INDEX_op_subbi || op->opc == INDEX_op_subbio;
    bool cout = op->opc != INDEX_op_addci && op->opc != INDEX_op_subbi;
    TCGTemp *t = tmp(ty);

    g_or(ty, 0, t, S(p, a), S(p, b));
    if (cin && p->scarry) {
        TCGTemp *c = tmp(ty);

        if (ty == TCG_TYPE_I32) {
            tcg_gen_extrl_i64_i32(I32(c), I64(p->scarry));
        } else {
            tcg_gen_mov_i64(I64(c), I64(p->scarry));
        }
        g_or(ty, 0, t, t, c);
    }
    if (cout) {
        if (!p->scarry) {
            p->scarry = tmp(TCG_TYPE_I64);
        }
        if (ty == TCG_TYPE_I32) {
            TCGv_i32 c = tcg_temp_new_i32();

            tcg_gen_setcondi_i32(TCG_COND_NE, c, I32(t), 0);
            tcg_gen_extu_i32_i64(I64(p->scarry), c);
        } else {
            tcg_gen_setcondi_i64(TCG_COND_NE, I64(p->scarry), I64(t), 0);
        }
    }
    g_lsmear(ty, 0, D(p, d), t);
}

static void ptr_track(Pass *p, TCGOp *op, TCGType ty)
{
    const TCGOpDef *def = &tcg_op_defs[op->opc];

    if (def->nb_oargs == 0 || op->opc == INDEX_op_call) {
        return;
    }
    TCGTemp *d = arg_temp(op->args[0]);
    size_t di = temp_idx(d);

    if (di >= p->nb_temps || d->kind == TEMP_GLOBAL) {
        return;
    }
    TCGTemp *base = NULL;
    TCGTemp *a = def->nb_iargs > 0 ? arg_temp(op->args[1]) : NULL;
    TCGTemp *b = def->nb_iargs > 1 ? arg_temp(op->args[2]) : NULL;

    if (ty == TCG_TYPE_PTR && (op->opc == INDEX_op_add || op->opc == INDEX_op_mov)) {
        TCGTemp *other = NULL;

        if (a && is_env(a)) {
            base = senv_temp(p), other = b;
        } else if (b && is_env(b)) {
            base = senv_temp(p), other = a;
        } else if (a && temp_idx(a) < p->nb_temps && p->sptr[temp_idx(a)]) {
            base = p->sptr[temp_idx(a)], other = b;
        }
        if (base) {
            TCGTemp *sp = tmp(TCG_TYPE_PTR);

            if (other) {
                tcg_gen_add_ptr(temp_tcgv_ptr(sp), temp_tcgv_ptr(base), temp_tcgv_ptr(other));
            } else {
                tcg_gen_mov_ptr(temp_tcgv_ptr(sp), temp_tcgv_ptr(base));
            }
            p->sptr[di] = sp;
            return;
        }
    }
    p->sptr[di] = NULL;
}

static void taint_op(Pass *p, TCGOp *op, TCGOp *next)
{
    TCGContext *s = p->s;
    TCGOpcode opc = op->opc;
    TCGType ty = TCGOP_TYPE(op);
    unsigned vece = TCGOP_VECE(op);
    const TCGOpDef *def = &tcg_op_defs[opc];
    TCGTemp *d = def->nb_oargs ? arg_temp(op->args[0]) : NULL;
    TCGTemp *a = def->nb_iargs > 0 ? arg_temp(op->args[def->nb_oargs]) : NULL;
    TCGTemp *b = def->nb_iargs > 1 ? arg_temp(op->args[def->nb_oargs + 1]) : NULL;

    /* Shadow work goes before the op, so value-dependent rules see the inputs. */
    if (is_carry_op(opc)) {
        if (!p->chain) {
            p->chain = op;
        }
        s->emit_before_op = p->chain;
        taint_carry(p, op, ty);
        return;
    }
    p->chain = NULL;
    s->emit_before_op = op;
    stats.ops++;

    switch (opc) {
    case INDEX_op_set_label:
        memset(p->sptr, 0, sizeof(TCGTemp *) * p->nb_temps);
        return;
    case INDEX_op_discard: case INDEX_op_br: case INDEX_op_brcond: case INDEX_op_mb:
    case INDEX_op_insn_start: case INDEX_op_exit_tb: case INDEX_op_goto_tb:
    case INDEX_op_goto_ptr: case INDEX_op_plugin_cb: case INDEX_op_plugin_mem_cb:
        return;
    case INDEX_op_call:
        taint_call(p, op);
        return;
    case INDEX_op_qemu_ld: case INDEX_op_qemu_st:
    case INDEX_op_qemu_ld2: case INDEX_op_qemu_st2:
        taint_qemu_ldst(p, op, next);
        return;
    case INDEX_op_ld8u: case INDEX_op_ld8s: case INDEX_op_ld16u: case INDEX_op_ld16s:
    case INDEX_op_ld32u: case INDEX_op_ld32s: case INDEX_op_ld: case INDEX_op_ld_vec:
    case INDEX_op_dupm_vec:
        taint_host_ldst(p, op, true);
        ptr_track(p, op, ty);
        return;
    case INDEX_op_st8: case INDEX_op_st16: case INDEX_op_st32: case INDEX_op_st:
    case INDEX_op_st_vec:
        taint_host_ldst(p, op, false);
        return;
    default:
        break;
    }

    ptr_track(p, op, ty);

    switch (opc) {
    case INDEX_op_mov: case INDEX_op_mov_vec:
        g_mov(ty, D(p, d), S(p, a));
        break;
    case INDEX_op_xor: case INDEX_op_eqv: case INDEX_op_xor_vec: case INDEX_op_eqv_vec:
        g_or(ty, vece, D(p, d), S(p, a), S(p, b));
        break;
    case INDEX_op_not: case INDEX_op_not_vec:
        g_mov(ty, D(p, d), S(p, a));
        break;
    case INDEX_op_and: case INDEX_op_nand: case INDEX_op_and_vec: case INDEX_op_nand_vec:
        rule_and(p, ty, vece, d, a, b, false);
        break;
    case INDEX_op_andc: case INDEX_op_andc_vec:
        rule_and(p, ty, vece, d, a, b, true);
        break;
    case INDEX_op_or: case INDEX_op_nor: case INDEX_op_or_vec: case INDEX_op_nor_vec:
        rule_or(p, ty, vece, d, a, b, false);
        break;
    case INDEX_op_orc: case INDEX_op_orc_vec:
        rule_or(p, ty, vece, d, a, b, true);
        break;
    case INDEX_op_add: case INDEX_op_sub: case INDEX_op_mul:
    case INDEX_op_add_vec: case INDEX_op_sub_vec: case INDEX_op_mul_vec: {
        TCGTemp *t = tmp(ty);

        g_or(ty, vece, t, S(p, a), S(p, b));
        g_lsmear(ty, vece, D(p, d), t);
        break;
    }
    case INDEX_op_neg: case INDEX_op_neg_vec:
        g_lsmear(ty, vece, D(p, d), S(p, a));
        break;
    case INDEX_op_shl: case INDEX_op_shr: case INDEX_op_sar:
    case INDEX_op_rotl: case INDEX_op_rotr:
        rule_shift(p, op, ty);
        break;
    case INDEX_op_shli_vec: case INDEX_op_shri_vec: case INDEX_op_sari_vec:
    case INDEX_op_rotli_vec:
    case INDEX_op_bswap16: case INDEX_op_bswap32: case INDEX_op_bswap64:
    case INDEX_op_extract: case INDEX_op_sextract: case INDEX_op_deposit:
    case INDEX_op_extract2: case INDEX_op_ext_i32_i64: case INDEX_op_extu_i32_i64:
    case INDEX_op_extrl_i64_i32: case INDEX_op_extrh_i64_i32: case INDEX_op_dup_vec:
        clone_on_shadows(p, op);
        break;
    case INDEX_op_shls_vec: case INDEX_op_shrs_vec: case INDEX_op_sars_vec:
    case INDEX_op_rotls_vec: {
        TCGTemp *t = tmp(ty), *m = tmp(ty);
        TCGv_i32 any = tcg_temp_new_i32();
        TCGArg args[3] = { temp_arg(t), temp_arg(S(p, a)), temp_arg(b) };

        emit_clone(op, args);
        tcg_gen_negsetcondi_i32(TCG_COND_NE, any, I32(S(p, b)), 0);
        tcg_gen_dup_i32_vec(MO_32, VEC(m), any);
        g_or(ty, vece, D(p, d), t, m);
        break;
    }
    case INDEX_op_shlv_vec: case INDEX_op_shrv_vec: case INDEX_op_sarv_vec:
    case INDEX_op_rotlv_vec: case INDEX_op_rotrv_vec: {
        TCGTemp *t = tmp(ty), *m = tmp(ty);
        TCGArg args[3] = { temp_arg(t), temp_arg(S(p, a)), temp_arg(b) };

        emit_clone(op, args);
        g_pcast(ty, vece, m, S(p, b));
        g_or(ty, vece, D(p, d), t, m);
        break;
    }
    case INDEX_op_setcond: case INDEX_op_negsetcond: {
        TCGTemp *t = tmp(ty);

        g_or(ty, 0, t, S(p, a), S(p, b));
        g_pcast(ty, 0, D(p, d), t);
        if (opc == INDEX_op_setcond) {
            if (ty == TCG_TYPE_I32) {
                tcg_gen_andi_i32(I32(D(p, d)), I32(D(p, d)), 1);
            } else {
                tcg_gen_andi_i64(I64(D(p, d)), I64(D(p, d)), 1);
            }
        }
        break;
    }
    case INDEX_op_movcond: {
        /* d = cond(c1, c2) ? v1 : v2: the chosen shadow, plus all bits if the condition is tainted */
        TCGTemp *c1 = a, *c2 = b;
        TCGTemp *v1 = arg_temp(op->args[3]), *v2 = arg_temp(op->args[4]);
        TCGCond cond = op->args[5];
        TCGTemp *t = tmp(ty), *m = tmp(ty);

        if (ty == TCG_TYPE_I32) {
            tcg_gen_movcond_i32(cond, I32(t), I32(c1), I32(c2), I32(S(p, v1)), I32(S(p, v2)));
        } else {
            tcg_gen_movcond_i64(cond, I64(t), I64(c1), I64(c2), I64(S(p, v1)), I64(S(p, v2)));
        }
        g_or(ty, 0, m, S(p, c1), S(p, c2));
        g_pcast(ty, 0, m, m);
        g_or(ty, 0, D(p, d), t, m);
        break;
    }
    case INDEX_op_cmp_vec: {
        TCGTemp *t = tmp(ty);

        g_or(ty, vece, t, S(p, a), S(p, b));
        g_pcast(ty, vece, D(p, d), t);
        break;
    }
    case INDEX_op_bitsel_vec: {
        /* d = (a & b) | (~a & c) */
        TCGTemp *c = arg_temp(op->args[3]);
        TCGTemp *t = tmp(ty), *u = tmp(ty);

        tcg_gen_bitsel_vec(vece, VEC(t), VEC(a), VEC(S(p, b)), VEC(S(p, c)));
        g_xor(ty, vece, u, b, c);
        g_or(ty, vece, u, u, S(p, b));
        g_or(ty, vece, u, u, S(p, c));
        g_and(ty, vece, u, u, S(p, a));
        g_or(ty, vece, D(p, d), t, u);
        break;
    }
    case INDEX_op_cmpsel_vec: {
        TCGTemp *v1 = arg_temp(op->args[3]), *v2 = arg_temp(op->args[4]);
        TCGCond cond = op->args[5];
        TCGTemp *t = tmp(ty), *m = tmp(ty);

        tcg_gen_cmpsel_vec(cond, vece, VEC(t), VEC(a), VEC(b), VEC(S(p, v1)), VEC(S(p, v2)));
        g_or(ty, vece, m, S(p, a), S(p, b));
        g_pcast(ty, vece, m, m);
        g_or(ty, vece, D(p, d), t, m);
        break;
    }
    case INDEX_op_mulu2: case INDEX_op_muls2: {
        /* lo: carry smear; hi: everything */
        TCGTemp *hi = arg_temp(op->args[1]);
        TCGTemp *x = arg_temp(op->args[2]), *y = arg_temp(op->args[3]);
        TCGTemp *t = tmp(ty);

        g_or(ty, 0, t, S(p, x), S(p, y));
        g_pcast(ty, 0, D(p, hi), t);
        g_lsmear(ty, 0, D(p, d), t);
        break;
    }
    default:
        /* div/rem/mulh/clz/ctz/ctpop/abs/saturating/min/max and anything new */
        if (!(def->flags & TCG_OPF_VECTOR) &&
            !(opc == INDEX_op_divs || opc == INDEX_op_divu || opc == INDEX_op_rems ||
              opc == INDEX_op_remu || opc == INDEX_op_divs2 || opc == INDEX_op_divu2 ||
              opc == INDEX_op_mulsh || opc == INDEX_op_muluh || opc == INDEX_op_clz ||
              opc == INDEX_op_ctz || opc == INDEX_op_ctpop)) {
            stats.unknown_ops++;
        }
        rule_pcast_all(p, op, ty, vece);
        break;
    }
}

void taint_gen_tb(CPUState *cpu, TranslationBlock *tb)
{
    TCGContext *s = tcg_ctx;
    Pass p = { .s = s, .nb_temps = s->nb_temps };
    TCGOp *op, *next;
    const TCGOpcode *old_list = tcg_swap_vecop_list(NULL);

    if (!senv_idx) {
        return;
    }
    stats.tbs++;
    p.sh = tcg_malloc(sizeof(TCGTemp *) * p.nb_temps);
    p.sptr = tcg_malloc(sizeof(TCGTemp *) * p.nb_temps);
    memset(p.sh, 0, sizeof(TCGTemp *) * p.nb_temps);
    memset(p.sptr, 0, sizeof(TCGTemp *) * p.nb_temps);

    /*
     * The pass adds branches (shadow TLB miss, helper counters). Guest EBB temps may be live
     * across them, so they become TB temps; free EBB temps are not reused for shadows.
     */
    for (int i = s->nb_globals; i < p.nb_temps; i++) {
        if (s->temps[i].kind == TEMP_EBB) {
            s->temps[i].kind = TEMP_TB;
        }
    }
    memset(s->free_temps, 0, sizeof(s->free_temps));

    QTAILQ_FOREACH_SAFE(op, &s->ops, link, next) {
        taint_op(&p, op, next);
    }
    s->emit_before_op = NULL;
    tcg_swap_vecop_list(old_list);
}

/* ---------------------------------------------------------------- devices and API */

void taint_device_write(uint64_t ram_addr, uint64_t len)
{
    stats.device_writes++;
    stats.device_bytes += len;
    if (ram_shadow && ram_addr < ram_shadow_size) {
        memset(ram_shadow + ram_addr, 0, MIN(len, ram_shadow_size - ram_addr));
    }
    if (device_write_fn) {
        device_write_fn(device_write_opaque, ram_addr, len);
    }
}

void taint_set_device_write_cb(TaintDeviceWriteFn fn, void *opaque)
{
    device_write_fn = fn;
    device_write_opaque = opaque;
}

static void taint_reset(void *opaque)
{
    CPUState *cpu;

    ram_shadow_clear();
    CPU_FOREACH(cpu) {
        senv_clear(cpu);
    }
}

static int taint_reset_owner;

static void set_cflags(bool on)
{
    CPUState *cpu;

    CPU_FOREACH(cpu) {
        stlb_flush_all(cpu);
        if (on) {
            tcg_cflags_set(cpu, CF_TAINT);
            if (cpu->cflags_next_tb != -1) {
                cpu->cflags_next_tb |= CF_TAINT;
            }
        } else {
            tcg_cflags_clear(cpu, CF_TAINT);
            if (cpu->cflags_next_tb != -1) {
                cpu->cflags_next_tb &= ~CF_TAINT;
            }
        }
    }
    tb_flush__exclusive_or_serial();
}

static void commit_work(CPUState *cpu, run_on_cpu_data data)
{
    set_cflags(data.host_int);
}

/* From a vCPU (a callback) the flush waits for a safe point; otherwise it is immediate. */
static void commit(bool on)
{
    if (current_cpu) {
        async_safe_run_on_cpu(current_cpu, commit_work, RUN_ON_CPU_HOST_INT(on));
        cpu_exit(current_cpu);
    } else {
        set_cflags(on);
    }
}

bool taint_enable(void)
{
    CPUState *cpu;

    if (!analysis_policy_has(ANALYSIS_CAP_TAINT) || !senv_idx ||
        replay_mode != REPLAY_MODE_PLAY) {
        return false;
    }
    if (taint_active) {
        return true;
    }
    ram_shadow_alloc();
    CPU_FOREACH(cpu) {
        senv_clear(cpu);
    }
    panda_register_transient_reset(&taint_reset_owner, taint_reset, NULL);
    taint_active = true;
    commit(true);
    return true;
}

void taint_disable(void)
{
    if (!taint_active) {
        return;
    }
    taint_active = false;
    commit(false);
    panda_unregister_transient_reset(&taint_reset_owner);
    taint_reset(NULL);
}

bool taint_enabled(void)
{
    return taint_active;
}

void taint_label_ram(uint64_t ram_addr, uint64_t len, uint8_t mask)
{
    if (!ram_shadow) {
        ram_shadow_alloc();
    }
    if (ram_addr < ram_shadow_size) {
        memset(ram_shadow + ram_addr, mask, MIN(len, ram_shadow_size - ram_addr));
    }
}

uint8_t taint_query_ram(uint64_t ram_addr)
{
    return ram_shadow && ram_addr < ram_shadow_size ? ram_shadow[ram_addr] : 0;
}

uint64_t taint_count_ram(void)
{
    uint64_t n = 0;

    if (!ram_shadow) {
        return 0;
    }
    /* only resident shadow pages can be nonzero; mincore keeps this cheap on a sparse map */
    size_t pg = qemu_real_host_page_size();
    size_t npages = ram_shadow_size / pg;
    g_autofree unsigned char *vec = g_malloc(npages);

    if (mincore(ram_shadow, ram_shadow_size, vec) != 0) {
        memset(vec, 1, npages);
    }
    for (size_t i = 0; i < npages; i++) {
        if (!(vec[i] & 1)) {
            continue;
        }
        const uint64_t *w = (const uint64_t *)(ram_shadow + i * pg);
        for (size_t j = 0; j < pg / 8; j++) {
            if (w[j]) {
                for (int k = 0; k < 8; k++) {
                    n += ((w[j] >> (8 * k)) & 0xff) != 0;
                }
            }
        }
    }
    return n;
}

uint64_t taint_va_to_ram(CPUState *cpu, uint64_t va)
{
    TranslateForDebugResult t;
    hwaddr xlat, len = 1;
    MemoryRegion *mr;
    uint64_t ra = -1;

    if (!cpu_translate_for_debug(cpu, va, &t)) {
        return -1;
    }
    RCU_READ_LOCK_GUARD();
    mr = address_space_translate(cpu_get_address_space(cpu, cpu_asidx_from_attrs(cpu, t.attrs)),
                                 t.physaddr, &xlat, &len, false, t.attrs);
    if (memory_region_is_ram(mr)) {
        ra = memory_region_get_ram_addr(mr) + xlat;
    }
    return ra;
}

void taint_stats(char *buf, size_t len)
{
    GString *out = g_string_new(NULL);
    GHashTableIter it;
    gpointer k, v;

    g_string_append_printf(out,
        "tbs %" PRIu64 " ops %" PRIu64 " untracked_ld %" PRIu64 " untracked_st %" PRIu64
        " ptr_calls %" PRIu64 " ld_slow %" PRIu64 " st_slow %" PRIu64 " ram_oob %" PRIu64
        " unknown_ops %" PRIu64 " h1_tainted %" PRIu64 " device_writes %" PRIu64
        " device_bytes %" PRIu64 "\n",
        stats.tbs, stats.ops, stats.untracked_ld, stats.untracked_st, stats.ptr_calls,
        stats.ld_slow, stats.st_slow, stats.ram_oob, stats.unknown_ops, stats.h1_tainted,
        stats.device_writes, stats.device_bytes);
    if (h1_hits) {
        g_hash_table_iter_init(&it, h1_hits);
        while (g_hash_table_iter_next(&it, &k, &v)) {
            g_string_append_printf(out, "h1 %s %zu\n", (char *)k, GPOINTER_TO_SIZE(v));
        }
    }
    g_strlcpy(buf, out->str, len);
    g_string_free(out, true);
}
