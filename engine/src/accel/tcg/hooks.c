/*
 * TCG address hooks: call a host function before a guest instruction.
 *
 * Sites (one per hooked address) are kept per 1 KiB chunk of guest
 * virtual addresses: a QHT of chunks, each with an array of its sites
 * sorted by pc, replaced as a whole (RCU) when a site is added.  The
 * translator reads them lock-free under RCU, once per chunk of a block
 * (tcg_hooks_gen(), hooks-internal.h), and before that tests the page
 * bitmap, so code on pages without sites costs a compare per instruction
 * on every target.  A site is never freed: translated code embeds its
 * address.  Its hooks are an RCU list, and generated code tests the site's
 * hook count before calling, so removing the last hook leaves a load and a
 * branch until the block is retranslated.
 *
 * A site without hooks is dead.  A TB flush removes all translated code, so
 * it reclaims the dead sites (freed after a grace period) and rebuilds the
 * page bitmap from the live ones: pages that lost their hooks translate
 * with CF_PCREL again.  Without that, per-process hooks over a long run
 * (an entry point hooked in every program under ASLR) would grow both
 * without bound.  Once dead sites outnumber live ones and pass
 * DEAD_SITES_FLUSH, or the graveyard of removed guarded hooks passes
 * GRAVEYARD_FLUSH, a flush is requested for that purpose.
 *
 * CF_PCREL stays on, except on the guest virtual pages that hold a site:
 * a lookup there asks for a block translated for its virtual address
 * (tcg_hooks_cflags), and only such blocks get hook calls.  While hooks
 * are in use, a CF_PCREL block never extends past its first page
 * (translator_loop), so it cannot reach a hooked page by running on, and
 * a lookup on a hooked page never returns one, nor do blocks there chain
 * into one.  A PIE mapped at a new address on every exec keeps reusing
 * its translations unless its own pages are hooked.
 *
 * So a page that gains its first site needs no flush, though CF_PCREL
 * blocks for it stay cached: goto_tb only chains within the page a block
 * starts on, so the way into one is a lookup on that page, and the commit
 * (exclusive: every vCPU leaves cpu_exec) empties every vCPU's jump cache,
 * so each looks up again.  A lookup asks for the page's cflags only after
 * a jump-cache miss (tb_lookup), which keeps the check off the hit path.
 * That depends on same-page goto_tb, the page stop, the exclusive commit
 * and the emptied jump caches; keep all four.  hktest `late` tests it.
 *
 * New sites are invalidated in batches, in exclusive context: the first
 * batch flushes everything (blocks translated before hooks were in use
 * may cross pages), later ones invalidate only the blocks whose guest
 * range contains them.  To find those without scanning every block,
 * blocks linked without CF_PCREL that touch a marked page are indexed by
 * guest virtual page (4 KiB buckets) until the next flush; a block's
 * memory stays valid until then even after it is invalidated.  Other
 * blocks are not indexed, so the common path takes no lock.  When a
 * batch marks a page bit that was clear, blocks already linked on pages
 * with that bit are not in the index: if any vCPU translates without
 * CF_PCREL (mips, ppc, loongarch, ...), the commit walks all blocks once
 * and indexes those.  With CF_PCREL everywhere there are none: a block
 * on a page is translated without CF_PCREL only once its bit is set.
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
#include "system/tcg.h"
#include "qemu/timer.h"
#include "panda/events-internal.h"
#include "trace.h"

typedef struct TCGHookSite {
    vaddr pc;
    int32_t nhooks;                     /* read by generated code */
    bool dead;                          /* no hooks: writers only */
    QLIST_HEAD(, TCGHook) hooks;
    QLIST_HEAD(, TCGHook) ghooks;       /* guarded: code emitted per hook */
    struct rcu_head rcu;
} TCGHookSite;

/* A chunk's sites, sorted by pc.  Never changed once published. */
struct TCGHookSiteArray {
    struct rcu_head rcu;
    unsigned n;
    TCGHookSite *s[];
};

typedef struct TCGHookChunk {
    vaddr chunk;                        /* pc >> TCG_HOOKS_CHUNK_BITS */
    TCGHookSiteArray *sites;            /* RCU */
    struct rcu_head rcu;
} TCGHookChunk;

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
unsigned long tcg_hooks_pages[BITS_TO_LONGS(1 << TCG_HOOKS_PAGE_BITS)];
/*
 * Zero until the first hook: tcg_hooks_enabled is never cleared, so the
 * cache is never stale when tcg_hooks_gen() starts being called mid-block.
 */
__thread TCGHooksXlate tcg_hooks_xl;

static QemuMutex hooks_lock;            /* writers: sites, lists, pending */
static struct qht chunks;
static GArray *pending;                 /* new site pcs to invalidate */
static GArray *newbits;                 /* page bits set since the last commit */
static bool commit_queued;
static bool flushed;                    /* the first commit flushed */
static bool pages_pending;              /* sites inserted before the page size */
static GHashTable *marks;               /* pcs given to tcg_hooks_mark_pc() */
static unsigned nsites, ndead, nreclaims;
static bool reclaim_queued;

/* see the header comment */
#define DEAD_SITES_FLUSH 4096
#define GRAVEYARD_FLUSH  1024

/* invalidating more pcs than this at once flushes everything instead */
#define MAX_TARGETED 1024

static GPtrArray *graveyard;            /* removed guarded hooks */

/*
 * The block of the hook being called on this thread (tcg_hook_tb), and the
 * tcg_hook_ra it was set with: a callback that leaves by longjmp leaves them
 * behind, and tcg_hook_ra is then cleared (patch 0030) or set by someone
 * else (the syscall trap), never to the same return address.
 */
static __thread TranslationBlock *hook_tb;
static __thread uintptr_t hook_tb_ra;

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

static bool chunk_cmp(const void *a, const void *b)
{
    return ((const TCGHookChunk *)a)->chunk == ((const TCGHookChunk *)b)->chunk;
}

static bool chunk_key_cmp(const void *p, const void *userp)
{
    return ((const TCGHookChunk *)p)->chunk == *(const vaddr *)userp;
}

static uint32_t chunk_hash(vaddr chunk)
{
    return qemu_xxhash2(chunk);
}

static void __attribute__((constructor)) tcg_hooks_init(void)
{
    qemu_mutex_init(&hooks_lock);
    qht_init(&chunks, chunk_cmp, 64, QHT_MODE_AUTO_RESIZE);
    pending = g_array_new(false, false, sizeof(vaddr));
    newbits = g_array_new(false, false, sizeof(unsigned long));
    graveyard = g_ptr_array_new();
    marks = g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free, NULL);
    qemu_mutex_init(&index_lock);
    qemu_mutex_init(&regloc_lock);
    regloc = g_hash_table_new_full(g_bytes_hash, g_bytes_equal,
                                   (GDestroyNotify)g_bytes_unref, g_free);
    tb_index = g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free,
                                     (GDestroyNotify)g_ptr_array_unref);
}

static inline bool page_marked(vaddr pc)
{
    unsigned long b = tcg_hooks_page_bit(pc);

    return qatomic_read(&tcg_hooks_pages[BIT_WORD(b)]) & BIT_MASK(b);
}

static inline vaddr tb_last_byte(const TranslationBlock *tb)
{
    return tb->pc + (tb->size ? tb->size - 1 : 0);
}

/* With index_lock held. */
static void index_add(TranslationBlock *tb)
{
    uint64_t b = BUCKET(tb->pc), last = BUCKET(tb_last_byte(tb));

    for (; b <= last; b++) {
        GPtrArray *a = g_hash_table_lookup(tb_index, &b);
        if (!a) {
            a = g_ptr_array_new();
            g_hash_table_insert(tb_index, g_memdup2(&b, sizeof(b)), a);
        }
        g_ptr_array_add(a, tb);
    }
}

void tcg_hooks_tb_linked(TranslationBlock *tb)
{
    if (tb_cflags(tb) & CF_PCREL) {
        return;                         /* not keyed by virtual address */
    }
    /*
     * A block on pages whose bits are clear is found by the commit's
     * walk if one of them is marked later (see the header comment).
     */
    if (!page_marked(tb->pc) && !page_marked(tb_last_byte(tb))) {
        return;
    }
    qemu_mutex_lock(&index_lock);
    index_add(tb);
    qemu_mutex_unlock(&index_lock);
}

/* Freed after a grace period (panda_events_call_rcu: a hook removed before the machine is up
 * waits until it is). */
static void hook_free_rcu(struct rcu_head *head)
{
    g_free(container_of(head, TCGHook, rcu));
}

static void reclaim(void);

void tcg_hooks_tb_flush(void)
{
    qemu_mutex_lock(&index_lock);
    g_hash_table_remove_all(tb_index);
    qemu_mutex_unlock(&index_lock);

    /* no translated code refers to removed guarded hooks or to sites any more */
    qemu_mutex_lock(&hooks_lock);
    for (guint i = 0; i < graveyard->len; i++) {
        TCGHook *h = g_ptr_array_index(graveyard, i);
        g_free(h->guards);
        panda_events_call_rcu(&h->rcu, hook_free_rcu);
    }
    g_ptr_array_set_size(graveyard, 0);
    reclaim();
    reclaim_queued = false;
    qemu_mutex_unlock(&hooks_lock);
}

static TCGHookChunk *chunk_lookup(vaddr chunk)
{
    return qht_lookup_custom(&chunks, &chunk, chunk_hash(chunk), chunk_key_cmp);
}

static TCGHookSite *array_find(const TCGHookSiteArray *a, vaddr pc)
{
    unsigned lo = 0, hi = a ? a->n : 0;

    while (lo < hi) {
        unsigned mid = (lo + hi) / 2;

        if (a->s[mid]->pc < pc) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return a && lo < a->n && a->s[lo]->pc == pc ? a->s[lo] : NULL;
}

/* Under RCU. */
static TCGHookSite *site_lookup(vaddr pc)
{
    TCGHookChunk *c = chunk_lookup(pc >> TCG_HOOKS_CHUNK_BITS);

    return c ? array_find(qatomic_rcu_read(&c->sites), pc) : NULL;
}

static void array_free_rcu(struct rcu_head *head)
{
    g_free(container_of(head, TCGHookSiteArray, rcu));
}

/* Publish @a as @c's sites.  With hooks_lock held. */
static void chunk_publish(TCGHookChunk *c, TCGHookSiteArray *a)
{
    TCGHookSiteArray *old = c->sites;

    qatomic_rcu_set(&c->sites, a);
    if (old) {
        panda_events_call_rcu(&old->rcu, array_free_rcu);
    }
}

/* Add a new site to its chunk.  With hooks_lock held. */
static void site_add(TCGHookSite *s)
{
    vaddr key = s->pc >> TCG_HOOKS_CHUNK_BITS;
    TCGHookChunk *c = chunk_lookup(key);
    TCGHookSiteArray *old, *a;
    unsigned n, at;

    if (!c) {
        c = g_new0(TCGHookChunk, 1);
        c->chunk = key;
        qht_insert(&chunks, c, chunk_hash(key), NULL);
    }
    old = c->sites;
    n = old ? old->n : 0;
    a = g_malloc(sizeof(*a) + (n + 1) * sizeof(a->s[0]));
    a->n = n + 1;
    for (at = 0; at < n && old->s[at]->pc < s->pc; at++) {
        a->s[at] = old->s[at];
    }
    a->s[at] = s;
    for (unsigned i = at; i < n; i++) {
        a->s[i + 1] = old->s[i];
    }
    chunk_publish(c, a);
}

typedef void (*SiteFn)(TCGHookSite *s, void *opaque);

static void chunk_iter(void *p, uint32_t h, void *up)
{
    TCGHookChunk *c = p;
    void **args = up;
    TCGHookSiteArray *a = c->sites;

    for (unsigned i = 0; a && i < a->n; i++) {
        ((SiteFn)args[0])(a->s[i], args[1]);
    }
}

/* Call @fn on every site.  With hooks_lock held. */
static void for_each_site(SiteFn fn, void *opaque)
{
    void *args[2] = { (void *)fn, opaque };

    qht_iter(&chunks, chunk_iter, args);
}

static void site_free_rcu(struct rcu_head *head)
{
    g_free(container_of(head, TCGHookSite, rcu));
}

static void chunk_free_rcu(struct rcu_head *head)
{
    g_free(container_of(head, TCGHookChunk, rcu));
}

static void collect_chunk(void *p, uint32_t h, void *up)
{
    g_ptr_array_add(up, p);
}

static void mark_page(vaddr pc);

static void mark_site_page(TCGHookSite *s, void *unused)
{
    mark_page(s->pc);
}

/* Mark the pages of every site and of tcg_hooks_mark_pc()'s pcs. */
static void mark_all(void)
{
    GHashTableIter it;
    gpointer k;

    for_each_site(mark_site_page, NULL);
    g_hash_table_iter_init(&it, marks);
    while (g_hash_table_iter_next(&it, &k, NULL)) {
        mark_page(*(vaddr *)k);
    }
}

/*
 * Drop the dead sites and rebuild the page bitmap.  With hooks_lock held,
 * when no translated code refers to a site: from a TB flush (exclusive or
 * serial context), or before any vCPU exists.
 */
static void reclaim(void)
{
    g_autoptr(GPtrArray) all = g_ptr_array_new();

    if (!ndead) {
        return;
    }
    qht_iter(&chunks, collect_chunk, all);
    for (guint i = 0; i < all->len; i++) {
        TCGHookChunk *c = g_ptr_array_index(all, i);
        TCGHookSiteArray *old = c->sites, *a;
        unsigned kept = 0;

        a = g_malloc(sizeof(*a) + old->n * sizeof(a->s[0]));
        for (unsigned j = 0; j < old->n; j++) {
            TCGHookSite *st = old->s[j];

            if (st->dead) {
                panda_events_call_rcu(&st->rcu, site_free_rcu);
                nsites--;
            } else {
                a->s[kept++] = st;
            }
        }
        a->n = kept;
        if (kept == old->n) {
            g_free(a);
        } else if (kept) {
            chunk_publish(c, a);
        } else {
            g_free(a);
            qht_remove(&chunks, c, chunk_hash(c->chunk));
            chunk_publish(c, NULL);
            panda_events_call_rcu(&c->rcu, chunk_free_rcu);
        }
    }
    ndead = 0;
    nreclaims++;
    if (target_page.decided) {
        for (size_t i = 0; i < ARRAY_SIZE(tcg_hooks_pages); i++) {
            qatomic_set(&tcg_hooks_pages[i], 0);
        }
        mark_all();
    }
    g_array_set_size(newbits, 0);       /* nothing is translated */
}

static void reclaim_work(CPUState *cpu, run_on_cpu_data unused)
{
    /* tcg_hooks_tb_flush() reclaims */
    tb_flush__exclusive_or_serial();
}

/* Too many dead sites or removed guarded hooks: flush.  With hooks_lock held. */
static void maybe_reclaim(void)
{
    CPUState *cpu = current_cpu ? current_cpu : first_cpu;

    if (reclaim_queued ||
        !((ndead >= DEAD_SITES_FLUSH && ndead * 2 > nsites) ||
          graveyard->len >= GRAVEYARD_FLUSH)) {
        return;
    }
    if (!cpu) {
        reclaim();                      /* nothing translated yet */
        return;
    }
    if (!tcg_enabled()) {
        return;
    }
    reclaim_queued = true;
    async_safe_run_on_cpu(cpu, reclaim_work, RUN_ON_CPU_NULL);
}

/* @s lost or gained its last hook.  With hooks_lock held. */
static void site_update_dead(TCGHookSite *s)
{
    bool dead = !s->nhooks && QLIST_EMPTY(&s->ghooks);

    if (dead != s->dead) {
        s->dead = dead;
        ndead += dead ? 1 : -1;
    }
}

void tcg_hooks_stats(TCGHooksStats *st)
{
    qemu_mutex_lock(&hooks_lock);
    st->sites = nsites;
    st->dead_sites = ndead;
    st->graveyard = graveyard->len;
    st->reclaims = nreclaims;
    qemu_mutex_unlock(&hooks_lock);
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

static int bit_cmp(gconstpointer a, gconstpointer b)
{
    unsigned long x = *(const unsigned long *)a, y = *(const unsigned long *)b;

    return x < y ? -1 : x > y;
}

static bool bit_in(GArray *bits, vaddr pc)
{
    unsigned long b = tcg_hooks_page_bit(pc);

    return bsearch(&b, bits->data, bits->len, sizeof(unsigned long), bit_cmp);
}

/*
 * tcg_tb_foreach() callback: index the live blocks without CF_PCREL that
 * touch a page whose bit was just set.  With index_lock held.
 */
static gboolean index_newly_marked(gpointer key, gpointer value, gpointer data)
{
    TranslationBlock *tb = value;
    GArray *bits = data;

    if (!(tb_cflags(tb) & (CF_PCREL | CF_INVALID)) &&
        (bit_in(bits, tb->pc) || bit_in(bits, tb_last_byte(tb)))) {
        index_add(tb);
    }
    return false;
}

static void hooks_commit(CPUState *cpu, run_on_cpu_data unused)
{
    CPUState *c;
    g_autoptr(GArray) pcs = NULL;
    g_autoptr(GArray) bits = NULL;
    bool flush, any_abs = false;
    int64_t t0 = get_clock();
    unsigned scanned = 0, invalidated = 0;

    qemu_mutex_lock(&hooks_lock);
    pcs = pending;
    pending = g_array_new(false, false, sizeof(vaddr));
    bits = newbits;
    newbits = g_array_new(false, false, sizeof(unsigned long));
    commit_queued = false;
    if (!tcg_enabled()) {
        /*
         * A TCG+KVM build under -accel kvm: no translated code to
         * invalidate, and tb_flush() would abort.  Callers may insert
         * hooks before they know the accelerator; the sites stay,
         * unused.
         */
        qemu_mutex_unlock(&hooks_lock);
        return;
    }
    flush = !flushed || pcs->len > MAX_TARGETED;
    flushed = true;
    qemu_mutex_unlock(&hooks_lock);

    CPU_FOREACH(c) {
        tcg_cflags_set(c, CF_HOOKS);
        if (c->cflags_next_tb != -1) {
            c->cflags_next_tb |= CF_HOOKS;
        }
        any_abs |= !tcg_cflags_has(c, CF_PCREL);
    }

    if (flush) {
        tb_flush__exclusive_or_serial();
    } else if (pcs->len) {
        g_autoptr(GPtrArray) tbs = g_ptr_array_new();
        g_autoptr(GHashTable) seen = g_hash_table_new(NULL, NULL);

        /* a jump-cache hit is not checked against the hooked pages */
        CPU_FOREACH(c) {
            tcg_flush_jmp_cache(c);
        }

        qemu_mutex_lock(&index_lock);
        if (bits->len && any_abs) {
            g_array_sort(bits, bit_cmp);
            tcg_tb_foreach(index_newly_marked, bits);
            scanned += tcg_nb_tbs();
        }
        for (guint i = 0; i < pcs->len; i++) {
            scanned += collect(g_array_index(pcs, vaddr, i), tbs);
        }
        qemu_mutex_unlock(&index_lock);
        /* a block linked while its page bit was being set may be indexed twice */
        for (guint i = 0; i < tbs->len; i++) {
            TranslationBlock *tb = g_ptr_array_index(tbs, i);

            if (g_hash_table_add(seen, tb)) {
                tb_phys_invalidate(tb, -1);
                invalidated++;
            }
        }
    }
    trace_tcg_hooks_commit(flush, pcs->len, scanned, invalidated,
                           get_clock() - t0);
}

/* Called with hooks_lock held. */
static void queue_commit(vaddr pc)
{
    CPUState *cpu = current_cpu ? current_cpu : first_cpu;

    if (!cpu) {
        /* No vCPU yet: no translated code either. */
        flushed = true;
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

/* Set @pc's page bit; remember it for the next commit if it was clear. */
static void mark_page(vaddr pc)
{
    unsigned long b = tcg_hooks_page_bit(pc);
    unsigned long old = qatomic_fetch_or(&tcg_hooks_pages[BIT_WORD(b)],
                                         BIT_MASK(b));

    if (!(old & BIT_MASK(b))) {
        g_array_append_val(newbits, b);
    }
}

/*
 * An embedder may insert hooks before the target page size is decided;
 * it is by the time a vCPU exists, and nothing is translated before.
 */
void tcg_hooks_vcpu_init(CPUState *cpu)
{
    qemu_mutex_lock(&hooks_lock);
    if (pages_pending) {
        mark_all();
        g_array_set_size(newbits, 0);   /* nothing was translated yet */
        pages_pending = false;
    }
    if (tcg_hooks_enabled) {
        tcg_cflags_set(cpu, CF_HOOKS);
    }
    qemu_mutex_unlock(&hooks_lock);
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
        if (target_page.decided) {
            mark_page(pc);
        } else {
            pages_pending = true;       /* marked in tcg_hooks_vcpu_init() */
        }
        s = g_new0(TCGHookSite, 1);
        s->pc = pc;
        QLIST_INIT(&s->hooks);
        QLIST_INIT(&s->ghooks);
        site_add(s);
        nsites++;
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
    site_update_dead(s);
    qemu_mutex_unlock(&hooks_lock);
    return h;
}

void tcg_hooks_mark_pc(vaddr pc)
{
    qemu_mutex_lock(&hooks_lock);
    qatomic_set(&tcg_hooks_enabled, true);
    g_hash_table_add(marks, g_memdup2(&pc, sizeof(pc)));
    if (target_page.decided) {
        mark_page(pc);
    } else {
        /* no vCPU yet, so nothing translated: mark it when the page size is known */
        pages_pending = true;
    }
    queue_commit(pc);
    qemu_mutex_unlock(&hooks_lock);
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
        site_update_dead(h->site);
        maybe_reclaim();
        qemu_mutex_unlock(&hooks_lock);
        return;
    }
    qatomic_dec(&h->site->nhooks);
    site_update_dead(h->site);
    maybe_reclaim();
    qemu_mutex_unlock(&hooks_lock);
    panda_events_call_rcu(&h->rcu, hook_free_rcu);
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
    hook_tb = NULL;
    cpu_loop_exit(cpu);
}

TranslationBlock *tcg_hook_tb(CPUState *cpu)
{
    uintptr_t ra = cpu->tcg_hook_ra;

    return ra && ra == hook_tb_ra ? hook_tb : NULL;
}

/*
 * The helper.  Entered from translated code with every TCG global saved
 * (call flags 0), and the globals are reloaded afterwards, so callbacks
 * may write registers.  vCPU threads run translated code inside an RCU
 * read-side critical section, which protects the hook list.  @tb is the
 * block the call was emitted in, for tcg_hook_tb().
 */
static void QEMU_DISABLE_CFI __attribute__((noinline))
tcg_hook_helper(CPUArchState *env, void *opaque, void *tb)
{
    TCGHookSite *s = opaque;
    CPUState *cpu = env_cpu(env);
    TCGHook *h;

    cpu->tcg_hook_ra = (uintptr_t)__builtin_return_address(0);
    hook_tb = tb;
    hook_tb_ra = cpu->tcg_hook_ra;
    QLIST_FOREACH_RCU(h, &s->hooks, next) {
        h->fn(cpu, s->pc, h->opaque);
    }
    cpu->tcg_hook_ra = 0;
    hook_tb = NULL;
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
tcg_hook_one_helper(CPUArchState *env, void *opaque, void *tb)
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
    hook_tb = tb;
    hook_tb_ra = cpu->tcg_hook_ra;
    h->fn(cpu, h->site->pc, h->opaque);
    cpu->tcg_hook_ra = 0;
    hook_tb = NULL;
}

static TCGHelperInfo tcg_hook_one_info = {
    .func = tcg_hook_one_helper,
    .name = "tcg_hook_one",
    .flags = 0,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) |
                dh_typemask(ptr, 2) | dh_typemask(ptr, 3),
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

static void gen_guarded(TCGHook *h, TranslationBlock *tb)
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
    tcg_gen_call3(tcg_hook_one_helper, &tcg_hook_one_info, NULL,
                  tcgv_ptr_temp(tcg_env), tcgv_ptr_temp(hp),
                  tcgv_ptr_temp(tcg_constant_ptr(tb)));
    gen_set_label(skip);
}

static TCGHelperInfo tcg_hook_info = {
    .func = tcg_hook_helper,
    .name = "tcg_hook",
    .flags = 0,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) |
                dh_typemask(ptr, 2) | dh_typemask(ptr, 3),
};

/*
 * A block starts or runs onto a new chunk: find its sites, if its page is
 * marked.  Translation runs inside an RCU read-side critical section
 * (cpu_exec, cpu_exec_step_atomic), which keeps the array alive while the
 * block is translated.  A site added meanwhile reaches this block through
 * the commit: its page bit is set before the site is published, and the
 * block is indexed when linked or found by the commit's walk.
 */
void tcg_hooks_gen_chunk(DisasContextBase *db)
{
    vaddr pc = db->pc_next;
    TCGHookChunk *c = NULL;

    tcg_hooks_xl.chunk = pc >> TCG_HOOKS_CHUNK_BITS;
    if (page_marked(pc)) {
        c = chunk_lookup(tcg_hooks_xl.chunk);
    }
    tcg_hooks_xl.sites = c ? qatomic_rcu_read(&c->sites) : NULL;
}

void tcg_hooks_gen_insn(DisasContextBase *db)
{
    TCGHookSite *s = array_find(tcg_hooks_xl.sites, db->pc_next);
    TCGHook *h;
    TCGv_ptr site;
    TCGv_i32 n;
    TCGLabel *skip;

    if (!s) {
        return;
    }
    WITH_RCU_READ_LOCK_GUARD() {
        QLIST_FOREACH_RCU(h, &s->ghooks, next) {
            gen_guarded(h, db->tb);
        }
    }
    site = tcg_constant_ptr(s);
    n = tcg_temp_ebb_new_i32();
    skip = gen_new_label();
    tcg_gen_ld_i32(n, site, offsetof(TCGHookSite, nhooks));
    tcg_gen_brcondi_i32(TCG_COND_EQ, n, 0, skip);
    tcg_temp_free_i32(n);
    tcg_gen_call3(tcg_hook_helper, &tcg_hook_info, NULL,
                  tcgv_ptr_temp(tcg_env), tcgv_ptr_temp(site),
                  tcgv_ptr_temp(tcg_constant_ptr(db->tb)));
    gen_set_label(skip);
}
