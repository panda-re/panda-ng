/*
 * The PANDA syscall events (panda/src/syscall-events.c) with the registry (events.c) and the trap
 * core, host-only: see tests/static/syscall_trap.py. The trap core is included here, so the test
 * sees its consumers and watches; events.c and syscall-events.c are compiled as they are.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "trap_core.inc"
#include "system/replay.h"
#include "panda/plugin.h"
#include "panda/events-internal.h"
#include "panda/syscall-events.h"

struct rcu_head *rcu_deferred;
CPUState cpus[NCPUS] = { { .cpu_index = 0 }, { .cpu_index = 1 } };
CPUState *first_cpu = cpus;
bool env_writes = true, env_frozen = true, env_analysis = true, env_kvm;
int replay_mode;

/* what events.c needs from callbacks.c */
const PandaCoreEventDef panda_core_events[] = { { 0, "alpha", "cpu:ptr", false } };
const size_t panda_core_events_count = 1;
uint32_t panda_core_event_modes(int type) { return 0; }
const char *panda_event_owner_name(void *owner) { return owner ? "plugin" : NULL; }
void panda_core_event_describe_subs(int type, GString *out, bool json) { }
bool panda_events_rcu_ready(void) { return true; }

static int fails;
static GString *log_;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n  log: %s\n", __func__, __LINE__, #cond, log_->str); \
        fails++; \
    } \
} while (0)

static bool logged(const char *want)
{
    bool ok = !strcmp(log_->str, want);

    if (!ok) {
        fprintf(stderr, "  want: %s", want);
    }
    g_string_truncate(log_, 0);
    return ok;
}

/* a subscriber: what it does is in its context */
typedef struct Sub {
    const char *name;
    panda_sub sub;
    int set_arg, skip, fault, inject, set_ret, want_return;   /* times left */
    int64_t value;
    bool mutate_observer;       /* try a mutation from an observer */
    bool unsubscribe_self;
    bool ret_only;
    uint32_t modes;             /* 0: PANDA_MODE_ANALYSIS */
    const char *refusal;
    uint64_t last_call;
} Sub;

static void fn(void *ctx, void *args, void *data)
{
    Sub *s = ctx;
    PandaSyscall *sc = args;
    uint64_t *d = data;
    bool at_ret = sc->flags & PANDA_SC_AT_RETURN;

    g_string_append_printf(log_, "%s%s%s%s%s a0=%" PRIu64 "%s", s->name, at_ret ? ".ret" : "",
                           sc->flags & PANDA_SC_RESUMED ? " resumed" : "",
                           sc->flags & PANDA_SC_SKIPPED ? " skipped" : "",
                           sc->flags & PANDA_SC_RESTART ? " restart" : "", sc->args[0],
                           at_ret ? "" : "");
    if (at_ret) {
        g_string_append_printf(log_, " ret=%" PRId64, sc->ret);
    }
    g_string_append_c(log_, '\n');
    g_assert(sc->size == sizeof(PandaSyscall));
    if (d) {
        if (!at_ret && !(sc->flags & PANDA_SC_RESUMED)) {
            CHECK(*d == 0);             /* zeroed at the call's first emit */
            *d = sc->call_id;
        } else if (s->ret_only) {
            CHECK(*d == 0);             /* a return subscription's first emit is the return */
        } else {
            CHECK(*d == sc->call_id);   /* the same bytes, later */
            if (*d != sc->call_id) {
                fprintf(stderr, "  %s flags %#x: data %" PRIu64 ", call %" PRIu64 "\n", s->name,
                        sc->flags, *d, sc->call_id);
            }
        }
    }
    s->last_call = sc->call_id;
    if (s->set_arg && !at_ret) {
        s->set_arg--;
        g_assert(panda_syscall_set_arg(sc, 0, s->value));
    }
    if (s->skip && !at_ret) {
        s->skip--;
        g_assert(panda_syscall_skip(sc, s->value));
    }
    if (s->set_ret && at_ret) {
        s->set_ret--;
        g_assert(panda_syscall_set_ret(sc, s->value));
    }
    if (s->fault && !(sc->flags & PANDA_SC_RESUMED)) {
        s->fault--;
        g_assert(panda_syscall_fault(sc, 0x5000, false));
    }
    if (s->want_return && !at_ret) {
        s->want_return--;
        if (!panda_syscall_want_return(sc)) {
            s->refusal = panda_syscall_refusal();
        }
    }
    if (s->mutate_observer) {
        s->mutate_observer = false;
        g_assert(!panda_syscall_skip(sc, 1));
        s->refusal = panda_syscall_refusal();
    }
    if (s->unsubscribe_self) {
        s->unsubscribe_self = false;
        g_assert(panda_unsubscribe(s->sub));
        s->sub = NULL;
    }
}

static panda_sub subscribe(Sub *s, unsigned which, uint32_t kind, int prio, size_t data)
{
    s->ret_only = which == PANDA_SC_RETURN;
    s->sub = panda_subscribe((void *)1, panda_syscall_event(which), fn, s,
                             s->modes ? s->modes : PANDA_MODE_ANALYSIS, kind);
    g_assert(s->sub);
    if (prio) {
        g_assert(panda_sub_set_priority(s->sub, prio));
    }
    if (data) {
        g_assert(panda_sub_set_data_size(s->sub, data));
    }
    return s->sub;
}

static int enter(int64_t num, uint64_t a0, uint64_t *ret, uint64_t args_out[6])
{
    uint64_t args[6] = { a0, 2, 3, 4, 5, 6 };
    int r;

    *ret = 0;
    r = syscall_trap_enter(&cpus[0], 0, num, args, 0x400, 2, 1, 0, 0, 0, ret);
    if (args_out) {
        memcpy(args_out, args, sizeof(args));
    }
    return r;
}

static int ret_(uint64_t *ret)
{
    SyscallTrapRewind rw;

    return syscall_trap_return(&cpus[0], 0x400, 1, 0, 0, 0, ret, &rw);
}

static void unsubscribe(Sub *s)
{
    if (s->sub) {
        g_assert(panda_unsubscribe(s->sub));
        s->sub = NULL;
    }
}

static void settle(void)
{
    rcu_flush();
    g_string_truncate(log_, 0);
}

static void t_zero_cost(void)
{
    Sub a = { "A" };

    CHECK(syscall_trap_consumers() == 0 && !syscall_trap_on);
    subscribe(&a, PANDA_SC_ENTER, PANDA_SUB_OBSERVE, 0, 0);
    /* a subscription with no watch: a consumer, but the trap stays off */
    CHECK(syscall_trap_consumers() == 1 && !syscall_trap_on);
    CHECK(panda_syscall_watch(a.sub, 0, 39, PANDA_SC_ENTER));
    CHECK(syscall_trap_on && sc_watch[0][39] == SYSCALL_TRAP_ENTER);
    CHECK(panda_syscall_unwatch(a.sub, 0, 39, PANDA_SC_ENTER));
    CHECK(!syscall_trap_on);
    unsubscribe(&a);
    CHECK(syscall_trap_consumers() == 0 && !syscall_trap_on);
    settle();
}

static void t_instance_data_and_filters(void)
{
    Sub a = { "A" }, b = { "B" };
    uint64_t ret;

    subscribe(&a, PANDA_SC_ENTER, PANDA_SUB_OBSERVE, 0, sizeof(uint64_t));
    subscribe(&b, PANDA_SC_RETURN, PANDA_SUB_OBSERVE, 0, sizeof(uint64_t));
    CHECK(panda_syscall_watch(a.sub, 0, 39, PANDA_SC_ENTER | PANDA_SC_RETURN));
    CHECK(!panda_syscall_watch(b.sub, 0, 39, PANDA_SC_ENTER));
    CHECK(strstr(panda_syscall_refusal(), "returns only"));
    CHECK(panda_syscall_watch(b.sub, 0, 40, PANDA_SC_RETURN));
    CHECK(!panda_syscall_watch(a.sub, 99, 1, PANDA_SC_ENTER));
    CHECK(!panda_syscall_watch(a.sub, 0, 1, 0));
    CHECK(enter(38, 9, &ret, NULL) == 0 && logged(""));          /* not watched */
    enter(39, 9, &ret, NULL);
    CHECK(logged("A a0=9\n") && syscall_trap_pending == 1);
    ret = 7;
    ret_(&ret);
    CHECK(logged("A.ret a0=9 ret=7\n") && !syscall_trap_pending);
    enter(40, 9, &ret, NULL);
    CHECK(logged("") && syscall_trap_pending == 1);
    ret = (uint64_t)-2;
    ret_(&ret);
    CHECK(logged("B.ret a0=9 ret=-2\n"));
    unsubscribe(&a);
    unsubscribe(&b);
    settle();
}

static void t_order_and_mutation(void)
{
    Sub orig = { "O" }, m1 = { "M1", .set_arg = 1, .value = 99 }, m2 = { "M2" }, obs = { "S" };
    uint64_t ret, args[6];

    /* subscribed out of order */
    subscribe(&obs, PANDA_SC_ENTER, PANDA_SUB_OBSERVE, 0, 0);
    subscribe(&m2, PANDA_SC_ENTER, PANDA_SUB_MUTATE, 5, 0);
    subscribe(&m1, PANDA_SC_ENTER, PANDA_SUB_MUTATE, 1, 0);
    subscribe(&orig, PANDA_SC_ENTER, PANDA_SUB_OBSERVE_ORIGINAL, 0, 0);
    CHECK(syscall_trap_consumers() == 3);   /* one per kind */
    for (Sub *s = &orig; s; s = s == &orig ? &m1 : s == &m1 ? &m2 : s == &m2 ? &obs : NULL) {
        CHECK(panda_syscall_watch(s->sub, 0, -1, PANDA_SC_ENTER));
    }
    obs.mutate_observer = true;
    CHECK(enter(39, 1, &ret, args) == SYSCALL_TRAP_ARGS && args[0] == 99);
    CHECK(logged("O a0=1\nM1 a0=1\nM2 a0=99\nS a0=99\n"));
    CHECK(obs.refusal && strstr(obs.refusal, "observer"));
    /* a mutation from outside a callback */
    PandaSyscall fake = { .size = sizeof(fake) };
    CHECK(!panda_syscall_skip(&fake, 0) && strstr(panda_syscall_refusal(), "not called"));
    unsubscribe(&orig);
    unsubscribe(&m1);
    unsubscribe(&m2);
    unsubscribe(&obs);
    CHECK(syscall_trap_consumers() == 0);
    settle();
}

static void t_hold_and_release(void)
{
    Sub orig = { "O" }, m1 = { "M1", .fault = 1 }, m2 = { "M2" }, obs = { "S" };
    uint64_t ret;

    subscribe(&orig, PANDA_SC_ENTER, PANDA_SUB_OBSERVE_ORIGINAL, 0, sizeof(uint64_t));
    subscribe(&m1, PANDA_SC_ENTER, PANDA_SUB_MUTATE, 1, sizeof(uint64_t));
    subscribe(&m2, PANDA_SC_ENTER, PANDA_SUB_MUTATE, 2, sizeof(uint64_t));
    subscribe(&obs, PANDA_SC_ENTER, PANDA_SUB_OBSERVE, 0, sizeof(uint64_t));
    for (Sub *s = &orig; s; s = s == &orig ? &m1 : s == &m1 ? &m2 : s == &m2 ? &obs : NULL) {
        CHECK(panda_syscall_watch(s->sub, 0, 39, PANDA_SC_ENTER | PANDA_SC_RETURN));
    }
    CHECK(enter(39, 1, &ret, NULL) == SYSCALL_TRAP_FAULT);
    CHECK(logged("O a0=1\nM1 a0=1\n"));
    CHECK(enter(39, 1, &ret, NULL) == 0);
    CHECK(logged("M1 resumed a0=1\nM2 a0=1\nS a0=1\n"));
    ret = 3;
    ret_(&ret);
    CHECK(logged("O.ret a0=1 ret=3\nM1.ret a0=1 ret=3\nM2.ret a0=1 ret=3\nS.ret a0=1 ret=3\n"));

    /* the holder unsubscribes: released, as if it had returned without changes */
    m1.fault = 1;
    CHECK(enter(39, 1, &ret, NULL) == SYSCALL_TRAP_FAULT);
    CHECK(logged("O a0=1\nM1 a0=1\n"));
    unsubscribe(&m1);
    CHECK(enter(39, 1, &ret, NULL) == 0);
    CHECK(logged("M2 a0=1\nS a0=1\n"));
    ret = 3;
    ret_(&ret);
    CHECK(logged("O.ret a0=1 ret=3\nM2.ret a0=1 ret=3\nS.ret a0=1 ret=3\n"));

    /* the last mutator goes while it holds a call: the trap consumer goes, the call resumes */
    m2.fault = 1;
    CHECK(enter(39, 1, &ret, NULL) == SYSCALL_TRAP_FAULT);
    CHECK(logged("O a0=1\nM2 a0=1\n"));
    unsubscribe(&m2);
    CHECK(syscall_trap_consumers() == 2);
    CHECK(enter(39, 1, &ret, NULL) == 0);
    CHECK(logged("S a0=1\n"));
    ret = 3;
    ret_(&ret);
    CHECK(logged("O.ret a0=1 ret=3\nS.ret a0=1 ret=3\n"));
    unsubscribe(&orig);
    unsubscribe(&obs);
    CHECK(!syscall_trap_pending && !syscall_trap_consumers());
    settle();
}

static void t_pending_unsubscribe(void)
{
    Sub a = { "A" }, b = { "B" };
    uint64_t ret;

    subscribe(&a, PANDA_SC_ENTER, PANDA_SUB_OBSERVE, 0, 64);
    subscribe(&b, PANDA_SC_ENTER, PANDA_SUB_OBSERVE, 0, 0);
    panda_syscall_watch(a.sub, 0, 39, PANDA_SC_ENTER | PANDA_SC_RETURN);
    panda_syscall_watch(b.sub, 0, 39, PANDA_SC_ENTER | PANDA_SC_RETURN);
    enter(39, 1, &ret, NULL);
    unsubscribe(&a);                /* its return is not delivered; its data is freed */
    rcu_flush();
    ret = 0;
    ret_(&ret);
    CHECK(logged("A a0=1\nB a0=1\nB.ret a0=1 ret=0\n"));
    /* data size changing while a call is pending: the call keeps its layout */
    panda_sub_set_data_size(b.sub, 8);
    enter(39, 1, &ret, NULL);
    panda_sub_set_data_size(b.sub, 4096);
    ret_(&ret);
    CHECK(logged("B a0=1\nB.ret a0=1 ret=0\n"));
    unsubscribe(&b);
    settle();
}

static void t_skip_and_set_ret(void)
{
    Sub m = { "M", .skip = 1, .value = 4242 }, s = { "S" }, r = { "R", .set_ret = 1, .value = -1 };
    uint64_t ret;

    subscribe(&m, PANDA_SC_ENTER, PANDA_SUB_MUTATE, 0, 0);
    subscribe(&s, PANDA_SC_ENTER, PANDA_SUB_OBSERVE, 0, 8);
    panda_syscall_watch(m.sub, 0, 110, PANDA_SC_ENTER);
    panda_syscall_watch(s.sub, 0, 110, PANDA_SC_ENTER | PANDA_SC_RETURN);
    CHECK(enter(110, 1, &ret, NULL) == SYSCALL_TRAP_SKIP && ret == 4242);
    CHECK(logged("M a0=1\nS skipped a0=1\nS.ret skipped a0=1 ret=4242\n"));
    CHECK(!syscall_trap_pending);
    unsubscribe(&m);
    /* set_ret at a return; refused at an entry that is not skipped */
    subscribe(&r, PANDA_SC_ENTER, PANDA_SUB_MUTATE, 0, 0);
    panda_syscall_watch(r.sub, 0, 110, PANDA_SC_ENTER | PANDA_SC_RETURN);
    enter(110, 1, &ret, NULL);
    ret = 5;
    CHECK(ret_(&ret) == SYSCALL_TRAP_RET && ret == (uint64_t)-1);
    CHECK(logged("R a0=1\nS a0=1\nR.ret a0=1 ret=5\nS.ret a0=1 ret=-1\n"));
    unsubscribe(&r);
    unsubscribe(&s);
    settle();
}

static void t_exit_calls(void)
{
    Sub a = { "A", .want_return = 1 }, b = { "B" };
    uint64_t ret;

    subscribe(&a, PANDA_SC_ENTER, PANDA_SUB_OBSERVE, 0, 0);
    panda_syscall_watch(a.sub, 0, 60, PANDA_SC_ENTER | PANDA_SC_RETURN);
    enter(60, 1, &ret, NULL);               /* exit: no return armed */
    CHECK(logged("A a0=1\n") && !syscall_trap_pending);
    CHECK(a.refusal && strstr(a.refusal, "NORETURN_TOO"));
    subscribe(&b, PANDA_SC_ENTER, PANDA_SUB_OBSERVE, 0, 0);
    panda_syscall_watch(b.sub, 0, 231, PANDA_SC_ENTER | PANDA_SC_RETURN | PANDA_SC_NORETURN_TOO);
    enter(231, 1, &ret, NULL);
    CHECK(logged("B a0=1\n") && syscall_trap_pending == 1);
    ret = 0;
    ret_(&ret);
    CHECK(logged("B.ret a0=1 ret=0\n"));
    unsubscribe(&a);
    unsubscribe(&b);
    settle();
}

static void t_rr_and_kvm(void)
{
    Sub m = { "M", .modes = PANDA_MODE_ANALYSIS | PANDA_MODE_RR },
        o = { "O", .modes = PANDA_MODE_ANALYSIS | PANDA_MODE_RR };
    const char *why;
    uint64_t ret;

    subscribe(&m, PANDA_SC_ENTER, PANDA_SUB_MUTATE, 0, 0);
    subscribe(&o, PANDA_SC_ENTER, PANDA_SUB_OBSERVE, 0, 0);
    panda_syscall_watch(m.sub, 0, 39, PANDA_SC_ENTER);
    panda_syscall_watch(o.sub, 0, 39, PANDA_SC_ENTER);
    /* record starts while the mutator is active: its calls are refused, not skipped */
    replay_mode = REPLAY_MODE_RECORD;
    m.skip = 1;
    m.value = 1;
    m.mutate_observer = true;           /* (a mutator here: the refusal is rr's) */
    m.skip = 0;
    enter(39, 1, &ret, NULL);
    CHECK(m.refusal && strstr(m.refusal, "record/replay"));
    g_string_truncate(log_, 0);
    /* re-evaluated for the mode: the mutator's subscription is refused, the observer stays */
    panda_events_reevaluate();
    CHECK(panda_sub_status(m.sub, &why) == PANDA_SUB_REFUSED && strstr(why, "not logged"));
    CHECK(panda_sub_status(o.sub, NULL) == PANDA_SUB_ACTIVE);
    CHECK(syscall_trap_consumers() == 1);
    enter(39, 1, &ret, NULL);
    CHECK(logged("O a0=1\n"));          /* the observer still runs; the mutator does not */
    replay_mode = REPLAY_MODE_NONE;
    panda_events_reevaluate();
    CHECK(panda_sub_status(m.sub, NULL) == PANDA_SUB_ACTIVE && syscall_trap_consumers() == 2);
    CHECK(sc_watch[0][39]);             /* the watches came back with it */
    env_kvm = true;
    panda_events_reevaluate();
    CHECK(panda_sub_status(o.sub, &why) == PANDA_SUB_REFUSED && strstr(why, "KVM"));
    env_kvm = false;
    panda_events_reevaluate();
    unsubscribe(&m);
    unsubscribe(&o);
    settle();
}

/* stand-ins for the QEMU-plugin bridge and Penguin, beside the events */
static void other_enter(CPUState *cpu, SyscallTrap *sc, void *o)
{
    g_string_append_printf(log_, "%s\n", (const char *)o);
}

static const SyscallTrapOps other_ops = { .enter = other_enter };

static void t_beside_other_consumers(void)
{
    Sub orig = { "O" }, m = { "M" }, obs = { "S" };
    SyscallTrapConsumer *bridge, *penguin;
    uint64_t ret;

    bridge = syscall_trap_consumer_add(&other_ops, "bridge", SYSCALL_TRAP_OBSERVE, INT_MAX);
    penguin = syscall_trap_consumer_add(&other_ops, "penguin", SYSCALL_TRAP_MUTATE, 0);
    syscall_trap_consumer_watch(bridge, 0, 39, SYSCALL_TRAP_ENTER);
    syscall_trap_consumer_watch(penguin, 0, -1, SYSCALL_TRAP_ENTER);
    subscribe(&obs, PANDA_SC_ENTER, PANDA_SUB_OBSERVE, 0, 0);
    subscribe(&m, PANDA_SC_ENTER, PANDA_SUB_MUTATE, 0, 0);
    subscribe(&orig, PANDA_SC_ENTER, PANDA_SUB_OBSERVE_ORIGINAL, 0, 0);
    panda_syscall_watch(orig.sub, 0, 39, PANDA_SC_ENTER);
    panda_syscall_watch(m.sub, 0, 39, PANDA_SC_ENTER);
    panda_syscall_watch(obs.sub, 0, 39, PANDA_SC_ENTER);
    enter(39, 1, &ret, NULL);
    CHECK(logged("O a0=1\npenguin\nM a0=1\nS a0=1\nbridge\n"));
    unsubscribe(&orig);
    unsubscribe(&m);
    unsubscribe(&obs);
    CHECK(syscall_trap_consumers() == 2);
    enter(39, 1, &ret, NULL);
    CHECK(logged("penguin\nbridge\n"));
    CHECK(syscall_trap_consumer_remove(penguin) && syscall_trap_consumer_remove(bridge));
    CHECK(!syscall_trap_on);
    settle();
}

int main(void)
{
    log_ = g_string_new(NULL);
    t_zero_cost();
    t_instance_data_and_filters();
    t_order_and_mutation();
    t_hold_and_release();
    t_pending_unsubscribe();
    t_skip_and_set_ret();
    t_exit_calls();
    t_rr_and_kvm();
    t_beside_other_consumers();
    rcu_flush();
    printf("syscall events directed: %d failures\n", fails);
    return fails != 0;
}
