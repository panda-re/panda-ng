/*
 * syscalls: Linux syscall hooks from the hypervisor, without a guest
 * kernel module, on TCG address hooks.
 *
 *   profile=FILE   kernel profile from guest/pk/sysprof.py: where each
 *                  syscall handler is, where its arguments are on entry,
 *                  the return register and address, how to find the
 *                  current task, task_struct and syscall metadata offsets
 *   rules=FILE     what to hook, one rule per line (below)
 *   endian=big|little   byte order of gdbstub register reads (default:
 *                  the target's)
 *   mode=handler|trap   where events come from (default handler):
 *                  handler: hooks on the syscall handlers, as below;
 *                  trap: QEMU's syscall events at the trap instruction and
 *                  the return to user mode, which see every ABI (compat
 *                  tasks too); the profile then supplies names, argument
 *                  types and the current task only
 *
 * Cost model.  Every hook is on a syscall's own handler, so a syscall no
 * rule names is not instrumented at all.  A rule's numeric argument
 * filters become conditions in the translated code when the arguments are
 * in registers at handler entry (armel, mips, ppc64, loongarch64), so a
 * call that fails them costs a compare and a branch.  Where handlers take
 * a struct pt_regs (x86_64, aarch64, riscv64) the filters run in C after
 * one read of the argument block.  Process and string filters run in C,
 * cheapest first.  Return events come from a hook at the handler's return
 * address, enabled by a plugin variable only while calls are outstanding.
 *
 * Rules, one per line (# starts a comment):
 *   NAME|*  [enter] [return] [child] [log] [count] [tag=WORD]
 *           [comm=NAME] [pid=N] [tgid=N]
 *           [aI OP V]           I = 0..5, OP: = != < <= > >= (signed)
 *                               <u <=u >u >=u (unsigned), &= (all bits of
 *                               V set), !& (none set); written aI=V etc.
 *           [aI=LO..HI]         range, inclusive
 *           [sI=STR] [sI^=PRE] [sI$=SUF] [sI~=SUB]   string argument
 *           [ret OP V] [ret=ok] [ret=err]           (return filters)
 *           [override=V]        skip the syscall, returning V
 *           [setI=V]            change argument I on entry
 *           [setret=V]          change the return value
 *   Default is "enter".  "child" reports the new task's first return from
 *   fork/vfork/clone/clone3 (from ret_from_fork).  Values are decimal or
 *   0x hex, negative allowed.  Comparisons are 32-bit for arguments whose
 *   type is a 32-bit integer, full width otherwise.
 *
 * Complex cases:
 *   - a string filter on user memory that is not resident at entry is
 *     evaluated at return (the kernel has faulted it in by then); the
 *     entry event is then reported late, with late=1
 *   - a call that blocks is matched to its return by the task's kernel
 *     stack pointer, whichever vCPU it returns on
 *   - exit and exit_group never return; no return is armed for them
 *   - override= returns to the caller at once and reports the return
 */
#include <glib.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define MAX_VCPUS 64
#define MAX_REGS 16
#define NARGS 6

static qemu_plugin_id_t self;
static bool big_endian;

static void logf_(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
static void logf_(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    g_autofree char *s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    qemu_plugin_outs(s);
}

/* --- registers: by name, read through a direct pointer when possible --- */

typedef struct {
    char *name;
    struct qemu_plugin_register *h;
    bool have;
    void *p[MAX_VCPUS];
    size_t size[MAX_VCPUS];
    bool tried[MAX_VCPUS];
} Reg;

static Reg regs[MAX_REGS];
static int n_regs;

static int reg_index(const char *name)
{
    for (int i = 0; i < n_regs; i++) {
        if (!strcmp(regs[i].name, name)) {
            return i;
        }
    }
    g_assert(n_regs < MAX_REGS);
    regs[n_regs].name = g_strdup(name);
    return n_regs++;
}

/* Find handles; needs a running vCPU (arm has no registers at vCPU init). */
static bool regs_resolve(void)
{
    g_autoptr(GArray) rl = qemu_plugin_get_registers();

    for (int i = 0; i < n_regs; i++) {
        Reg *r = &regs[i];
        if (r->name[0] == '$') {            /* mips: gdbstub core number */
            r->h = GINT_TO_POINTER(atoi(r->name + 1) << 1);
            r->have = true;
            continue;
        }
        for (guint j = 0; j < rl->len; j++) {
            qemu_plugin_reg_descriptor *d =
                &g_array_index(rl, qemu_plugin_reg_descriptor, j);
            /*
             * First match: the core feature.  ppc lists lr and ctr again
             * among its SPRs, read from env->spr[], which TCG does not keep.
             */
            if (!strcmp(d->name, r->name)) {
                r->h = d->handle;           /* may be NULL: register 0 */
                r->have = true;
                break;
            }
        }
        if (!r->have) {
            logf_("SC error: no register %s\n", r->name);
            return false;
        }
    }
    return true;
}

static uint64_t load_n(const uint8_t *b, size_t n, bool be)
{
    uint64_t v = 0;
    for (size_t i = 0; i < n; i++) {
        v |= (uint64_t)b[be ? n - 1 - i : i] << (8 * i);
    }
    return v;
}

static uint64_t reg_read(int i, unsigned vcpu)
{
    Reg *r = &regs[i];

    if (vcpu < MAX_VCPUS) {
        if (!r->tried[vcpu]) {
            r->p[vcpu] = qemu_plugin_register_ptr(r->h, &r->size[vcpu]);
            r->tried[vcpu] = true;
        }
        if (r->p[vcpu]) {
            return r->size[vcpu] == 8 ? *(uint64_t *)r->p[vcpu]
                                      : *(uint32_t *)r->p[vcpu];
        }
    }
    g_autoptr(GByteArray) b = g_byte_array_new();
    qemu_plugin_read_register(r->h, b);
    return load_n(b->data, b->len > 8 ? 8 : b->len, big_endian);
}

static void reg_write(int i, unsigned vcpu, uint64_t v)
{
    Reg *r = &regs[i];

    reg_read(i, vcpu);                      /* looks the pointer up */
    if (vcpu < MAX_VCPUS && r->p[vcpu]) {
        if (r->size[vcpu] == 8) {
            *(uint64_t *)r->p[vcpu] = v;
        } else {
            *(uint32_t *)r->p[vcpu] = v;
        }
        return;
    }
    g_autoptr(GByteArray) b = g_byte_array_new();
    qemu_plugin_read_register(r->h, b);
    for (size_t k = 0; k < b->len && k < 8; k++) {
        b->data[big_endian ? b->len - 1 - k : k] = v >> (8 * k);
    }
    qemu_plugin_write_register(r->h, b);
}

/* --- guest memory -------------------------------------------------------- */

static unsigned ptr_size = 8;

static bool mem_read(uint64_t va, void *dst, size_t n)
{
    static __thread GByteArray *b;

    if (!b) {
        b = g_byte_array_new();
    }
    if (!qemu_plugin_read_memory_vaddr(va, b, n)) {
        return false;
    }
    memcpy(dst, b->data, n);
    return true;
}

static bool mem_word(uint64_t va, size_t n, uint64_t *v)
{
    uint8_t b[8];

    if (!mem_read(va, b, n)) {
        return false;
    }
    /* guest memory is in the guest's order: the target's, as for registers */
    *v = load_n(b, n, big_endian);
    return true;
}

static bool mem_write_word(uint64_t va, size_t n, uint64_t v)
{
    g_autoptr(GByteArray) b = g_byte_array_sized_new(n);

    g_byte_array_set_size(b, n);
    for (size_t k = 0; k < n; k++) {
        b->data[big_endian ? n - 1 - k : k] = v >> (8 * k);
    }
    return qemu_plugin_write_memory_vaddr(va, b);
}

/*
 * User memory, translated as an unprivileged access: at syscall entry the
 * kernel may have user access switched off (riscv SUM, arm64/x86 PAN/SMAP,
 * ppc64 KUAP), which a read at the current privilege would honour.  Never
 * faults.
 */
static bool user_read(uint64_t va, void *dst, size_t n)
{
    static __thread GByteArray *b;

    if (!b) {
        b = g_byte_array_new();
    }
    if (qemu_plugin_read_memory_vaddr_fault(va, b, n, QEMU_PLUGIN_VADDR_USER,
                                            false) != QEMU_PLUGIN_VADDR_OK) {
        return false;
    }
    memcpy(dst, b->data, n);
    return true;
}

/* A NUL-terminated user string, page by page, without faulting. */
static bool user_string(uint64_t va, char *dst, size_t max)
{
    size_t got = 0;

    while (got + 1 < max) {
        size_t chunk = 4096 - ((va + got) & 4095);
        if (chunk > max - 1 - got) {
            chunk = max - 1 - got;
        }
        if (!user_read(va + got, dst + got, chunk)) {
            return false;
        }
        void *nul = memchr(dst + got, 0, chunk);
        if (nul) {
            return true;
        }
        got += chunk;
    }
    dst[max - 1] = 0;
    return true;
}

/* A NUL-terminated kernel string. */
static bool mem_string(uint64_t va, char *dst, size_t max)
{
    for (size_t i = 0; i + 1 < max; i++) {
        if (!mem_read(va + i, dst + i, 1)) {
            return false;
        }
        if (!dst[i]) {
            return true;
        }
    }
    dst[max - 1] = 0;
    return true;
}

/* --- locations from the profile ------------------------------------------ */

typedef struct {
    bool mem;
    int reg;
    uint64_t mask;          /* 0: none */
    int64_t off;
    unsigned size;
} Loc;

static bool loc_parse(const char *s, Loc *l)
{
    memset(l, 0, sizeof(*l));
    if (g_str_has_prefix(s, "r:")) {
        l->reg = reg_index(s + 2);
        return true;
    }
    if (!g_str_has_prefix(s, "m:")) {
        return false;
    }
    g_autofree char *t = g_strdup(s + 2);
    char *colon = strrchr(t, ':'), *plus, *amp;
    if (!colon) {
        return false;
    }
    *colon = 0;
    l->size = atoi(colon + 1);
    plus = strrchr(t, '+');
    if (!plus) {
        return false;
    }
    *plus = 0;
    l->off = (int64_t)g_ascii_strtoull(plus + 1, NULL, 0);   /* may be an address */
    amp = strchr(t, '&');
    if (amp) {
        *amp = 0;
        l->mask = g_ascii_strtoull(amp + 1, NULL, 0);
    }
    l->mem = true;
    l->reg = reg_index(t);
    return true;
}

static bool loc_read(const Loc *l, unsigned vcpu, uint64_t *v)
{
    uint64_t r = reg_read(l->reg, vcpu);

    if (!l->mem) {
        *v = r;
        return true;
    }
    if (l->mask) {
        r &= l->mask;
    }
    return mem_word(r + l->off, l->size, v);
}

static bool loc_write(const Loc *l, unsigned vcpu, uint64_t v)
{
    if (!l->mem) {
        reg_write(l->reg, vcpu, v);
        return true;
    }
    uint64_t r = reg_read(l->reg, vcpu);
    if (l->mask) {
        r &= l->mask;
    }
    return mem_write_word(r + l->off, l->size, v);
}

/* --- the profile --------------------------------------------------------- */

typedef struct Sys {
    char *name;
    uint64_t handler, meta;
    int nr;                     /* from the metadata, read once booted */
    int nargs;                  /* -1: unknown */
    bool is_str[NARGS], is32[NARGS];
    bool noreturn;
    uint64_t ra;                /* return address, learned on first call */
    /* trap mode */
    GPtrArray *trules;          /* Rule, at most 32 */
    bool trap_noreturn, forkish;
} Sys;

static struct {
    bool wrapper;
    Loc args[NARGS], ra, cur, ucur;  /* ucur: the current task from user mode */
    bool have_ucur;
    int abi;                    /* the native enum qemu_plugin_syscall_abi */
    GPtrArray *csys;            /* "ABI NAME NR" of other ABIs */
    int ret, sp;
    int64_t spdelta;
    int64_t comm, pid, tgid, mm;
    int64_t m_name, m_nr, m_nb, m_types, m_args;
    uint64_t bootstrap, fork_child;
    GPtrArray *sys;             /* Sys */
    /* the argument block, read at once when all args are memory at one base */
    bool block;
    int64_t block_lo;
    unsigned block_len;
} P;

static const char *const abi_names[QEMU_PLUGIN_SYSCALL_ABI_COUNT] = {
    [QEMU_PLUGIN_SYSCALL_ABI_X86_64] = "x86_64",
    [QEMU_PLUGIN_SYSCALL_ABI_I386] = "i386",
    [QEMU_PLUGIN_SYSCALL_ABI_AARCH64] = "aarch64",
    [QEMU_PLUGIN_SYSCALL_ABI_ARM] = "arm",
    [QEMU_PLUGIN_SYSCALL_ABI_ARM_OABI] = "arm_oabi",
    [QEMU_PLUGIN_SYSCALL_ABI_MIPS] = "mips",
    [QEMU_PLUGIN_SYSCALL_ABI_PPC64] = "ppc64",
    [QEMU_PLUGIN_SYSCALL_ABI_PPC] = "ppc",
    [QEMU_PLUGIN_SYSCALL_ABI_RISCV64] = "riscv64",
    [QEMU_PLUGIN_SYSCALL_ABI_RISCV32] = "riscv32",
    [QEMU_PLUGIN_SYSCALL_ABI_LOONGARCH64] = "loongarch64",
};

static int abi_by_name(const char *n)
{
    for (int i = 0; i < QEMU_PLUGIN_SYSCALL_ABI_COUNT; i++) {
        if (!strcmp(abi_names[i], n)) {
            return i;
        }
    }
    return -1;
}

static int abi_of_arch(const char *a)
{
    if (!strcmp(a, "armel")) {
        return QEMU_PLUGIN_SYSCALL_ABI_ARM;
    } else if (g_str_has_prefix(a, "mips")) {
        return QEMU_PLUGIN_SYSCALL_ABI_MIPS;
    } else if (g_str_has_prefix(a, "powerpc64")) {
        return QEMU_PLUGIN_SYSCALL_ABI_PPC64;
    }
    return abi_by_name(a);
}

static bool abi_is32(int abi)
{
    return abi == QEMU_PLUGIN_SYSCALL_ABI_I386 || abi == QEMU_PLUGIN_SYSCALL_ABI_ARM ||
           abi == QEMU_PLUGIN_SYSCALL_ABI_ARM_OABI || abi == QEMU_PLUGIN_SYSCALL_ABI_PPC ||
           abi == QEMU_PLUGIN_SYSCALL_ABI_RISCV32;
}

static bool profile_load(const char *path)
{
    g_autofree char *text = NULL;
    g_auto(GStrv) lines = NULL;

    if (!g_file_get_contents(path, &text, NULL, NULL)) {
        fprintf(stderr, "syscalls: cannot read %s\n", path);
        return false;
    }
    P.sys = g_ptr_array_new();
    P.csys = g_ptr_array_new();
    lines = g_strsplit(text, "\n", -1);
    for (char **lp = lines; *lp; lp++) {
        g_auto(GStrv) w = g_strsplit_set(*lp, " \t", -1);
        int n = g_strv_length(w);
        if (n < 2 || w[0][0] == '#') {
            continue;
        }
        if (!strcmp(w[0], "ptr")) {
            ptr_size = atoi(w[1]);
        } else if (!strcmp(w[0], "wrapper")) {
            P.wrapper = atoi(w[1]);
        } else if (!strcmp(w[0], "args")) {
            for (int i = 0; i < NARGS && i + 1 < n; i++) {
                if (!loc_parse(w[i + 1], &P.args[i])) {
                    return false;
                }
            }
        } else if (!strcmp(w[0], "ret")) {
            P.ret = reg_index(w[1]);
        } else if (!strcmp(w[0], "ra")) {
            loc_parse(w[1], &P.ra);
        } else if (!strcmp(w[0], "sp")) {
            P.sp = reg_index(w[1]);
        } else if (!strcmp(w[0], "spdelta")) {
            P.spdelta = atoi(w[1]);
        } else if (!strcmp(w[0], "current")) {
            loc_parse(w[1], &P.cur);
        } else if (!strcmp(w[0], "ucurrent")) {
            P.have_ucur = loc_parse(w[1], &P.ucur);
        } else if (!strcmp(w[0], "arch")) {
            P.abi = abi_of_arch(w[1]);
        } else if (!strcmp(w[0], "csys") && n >= 4) {
            g_ptr_array_add(P.csys, g_strdup_printf("%s %s %s", w[1], w[2], w[3]));
        } else if (!strcmp(w[0], "task") || !strcmp(w[0], "meta")) {
            for (int i = 1; i < n; i++) {
                g_auto(GStrv) kv = g_strsplit(w[i], "=", 2);
                int64_t v = g_ascii_strtoll(kv[1] ? kv[1] : "-1", NULL, 0);
                const char *k = kv[0];
                if (!strcmp(k, "comm")) P.comm = v;
                else if (!strcmp(k, "pid")) P.pid = v;
                else if (!strcmp(k, "tgid")) P.tgid = v;
                else if (!strcmp(k, "mm")) P.mm = v;
                else if (!strcmp(k, "name")) P.m_name = v;
                else if (!strcmp(k, "syscall_nr")) P.m_nr = v;
                else if (!strcmp(k, "nb_args")) P.m_nb = v;
                else if (!strcmp(k, "types")) P.m_types = v;
                else if (!strcmp(k, "args")) P.m_args = v;
            }
        } else if (!strcmp(w[0], "bootstrap")) {
            P.bootstrap = g_ascii_strtoull(w[1], NULL, 0);
        } else if (!strcmp(w[0], "fork_child")) {
            P.fork_child = g_ascii_strtoull(w[1], NULL, 0);
        } else if (!strcmp(w[0], "sys") && n >= 4) {
            Sys *s = g_new0(Sys, 1);
            s->name = g_strdup(w[1]);
            s->handler = g_ascii_strtoull(w[2], NULL, 0);
            s->meta = g_ascii_strtoull(w[3], NULL, 0);
            s->nr = -1;
            s->nargs = -1;
            s->noreturn = !strcmp(s->name, "exit") || !strcmp(s->name, "exit_group");
            g_ptr_array_add(P.sys, s);
        }
    }

    /* one read for all arguments when they share a base register */
    P.block = P.args[0].mem;
    int64_t lo = INT64_MAX, hi = 0;
    for (int i = 0; i < NARGS; i++) {
        Loc *l = &P.args[i];
        if (!l->mem || l->reg != P.args[0].reg || l->mask != P.args[0].mask) {
            P.block = false;
            break;
        }
        lo = MIN(lo, l->off);
        hi = MAX(hi, l->off + (int64_t)l->size);
    }
    if (P.block) {
        P.block_lo = lo;
        P.block_len = hi - lo;
    }
    return P.bootstrap && P.sys->len;
}

static Sys *sys_by_name(const char *name)
{
    for (guint i = 0; i < P.sys->len; i++) {
        Sys *s = g_ptr_array_index(P.sys, i);
        if (!strcmp(s->name, name)) {
            return s;
        }
    }
    return NULL;
}

/* Argument types from the kernel's syscall metadata (CONFIG_FTRACE_SYSCALLS). */
static bool type_is32(const char *t)
{
    static const char *const w32[] = {
        "int", "unsigned int", "unsigned", "u32", "__u32", "s32", "__s32",
        "pid_t", "uid_t", "gid_t", "mode_t", "umode_t", "key_t", "clockid_t",
        "timer_t", "qid_t", "rwf_t", "key_serial_t", "aio_context_t32",
    };
    if (strchr(t, '*')) {
        return false;
    }
    for (size_t i = 0; i < G_N_ELEMENTS(w32); i++) {
        if (!strcmp(t, w32[i])) {
            return true;
        }
    }
    return false;
}

static void meta_read(Sys *s)
{
    uint64_t nb, types, tp;
    char t[96];

    if (!s->meta || !mem_word(s->meta + P.m_nb, 4, &nb) || nb > NARGS ||
        !mem_word(s->meta + P.m_types, ptr_size, &types)) {
        return;
    }
    s->nargs = nb;
    for (unsigned i = 0; i < nb; i++) {
        if (mem_word(types + i * ptr_size, ptr_size, &tp) &&
            mem_string(tp, t, sizeof(t))) {
            /* input strings only: a read buffer is a char * too */
            s->is_str[i] = strstr(t, "const char") && strchr(t, '*');
            s->is32[i] = type_is32(t);
        }
    }
}

static int sys_nr(Sys *s)
{
    uint64_t nr;

    if (s->nr < 0 && s->meta && mem_word(s->meta + P.m_nr, 4, &nr)) {
        s->nr = (int32_t)nr;
    }
    return s->nr;
}

/* --- rules --------------------------------------------------------------- */

enum { F_EQ, F_NE, F_LT, F_LE, F_GT, F_GE, F_LTU, F_LEU, F_GTU, F_GEU,
       F_ALL, F_NONE, F_RANGE };
enum { S_EXACT, S_PREFIX, S_SUFFIX, S_SUB };

typedef struct { int arg; int op; int64_t v, v2; bool inl; } NumF;
typedef struct { int arg; int op; char *s; } StrF;

typedef struct Rule {
    char *tag;
    char *name;                 /* NULL: every syscall */
    bool enter, ret, child, log, count;
    char *comm;
    int64_t pid, tgid;          /* -1: any */
    NumF num[8];
    int n_num;
    StrF str[4];
    int n_str;
    bool ret_f;
    NumF retf;
    bool override, setret;
    int64_t override_v, setret_v;
    bool set[NARGS];
    int64_t set_v[NARGS];
    uint64_t n_enter, n_return, n_child, n_late;
} Rule;

static GPtrArray *rules;

static bool parse_op(const char *s, int *op, const char **rest)
{
    static const struct { const char *t; int op; } ops[] = {
        { "<=u", F_LEU }, { ">=u", F_GEU }, { "<u", F_LTU }, { ">u", F_GTU },
        { "!=", F_NE }, { "<=", F_LE }, { ">=", F_GE }, { "&=", F_ALL },
        { "!&", F_NONE }, { "<", F_LT }, { ">", F_GT }, { "=", F_EQ },
    };
    for (size_t i = 0; i < G_N_ELEMENTS(ops); i++) {
        if (g_str_has_prefix(s, ops[i].t)) {
            *op = ops[i].op;
            *rest = s + strlen(ops[i].t);
            return true;
        }
    }
    return false;
}

static Rule *rule_parse(char *line)
{
    g_auto(GStrv) w = g_strsplit_set(g_strstrip(line), " \t", -1);
    Rule *r;

    if (!w[0] || !w[0][0] || w[0][0] == '#') {
        return NULL;
    }
    r = g_new0(Rule, 1);
    r->name = strcmp(w[0], "*") ? g_strdup(w[0]) : NULL;
    r->pid = r->tgid = -1;
    for (int i = 1; w[i]; i++) {
        char *t = w[i];
        const char *rest;
        int op;

        if (!*t) continue;
        if (!strcmp(t, "enter")) r->enter = true;
        else if (!strcmp(t, "return")) r->ret = true;
        else if (!strcmp(t, "child")) r->child = true;
        else if (!strcmp(t, "log")) r->log = true;
        else if (!strcmp(t, "count")) r->count = true;
        else if (g_str_has_prefix(t, "tag=")) r->tag = g_strdup(t + 4);
        else if (g_str_has_prefix(t, "comm=")) r->comm = g_strdup(t + 5);
        else if (g_str_has_prefix(t, "pid=")) r->pid = g_ascii_strtoll(t + 4, NULL, 0);
        else if (g_str_has_prefix(t, "tgid=")) r->tgid = g_ascii_strtoll(t + 5, NULL, 0);
        else if (g_str_has_prefix(t, "override=")) {
            r->override = true;
            r->override_v = g_ascii_strtoll(t + 9, NULL, 0);
        } else if (g_str_has_prefix(t, "setret=")) {
            r->setret = true;
            r->setret_v = g_ascii_strtoll(t + 7, NULL, 0);
        } else if (g_str_has_prefix(t, "set") && g_ascii_isdigit(t[3]) && t[4] == '=') {
            r->set[t[3] - '0'] = true;
            r->set_v[t[3] - '0'] = g_ascii_strtoll(t + 5, NULL, 0);
        } else if (!strcmp(t, "ret=ok")) {
            r->ret_f = true;
            r->retf = (NumF){ .op = F_GE, .v = 0 };
        } else if (!strcmp(t, "ret=err")) {
            r->ret_f = true;
            r->retf = (NumF){ .op = F_RANGE, .v = -4095, .v2 = -1 };
        } else if (g_str_has_prefix(t, "ret") && parse_op(t + 3, &op, &rest)) {
            r->ret_f = true;
            r->retf = (NumF){ .op = op, .v = g_ascii_strtoll(rest, NULL, 0) };
        } else if (t[0] == 's' && g_ascii_isdigit(t[1]) && r->n_str < 4) {
            StrF *f = &r->str[r->n_str++];
            f->arg = t[1] - '0';
            const char *p = t + 2;
            if (g_str_has_prefix(p, "^=")) f->op = S_PREFIX, p += 2;
            else if (g_str_has_prefix(p, "$=")) f->op = S_SUFFIX, p += 2;
            else if (g_str_has_prefix(p, "~=")) f->op = S_SUB, p += 2;
            else if (*p == '=') f->op = S_EXACT, p += 1;
            else goto bad;
            f->s = g_strdup(p);
        } else if (t[0] == 'a' && g_ascii_isdigit(t[1]) && parse_op(t + 2, &op, &rest)
                   && r->n_num < 8) {
            NumF *f = &r->num[r->n_num++];
            const char *dots = strstr(rest, "..");
            f->arg = t[1] - '0';
            f->op = op;
            f->v = g_ascii_strtoll(rest, NULL, 0);
            if (op == F_EQ && dots) {
                f->op = F_RANGE;
                f->v2 = g_ascii_strtoll(dots + 2, NULL, 0);
            }
        } else {
            goto bad;
        }
        continue;
bad:
        fprintf(stderr, "syscalls: bad rule term '%s'\n", t);
        exit(1);
    }
    if (!r->enter && !r->ret && !r->child) {
        r->enter = true;
    }
    if (!r->tag) {
        r->tag = g_strdup(r->name ? r->name : "all");
    }
    return r;
}

static bool num_holds(const NumF *f, uint64_t x, bool is32)
{
    int64_t s = is32 ? (int64_t)(int32_t)x : (int64_t)x;
    uint64_t u = is32 ? (uint32_t)x : x;
    uint64_t vu = is32 ? (uint32_t)f->v : (uint64_t)f->v;

    switch (f->op) {
    case F_EQ: return u == vu;
    case F_NE: return u != vu;
    case F_LT: return s < f->v;
    case F_LE: return s <= f->v;
    case F_GT: return s > f->v;
    case F_GE: return s >= f->v;
    case F_LTU: return u < vu;
    case F_LEU: return u <= vu;
    case F_GTU: return u > vu;
    case F_GEU: return u >= vu;
    case F_ALL: return (u & vu) == vu;
    case F_NONE: return (u & vu) == 0;
    case F_RANGE: return s >= f->v && s <= f->v2;
    }
    return false;
}

static bool str_holds(const StrF *f, const char *s)
{
    switch (f->op) {
    case S_EXACT: return !strcmp(s, f->s);
    case S_PREFIX: return g_str_has_prefix(s, f->s);
    case S_SUFFIX: return g_str_has_suffix(s, f->s);
    case S_SUB: return strstr(s, f->s) != NULL;
    }
    return false;
}

/* --- per-call state ------------------------------------------------------ */

typedef struct { Rule *r; Sys *s; } RuleHook;

typedef struct {
    unsigned vcpu;
    uint64_t a[NARGS];
    bool have_a;
    uint64_t task;
    bool have_task;
    bool user;                  /* trap mode: at the user/kernel boundary */
    bool abi32;                 /* every argument is 32-bit */
    int abi;                    /* trap mode */
    int64_t num;
} Call;

static bool call_args(Call *c, const Sys *s)
{
    if (c->have_a) {
        return true;
    }
    if (P.block) {
        uint8_t b[512];
        uint64_t base = reg_read(P.args[0].reg, c->vcpu);
        if (P.args[0].mask) {
            base &= P.args[0].mask;
        }
        if (P.block_len > sizeof(b) || !mem_read(base + P.block_lo, b, P.block_len)) {
            return false;
        }
        for (int i = 0; i < NARGS; i++) {
            c->a[i] = load_n(b + P.args[i].off - P.block_lo, P.args[i].size, big_endian);
        }
    } else {
        int n = s->nargs >= 0 ? s->nargs : NARGS;
        for (int i = 0; i < n; i++) {
            if (!loc_read(&P.args[i], c->vcpu, &c->a[i])) {
                return false;
            }
        }
    }
    c->have_a = true;
    return true;
}

static uint64_t call_task(Call *c)
{
    if (!c->have_task) {
        if (!loc_read(c->user ? &P.ucur : &P.cur, c->vcpu, &c->task)) {
            c->task = 0;
        }
        c->have_task = true;
    }
    return c->task;
}

static bool task_comm(uint64_t task, char comm[17])
{
    memset(comm, 0, 17);
    return task && mem_read(task + P.comm, comm, 16);
}

static int64_t task_int(uint64_t task, int64_t off)
{
    uint64_t v;
    return task && mem_word(task + off, 4, &v) ? (int32_t)v : -1;
}

/* The rule's process filters. */
static bool proc_holds(Rule *r, Call *c)
{
    if (r->comm || r->pid >= 0 || r->tgid >= 0) {
        uint64_t t = call_task(c);
        char comm[17];
        if (r->comm && (!task_comm(t, comm) || strcmp(comm, r->comm))) {
            return false;
        }
        if (r->pid >= 0 && task_int(t, P.pid) != r->pid) {
            return false;
        }
        if (r->tgid >= 0 && task_int(t, P.tgid) != r->tgid) {
            return false;
        }
    }
    return true;
}

/*
 * The C side of a rule's filters, cheapest first.  Returns 1 if they hold,
 * 0 if not, -1 if a string could not be read yet (evaluate at return).
 */
static int rule_holds(Rule *r, Sys *s, Call *c)
{
    for (int i = 0; i < r->n_num; i++) {
        NumF *f = &r->num[i];
        if (f->inl) {
            continue;               /* tested in the translated code */
        }
        if (!call_args(c, s) ||
            !num_holds(f, c->a[f->arg], s->is32[f->arg] || c->abi32)) {
            return 0;
        }
    }
    if (!proc_holds(r, c)) {
        return 0;
    }
    for (int i = 0; i < r->n_str; i++) {
        char buf[256];
        if (!call_args(c, s)) {
            return 0;
        }
        if (!user_string(c->a[r->str[i].arg], buf, sizeof(buf))) {
            return -1;
        }
        if (!str_holds(&r->str[i], buf)) {
            return 0;
        }
    }
    return 1;
}

/* --- events -------------------------------------------------------------- */

static void ev_head(GString *g, const char *what, Rule *r, Sys *s, Call *c)
{
    uint64_t t = call_task(c);
    char comm[17];

    task_comm(t, comm);
    g_string_append_printf(g, "SC %s tag=%s name=%s nr=%d vcpu=%u pid=%" PRId64
                           " tgid=%" PRId64 " comm=%s", what, r->tag,
                           s ? s->name : "?",
                           c->user ? (int)c->num : s ? sys_nr(s) : -1, c->vcpu,
                           task_int(t, P.pid), task_int(t, P.tgid), comm);
    if (c->user) {
        g_string_append_printf(g, " abi=%s", abi_names[c->abi]);
    }
}

static void ev_enter(Rule *r, Sys *s, Call *c, bool late)
{
    if (r->count) {
        __atomic_fetch_add(&r->n_enter, 1, __ATOMIC_RELAXED);
        if (late) {
            __atomic_fetch_add(&r->n_late, 1, __ATOMIC_RELAXED);
        }
    }
    if (!r->log) {
        return;
    }
    g_autoptr(GString) g = g_string_new(NULL);
    ev_head(g, "enter", r, s, c);
    if (call_args(c, s)) {
        int n = s->nargs >= 0 ? s->nargs : NARGS;
        for (int i = 0; i < n; i++) {
            char buf[256];
            if (s->is_str[i] && user_string(c->a[i], buf, sizeof(buf))) {
                g_string_append_printf(g, " a%d=\"%s\"", i, buf);
            } else {
                g_string_append_printf(g, " a%d=%#" PRIx64, i, c->a[i]);
            }
        }
    }
    g_string_append_printf(g, " late=%d\n", late);
    qemu_plugin_outs(g->str);
}

static void ev_return(Rule *r, Sys *s, Call *c, int64_t ret, const char *what)
{
    if (r->count) {
        __atomic_fetch_add(!strcmp(what, "child") ? &r->n_child : &r->n_return,
                           1, __ATOMIC_RELAXED);
    }
    if (!r->log) {
        return;
    }
    g_autoptr(GString) g = g_string_new(NULL);
    ev_head(g, what, r, s, c);
    g_string_append_printf(g, " ret=%" PRId64 "\n", ret);
    qemu_plugin_outs(g->str);
}

/* --- returns: outstanding calls, keyed by the task's kernel stack pointer - */

/*
 * A fixed open-addressing table (no allocation per call): a task runs one
 * syscall at a time, so its entry stack pointer identifies the call until
 * it returns, on whichever vCPU.  Several rules may wait on one call.
 */
#define PSLOTS 1024             /* outstanding calls; a power of two */
#define PRULES 8                /* rules waiting on one call */

typedef struct {
    uint64_t sp;                /* 0: empty */
    uint8_t n;
    bool have_a;
    RuleHook *rh[PRULES];
    bool deferred[PRULES];      /* entry filters wait for a string */
    uint64_t a[NARGS];
} Slot;

static Slot slots[PSLOTS];
static GMutex pend_lock;
static uint32_t n_pending;      /* calls; read by the return hooks' condition */
static GHashTable *ret_sites;   /* return address -> hook */
static uint64_t n_stale, n_full;

static inline unsigned slot_home(uint64_t sp)
{
    return (sp >> 4 ^ sp >> 14) & (PSLOTS - 1);
}

/* the slot holding @sp, or the empty slot where it would go; NULL if full */
static Slot *slot_find(uint64_t sp)
{
    for (unsigned k = 0, i = slot_home(sp); k < PSLOTS; k++, i = (i + 1) & (PSLOTS - 1)) {
        if (slots[i].sp == sp || !slots[i].sp) {
            return &slots[i];
        }
    }
    return NULL;
}

/* remove the entry at @s, shifting later entries of the cluster back */
static void slot_delete(Slot *s)
{
    unsigned i = s - slots, j = i;

    slots[i].sp = 0;
    for (;;) {
        j = (j + 1) & (PSLOTS - 1);
        if (!slots[j].sp) {
            return;
        }
        unsigned h = slot_home(slots[j].sp);
        /* can slot j move to the hole at i?  only if its home is not in (i, j] */
        if ((j > i && (h <= i || h > j)) || (j < i && h <= i && h > j)) {
            slots[i] = slots[j];
            slots[j].sp = 0;
            i = j;
        }
    }
}

static void cb_return(unsigned int vcpu, uint64_t pc, void *ud);

static void ret_site(uint64_t ra)
{
    /* called with pend_lock held */
    if (g_hash_table_contains(ret_sites, &ra)) {
        return;
    }
    struct qemu_plugin_hook_cond c = {
        .kind = QEMU_PLUGIN_HOOK_OPERAND_HOST, .host = &n_pending, .size = 4,
        .cond = QEMU_PLUGIN_HOOK_NE, .value = 0,
    };
    struct qemu_plugin_hook *h =
        qemu_plugin_hook_insert_cond(self, ra, cb_return, NULL, &c, 1);
    g_hash_table_insert(ret_sites, g_memdup2(&ra, sizeof(ra)), h);
}

static void pend_add(uint64_t sp, RuleHook *rh, bool deferred, const Call *c,
                     uint64_t ra)
{
    g_mutex_lock(&pend_lock);
    Slot *s = slot_find(sp);
    if (!s) {
        n_full++;
        g_mutex_unlock(&pend_lock);
        return;
    }
    if (!s->sp) {
        s->sp = sp;
        s->n = 0;
        s->have_a = false;
        __atomic_fetch_add(&n_pending, 1, __ATOMIC_RELAXED);
    }
    /* the same rule again on one stack pointer: its earlier return was lost */
    int k;
    for (k = 0; k < s->n && s->rh[k]->r != rh->r; k++) {
    }
    if (k < s->n) {
        n_stale++;
    } else if (s->n < PRULES) {
        k = s->n++;
    } else {
        n_full++;
        g_mutex_unlock(&pend_lock);
        return;
    }
    s->rh[k] = rh;
    s->deferred[k] = deferred;
    if (c->have_a) {
        memcpy(s->a, c->a, sizeof(s->a));
        s->have_a = true;
    }
    ret_site(ra);
    g_mutex_unlock(&pend_lock);
}

static void cb_return(unsigned int vcpu, uint64_t pc, void *ud)
{
    uint64_t sp = reg_read(P.sp, vcpu) - P.spdelta;
    Slot s;

    g_mutex_lock(&pend_lock);
    Slot *p = slot_find(sp);
    if (!p || p->sp != sp) {
        g_mutex_unlock(&pend_lock);
        return;                     /* another task's syscall */
    }
    s = *p;
    slot_delete(p);
    __atomic_fetch_sub(&n_pending, 1, __ATOMIC_RELAXED);
    g_mutex_unlock(&pend_lock);

    int64_t ret = (int64_t)reg_read(P.ret, vcpu);
    if (ptr_size == 4) {
        ret = (int32_t)ret;
    }
    for (int i = 0; i < s.n; i++) {
        Rule *r = s.rh[i]->r;
        Sys *sy = s.rh[i]->s;
        Call c = { .vcpu = vcpu, .have_a = s.have_a };

        memcpy(c.a, s.a, sizeof(c.a));
        if (s.deferred[i]) {
            if (rule_holds(r, sy, &c) != 1) {
                continue;
            }
            ev_enter(r, sy, &c, true);
        }
        if (!r->ret || (r->ret_f && !num_holds(&r->retf, ret, ptr_size == 4))) {
            continue;
        }
        ev_return(r, sy, &c, ret, "return");
        if (r->setret) {
            reg_write(P.ret, vcpu, r->setret_v);
        }
    }
}

/* --- entry --------------------------------------------------------------- */

static void cb_enter(unsigned int vcpu, uint64_t pc, void *ud)
{
    RuleHook *rh = ud;
    Rule *r = rh->r;
    Sys *s = rh->s;
    Call c = { .vcpu = vcpu };
    int ok = rule_holds(r, s, &c);
    uint64_t ra;

    if (ok == 0) {
        return;
    }
    if (ok == 1) {
        if (r->enter) {
            ev_enter(r, s, &c, false);
        }
        for (int i = 0; i < NARGS; i++) {
            if (r->set[i]) {
                loc_write(&P.args[i], vcpu, r->set_v[i]);
                if (c.have_a) {
                    c.a[i] = r->set_v[i];
                }
            }
        }
    }
    if (!(r->ret || ok < 0) || s->noreturn) {
        return;
    }

    /* the return address: learned once per handler, the same thereafter */
    ra = __atomic_load_n(&s->ra, __ATOMIC_RELAXED);
    if (!ra) {
        if (!loc_read(&P.ra, vcpu, &ra) || !ra) {
            logf_("SC error: %s: no return address\n", s->name);
            return;
        }
        __atomic_store_n(&s->ra, ra, __ATOMIC_RELAXED);
        logf_("SC learn name=%s ra=%#" PRIx64 "\n", s->name, ra);
    }

    if (ok == 1 && r->override) {
        /* skip the handler: return to the caller with the value */
        ev_return(r, s, &c, r->override_v, "return");
        reg_write(P.ret, vcpu, r->override_v);
        if (P.spdelta) {
            reg_write(P.sp, vcpu, reg_read(P.sp, vcpu) + P.spdelta);
        }
        qemu_plugin_set_pc(ra);
    }

    pend_add(reg_read(P.sp, vcpu), rh, ok < 0, &c, ra);
}

/* override without a return rule still skips the handler */
static void cb_override(unsigned int vcpu, uint64_t pc, void *ud)
{
    RuleHook *rh = ud;
    Call c = { .vcpu = vcpu };
    uint64_t ra;

    if (rule_holds(rh->r, rh->s, &c) != 1) {
        return;
    }
    if (rh->r->enter) {
        ev_enter(rh->r, rh->s, &c, false);
    }
    if (!loc_read(&P.ra, vcpu, &ra) || !ra) {
        logf_("SC error: %s: no return address\n", rh->s->name);
        return;
    }
    reg_write(P.ret, vcpu, rh->r->override_v);
    if (P.spdelta) {
        reg_write(P.sp, vcpu, reg_read(P.sp, vcpu) + P.spdelta);
    }
    qemu_plugin_set_pc(ra);
}

/* --- fork children: the new task's first return, from ret_from_fork ------ */

static void cb_child(unsigned int vcpu, uint64_t pc, void *ud)
{
    Call c = { .vcpu = vcpu };
    uint64_t t = call_task(&c), mm;

    /* kernel threads have no mm */
    if (!t || !mem_word(t + P.mm, ptr_size, &mm) || !mm) {
        return;
    }
    for (guint i = 0; i < rules->len; i++) {
        Rule *r = g_ptr_array_index(rules, i);
        static const char *const forks[] = { "fork", "vfork", "clone", "clone3" };
        bool match = !r->name;
        for (size_t k = 0; k < G_N_ELEMENTS(forks) && !match; k++) {
            match = !strcmp(r->name, forks[k]);
        }
        if (!r->child || !match) {
            continue;
        }
        /* argument filters were for the parent's call; process filters apply */
        Call cc = c;
        cc.have_a = true;
        if (proc_holds(r, &cc)) {
            ev_return(r, sys_by_name(r->name ? r->name : "clone"), &cc, 0, "child");
        }
    }
}

/* --- bootstrap: insert the hooks once the kernel runs -------------------- */

static struct qemu_plugin_hook *h_boot;
static uint64_t n_hooks, n_inline;

/* --- trap mode: QEMU's syscall events ------------------------------------ */

static bool trap_mode;
static Sys **ttab[QEMU_PLUGIN_SYSCALL_ABI_COUNT];   /* number -> Sys */
static uint64_t n_restart, n_kept;

#define TNUMS 8192
#define TRULES 32               /* rules on one syscall: the cookie's masks */

static Sys *trap_sys(int abi, int64_t num)
{
    return abi >= 0 && abi < QEMU_PLUGIN_SYSCALL_ABI_COUNT && ttab[abi] &&
           num >= 0 && num < TNUMS ? ttab[abi][num] : NULL;
}

static void trap_call(Call *c, unsigned vcpu, const struct qemu_plugin_syscall *sc)
{
    memset(c, 0, sizeof(*c));
    c->vcpu = vcpu;
    c->user = true;
    c->abi = sc->abi;
    c->num = sc->num;
    c->abi32 = abi_is32(sc->abi);
    memcpy(c->a, sc->args, sizeof(c->a));
    c->have_a = true;
}

/*
 * The cookie, kept by QEMU from entry to return: bit i of the low half,
 * rule i's filters wait for a string; of the high half, rule i held and
 * wants the return.
 */
static void trap_boot(void);
static gsize trap_ready;

static void cb_trap_enter(qemu_plugin_id_t id, unsigned int vcpu,
                          struct qemu_plugin_syscall *sc, void *ud)
{
    Sys *s;
    uint64_t cookie = 0;
    Call c;

    /*
     * The first trap from user mode: the kernel has numbered its syscall
     * metadata by now (at start_kernel, syscall_nr is still -1).
     */
    if (g_once_init_enter(&trap_ready)) {
        trap_boot();
        g_once_init_leave(&trap_ready, 1);
    }
    s = trap_sys(sc->abi, sc->num);

    if (!s || !s->trules) {
        sc->want_return = false;
        return;
    }
    trap_call(&c, vcpu, sc);
    for (guint i = 0; i < s->trules->len; i++) {
        Rule *r = g_ptr_array_index(s->trules, i);
        int ok = rule_holds(r, s, &c);

        if (ok == 0) {
            continue;
        }
        if (ok < 0) {
            cookie |= 1ull << i;
            continue;
        }
        if (r->enter) {
            ev_enter(r, s, &c, false);
        }
        for (int k = 0; k < NARGS; k++) {
            if (r->set[k]) {
                sc->args[k] = c.a[k] = r->set_v[k];
            }
        }
        if (r->override && !sc->skip) {
            /* the kernel never sees the call; later rules still see it */
            ev_return(r, s, &c, r->override_v, "return");
            sc->skip = true;
            sc->ret = r->override_v;
        } else if (r->ret) {
            cookie |= 1ull << (32 + i);
        }
    }
    sc->want_return = cookie && !s->trap_noreturn && !sc->skip;
    sc->cookie = cookie;
}

static void cb_trap_return(qemu_plugin_id_t id, unsigned int vcpu,
                           struct qemu_plugin_syscall *sc, void *ud)
{
    Sys *s = trap_sys(sc->abi, sc->num);
    Call c;

    if (!s || !s->trules) {
        return;
    }
    if (sc->restart) {
        /* the task enters the call again; its entry is reported again */
        __atomic_fetch_add(&n_restart, 1, __ATOMIC_RELAXED);
        return;
    }
    if (s->forkish && sc->ret == 0) {
        /* a vfork child, on the parent's stack: the parent still waits */
        sc->keep = true;
        __atomic_fetch_add(&n_kept, 1, __ATOMIC_RELAXED);
        return;
    }
    trap_call(&c, vcpu, sc);
    for (guint i = 0; i < s->trules->len; i++) {
        Rule *r = g_ptr_array_index(s->trules, i);

        if (sc->cookie & (1ull << i)) {
            if (rule_holds(r, s, &c) != 1) {
                continue;
            }
            ev_enter(r, s, &c, true);
        } else if (!(sc->cookie & (1ull << (32 + i)))) {
            continue;
        }
        if (!r->ret || (r->ret_f && !num_holds(&r->retf, sc->ret, c.abi32))) {
            continue;
        }
        ev_return(r, s, &c, sc->ret, "return");
        if (r->setret) {
            sc->ret = r->setret_v;
        }
    }
}

static void trap_add(int abi, int64_t num, Sys *s)
{
    if (abi < 0 || num < 0 || num >= TNUMS) {
        return;
    }
    if (!ttab[abi]) {
        ttab[abi] = g_new0(Sys *, TNUMS);
    }
    ttab[abi][num] = s;
}

static void trap_rule(Rule *r, Sys *s)
{
    if (!s->trules) {
        s->trules = g_ptr_array_new();
    }
    if (s->trules->len >= TRULES) {
        logf_("SC error: more than %d rules on %s\n", TRULES, s->name);
        return;
    }
    g_ptr_array_add(s->trules, r);
    n_hooks++;
}

/* Number tables, rules per syscall, and what QEMU should report. */
static void trap_boot(void)
{
    for (guint i = 0; i < P.sys->len; i++) {
        Sys *s = g_ptr_array_index(P.sys, i);
        static const char *const nr[] = { "rt_sigreturn", "sigreturn" };
        static const char *const fk[] = { "fork", "vfork", "clone", "clone3" };

        s->trap_noreturn = s->noreturn;
        for (size_t k = 0; k < G_N_ELEMENTS(nr); k++) {
            s->trap_noreturn |= !strcmp(s->name, nr[k]);
        }
        for (size_t k = 0; k < G_N_ELEMENTS(fk); k++) {
            s->forkish |= !strcmp(s->name, fk[k]);
        }
        if (sys_nr(s) >= 0) {
            trap_add(P.abi, s->nr, s);
        }
    }
    for (guint i = 0; i < P.csys->len; i++) {
        g_auto(GStrv) w = g_strsplit(g_ptr_array_index(P.csys, i), " ", 3);
        Sys *s = sys_by_name(w[1]);
        if (s) {
            trap_add(abi_by_name(w[0]), g_ascii_strtoll(w[2], NULL, 0), s);
        }
    }
    for (guint i = 0; i < rules->len; i++) {
        Rule *r = g_ptr_array_index(rules, i);
        if (!(r->enter || r->ret || r->override)) {
            continue;
        }
        if (r->name) {
            Sys *s = sys_by_name(r->name);
            if (!s) {
                logf_("SC error: no syscall %s in the profile\n", r->name);
                continue;
            }
            trap_rule(r, s);
        } else {
            for (guint k = 0; k < P.sys->len; k++) {
                trap_rule(r, g_ptr_array_index(P.sys, k));
            }
        }
    }
    /* report a number only if a rule is on its syscall */
    for (int abi = 0; abi < QEMU_PLUGIN_SYSCALL_ABI_COUNT; abi++) {
        for (int64_t n = 0; ttab[abi] && n < TNUMS; n++) {
            Sys *s = ttab[abi][n];
            unsigned f = 0;
            if (!s || !s->trules) {
                continue;
            }
            for (guint i = 0; i < s->trules->len; i++) {
                Rule *r = g_ptr_array_index(s->trules, i);
                f |= QEMU_PLUGIN_SYSCALL_ENTER;
                if ((r->ret || r->n_str) && !s->trap_noreturn) {
                    f |= QEMU_PLUGIN_SYSCALL_RETURN;
                }
            }
            qemu_plugin_syscall_watch(self, abi, n, f);
            n_inline++;         /* numbers watched */
        }
        qemu_plugin_syscall_watch(self, abi, -1, 0);
    }
    logf_("SC trap numbers=%" PRIu64 " rules=%" PRIu64 "\n", n_inline, n_hooks);
}

/* Until the first trap, every syscall of every ABI the profile knows. */
static void trap_watch_all(void)
{
    qemu_plugin_syscall_watch(self, P.abi, -1, QEMU_PLUGIN_SYSCALL_ENTER);
    for (guint i = 0; i < P.csys->len; i++) {
        g_auto(GStrv) w = g_strsplit(g_ptr_array_index(P.csys, i), " ", 2);
        qemu_plugin_syscall_watch(self, abi_by_name(w[0]), -1,
                                  QEMU_PLUGIN_SYSCALL_ENTER);
    }
}


static void hook_rule(Rule *r, Sys *s)
{
    RuleHook *rh = g_new0(RuleHook, 1);
    struct qemu_plugin_hook_cond conds[16];
    size_t nc = 0;
    qemu_plugin_vcpu_hook_cb_t cb = r->override && !r->ret ? cb_override : cb_enter;

    rh->r = r;
    rh->s = s;
    /* numeric filters on register arguments go into the translated code */
    for (int i = 0; i < r->n_num && !P.wrapper; i++) {
        NumF *f = &r->num[i];
        Loc *l = &P.args[f->arg];
        static const enum qemu_plugin_hook_cmp map[] = {
            [F_EQ] = QEMU_PLUGIN_HOOK_EQ, [F_NE] = QEMU_PLUGIN_HOOK_NE,
            [F_LT] = QEMU_PLUGIN_HOOK_LT, [F_LE] = QEMU_PLUGIN_HOOK_LE,
            [F_GT] = QEMU_PLUGIN_HOOK_GT, [F_GE] = QEMU_PLUGIN_HOOK_GE,
            [F_LTU] = QEMU_PLUGIN_HOOK_LTU, [F_LEU] = QEMU_PLUGIN_HOOK_LEU,
            [F_GTU] = QEMU_PLUGIN_HOOK_GTU, [F_GEU] = QEMU_PLUGIN_HOOK_GEU,
        };
        unsigned size = s->is32[f->arg] ? 4 : ptr_size;
        struct qemu_plugin_hook_cond base = {
            .kind = QEMU_PLUGIN_HOOK_OPERAND_REG, .reg = regs[l->reg].h,
            .size = size,
        };

        if (l->mem || nc + 2 > G_N_ELEMENTS(conds)) {
            continue;
        }
        f->inl = true;
        if (f->op == F_RANGE) {
            conds[nc] = base;
            conds[nc].cond = QEMU_PLUGIN_HOOK_GE, conds[nc++].value = f->v;
            conds[nc] = base;
            conds[nc].cond = QEMU_PLUGIN_HOOK_LE, conds[nc++].value = f->v2;
        } else if (f->op == F_ALL || f->op == F_NONE) {
            conds[nc] = base;
            conds[nc].mask = f->v;
            conds[nc].cond = QEMU_PLUGIN_HOOK_EQ;
            conds[nc++].value = f->op == F_ALL ? f->v : 0;
        } else {
            conds[nc] = base;
            conds[nc].cond = map[f->op];
            conds[nc++].value = f->v;
        }
    }
    if (nc) {
        qemu_plugin_hook_insert_cond(self, s->handler, cb, rh, conds, nc);
        n_inline++;
    } else {
        qemu_plugin_hook_insert(self, s->handler, cb, rh);
    }
    n_hooks++;
}

static void cb_boot(unsigned int vcpu, uint64_t pc, void *ud)
{
    bool child = false;
    int64_t t0 = g_get_monotonic_time(), t_meta;

    qemu_plugin_hook_remove(h_boot);
    if (!regs_resolve()) {
        return;
    }
    for (int i = 0; i < n_regs; i++) {
        uint64_t v = reg_read(i, vcpu);
        g_autoptr(GByteArray) b = g_byte_array_new();
        qemu_plugin_read_register(regs[i].h, b);
        logf_("SC reg name=%s direct=%d size=%zu value=%#" PRIx64
              " gdbstub=%#" PRIx64 "\n",
              regs[i].name, vcpu < MAX_VCPUS && regs[i].p[vcpu] != NULL,
              vcpu < MAX_VCPUS ? regs[i].size[vcpu] : 0, v,
              load_n(b->data, b->len > 8 ? 8 : b->len, big_endian));
    }
    for (guint i = 0; i < P.sys->len; i++) {
        meta_read(g_ptr_array_index(P.sys, i));
    }
    t_meta = g_get_monotonic_time();
    for (guint i = 0; i < rules->len && trap_mode; i++) {
        child |= ((Rule *)g_ptr_array_index(rules, i))->child;
    }
    if (trap_mode) {
        trap_watch_all();
    }
    for (guint i = 0; i < rules->len && !trap_mode; i++) {
        Rule *r = g_ptr_array_index(rules, i);
        child |= r->child;
        if (r->name) {
            Sys *s = sys_by_name(r->name);
            if (!s) {
                logf_("SC error: no syscall %s in the profile\n", r->name);
                continue;
            }
            if (r->enter || r->ret || r->override) {
                hook_rule(r, s);
            }
        } else {
            /* filters with argument indices make no sense for every syscall */
            for (guint k = 0; k < P.sys->len && (r->enter || r->ret); k++) {
                hook_rule(r, g_ptr_array_index(P.sys, k));
            }
        }
    }
    if (child && P.fork_child) {
        qemu_plugin_hook_insert(self, P.fork_child, cb_child, NULL);
    }
    logf_("SC boot syscalls=%u hooks=%" PRIu64 " inline=%" PRIu64
          " wrapper=%d argblock=%d meta_us=%" PRId64 " insert_us=%" PRId64 "\n",
          P.sys->len, n_hooks, n_inline, P.wrapper, P.block, t_meta - t0,
          g_get_monotonic_time() - t_meta);
}

#if QEMU_PLUGIN_VERSION >= 7
static void at_exit(void *ud)
#else
static void at_exit(qemu_plugin_id_t id, void *ud)
#endif
{
    for (guint i = 0; i < rules->len; i++) {
        Rule *r = g_ptr_array_index(rules, i);
        if (r->count) {
            logf_("SC count tag=%s enter=%" PRIu64 " return=%" PRIu64
                  " child=%" PRIu64 " late=%" PRIu64 "\n", r->tag, r->n_enter,
                  r->n_return, r->n_child, r->n_late);
        }
    }
    if (trap_mode) {
        uint64_t pend, full, stale;
        qemu_plugin_syscall_stats(&pend, &full, &stale);
        logf_("SC exit pending=%" PRIu64 " stale=%" PRIu64 " full=%" PRIu64
              " mode=trap restart=%" PRIu64 " kept=%" PRIu64 "\n",
              pend, stale, full, n_restart, n_kept);
        return;
    }
    logf_("SC exit pending=%u stale=%" PRIu64 " full=%" PRIu64
          " return_sites=%u\n", n_pending, n_stale, n_full,
          g_hash_table_size(ret_sites));
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    const char *t = info->target_name, *profile = NULL, *rulefile = NULL;
    g_autofree char *text = NULL;

    self = id;
    big_endian = !strcmp(t, "mips") || !strcmp(t, "mips64") || !strcmp(t, "ppc64");
    for (int i = 0; i < argc; i++) {
        g_auto(GStrv) kv = g_strsplit(argv[i], "=", 2);
        const char *v = kv[1] ? kv[1] : "";
        if (!strcmp(kv[0], "profile")) {
            profile = g_strdup(v);
        } else if (!strcmp(kv[0], "rules")) {
            rulefile = g_strdup(v);
        } else if (!strcmp(kv[0], "endian")) {
            big_endian = !strcmp(v, "big");
        } else if (!strcmp(kv[0], "mode")) {
            trap_mode = !strcmp(v, "trap");
        } else {
            fprintf(stderr, "syscalls: unknown option %s\n", kv[0]);
            return -1;
        }
    }
    if (!profile || !rulefile || !profile_load(profile)) {
        fprintf(stderr, "syscalls: need profile= and rules=\n");
        return -1;
    }
    rules = g_ptr_array_new();
    if (!g_file_get_contents(rulefile, &text, NULL, NULL)) {
        fprintf(stderr, "syscalls: cannot read %s\n", rulefile);
        return -1;
    }
    g_auto(GStrv) lines = g_strsplit(text, "\n", -1);
    for (char **lp = lines; *lp; lp++) {
        Rule *r = rule_parse(*lp);
        if (r) {
            g_ptr_array_add(rules, r);
        }
    }
    ret_sites = g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free, NULL);
    h_boot = qemu_plugin_hook_insert(id, P.bootstrap, cb_boot, NULL);
    if (!h_boot) {
        fprintf(stderr, "syscalls: hooks unavailable\n");
        return -1;
    }
    if (trap_mode) {
        if (!P.have_ucur || P.abi < 0 ||
            !qemu_plugin_register_syscall_event_cb(id, cb_trap_enter,
                                                   cb_trap_return, NULL)) {
            fprintf(stderr, "syscalls: trap mode needs ucurrent and arch in "
                    "the profile, and QEMU syscall events\n");
            return -1;
        }
    }
    qemu_plugin_register_atexit_cb(id, at_exit, NULL);
    return 0;
}
