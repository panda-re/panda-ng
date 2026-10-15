/*
 * TCG address hooks: call a host function before a guest instruction.
 *
 * Sites (one per hooked address) live in a QHT that the translator reads
 * lock-free under RCU.  A site is never freed: translated code embeds its
 * address.  Its hooks are an RCU list, and generated code tests the site's
 * hook count before calling, so removing the last hook leaves a load and a
 * branch until the block is retranslated.
 *
 * New sites are invalidated in batches, in exclusive context: the first
 * also turns CF_PCREL off on every vCPU and flushes everything, later ones
 * invalidate only the blocks whose guest range contains them.  To find
 * those without scanning every block, blocks linked while hooks are in use
 * are indexed by guest virtual page (4 KiB buckets) until the next flush;
 * a block's memory stays valid until then even after it is invalidated.
 *
 * A guarded hook carries conditions that the generated code tests before
 * calling it: on guest registers (read from their TCG globals) or on a
 * host variable.  Its code is emitted per hook, so inserting one always
 * invalidates the blocks containing its address, and a removed guarded
 * hook stays allocated (with its live flag cleared) until the next flush.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/qht.h"
#include "qemu/rcu.h"
#include "qemu/rcu_queue.h"
#include "qemu/xxhash.h"
#include "qemu/main-loop.h"
#include "qom/object.h"
#include "exec/gdbstub.h"
#include "exec/cpu-common.h"
#include "exec/translation-block.h"
#include "exec/translator.h"
#include "exec/tb-flush.h"
#include "accel/tcg/cpu-loop.h"
#include "exec/helper-head.h.inc"
#include "hw/core/cpu.h"
#include "tcg/tcg.h"
#include "tcg/tcg-op-common.h"
#include "tcg/tcg-temp-internal.h"
#include "accel/tcg/hooks.h"
#include "tb-context.h"
#include "internal-common.h"
#include "hooks-internal.h"
#include "qemu/timer.h"
#include "trace.h"

typedef struct TCGHookSite {
    vaddr pc;
    int32_t nhooks;                     /* read by generated code */
    QLIST_HEAD(, TCGHook) hooks;
    QLIST_HEAD(, TCGHook) ghooks;       /* guarded: code emitted per hook */
} TCGHookSite;

struct TCGHook {
    struct rcu_head rcu;
    TCGHookSite *site;
    TCGHookFn fn;
    void *opaque;
    QLIST_ENTRY(TCGHook) next;
    int32_t live;                       /* guarded: read by generated code */
    unsigned nguards;
    TCGHookGuard *guards;
};

bool tcg_hooks_enabled;

static QemuMutex hooks_lock;            /* writers: sites, lists, pending */
static struct qht sites;
static GArray *pending;                 /* new site pcs to invalidate */
static bool commit_queued;
static bool pcrel_off;                  /* CF_PCREL cleared + flushed */

/* invalidating more pcs than this at once flushes everything instead */
#define MAX_TARGETED 1024

static GPtrArray *graveyard;            /* removed guarded hooks */

static QemuMutex index_lock;
static GHashTable *tb_index;            /* bucket -> GPtrArray of TBs */
#define BUCKET(pc) ((uint64_t)(pc) >> 12)

/* register locations, per CPU class and gdbstub register number */
typedef struct RegLoc {
    bool ok;
    unsigned size;
    ptrdiff_t offset;
} RegLoc;
static QemuMutex regloc_lock;
static GHashTable *regloc;

static bool site_cmp(const void *a, const void *b)
{
    return ((const TCGHookSite *)a)->pc == ((const TCGHookSite *)b)->pc;
}

static bool site_key_cmp(const void *p, const void *userp)
{
    return ((const TCGHookSite *)p)->pc == *(const vaddr *)userp;
}

static uint32_t site_hash(vaddr pc)
{
    return qemu_xxhash2(pc);
}

static void __attribute__((constructor)) tcg_hooks_init(void)
{
    qemu_mutex_init(&hooks_lock);
    qht_init(&sites, site_cmp, 64, QHT_MODE_AUTO_RESIZE);
    pending = g_array_new(false, false, sizeof(vaddr));
    graveyard = g_ptr_array_new();
    qemu_mutex_init(&index_lock);
    qemu_mutex_init(&regloc_lock);
    regloc = g_hash_table_new_full(g_bytes_hash, g_bytes_equal,
                                   (GDestroyNotify)g_bytes_unref, g_free);
    tb_index = g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free,
                                     (GDestroyNotify)g_ptr_array_unref);
}

void tcg_hooks_tb_linked(TranslationBlock *tb)
{
    uint64_t b, last;

    if (tb_cflags(tb) & CF_PCREL) {
        return;                         /* not keyed by virtual address */
    }
    b = BUCKET(tb->pc);
    last = BUCKET(tb->pc + (tb->size ? tb->size - 1 : 0));
    qemu_mutex_lock(&index_lock);
    for (; b <= last; b++) {
        GPtrArray *a = g_hash_table_lookup(tb_index, &b);
        if (!a) {
            a = g_ptr_array_new();
            g_hash_table_insert(tb_index, g_memdup2(&b, sizeof(b)), a);
        }
        g_ptr_array_add(a, tb);
    }
    qemu_mutex_unlock(&index_lock);
}

void tcg_hooks_tb_flush(void)
{
    qemu_mutex_lock(&index_lock);
    g_hash_table_remove_all(tb_index);
    qemu_mutex_unlock(&index_lock);

    /* no translated code refers to removed guarded hooks any more */
    qemu_mutex_lock(&hooks_lock);
    for (guint i = 0; i < graveyard->len; i++) {
        TCGHook *h = g_ptr_array_index(graveyard, i);
        g_free(h->guards);
        g_free_rcu(h, rcu);
    }
    g_ptr_array_set_size(graveyard, 0);
    qemu_mutex_unlock(&hooks_lock);
}

static TCGHookSite *site_lookup(vaddr pc)
{
    return qht_lookup_custom(&sites, &pc, site_hash(pc), site_key_cmp);
}

int tcg_hook_count(vaddr pc)
{
    TCGHookSite *s;

    RCU_READ_LOCK_GUARD();
    s = site_lookup(pc);
    return s ? qatomic_read(&s->nhooks) : 0;
}

/*
 * Commit: runs in exclusive context, so no vCPU is translating or
 * executing translated code.
 */

/* Add the live blocks containing @pc to @tbs; drop dead ones from the index. */
static unsigned collect(vaddr pc, GPtrArray *tbs)
{
    uint64_t b = BUCKET(pc);
    GPtrArray *a = g_hash_table_lookup(tb_index, &b);
    unsigned seen = 0;

    for (guint i = 0; a && i < a->len;) {
        TranslationBlock *tb = g_ptr_array_index(a, i);

        seen++;
        if (tb_cflags(tb) & CF_INVALID) {
            g_ptr_array_remove_index_fast(a, i);
            continue;
        }
        if (pc >= tb->pc && pc < tb->pc + tb->size) {
            g_ptr_array_add(tbs, tb);
        }
        i++;
    }
    return seen;
}

static void hooks_commit(CPUState *cpu, run_on_cpu_data unused)
{
    g_autoptr(GArray) pcs = NULL;
    bool flush;
    CPUState *c;
    int64_t t0 = get_clock();
    unsigned scanned = 0, invalidated = 0;

    qemu_mutex_lock(&hooks_lock);
    pcs = pending;
    pending = g_array_new(false, false, sizeof(vaddr));
    commit_queued = false;
    flush = !pcrel_off || pcs->len > MAX_TARGETED;
    pcrel_off = true;
    qemu_mutex_unlock(&hooks_lock);

    CPU_FOREACH(c) {
        if (c->tcg_cflags & CF_PCREL) {
            qatomic_set(&c->tcg_cflags, c->tcg_cflags & ~CF_PCREL);
            flush = true;
        }
    }
    if (flush) {
        tb_flush__exclusive_or_serial();
    } else if (pcs->len) {
        g_autoptr(GPtrArray) tbs = g_ptr_array_new();

        qemu_mutex_lock(&index_lock);
        for (guint i = 0; i < pcs->len; i++) {
            scanned += collect(g_array_index(pcs, vaddr, i), tbs);
        }
        qemu_mutex_unlock(&index_lock);
        for (guint i = 0; i < tbs->len; i++) {
            tb_phys_invalidate(g_ptr_array_index(tbs, i), -1);
        }
        invalidated = tbs->len;
    }
    trace_tcg_hooks_commit(flush, pcs->len, scanned, invalidated,
                           get_clock() - t0);
}

/* Called with hooks_lock held. */
static void queue_commit(vaddr pc)
{
    CPUState *cpu = current_cpu ? current_cpu : first_cpu;

    if (!cpu) {
        /*
         * No vCPU yet: no translated code either, and every vCPU's cflags
         * are fixed in tcg_hooks_vcpu_cflags() when it is created.
         */
        pcrel_off = true;
        return;
    }
    g_array_append_val(pending, pc);
    if (commit_queued) {
        return;
    }
    commit_queued = true;
    async_safe_run_on_cpu(cpu, hooks_commit, RUN_ON_CPU_NULL);
    if (current_cpu) {
        /* leave the current block soon, so the commit runs */
        cpu_exit(current_cpu);
    }
}

void tcg_hooks_vcpu_cflags(CPUState *cpu)
{
    if (qatomic_read(&tcg_hooks_enabled)) {
        cpu->tcg_cflags &= ~CF_PCREL;
    }
}

static TCGHook *hook_insert(vaddr pc, TCGHookFn fn, void *opaque,
                            const TCGHookGuard *guards, unsigned n)
{
    TCGHook *h = g_new0(TCGHook, 1);
    TCGHookSite *s;

    h->fn = fn;
    h->opaque = opaque;
    h->live = 1;
    h->nguards = n;
    h->guards = n ? g_memdup2(guards, n * sizeof(*guards)) : NULL;

    qemu_mutex_lock(&hooks_lock);
    qatomic_set(&tcg_hooks_enabled, true);
    WITH_RCU_READ_LOCK_GUARD() {
        s = site_lookup(pc);
    }
    if (!s) {
        s = g_new0(TCGHookSite, 1);
        s->pc = pc;
        QLIST_INIT(&s->hooks);
        QLIST_INIT(&s->ghooks);
        qht_insert(&sites, s, site_hash(pc), NULL);
        queue_commit(pc);
    } else if (n) {
        queue_commit(pc);               /* its code is emitted per hook */
    }
    h->site = s;
    if (n) {
        QLIST_INSERT_HEAD_RCU(&s->ghooks, h, next);
    } else {
        QLIST_INSERT_HEAD_RCU(&s->hooks, h, next);
        qatomic_inc(&s->nhooks);
    }
    qemu_mutex_unlock(&hooks_lock);
    return h;
}

TCGHook *tcg_hook_insert(vaddr pc, TCGHookFn fn, void *opaque)
{
    return hook_insert(pc, fn, opaque, NULL, 0);
}

TCGHook *tcg_hook_insert_guarded(vaddr pc, TCGHookFn fn, void *opaque,
                                 const TCGHookGuard *guards, unsigned n)
{
    g_assert(n > 0);
    return hook_insert(pc, fn, opaque, guards, n);
}

void tcg_hook_remove(TCGHook *h)
{
    if (!h) {
        return;
    }
    qemu_mutex_lock(&hooks_lock);
    QLIST_REMOVE_RCU(h, next);
    if (h->nguards) {
        /* translated code may still test its live flag: keep it until a flush */
        qatomic_set(&h->live, 0);
        g_ptr_array_add(graveyard, h);
        qemu_mutex_unlock(&hooks_lock);
        return;
    }
    qatomic_dec(&h->site->nhooks);
    qemu_mutex_unlock(&hooks_lock);
    g_free_rcu(h, rcu);
}

/*
 * Does the aligned field at @f hold what the gdbstub reads back (@r, in
 * the gdbstub's byte order, which is the target's or the guest's current
 * one)?
 */
static bool reg_field_matches(const uint8_t *f, GByteArray *r, size_t n)
{
    if (r->len != n) {
        return false;
    }
    if (!memcmp(f, r->data, n)) {
        return true;
    }
    for (size_t i = 0; i < n; i++) {
        if (f[i] != r->data[n - 1 - i]) {
            return false;
        }
    }
    return true;
}

/*
 * Write two patterns that differ from the register's value in every byte.
 * Each must change bytes inside one aligned n-byte field and nothing else
 * (compared with a snapshot taken just before the write), and that field
 * must equal what a gdbstub read returns.  The value is written back.
 */
static uint8_t *reg_probe(CPUState *cpu, int regno, GByteArray *orig,
                          uint8_t *env, size_t envsz)
{
    size_t n = orig->len;
    g_autofree uint8_t *snap = g_malloc(envsz);
    g_autoptr(GByteArray) back = g_byte_array_new();
    uint8_t pat[8], *field = NULL;
    bool ok = true;

    for (int k = 0; k < 2 && ok; k++) {
        size_t first = SIZE_MAX, last = 0;

        for (size_t i = 0; i < n; i++) {
            pat[i] = orig->data[i] ^ (k ? 0xa5 : 0x5a);
        }
        memcpy(snap, env, envsz);
        if (gdb_write_register(cpu, pat, regno) <= 0) {
            return NULL;
        }
        for (size_t i = 0; i < envsz; i++) {
            if (env[i] != snap[i]) {
                first = MIN(first, i);
                last = i;
            }
        }
        if (first == SIZE_MAX || first / n != last / n
            || (field && field != env + first / n * n)) {
            trace_tcg_hooks_regloc_reject(regno, k,
                                          first == SIZE_MAX ? -1 : (long)first,
                                          first == SIZE_MAX ? -1 : (long)last);
            ok = false;
            break;
        }
        field = env + first / n * n;
        g_byte_array_set_size(back, 0);
        ok = gdb_read_register(cpu, back, regno) > 0
             && reg_field_matches(field, back, n);
    }
    gdb_write_register(cpu, orig->data, regno);
    return ok ? field : NULL;
}

bool tcg_register_location(CPUState *cpu, int regno, ptrdiff_t *offset,
                           unsigned *size)
{
    ObjectClass *oc = object_get_class(OBJECT(cpu));
    struct { ObjectClass *oc; int regno; } key = { oc, regno };
    g_autoptr(GBytes) k = g_bytes_new(&key, sizeof(key));
    g_autoptr(GByteArray) orig = g_byte_array_new();
    RegLoc *loc;
    uint8_t *env, *field = NULL;
    size_t envsz, n;
    bool locked;

    qemu_mutex_lock(&regloc_lock);
    loc = g_hash_table_lookup(regloc, k);
    qemu_mutex_unlock(&regloc_lock);
    if (loc) {
        goto out;
    }

    loc = g_new0(RegLoc, 1);
    if (gdb_read_register(cpu, orig, regno) > 0) {
        n = orig->len;
        if (n == 1 || n == 2 || n == 4 || n == 8) {
            /* everything from the architectural state to the end of the ArchCPU */
            env = (uint8_t *)cpu_env(cpu);
            envsz = object_class_get_instance_size(oc) - sizeof(CPUState);

            /*
             * Device and timer code updates CPU state under the BQL (e.g. the
             * Arm generic timer's status bits); hold it so the comparison only
             * sees our own writes.  Retry in case something else got in anyway.
             */
            locked = bql_locked();
            if (!locked) {
                bql_lock();
            }
            for (int tries = 0; tries < 3 && !field; tries++) {
                field = reg_probe(cpu, regno, orig, env, envsz);
            }
            if (!locked) {
                bql_unlock();
            }
            if (field) {
                loc->ok = true;
                loc->size = n;
                loc->offset = field - env;
            }
        }
    }
    qemu_mutex_lock(&regloc_lock);
    g_hash_table_insert(regloc, g_bytes_ref(k), loc);
    qemu_mutex_unlock(&regloc_lock);

out:
    if (loc->ok) {
        *offset = loc->offset;
        *size = loc->size;
    }
    return loc->ok;
}

void tcg_hook_restore_state(CPUState *cpu)
{
    /*
     * cpu_restore_state() also credits icount with the instructions the
     * block will not execute; we return into the block, so undo that.
     */
    uint16_t budget = cpu->neg.icount_decr.u16.low;

    if (cpu->tcg_hook_ra) {
        cpu_restore_state(cpu, cpu->tcg_hook_ra);
    }
    cpu->neg.icount_decr.u16.low = budget;
}

void tcg_hook_set_pc(CPUState *cpu, vaddr pc)
{
    if (cpu->tcg_hook_ra) {
        cpu_restore_state(cpu, cpu->tcg_hook_ra);
    }
    cpu_set_pc(cpu, pc);
    cpu->tcg_hook_ra = 0;
    cpu_loop_exit(cpu);
}

/*
 * The helper.  Entered from translated code with every TCG global saved
 * (call flags 0), and the globals are reloaded afterwards, so callbacks
 * may write registers.  vCPU threads run translated code inside an RCU
 * read-side critical section, which protects the hook list.
 */
static void QEMU_DISABLE_CFI __attribute__((noinline))
tcg_hook_helper(CPUArchState *env, void *opaque)
{
    TCGHookSite *s = opaque;
    CPUState *cpu = env_cpu(env);
    TCGHook *h;

    cpu->tcg_hook_ra = (uintptr_t)__builtin_return_address(0);
    QLIST_FOREACH_RCU(h, &s->hooks, next) {
        h->fn(cpu, s->pc, h->opaque);
    }
    cpu->tcg_hook_ra = 0;
}

/* A guard that could not be compiled: evaluated here, from the gdbstub. */
static bool guard_holds(CPUState *cpu, const TCGHookGuard *g)
{
    uint64_t v = 0;

    if (g->kind == TCG_HOOK_GUARD_HOST) {
        v = g->size == 8 ? qatomic_read((uint64_t *)g->host)
                         : qatomic_read((uint32_t *)g->host);
    } else {
        ptrdiff_t off;
        unsigned sz;

        if (tcg_register_location(cpu, g->regno, &off, &sz)) {
            const void *p = (uint8_t *)cpu_env(cpu) + off;
            v = sz == 8 ? *(uint64_t *)p : sz == 4 ? *(uint32_t *)p
              : sz == 2 ? *(uint16_t *)p : *(uint8_t *)p;
        } else {
            return true;                /* cannot tell: let the callback decide */
        }
    }
    if (g->size == 4) {
        v = is_signed_cond(g->cond) ? (uint64_t)(int64_t)(int32_t)v
                                        : (uint32_t)v;
    }
    if (g->mask) {
        v &= g->mask;
    }
    switch (g->cond) {
    case TCG_COND_EQ:  return v == g->value;
    case TCG_COND_NE:  return v != g->value;
    case TCG_COND_LTU: return v < g->value;
    case TCG_COND_LEU: return v <= g->value;
    case TCG_COND_GTU: return v > g->value;
    case TCG_COND_GEU: return v >= g->value;
    case TCG_COND_LT:  return (int64_t)v < (int64_t)g->value;
    case TCG_COND_LE:  return (int64_t)v <= (int64_t)g->value;
    case TCG_COND_GT:  return (int64_t)v > (int64_t)g->value;
    case TCG_COND_GE:  return (int64_t)v >= (int64_t)g->value;
    default:           return true;
    }
}

static void QEMU_DISABLE_CFI __attribute__((noinline))
tcg_hook_one_helper(CPUArchState *env, void *opaque)
{
    TCGHook *h = opaque;
    CPUState *cpu = env_cpu(env);

    if (!qatomic_read(&h->live)) {
        return;
    }
    for (unsigned i = 0; i < h->nguards; i++) {
        if (h->guards[i].compiled == false && !guard_holds(cpu, &h->guards[i])) {
            return;
        }
    }
    cpu->tcg_hook_ra = (uintptr_t)__builtin_return_address(0);
    h->fn(cpu, h->site->pc, h->opaque);
    cpu->tcg_hook_ra = 0;
}

static TCGHelperInfo tcg_hook_one_info = {
    .func = tcg_hook_one_helper,
    .name = "tcg_hook_one",
    .flags = 0,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) |
                dh_typemask(ptr, 2),
};

/* The TCG global holding the guest register at @off in the CPU state, if any. */
static TCGTemp *find_global(ptrdiff_t off)
{
    TCGTemp *env = tcgv_ptr_temp(tcg_env);

    for (int i = 0; i < tcg_ctx->nb_globals; i++) {
        TCGTemp *ts = &tcg_ctx->temps[i];
        if (ts->kind == TEMP_GLOBAL && ts->mem_base == env &&
            ts->mem_offset == off && !ts->indirect_reg &&
            ts->temp_subindex == 0) {
            return ts;
        }
    }
    return NULL;
}

/*
 * Emit "if (!guard) goto skip".  Returns false, emitting nothing, if the
 * guard cannot be compiled; the helper evaluates it then.
 */
static bool gen_guard(TCGHookGuard *g, TCGLabel *skip)
{
    TCGv_i64 v = tcg_temp_ebb_new_i64();
    uint64_t value = g->value;

    if (g->kind == TCG_HOOK_GUARD_HOST) {
        TCGv_ptr p = tcg_constant_ptr(g->host);
        if (g->size == 8) {
            tcg_gen_ld_i64(v, p, 0);
        } else {
            tcg_gen_ld32u_i64(v, p, 0);
        }
    } else {
        ptrdiff_t off;
        unsigned sz;
        TCGTemp *ts;

        if (!current_cpu ||
            !tcg_register_location(current_cpu, g->regno, &off, &sz) ||
            (sz != 4 && sz != 8)) {
            tcg_temp_free_i64(v);
            return false;
        }
        ts = find_global(off);
        if (ts && ts->base_type == TCG_TYPE_I64) {
            tcg_gen_mov_i64(v, temp_tcgv_i64(ts));
        } else if (ts && ts->base_type == TCG_TYPE_I32) {
            tcg_gen_extu_i32_i64(v, temp_tcgv_i32(ts));
        } else if (sz == 8) {
            /* not a TCG global: the CPU state is always current */
            tcg_gen_ld_i64(v, tcg_env, off);
        } else {
            tcg_gen_ld32u_i64(v, tcg_env, off);
        }
    }
    if (g->size == 4) {
        if (is_signed_cond(g->cond)) {
            tcg_gen_ext32s_i64(v, v);
            value = (int64_t)(int32_t)value;
        } else {
            tcg_gen_ext32u_i64(v, v);
            value = (uint32_t)value;
        }
    }
    if (g->mask) {
        tcg_gen_andi_i64(v, v, g->mask);
    }
    tcg_gen_brcondi_i64(tcg_invert_cond(g->cond), v, value, skip);
    tcg_temp_free_i64(v);
    return true;
}

static void gen_guarded(TCGHook *h)
{
    TCGv_ptr hp = tcg_constant_ptr(h);
    TCGv_i32 live = tcg_temp_ebb_new_i32();
    TCGLabel *skip = gen_new_label();

    tcg_gen_ld_i32(live, hp, offsetof(TCGHook, live));
    tcg_gen_brcondi_i32(TCG_COND_EQ, live, 0, skip);
    tcg_temp_free_i32(live);
    for (unsigned i = 0; i < h->nguards; i++) {
        /* only the translating thread writes this, before the code runs */
        h->guards[i].compiled = gen_guard(&h->guards[i], skip);
    }
    tcg_gen_call2(tcg_hook_one_helper, &tcg_hook_one_info, NULL,
                  tcgv_ptr_temp(tcg_env), tcgv_ptr_temp(hp));
    gen_set_label(skip);
}

static TCGHelperInfo tcg_hook_info = {
    .func = tcg_hook_helper,
    .name = "tcg_hook",
    .flags = 0,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) |
                dh_typemask(ptr, 2),
};

void tcg_hooks_gen(DisasContextBase *db)
{
    TCGHookSite *s;
    TCGHook *h;
    TCGv_ptr site;
    TCGv_i32 n;
    TCGLabel *skip;

    WITH_RCU_READ_LOCK_GUARD() {
        s = site_lookup(db->pc_next);
    }
    if (!s) {
        return;
    }
    WITH_RCU_READ_LOCK_GUARD() {
        QLIST_FOREACH_RCU(h, &s->ghooks, next) {
            gen_guarded(h);
        }
    }
    site = tcg_constant_ptr(s);
    n = tcg_temp_ebb_new_i32();
    skip = gen_new_label();
    tcg_gen_ld_i32(n, site, offsetof(TCGHookSite, nhooks));
    tcg_gen_brcondi_i32(TCG_COND_EQ, n, 0, skip);
    tcg_temp_free_i32(n);
    tcg_gen_call2(tcg_hook_helper, &tcg_hook_info, NULL,
                  tcgv_ptr_temp(tcg_env), tcgv_ptr_temp(site));
    gen_set_label(skip);
}
