/*
 * The syscall trap core (system/syscall-trap.c), host-only: see tests/static/syscall_trap.py.
 *
 * "model" reads commands from stdin and prints what the trap did, for the Python model to
 * compare; "directed" runs the cases the model does not cover (holds, injections, detach, the
 * target's word, an abandoned dispatch). The trap core is included, so its statics are visible.
 * Every consumer's data is a token: dropping one twice, or the wrong consumer's, fails, and the
 * live count must match what the slots hold.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "trap_core.inc"
#include <setjmp.h>

struct rcu_head *rcu_deferred;
CPUState cpus[NCPUS] = { { .cpu_index = 0 }, { .cpu_index = 1 } };
CPUState *first_cpu = cpus;
bool env_writes = true, env_frozen = true, env_analysis = true, env_kvm;
int replay_mode;

#define MAXC 8

typedef struct Tok {
    int cons;
    int id;
} Tok;

typedef struct Cfg {
    int id;
    SyscallTrapConsumer *c;
    int want;           /* enter: 0 as watched, 1 on, 2 off */
    int data, task, skip, consume, keep, setret, skipret;
    int64_t skipv, retv;
    int fault_enter, fault_ret, inject_enter, inject_ret;   /* times left (directed) */
    bool longjmp_enter, remove_self;
} Cfg;

static Cfg cfg[MAXC];
static int tok_live, tok_next;
static unsigned drops[MAXC];
static jmp_buf escape;
static GString *log_;

static void say(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
static void say(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (log_) {
        g_string_append_vprintf(log_, fmt, ap);
    } else {
        vprintf(fmt, ap);
    }
    va_end(ap);
}

static void h_enter(CPUState *cpu, SyscallTrap *sc, void *o)
{
    Cfg *f = o;

    g_assert(!sc_lock.held);
    say("E %d%s%s\n", f->id, sc->resumed ? " r" : "", sc->injected ? " i" : "");
    if (sc->data) {
        g_assert(((Tok *)sc->data)->cons == f->id);
    }
    if (f->want == 1) {
        sc->want_return = true;
    } else if (f->want == 2) {
        sc->want_return = false;
    }
    if (f->data && !sc->data) {
        Tok *t = g_new(Tok, 1);
        t->cons = f->id;
        t->id = ++tok_next;
        tok_live++;
        sc->data = t;
    }
    if (f->task) {
        sc->task = f->task;
    }
    if (f->skip) {
        sc->skip = true;
        sc->ret = f->skipv;
    }
    if (f->remove_self) {
        f->remove_self = false;
        g_assert(syscall_trap_consumer_remove(f->c));
    }
    if (f->fault_enter && !sc->resumed) {
        f->fault_enter--;
        sc->fault = true;
        sc->fault_addr = 0x1234;
    } else if (f->inject_enter && !sc->resumed) {
        f->inject_enter--;
        sc->inject = true;
        sc->inject_num = 39;
        memset(sc->inject_args, 0, sizeof(sc->inject_args));
    }
    if (f->longjmp_enter) {
        f->longjmp_enter = false;
        longjmp(escape, 1);
    }
}

static void h_ret(CPUState *cpu, SyscallTrap *sc, void *o)
{
    Cfg *f = o;

    g_assert(!sc_lock.held);
    say("R %d%s%s%s%s\n", f->id, sc->skip ? " s" : "", sc->restart ? " x" : "",
        sc->resumed ? " r" : "", sc->injected ? " i" : "");
    if (sc->data) {
        g_assert(((Tok *)sc->data)->cons == f->id);
    }
    if (f->keep) {
        sc->keep = true;
    }
    if (f->setret) {
        sc->ret = f->retv;
    }
    if (f->fault_ret && !sc->resumed) {
        f->fault_ret--;
        sc->fault = true;
        return;
    }
    if (f->inject_ret && !sc->resumed) {
        f->inject_ret--;
        sc->inject = true;
        sc->inject_num = 39;
        return;
    }
    if (f->consume && sc->data) {
        g_free(sc->data);
        tok_live--;
        sc->data = NULL;
    }
}

static void h_drop(SyscallTrap *sc, void *o)
{
    Cfg *f = o;
    Tok *t = sc->data;

    g_assert(!sc_lock.held);
    g_assert(t && t->cons == f->id);
    say("D %d\n", f->id);
    drops[f->id]++;
    g_free(t);
    tok_live--;
}

static const SyscallTrapOps ops = { .enter = h_enter, .ret = h_ret, .drop = h_drop };
static const SyscallTrapOps ops_skip = {
    .enter = h_enter, .ret = h_ret, .drop = h_drop, .flags = SYSCALL_TRAP_OPS_SKIPPED_RETURN,
};

/* the tokens the slots hold: every other one has been dropped or consumed */
static int slot_tokens(void)
{
    int n = 0;

    for (unsigned i = 0; i < SC_SLOTS; i++) {
        for (unsigned k = 0; sc_slots[i].pc && k < sc_ncols; k++) {
            n += sc_row(i)[k].data != NULL;
        }
    }
    return n;
}

static void add(int id, int order, int prio, int skipret)
{
    Cfg *f = &cfg[id];

    *f = (Cfg) { .id = id, .skipret = skipret };
    f->c = syscall_trap_consumer_add(skipret ? &ops_skip : &ops, f, order, prio);
}

static int do_enter(uint64_t task, int abi, int64_t num, uint64_t pc, uint64_t *ret)
{
    uint64_t args[6] = { 1, 2, 3, 4, 5, 6 };

    *ret = 0;
    return syscall_trap_enter(&cpus[0], abi, num, args, pc, 2, task, 0, 0, 0, ret);
}

static int do_ret(uint64_t task, uint64_t pc, uint64_t *ret)
{
    SyscallTrapRewind rw;

    return syscall_trap_return(&cpus[0], pc, task, 0, 0, 0, ret, &rw);
}

static int model(void)
{
    char line[256];

    while (fgets(line, sizeof(line), stdin)) {
        char op[16];
        long long a[12] = { 0 };
        uint64_t ret = 0;
        int n = sscanf(line, "%15s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",
                       op, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8],
                       &a[9], &a[10], &a[11]);
        if (n < 1) {
            continue;
        }
        if (!strcmp(op, "add")) {
            add(a[0], a[1], a[2], a[3]);
            printf("= %d\n", cfg[a[0]].c != NULL);
        } else if (!strcmp(op, "cfg")) {
            Cfg *f = &cfg[a[0]];
            f->want = a[1];
            f->data = a[2];
            f->task = a[3];
            f->skip = a[4];
            f->skipv = a[5];
            f->consume = a[6];
            f->keep = a[7];
            f->setret = a[8];
            f->retv = a[9];
        } else if (!strcmp(op, "rm")) {
            printf("= %d\n", syscall_trap_consumer_remove(cfg[a[0]].c));
        } else if (!strcmp(op, "watch")) {
            syscall_trap_consumer_watch(cfg[a[0]].c, a[1], a[2], a[3]);
        } else if (!strcmp(op, "enter")) {
            int r = do_enter(a[0], a[1], a[2], a[3], &ret);
            printf("= %d %llu\n", r, (unsigned long long)ret);
        } else if (!strcmp(op, "ret")) {
            ret = a[2];
            int r = do_ret(a[0], a[1], &ret);
            printf("= %d %llu\n", r, (unsigned long long)ret);
        } else if (!strcmp(op, "forget")) {
            printf("= %u\n", syscall_trap_forget_task(a[0]));
        } else if (!strcmp(op, "cancel")) {
            printf("= %u\n", syscall_trap_cancel_all());
        } else if (!strcmp(op, "state")) {
            g_assert(tok_live == slot_tokens());
            printf("S %d %u %d %u\n", syscall_trap_on, syscall_trap_pending, tok_live,
                   syscall_trap_consumers());
        } else if (!strcmp(op, "flush")) {
            rcu_flush();
        }
        printf(".\n");
        fflush(stdout);
    }
    rcu_flush();
    return 0;
}

/* --- directed ------------------------------------------------------------------------------- */

static int fails;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n  log: %s\n", __func__, __LINE__, #cond, \
                log_ ? log_->str : ""); \
        fails++; \
    } \
} while (0)

static void begin(void)
{
    if (log_) {
        g_string_truncate(log_, 0);
    } else {
        log_ = g_string_new(NULL);
    }
}

static bool logged(const char *want)
{
    bool ok = !strcmp(log_->str, want);

    if (!ok) {
        fprintf(stderr, "  logged: %s  wanted: %s", log_->str, want);
    }
    g_string_truncate(log_, 0);
    return ok;
}

/* remove every consumer: the core is reset */
static void reset(void)
{
    for (int i = 0; i < MAXC; i++) {
        if (cfg[i].c && cfg[i].c->live) {
            syscall_trap_consumer_remove(cfg[i].c);
        }
        cfg[i].c = NULL;
    }
    rcu_flush();
    g_string_truncate(log_, 0);
    g_assert(!sc_list && !syscall_trap_on && !syscall_trap_pending && !sc_held);
    g_assert(tok_live == 0);
    memset(drops, 0, sizeof(drops));
}

static void three(void)
{
    add(1, SYSCALL_TRAP_OBSERVE_ORIGINAL, 0, 0);
    add(2, SYSCALL_TRAP_MUTATE, 0, 0);
    add(3, SYSCALL_TRAP_OBSERVE, 0, 0);
    for (int i = 1; i <= 3; i++) {
        syscall_trap_consumer_watch(cfg[i].c, 0, 7, SYSCALL_TRAP_ENTER | SYSCALL_TRAP_RETURN);
        cfg[i].data = 1;
        cfg[i].consume = 1;
    }
}

static void d_order_and_watch_or(void)
{
    uint64_t ret;

    begin();
    /* added out of order: dispatch follows class, then priority, then age */
    add(4, SYSCALL_TRAP_OBSERVE, 0, 0);
    add(3, SYSCALL_TRAP_MUTATE, 5, 0);
    add(2, SYSCALL_TRAP_MUTATE, -5, 0);
    add(1, SYSCALL_TRAP_OBSERVE_ORIGINAL, 9, 0);
    add(5, SYSCALL_TRAP_OBSERVE, 0, 0);
    CHECK(!syscall_trap_on);
    syscall_trap_consumer_watch(cfg[4].c, 0, 7, SYSCALL_TRAP_ENTER);
    syscall_trap_consumer_watch(cfg[2].c, 0, 7, SYSCALL_TRAP_RETURN);
    CHECK(sc_watch[0][7] == 3 && syscall_trap_on);
    for (int i = 1; i <= 5; i++) {
        syscall_trap_consumer_watch(cfg[i].c, 0, -1, SYSCALL_TRAP_ENTER);
    }
    do_enter(1, 0, 7, 0x100, &ret);
    CHECK(logged("E 1\nE 2\nE 3\nE 4\nE 5\n"));
    CHECK(syscall_trap_pending == 1);      /* 2 watched its return */
    ret = 0;
    do_ret(1, 0x100, &ret);
    CHECK(logged("R 2\n"));
    syscall_trap_consumer_remove(cfg[2].c);
    CHECK(sc_watch[0][7] == 1);
    syscall_trap_consumer_unwatch_all(cfg[4].c);
    CHECK(sc_watch[0][7] == 0 && sc_watch_all[0] == 1);
    for (int i = 1; i <= 5; i++) {
        syscall_trap_consumer_unwatch_all(cfg[i].c);
    }
    CHECK(!syscall_trap_on && syscall_trap_consumers() == 4);
    reset();
}

static void d_hold_enter(void)
{
    uint64_t ret;

    begin();
    three();
    cfg[2].fault_enter = 1;
    CHECK(do_enter(1, 0, 7, 0x100, &ret) == SYSCALL_TRAP_FAULT);
    CHECK(logged("E 1\nE 2\n"));             /* 3 sees it when it resumes */
    CHECK(sc_held == 1 && syscall_trap_pending == 1 && tok_live == 2);
    CHECK(sc_fault_addr == 0x1234);
    CHECK(do_enter(1, 0, 7, 0x100, &ret) == 0);
    CHECK(logged("E 2 r\nE 3\n"));           /* the holder again, then those after it */
    CHECK(sc_held == 0 && syscall_trap_pending == 1);
    ret = 5;
    CHECK(do_ret(1, 0x100, &ret) == 0);
    CHECK(logged("R 1\nR 2\nR 3\n"));
    CHECK(tok_live == 0 && !syscall_trap_pending);
    reset();
}

static void d_hold_return(void)
{
    uint64_t ret;

    begin();
    three();
    cfg[1].consume = 0;                     /* its data is dropped once it is done */
    cfg[2].fault_ret = 1;
    do_enter(1, 0, 7, 0x100, &ret);
    CHECK(logged("E 1\nE 2\nE 3\n"));
    ret = 5;
    CHECK(do_ret(1, 0x100, &ret) == SYSCALL_TRAP_FAULT);
    CHECK(logged("R 1\nR 2\nD 1\n"));     /* 1 is done with it: its data goes at the hold */
    CHECK(sc_held == 1 && drops[1] == 1);
    ret = 5;
    CHECK(do_ret(1, 0x100, &ret) == 0);
    CHECK(logged("R 2 r\nR 3\n"));
    CHECK(tok_live == 0 && !syscall_trap_pending && !sc_held);
    reset();
}

static void d_inject_enter(void)
{
    uint64_t ret, args[6] = { 1, 2, 3, 4, 5, 6 };
    SyscallTrapRewind rw;

    begin();
    three();
    cpus[0].syscall_inject_abis = 1;
    cfg[2].inject_enter = 1;
    ret = 0;
    CHECK(syscall_trap_enter(&cpus[0], 0, 7, args, 0x100, 2, 1, 0, 0, 0, &ret) ==
          SYSCALL_TRAP_INJECT);
    CHECK(ret == 39 && logged("E 1\nE 2\n"));
    /* removing the holder now is refused, without changes */
    SCSlot before[SC_SLOTS];
    memcpy(before, sc_slots, sizeof(before));
    CHECK(!syscall_trap_consumer_remove(cfg[2].c));
    CHECK(!memcmp(before, sc_slots, sizeof(before)) && cfg[2].c->live && tok_live == 2);
    /* the injected call returns: the task is sent back to the trap */
    ret = 42;
    CHECK(syscall_trap_return(&cpus[0], 0x100, 1, 0, 0, 0, &ret, &rw) == SYSCALL_TRAP_REWIND);
    CHECK(rw.num == 7 && rw.len == 2 && logged(""));
    CHECK(syscall_trap_enter(&cpus[0], 0, 7, args, 0x100, 2, 1, 0, 0, 0, &ret) == 0);
    CHECK(logged("E 2 r i\nE 3\n"));
    ret = 0;
    CHECK(do_ret(1, 0x100, &ret) == 0);
    CHECK(logged("R 1\nR 2\nR 3\n") && tok_live == 0);
    cpus[0].syscall_inject_abis = 0;
    reset();
}

static void d_inject_unsupported(void)
{
    uint64_t ret;

    begin();
    three();
    cfg[2].inject_enter = 1;                /* the target does not inject this ABI */
    CHECK(do_enter(1, 0, 7, 0x100, &ret) == 0);
    CHECK(logged("E 1\nE 2\nD 1\nD 2\n") && tok_live == 0 && !syscall_trap_pending);
    reset();
}

static void d_remove_holder(void)
{
    uint64_t ret;

    begin();
    three();
    cfg[2].fault_enter = 1;
    do_enter(1, 0, 7, 0x100, &ret);
    CHECK(logged("E 1\nE 2\n"));
    CHECK(syscall_trap_consumer_remove(cfg[2].c));
    CHECK(logged("D 2\n") && drops[2] == 1);
    /* resumes after the holder that went: as if it had returned without changes */
    do_enter(1, 0, 7, 0x100, &ret);
    CHECK(logged("E 3\n"));
    ret = 0;
    do_ret(1, 0x100, &ret);
    CHECK(logged("R 1\nR 3\n") && tok_live == 0);
    CHECK(drops[2] == 1);
    reset();
}

static void d_cancel_and_last(void)
{
    uint64_t ret;

    begin();
    three();
    cfg[2].fault_enter = 1;
    do_enter(1, 0, 7, 0x100, &ret);         /* held, 1 and 2 have data */
    do_enter(2, 0, 7, 0x100, &ret);         /* pending, all three */
    do_enter(3, 0, 7, 0x100, &ret);
    g_string_truncate(log_, 0);
    CHECK(syscall_trap_pending == 3 && sc_held == 1 && tok_live == 8);
    cpus[0].syscall_current = cpus[1].syscall_current = 42;
    cpus[0].syscall_inject_abis = 3;
    CHECK(syscall_trap_cancel_all() == 3);
    CHECK(tok_live == 0 && !syscall_trap_pending && !sc_held);
    CHECK(drops[1] == 3 && drops[2] == 3 && drops[3] == 2);
    CHECK(!cpus[0].syscall_current && !cpus[1].syscall_current && !cpus[0].syscall_inject_abis);
    CHECK(syscall_trap_on && sc_list && sc_list->n == 3);    /* watches and consumers stay */
    CHECK(syscall_trap_cancel_all() == 0);
    /* the last consumer going resets the core */
    do_enter(1, 0, 7, 0x100, &ret);
    CHECK(syscall_trap_consumer_remove(cfg[1].c) && syscall_trap_consumer_remove(cfg[2].c));
    CHECK(syscall_trap_pending == 1 && syscall_trap_on);
    cpus[1].syscall_current = 42;
    CHECK(syscall_trap_consumer_remove(cfg[3].c));
    CHECK(!syscall_trap_on && !syscall_trap_pending && !sc_list && !cpus[1].syscall_current);
    CHECK(tok_live == 0 && !syscall_trap_consumer_remove(cfg[3].c));
    reset();
}

static void d_forget(void)
{
    uint64_t ret;

    begin();
    three();
    cfg[2].task = 77;
    cfg[2].fault_enter = 1;
    do_enter(1, 0, 7, 0x100, &ret);         /* held, with 2's task */
    cfg[2].task = 0;
    do_enter(2, 0, 7, 0x100, &ret);
    g_string_truncate(log_, 0);
    CHECK(syscall_trap_forget_task(77) == 1 && tok_live == 3 && syscall_trap_pending == 1);
    CHECK(drops[1] == 1 && drops[2] == 1 && !sc_held);
    reset();
}

static void d_arch_word(void)
{
    uint64_t ret;

    begin();
    three();
    syscall_trap_set_arch(77);
    do_enter(1, 0, 7, 0x100, &ret);
    do_enter(2, 0, 7, 0x100, &ret);         /* the word is consumed: this one has 0 */
    ret = 0;
    do_ret(1, 0x100, &ret);
    CHECK(syscall_trap_arch() == 77);
    do_ret(2, 0x100, &ret);
    CHECK(syscall_trap_arch() == 0);
    syscall_trap_set_arch(5);
    do_ret(9, 0x100, &ret);                 /* no call */
    CHECK(syscall_trap_arch() == 0);
    /* a held call takes the word of the trap it resumes at */
    cfg[2].fault_enter = 1;
    syscall_trap_set_arch(8);
    do_enter(1, 0, 7, 0x100, &ret);
    syscall_trap_set_arch(9);
    do_enter(1, 0, 7, 0x100, &ret);
    do_ret(1, 0x100, &ret);
    CHECK(syscall_trap_arch() == 9);
    reset();
}

static void d_abandoned(void)
{
    uint64_t ret;

    begin();
    three();
    /* qemu_plugin_set_pc from an entry callback: what the consumers before it had is dropped by
     * the next dispatch on the thread (what the one that left set in that callback, the core
     * never saw) */
    cfg[3].longjmp_enter = true;
    cfg[3].data = 0;
    if (!setjmp(escape)) {
        do_enter(1, 0, 7, 0x100, &ret);
        CHECK(0);
    }
    CHECK(sc_busy && tok_live == 2 && logged("E 1\nE 2\nE 3\n"));
    do_enter(2, 0, 7, 0x104, &ret);
    CHECK(logged("D 1\nD 2\nE 1\nE 2\nE 3\n") && !sc_busy);
    CHECK(tok_live == 2);
    reset();
}

static void d_remove_self(void)
{
    uint64_t ret;

    begin();
    three();
    cfg[2].remove_self = true;              /* from its own entry callback */
    do_enter(1, 0, 7, 0x100, &ret);
    /* its in-flight data is dropped when the dispatch stores the call */
    CHECK(logged("E 1\nE 2\nE 3\nD 2\n"));
    CHECK(tok_live == 2 && drops[2] == 1);
    ret = 0;
    do_ret(1, 0x100, &ret);
    CHECK(logged("R 1\nR 3\n") && tok_live == 0);
    reset();
}

static void d_skipped_return(void)
{
    uint64_t ret;

    begin();
    add(1, SYSCALL_TRAP_MUTATE, 0, 0);
    add(2, SYSCALL_TRAP_OBSERVE, 0, 1);     /* asks for skipped returns */
    add(3, SYSCALL_TRAP_OBSERVE, 1, 0);     /* does not */
    for (int i = 1; i <= 3; i++) {
        syscall_trap_consumer_watch(cfg[i].c, 1, 7, SYSCALL_TRAP_ENTER | SYSCALL_TRAP_RETURN);
        cfg[i].data = 1;
    }
    cfg[1].skip = 1;
    cfg[1].skipv = -5;
    cfg[2].setret = 1;
    cfg[2].retv = -9;
    CHECK(do_enter(1, 1, 7, 0x100, &ret) == SYSCALL_TRAP_SKIP);
    CHECK(ret == (uint32_t)-9);             /* i386: 32 bits */
    CHECK(logged("E 1\nE 2\nE 3\nR 2 s\nD 1\nD 2\nD 3\n") && tok_live == 0);
    reset();
}

static int directed(void)
{
    d_order_and_watch_or();
    d_hold_enter();
    d_hold_return();
    d_inject_enter();
    d_inject_unsupported();
    d_remove_holder();
    d_cancel_and_last();
    d_forget();
    d_arch_word();
    d_abandoned();
    d_remove_self();
    d_skipped_return();
    rcu_flush();
    printf("trap directed: %d failures\n", fails);
    return fails != 0;
}

int main(int argc, char **argv)
{
    return argc > 1 && !strcmp(argv[1], "directed") ? directed() : model();
}
