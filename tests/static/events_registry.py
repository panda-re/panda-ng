#!/usr/bin/env python3
"""The PANDA event registry, host-only: the real code against a model.

No engine and no guest. engine/src/panda/src/events.c is compiled whole, with ASan and UBSan,
against stub headers (atomics, a deferred-free RCU, a non-recursive lock, the admission inputs as
variables) into a program that reads registry calls from stdin. A Python model written from
include/panda/events.h's contract runs the same calls. After every call the two must agree on what
it returned, the order subscribers ran in (and whether each got its instance data, zeroed at open
and the same bytes at close), and every subscription's state, with the reason naming the rule.

The calls come from a simulator: four plugins that declare events (good and malformed), find them
under the publisher's signature or another one (a type changed, a field appended), subscribe as
observers, early observers and mutators with priorities and instance data, enable, disable and
unsubscribe, unload, and emit, open, close and drop, while the run moves between analysis,
record, replay and KVM. A reader can hold a dispatch snapshot across changes and walk it before
the grace period ends (ASan catches an early free). A failing sequence is shrunk and printed.

Then directed cases: UUIDv5 against Python's uuid5 (and the 10-30 vector), signature parsing
and the layout PANDA_EVENT_STRUCT gives, PANDA_EVENT_HAS, a subscriber removing a later one
mid-dispatch, the JSON listing, and the rr check.

    python3 tests/static/events_registry.py [--seqs N] [--seed S]
    (nix build .#checks.x86_64-linux.events-registry)

Needs a C compiler with ASan and UBSan, pkg-config and GLib.
"""
import argparse
import json
import os
import random
import shlex
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "engine/src"
EVENTS_C = SRC / "panda/src/events.c"

PANDA_NS = uuid.UUID("40c975de-b1b1-4814-923b-7b613e2fef15")
OTHER_NS = uuid.UUID("6ba7b810-9dad-11d1-80b4-00c04fd430c8")    # uuid.NAMESPACE_DNS
NSS = [PANDA_NS, OTHER_NS]
ANALYSIS, FUZZ, RR, KVM = 1, 2, 4, 8
MUTABLE, RR_MUTATE, INSTANCES = 1, 2, 4
OBSERVE, MUTATE, ORIGINAL = 1, 2, 4
STATES = ["active", "pending", "mismatch", "refused", "disabled"]
ACTIVE, PENDING, MISMATCH, REFUSED, DISABLED = range(5)
NAMES = ["a.x", "a.y", "cb.alpha"]
SIGS = ["cpu:ptr,pc:u64", "cpu:ptr,pc:u32", "cpu:ptr,pc:u64|flags:u32", "cpu:ptr,pc:u64,"]
CORE = (0, "cb.alpha", "cpu:ptr,pc:u64")        # the stub core table's one declared type

STUBS = {
"qemu/osdep.h": r'''
#pragma once
#include <glib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ROUND_UP(n, d) ((((n) + (d) - 1) / (d)) * (d))
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
''',
"qemu/atomic.h": r'''
#pragma once
#define qatomic_read(p) (*(p))
#define qatomic_set(p, v) (*(p) = (v))
#define qatomic_inc(p) (++*(p))
#define qatomic_fetch_dec(p) ((*(p))--)
#define qatomic_rcu_read(p) (*(p))
#define qatomic_rcu_set(p, v) (*(p) = (v))
''',
"qemu/rcu.h": r'''
#pragma once
/* Frees wait for the harness's grace period ("flush"), so a held snapshot stays readable. */
struct rcu_head { struct rcu_head *next; void (*func)(void *); };
extern struct rcu_head *rcu_deferred;
static inline void call_rcu1(struct rcu_head *h, void (*f)(void *))
{
    h->func = f; h->next = rcu_deferred; rcu_deferred = h;
}
#define call_rcu(head, func, field) call_rcu1(&(head)->field, (void (*)(void *))(func))
#define g_free_rcu(obj, field) call_rcu1(&(obj)->field, g_free)
''',
"qemu/thread.h": r'''
#pragma once
typedef struct { int held; } QemuMutex;
static inline void qemu_mutex_init(QemuMutex *m) { m->held = 0; }
''',
"qemu/lockable.h": r'''
#pragma once
/* Not recursive: taking it twice is a bug (a deadlock in the engine). */
static inline void lock_release_(QemuMutex **m) { (*m)->held = 0; }
static inline QemuMutex *lock_take_(QemuMutex *m) { g_assert(!m->held); m->held = 1; return m; }
#define QEMU_LOCK_GUARD(m) \
    QemuMutex *lock_guard_ __attribute__((cleanup(lock_release_))) = lock_take_(m)
''',
"qemu/error-report.h": r'''
#pragma once
#define error_report(...) (fprintf(stderr, "error: " __VA_ARGS__), fputc('\n', stderr))
#define warn_report(...) (fprintf(stderr, "warn: " __VA_ARGS__), fputc('\n', stderr))
''',
"system/analysis-policy.h": r'''
#pragma once
#define ANALYSIS_CAP_ANALYSIS 1u
extern bool env_frozen, env_analysis;
extern uint32_t env_caps;
static inline bool analysis_policy_is_frozen(void) { return env_frozen; }
static inline bool analysis_policy_has(uint32_t c)
{
    return env_analysis && (c == ANALYSIS_CAP_ANALYSIS || (env_caps & c) == c);
}
''',
"system/kvm.h": r'''
#pragma once
extern bool env_kvm;
static inline bool kvm_enabled(void) { return env_kvm; }
''',
"system/replay.h": r'''
#pragma once
enum { REPLAY_MODE_NONE, REPLAY_MODE_RECORD, REPLAY_MODE_PLAY };
extern int replay_mode;
''',
"panda/plugin.h": r'''
#pragma once
enum { PANDA_MODE_ANALYSIS = 1, PANDA_MODE_FUZZ = 2, PANDA_MODE_RR = 4, PANDA_MODE_KVM = 8 };
''',
}

HARNESS = r'''
#include "panda/events-internal.h"

struct rcu_head *rcu_deferred;
bool env_frozen = true, env_analysis = true, env_kvm;
uint32_t env_caps;
int replay_mode;

const PandaCoreEventDef panda_core_events[] = {
    { 0, "alpha", "cpu:ptr,pc:u64", false },
    { 1, "beta", "cpu:ptr", false },                /* no modes: not declared */
};
const size_t panda_core_events_count = 2;
uint32_t panda_core_event_modes(int type) { return type == 0 ? 7 : 0; }

static const char *owner_names[] = { "?", "p1", "p2", "p3", "p4" };
const char *panda_event_owner_name(void *owner)
{
    uintptr_t i = (uintptr_t)owner;
    return i < 5 ? owner_names[i] : NULL;
}
void panda_core_event_describe_subs(int type, GString *out, bool json)
{
    if (type == 0) {
        panda_events_describe_sub(out, json, "p9", "active", "panda_register_callback",
                                  "observe", 7);
    }
}

static void rcu_flush(void)
{
    while (rcu_deferred) {
        struct rcu_head *h = rcu_deferred;
        rcu_deferred = h->next;
        h->func(h);
    }
}

static const panda_event_uuid NS[2] = {
    { { 0x40, 0xc9, 0x75, 0xde, 0xb1, 0xb1, 0x48, 0x14,
        0x92, 0x3b, 0x7b, 0x61, 0x3e, 0x2f, 0xef, 0x15 } },
    { { 0x6b, 0xa7, 0xb8, 0x10, 0x9d, 0xad, 0x11, 0xd1,
        0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8 } },
};

#define MAXE 64
#define MAXS 512
#define MAXI 256
static panda_event_id evs[MAXE];
static int nevs;
static panda_sub subs[MAXS];
static long owners[MAXS];
static int nsubs;
static panda_event_instance insts[MAXI];
static int ninsts;
static GString *calls;
static int phase;               /* 0 emit, 1 open, 2 close */
static unsigned serial;
static panda_sub victim;        /* the 'K' case: a subscriber removes it mid-dispatch */

static int ev_index(panda_event_id ev)
{
    if (!ev) {
        return -1;
    }
    for (int i = 0; i < nevs; i++) {
        if (evs[i] == ev) {
            return i;
        }
    }
    g_assert(nevs < MAXE);
    evs[nevs] = ev;
    return nevs++;
}

typedef struct { int id; } Ctx;
static Ctx ctxs[MAXS];

static void on_event(void *ctx, void *args, void *data)
{
    int id = ((Ctx *)ctx)->id;
    char d = '-';

    g_assert(*(uint32_t *)args == 0xa5a5);
    if (data && phase == 1) {
        for (int i = 0; i < 16; i++) {
            g_assert(((uint8_t *)data)[i] == 0);        /* zeroed at open */
        }
        ((uint32_t *)data)[0] = id * 7919u + serial;
        d = 'o';
    } else if (data && phase == 2) {
        g_assert(((uint32_t *)data)[0] == id * 7919u + serial);
        d = 'c';
    } else {
        g_assert(!data);
    }
    g_string_append_printf(calls, " %d:%c", id, d);
    if (victim && id == 0) {
        g_assert(panda_unsubscribe(victim));
        victim = NULL;
    }
}

/* A reader standing in a dispatch: the snapshot it holds must survive until the flush. */
static PandaSubArray *held;

static void print_states(void)
{
    for (int i = 0; i < nsubs; i++) {
        const char *reason = NULL;
        if (!subs[i]) {
            printf(" x");
            continue;
        }
        panda_sub_state st = panda_sub_status(subs[i], &reason);
        printf(" %d", st);
        fprintf(stderr, "reason %d %s\n", i, reason);
    }
}

int main(void)
{
    char line[512];
    uint32_t args[16] = { 0xa5a5 };

    calls = g_string_new(NULL);
    setvbuf(stdout, NULL, _IOLBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        char op = line[0], *rest = line + 1;
        long a[8] = { 0 };
        char s1[64] = "", s2[128] = "";
        int k;

        g_string_truncate(calls, 0);
        switch (op) {
        case 'D': {         /* declare owner ns name sig args_delta modes flags */
            sscanf(rest, "%ld %ld %63s %127s %ld %ld %ld", &a[0], &a[1], s1, s2, &a[2], &a[3],
                   &a[4]);
            char *sig = !strcmp(s2, "-") ? (char *)"" : s2;
            uint32_t layout = 0;
            char *why = NULL;
            if (!panda_event_signature_parse(sig, NULL, NULL, &layout, &why)) {
                g_free(why);
            }
            panda_event_desc d = { sizeof(d), &NS[a[1]], s1, sig, layout + a[2], a[3], 0, a[4] };
            printf("%d", ev_index(panda_event_declare((void *)(uintptr_t)a[0], &d)));
            break;
        }
        case 'F':           /* find ns name sig */
            sscanf(rest, "%ld %63s %127s", &a[0], s1, s2);
            printf("%d", ev_index(panda_event_find(&NS[a[0]], s1, !strcmp(s2, "-") ? "" : s2)));
            break;
        case 'S':           /* subscribe owner ev modes flags */
            sscanf(rest, "%ld %ld %ld %ld", &a[0], &a[1], &a[2], &a[3]);
            g_assert(nsubs < MAXS);
            ctxs[nsubs].id = nsubs;
            owners[nsubs] = a[0];
            subs[nsubs] = panda_subscribe((void *)(uintptr_t)a[0], evs[a[1]], on_event,
                                          &ctxs[nsubs], a[2], a[3]);
            printf("%d", subs[nsubs] ? nsubs : -1);
            nsubs++;
            break;
        case 'U':
            sscanf(rest, "%ld", &a[0]);
            printf("%d", panda_unsubscribe(subs[a[0]]));
            subs[a[0]] = NULL;
            break;
        case 'E':
            sscanf(rest, "%ld %ld", &a[0], &a[1]);
            printf("%d", panda_sub_enable(subs[a[0]], a[1]));
            break;
        case 'P':
            sscanf(rest, "%ld %ld", &a[0], &a[1]);
            printf("%d", panda_sub_set_priority(subs[a[0]], a[1]));
            break;
        case 'Z':
            sscanf(rest, "%ld %ld", &a[0], &a[1]);
            printf("%d", panda_sub_set_data_size(subs[a[0]], a[1]));
            break;
        case 'G':           /* the owner unloads: its subscriptions go */
            sscanf(rest, "%ld", &a[0]);
            panda_events_owner_gone((void *)(uintptr_t)a[0]);
            for (int i = 0; i < nsubs; i++) {
                if (owners[i] == a[0]) {
                    subs[i] = NULL;
                }
            }
            printf("0");
            break;
        case 'M':           /* mode: analysis frozen kvm replay caps; then re-evaluate? */
            sscanf(rest, "%ld %ld %ld %ld %ld %ld", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);
            env_analysis = a[0];
            env_frozen = a[1];
            env_kvm = a[2];
            replay_mode = a[3];
            env_caps = a[4];
            if (a[5]) {
                panda_events_reevaluate();
            }
            printf("0");
            break;
        case 'X':
            sscanf(rest, "%ld", &a[0]);
            phase = 0;
            panda_event_emit(evs[a[0]], args);
            printf("%d%s", panda_event_wanted(evs[a[0]]), calls->str);
            break;
        case 'O':
            sscanf(rest, "%ld", &a[0]);
            phase = 1;
            serial = ninsts;
            g_assert(ninsts < MAXI);
            insts[ninsts] = panda_event_open(evs[a[0]], args);
            printf("%d%s", insts[ninsts] ? ninsts : -1, calls->str);
            ninsts++;
            break;
        case 'C':           /* close inst ev */
            sscanf(rest, "%ld %ld", &a[0], &a[1]);
            phase = 2;
            serial = a[0];
            panda_event_close(evs[a[1]], insts[a[0]], args);
            insts[a[0]] = NULL;
            printf("0%s", calls->str);
            break;
        case 'Q':
            sscanf(rest, "%ld", &a[0]);
            panda_event_drop(insts[a[0]]);
            insts[a[0]] = NULL;
            printf("0");
            break;
        case 'H':           /* hold ev's dispatch snapshot */
            sscanf(rest, "%ld", &a[0]);
            held = evs[a[0]]->active;
            printf("%d", held ? (int)held->n : 0);
            break;
        case 'W':           /* walk the held snapshot (reading, not calling) */
            k = 0;
            for (unsigned i = 0; held && i < held->n; i++) {
                k += held->s[i].sub->live + (int)held->s[i].data_size;
            }
            printf("%d", k >= 0);
            break;
        case 'R': {
            sscanf(rest, "%ld", &a[0]);
            char *why = NULL;
            bool ok = panda_events_rr_safe(a[0], &why);
            printf("%d", ok);
            if (why) {
                fprintf(stderr, "rr %s\n", why);
            }
            g_free(why);
            break;
        }
        case 'L':           /* a grace period passes: nobody holds a snapshot any more */
            held = NULL;
            rcu_flush();
            printf("0");
            break;
        case 'K':           /* the next emit's first subscriber (id 0) removes sub a[0] */
            sscanf(rest, "%ld", &a[0]);
            victim = subs[a[0]];
            subs[a[0]] = NULL;
            printf("0");
            break;
        case 'J': {
            char *j = panda_events_describe(true), *t = panda_events_describe(false);
            printf("%s", j);
            fprintf(stderr, "%s", t);
            g_free(j);
            g_free(t);
            break;
        }
        case 'V': {         /* uuid ns name sig */
            sscanf(rest, "%ld %63s %127s", &a[0], s1, s2);
            char *h = NULL, *why = NULL;
            panda_event_uuid u;
            if (!panda_event_signature_parse(!strcmp(s2, "-") ? "" : s2, &h, NULL, NULL, &why)) {
                printf("bad");
                g_free(why);
                break;
            }
            panda_event_uuid_derive(&NS[a[0]], s1, h, &u);
            for (int i = 0; i < 16; i++) {
                printf("%02x", u.b[i]);
            }
            g_free(h);
            break;
        }
        case 'Y': {         /* parse sig: hashed full layout, or bad */
            sscanf(rest, "%127[^\n]", s2);
            char *h = NULL, *f = NULL, *why = NULL;
            uint32_t layout = 0;
            if (panda_event_signature_parse(s2, &h, &f, &layout, &why)) {
                printf("%s %s %u", *h ? h : "-", *f ? f : "-", layout);
            } else {
                printf("bad");
                g_free(why);
            }
            g_free(h);
            g_free(f);
            break;
        }
        default:
            fprintf(stderr, "bad op %c\n", op);
            return 2;
        }
        printf("\n");
        if (op != 'J' && op != 'V' && op != 'Y') {
            printf("S");
            print_states();
            printf("\n");
        }
    }
    rcu_flush();
    return 0;
}

/* The field-list macros against the parser's layout (checked at compile time). */
#define T1(F) F(cpu, ptr) F(pc, u64) F(hook, ptr)
#define T2(F) F(a, u8) F(b, u64) F(c, u16)
#define T3(F) F(a, u8)
#define T3X(F) F(b, u32) F(c, i64)
PANDA_EVENT_STRUCT(t1, T1);
PANDA_EVENT_STRUCT(t2, T2);
PANDA_EVENT_STRUCT_EXT(t3, T3, T3X);
const char *layout_sigs[] = { PANDA_EVENT_SIGNATURE(T1), PANDA_EVENT_SIGNATURE(T2),
                              PANDA_EVENT_SIGNATURE_EXT(T3, T3X) };
const unsigned layout_sizes[] = { sizeof(struct t1), sizeof(struct t2), sizeof(struct t3) };
'''

# PandaSubArray is private to events.c: the harness is appended to it, so 'H'/'W' can read it.


def source():
    return EVENTS_C.read_text() + HARNESS


def build(out):
    inc = out / "stubs"
    for name, text in STUBS.items():
        (inc / name).parent.mkdir(parents=True, exist_ok=True)
        (inc / name).write_text(text)
    (out / "events_test.c").write_text(source())
    exe = out / "events_test"
    glib = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0"],
                                               text=True))
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-O1", "-g", "-Wall", "-Wextra",
                    "-Wno-unused-parameter", "-Wno-sign-compare", "-Wno-missing-field-initializers",
                    "-Wmissing-prototypes", "-Wstrict-prototypes", "-Wundef", "-Wvla", "-Werror",
                    "-Wno-error=missing-prototypes",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    "-I", str(inc), "-I", str(SRC / "include"),
                    str(out / "events_test.c"), "-o", str(exe), *glib], check=True)
    return exe


def run_c(exe, cmds):
    text = "".join(c + "\n" for c in cmds)
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:abort_on_error=0",
               UBSAN_OPTIONS="print_stacktrace=1", G_DEBUG="")
    p = subprocess.run([str(exe)], input=text, capture_output=True, text=True, env=env)
    lines = p.stdout.splitlines()
    return lines, p.stderr, p.returncode


# --- the model -------------------------------------------------------------------------------

def parse_sig(sig):
    """(hashed, full, layout) or None: events.h's grammar."""
    parts = sig.split("|")
    if len(parts) > 2:
        return None
    sizes = dict(u8=1, u16=2, u32=4, u64=8, i8=1, i16=2, i32=4, i64=8, ptr=8)
    canon, names, off, align = [], set(), 4, 4
    for p in parts:
        fields = []
        for item in p.split(","):
            item = item.strip()
            if not item:
                continue
            if ":" not in item:
                return None
            n, t = (x.strip() for x in item.split(":", 1))
            if not (n and (n[0].isalpha() or n[0] == "_") and all(c.isalnum() or c == "_" for c in n)):
                return None
            if t not in sizes or n in names:
                return None
            names.add(n)
            fields.append(f"{n}:{t}")
            sz = sizes[t]
            off = -(-off // sz) * sz + sz
            align = max(align, sz)
        canon.append(",".join(fields))
    hashed = canon[0]
    full = hashed + ("|" + canon[1] if len(canon) > 1 and canon[1] else "")
    return hashed, full, -(-off // align) * align


def uuid_of(ns, name, hashed):
    return uuid.uuid5(ns, name + "\0" + hashed)


class Ev:
    def __init__(self, key, idx):
        self.key, self.idx = key, idx
        self.declared, self.owner, self.ever = False, None, False
        self.full = None
        self.modes = self.flags = 0
        self.core = False
        self.subs = []


class Sub:
    def __init__(self, sid, owner, ev, modes, flags, seq):
        self.id, self.owner, self.ev, self.modes, self.flags, self.seq = sid, owner, ev, modes, flags, seq
        self.enabled, self.live, self.prio, self.size = True, True, 0, 0
        self.state = PENDING
        self.pub_size = 0       # data size in the published snapshot


class Model:
    def __init__(self):
        self.evs = {}           # key -> Ev
        self.order = []         # handle index -> Ev
        self.subs = []          # id -> Sub or None (unsubscribed)
        self.seq = 0
        self.env = dict(analysis=1, frozen=1, kvm=0, replay=0, caps=0)
        self.active = {}        # ev idx -> [(sub, size)] snapshot
        self.insts = []
        h, f, _ = parse_sig(CORE[2])
        ev = self.get((0, CORE[1], h))
        ev.full, ev.declared, ev.ever, ev.owner, ev.core = f, True, True, 0, True
        ev.modes, ev.flags = 7, MUTABLE
        self.core_ev = ev       # not yet indexed: the C side indexes on first sight

    def get(self, key):
        if key not in self.evs:
            self.evs[key] = Ev(key, None)
        return self.evs[key]

    def index(self, ev):
        if ev is None:
            return -1
        if ev.idx is None:
            ev.idx = len(self.order)
            self.order.append(ev)
        return ev.idx

    def rival(self, ev):
        for o in self.evs.values():
            if o is not ev and o.declared and o.key[0] == ev.key[0] and o.key[1] == ev.key[1]:
                return o
        return None

    def admit(self, ev, s):
        e = self.env
        if not e["frozen"] or not e["analysis"]:
            return False
        if not s.modes or s.modes & ~ev.modes:
            return False
        if e["kvm"]:
            return bool(s.modes & KVM)
        if e["replay"]:
            if not s.modes & RR:
                return False
            if s.flags & MUTATE and not ev.flags & RR_MUTATE:
                return False
        return True

    def eval_sub(self, s):
        ev = s.ev
        if not s.enabled:
            s.state = DISABLED
        elif ev.declared and ev.core:
            s.state = REFUSED
        elif ev.declared and s.flags & MUTATE and not ev.flags & MUTABLE:
            s.state = REFUSED
        elif ev.declared:
            s.state = ACTIVE if self.admit(ev, s) else REFUSED
        elif self.rival(ev):
            s.state = MISMATCH
        else:
            s.state = PENDING

    def publish(self, ev):
        act = [s for s in ev.subs if s.state == ACTIVE]
        phase = lambda s: 0 if s.flags & ORIGINAL else 1 if s.flags & MUTATE else 2
        act.sort(key=lambda s: (phase(s), s.prio if phase(s) == 1 else 0, s.seq))
        for s in act:
            s.pub_size = s.size
        self.active[id(ev)] = [(s, s.size) for s in act]

    def eval_ev(self, ev):
        for s in ev.subs:
            self.eval_sub(s)
        self.publish(ev)

    def eval_group(self, ev):
        for o in list(self.evs.values()):
            if o is ev or (o.key[0] == ev.key[0] and o.key[1] == ev.key[1]):
                self.eval_ev(o)

    # --- operations: each returns the line the C side prints ---

    def declare(self, owner, ns, name, sig, delta, modes, flags):
        p = parse_sig(sig)
        if p is None or delta or (flags & RR_MUTATE and not flags & MUTABLE):
            return -1
        h, f, _ = p
        ev = self.get((ns, name, h))
        if ev.declared or self.rival(ev):
            return -1
        ev.full, ev.declared, ev.ever, ev.owner = f, True, True, owner
        ev.modes, ev.flags = modes, flags
        self.eval_group(ev)
        return self.index(ev)

    def find(self, ns, name, sig):
        p = parse_sig(sig)
        if p is None:
            return -1
        ev = self.get((ns, name, p[0]))
        if ev.full is None:
            ev.full = p[1]
        return self.index(ev)

    def subscribe(self, owner, evi, modes, flags):
        sid = len(self.subs)
        kinds = flags & (OBSERVE | MUTATE | ORIGINAL)
        if flags != kinds or not kinds or kinds & (kinds - 1):
            self.subs.append(None)
            return -1
        ev = self.order[evi]
        s = Sub(sid, owner, ev, modes, flags, self.seq)
        self.seq += 1
        self.subs.append(s)
        ev.subs.append(s)
        self.eval_ev(ev)
        return sid

    def remove(self, s):
        s.ev.subs.remove(s)
        s.live = False
        self.publish(s.ev)

    def unsubscribe(self, sid):
        s = self.subs[sid]
        self.subs[sid] = None
        if s is None or not s.live:
            return 0
        self.remove(s)
        return 1

    def enable(self, sid, on):
        s = self.subs[sid]
        if not s.live:
            return 0
        s.enabled = bool(on)
        self.eval_ev(s.ev)
        return int(not on or s.state == ACTIVE)

    def priority(self, sid, prio):
        s = self.subs[sid]
        if not s.live or not s.flags & MUTATE:
            return 0
        s.prio = prio
        self.publish(s.ev)
        return 1

    def data_size(self, sid, size):
        s = self.subs[sid]
        if not s.live or (s.ev.declared and not s.ev.flags & INSTANCES):
            return 0
        s.size = size
        self.publish(s.ev)
        return 1

    def gone(self, owner):
        for ev in list(self.evs.values()):
            for s in list(ev.subs):
                if s.owner == owner:
                    self.remove(s)
                    self.subs[s.id] = None
        for ev in list(self.evs.values()):
            if ev.declared and ev.owner == owner and not ev.core:
                ev.declared, ev.owner = False, None
                self.eval_group(ev)
        return 0

    def mode(self, analysis, frozen, kvm, replay, caps, reeval):
        self.env = dict(analysis=analysis, frozen=frozen, kvm=kvm, replay=replay, caps=caps)
        if reeval:
            for ev in list(self.evs.values()):
                self.eval_ev(ev)
        return 0

    def emit(self, evi):
        act = self.active.get(id(self.order[evi]), [])
        return str(int(bool(act))) + "".join(f" {s.id}:-" for s, _ in act if s.live)

    def open(self, evi):
        ev = self.order[evi]
        act = self.active.get(id(ev), [])
        if not act:
            self.insts.append(None)
            return "-1"
        entries = [(s, size) for s, size in act if s.live]
        self.insts.append((ev, entries))
        return str(len(self.insts) - 1) + "".join(f" {s.id}:{'o' if size else '-'}"
                                                 for s, size in entries)

    def close(self, ii):
        inst = self.insts[ii]
        self.insts[ii] = None
        if inst is None:
            return "0"
        _, entries = inst
        return "0" + "".join(f" {s.id}:{'c' if size else '-'}" for s, size in entries
                             if s.live and s.state == ACTIVE)

    def drop(self, ii):
        self.insts[ii] = None
        return "0"

    def rr_safe(self, replay):
        for ev in self.evs.values():
            for s in ev.subs:
                if s.state != ACTIVE:
                    continue
                if not s.modes & RR or (s.flags & MUTATE and not ev.flags & RR_MUTATE):
                    return 0
        return 1

    def states(self):
        return "S" + "".join(" x" if s is None else f" {s.state}" for s in self.subs)


# --- the simulator ---------------------------------------------------------------------------

def gen(rng, n):
    """A random sequence of (command, model call) pairs."""
    m = Model()
    cmds = []
    open_insts = []
    held = False

    def live_subs():
        return [s.id for s in m.subs if s is not None]

    def declared():
        return [e.idx for e in m.order if e.declared]

    def pick_ev():
        d = declared()
        return rng.choice(d) if d and rng.random() < 0.75 else rng.randrange(len(m.order))

    for _ in range(n):
        r = rng.random()
        if r < 0.10:
            owner, ns = rng.randint(1, 4), rng.randint(0, 1)
            name, sig = rng.choice(NAMES), rng.choice(SIGS + ["cpu:ptr,pc:f32", "a:u8|b:u8|c:u8"])
            delta = 0 if rng.random() < 0.9 else rng.choice([-4, 4])
            modes = rng.choice([1, 3, 5, 7, 15, 13])
            flags = rng.choice([0, MUTABLE, MUTABLE | RR_MUTATE, MUTABLE | INSTANCES,
                                INSTANCES, RR_MUTATE, MUTABLE | RR_MUTATE | INSTANCES] +
                               [MUTABLE | INSTANCES] * 3)
            c = f"D {owner} {ns} {name} {sig} {delta} {modes} {flags}"
            exp = m.declare(owner, ns, name, sig, delta, modes, flags)
        elif r < 0.18 or not m.order:
            ns, name, sig = rng.randint(0, 1), rng.choice(NAMES), rng.choice(SIGS)
            c = f"F {ns} {name} {sig}"
            exp = m.find(ns, name, sig)
        elif r < 0.38:
            owner, evi = rng.randint(1, 4), pick_ev()
            modes = rng.choice([1] * 6 + [0, 3, 5, 7, 9, 13, 15, 16])
            flags = rng.choice([OBSERVE] * 4 + [MUTATE] * 4 + [ORIGINAL] * 2 + [OBSERVE | MUTATE, 0])
            c = f"S {owner} {evi} {modes} {flags}"
            exp = m.subscribe(owner, evi, modes, flags)
        elif r < 0.42 and live_subs():
            sid = rng.choice(live_subs())
            c, exp = f"U {sid}", m.unsubscribe(sid)
        elif r < 0.46 and live_subs():
            sid, on = rng.choice(live_subs()), int(rng.random() < 0.6)
            c, exp = f"E {sid} {on}", m.enable(sid, on)
        elif r < 0.50 and live_subs():
            sid, prio = rng.choice(live_subs()), rng.randint(-2, 2)
            c, exp = f"P {sid} {prio}", m.priority(sid, prio)
        elif r < 0.56 and live_subs():
            sid, size = rng.choice(live_subs()), rng.choice([0, 1, 16, 24, 100])
            c, exp = f"Z {sid} {size}", m.data_size(sid, size)
        elif r < 0.575:
            owner = rng.randint(1, 4)
            c, exp = f"G {owner}", m.gone(owner)
        elif r < 0.61:
            normal = rng.random() < 0.5
            env = [1, 1, 0, 0, 0, 1] if normal else [
                rng.random() < 0.9, rng.random() < 0.95, rng.random() < 0.2,
                rng.choice([0, 1, 2]), 0, rng.random() < 0.8]
            env = [int(x) for x in env]
            c, exp = "M " + " ".join(map(str, env)), m.mode(*env)
        elif r < 0.74:
            evi = pick_ev()
            c, exp = f"X {evi}", m.emit(evi)
        elif r < 0.82:
            evi = pick_ev()
            c, exp = f"O {evi}", m.open(evi)
            ii = len(m.insts) - 1
            if m.insts[ii] is not None:
                open_insts.append((ii, evi))
        elif r < 0.89 and open_insts:
            ii, evi = open_insts.pop(rng.randrange(len(open_insts)))
            if rng.random() < 0.8:
                c, exp = f"C {ii} {evi}", m.close(ii)
            else:
                c, exp = f"Q {ii}", m.drop(ii)
        elif r < 0.92 and not held:
            evi = pick_ev()
            act = m.active.get(id(m.order[evi]), [])
            c, exp, held = f"H {evi}", len(act), True
        elif r < 0.95 and held:
            c, exp = "W", 1
        elif r < 0.97:
            c, exp, held = "L", 0, False
        else:
            rp = rng.randint(1, 2)
            c, exp = f"R {rp}", m.rr_safe(rp)
        cmds.append((c, str(exp), m.states()))
    for ii, evi in open_insts:
        cmds.append((f"Q {ii}", "0", m.states()))
    cmds.append(("L", "0", m.states()))
    return cmds


def check(exe, cmds):
    """None if the C side agrees with the model on every line, else the first difference."""
    lines, err, rc = run_c(exe, [c for c, _, _ in cmds])
    for i, (c, exp, st) in enumerate(cmds):
        got = lines[2 * i:2 * i + 2]
        if got != [exp, st]:
            return i, c, (exp, st), got, err[-4000:]
    if rc:
        return len(cmds), "(exit)", None, rc, err[-4000:]
    return None


def shrink(exe, cmds):
    """Drop commands while the failure stays (model expectations are recomputed by replaying)."""
    def replay(sub):
        m = Model()
        out, open_insts = [], {}
        for c, _, _ in sub:
            op, *a = c.split()
            try:
                if op == "D":
                    exp = m.declare(int(a[0]), int(a[1]), a[2], a[3], int(a[4]), int(a[5]), int(a[6]))
                elif op == "F":
                    exp = m.find(int(a[0]), a[1], a[2])
                elif op == "S":
                    if int(a[1]) >= len(m.order):
                        return None
                    exp = m.subscribe(*map(int, a))
                elif op in "UEPZ":
                    sid = int(a[0])
                    if sid >= len(m.subs) or m.subs[sid] is None:
                        return None
                    exp = {"U": lambda: m.unsubscribe(sid), "E": lambda: m.enable(sid, int(a[1])),
                           "P": lambda: m.priority(sid, int(a[1])),
                           "Z": lambda: m.data_size(sid, int(a[1]))}[op]()
                elif op == "G":
                    exp = m.gone(int(a[0]))
                elif op == "M":
                    exp = m.mode(*map(int, a))
                elif op in "XOH":
                    if int(a[0]) >= len(m.order):
                        return None
                    if op == "X":
                        exp = m.emit(int(a[0]))
                    elif op == "O":
                        exp = m.open(int(a[0]))
                    else:
                        exp = len(m.active.get(id(m.order[int(a[0])]), []))
                elif op in "CQ":
                    ii = int(a[0])
                    if ii >= len(m.insts) or m.insts[ii] is None:
                        return None
                    exp = m.close(ii) if op == "C" else m.drop(ii)
                elif op == "W":
                    exp = 1
                elif op == "L":
                    exp = 0
                elif op == "R":
                    exp = m.rr_safe(int(a[0]))
            except (IndexError, KeyError, AttributeError):
                return None
            out.append((c, str(exp), m.states()))
        return out

    cur = cmds
    changed = True
    while changed:
        changed = False
        for i in range(len(cur) - 1, -1, -1):
            cand = replay(cur[:i] + cur[i + 1:])
            if cand and check(exe, cand):
                cur, changed = cand, True
    return cur


# --- directed cases --------------------------------------------------------------------------

def directed(exe):
    fails = []

    def run(cmds):
        lines, err, rc = run_c(exe, cmds)
        if rc:
            fails.append(f"exit {rc}: {cmds}\n{err[-3000:]}")
            lines = lines + [""] * (2 * len(cmds))         # a crash fails the checks, not the test
        return lines, err

    # UUIDv5: the 10-30 vector (NAMESPACE_DNS), and the derivation against Python's.
    lines, _ = run(["V 1 hooks.hit cpu:ptr,pc:u64,hook:ptr"])
    if lines[0] != "13ac12615fea5a2984d28ddc820e619e":
        fails.append(f"10-30 vector: {lines[0]}")
    vec = [(0, "cb.before_block_exec", "cpu:ptr,tb:ptr"), (1, "a.x", "-"),
           (0, "syscall.enter", "cpu:ptr,abi:u32,num:u64|extra:u8"), (1, "x", " a : u8 , b:u64,")]
    lines, _ = run([f"V {ns} {n} {s}" for ns, n, s in vec if " " not in s])
    for (ns, n, s), got in zip([v for v in vec if " " not in v[2]], lines):
        h = parse_sig("" if s == "-" else s)[0]
        if got != uuid_of(NSS[ns], n, h).hex:
            fails.append(f"uuid {n} {s}: {got} != {uuid_of(NSS[ns], n, h).hex}")

    # Parsing: canonical forms, layout, and refusals.
    sigs = ["cpu:ptr,pc:u64", " cpu : ptr , pc:u64 ,", "a:u8,b:u64,c:u16", "a:u8|b:u32,c:i64",
            "a:u8,", "a:u8|", "", "a:u8,a:u16", "a:f32", "a", "1a:u8", "a:u8|b:u8|c:u8",
            "a:u8|a:u8", "x:i8,y:i16,z:i32,w:i64,p:ptr"]
    lines, _ = run([f"Y {s}" if s else "Y " for s in sigs])
    for s, got in zip(sigs, lines):
        p = parse_sig(s)
        exp = "bad" if p is None else f"{p[0] or '-'} {p[1] or '-'} {p[2]}"
        if got != exp:
            fails.append(f"parse {s!r}: {got!r} != {exp!r}")

    # The macros' structs have the size the parser computes (layout_sigs/sizes in the harness).
    src = source()
    for sig, size in [("cpu:ptr,pc:u64,hook:ptr,", 32), ("a:u8,b:u64,c:u16,", 24),
                      ("a:u8,|b:u32,c:i64,", 24)]:
        if parse_sig(sig)[2] != size:
            fails.append(f"layout model {sig}: {parse_sig(sig)[2]} != {size}")
    probe = Path(tempfile.mkdtemp(prefix="events-layout-")) / "p.c"
    probe.write_text('#include <stdio.h>\n#include <stdint.h>\n#include <stddef.h>\n'
                     '#include "panda/events.h"\n'
                     + src[src.index("/* The field-list macros"):]
                     + '\nint main(void){for(int i=0;i<3;i++)printf("%s %u\\n",layout_sigs[i],'
                       'layout_sizes[i]);\nstruct t3 v={.size=offsetof(struct t3,b)};'
                       'printf("%d",PANDA_EVENT_HAS(&v,b));v.size=sizeof v;'
                       'printf("%d\\n",PANDA_EVENT_HAS(&v,c));return 0;}\n')
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-Wall", "-Werror", "-I",
                    str(SRC / "include"), str(probe), "-o", str(probe.with_suffix(""))], check=True)
    out = subprocess.run([str(probe.with_suffix(""))], capture_output=True, text=True,
                         check=True).stdout.splitlines()
    for line in out[:3]:
        sig, size = line.rsplit(" ", 1)
        if parse_sig(sig)[2] != int(size):
            fails.append(f"PANDA_EVENT_STRUCT {sig}: sizeof {size}, parser {parse_sig(sig)[2]}")
    if out[3] != "01":
        fails.append(f"PANDA_EVENT_HAS: {out[3]}")

    # A subscriber removing a later one mid-dispatch: the later one is not called.
    lines, _ = run(["D 1 1 a.x cpu:ptr,pc:u64 0 1 0", "S 2 0 1 1", "S 3 0 1 1", "S 4 0 1 1",
                    "K 2", "X 0", "X 0", "L"])
    if lines[10] != "1 0:- 1:-" or lines[12] != "1 0:- 1:-":
        fails.append(f"remove mid-dispatch: {lines[10]!r} {lines[12]!r}")

    # Reasons: a mismatch names both signatures and the diff; refusals name the rule.
    lines, err = run(["F 1 a.x cpu:ptr,pc:u32", "S 2 0 1 1", "D 1 1 a.x cpu:ptr,pc:u64 0 1 0",
                      "S 3 1 1 2", "S 3 1 1 1", "R 2"])
    reasons = {l.split(" ", 2)[1]: l.split(" ", 2)[2] for l in err.splitlines()
               if l.startswith("reason ")}
    want = {"0": ["publishes cpu:ptr,pc:u64", "built with cpu:ptr,pc:u32", "pc) is u64, was u32"],
            "1": ["does not accept mutators"], "2": ["published by p1"]}
    for sid, words in want.items():
        for w in words:
            if w not in reasons.get(sid, ""):
                fails.append(f"reason of sub {sid}: {reasons.get(sid)!r} lacks {w!r}")
    if lines[-2] != "0" or "does not declare PANDA_MODE_RR" not in err:
        fails.append("rr check")

    # Unload: the publisher's subscribers go PENDING with "unloaded"; its own subscriptions go.
    lines, err = run(["D 1 1 a.x cpu:ptr,pc:u64 0 1 0", "S 2 0 1 1", "S 1 0 1 1", "G 1", "L"])
    if lines[-1] != "S 1 x":
        fails.append(f"unload states: {lines[-1]}")
    if "its publisher p1 was unloaded" not in err:
        fails.append("unload reason")

    # The listing parses, and shows the core list's callbacks and the registry's subscriptions.
    lines, _ = run(["F 0 cb.alpha cpu:ptr,pc:u64", "S 2 0 1 1", "D 1 1 a.y a:u8 0 1 0", "J"])
    try:
        doc = json.loads(lines[-1])
        core = next(e for e in doc["events"] if e["name"] == "cb.alpha")
        if not core["core"] or [s["owner"] for s in core["subscriptions"]] != ["p9", "p2"]:
            fails.append(f"listing core: {core}")
        if core["subscriptions"][1]["state"] != "refused":
            fails.append("a registry subscription to a core event is refused")
        if not any(e["name"] == "a.y" and e["publisher"] == "p1" for e in doc["events"]):
            fails.append("listing a.y")
        if any(e["name"] == "cb.beta" for e in doc["events"]):
            fails.append("a type with no modes is declared")
    except (ValueError, StopIteration) as e:
        fails.append(f"listing: {e}: {lines[-1][:300]}")
    return fails


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seqs", type=int, default=int(os.environ.get("EVENTS_MODEL_SEQS", 3000)))
    ap.add_argument("--len", type=int, default=120)
    ap.add_argument("--seed", type=int, default=None)
    args = ap.parse_args()
    seed = args.seed if args.seed is not None else random.randrange(1 << 30)
    with tempfile.TemporaryDirectory(prefix="events-registry-") as tmp:
        exe = build(Path(tmp))
        fails = directed(exe)
        for f in fails:
            print("FAIL", f)
        rng = random.Random(seed)
        for n in range(args.seqs):
            cmds = gen(rng, rng.randint(5, args.len))
            bad = check(exe, cmds)
            if bad:
                small = shrink(exe, cmds)
                bad = check(exe, small) or bad
                print(f"FAIL seed {seed} sequence {n}: at {bad[0]} {bad[1]!r}")
                print(f"  model: {bad[2]}\n  C:     {bad[3]}")
                print("  sequence:\n    " + "\n    ".join(c for c, _, _ in small))
                print(bad[4])
                return 1
        print(f"events registry: {len(fails)} directed failures; {args.seqs} sequences agree "
              f"(seed {seed})")
        return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
