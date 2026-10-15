/*
 * Penguin syscall hooks at the trap (system mode, TCG)
 *
 * igloo_driver's syscall hook model on QEMU's syscall trap events: see
 * include/system/penguin-syscalls.h.
 *
 * Writers (registration, names, layout) change the state under psc_lock
 * and mark it dirty; the next trap rebuilds the dispatch table, which the
 * vCPUs read under RCU.  While the state is dirty every trap reaches the
 * engine, so a change can never wait for a trap on a number nobody watches.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qemu/lockable.h"
#include "qemu/rcu.h"
#include "qemu/atomic.h"
#include "qemu/bswap.h"
#include "qemu/cutils.h"
#include "qemu/target-info.h"
#include "qemu/target-info-impl.h"
#include "hw/core/cpu.h"
#include "exec/target_page.h"
#include "exec/gdbstub.h"
#include "system/tcg.h"
#include "system/syscall-trap.h"
#include "system/penguin-syscalls.h"
#include "system/penguin-osi.h"
#ifdef CONFIG_TCG
#include "accel/tcg/hooks.h"
#endif

#define PSC_ARGS    6
#define PSC_MATCHES 64          /* hooks one event can reach */
#define PSC_STR_MAX 4096        /* igloo's MAX_SAFE_ALLOC */
#define PSC_ERESTARTSYS 512
#define PSC_TRIES   2           /* faults on one page without progress */
#define PSC_FAULTS  32          /* faults per dispatch */
#define PSC_BAD     8           /* pages the guest refused, per dispatch */

typedef struct PHook {
    struct rcu_head rcu;
    uint64_t id;
    bool dead;
    struct penguin_sc_hook h;
    char norm[48];
    char *pat[PSC_ARGS + 2];    /* the filters' patterns; then retval's, exe's */
} PHook;

typedef struct PList {
    unsigned n;
    PHook *h[];
} PList;

typedef struct PNum {
    const char *name;           /* interned, or NULL */
    int8_t nargs;
    bool noret;                 /* never returns to the trap */
    PList *hooks;
} PNum;

typedef struct PTable {
    struct rcu_head rcu;
    struct penguin_sc_layout layout;
    PList *all, *unknown;
    PList *hooks;               /* every live hook, by id */
    PNum *nums[SYSCALL_ABI_COUNT];
    GPtrArray *lists;           /* the PLists the PNums share */
} PTable;

typedef struct PName {
    const char *name;
    int8_t nargs;
} PName;

static QemuMutex psc_lock;      /* writers */
static QemuMutex psc_boot_lock;
static GPtrArray *psc_hooks;    /* PHook *, registration order */
static PName *psc_names[SYSCALL_ABI_COUNT];
static GHashTable *psc_interned;
static struct penguin_sc_layout psc_layout = {
    .task_pid = -1, .task_comm = -1, .task_start_time = -1,
    .task_nsproxy = -1, .nsproxy_uts_ns = -1,
};
static uint64_t psc_next_id;
static bool psc_consumer;

static PTable *psc_table;       /* RCU */
static bool psc_dirty;
static bool psc_booted;
static bool psc_scope;

static penguin_syscall_cb_t psc_cb;
static void *psc_cb_opaque;
static penguin_syscall_boot_cb_t psc_boot_cb;
static void *psc_boot_opaque;

static uint64_t psc_enters, psc_events, psc_commands, psc_refused, psc_kept;
static uint64_t psc_reaped, psc_injected;
static bool psc_exec_hooked;    /* a successful exec drops the task's calls */
static uint64_t psc_next_call;
static bool psc_guarded;        /* faults may be raised */

/* faults raised and not resolved yet; the guard (below) looks them up */
typedef struct PFault {
    struct PRun *r;
    uint64_t page;
    bool refused;
} PFault;

static QemuMutex psc_fault_lock;
static GArray *psc_out;         /* PFault */

static void __attribute__((constructor)) psc_init(void)
{
    qemu_mutex_init(&psc_lock);
    qemu_mutex_init(&psc_boot_lock);
    qemu_mutex_init(&psc_fault_lock);
    psc_out = g_array_new(false, false, sizeof(PFault));
    psc_hooks = g_ptr_array_new();
    psc_interned = g_hash_table_new(g_str_hash, g_str_equal);
}

/* igloo's normalize_syscall_name */
static const char *psc_normalize(const char *n)
{
    static const char *const prefixes[] = {
        "sys_", "compat_sys_", "arm64_sys_", "riscv_sys_",
    };

    while (*n == '_') {
        n++;
    }
    for (int i = 0; i < ARRAY_SIZE(prefixes); i++) {
        if (g_str_has_prefix(n, prefixes[i])) {
            return n + strlen(prefixes[i]);
        }
    }
    return n;
}

static bool psc_noret(const char *name)
{
    /* exit never returns; sigreturn returns to the interrupted context */
    return !strcmp(name, "exit") || !strcmp(name, "exit_group") ||
           !strcmp(name, "rt_sigreturn") || !strcmp(name, "sigreturn");
}

/* the dispatch table */

static PList *psc_list(GPtrArray *a)
{
    PList *l;

    if (!a || !a->len) {
        return NULL;
    }
    l = g_malloc(sizeof(PList) + a->len * sizeof(PHook *));
    l->n = a->len;
    memcpy(l->h, a->pdata, a->len * sizeof(PHook *));
    return l;
}

static void psc_table_free(PTable *t)
{
    g_free(t->all);
    g_free(t->unknown);
    g_free(t->hooks);
    for (int abi = 0; abi < SYSCALL_ABI_COUNT; abi++) {
        g_free(t->nums[abi]);
    }
    g_ptr_array_free(t->lists, true);
    g_free(t);
}

static void psc_hook_free(PHook *h)
{
    for (int i = 0; i <= PSC_ARGS + 1; i++) {
        g_free(h->pat[i]);
    }
    g_free(h);
}

/* psc_lock held */
static void psc_rebuild(void)
{
    PTable *t = g_new0(PTable, 1), *old;
    g_autoptr(GPtrArray) all = g_ptr_array_new();
    g_autoptr(GPtrArray) unknown = g_ptr_array_new();
    g_autoptr(GPtrArray) live = g_ptr_array_new();
    g_autoptr(GHashTable) byname = g_hash_table_new_full(
        g_str_hash, g_str_equal, NULL, (GDestroyNotify)g_ptr_array_unref);
    g_autoptr(GHashTable) shared = g_hash_table_new(NULL, NULL);

    t->layout = psc_layout;
    t->lists = g_ptr_array_new_with_free_func(g_free);
    for (unsigned i = 0; i < psc_hooks->len;) {
        PHook *h = g_ptr_array_index(psc_hooks, i);

        if (h->dead) {
            g_ptr_array_remove_index(psc_hooks, i);
            call_rcu(h, psc_hook_free, rcu);
            continue;
        }
        i++;
        g_ptr_array_add(live, h);
        if (!h->h.enabled || !(h->h.on_enter || h->h.on_return)) {
            continue;
        }
        if (h->h.on_all) {
            g_ptr_array_add(all, h);
        } else if (h->h.on_unknown) {
            g_ptr_array_add(unknown, h);
        } else if (h->norm[0]) {
            GPtrArray *a = g_hash_table_lookup(byname, h->norm);
            if (!a) {
                a = g_ptr_array_new();
                g_hash_table_insert(byname, h->norm, a);
            }
            g_ptr_array_add(a, h);
        }
    }
    t->all = psc_list(all);
    t->unknown = psc_list(unknown);
    t->hooks = psc_list(live);

    for (int abi = 0; abi < SYSCALL_ABI_COUNT; abi++) {
        const PName *nm = psc_names[abi];
        PNum *nums = NULL;

        if (nm) {
            nums = t->nums[abi] = g_new0(PNum, SYSCALL_TRAP_NUMS);
            for (int nr = 0; nr < SYSCALL_TRAP_NUMS; nr++) {
                GPtrArray *a;

                if (!nm[nr].name) {
                    continue;
                }
                nums[nr].name = nm[nr].name;
                nums[nr].nargs = nm[nr].nargs;
                nums[nr].noret = psc_noret(nm[nr].name);
                a = g_hash_table_lookup(byname, nm[nr].name);
                if (a) {
                    PList *l = g_hash_table_lookup(shared, a);
                    if (!l) {
                        l = psc_list(a);
                        g_ptr_array_add(t->lists, l);
                        g_hash_table_insert(shared, a, l);
                    }
                    nums[nr].hooks = l;
                }
            }
        }
        syscall_trap_watch(abi, -1, t->all || t->unknown ? SYSCALL_TRAP_ENTER : 0);
        for (int nr = 0; nr < SYSCALL_TRAP_NUMS; nr++) {
            syscall_trap_watch(abi, nr, nums && nums[nr].hooks ?
                                        SYSCALL_TRAP_ENTER : 0);
        }
    }

    old = psc_table;
    qatomic_rcu_set(&psc_table, t);
    if (old) {
        call_rcu(old, psc_table_free, rcu);
    }
}

/* psc_lock held: rebuild at the next trap, and until then see every trap */
static void psc_mark_dirty(void)
{
    qatomic_set(&psc_dirty, true);
    if (psc_consumer) {
        for (int abi = 0; abi < SYSCALL_ABI_COUNT; abi++) {
            syscall_trap_watch(abi, -1, SYSCALL_TRAP_ENTER);
        }
    }
}

static void psc_refresh(CPUState *cpu)
{
    if (!qatomic_read(&psc_booted)) {
        WITH_QEMU_LOCK_GUARD(&psc_boot_lock) {
            if (!psc_booted) {
                if (psc_boot_cb) {
                    psc_boot_cb(cpu, psc_boot_opaque);
                }
                qatomic_set(&psc_booted, true);
            }
        }
    }
    WITH_QEMU_LOCK_GUARD(&psc_lock) {
        if (psc_dirty) {
            qatomic_set(&psc_dirty, false);
            psc_rebuild();
        }
    }
}

/* one event's facts, read on demand */

enum { PSC_TASK = 1, PSC_PID = 2, PSC_COMM = 4, PSC_TIME = 8, PSC_SCOPE = 16 };
enum { PSC_NO, PSC_YES, PSC_FAULT };

typedef struct PCtx {
    CPUState *cpu;
    const PTable *t;
    SyscallTrap *sc;
    const PNum *pn;
    unsigned argc;
    bool is32;
    int64_t retval;             /* the return filters' value */
    unsigned have;
    uint64_t task, time;
    int64_t pid;
    bool scope;
    char comm[16];
    struct PRun *r;             /* the dispatch's run, if it has one */
    uint64_t fault_page;        /* the fault it needs */
    bool fault_write;
    bool at_enter;              /* at the call's entry: SYSCALL may run */
    bool inject;                /* the run's SYSCALL has to run first */
} PCtx;

static PCtx psc_ctx(CPUState *cpu, const PTable *t, SyscallTrap *sc)
{
    PCtx c = { .cpu = cpu, .t = t, .sc = sc, .argc = PSC_ARGS };

    if (t->nums[sc->abi] && sc->num >= 0 && sc->num < SYSCALL_TRAP_NUMS) {
        c.pn = &t->nums[sc->abi][sc->num];
        if (c.pn->name) {
            c.argc = c.pn->nargs;
        }
    }
    c.is32 = sc->abi == SYSCALL_ABI_I386 || sc->abi == SYSCALL_ABI_ARM ||
             sc->abi == SYSCALL_ABI_ARM_OABI || sc->abi == SYSCALL_ABI_PPC ||
             sc->abi == SYSCALL_ABI_RISCV32;
    return c;
}

static uint64_t psc_load(const uint8_t *b, unsigned n)
{
    if (target_big_endian()) {
        return n == 4 ? ldl_be_p(b) : ldq_be_p(b);
    }
    return n == 4 ? ldl_le_p(b) : ldq_le_p(b);
}

/* a pointer-sized or @n-byte guest value, or 0 */
static uint64_t psc_word(PCtx *c, uint64_t va, int off, unsigned n)
{
    uint8_t b[8];

    if (!va || off < 0 ||
        (!penguin_osi_kread(c->cpu, va + off, b, n) &&
         !syscall_trap_read(c->cpu, va + off, b, n))) {
        return 0;
    }
    return psc_load(b, n);
}

static uint64_t psc_ptr(PCtx *c, uint64_t va, int off)
{
    return psc_word(c, va, off, c->t->layout.ptr_size == 4 ? 4 : 8);
}

static bool psc_task(PCtx *c)
{
    const struct penguin_sc_layout *l = &c->t->layout;

    if (!(c->have & PSC_TASK)) {
        c->have |= PSC_TASK;
        c->task = 0;
        if (l->cur_mask) {
            c->task = psc_ptr(c, (c->sc->percpu & l->cur_mask) + l->cur_offset,
                              0);
        }
    }
    return c->task != 0;
}

static bool psc_pid(PCtx *c)
{
    if (!(c->have & PSC_PID)) {
        c->have |= PSC_PID;
        c->pid = -1;
        if (psc_task(c) && c->t->layout.task_pid >= 0) {
            c->pid = (int32_t)psc_word(c, c->task, c->t->layout.task_pid, 4);
        }
    }
    return c->pid >= 0;
}

static bool psc_comm(PCtx *c)
{
    if (!(c->have & PSC_COMM)) {
        c->have |= PSC_COMM;
        c->comm[0] = 0;
        if (!psc_task(c) || c->t->layout.task_comm < 0 ||
            !syscall_trap_read(c->cpu, c->task + c->t->layout.task_comm,
                               c->comm, sizeof(c->comm))) {
            c->comm[0] = 0;
            return false;
        }
    }
    return true;
}

static uint64_t psc_time(PCtx *c)
{
    if (!(c->have & PSC_TIME)) {
        c->have |= PSC_TIME;
        c->time = 0;
        if (psc_task(c) && c->t->layout.task_start_time >= 0) {
            c->time = psc_word(c, c->task, c->t->layout.task_start_time, 8);
        }
    }
    return c->time;
}

/* igloo_in_scope: outside the initial UTS namespace */
static bool psc_in_scope(PCtx *c)
{
    const struct penguin_sc_layout *l = &c->t->layout;

    if (!(c->have & PSC_SCOPE)) {
        c->have |= PSC_SCOPE;
        c->scope = false;
        if (psc_task(c) && l->task_nsproxy >= 0 && l->nsproxy_uts_ns >= 0) {
            uint64_t ns = psc_ptr(c, c->task, l->task_nsproxy);
            c->scope = ns && psc_ptr(c, ns, l->nsproxy_uts_ns) != l->init_uts_ns;
        }
    }
    return c->scope;
}

/*
 * A dispatch that waits for the guest: held with its call (in
 * SyscallTrap.data) while the task takes a fault, and resumed when the
 * task comes back to the trap.  Phases: choosing the entry hooks, running
 * them, choosing the return hooks (at the return, or at the entry after a
 * skip), running them.
 */
enum { PH_EMATCH, PH_ENTER, PH_RMATCH, PH_RETURN };

typedef struct PRun {
    int phase;
    bool want;                  /* return hooks may match: follow the call */
    bool skip;                  /* an entry hook skipped the call */
    unsigned n, i;              /* the chosen hooks, the one running */
    uint64_t ids[PSC_MATCHES];
    bool running;               /* hook i waits for its command's answer */
    struct penguin_sc_event ev; /* its event */
    struct penguin_sc_cmd cmd;  /* its command */
    uint8_t *wdata;             /* the bytes it writes */
    uint8_t *rbuf;              /* the bytes it read */
    size_t rcap;
    uint64_t args[PSC_ARGS];    /* the arguments as the hooks left them */
    uint64_t orig[PSC_ARGS];    /* the registers': the same call when back */
    int64_t ret;                /* the return value as the hooks left it */
    uint64_t fpage;             /* the page of the last fault */
    unsigned streak, total;     /* faults on it without progress, all */
    uint64_t bad[PSC_BAD];      /* pages the guest would not map */
    unsigned nbad;
} PRun;

/* filters */

/* may the dispatch raise a fault for @page? */
static bool psc_may_fault(PCtx *c, uint64_t page)
{
    const PRun *r = c->r;

    if (!qatomic_read(&psc_guarded)) {
        return false;
    }
    if (!r) {
        return true;
    }
    for (unsigned i = 0; i < r->nbad; i++) {
        if (r->bad[i] == page) {
            return false;
        }
    }
    return r->total < PSC_FAULTS &&
           !(page == r->fpage && r->streak >= PSC_TRIES);
}

/*
 * The user string at @va: its length up to @max; -1 if not mapped; or
 * PSC_FAULT's -2 if a fault for c->fault_page may map it.
 */
static long psc_str(PCtx *c, uint64_t va, char *buf, size_t max)
{
    size_t n = 0, pg = qemu_target_page_size();

    while (n < max) {
        size_t chunk = MIN(max - n, pg - ((va + n) & (pg - 1)));
        char *z;

        if (!syscall_trap_read(c->cpu, va + n, buf + n, chunk)) {
            uint64_t page = (va + n) & ~(uint64_t)(pg - 1);

            if (psc_may_fault(c, page)) {
                c->fault_page = page;
                c->fault_write = false;
                return -2;
            }
            return -1;
        }
        z = memchr(buf + n, 0, chunk);
        if (z) {
            return z - buf;
        }
        n += chunk;
    }
    return n;
}

/* igloo's check_str_*() */
static int psc_str_match(PCtx *c, const struct penguin_sc_filter *f,
                         const char *pat, uint64_t va)
{
    uint32_t plen = f->pattern_len;
    char buf[PSC_STR_MAX];
    size_t want = PSC_STR_MAX;
    long n;

    if (c->is32) {
        va = (uint32_t)va;
    }
    if (!va || !pat || !plen || plen >= PSC_STR_MAX) {
        return PSC_NO;
    }
    if (f->type == PENGUIN_SC_FILTER_STR_EXACT) {
        want = plen + 1;
    } else if (f->type == PENGUIN_SC_FILTER_STR_STARTSWITH) {
        want = plen;
    }
    n = psc_str(c, va, buf, want);
    if (n == -2) {
        return PSC_FAULT;
    }
    if (n < 0) {
        return PSC_NO;
    }
    switch (f->type) {
    case PENGUIN_SC_FILTER_STR_EXACT:
    case PENGUIN_SC_FILTER_STR_STARTSWITH:
        return n == plen && !memcmp(buf, pat, plen) ? PSC_YES : PSC_NO;
    case PENGUIN_SC_FILTER_STR_ENDSWITH:
        return n >= plen && !memcmp(buf + n - plen, pat, plen) ? PSC_YES
                                                              : PSC_NO;
    default:
        return n >= plen && memmem(buf, n, pat, plen) ? PSC_YES : PSC_NO;
    }
}

static bool psc_num_match(PCtx *c, const struct penguin_sc_filter *f,
                          int64_t v)
{
    int64_t val = f->value, lo = f->min_value, hi = f->max_value;
    uint64_t mask = f->bitmask;

    if (c->is32) {
        v = (int32_t)v;
        val = (int32_t)val;
        lo = (int32_t)lo;
        hi = (int32_t)hi;
        mask = (uint32_t)mask;
    }
    switch (f->type) {
    case PENGUIN_SC_FILTER_EXACT:
        return v == val;
    case PENGUIN_SC_FILTER_GREATER:
        return v > val;
    case PENGUIN_SC_FILTER_GREATER_EQUAL:
        return v >= val;
    case PENGUIN_SC_FILTER_LESS:
        return v < val;
    case PENGUIN_SC_FILTER_LESS_EQUAL:
        return v <= val;
    case PENGUIN_SC_FILTER_NOT_EQUAL:
        return v != val;
    case PENGUIN_SC_FILTER_RANGE:
        return v >= lo && v <= hi;
    case PENGUIN_SC_FILTER_SUCCESS:
        return v >= 0;
    case PENGUIN_SC_FILTER_ERROR:
        return v < 0;
    case PENGUIN_SC_FILTER_BITMASK_SET:
        return ((uint64_t)v & mask) == mask;
    case PENGUIN_SC_FILTER_BITMASK_CLEAR:
        return ((uint64_t)v & mask) == 0;
    default:
        return false;
    }
}

static bool psc_is_str(const struct penguin_sc_filter *f)
{
    return f->type >= PENGUIN_SC_FILTER_STR_EXACT &&
           f->type <= PENGUIN_SC_FILTER_STR_ENDSWITH;
}

/* a string filter's type as penguin-osi.h's match */
static int psc_how(const struct penguin_sc_filter *f)
{
    switch (f->type) {
    case PENGUIN_SC_FILTER_STR_EXACT:
        return PENGUIN_OSI_MATCH_EXACT;
    case PENGUIN_SC_FILTER_STR_STARTSWITH:
        return PENGUIN_OSI_MATCH_STARTSWITH;
    case PENGUIN_SC_FILTER_STR_ENDSWITH:
        return PENGUIN_OSI_MATCH_ENDSWITH;
    default:
        return PENGUIN_OSI_MATCH_CONTAINS;
    }
}

static int psc_filter(PCtx *c, const struct penguin_sc_filter *f,
                      const char *pat, uint64_t v)
{
    if (psc_is_str(f)) {
        if (f->subject == PENGUIN_SC_SUBJECT_FD_PATH) {
            return pat && psc_task(c) &&
                   penguin_osi_fd_path_match(c->cpu, c->task, (int32_t)v,
                                             psc_how(f), pat,
                                             f->pattern_len) == 1
                   ? PSC_YES : PSC_NO;
        }
        return psc_str_match(c, f, pat, v);
    }
    return psc_num_match(c, f, v) ? PSC_YES : PSC_NO;
}

/*
 * igloo's hook_matches_syscall{,_return}; the name matched already.
 * PSC_FAULT: a string filter's memory has to be paged in first.
 */
static int psc_match(PCtx *c, const PHook *h, const uint64_t *args,
                     bool ret_side)
{
    const struct penguin_sc_hook *k = &h->h;

    if (qatomic_read(&h->dead)) {
        return PSC_NO;
    }
    if (k->pid_filter_enabled && (!psc_pid(c) || c->pid != k->filter_pid)) {
        return PSC_NO;
    }
    if (k->comm_filter_enabled &&
        (!psc_comm(c) || strncmp(c->comm, k->comm_filter, 16))) {
        return PSC_NO;
    }
    if (k->scope_filter_enabled && qatomic_read(&psc_scope) &&
        !psc_in_scope(c)) {
        return PSC_NO;
    }
    for (unsigned i = 0; i < PSC_ARGS && i < c->argc; i++) {
        const struct penguin_sc_filter *f = &k->arg_filters[i];
        int m;

        if (!f->enabled) {
            continue;
        }
        m = psc_filter(c, f, h->pat[i], args[i]);
        if (m != PSC_YES) {
            return m;
        }
    }
    if (k->exe_filter.enabled &&
        (!psc_is_str(&k->exe_filter) || !h->pat[PSC_ARGS + 1] ||
         !psc_task(c) ||
         penguin_osi_exe_match(c->cpu, c->task, psc_how(&k->exe_filter),
                               h->pat[PSC_ARGS + 1],
                               k->exe_filter.pattern_len) != 1)) {
        return PSC_NO;
    }
    if (ret_side && k->retval_filter.enabled) {
        return psc_filter(c, &k->retval_filter, h->pat[PSC_ARGS], c->retval);
    }
    return PSC_YES;
}

/* faults the guest may refuse */


static void psc_fault_raised(PRun *r, uint64_t page)
{
    PFault f = { .r = r, .page = page };

    QEMU_LOCK_GUARD(&psc_fault_lock);
    g_array_append_val(psc_out, f);
}

/* the dispatch is back (or gone): did the guest refuse its fault? */
static bool psc_fault_resolved(PRun *r)
{
    bool refused = false;

    QEMU_LOCK_GUARD(&psc_fault_lock);
    for (unsigned i = 0; i < psc_out->len;) {
        PFault *f = &g_array_index(psc_out, PFault, i);
        if (f->r == r) {
            refused |= f->refused;
            g_array_remove_index_fast(psc_out, i);
        } else {
            i++;
        }
    }
    return refused;
}

/* the guard: is a signal for @addr the guest refusing one of our faults? */
static bool psc_fault_refuse(uint64_t addr)
{
    uint64_t page = addr & ~(uint64_t)(qemu_target_page_size() - 1);
    bool ours = false;

    QEMU_LOCK_GUARD(&psc_fault_lock);
    for (unsigned i = 0; i < psc_out->len; i++) {
        PFault *f = &g_array_index(psc_out, PFault, i);
        if (f->page == page) {
            f->refused = ours = true;
        }
    }
    return ours;
}

/* runs */

static PRun *psc_run_new(PCtx *c, int phase)
{
    PRun *r = g_new0(PRun, 1);

    r->phase = phase;
    memcpy(r->args, c->sc->args, sizeof(r->args));
    memcpy(r->orig, c->sc->args, sizeof(r->orig));
    r->ret = c->retval;
    return r;
}

static void psc_run_free(PRun *r)
{
    if (r) {
        psc_fault_resolved(r);
        g_free(r->wdata);
        g_free(r->rbuf);
        g_free(r);
    }
}

/* the call goes; so does the run, and the callback is told */
static void psc_run_abort(PRun *r)
{
    penguin_syscall_cb_t cb = qatomic_read(&psc_cb);

    if (r && r->running && cb) {
        r->cmd = (struct penguin_sc_cmd) { .op = PENGUIN_SC_OP_ABORT };
        cb(NULL, &r->ev, &r->cmd, psc_cb_opaque);
    }
    psc_run_free(r);
}

/*
 * Hold the dispatch: the task takes a fault on c->fault_page, and the trap
 * delivers the call again (resumed) when the task is back.
 */
static void psc_hold(PCtx *c, int phase)
{
    PRun *r = c->r;

    if (!r) {
        r = c->r = psc_run_new(c, phase);
    }
    r->phase = phase;
    if (c->inject) {
        /* the task runs the call, then comes back to the trap */
        c->inject = false;
        c->sc->inject = true;
        c->sc->inject_num = r->cmd.num;
        memcpy(c->sc->inject_args, r->cmd.args, sizeof(c->sc->inject_args));
        c->sc->data = r;
        if (!c->sc->task && psc_task(c)) {
            c->sc->task = c->task;
        }
        qatomic_inc(&psc_injected);
        return;
    }
    r->streak = c->fault_page == r->fpage ? r->streak + 1 : 1;
    r->fpage = c->fault_page;
    r->total++;
    c->sc->fault = true;
    c->sc->fault_addr = c->fault_page;
    c->sc->fault_write = c->fault_write;
    c->sc->data = r;
    /* the reaper drops it if the task dies before it is back */
    if (!c->sc->task && psc_task(c)) {
        c->sc->task = c->task;
    }
    psc_fault_raised(r, c->fault_page);
}

/* back after a fault */
static void psc_resumed(PCtx *c)
{
    PRun *r = c->r;

    if (psc_fault_resolved(r)) {
        qatomic_inc(&psc_refused);
        if (r->nbad < PSC_BAD) {
            r->bad[r->nbad++] = r->fpage;
        }
    }
}

/* events */

static void psc_fill(PCtx *c, struct penguin_sc_event *ev,
                     const uint64_t *args, bool is_enter)
{
    memset(ev, 0, sizeof(*ev));
    memcpy(ev->args, args, sizeof(ev->args));
    ev->pc = c->sc->pc;
    if (psc_task(c)) {
        ev->task = c->task;
        ev->pid = psc_pid(c) ? c->pid : 0;
        ev->create_time = psc_time(c);
    }
    ev->argc = c->argc;
    if (c->pn && c->pn->name) {
        pstrcpy(ev->syscall_name, sizeof(ev->syscall_name), c->pn->name);
    }
    ev->abi = c->sc->abi;
    ev->num = c->sc->num;
    ev->is_enter = is_enter;
    ev->restart = c->sc->restart;
}

static const PHook *psc_find(const PTable *t, uint64_t id)
{
    for (unsigned i = 0; t->hooks && i < t->hooks->n; i++) {
        if (t->hooks->h[i]->id == id) {
            return t->hooks->h[i];
        }
    }
    return NULL;
}

/* commands */

static void psc_rbuf(PRun *r, size_t n)
{
    if (r->rcap < n) {
        r->rcap = MAX(n, 256);
        r->rbuf = g_realloc(r->rbuf, r->rcap);
    }
}

/* validate the callback's command; copy what it writes */
static bool psc_cmd_take(PRun *r)
{
    struct penguin_sc_cmd *k = &r->cmd;

    switch (k->op) {
    case PENGUIN_SC_OP_READ:
        return k->size && k->size <= PENGUIN_SC_IO_MAX;
    case PENGUIN_SC_OP_READ_STR:
        if (!k->size || k->size > PENGUIN_SC_STR_MAX) {
            k->size = PENGUIN_SC_STR_MAX;
        }
        return true;
    case PENGUIN_SC_OP_WRITE:
        if (!k->size || k->size > PENGUIN_SC_IO_MAX || !k->data) {
            return false;
        }
        r->wdata = g_realloc(r->wdata, k->size);
        memcpy(r->wdata, k->data, k->size);
        k->data = NULL;
        return true;
    case PENGUIN_SC_OP_SYSCALL:
        return k->num >= 0;
    default:
        return false;
    }
}

/* calls an injected call must not be: they do not come back to the trap */
static bool psc_may_inject(PCtx *c, int64_t num)
{
    static const char *const never[] = {
        "fork", "vfork", "clone", "clone3", "execve", "execveat",
    };
    /* an exec that succeeds does not come back: only with the exec hook */
    unsigned n = qatomic_read(&psc_exec_hooked) ? 4 : ARRAY_SIZE(never);
    const PNum *pn;

    if (!c->at_enter || (c->sc->abi != SYSCALL_ABI_X86_64 &&
                         c->sc->abi != SYSCALL_ABI_AARCH64)) {
        return false;
    }
    if (!c->t->nums[c->sc->abi] || num >= SYSCALL_TRAP_NUMS) {
        return true;
    }
    pn = &c->t->nums[c->sc->abi][num];
    if (pn->noret) {
        return false;
    }
    for (int i = 0; pn->name && i < n; i++) {
        if (!strcmp(pn->name, never[i])) {
            return false;
        }
    }
    return true;
}

/*
 * Carry out the running hook's command and set its answer.  False if a
 * fault for c->fault_page, or the injected call (c->inject), has to come
 * first.
 */
static bool psc_cmd(PCtx *c)
{
    PRun *r = c->r;
    struct penguin_sc_cmd *k = &r->cmd;
    vaddr bad;
    long n;

    qatomic_inc(&psc_commands);
    k->buf = NULL;
    k->len = 0;
    switch (k->op) {
    case PENGUIN_SC_OP_READ:
        psc_rbuf(r, k->size);
        k->buf = r->rbuf;
        if (syscall_trap_access(c->cpu, k->addr, r->rbuf, k->size, false,
                                &bad)) {
            k->status = PENGUIN_SC_OK;
            k->len = k->size;
            return true;
        }
        break;
    case PENGUIN_SC_OP_READ_STR:
        psc_rbuf(r, k->size);
        k->buf = r->rbuf;
        n = psc_str(c, k->addr, (char *)r->rbuf, k->size - 1);
        if (n == -2) {
            return false;
        }
        k->status = n < 0 ? PENGUIN_SC_FAIL : PENGUIN_SC_OK;
        k->len = MAX(n, 0);
        return true;
    case PENGUIN_SC_OP_WRITE:
        if (syscall_trap_access(c->cpu, k->addr, r->wdata, k->size, true,
                                &bad)) {
            k->status = PENGUIN_SC_OK;
            return true;
        }
        break;
    case PENGUIN_SC_OP_SYSCALL:
        if (c->sc->injected) {
            /* back at the trap: the answer */
            c->sc->injected = false;
            k->status = PENGUIN_SC_OK;
            k->ret = c->sc->inject_ret;
            return true;
        }
        if (!psc_may_inject(c, k->num)) {
            k->status = PENGUIN_SC_FAIL;
            k->ret = 0;
            return true;
        }
        c->inject = true;
        return false;
    default:
        k->status = PENGUIN_SC_FAIL;
        return true;
    }
    if (psc_may_fault(c, bad)) {
        c->fault_page = bad;
        c->fault_write = k->op == PENGUIN_SC_OP_WRITE;
        return false;
    }
    /* the guest would not map it: what could be read before it */
    k->len = k->op == PENGUIN_SC_OP_READ && bad > k->addr ? bad - k->addr : 0;
    k->status = k->len ? PENGUIN_SC_PARTIAL : PENGUIN_SC_FAIL;
    return true;
}

/*
 * Run the chosen hooks from r->i on, each to the end of its commands; the
 * entry hooks chain the arguments, the return hooks the return value.
 * False if held for a fault.
 */
static bool psc_run(PCtx *c, bool is_enter)
{
    PRun *r = c->r;

    for (; r->i < r->n; r->i++) {
        penguin_syscall_cb_t cb = qatomic_read(&psc_cb);

        if (!r->running) {
            const PHook *h = psc_find(c->t, r->ids[r->i]);

            if (!h || qatomic_read(&h->dead) || !cb) {
                continue;
            }
            psc_fill(c, &r->ev, is_enter ? r->args : c->sc->args, is_enter);
            r->ev.retval = is_enter ? 0 : r->ret;
            r->ev.hook = h->id;
            r->ev.call = qatomic_fetch_inc(&psc_next_call) + 1;
            r->cmd = (struct penguin_sc_cmd) { .op = PENGUIN_SC_OP_NONE };
            r->running = true;
            qatomic_inc(&psc_events);
        } else if (!psc_cmd(c)) {
            psc_hold(c, r->phase);
            return false;
        }
        for (;;) {
            if (cb) {
                cb(c->cpu, &r->ev, &r->cmd, psc_cb_opaque);
            } else {
                r->cmd.op = PENGUIN_SC_OP_NONE;
            }
            if (r->cmd.op == PENGUIN_SC_OP_NONE) {
                break;
            }
            if (!psc_cmd_take(r)) {
                r->cmd.status = PENGUIN_SC_FAIL;
                r->cmd.buf = NULL;
                r->cmd.len = 0;
            } else if (!psc_cmd(c)) {
                psc_hold(c, r->phase);
                return false;
            }
        }
        r->running = false;
        if (is_enter) {
            memcpy(r->args, r->ev.args, sizeof(r->args));
            if (r->ev.skip_syscall) {
                /* the call does not run; the return hooks still see it */
                r->skip = true;
                r->ret = r->ev.retval;
                r->i = r->n;
                break;
            }
        } else {
            r->ret = r->ev.retval;
        }
    }
    return true;
}

/* the hooks an event can reach, in igloo's order */
static unsigned psc_lists(PCtx *c, const PList *ls[3])
{
    ls[0] = c->t->all;
    ls[1] = !c->pn || !c->pn->name ? c->t->unknown : NULL;
    ls[2] = c->pn ? c->pn->hooks : NULL;
    return 3;
}

enum { PSC_HELD = -1, PSC_NONE = 0, PSC_RUN = 1 };

/* choose the entry hooks; and whether a return hook can match */
static int psc_ematch(PCtx *c)
{
    const PList *ls[3];
    uint64_t ids[PSC_MATCHES];
    unsigned n = 0;
    bool want = false;
    PRun *r;

    for (unsigned j = 0, nl = psc_lists(c, ls); j < nl; j++) {
        for (unsigned i = 0; ls[j] && i < ls[j]->n; i++) {
            const PHook *h = ls[j]->h[i];
            int m;

            /* a return hook whose other filters fail now cannot match */
            if (h->h.on_return && !want) {
                m = psc_match(c, h, c->sc->args, false);
                if (m == PSC_FAULT) {
                    goto fault;
                }
                want = m == PSC_YES;
            }
            if (!h->h.on_enter || n == PSC_MATCHES) {
                continue;
            }
            m = psc_match(c, h, c->sc->args, false);
            if (m == PSC_FAULT) {
                goto fault;
            }
            if (m == PSC_YES) {
                ids[n++] = h->id;
            }
        }
    }
    if (!n && !want) {
        return PSC_NONE;
    }
    qatomic_inc(&psc_enters);
    r = c->r ? c->r : (c->r = psc_run_new(c, PH_ENTER));
    r->phase = PH_ENTER;
    r->want = want;
    r->n = n;
    r->i = 0;
    memcpy(r->ids, ids, n * sizeof(ids[0]));
    return PSC_RUN;
fault:
    psc_hold(c, PH_EMATCH);
    return PSC_HELD;
}

/* choose the return hooks, for the return value @ret */
static int psc_rmatch(PCtx *c, int64_t ret)
{
    const PList *ls[3];
    uint64_t ids[PSC_MATCHES];
    unsigned n = 0;
    PRun *r;

    c->retval = ret;
    for (unsigned j = 0, nl = psc_lists(c, ls); j < nl; j++) {
        for (unsigned i = 0; ls[j] && i < ls[j]->n && n < PSC_MATCHES; i++) {
            const PHook *h = ls[j]->h[i];
            int m;

            if (!h->h.on_return) {
                continue;
            }
            m = psc_match(c, h, c->sc->args, true);
            if (m == PSC_FAULT) {
                psc_hold(c, PH_RMATCH);
                return PSC_HELD;
            }
            if (m == PSC_YES) {
                ids[n++] = h->id;
            }
        }
    }
    if (!n) {
        return PSC_NONE;
    }
    r = c->r ? c->r : (c->r = psc_run_new(c, PH_RETURN));
    r->phase = PH_RETURN;
    r->ret = ret;
    r->n = n;
    r->i = 0;
    memcpy(r->ids, ids, n * sizeof(ids[0]));
    return PSC_RUN;
}

static void psc_enter(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    PRun *r = sc->resumed ? sc->data : NULL;
    const PTable *t;

    sc->data = NULL;
    if (unlikely(qatomic_read(&psc_dirty) || !qatomic_read(&psc_booted))) {
        psc_refresh(cpu);
    }
    RCU_READ_LOCK_GUARD();
    t = qatomic_rcu_read(&psc_table);
    if (!t) {
        psc_run_abort(r);
        return;
    }
    PCtx c = psc_ctx(cpu, t, sc);
    c.r = r;
    c.at_enter = true;
    if (r) {
        psc_resumed(&c);
        if (memcmp(r->orig, sc->args, sizeof(r->orig))) {
            /* not the call that was held */
            psc_run_abort(r);
            c.r = r = NULL;
        }
    }
    if (!r || r->phase == PH_EMATCH) {
        if (psc_ematch(&c) != PSC_RUN) {
            if (!sc->fault) {
                psc_run_free(c.r);
            }
            return;
        }
        r = c.r;
    }
    if (r->phase == PH_ENTER) {
        if (!psc_run(&c, true)) {
            return;
        }
        memcpy(sc->args, r->args, sizeof(sc->args));
        if (!r->skip) {
            if (r->want && !(c.pn && c.pn->noret)) {
                sc->want_return = true;
                if (psc_task(&c)) {
                    sc->task = c.task;
                }
            }
            psc_run_free(r);
            return;
        }
        r->phase = PH_RMATCH;
    }
    if (r->phase == PH_RMATCH) {
        int m = psc_rmatch(&c, r->ret);

        if (m == PSC_HELD) {
            return;
        }
        if (m == PSC_NONE) {
            r->n = 0;
        }
    }
    if (!psc_run(&c, false)) {
        return;
    }
    memcpy(sc->args, r->args, sizeof(sc->args));
    sc->skip = true;
    sc->ret = r->ret;
    psc_run_free(r);
}

static void psc_ret(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    PRun *r = sc->resumed ? sc->data : NULL;
    const PTable *t;

    sc->data = NULL;
    RCU_READ_LOCK_GUARD();
    t = qatomic_rcu_read(&psc_table);
    if (!t) {
        psc_run_abort(r);
        return;
    }
    PCtx c = psc_ctx(cpu, t, sc);
    c.r = r;
    if (r) {
        psc_resumed(&c);
    } else if (sc->task && psc_task(&c) && c.task != sc->task) {
        /* x86 vfork: the child returns first, to the caller's stack */
        sc->keep = true;
        qatomic_inc(&psc_kept);
        return;
    }
    if (!r || r->phase == PH_RMATCH) {
        int m = psc_rmatch(&c, r ? r->ret
                                 : sc->restart ? -PSC_ERESTARTSYS : sc->ret);
        if (m != PSC_RUN) {
            if (!sc->fault) {
                psc_run_free(c.r);
            }
            return;
        }
        r = c.r;
    }
    if (!psc_run(&c, false)) {
        return;
    }
    if (!sc->restart) {
        sc->ret = r->ret;
    }
    psc_run_free(r);
}

static void psc_drop(SyscallTrap *sc, void *opaque)
{
    psc_run_abort(sc->data);
}

static const SyscallTrapOps psc_ops = {
    .enter = psc_enter,
    .ret = psc_ret,
    .drop = psc_drop,
};

/*
 * The guard: the kernel's signal for a fault it refused (pagein's
 * force_sig_fault() hook).  Returns 0 from the function at its entry, so
 * the task is not signalled; the fault handler returns to the trap, where
 * the dispatch finds its fault refused.
 */
#ifdef CONFIG_TCG
static uint64_t psc_reg(CPUState *cpu, int n)
{
    g_autoptr(GByteArray) b = g_byte_array_new();

    gdb_read_register(cpu, b, n);
    return b->len >= 8 ? ldq_le_p(b->data) : 0;
}

static void psc_set_reg(CPUState *cpu, int n, uint64_t v)
{
    uint8_t b[8];

    stq_le_p(b, v);
    gdb_write_register(cpu, b, n);
}

/* gdb register numbers: x86_64 rax 0, rdx 3, rsi 4, rdi 5, rsp 7; AArch64 xN */
static bool psc_x86;
static int psc_arch_kind = -1;

/*
 * The guest's architecture, for the kernel-function hooks' registers: 1
 * x86_64, 2 AArch64, 0 another, -1 not known yet.  QEMU knows its target
 * only once it is initialized, and the setters below may come first: they
 * accept then, and the hooks check when they run.
 */
static int psc_arch(void)
{
    int k = qatomic_read(&psc_arch_kind);

    if (k < 0) {
        const TargetInfo *ti = target_info();

        if (!ti) {
            return -1;
        }
        k = !strcmp(ti->target_name, "x86_64") ? 1 :
            !strcmp(ti->target_name, "aarch64") ? 2 : 0;
        psc_x86 = k == 1;
        qatomic_set(&psc_arch_kind, k);
    }
    return k;
}

static void psc_guard_cb(CPUState *cpu, vaddr pc, void *opaque)
{
    bool info = (uintptr_t)opaque;
    uint64_t addr, ra, sp;
    uint8_t b[8];

    if (psc_arch() <= 0) {
        return;
    }
    if (info) {
        /* force_sig_info(sig, info, task): si_addr at 16 */
        uint64_t p = psc_reg(cpu, psc_x86 ? 4 : 1);
        if (cpu_memory_rw_debug(cpu, p + 16, b, 8, false)) {
            return;
        }
        addr = ldq_le_p(b);
    } else {
        addr = psc_reg(cpu, psc_x86 ? 3 : 2);
    }
    if (!psc_fault_refuse(addr)) {
        return;
    }
    if (psc_x86) {
        sp = psc_reg(cpu, 7);
        if (cpu_memory_rw_debug(cpu, sp, b, 8, false)) {
            return;
        }
        ra = ldq_le_p(b);
        psc_set_reg(cpu, 7, sp + 8);
        psc_set_reg(cpu, 0, 0);
    } else {
        ra = psc_reg(cpu, 30);
        psc_set_reg(cpu, 0, 0);
    }
    tcg_hook_set_pc(cpu, ra);
}

/* the reaper: release_task(p), the task's calls go with it */
static void psc_reap_cb(CPUState *cpu, vaddr pc, void *opaque)
{
    unsigned n;

    if (psc_arch() <= 0) {
        return;
    }
    n = syscall_trap_forget_task(psc_reg(cpu, psc_x86 ? 5 : 0));

    if (n) {
        qatomic_add(&psc_reaped, n);
    }
}

/*
 * exec: __set_task_comm(task, name, true), past the point of no return.
 * The task's calls were made by the image it leaves and cannot come back:
 * a held one (an injected execve's) goes, its run aborted.
 */
static void psc_exec_cb(CPUState *cpu, vaddr pc, void *opaque)
{
    unsigned n;

    if (psc_arch() <= 0 || !(psc_reg(cpu, psc_x86 ? 3 : 2) & 0xff)) {
        return;
    }
    n = syscall_trap_forget_task(psc_reg(cpu, psc_x86 ? 5 : 0));
    if (n) {
        qatomic_add(&psc_reaped, n);
    }
}
#endif

/* the C ABI */

bool __attribute__((visibility("default")))
set_penguin_syscall_callback(penguin_syscall_cb_t cb, void *opaque)
{
    QEMU_LOCK_GUARD(&psc_lock);
    if (!syscall_trap_set_consumer(&psc_ops, NULL)) {
        return false;
    }
    psc_cb_opaque = opaque;
    qatomic_set(&psc_cb, cb);
    psc_consumer = true;
    psc_mark_dirty();
    return true;
}

void __attribute__((visibility("default")))
set_penguin_syscall_boot_callback(penguin_syscall_boot_cb_t cb, void *opaque)
{
    QEMU_LOCK_GUARD(&psc_boot_lock);
    psc_boot_cb = cb;
    psc_boot_opaque = opaque;
}

bool __attribute__((visibility("default")))
penguin_syscall_set_name(int32_t abi, int64_t num, const char *name,
                         int32_t nargs)
{
    const char *norm;

    if (abi < 0 || abi >= SYSCALL_ABI_COUNT || num < 0 ||
        num >= SYSCALL_TRAP_NUMS) {
        return false;
    }
    QEMU_LOCK_GUARD(&psc_lock);
    if (!psc_names[abi]) {
        if (!name) {
            return true;
        }
        psc_names[abi] = g_new0(PName, SYSCALL_TRAP_NUMS);
    }
    if (!name) {
        psc_names[abi][num].name = NULL;
    } else {
        /* interned for good: the tables point at them */
        norm = psc_normalize(name);
        psc_names[abi][num].name = g_hash_table_lookup(psc_interned, norm);
        if (!psc_names[abi][num].name) {
            char *s = g_strndup(norm, 47);
            g_hash_table_add(psc_interned, s);
            psc_names[abi][num].name = s;
        }
    }
    psc_names[abi][num].nargs = nargs < 0 || nargs > PSC_ARGS ? PSC_ARGS
                                                               : nargs;
    psc_mark_dirty();
    return true;
}

bool __attribute__((visibility("default")))
penguin_syscall_set_layout(const struct penguin_sc_layout *layout)
{
    if (!layout || (layout->ptr_size != 4 && layout->ptr_size != 8)) {
        return false;
    }
    QEMU_LOCK_GUARD(&psc_lock);
    psc_layout = *layout;
    psc_mark_dirty();
    return true;
}

bool __attribute__((visibility("default")))
penguin_syscall_set_guard(uint64_t force_sig_fault, uint64_t force_sig_info)
{
#ifdef CONFIG_TCG
    static bool done;

    /* may come before the accelerator is chosen: under KVM, no events */
    if (psc_arch() == 0 || !(force_sig_fault || force_sig_info)) {
        return false;
    }
    QEMU_LOCK_GUARD(&psc_lock);
    if (!done) {
        if (force_sig_fault) {
            tcg_hook_insert(force_sig_fault, psc_guard_cb, (void *)0);
        }
        if (force_sig_info) {
            tcg_hook_insert(force_sig_info, psc_guard_cb, (void *)1);
        }
        done = true;
    }
    qatomic_set(&psc_guarded, true);
    return true;
#else
    return false;
#endif
}

bool __attribute__((visibility("default")))
penguin_syscall_set_reaper(uint64_t release_task)
{
#ifdef CONFIG_TCG
    static bool done;

    if (psc_arch() == 0 || !release_task) {
        return false;
    }
    QEMU_LOCK_GUARD(&psc_lock);
    if (!done) {
        tcg_hook_insert(release_task, psc_reap_cb, NULL);
        done = true;
    }
    return true;
#else
    return false;
#endif
}

bool __attribute__((visibility("default")))
penguin_syscall_set_exec(uint64_t set_task_comm)
{
#ifdef CONFIG_TCG
    static bool done;

    if (psc_arch() == 0 || !set_task_comm) {
        return false;
    }
    QEMU_LOCK_GUARD(&psc_lock);
    if (!done) {
        tcg_hook_insert(set_task_comm, psc_exec_cb, NULL);
        done = true;
    }
    qatomic_set(&psc_exec_hooked, true);
    return true;
#else
    return false;
#endif
}

void __attribute__((visibility("default")))
penguin_syscall_set_scope(bool enabled)
{
    qatomic_set(&psc_scope, enabled);
}

uint64_t __attribute__((visibility("default")))
penguin_syscall_register(const struct penguin_sc_hook *hook)
{
    PHook *h;

    if (!hook) {
        return 0;
    }
    h = g_new0(PHook, 1);
    h->h = *hook;
    h->h.name[sizeof(h->h.name) - 1] = 0;
    pstrcpy(h->norm, sizeof(h->norm), psc_normalize(h->h.name));
    for (int i = 0; i <= PSC_ARGS + 1; i++) {
        struct penguin_sc_filter *f = i < PSC_ARGS ? &h->h.arg_filters[i]
                                    : i == PSC_ARGS ? &h->h.retval_filter
                                                    : &h->h.exe_filter;
        if (f->enabled && f->pattern && f->pattern_len) {
            h->pat[i] = g_strndup(f->pattern, f->pattern_len);
        }
        f->pattern = h->pat[i];
    }
    QEMU_LOCK_GUARD(&psc_lock);
    h->id = ++psc_next_id;
    g_ptr_array_add(psc_hooks, h);
    psc_mark_dirty();
    return h->id;
}

bool __attribute__((visibility("default")))
penguin_syscall_unregister(uint64_t id)
{
    QEMU_LOCK_GUARD(&psc_lock);
    for (unsigned i = 0; i < psc_hooks->len; i++) {
        PHook *h = g_ptr_array_index(psc_hooks, i);
        if (h->id == id && !h->dead) {
            /* freed by the next rebuild, after a grace period */
            qatomic_set(&h->dead, true);
            psc_mark_dirty();
            return true;
        }
    }
    return false;
}

bool __attribute__((visibility("default")))
penguin_syscall_read_memory(CPUState *cs, uint64_t addr, void *buf,
                            size_t len)
{
    return cs && buf && syscall_trap_read(cs, addr, buf, len);
}

bool __attribute__((visibility("default")))
penguin_syscall_write_memory(CPUState *cs, uint64_t addr, const void *buf,
                             size_t len)
{
    vaddr bad;

    return cs && buf && len &&
           syscall_trap_access(cs, addr, (void *)buf, len, true, &bad);
}

int __attribute__((visibility("default")))
penguin_syscall_regs(CPUState *cs, int first, int n, uint64_t *out)
{
    g_autoptr(GByteArray) b = g_byte_array_new();
    int i;

    if (!cs || !out || first < 0 || n < 0) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        uint8_t v[8] = { 0 };
        int len;

        g_byte_array_set_size(b, 0);
        len = gdb_read_register(cs, b, first + i);
        if (len <= 0 || len > 8) {
            break;
        }
        memcpy(v, b->data, len);
        /* the gdbstub's bytes are the target's order */
        out[i] = target_big_endian() ? ldn_be_p(v, len) : ldn_le_p(v, len);
    }
    return i;
}

void __attribute__((visibility("default")))
penguin_syscall_stats(struct penguin_sc_stats *st)
{
    st->enters = qatomic_read(&psc_enters);
    st->events = qatomic_read(&psc_events);
    st->commands = qatomic_read(&psc_commands);
    st->faults = syscall_trap_faults();
    st->refused = qatomic_read(&psc_refused);
    st->kept = qatomic_read(&psc_kept);
    st->reaped = qatomic_read(&psc_reaped);
    st->injected = qatomic_read(&psc_injected);
    syscall_trap_stats(&st->pending, &st->full, &st->stale);
}
