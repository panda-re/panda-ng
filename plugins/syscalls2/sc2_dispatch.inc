/*
 * syscalls2's dispatch: the subscriptions (the exported ppp_add_cb_* / ppp_remove_cb_* functions'
 * registry), the tables' decoding, and the engine's deliveries and wants. Included once, by
 * syscalls2.cpp (and tests/static/syscalls2/dispatch.cpp, against a stub engine), after:
 * profiles[] and load_info, sysinfo_load(), syscalls_get_id(), syscalls2_vcpus; and for the engine
 * (SYSCALLS2_ENGINE) SC2_NB, SC2_NABI, engine_mode, engine_abi[], engine_profile[],
 * engine_want[] and sc2_slow_word().
 */

/* The engine delivers each number straight to its slot's functions (panda_syscall_route): no
 * dispatcher by number here */
#if defined(PANDA_SYSCALL_HAS_ROUTE) && SYSCALLS2_ENGINE
#define SC2_ROUTED 1
#else
#define SC2_ROUTED 0
#endif

/* ### Subscriptions #################################################### */
/*
 * Every subscriber is in a list: a callback name's own (SC2_LIST(name, dir): the call's numbers in
 * every table that has the name), on_unknown_sys_* (name 0: the numbers no table has), or one of
 * the "every call" lists (SC2_XLIST). The exported ppp_add_cb_* / ppp_remove_cb_* functions
 * (sc2_tables.h, generated/syscall_ppp_boilerplate_*.cpp) keep each list's subscribers as the PPP
 * arrays kept them, and publish it as an immutable sc2_list, which the dispatch reads without a
 * lock. A change also updates the engine's wants for the list's numbers (sc2_changed).
 */
static void (*sc2_pre[64])(CPUState *, target_ulong);     /* registerExecPreCallback's */
static std::atomic<unsigned> sc2_npre{0};

#define SC2_NLISTS (SC2_N_COUNT * 2 + SC2_L_COUNT)
static std::atomic<const sc2_list *> sc2_lists[SC2_NLISTS];
static std::mutex sc2_lock;
static std::vector<const sc2_list *> sc2_retired;       /* replaced lists (freed at uninit) */

struct sc2_reg {                                        /* as PPP_CB_BOILERPLATE's arrays */
    sc2_fn fn[PPP_MAX_CB];
    int n;
    sc2_fn cfn[PPP_MAX_CB];
    void *cctx[PPP_MAX_CB];
    int cn;
    sc2_fn inv, cinv;
};
static sc2_reg *sc2_regs[SC2_NLISTS];

static void sc2_changed(unsigned list);                 /* the engine's wants (below) */

/* The common case, per direction (SC2_ENTER, SC2_RETURN): the one plain on_all_sys_enter (_return)
 * subscriber, when nothing else wants every call (no _with_context one, no on_all_sys_*2, no
 * registerExecPreCallback); else NULL */
static std::atomic<sc2_plain_nr> sc2_all1[2];
/* per direction: anything wants every call but the one plain on_all_sys_* (sc2_e_full, sc2_r_full) */
static std::atomic<bool> sc2_full[2];

/* A change, for the deliveries in progress: each decides its call's keep again after its
 * callbacks (one of them may have subscribed to its return) */
static void sc2_mark_changed(void)
{
#if SYSCALLS2_ENGINE
    for (unsigned i = 0; i < SYSCALLS2_VCPUS; i++) {
        __atomic_store_n(&syscalls2_vcpus[i].changed, 1, __ATOMIC_RELEASE);
    }
#endif
}

static inline const sc2_list *sc2_get(unsigned list)
{
    return sc2_lists[list].load(std::memory_order_acquire);
}

/* A call's own callbacks, with its arguments */
static inline void sc2_run(const sc2_list *L, CPUState *cpu, target_ulong pc, const uint64_t *a)
{
    const sc2_ent *e = L->e, *end = e + L->n;     /* a published list is not empty */

    do {
        ((sc2_inv)e->inv)(e->fn, e->ctx, cpu, pc, a);
    } while (++e < end);
}

/* on_all_sys_* and on_unknown_sys_*, with the call's number */
static inline void sc2_run_nr(const sc2_list *L, CPUState *cpu, target_ulong pc, target_ulong no)
{
    const sc2_ent *e = L->e, *end = e + L->n;

    do {
        if (likely(!e->inv)) {
            ((sc2_plain_nr)e->fn)(cpu, pc, no);
        } else {
            ((sc2_inv_nr)e->inv)(e->fn, e->ctx, cpu, pc, no);
        }
    } while (++e < end);
}

/* on_all_sys_*2, with the call's information and context */
static inline void sc2_run_ctx(const sc2_list *L, CPUState *cpu, target_ulong pc,
                               const syscall_info_t *info, const syscall_ctx_t *ctx)
{
    for (uint32_t i = 0; i < L->n; i++) {
        ((sc2_inv_ctx)L->e[i].inv)(L->e[i].fn, L->e[i].ctx, cpu, pc, info, ctx);
    }
}

static unsigned sc2_slots_used(const sc2_sys *sys)
{
    unsigned n = 0;

    for (unsigned k = 0; k < sys->nargs; k++) {
        n = std::max(n, (unsigned)(sys->kind[k] & 31) + ((sys->kind[k] >> 5) == SC2_K_PAIR ? 2 : 1));
    }
    return n;
}

/* A callback type's arguments against the tables' (the switches checked it at compile time) */
static void sc2_check_nargs(unsigned name, int nargs)
{
    for (int p = 0; p < PROFILE_LAST; p++) {
        const sc2_table *t = profiles[p].table;

        for (uint32_t i = 0; t && i < t->count + t->nsparse; i++) {
            const sc2_sys *s = i < t->count ? &t->sys[i] : &t->sparse[i - t->count];

            if (s->name == name && s->nargs != nargs) {
                std::cerr << PANDA_MSG "WARNING callback " << name << " has " << nargs
                          << " arguments, profile " << p << " " << (int)s->nargs << std::endl;
                return;
            }
        }
    }
}

static void sc2_all1_update(void)
{
    for (unsigned dir = 0; dir < 2; dir++) {
        const sc2_list *A = sc2_get(SC2_XLIST(dir == SC2_ENTER ? SC2_L_ALL_ENTER : SC2_L_ALL_RETURN));
        bool ctx = sc2_get(SC2_XLIST(dir == SC2_ENTER ? SC2_L_ENTER2 : SC2_L_RETURN2)) ||
                   (dir == SC2_ENTER && sc2_npre.load(std::memory_order_relaxed));

        bool one = A && A->n == 1 && !A->e[0].inv;

        sc2_all1[dir].store(one && !ctx ? (sc2_plain_nr)A->e[0].fn : nullptr,
                            std::memory_order_release);
        sc2_full[dir].store(ctx || (A && !one), std::memory_order_release);
    }
}

/* Publish list @l from its registry (under sc2_lock) */
static void sc2_publish(unsigned l)
{
    const sc2_reg *r = sc2_regs[l];
    unsigned n = 0;
    sc2_list *L = nullptr;

    for (int i = 0; i < std::min(r->n, PPP_MAX_CB); i++) {
        n += r->fn[i] != nullptr;
    }
    for (int i = 0; i < std::min(r->cn, PPP_MAX_CB); i++) {
        n += r->cfn[i] != nullptr;
    }
    if (n) {
        L = (sc2_list *)malloc(sizeof(sc2_list) + n * sizeof(sc2_ent));
        L->n = 0;
        for (int i = 0; i < std::min(r->n, PPP_MAX_CB); i++) {
            if (r->fn[i]) {
                L->e[L->n++] = { r->inv, r->fn[i], nullptr };
            }
        }
        for (int i = 0; i < std::min(r->cn, PPP_MAX_CB); i++) {
            if (r->cfn[i]) {
                L->e[L->n++] = { r->cinv, r->cfn[i], r->cctx[i] };
            }
        }
    }
    const sc2_list *old = sc2_lists[l].exchange(L, std::memory_order_acq_rel);
    if (old) {
        sc2_retired.push_back(old);
    }
    sc2_all1_update();
    sc2_changed(l);
    sc2_mark_changed();
}

void sc2_add(unsigned l, sc2_fn fn, void *ctx, bool with_ctx, sc2_fn inv, int nargs, int slot)
{
    std::lock_guard<std::mutex> g(sc2_lock);
    sc2_reg *&r = sc2_regs[l];

    if (!r) {
        r = (sc2_reg *)calloc(1, sizeof(*r));
        if (nargs >= 0 && l < SC2_N_COUNT * 2) {
            sc2_check_nargs(l / 2, nargs);
        }
    }
    if (with_ctx) {
        r->cinv = inv;
        if (slot < 0) {
            assert(r->cn < PPP_MAX_CB);
            r->cfn[r->cn] = fn;
            r->cctx[r->cn] = ctx;
            r->cn++;
        } else {
            assert(slot < PPP_MAX_CB);
            r->cfn[slot] = fn;
            r->cctx[slot] = ctx;
            r->cn = std::max(slot, r->cn);
        }
    } else {
        r->inv = inv;
        if (slot < 0) {
            assert(r->n < PPP_MAX_CB);
            r->fn[r->n++] = fn;
        } else {
            assert(slot < PPP_MAX_CB);
            r->fn[slot] = fn;
            r->n = std::max(slot, r->n);
        }
    }
    sc2_publish(l);
}

bool sc2_remove(unsigned l, sc2_fn fn, void *ctx, bool with_ctx)
{
    std::lock_guard<std::mutex> g(sc2_lock);
    sc2_reg *r = sc2_regs[l];
    bool found = false;

    if (!r) {
        return false;
    }
    if (with_ctx) {
        for (int i = 0, n = r->cn; i < std::min(PPP_MAX_CB, n); i++) {
            if (!found && fn == r->cfn[i] && ctx == r->cctx[i]) {
                found = true;
                r->cn--;
            }
            if (found && i < PPP_MAX_CB - 2) {
                r->cfn[i] = r->cfn[i + 1];
                r->cctx[i] = r->cctx[i + 1];
            }
        }
    } else {
        for (int i = 0, n = r->n; i < std::min(PPP_MAX_CB, n); i++) {
            if (!found && fn == r->fn[i]) {
                found = true;
                r->n--;
            }
            if (found && i < PPP_MAX_CB - 2) {
                r->fn[i] = r->fn[i + 1];
            }
        }
    }
    if (found) {
        sc2_publish(l);
    }
    return found;
}

void registerExecPreCallback(void (*callback)(CPUState*, target_ulong)){
    std::lock_guard<std::mutex> g(sc2_lock);
    unsigned n = sc2_npre.load(std::memory_order_relaxed);

    assert(n < sizeof(sc2_pre) / sizeof(sc2_pre[0]));
    sc2_pre[n] = callback;
    sc2_npre.store(n + 1, std::memory_order_release);
    sc2_all1_update();
    sc2_changed(SC2_XLIST(SC2_L_ALL_ENTER));        /* every entry */
    sc2_mark_changed();
}

static void SC2_HOT __attribute__((noinline)) sc2_run_pre(CPUState *cpu, target_ulong pc)
{
    unsigned n = sc2_npre.load(std::memory_order_acquire);

    for (unsigned i = 0; i < n; i++) {
        sc2_pre[i](cpu, pc);
    }
}

/* ### Dispatch: decoding, contexts ####################################### */
static const sc2_sys sc2_none = {};                     /* a number no table has */

/* @no's entry in @t, or NULL */
static const sc2_sys *sc2_lookup(const sc2_table *t, int64_t no)
{
    uint64_t i = (uint64_t)no - t->base;

    if (i < t->count) {
        return t->sys[i].name ? &t->sys[i] : nullptr;
    }
    for (uint32_t k = 0; k < t->nsparse; k++) {
        if ((int64_t)t->sparse_nr[k] == no) {
            return &t->sparse[k];
        }
    }
    return nullptr;
}

/* The arguments of @sys from the raw words @w, by their kinds */
static inline void sc2_decode(const sc2_sys *sys, const uint64_t *w, uint64_t *a)
{
    for (unsigned k = 0; k < sys->nargs; k++) {
        unsigned kd = sys->kind[k], i = kd & 31;
        uint64_t v = w[i];

        switch (kd >> 5) {
        case SC2_K_U32:
            v = (uint32_t)v;
            break;
        case SC2_K_S32:
            v = (uint64_t)(int64_t)(int32_t)v;
            break;
        case SC2_K_PAIR:
            v = (uint64_t)(uint32_t)w[i + 1] << 32 | (uint32_t)v;
            break;
        default:
            break;
        }
        a[k] = v;
    }
}

/* syscall_ctx_t.args: each argument in its type's size (4 or 8 bytes), the rest 0 */
static void sc2_ctx_args(syscall_ctx_t *ctx, const sc2_sys *sys, const uint64_t *a)
{
    memset(ctx->args, 0, sizeof(ctx->args));
    for (unsigned k = 0; k < sys->nargs && k < GLOBAL_MAX_SYSCALL_ARGS; k++) {
        unsigned op = sys->kind[k] >> 5;

        if (op == SC2_K_U32 || op == SC2_K_S32) {
            uint32_t v = (uint32_t)a[k];
            memcpy(ctx->args[k], &v, 4);
        } else {
            memcpy(ctx->args[k], &a[k], 8);
        }
    }
}

/* The arguments back from syscall_ctx_t.args */
static void sc2_ctx_decode(const syscall_ctx_t *ctx, const sc2_sys *sys, uint64_t *a)
{
    for (unsigned k = 0; k < sys->nargs && k < GLOBAL_MAX_SYSCALL_ARGS; k++) {
        unsigned op = sys->kind[k] >> 5;

        if (op == SC2_K_U32 || op == SC2_K_S32) {
            uint32_t v;
            memcpy(&v, ctx->args[k], 4);
            a[k] = op == SC2_K_S32 ? (uint64_t)(int64_t)(int32_t)v : v;
        } else {
            memcpy(&a[k], ctx->args[k], 8);
        }
    }
}

/* on_all_sys_*2's syscall_info_t: the profile's (load-info), if it has the number */
static std::mutex sc2_info_lock;
static SC2_HOT const syscall_info_t *sc2_info(int profile, int no)
{
    static const syscall_info_t zero = {};
    const syscall_info_t *info;
    const syscall_meta_t *meta;

    if (!load_info || profile < 0 || profile >= PROFILE_LAST) {
        return nullptr;
    }
    static std::atomic<bool> tried[PROFILE_LAST];
    if (!tried[profile].load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> g(sc2_info_lock);
        if (!tried[profile].load(std::memory_order_relaxed)) {
            if (!profiles[profile].syscall_meta) {
                sysinfo_load(profile);
            }
            tried[profile].store(true, std::memory_order_release);
        }
    }
    info = (const syscall_info_t *)profiles[profile].syscall_info;
    meta = (const syscall_meta_t *)profiles[profile].syscall_meta;
    if (meta && info && (uint32_t)no <= meta->max_generic &&
        memcmp(&info[no], &zero, sizeof(zero)) != 0) {
        return &info[no];
    }
    return nullptr;
}


#if SYSCALLS2_ENGINE
/*
 * The engine's numbers, per ABI: each one's table entry (sc2_none: unknown), callback name and
 * profile, filled at init from the profile tables. Index SC2_NB is every number from SC2_NB on
 * (and the negative ones): looked up at each call (ARM's private calls are there).
 */
enum {
    SC2_S_STACK = 1u << 0,                  /* reads words the engine does not capture */
    SC2_S_OVER = 1u << 1,                   /* index SC2_NB */
    SC2_S_CTX = 1u << 2,                    /* a kept call's return needs its entry's context */
    SC2_S_GEN = 1u << 3,                    /* not argument k in word k (pairs, the stack, more
                                             * than 6): sc2_decode, not sc2_decode_fast */
};
struct sc2_slot {
    const sc2_sys *sys;
    uint16_t name;
    uint8_t flags;
    uint8_t profile;
    uint8_t nargs;
    uint8_t m64, ms32;                      /* sc2_decode_fast: the X64 and S32 arguments */
};
/* index SC2_NB included; a power of two apart */
#define SC2_NBP (SC2_NB < 1024 ? 1024 : SC2_NB < 2048 ? 2048 : SC2_NB < 4096 ? 4096 : 8192)
static sc2_slot sc2_slots[SC2_NABI][SC2_NBP];
static std::vector<uint32_t> sc2_name_slots[SC2_N_COUNT];  /* (ABI << 16 | index) of each name */
static bool sc2_name_over[SC2_N_COUNT];                    /* has numbers from SC2_NB on */

/* syscall_ctx_t in the call's data, and whether it was built for this call */
struct sc2_data {
    syscall_ctx_t ctx;
    uint64_t mark;                          /* SC2_MARK(call): ctx is this call's */
};
#define SC2_MARK(c) ((c)->id + 1)

/* The table and profile that serve ABI @a's number @no */
static const sc2_table *sc2_abi_table(unsigned a, int64_t no, ProfileType *p)
{
#if defined(TARGET_MIPS64)
    if (no >= 4000 && no < 5000) {
        *p = PROFILE_LINUX_MIPS32;
        return &sc2_table_linux_mips;
    }
    if (no >= 6000 && no < 7000) {
        *p = PROFILE_LINUX_MIPS64N32;
        return &sc2_table_linux_mips64n32;
    }
#endif
    *p = engine_profile[a];
    return profiles[*p].table;
}

static sc2_slot sc2_slot_of(unsigned a, int64_t no)
{
    ProfileType p;
    const sc2_table *t = sc2_abi_table(a, no, &p);
    const sc2_sys *sys = sc2_lookup(t, no);
    sc2_slot s = { sys ? sys : &sc2_none, (uint16_t)(sys ? sys->name : 0), 0, (uint8_t)p,
                   0, 0, 0 };

    if (!sys) {
        return s;
    }
    if (sc2_slots_used(sys) > t->fast_slots) {
        s.flags |= SC2_S_STACK | SC2_S_GEN;
    }
    s.nargs = sys->nargs;
    for (unsigned k = 0; k < sys->nargs; k++) {
        unsigned op = sys->kind[k] >> 5;

        if (k >= 6 || (sys->kind[k] & 31) != k || op == SC2_K_PAIR) {
            s.flags |= SC2_S_GEN;           /* (syscalls2_vcpu_t.a holds 6) */
        }
        s.m64 |= (op == SC2_K_X64) << k;
        s.ms32 |= (op == SC2_K_S32) << k;
    }
    return s;
}

/* The call's raw argument words: the engine's, and the slow ones read at the entry (0 at a return:
 * they were saved in the context at the entry when a return callback wanted them) */
static inline const uint64_t *sc2_words(CPUState *cpu, const PandaSyscallCall *c,
                                        const sc2_slot *s, uint64_t *buf, bool entry)
{
    if (likely(!(s->flags & SC2_S_STACK))) {
        return c->args;
    }
    ProfileType p;
    const sc2_table *t = sc2_abi_table(c->abi, c->nr, &p);
    for (unsigned i = 0; i < SC2_MAX_SLOTS; i++) {
        buf[i] = i < t->fast_slots ? (i < 6 ? c->args[i] : 0) : entry ? sc2_slow_word(cpu, i) : 0;
    }
    return buf;
}

/* sc2_decode, for argument k in word k: the low 32 bits, sign-extended for S32, or the word */
static inline void sc2_decode_fast(const sc2_slot *s, const uint64_t *w, uint64_t *a)
{
    for (unsigned k = 0; k < s->nargs; k++) {
        uint64_t v = w[k];

        a[k] = (s->m64 >> k) & 1 ? v : (s->ms32 >> k) & 1 ? (uint64_t)(int64_t)(int32_t)v
                                                         : (uint64_t)(uint32_t)v;
    }
}

/* The context, in the call's data: as the switches built it, the arguments decoded */
static void SC2_HOT sc2_ctx_build(CPUState *cpu, const PandaSyscallCall *c, const sc2_slot *s,
                          sc2_data *d, bool entry)
{
    syscall_ctx_t *ctx = &d->ctx;

    ctx->no = (int)c->nr;
    ctx->asid = syscalls_get_id(cpu);
    ctx->retaddr = c->pc + c->len;
    ctx->double_return = s->sys->flags & SC2_F_DOUBLE;
    ctx->profile = s->profile;
    if (s->sys->nargs) {
        uint64_t w[SC2_MAX_SLOTS], a[SC2_MAX_ARGS];

        sc2_decode(s->sys, sc2_words(cpu, c, s, w, entry), a);
        sc2_ctx_args(ctx, s->sys, a);
    } else {
        memset(ctx->args, 0, sizeof(ctx->args));
    }
    d->mark = SC2_MARK(c);
}

/* Whether a call's return is wanted now; whether its return needs the context from its entry */
static inline bool sc2_keep_of(const sc2_slot *s)
{
    return !(s->sys->flags & SC2_F_NORET) &&
           (sc2_get(SC2_LIST(s->name, SC2_RETURN)) || sc2_get(SC2_XLIST(SC2_L_ALL_RETURN)) ||
            sc2_get(SC2_XLIST(SC2_L_RETURN2)));
}
static inline bool sc2_needs_ctx(const sc2_slot *s)
{
    return sc2_get(SC2_XLIST(SC2_L_RETURN2)) ||
           ((s->flags & SC2_S_STACK) && sc2_get(SC2_LIST(s->name, SC2_RETURN)));
}

/* ### Engine wants ####################################################### */
/* What the engine is to deliver of ABI @a's index @i, from the subscriptions */
static uint8_t sc2_want_of(unsigned a, unsigned i)
{
    const sc2_slot *s = &sc2_slots[a][i];
    bool e = sc2_get(SC2_XLIST(SC2_L_ALL_ENTER)) || sc2_get(SC2_XLIST(SC2_L_ENTER2)) ||
             sc2_npre.load(std::memory_order_relaxed);
    bool r;

    if (s->flags & SC2_S_OVER) {
        /* unknown numbers, and the tables' numbers from SC2_NB on */
        r = sc2_get(SC2_XLIST(SC2_L_ALL_RETURN)) || sc2_get(SC2_XLIST(SC2_L_RETURN2)) ||
            sc2_get(SC2_LIST(0, SC2_RETURN));
        e = e || sc2_get(SC2_LIST(0, SC2_ENTER)) || (r && sc2_get(SC2_XLIST(SC2_L_RETURN2)));
        for (unsigned n = 1; n < SC2_N_COUNT; n++) {
            if (sc2_name_over[n]) {
                e = e || sc2_get(SC2_LIST(n, SC2_ENTER)) || sc2_get(SC2_LIST(n, SC2_RETURN));
                r = r || sc2_get(SC2_LIST(n, SC2_RETURN));
            }
        }
    } else {
        r = sc2_keep_of(s);
        e = e || sc2_get(SC2_LIST(s->name, SC2_ENTER)) || (r && sc2_needs_ctx(s));
    }
    return (e ? PANDA_SYSCALL_WANT_ENTRY : 0) | (r ? PANDA_SYSCALL_WANT_RETURN : 0);
}

static void sc2_want_update(unsigned a, unsigned i)
{
    uint8_t w = sc2_want_of(a, i);
    sc2_slot *s = &sc2_slots[a][i];

    if (!(s->flags & SC2_S_OVER)) {
        s->flags = (s->flags & ~SC2_S_CTX) | (sc2_needs_ctx(s) ? SC2_S_CTX : 0);
    }

    if (engine_want[a][i] != w) {
        engine_want[a][i] = w;
        panda_syscall_want(engine_abi[a], i, w);
    }
}

/* After list @l changed (under sc2_lock): the wants of its numbers, O(numbers of its name), or of
 * every number for the "every call" lists and on_unknown_sys_* */
static void sc2_changed(unsigned l)
{
    if (!engine_mode) {
        return;
    }
    if (l >= SC2_N_COUNT * 2) {
        for (unsigned a = 0; a < SC2_NABI; a++) {
            for (unsigned i = 0; i <= SC2_NB; i++) {
                sc2_want_update(a, i);
            }
        }
        return;
    }
    unsigned name = l / 2;
    for (uint32_t x : sc2_name_slots[name]) {
        sc2_want_update(x >> 16, x & 0xffff);
    }
    if (sc2_name_over[name]) {
        for (unsigned a = 0; a < SC2_NABI; a++) {
            sc2_want_update(a, SC2_NB);
        }
    }
}

static void sc2_e(void *, CPUState *, PandaSyscallCall *, int64_t, uint32_t);
static void sc2_r(void *, CPUState *, PandaSyscallCall *, int64_t, uint32_t);
static void sc2_over_route(void *, CPUState *, PandaSyscallCall *, int64_t, uint32_t);

/* Once the ABIs are declared: each one's numbers' table entries (the tables' numbers from SC2_NB
 * on are looked up at each call), and the engine's wants from the subscriptions so far: nothing is
 * delivered that nobody wants */
static void sc2_engine_start(void)
{
    for (unsigned a = 0; a < SC2_NABI; a++) {
        ProfileType p;
        if (!sc2_abi_table(a, 0, &p)->count) {
            std::cerr << PANDA_MSG "ERROR profile " << p << ": no table in this build" << std::endl;
        }
        for (unsigned n = 0; n < SC2_NB; n++) {
            sc2_slots[a][n] = sc2_slot_of(a, n);
            sc2_name_slots[sc2_slots[a][n].name].push_back(a << 16 | n);
        }
        sc2_slots[a][SC2_NB] = { &sc2_none, 0, SC2_S_OVER, (uint8_t)engine_profile[a] };
        sc2_name_slots[0].push_back(a << 16 | SC2_NB);
#if SC2_ROUTED
        /* each number straight to its slot's functions (wants are still 0: no delivery yet) */
        for (unsigned n = 0; n < SC2_NB; n++) {
            panda_syscall_route(engine_abi[a], n, sc2_e, sc2_r, &sc2_slots[a][n]);
        }
        panda_syscall_route(engine_abi[a], SC2_NB, sc2_over_route, sc2_over_route, nullptr);
#endif
    }
    for (unsigned p = 0; p < PROFILE_LAST; p++) {
        const sc2_table *t = profiles[p].table;
        for (uint32_t k = 0; t && k < t->nsparse; k++) {
            if (t->sparse_nr[k] >= SC2_NB) {
                sc2_name_over[t->sparse[k].name] = true;
            }
        }
    }
    std::lock_guard<std::mutex> g(sc2_lock);
    engine_mode = true;
    sc2_changed(SC2_XLIST(SC2_L_ALL_ENTER));
}

/* ### Engine deliveries ################################################## */
/*
 * An entry: the call's own callbacks (or on_unknown_sys_enter) with its arguments decoded from the
 * engine's, on_all_sys_enter with its number, on_all_sys_enter2 with its context, built in the
 * call's data. A call is kept if its return was wanted when it entered (the engine's want), or
 * its return is wanted after the entry's callbacks (a subscription changed meanwhile); a kept
 * call's context is built at the entry when its return needs one (on_all_sys_return2, or
 * arguments the engine does not capture: SC2_S_CTX).
 *
 * The common work is in small functions without arrays (no stack protector, few saved
 * registers): the dispatcher engine_call tail-calls sc2_e or sc2_r, which call the "every call"
 * subscribers themselves and the rest out of line.
 */
static void SC2_HOT __attribute__((noinline)) sc2_enter_changed(CPUState *cpu, PandaSyscallCall *c,
                                                        const sc2_slot *s, bool built)
{
    bool keep = sc2_keep_of(s);

    if (keep && !built && sc2_needs_ctx(s)) {
        sc2_ctx_build(cpu, c, s, (sc2_data *)c->data, true);
    }
    if (keep != (c->keytype != PANDA_TASK_KEY_NONE)) {
        panda_syscall_keep(cpu, c, keep);
    }
}

/* The call's own callbacks (or on_unknown_sys_*): list @L at @pc, the arguments as @entry's */
static void SC2_HOT __attribute__((noinline)) sc2_own(CPUState *cpu, PandaSyscallCall *c, const sc2_slot *s,
                                              const sc2_list *L, target_ulong pc, bool entry)
{
    syscalls2_vcpu_t *sv = SYSCALLS2_VCPU(cpu);
    sc2_data *d = (sc2_data *)c->data;
    uint64_t w[SC2_MAX_SLOTS], a[SC2_MAX_ARGS];

    if (unlikely(!s->name)) {
        sc2_run_nr(L, cpu, pc, (target_ulong)(int)c->nr);
    } else if (likely(!(s->flags & SC2_S_GEN))) {
        sc2_decode_fast(s, c->args, sv->a);
        sc2_run(L, cpu, pc, sv->a);
    } else {
        if (!entry && d->mark == SC2_MARK(c)) {
            sc2_ctx_decode(&d->ctx, s->sys, a);  /* words the engine does not capture */
        } else {
            sc2_decode(s->sys, sc2_words(cpu, c, s, w, entry), a);
        }
        sc2_run(L, cpu, pc, a);
    }
}

/* One own subscriber of a call whose arguments are its first words: called from sc2_e, sc2_r */
static inline void sc2_own1(CPUState *cpu, PandaSyscallCall *c, const sc2_slot *s,
                            const sc2_list *L, target_ulong pc, bool entry)
{
    if (likely(L->n == 1 && s->name && !(s->flags & SC2_S_GEN))) {
        syscalls2_vcpu_t *sv = SYSCALLS2_VCPU(cpu);

        sc2_decode_fast(s, c->args, sv->a);
        ((sc2_inv)L->e[0].inv)(L->e[0].fn, L->e[0].ctx, cpu, pc, sv->a);
    } else {
        sc2_own(cpu, c, s, L, pc, entry);
    }
}

/* on_all_sys_enter2: the context, built in the call's data */
static void SC2_HOT __attribute__((noinline)) sc2_e_ctx(CPUState *cpu, PandaSyscallCall *c,
                                                const sc2_slot *s, const sc2_list *L)
{
    sc2_data *d = (sc2_data *)c->data;

    sc2_ctx_build(cpu, c, s, d, true);
    sc2_run_ctx(L, cpu, c->pc, sc2_info(s->profile, (int)c->nr), &d->ctx);
}

/* After the entry's callbacks: the keep decision if a subscription changed, the context */
static void SC2_HOT __attribute__((noinline)) sc2_e_after(CPUState *cpu, PandaSyscallCall *c,
                                                  const sc2_slot *s, bool changed)
{
    bool built = ((sc2_data *)c->data)->mark == SC2_MARK(c);

    if (changed) {
        sc2_enter_changed(cpu, c, s, built);
    } else if (c->keytype != PANDA_TASK_KEY_NONE && !built && sc2_needs_ctx(s)) {
        sc2_ctx_build(cpu, c, s, (sc2_data *)c->data, true);
    }
}

/* An entry's callbacks, when something wants every call but one plain on_all_sys_enter */
static void SC2_HOT __attribute__((noinline)) sc2_e_full(CPUState *cpu, PandaSyscallCall *c,
                                                 const sc2_slot *s)
{
    const sc2_list *L;

    if (unlikely(sc2_npre.load(std::memory_order_relaxed))) {
        sc2_run_pre(cpu, c->pc);
    }
    if ((L = sc2_get(SC2_LIST(s->name, SC2_ENTER)))) {
        sc2_own(cpu, c, s, L, c->pc, true);
    }
    if ((L = sc2_get(SC2_XLIST(SC2_L_ALL_ENTER)))) {
        sc2_run_nr(L, cpu, c->pc, (target_ulong)(int)c->nr);
    }
    if ((L = sc2_get(SC2_XLIST(SC2_L_ENTER2)))) {
        sc2_e_ctx(cpu, c, s, L);
    }
}

/* An entry: @slot the number's sc2_slot (the engine's route, or engine_call) */
static void SC2_HOT __attribute__((noinline)) sc2_e(void *slot, CPUState *cpu, PandaSyscallCall *c,
                                                    int64_t ret, uint32_t flags)
{
    const sc2_slot *s = (const sc2_slot *)slot;
    syscalls2_vcpu_t *sv = SYSCALLS2_VCPU(cpu);
    const sc2_list *L;

    sv->call = c;
    sv->flags_changed = 0;
    if (unlikely(sc2_full[SC2_ENTER].load(std::memory_order_acquire))) {
        sc2_e_full(cpu, c, s);
    } else {
        sc2_plain_nr f;

        if ((L = sc2_get(SC2_LIST(s->name, SC2_ENTER)))) {
            sc2_own1(cpu, c, s, L, c->pc, true);
        }
        if ((f = sc2_all1[SC2_ENTER].load(std::memory_order_acquire))) {
            f(cpu, c->pc, (target_ulong)(int)c->nr);
        }
    }
    sv->call = nullptr;
    if (unlikely(__atomic_load_n(&sv->changed, __ATOMIC_ACQUIRE) | (s->flags & SC2_S_CTX))) {
        sc2_e_after(cpu, c, s, sv->changed);
    }
}

/* on_all_sys_return2: the context from the entry, or built now (a subscriber since) */
static void SC2_HOT __attribute__((noinline)) sc2_r_ctx(CPUState *cpu, PandaSyscallCall *c,
                                                const sc2_slot *s, const sc2_list *L)
{
    sc2_data *d = (sc2_data *)c->data;

    if (d->mark != SC2_MARK(c)) {
        sc2_ctx_build(cpu, c, s, d, false);
    }
    sc2_run_ctx(L, cpu, c->pc + c->len, sc2_info(s->profile, (int)c->nr), &d->ctx);
}

/* A return's callbacks, when something wants every call but one plain on_all_sys_return */
static void SC2_HOT __attribute__((noinline)) sc2_r_full(CPUState *cpu, PandaSyscallCall *c,
                                                 const sc2_slot *s)
{
    const sc2_list *L;

    if ((L = sc2_get(SC2_LIST(s->name, SC2_RETURN)))) {
        sc2_own(cpu, c, s, L, c->pc + c->len, false);
    }
    if ((L = sc2_get(SC2_XLIST(SC2_L_ALL_RETURN)))) {
        sc2_run_nr(L, cpu, c->pc + c->len, (target_ulong)(int)c->nr);
    }
    if ((L = sc2_get(SC2_XLIST(SC2_L_RETURN2)))) {
        sc2_r_ctx(cpu, c, s, L);
    }
}

/* A return: @slot as sc2_e's. Not a restarted call (a new entry follows) or execve's end. */
static void SC2_HOT __attribute__((noinline)) sc2_r(void *slot, CPUState *cpu, PandaSyscallCall *c,
                                                    int64_t ret, uint32_t flags)
{
    const sc2_slot *s = (const sc2_slot *)slot;
    const sc2_list *L;

    if (unlikely(flags & (PANDA_SYSCALL_RESTART | PANDA_SYSCALL_ENDED))) {
        return;
    }
    syscalls2_vcpu_t *sv = SYSCALLS2_VCPU(cpu);
    sv->call = c;
    sv->flags = flags;
    if (unlikely(sc2_full[SC2_RETURN].load(std::memory_order_acquire))) {
        sc2_r_full(cpu, c, s);
    } else {
        sc2_plain_nr f;

        if ((L = sc2_get(SC2_LIST(s->name, SC2_RETURN)))) {
            sc2_own1(cpu, c, s, L, c->pc + c->len, false);
        }
        if ((f = sc2_all1[SC2_RETURN].load(std::memory_order_acquire))) {
            f(cpu, c->pc + c->len, (target_ulong)(int)c->nr);
        }
    }
    sv->call = nullptr;
}

/* A number from SC2_NB on (or negative): its entry looked up */
static void SC2_HOT __attribute__((noinline)) sc2_over(CPUState *cpu, PandaSyscallCall *c, uint32_t flags)
{
    sc2_slot s = sc2_slot_of(c->abi, c->nr);

    s.flags |= sc2_needs_ctx(&s) ? SC2_S_CTX : 0;
    if (flags & PANDA_SYSCALL_RETURN) {
        sc2_r(&s, cpu, c, 0, flags);
    } else {
        sc2_e(&s, cpu, c, 0, flags);
    }
}

/* The routes' signature for the numbers from SC2_NB on */
static void SC2_HOT sc2_over_route(void *opaque, CPUState *cpu, PandaSyscallCall *c, int64_t ret,
                                   uint32_t flags)
{
    sc2_over(cpu, c, flags);
}

static void SC2_HOT engine_call(void *opaque, CPUState *cpu, PandaSyscallCall *c, int64_t ret,
                        uint32_t flags)
{
    uint64_t i = (uint64_t)c->nr < SC2_NB ? (uint64_t)c->nr : SC2_NB;
    const sc2_slot *s = &sc2_slots[c->abi][i];

    if (unlikely(s->flags & SC2_S_OVER)) {
        sc2_over(cpu, c, flags);
    } else if (!flags) {
        sc2_e((void *)s, cpu, c, ret, flags);  /* an entry */
    } else {
        sc2_r((void *)s, cpu, c, ret, flags);  /* a return */
    }
}

/* The call being delivered on @cpu, NULL outside a delivery */
static inline const PandaSyscallCall *sc2_current(CPUState *cpu, uint32_t *flags)
{
    const syscalls2_vcpu_t *sv = SYSCALLS2_VCPU(cpu);

    if (sv->call && flags) {
        *flags = sv->flags;
    }
    return sv->call;
}

bool syscalls2_current_call(CPUState *cpu, uint64_t *id, uint32_t *flags)
{
    const PandaSyscallCall *c = sc2_current(cpu, flags);

    if (!c) {
        return false;
    }
    *id = c->id;
    return true;
}

#endif /* SYSCALLS2_ENGINE */

