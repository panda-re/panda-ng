/*
 * hooktest: exercise address hooks, through qemu_plugin_hook_insert()
 * (api=hook) or through instruction callbacks registered from a
 * translation callback (api=insn, what plugins do today).
 *
 * Addresses are guest symbols of guest/pagetest.c (hk_*), from nm:
 *   target=A   count calls                  target2=A  count calls
 *   phase=A    hk_phase(n): log the counts with n (arg0)
 *   regs=A     hk_regs(a): add 1000 to arg0 at entry
 *   redir=A other=A   hk_redirect(): continue at hk_other instead
 *   fault=A    hk_fault(p): read 32 bytes at p (arg0), faulting them in
 *   dyn=on     api=hook: target= is inserted at phase 1, removed at
 *              phase 2, inserted again at phase 3, removed at phase 4
 *   arm=on     api=hook, dyn=on: insert target= at the start as well
 *   ktarget=A  count calls (a kernel function)
 *   reads=K    read arg0 K times per target= hit (register-read cost)
 *   reuse=on   reuse one buffer for register reads instead of a new one
 *   direct=on  read and write arg0 through qemu_plugin_register_ptr(),
 *              checking the first reads against qemu_plugin_read_register()
 *   many=N     api=hook: also insert N hooks at addresses never executed
 *              (table-size cost)
 *   dup=on     api=hook: a second hook at target= (counted as dup)
 *   selfrm=K   api=hook: a hook at target2= that removes itself after K hits
 *   mid=A      count calls of an instruction in the middle of a block
 *   ds=A       count calls of a branch delay slot instruction (mips)
 *   cond=on    conditional hooks at target= on arg0: ==7, <10 unsigned,
 *              (arg0 & 3) == 3, 20..29, > -1 signed, and one on a host
 *              variable that phase(n) sets to n % 2 == 0 (counters
 *              ceq cltu cmask crange cgt chost)
 *   condbench=pass|fail  api=hook: target= is one conditional hook whose
 *              condition always holds / never holds (cost of the test)
 *   rptest=on  check qemu_plugin_register_ptr() for the argument, stack and
 *              return-address registers (found, equal to the gdbstub read
 *              on every hit, all registers unchanged by the lookup) and for
 *              the program counter and flags (NULL)
 */
#include <glib.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static qemu_plugin_id_t self;
static bool api_hook = true, opt_dyn, wide = true, big_endian;
static uint64_t a_target, a_target2, a_phase, a_regs, a_redir, a_other;
static uint64_t a_fault, a_ktarget, opt_reads, opt_many;
static uint64_t n_target, n_target2, n_trans, n_ktarget;
static uint64_t a_mid, a_ds, opt_selfrm;
static uint64_t n_dup, n_mid, n_ds, n_selfrm;
static bool opt_dup, opt_rptest;
static struct qemu_plugin_hook *h_selfrm;
static bool opt_cond;
static const char *opt_condbench;
static uint64_t n_ceq, n_cltu, n_cmask, n_crange, n_cgt, n_chost;
static uint32_t host_flag;
static struct qemu_plugin_hook *h_target;
static struct qemu_plugin_register *arg0;
static bool have_arg0;
static const char *arg0_name;
static int arg0_mips = -1;

static void logf_(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
static void logf_(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    g_autofree char *s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    qemu_plugin_outs(s);
}

static uint64_t load_n(const uint8_t *b, int n)
{
    uint64_t v = 0;
    for (int i = 0; i < n; i++) {
        v |= (uint64_t)b[big_endian ? n - 1 - i : i] << (8 * i);
    }
    return v;
}

static void store_n(uint8_t *b, int n, uint64_t v)
{
    for (int i = 0; i < n; i++) {
        b[big_endian ? n - 1 - i : i] = v >> (8 * i);
    }
}

static void find_arg0(void)
{
    if (have_arg0) {
        return;
    }
    if (arg0_mips >= 0) {
        arg0 = GINT_TO_POINTER(arg0_mips << 1);
        have_arg0 = true;
        return;
    }
    g_autoptr(GArray) rl = qemu_plugin_get_registers();
    for (guint i = 0; i < rl->len; i++) {
        qemu_plugin_reg_descriptor *d =
            &g_array_index(rl, qemu_plugin_reg_descriptor, i);
        if (!strcmp(d->name, arg0_name)) {
            arg0 = d->handle;
            have_arg0 = true;
            return;
        }
    }
}

static bool opt_reuse, opt_direct;

#define MAX_VCPUS 64
static void *direct_ptr[MAX_VCPUS];
static size_t direct_size[MAX_VCPUS];
static bool direct_tried[MAX_VCPUS];
static uint64_t n_checked, n_mismatch;

static uint64_t get_arg0_gdb(void);

/* arg0 in this vCPU's state, or NULL (then fall back to the gdbstub) */
static void *arg0_ptr(unsigned vcpu, size_t *size)
{
    if (vcpu >= MAX_VCPUS) {
        return NULL;
    }
    if (!direct_tried[vcpu]) {
        find_arg0();
        direct_ptr[vcpu] = qemu_plugin_register_ptr(arg0, &direct_size[vcpu]);
        direct_tried[vcpu] = true;
        logf_("HOOKTEST direct vcpu=%u found=%d size=%zu\n", vcpu,
              direct_ptr[vcpu] != NULL, direct_size[vcpu]);
    }
    *size = direct_size[vcpu];
    return direct_ptr[vcpu];
}

static uint64_t load_ptr(const void *p, size_t size)
{
    /* volatile: the reads=K loop discards the value */
    return size == 8 ? *(const volatile uint64_t *)p
                     : *(const volatile uint32_t *)p;
}

static uint64_t get_arg0_direct(unsigned vcpu)
{
    size_t size;
    void *p = arg0_ptr(vcpu, &size);
    uint64_t v;

    if (!p) {
        return get_arg0_gdb();
    }
    v = load_ptr(p, size);
    if (n_checked < 1000) {
        n_checked++;
        if (v != get_arg0_gdb()) {
            n_mismatch++;
        }
    }
    return v;
}

static uint64_t get_arg0_gdb(void)
{
    static __thread GByteArray *keep;
    g_autoptr(GByteArray) fresh = NULL;
    GByteArray *b;

    if (opt_reuse) {
        b = keep ? keep : (keep = g_byte_array_sized_new(16));
        g_byte_array_set_size(b, 0);
    } else {
        b = fresh = g_byte_array_new();
    }
    find_arg0();
    qemu_plugin_read_register(arg0, b);
    return load_n(b->data, b->len > 8 ? 8 : b->len);
}

static __thread unsigned cur_vcpu;   /* one thread per vCPU (MTTCG) */

static uint64_t get_arg0(void)
{
    return opt_direct ? get_arg0_direct(cur_vcpu) : get_arg0_gdb();
}

static void set_arg0(uint64_t v)
{
    g_autoptr(GByteArray) b = g_byte_array_new();
    size_t size;
    void *p = opt_direct ? arg0_ptr(cur_vcpu, &size) : NULL;

    if (p) {
        if (size == 8) {
            *(uint64_t *)p = v;
        } else {
            *(uint32_t *)p = v;
        }
        return;
    }
    find_arg0();
    qemu_plugin_read_register(arg0, b);
    store_n(b->data, wide ? 8 : 4, v);
    qemu_plugin_write_register(arg0, b);
}

/* --- callbacks, shared by both APIs (udata = what to do) ---------------- */

enum what { W_TARGET, W_TARGET2, W_PHASE, W_REGS, W_REDIR, W_FAULT,
            W_KTARGET, W_DUP, W_MID, W_DS, W_SELFRM,
            W_CEQ, W_CLTU, W_CMASK, W_CRANGE, W_CGT, W_CHOST };

static void rp_battery(void);
static void rp_check(void);

static void cb_hook(unsigned int vcpu, uint64_t pc, void *ud);

static void do_phase(void)
{
    uint64_t n = get_arg0();

    logf_("HOOKTEST phase=%" PRIu64 " ktarget=%" PRIu64 " target=%" PRIu64
          " target2=%" PRIu64 " dup=%" PRIu64 " mid=%" PRIu64 " ds=%" PRIu64
          " ceq=%" PRIu64 " cltu=%" PRIu64 " cmask=%" PRIu64 " crange=%" PRIu64
          " cgt=%" PRIu64 " chost=%" PRIu64 "\n", n,
          __atomic_load_n(&n_ktarget, __ATOMIC_RELAXED),
          __atomic_load_n(&n_target, __ATOMIC_RELAXED),
          __atomic_load_n(&n_target2, __ATOMIC_RELAXED),
          __atomic_load_n(&n_dup, __ATOMIC_RELAXED),
          __atomic_load_n(&n_mid, __ATOMIC_RELAXED),
          __atomic_load_n(&n_ds, __ATOMIC_RELAXED),
          n_ceq, n_cltu, n_cmask, n_crange, n_cgt, n_chost);
    __atomic_store_n(&host_flag, n % 2 == 0, __ATOMIC_RELAXED);
    if (opt_dyn && api_hook) {
        if (n == 1 || n == 3) {
            h_target = qemu_plugin_hook_insert(self, a_target, cb_hook,
                                               (void *)W_TARGET);
        } else if ((n == 2 || n == 4) && h_target) {
            qemu_plugin_hook_remove(h_target);
            h_target = NULL;
        }
    }
}

static void do_what(enum what w)
{
    switch (w) {
    case W_TARGET: {
        size_t size;
        void *p = opt_direct && opt_reads ? arg0_ptr(cur_vcpu, &size) : NULL;

        if (p) {
            /* what a plugin does: pointer looked up once, then loads */
            get_arg0_direct(cur_vcpu);      /* keeps the cross-check */
            for (uint64_t i = 1; i < opt_reads; i++) {
                load_ptr(p, size);
            }
        } else {
            for (uint64_t i = 0; i < opt_reads; i++) {
                get_arg0();
            }
        }
        if (opt_rptest) {
            rp_check();
        }
        __atomic_fetch_add(&n_target, 1, __ATOMIC_RELAXED);
        break;
    }
    case W_CEQ: __atomic_fetch_add(&n_ceq, 1, __ATOMIC_RELAXED); break;
    case W_CLTU: __atomic_fetch_add(&n_cltu, 1, __ATOMIC_RELAXED); break;
    case W_CMASK: __atomic_fetch_add(&n_cmask, 1, __ATOMIC_RELAXED); break;
    case W_CRANGE: __atomic_fetch_add(&n_crange, 1, __ATOMIC_RELAXED); break;
    case W_CGT: __atomic_fetch_add(&n_cgt, 1, __ATOMIC_RELAXED); break;
    case W_CHOST: __atomic_fetch_add(&n_chost, 1, __ATOMIC_RELAXED); break;
    case W_DUP:
        __atomic_fetch_add(&n_dup, 1, __ATOMIC_RELAXED);
        break;
    case W_MID:
        __atomic_fetch_add(&n_mid, 1, __ATOMIC_RELAXED);
        break;
    case W_DS:
        __atomic_fetch_add(&n_ds, 1, __ATOMIC_RELAXED);
        break;
    case W_SELFRM:
        /* one vCPU at a time here in the tests; remove from our own callback */
        if (__atomic_add_fetch(&n_selfrm, 1, __ATOMIC_RELAXED) == opt_selfrm) {
            qemu_plugin_hook_remove(h_selfrm);
        }
        break;
    case W_KTARGET:
        __atomic_fetch_add(&n_ktarget, 1, __ATOMIC_RELAXED);
        break;
    case W_TARGET2:
        __atomic_fetch_add(&n_target2, 1, __ATOMIC_RELAXED);
        break;
    case W_PHASE:
        do_phase();
        break;
    case W_REGS:
        set_arg0(get_arg0() + 1000);
        break;
    case W_REDIR:
        qemu_plugin_set_pc(a_other);
        break;
    case W_FAULT: {
        g_autoptr(GByteArray) b = g_byte_array_new();
        uint64_t p = get_arg0();
        int r = qemu_plugin_read_memory_vaddr_fault(p, b, 32,
                                                    QEMU_PLUGIN_VADDR_CURRENT,
                                                    true);
        /* a fault leaves the callback; we get here once paged in */
        logf_("HOOKTEST fault addr=0x%" PRIx64 " result=%d data=%.*s\n", p, r,
              r ? 0 : (int)b->len, (const char *)b->data);
        break;
    }
    }
}

static void cb_hook(unsigned int vcpu, uint64_t pc, void *ud)
{
    cur_vcpu = vcpu;
    do_what((enum what)(uintptr_t)ud);
}

static void cb_insn(unsigned int vcpu, void *ud)
{
    cur_vcpu = vcpu;
    do_what((enum what)(uintptr_t)ud);
}

/* --- conditional hooks: inserted once a vCPU exists (register handles) - */

static void cond_insert(enum what w, struct qemu_plugin_hook_cond *c, size_t n)
{
    if (!qemu_plugin_hook_insert_cond(self, a_target, cb_hook,
                                      (void *)(uintptr_t)w, c, n)) {
        fprintf(stderr, "hooktest: conditional hook refused\n");
        exit(1);
    }
}

static void cond_setup(void)
{
    unsigned sz = wide ? 8 : 4;

    find_arg0();
    if (opt_condbench) {
        struct qemu_plugin_hook_cond c = {
            .reg = arg0, .size = sz, .value = 0xdeadbeef,
            .cond = !strcmp(opt_condbench, "pass") ? QEMU_PLUGIN_HOOK_NE
                                                   : QEMU_PLUGIN_HOOK_EQ };
        cond_insert(W_TARGET, &c, 1);
        return;
    }
    struct qemu_plugin_hook_cond eq = { .reg = arg0, .size = sz,
                                        .cond = QEMU_PLUGIN_HOOK_EQ, .value = 7 };
    struct qemu_plugin_hook_cond ltu = { .reg = arg0, .size = sz,
                                         .cond = QEMU_PLUGIN_HOOK_LTU, .value = 10 };
    struct qemu_plugin_hook_cond mask = { .reg = arg0, .size = sz, .mask = 3,
                                          .cond = QEMU_PLUGIN_HOOK_EQ, .value = 3 };
    struct qemu_plugin_hook_cond range[2] = {
        { .reg = arg0, .size = sz, .cond = QEMU_PLUGIN_HOOK_GEU, .value = 20 },
        { .reg = arg0, .size = sz, .cond = QEMU_PLUGIN_HOOK_LEU, .value = 29 } };
    /* 4 bytes, signed: catches a missing sign extension on 64-bit guests */
    struct qemu_plugin_hook_cond gt = { .reg = arg0, .size = 4,
                                        .cond = QEMU_PLUGIN_HOOK_GT,
                                        .value = (uint64_t)-1 };
    struct qemu_plugin_hook_cond host = { .kind = QEMU_PLUGIN_HOOK_OPERAND_HOST,
                                          .host = &host_flag, .size = 4,
                                          .cond = QEMU_PLUGIN_HOOK_NE, .value = 0 };
    cond_insert(W_CEQ, &eq, 1);
    cond_insert(W_CLTU, &ltu, 1);
    cond_insert(W_CMASK, &mask, 1);
    cond_insert(W_CRANGE, range, 2);
    cond_insert(W_CGT, &gt, 1);
    cond_insert(W_CHOST, &host, 1);
}

#if QEMU_PLUGIN_VERSION >= 7
static void vcpu_init(unsigned int vcpu, void *ud)
#else
static void vcpu_init(qemu_plugin_id_t id, unsigned int vcpu)
#endif
{
    if (vcpu == 0 && (opt_cond || opt_condbench)) {
        cond_setup();
    }
}

/* --- rptest: qemu_plugin_register_ptr() ------------------------------- */

/* expect: 1 = a pointer, 0 = NULL, 2 = a pointer to a nonzero value (the
 * return address at a function entry: catches a stale duplicate register) */
struct rp { const char *name; int mips; int expect; void *p; size_t size;
            struct qemu_plugin_register *h; bool have; };
static struct rp rps[16];
static int n_rps;
static uint64_t rp_checked, rp_mismatch;
static const char *t_name;

static void rp_add(const char *name, int mips, int expect)
{
    rps[n_rps++] = (struct rp){ .name = name, .mips = mips, .expect = expect };
}

static void rp_list(void)
{
    const char *t = t_name;

    if (!strcmp(t, "x86_64")) {
        const char *n[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9", "rsp" };
        for (int i = 0; i < 7; i++) rp_add(n[i], -1, 1);
        rp_add("rip", -1, 0); rp_add("eflags", -1, 0);
    } else if (!strcmp(t, "aarch64")) {
        const char *n[] = { "x0", "x1", "x2", "x3", "x4", "x5", "sp" };
        for (int i = 0; i < 7; i++) rp_add(n[i], -1, 1);
        rp_add("x30", -1, 2);
        rp_add("pc", -1, 0); rp_add("cpsr", -1, 0);
    } else if (!strcmp(t, "arm")) {
        const char *n[] = { "r0", "r1", "r2", "r3", "sp" };
        for (int i = 0; i < 5; i++) rp_add(n[i], -1, 1);
        rp_add("lr", -1, 2);
        rp_add("pc", -1, 0); rp_add("cpsr", -1, 0);
    } else if (!strncmp(t, "mips", 4)) {
        /* no register names: gdb core numbers (a0-a3, t0/t1 or a4/a5, sp, ra) */
        const int n[] = { 4, 5, 6, 7, 8, 9, 29 };
        for (int i = 0; i < 7; i++) rp_add(NULL, n[i], 1);
        rp_add(NULL, 31, 2);
        rp_add(NULL, 37, 0);            /* pc */
    } else if (!strcmp(t, "ppc64")) {
        const char *n[] = { "r3", "r4", "r5", "r6", "r7", "r8", "r1" };
        for (int i = 0; i < 7; i++) rp_add(n[i], -1, 1);
        rp_add("lr", -1, 2);
        rp_add("pc", -1, 0);
    } else if (!strcmp(t, "riscv64")) {
        const char *n[] = { "a0", "a1", "a2", "a3", "a4", "a5", "sp" };
        for (int i = 0; i < 7; i++) rp_add(n[i], -1, 1);
        rp_add("ra", -1, 2);
        rp_add("pc", -1, 0);
    } else if (!strcmp(t, "loongarch64")) {
        const char *n[] = { "r4", "r5", "r6", "r7", "r8", "r9", "r3" };
        for (int i = 0; i < 7; i++) rp_add(n[i], -1, 1);
        rp_add("r1", -1, 2);
        rp_add("pc", -1, 0);
    }
}

/* the gdbstub's core registers, to show a lookup leaves them unchanged */
static GPtrArray *core_regs(void)
{
    GPtrArray *a = g_ptr_array_new();

    if (!strncmp(t_name, "mips", 4)) {
        for (int i = 0; i < 38; i++) {
            g_ptr_array_add(a, GINT_TO_POINTER(i << 1));
        }
        return a;
    }
    g_autoptr(GArray) rl = qemu_plugin_get_registers();
    for (guint i = 0; i < rl->len; i++) {
        qemu_plugin_reg_descriptor *d =
            &g_array_index(rl, qemu_plugin_reg_descriptor, i);
        const char *f = d->feature ? d->feature : "";
        if (strstr(f, ".core") || g_str_has_suffix(f, ".cpu") ||
            g_str_has_suffix(f, ".base")) {
            g_ptr_array_add(a, d->handle);
        }
    }
    return a;
}

static GByteArray *dump(GPtrArray *regs)
{
    GByteArray *all = g_byte_array_new();
    g_autoptr(GByteArray) b = g_byte_array_new();

    for (guint i = 0; i < regs->len; i++) {
        g_byte_array_set_size(b, 0);
        qemu_plugin_read_register(regs->pdata[i], b);
        g_byte_array_append(all, b->data, b->len);
    }
    return all;
}

static uint64_t gdb_value(struct qemu_plugin_register *h)
{
    g_autoptr(GByteArray) b = g_byte_array_new();
    qemu_plugin_read_register(h, b);
    return load_n(b->data, b->len > 8 ? 8 : b->len);
}

static void rp_battery(void)
{
    g_autoptr(GPtrArray) core = core_regs();
    g_autoptr(GArray) rl = qemu_plugin_get_registers();

    rp_list();
    for (int i = 0; i < n_rps; i++) {
        struct rp *r = &rps[i];
        bool is_pc = r->expect == 0 && (r->mips == 37 || !strcmp(r->name ? r->name : "", "pc")
                                        || !strcmp(r->name ? r->name : "", "rip"));

        if (r->mips >= 0) {
            r->h = GINT_TO_POINTER((r->mips << 1) | (is_pc ? 1 : 0));
            r->have = true;
        } else {
            for (guint j = 0; j < rl->len; j++) {
                qemu_plugin_reg_descriptor *d =
                    &g_array_index(rl, qemu_plugin_reg_descriptor, j);
                if (!strcmp(d->name, r->name)) {
                    r->h = d->handle;       /* may be NULL: register 0 */
                    r->have = true;
                    break;                  /* first: the core feature */
                }
            }
        }
        if (!r->have) {
            logf_("HOOKTEST rp name=%s found=-1 (no such register)\n", r->name);
            continue;
        }
        g_autoptr(GByteArray) before = dump(core);
        r->p = qemu_plugin_register_ptr(r->h, &r->size);
        g_autoptr(GByteArray) after = dump(core);
        bool same = before->len == after->len &&
                    !memcmp(before->data, after->data, before->len);
        bool eq = !r->p || load_ptr(r->p, r->size) == gdb_value(r->h);
        char nb[16];
        snprintf(nb, sizeof(nb), "$%d", r->mips);
        logf_("HOOKTEST rp name=%s found=%d size=%zu expect=%d same_state=%d "
              "eq=%d nonzero=%d core_regs=%u\n", r->name ? r->name : nb,
              r->p != NULL, r->p ? r->size : 0, r->expect, same, eq,
              r->p && load_ptr(r->p, r->size) != 0, core->len);
    }
}

/* every pointer found agrees with the gdbstub, on the first 1000 hits */
static void rp_check(void)
{
    if (!n_rps) {
        rp_battery();
    }
    if (rp_checked >= 1000) {
        return;
    }
    rp_checked++;
    for (int i = 0; i < n_rps; i++) {
        if (rps[i].p && load_ptr(rps[i].p, rps[i].size) != gdb_value(rps[i].h)) {
            rp_mismatch++;
        }
    }
}

/* --- api=insn ---------------------------------------------------------- */

static void tb_trans(struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    const struct { uint64_t *a; enum what w; } sites[] = {
        { &a_target, W_TARGET }, { &a_target2, W_TARGET2 },
        { &a_phase, W_PHASE }, { &a_regs, W_REGS }, { &a_redir, W_REDIR },
        { &a_fault, W_FAULT }, { &a_ktarget, W_KTARGET },
        { &a_mid, W_MID }, { &a_ds, W_DS },
    };

    n_trans++;
    for (size_t i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t pc = qemu_plugin_insn_vaddr(insn);

        for (size_t j = 0; j < G_N_ELEMENTS(sites); j++) {
            if (*sites[j].a && pc == *sites[j].a) {
                qemu_plugin_register_vcpu_insn_exec_cb(
                    insn, cb_insn, QEMU_PLUGIN_CB_RW_REGS_PC,
                    (void *)(uintptr_t)sites[j].w);
            }
        }
    }
}

#if QEMU_PLUGIN_VERSION >= 7
static void tb_trans_cb(struct qemu_plugin_tb *tb, void *ud) { tb_trans(tb); }
static void at_exit(void *ud)
#else
static void tb_trans_cb(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    tb_trans(tb);
}
static void at_exit(qemu_plugin_id_t id, void *ud)
#endif
{
    logf_("HOOKTEST exit target=%" PRIu64 " target2=%" PRIu64
          " ktarget=%" PRIu64 " translations=%" PRIu64 " dup=%" PRIu64
          " mid=%" PRIu64 " ds=%" PRIu64 " selfrm=%" PRIu64 "\n", n_target,
          n_target2, n_ktarget, n_trans, n_dup, n_mid, n_ds, n_selfrm);
    if (opt_direct) {
        logf_("HOOKTEST direct checked=%" PRIu64 " mismatch=%" PRIu64 "\n",
              n_checked, n_mismatch);
    }
    if (opt_rptest) {
        logf_("HOOKTEST rp checked=%" PRIu64 " mismatch=%" PRIu64 "\n",
              rp_checked, rp_mismatch);
    }
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    const char *t = info->target_name;
    bool arm_now = false;

    self = id;
    t_name = t;
    if (!strcmp(t, "x86_64")) {
        arg0_name = "rdi";
    } else if (!strcmp(t, "aarch64")) {
        arg0_name = "x0";
    } else if (!strcmp(t, "arm")) {
        arg0_name = "r0", wide = false;
    } else if (!strcmp(t, "mips") || !strcmp(t, "mipsel")) {
        arg0_mips = 4, wide = false, big_endian = !strcmp(t, "mips");
    } else if (!strcmp(t, "mips64") || !strcmp(t, "mips64el")) {
        arg0_mips = 4, big_endian = !strcmp(t, "mips64");
    } else if (!strcmp(t, "ppc64")) {
        arg0_name = "r3", big_endian = true;
    } else if (!strcmp(t, "riscv64")) {
        arg0_name = "a0";
    } else if (!strcmp(t, "loongarch64")) {
        arg0_name = "r4";
    } else {
        return -1;
    }

    for (int i = 0; i < argc; i++) {
        g_auto(GStrv) kv = g_strsplit(argv[i], "=", 2);
        const char *k = kv[0], *v = kv[1] ? kv[1] : "";
        uint64_t n = g_ascii_strtoull(v, NULL, 0);

        if (!strcmp(k, "api")) {
            api_hook = !strcmp(v, "hook");
        } else if (!strcmp(k, "target")) {
            a_target = n;
        } else if (!strcmp(k, "target2")) {
            a_target2 = n;
        } else if (!strcmp(k, "phase")) {
            a_phase = n;
        } else if (!strcmp(k, "regs")) {
            a_regs = n;
        } else if (!strcmp(k, "redir")) {
            a_redir = n;
        } else if (!strcmp(k, "other")) {
            a_other = n;
        } else if (!strcmp(k, "fault")) {
            a_fault = n;
        } else if (!strcmp(k, "ktarget")) {
            a_ktarget = n;
        } else if (!strcmp(k, "reuse")) {
            qemu_plugin_bool_parse(k, v, &opt_reuse);
        } else if (!strcmp(k, "direct")) {
            qemu_plugin_bool_parse(k, v, &opt_direct);
        } else if (!strcmp(k, "reads")) {
            opt_reads = n;
        } else if (!strcmp(k, "many")) {
            opt_many = n;
        } else if (!strcmp(k, "dyn")) {
            qemu_plugin_bool_parse(k, v, &opt_dyn);
        } else if (!strcmp(k, "cond")) {
            qemu_plugin_bool_parse(k, v, &opt_cond);
        } else if (!strcmp(k, "condbench")) {
            opt_condbench = g_strdup(v);
        } else if (!strcmp(k, "dup")) {
            qemu_plugin_bool_parse(k, v, &opt_dup);
        } else if (!strcmp(k, "rptest")) {
            qemu_plugin_bool_parse(k, v, &opt_rptest);
        } else if (!strcmp(k, "selfrm")) {
            opt_selfrm = n;
        } else if (!strcmp(k, "mid")) {
            a_mid = n;
        } else if (!strcmp(k, "ds")) {
            a_ds = n;
        } else if (!strcmp(k, "arm")) {
            qemu_plugin_bool_parse(k, v, &arm_now);
        } else if (!strcmp(k, "endian")) {
            big_endian = !strcmp(v, "big");
        } else {
            fprintf(stderr, "hooktest: unknown option %s\n", k);
            return -1;
        }
    }

    if (api_hook) {
        const struct { uint64_t a; enum what w; } sites[] = {
            { (opt_dyn && !arm_now) || opt_condbench ? 0 : a_target, W_TARGET },
            { a_target2, W_TARGET2 }, { a_phase, W_PHASE },
            { a_regs, W_REGS }, { a_redir, W_REDIR }, { a_fault, W_FAULT },
            { a_ktarget, W_KTARGET },
            { opt_dup ? a_target : 0, W_DUP }, { a_mid, W_MID },
            { a_ds, W_DS }, { opt_selfrm ? a_target2 : 0, W_SELFRM },
        };
        for (size_t j = 0; j < G_N_ELEMENTS(sites); j++) {
            if (sites[j].a) {
                struct qemu_plugin_hook *h =
                    qemu_plugin_hook_insert(id, sites[j].a, cb_hook,
                                            (void *)(uintptr_t)sites[j].w);
                if (!h) {
                    fprintf(stderr, "hooktest: hooks unavailable\n");
                    return -1;
                }
                if (sites[j].w == W_TARGET) {
                    h_target = h;
                } else if (sites[j].w == W_SELFRM) {
                    h_selfrm = h;
                }
            }
        }
        /* page 0x1000 of the guest: never code on any penguin target */
        for (uint64_t i = 0; i < opt_many; i++) {
            qemu_plugin_hook_insert(id, 0x1000 + 4 * i, cb_hook,
                                    (void *)W_TARGET2);
        }
    } else {
#if QEMU_PLUGIN_VERSION >= 7
        qemu_plugin_register_vcpu_tb_trans_cb(id, tb_trans_cb, NULL);
#else
        qemu_plugin_register_vcpu_tb_trans_cb(id, tb_trans_cb);
#endif
    }
    qemu_plugin_register_atexit_cb(id, at_exit, NULL);
#if QEMU_PLUGIN_VERSION >= 7
    qemu_plugin_register_vcpu_init_cb(id, vcpu_init, NULL);
#else
    qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
#endif
    return 0;
}
