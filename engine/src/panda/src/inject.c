/*
 * System-call injection sessions (include/panda/inject.h) for this target.
 *
 * Sessions wait in a queue until a vCPU reaches a user-mode instruction boundary of their target
 * (the CPU loop calls panda_inject_service() there while anything is queued or held:
 * PANDA_LOOP_INJECT). The thread's registers are saved, a token goes in its marker register, and
 * it enters the kernel as if it had executed a system call instruction just before the return
 * address (the current PC; for a held entry, past its own trap instruction). Back in user mode
 * at that address with the token, the call's value goes to next(), which chooses the next call
 * or finishes; then the registers go back and the thread runs on.
 *
 * Only system calls the guest could have made itself; never a fault the guest would not take
 * (a session's next() may ask for one: PandaInjectStep.touch, a user access of the thread's own,
 * after its registers are back).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/timer.h"
#include "qemu/rcu.h"
#include "qapi/error.h"
#include "hw/core/cpu.h"
#include "exec/cpu-common.h"
#include "accel/tcg/cpu-ops.h"
#include "accel/tcg/cpu-mmu-index.h"
#include "accel/tcg/cpu-loop.h"
#include "accel/tcg/probe.h"
#include "system/tcg.h"
#include "system/replay.h"
#include "system/replay-analysis.h"
#include "panda/common.h"
#include "panda/hooks.h"
#include "panda/inject.h"
#include "panda/syscall-pair.h"
#include "plugins/qemu-plugin.h"
#include "inject-int.h"
#if defined(TARGET_I386)
#include "target/i386/tcg/helper-tcg.h"
#endif
#if defined(TARGET_ARM)
#include "target/arm/internals.h"
#include "target/arm/syndrome.h"
#endif
#if defined(TARGET_MIPS)
#include "target/mips/internal.h"
#endif
#if defined(TARGET_PPC)
#include "target/ppc/internal.h"
#endif

__thread bool panda_inject_raising;
__thread uint8_t panda_sc_hold;
__thread PandaSyscallCall *panda_sc_hold_call;
uint32_t panda_inject_resumed_pending;

#include "inject-arch.h"

#define INJECT_TIMEOUT_NS (5 * NANOSECONDS_PER_SECOND)
#define INJECT_RESUMED_MAX 64
/* How long a passed recording check keeps new injections from starting (host time) */
#define INJECT_FREEZE_NS (2 * NANOSECONDS_PER_SECOND)

typedef struct Session Session;

/* A thread inside an injected call, or between two: its registers, to put back */
typedef struct Held {
    QTAILQ_ENTRY(Held) next;
    uint64_t asid, sp;
    uint64_t pc;                /* where the thread resumes */
    uint64_t ret_pc;            /* where the injected call returns */
    uint64_t token;             /* in the marker register while the call is out (0: none) */
    unsigned len;               /* the call instruction's length: restart = ret_pc - len */
    int abi;
    bool hold;                  /* a held entry: the thread executes its trap again */
    bool rewind;                /* and with rw's number and arguments put back (a skipped call) */
    PandaInjectRewind rw;
    uint32_t calls;             /* calls of this round that returned */
    bool finishing;             /* the session is over: only the stack words wait to go back */
    int final_r;                /* then: next()'s last answer */
    Session *s;                 /* NULL once the session is gone (timeout, cancel) */
    PandaInjectCall call;       /* the call out, for a restart */
    /* MIPS o32: arguments 5 and 6 at sp + 16, the words that were there */
    bool stack_saved;
    uint32_t stack_words[2];
    InjRegs regs;
} Held;

struct Session {
    QTAILQ_ENTRY(Session) next;
    uint64_t id;
    int kind;
    uint64_t target;            /* asid or task key */
    /* a held entry: its thread is at (asid, sp, pc), len bytes of trap; or, with rewind, a
     * skipped call: its thread comes back at (asid, sp, pc = the trap's return address) */
    bool hold;
    bool rewind;
    PandaInjectRewind rw;
    uint64_t hold_asid, hold_sp, hold_pc;
    unsigned hold_len;
    int64_t deadline;           /* 0: none */
    bool inflight;              /* a thread runs it now (in a call, or in next()) */
    bool busy;                  /* next() is running: nobody but its thread may finish it */
    bool cancelled;             /* end it at its thread's next boundary */
    bool has_pending;           /* a call next() gave that could not go out yet */
    PandaInjectCall pending;
    uint32_t calls;
    panda_inject_next_fn next_fn;
    panda_inject_done_fn done_fn;
    void *opaque;
};

typedef struct Resumed {
    uint64_t asid, sp, pc;
} Resumed;

static QemuMutex lock;
static bool inited;
static bool debug;              /* PANDA_INJECT_DEBUG: log each call and its answer */
static uint32_t token_seq, token_salt;
static QTAILQ_HEAD(, Session) sessions = QTAILQ_HEAD_INITIALIZER(sessions);
static QTAILQ_HEAD(, Held) held = QTAILQ_HEAD_INITIALIZER(held);
static unsigned nsessions, nheld;
static uint64_t next_id = 1;
static int64_t frozen_until;    /* QEMU_CLOCK_REALTIME: a recording is about to start */
static Resumed resumed[INJECT_RESUMED_MAX];
static unsigned nresumed;
static PandaInjectStats st;

/* Completions to run after the lock is dropped */
typedef struct Done {
    Session *s;
    int status;
} Done;
#define NDONE 16

static void inject_init(void)
{
    if (!qatomic_read(&inited)) {
        static gsize once;
        if (g_once_init_enter(&once)) {
            qemu_mutex_init(&lock);
            debug = getenv("PANDA_INJECT_DEBUG") != NULL;
            token_salt = g_random_int();
            qatomic_set(&inited, true);
            g_once_init_leave(&once, 1);
        }
    }
}

#define DBG(...) do { if (debug) { fprintf(stderr, "inject: " __VA_ARGS__); } } while (0)

/* lock held: what the CPU loop has to look at */
static void update_loop_bit(void)
{
    if (QTAILQ_EMPTY(&sessions) && QTAILQ_EMPTY(&held)) {
        qatomic_and(&panda_loop_hooks, ~PANDA_LOOP_INJECT);
    } else {
        qatomic_or(&panda_loop_hooks, PANDA_LOOP_INJECT);
    }
}

/* lock held: take a session off the queue, to complete once the lock is dropped */
static void finish(Session *s, int status, Done *d, int *nd)
{
    Held *h;

    QTAILQ_REMOVE(&sessions, s, next);
    nsessions--;
    QTAILQ_FOREACH(h, &held, next) {
        if (h->s == s) {
            h->s = NULL;        /* the registers still go back */
        }
    }
    st.completed++;
    g_assert(*nd < NDONE);
    d[*nd].s = s;
    d[*nd].status = status;
    (*nd)++;
}

static void complete(Done *d, int n)
{
    for (int i = 0; i < n; i++) {
        Session *s = d[i].s;
        DBG("session %" PRIu64 " done: %s\n", s->id, panda_inject_strerror(d[i].status));
        if (s->done_fn) {
            s->done_fn(s->opaque, s->id, d[i].status);
        }
        g_free(s);
    }
}

const char *panda_inject_strerror(int status)
{
    switch (status) {
    case PANDA_INJECT_OK: return "ok";
    case PANDA_INJECT_QUEUED: return "queued";
    case PANDA_INJECT_ETIMEDOUT: return "the target did not run in user mode in time";
    case PANDA_INJECT_ECANCELED: return "cancelled (or a snapshot load or reset came first)";
    case PANDA_INJECT_ERECORD: return "refused: record/replay is active or starting";
    case PANDA_INJECT_EACCEL: return "refused: not supported under this accelerator";
    case PANDA_INJECT_EARCH: return "refused: no system-call injector for this target or task";
    case PANDA_INJECT_EINVAL: return "refused: bad target or no next()";
    case PANDA_INJECT_EBUSY: return "refused: too many sessions outstanding";
    case PANDA_INJECT_EABANDONED: return "the thread came back somewhere else (longjmp, rseq)";
    case PANDA_INJECT_ENOTHERE: return "refused: not at a system-call entry that can be held";
    case PANDA_INJECT_EFAILED: return "next() gave up";
    }
    return "unknown";
}

void panda_inject_stats(PandaInjectStats *out)
{
    inject_init();
    qemu_mutex_lock(&lock);
    *out = st;
    out->held = nheld;
    out->queued = nsessions;
    qemu_mutex_unlock(&lock);
}

bool panda_inject_record_safe(Error **errp)
{
    if (!qatomic_read(&inited)) {
        return true;
    }
    qemu_mutex_lock(&lock);
    if (nheld) {
        qemu_mutex_unlock(&lock);
        error_setg(errp, "%u guest thread%s inside an injected system call (registers go back "
                   "when the call returns: try again)", nheld, nheld == 1 ? " is" : "s are");
        return false;
    }
    /* nothing starts until the recording has (replay_analysis_write_allowed() then refuses) */
    frozen_until = qemu_clock_get_ns(QEMU_CLOCK_REALTIME) + INJECT_FREEZE_NS;
    qemu_mutex_unlock(&lock);
    return true;
}

/* lock held */
static bool may_start(void)
{
    if (!replay_analysis_write_allowed()) {
        return false;
    }
    if (frozen_until) {
        if (replay_mode == REPLAY_MODE_NONE &&
            qemu_clock_get_ns(QEMU_CLOCK_REALTIME) < frozen_until) {
            return false;
        }
        frozen_until = 0;
    }
    return true;
}

static int submit_common(Session *s, uint64_t timeout_ns, uint64_t *id)
{
    if (!s->next_fn && !s->rewind) {
        g_free(s);
        return PANDA_INJECT_EINVAL;
    }
    if (!replay_analysis_write_allowed()) {
        g_free(s);
        return PANDA_INJECT_ERECORD;    /* the calls would not be in the recording */
    }
    if (!tcg_enabled()) {
        g_free(s);
        return PANDA_INJECT_EACCEL;
    }
#ifdef INJ_NO_INJECTOR
    g_free(s);
    return PANDA_INJECT_EARCH;
#else
    if (timeout_ns != UINT64_MAX) {
        s->deadline = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                      (timeout_ns ? timeout_ns : INJECT_TIMEOUT_NS);
    }
    qemu_mutex_lock(&lock);
    if (nsessions >= PANDA_INJECT_MAX_OUTSTANDING) {
        qemu_mutex_unlock(&lock);
        g_free(s);
        return PANDA_INJECT_EBUSY;
    }
    s->id = next_id++;
    if (id) {
        *id = s->id;
    }
    QTAILQ_INSERT_TAIL(&sessions, s, next);
    nsessions++;
    st.submitted++;
    update_loop_bit();
    qemu_mutex_unlock(&lock);
    return PANDA_INJECT_QUEUED;
#endif
}

static void kick_all(void)
{
    CPUState *cs;

    /* Every vCPU back to its loop, where the session is looked at */
    WITH_RCU_READ_LOCK_GUARD() {
        CPU_FOREACH(cs) {
            cpu_exit(cs);
        }
    }
}

int panda_inject_submit(int kind, uint64_t target, uint64_t timeout_ns,
                        panda_inject_next_fn next, panda_inject_done_fn done, void *opaque,
                        uint64_t *id)
{
    Session *s;
    int r;

    inject_init();
    if (kind != PANDA_INJECT_TARGET_ASID && kind != PANDA_INJECT_TARGET_TASK) {
        return PANDA_INJECT_EINVAL;
    }
    s = g_new0(Session, 1);
    s->kind = kind;
    s->target = target;
    s->next_fn = next;
    s->done_fn = done;
    s->opaque = opaque;
    r = submit_common(s, timeout_ns, id);
    if (r == PANDA_INJECT_QUEUED) {
        kick_all();
    }
    return r;
}

int panda_inject_hold(CPUState *cpu, PandaSyscallCall *call, uint64_t timeout_ns,
                      panda_inject_next_fn next, panda_inject_done_fn done, void *opaque,
                      uint64_t *id)
{
    uint64_t sp;
    int abi, r;
    Session *s;

    inject_init();
    if (!cpu || !call || cpu != current_cpu || panda_sc_hold != 1) {
        return PANDA_INJECT_ENOTHERE;
    }
#ifdef INJ_NO_INJECTOR
    return PANDA_INJECT_EARCH;
#else
    if (inj_user(cpu, &sp, &abi) != 1) {
        return PANDA_INJECT_EARCH;
    }
    if (!inj_can(cpu, abi, call->pc + call->len)) {
        return PANDA_INJECT_ENOTHERE;
    }
    s = g_new0(Session, 1);
    s->hold = true;
    s->hold_asid = panda_current_asid(cpu);
    s->hold_sp = sp;
    s->hold_pc = call->pc;
    s->hold_len = call->len;
    s->next_fn = next;
    s->done_fn = done;
    s->opaque = opaque;
    r = submit_common(s, timeout_ns, id);
    if (r == PANDA_INJECT_QUEUED) {
        /* the entry site puts the task back at its trap, and drops the call's pending record
         * once every consumer has seen the entry (panda_sc_hold_end) */
        panda_sc_hold_call = call;
        panda_sc_hold = 2;
        qemu_mutex_lock(&lock);
        st.holds++;
        qemu_mutex_unlock(&lock);
    }
    return r;
#endif
}

int panda_inject_rewind(uint64_t asid, uint64_t sp, uint64_t ret_pc, const PandaInjectRewind *rw,
                        uint64_t timeout_ns, panda_inject_next_fn next,
                        panda_inject_done_fn done, void *opaque, uint64_t *id)
{
    Session *s;
    int r;

    inject_init();
    if (!rw || !rw->len || rw->len > 4 || rw->nargs > 6 || ret_pc < rw->len) {
        return PANDA_INJECT_EINVAL;
    }
    s = g_new0(Session, 1);
    s->hold = true;
    s->rewind = true;
    s->rw = *rw;
    s->hold_asid = asid;
    s->hold_sp = sp;
    s->hold_pc = ret_pc;
    s->hold_len = rw->len;
    s->next_fn = next;
    s->done_fn = done;
    s->opaque = opaque;
    r = submit_common(s, timeout_ns, id);
    if (r == PANDA_INJECT_QUEUED) {
        kick_all();
    }
    return r;
}

bool panda_inject_rewound(uint64_t asid, uint64_t sp, uint64_t pc)
{
    bool found = false;

    if (!qatomic_read(&inited) || !qatomic_read(&panda_inject_resumed_pending)) {
        return false;
    }
    qemu_mutex_lock(&lock);
    for (unsigned i = 0; i < nresumed; i++) {
        if (resumed[i].asid == asid && resumed[i].sp == sp && resumed[i].pc == pc) {
            memmove(&resumed[i], &resumed[i + 1], sizeof(resumed[0]) * (nresumed - i - 1));
            nresumed--;
            qatomic_set(&panda_inject_resumed_pending, nresumed);
            st.resumed++;
            found = true;
            break;
        }
    }
    qemu_mutex_unlock(&lock);
    return found;
}

bool panda_inject_cancel(uint64_t id)
{
    Done d[NDONE];
    int nd = 0;
    Session *s;

    if (!qatomic_read(&inited)) {
        return false;
    }
    qemu_mutex_lock(&lock);
    QTAILQ_FOREACH(s, &sessions, next) {
        if (s->id == id) {
            break;
        }
    }
    if (s && !s->cancelled) {
        s->cancelled = true;
        if (!s->busy) {
            /* a thread inside a call keeps it until it is back: its registers go back then */
            s->inflight = false;
            finish(s, PANDA_INJECT_ECANCELED, d, &nd);
        }
        /* else: next() is running; its thread ends it when next() returns */
    } else {
        s = NULL;
    }
    update_loop_bit();
    qemu_mutex_unlock(&lock);
    complete(d, nd);
    if (s) {
        kick_all();
    }
    return s != NULL;
}

int panda_inject_abi(CPUState *cpu)
{
#ifdef INJ_NO_INJECTOR
    return -1;
#else
    uint64_t sp;
    int abi;
    return cpu && inj_user(cpu, &sp, &abi) == 1 ? abi : -1;
#endif
}

/* --- Held entries delivered again ----------------------------------------------------- */

/* lock held */
static void resumed_add(uint64_t asid, uint64_t sp, uint64_t pc)
{
    if (nresumed == INJECT_RESUMED_MAX) {
        memmove(&resumed[0], &resumed[1], sizeof(resumed[0]) * (INJECT_RESUMED_MAX - 1));
        nresumed--;
    }
    resumed[nresumed++] = (Resumed) { asid, sp, pc };
    qatomic_set(&panda_inject_resumed_pending, nresumed);
}

uint32_t panda_inject_resumed_take(CPUState *cpu, uint64_t pc)
{
#ifdef INJ_NO_INJECTOR
    return 0;
#else
    uint64_t sp, asid;
    uint32_t f = 0;
    int abi;

    if (inj_user(cpu, &sp, &abi) != 1) {
        return 0;
    }
    asid = panda_current_asid(cpu);
    qemu_mutex_lock(&lock);
    for (unsigned i = 0; i < nresumed; i++) {
        if (resumed[i].asid == asid && resumed[i].sp == sp && resumed[i].pc == pc) {
            memmove(&resumed[i], &resumed[i + 1], sizeof(resumed[0]) * (nresumed - i - 1));
            nresumed--;
            qatomic_set(&panda_inject_resumed_pending, nresumed);
            st.resumed++;
            f = PANDA_SYSCALL_RESUMED;
            break;
        }
    }
    qemu_mutex_unlock(&lock);
    return f;
#endif
}

#ifndef INJ_NO_INJECTOR

/* --- The service ---------------------------------------------------------------------- */

static uint64_t new_token(void)
{
    /* below 2^31 and not 0: the same in a 32-bit register and sign-extended on MIPS64 */
    return 0x40000000u | ((++token_seq * 0x9e3779b1u ^ token_salt) & 0x3fffffffu);
}

/* MIPS o32: arguments 5 and 6 go at sp + 16 and sp + 20 (the caller's argument area) */
static bool stack_args(const Held *h, const PandaInjectCall *c)
{
    return h->abi == PANDA_INJECT_ABI_MIPS_O32 && (c->nargs < 0 || c->nargs > 4);
}

static bool user_rw(uint64_t addr, void *buf, size_t len, bool write)
{
    GByteArray *b = g_byte_array_new();
    int ret;

    if (write) {
        g_byte_array_append(b, buf, len);
        ret = qemu_plugin_write_memory_vaddr_fault(addr, b, QEMU_PLUGIN_VADDR_USER, false);
    } else {
        ret = qemu_plugin_read_memory_vaddr_fault(addr, b, len, QEMU_PLUGIN_VADDR_USER, false);
        if (ret == QEMU_PLUGIN_VADDR_OK) {
            memcpy(buf, b->data, len);
        }
    }
    g_byte_array_unref(b);
    return ret == QEMU_PLUGIN_VADDR_OK;
}

static void put_word(uint8_t *p, uint32_t v, bool be)
{
    if (be) {
        stl_be_p(p, v);
    } else {
        stl_le_p(p, v);
    }
}

/*
 * Before injecting @c from @h: on MIPS o32, arguments 5 and 6 into the stack words (saving what
 * was there the first time). False if the stack words can't be reached without a fault now:
 * *touch is the address for the thread to access first.
 */
static bool stack_put(CPUState *cs, Held *h, const PandaInjectCall *c, uint64_t *touch)
{
    uint8_t w[8];
    bool be;

    if (!stack_args(h, c)) {
        return true;
    }
#if TARGET_BIG_ENDIAN
    be = true;
#else
    be = false;
#endif
    if (!h->stack_saved) {
        if (!user_rw(h->sp + 16, w, 8, false)) {
            *touch = h->sp + 16;
            return false;
        }
        h->stack_words[0] = be ? ldl_be_p(w) : ldl_le_p(w);
        h->stack_words[1] = be ? ldl_be_p(w + 4) : ldl_le_p(w + 4);
        h->stack_saved = true;
    }
    put_word(w, c->args[4], be);
    put_word(w + 4, c->args[5], be);
    if (!user_rw(h->sp + 16, w, 8, true)) {
        *touch = h->sp + 16;
        return false;
    }
    return true;
}

/* At the end: the stack words back. False if they can't be reached now (*touch: where). */
static bool stack_restore(Held *h, uint64_t *touch)
{
    uint8_t w[8];
    bool be;

    if (!h->stack_saved) {
        return true;
    }
#if TARGET_BIG_ENDIAN
    be = true;
#else
    be = false;
#endif
    put_word(w, h->stack_words[0], be);
    put_word(w + 4, h->stack_words[1], be);
    if (!user_rw(h->sp + 16, w, 8, true)) {
        *touch = h->sp + 16;
        return false;
    }
    h->stack_saved = false;
    return true;
}

/*
 * A user-mode access of the thread's own at this boundary, which may fault (a page the guest's
 * kernel refills in software, or populated by a call that just succeeded): the kernel resolves
 * it and returns to this instruction. Returns only if no fault was needed.
 */
static void touch_page(CPUState *cs, uint64_t addr, bool write)
{
    DBG("vcpu %d touch %#" PRIx64 " %s at %#" PRIx64 "\n", cs->cpu_index, addr,
        write ? "write" : "read", inj_get_pc(cs));
    if (!cs->cc->tcg_ops->user_mmu_index) {
        return;
    }
    probe_access(cpu_env(cs), addr, 1, write ? MMU_DATA_STORE : MMU_DATA_LOAD,
                 cs->cc->tcg_ops->user_mmu_index(cs), 0);
}

/* lock held: does @s target the thread at this boundary? */
static bool session_matches(CPUState *cs, Session *s, uint64_t asid, uint64_t sp, uint64_t pc)
{
    uint64_t key;

    if (s->inflight || s->cancelled) {
        return false;
    }
    if (s->hold) {
        return s->hold_asid == asid && s->hold_sp == sp && s->hold_pc == pc;
    }
    if (s->kind == PANDA_INJECT_TARGET_ASID) {
        return s->target == asid;
    }
    switch (panda_task_key(cs, &key)) {
    case PANDA_TASK_KEY_REG:
    case PANDA_TASK_KEY_WORD:
        return key == s->target;
    }
    return false;
}

/* Inject @h's call. Drops the lock; does not return. */
static G_NORETURN void go(CPUState *cs, Held *h, Done *d, int nd)
{
    /*
     * The token goes out with each call whose arguments leave the marker register free (on i386
     * a six-argument call takes ebp); otherwise this call's return is recognized by its stack
     * pointer and return address alone.
     */
    if (!inj_marker_ok(h->abi, h->call.nargs)) {
        h->token = 0;
    } else if (!h->token) {
        h->token = new_token();
    }
    if (h->token) {
        inj_marker_set(cs, h->abi, h->token);
    }
    DBG("vcpu %d asid %#" PRIx64 " session %" PRIu64 " call %" PRId64 " (%#" PRIx64 ", %#"
        PRIx64 ", %#" PRIx64 ", ...) returning to %#" PRIx64 " sp %#" PRIx64 "\n", cs->cpu_index,
        h->asid, h->s ? h->s->id : 0, h->call.nr, h->call.args[0], h->call.args[1],
        h->call.args[2], h->ret_pc, h->sp);
    update_loop_bit();
    qemu_mutex_unlock(&lock);
    complete(d, nd);
    inj_enter(cs, h->abi, h->ret_pc, &h->call);
}

/* What next() answered, with the call it gave */
typedef struct Answer {
    int r;
    PandaInjectCall c;
    uint64_t touch;
} Answer;

/*
 * The session's next call: one that could not go out before, or next()'s answer (called without
 * the lock; @s is inflight and busy meanwhile, so nobody else finishes it).
 */
static void ask(CPUState *cs, Session *s, int abi, uint32_t calls, int64_t ret, Answer *a)
{
    PandaInjectStep step = {
        .id = s->id, .abi = abi, .calls = calls, .ret = ret, .touch = (uint64_t)-1,
    };

    if (s->has_pending) {
        s->has_pending = false;
        a->r = PANDA_INJECT_CALL;
        a->c = s->pending;
        a->touch = (uint64_t)-1;
        return;
    }
    a->c = (PandaInjectCall) { .nargs = -1 };
    if (!s->next_fn) {
        a->r = PANDA_INJECT_DONE;       /* a rewind alone */
        a->touch = (uint64_t)-1;
        return;
    }
    s->busy = true;
    qemu_mutex_unlock(&lock);
    a->r = s->next_fn(s->opaque, cs, &step, &a->c);
    qemu_mutex_lock(&lock);
    s->busy = false;
    a->touch = step.touch;
    if (a->r < PANDA_INJECT_DONE || a->r > PANDA_INJECT_FAIL) {
        a->r = PANDA_INJECT_FAIL;
    }
    if (s->cancelled && a->r == PANDA_INJECT_CALL) {
        s->has_pending = false;
        a->r = PANDA_INJECT_DONE;
    }
}

/* lock held: the session's state after its thread let go of it, with next()'s answer @r */
static void settle(Session *s, int r, bool hold_at_trap, uint64_t asid, uint64_t sp, uint64_t pc,
                   int64_t now, Done *d, int *nd)
{
    s->inflight = false;
    if (s->cancelled) {
        finish(s, s->deadline && now > s->deadline ? PANDA_INJECT_ETIMEDOUT
                                                   : PANDA_INJECT_ECANCELED, d, nd);
    } else if (r == PANDA_INJECT_DONE) {
        finish(s, PANDA_INJECT_OK, d, nd);
    } else if (r == PANDA_INJECT_FAIL) {
        finish(s, PANDA_INJECT_EFAILED, d, nd);
    }
    /* AGAIN: it stays queued */
    if (hold_at_trap && r != PANDA_INJECT_AGAIN) {
        resumed_add(asid, sp, pc);      /* the thread now executes its trap */
    }
}

/* A skipped call's number and arguments back in their registers (the PC is set already) */
static void put_back(CPUState *cs, int abi, const PandaInjectRewind *rw)
{
    PandaInjectCall c = { .nr = rw->nr, .nargs = rw->nargs };

    memcpy(c.args, rw->args, sizeof(c.args));
    inj_set_call(cs, abi, &c);
    st.rewinds++;
}

/*
 * @h's thread leaves the session: its stack words and registers go back, the PC to where it
 * resumes. Lock held. False if the stack words can't be reached now (*touch: where).
 */
static bool release(CPUState *cs, Held *h, uint64_t *touch)
{
    if (!stack_restore(h, touch)) {
        return false;
    }
    inj_restore(cs, &h->regs);
    inj_set_pc(cs, h->pc);
    if (h->rewind) {
        put_back(cs, h->abi, &h->rw);
    }
    QTAILQ_REMOVE(&held, h, next);
    nheld--;
    if (h->hold) {
        resumed_add(h->asid, h->sp, h->pc);
    }
    return true;
}

/* Unlock, complete, take the thread's own access at @touch if any, and look the block up again */
static G_NORETURN void leave(CPUState *cs, Done *d, int nd, uint64_t touch)
{
    update_loop_bit();
    qemu_mutex_unlock(&lock);
    complete(d, nd);
    if (touch != (uint64_t)-1) {
        touch_page(cs, touch, false);
    }
    cpu_loop_exit_noexc(cs);
}

/* A thread back at the return address (or restart point) of @h's call, with its token */
static G_NORETURN void back(CPUState *cs, Held *h, uint64_t pc, int64_t now, Done *d, int nd)
{
    Session *s = h->s;
    uint64_t touch = (uint64_t)-1;
    Answer a = { .r = PANDA_INJECT_DONE, .touch = (uint64_t)-1 };

    if (h->finishing) {
        a.r = h->final_r;               /* only the stack words were left to put back */
    } else if (pc == h->ret_pc - h->len) {
        /* the kernel restarts the call (its return address minus the instruction): again */
        st.restarts++;
        go(cs, h, d, nd);
    } else {
        int64_t ret = inj_retval(cs, h->abi);

        if (s && !s->cancelled) {
            s->calls++;
            h->calls++;
            ask(cs, s, h->abi, h->calls, ret, &a);
        }
        if (a.r == PANDA_INJECT_CALL) {
            h->call = a.c;
            if (stack_put(cs, h, &a.c, &touch)) {
                st.calls++;
                go(cs, h, d, nd);
            }
            /* the stack arguments are a fault away: end this part, the call waits */
            s->has_pending = true;
            s->pending = a.c;
            a.r = PANDA_INJECT_AGAIN;
            a.touch = touch;
        }
    }
    touch = a.touch;
    if (!release(cs, h, &touch)) {
        /* the stack words are a fault away: the thread takes it at this instruction, with
         * the token, and comes back here */
        h->finishing = true;
        h->final_r = a.r;
        leave(cs, d, nd, touch);
    }
    if (s) {
        settle(s, a.r, false, 0, 0, 0, now, d, &nd);
    }
    g_free(h);
    leave(cs, d, nd, touch);
}

void panda_inject_service(CPUState *cs)
{
    Done d[NDONE];
    int nd = 0;
    uint64_t sp = 0, asid, pc, ret_pc, resume_pc, touch = (uint64_t)-1;
    int user, abi = -1;
    int64_t now;
    Session *s, *sn;
    Held *h, *hn;
    Answer a;

    inject_init();
    user = inj_user(cs, &sp, &abi);
    now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    qemu_mutex_lock(&lock);

    /*
     * A recording or replay started after these were queued: serving them now would put system
     * calls in the recording that its replay never makes. Refuse them. A thread inside an
     * injected call can't be here (starting a recording is refused while one is); if one is
     * anyway, its registers can't go back unlogged: say so.
     */
    if (!replay_analysis_write_allowed()) {
        QTAILQ_FOREACH_SAFE(s, &sessions, next, sn) {
            if (nd < NDONE && !s->busy) {
                s->inflight = false;
                finish(s, PANDA_INJECT_ERECORD, d, &nd);
            }
        }
        if (!QTAILQ_EMPTY(&held)) {
            static bool said;
            if (!said) {
                said = true;
                fprintf(stderr, "panda: inject: %u thread(s) inside an injected system call "
                        "while recording or replaying; their registers stay as the call left "
                        "them\n", nheld);
            }
            while ((h = QTAILQ_FIRST(&held))) {
                QTAILQ_REMOVE(&held, h, next);
                nheld--;
                g_free(h);
            }
        }
        goto out;
    }

    /* Past their deadline: the session ends now (a thread inside one of its calls gets its
     * registers back when it returns), unless next() is running */
    QTAILQ_FOREACH_SAFE(s, &sessions, next, sn) {
        if (s->deadline && now > s->deadline && nd < NDONE) {
            if (!s->busy) {
                s->inflight = false;
                finish(s, PANDA_INJECT_ETIMEDOUT, d, &nd);
            } else {
                s->cancelled = true;
            }
        }
    }
    if (user == 0) {
        goto out;
    }
    asid = panda_current_asid(cs);
    pc = inj_get_pc(cs);

    /*
     * A thread back from an injected call. It is ours if it carries the call's token in the
     * marker register, at the call's stack pointer and return address (or restart point). With
     * the token elsewhere it is running a signal handler over the call: leave it alone. Without
     * the token at the call's place, it left the call (a handler longjmp'd) and came back by
     * another path: restoring there would be wrong.
     */
    h = NULL;
    if (user > 0) {
        QTAILQ_FOREACH(hn, &held, next) {
            if (hn->asid != asid) {
                continue;
            }
            if (hn->token ? inj_marker_get(cs, hn->abi) == hn->token
                          : (hn->sp == sp && (pc == hn->ret_pc || pc == hn->ret_pc - hn->len))) {
                h = hn;
                break;
            }
        }
    }
    if (h) {
        if (h->sp != sp || (pc != h->ret_pc && pc != h->ret_pc - h->len)) {
            st.overframes++;
            goto out;
        }
        back(cs, h, pc, now, d, nd);
    }
    if (user > 0) {
        /* held sets this thread can no longer come back to: it left them without the token */
        QTAILQ_FOREACH_SAFE(hn, &held, next, h) {
            if (hn->asid != asid || hn->sp != sp || !hn->token || nd >= NDONE) {
                continue;
            }
            DBG("vcpu %d asid %#" PRIx64 " session %" PRIu64 " abandoned at %#" PRIx64
                " (call returns to %#" PRIx64 ")\n", cs->cpu_index, asid,
                hn->s ? hn->s->id : 0, pc, hn->ret_pc);
            QTAILQ_REMOVE(&held, hn, next);
            nheld--;
            st.abandoned++;
            if (pc == hn->ret_pc || pc == hn->ret_pc - hn->len) {
                st.stale++;
            }
            if (hn->s) {
                hn->s->inflight = false;
                finish(hn->s, PANDA_INJECT_EABANDONED, d, &nd);
            }
            g_free(hn);
        }
    }

    /* A session for this boundary */
    QTAILQ_FOREACH(s, &sessions, next) {
        if (session_matches(cs, s, asid, sp, pc)) {
            break;
        }
    }
    if (!s || nd >= NDONE) {
        goto out;
    }
    if (user < 0) {
        finish(s, PANDA_INJECT_EARCH, d, &nd);
        goto out;
    }
    ret_pc = s->hold && !s->rewind ? pc + s->hold_len : pc;
    resume_pc = s->rewind ? pc - s->rw.len : pc;
    if (!may_start() || !inj_can(cs, abi, ret_pc)) {
        if (s->hold) {
            /* the thread is at its trap (or back from a skipped call): it can't wait there; the
             * call goes on without the session */
            finish(s, may_start() ? PANDA_INJECT_ENOTHERE : PANDA_INJECT_ERECORD, d, &nd);
            if (s->rewind) {
                inj_set_pc(cs, resume_pc);
                put_back(cs, abi, &s->rw);
                resumed_add(asid, sp, resume_pc);
                leave(cs, d, nd, (uint64_t)-1);
            }
            resumed_add(asid, sp, pc);
        }
        goto out;
    }
    s->inflight = true;
    ask(cs, s, abi, 0, 0, &a);
    if (a.r == PANDA_INJECT_CALL) {
        h = g_new0(Held, 1);
        h->asid = asid;
        h->sp = sp;
        h->pc = resume_pc;
        h->ret_pc = ret_pc;
        h->abi = abi;
        h->len = inj_len(cs, abi);
        h->hold = s->hold;
        h->rewind = s->rewind;
        h->rw = s->rw;
        h->s = s;
        h->call = a.c;
        inj_save(cs, &h->regs);
        if (stack_put(cs, h, &a.c, &touch)) {
            QTAILQ_INSERT_TAIL(&held, h, next);
            nheld++;
            st.calls++;
            go(cs, h, d, nd);
        }
        g_free(h);
        s->has_pending = true;
        s->pending = a.c;
        a.r = PANDA_INJECT_AGAIN;
        a.touch = touch;
    }
    /* nothing injected: nothing to put back but what next() changed */
    if (s->hold && a.r == PANDA_INJECT_AGAIN && a.touch == (uint64_t)-1) {
        a.r = PANDA_INJECT_FAIL;        /* a held thread can only wait for its own access */
    }
    if (s->rewind && a.r != PANDA_INJECT_AGAIN) {
        /* nothing was injected: the skipped call goes back at its trap now */
        settle(s, a.r, false, asid, sp, pc, now, d, &nd);
        inj_set_pc(cs, resume_pc);
        put_back(cs, abi, &s->rw);
        resumed_add(asid, sp, resume_pc);
        leave(cs, d, nd, (uint64_t)-1);
    }
    settle(s, a.r, s->hold, asid, sp, pc, now, d, &nd);
    if (a.touch != (uint64_t)-1 || (s->hold && a.r == PANDA_INJECT_AGAIN)) {
        /* the thread's own access, then this boundary again (a held thread stays at its trap) */
        leave(cs, d, nd, a.touch);
    }
out:
    update_loop_bit();
    qemu_mutex_unlock(&lock);
    complete(d, nd);
}

#else

void panda_inject_service(CPUState *cs)
{
}

#endif

/* A snapshot load or reset: the state the held registers belong to is gone */
void panda_inject_cancel_all(void)
{
    Done d[NDONE];
    int nd;
    Held *h;
    Session *s;

    if (!qatomic_read(&inited)) {
        return;
    }
    for (;;) {
        nd = 0;
        qemu_mutex_lock(&lock);
        while ((h = QTAILQ_FIRST(&held))) {
            QTAILQ_REMOVE(&held, h, next);
            g_free(h);
        }
        nheld = 0;
        nresumed = 0;
        qatomic_set(&panda_inject_resumed_pending, 0);
        while (nd < NDONE && (s = QTAILQ_FIRST(&sessions))) {
            s->inflight = false;
            finish(s, PANDA_INJECT_ECANCELED, d, &nd);
        }
        update_loop_bit();
        qemu_mutex_unlock(&lock);
        if (!nd) {
            return;
        }
        complete(d, nd);
    }
}
