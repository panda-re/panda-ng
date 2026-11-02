#!/usr/bin/env python3
"""The syscall trap's consumers and slot state machine, host-only: the real code against a model.

No engine and no guest. The slot table, the consumer list and the enter and return paths of
engine/src/system/syscall-trap.c (with forget_task, cancel_all and the watches) are compiled
against stubs, with ASan and UBSan, into a program that reads target calls from stdin: enter and
return, as the target makes them, each with what every consumer does if it gets the event (set
its task and data, change the arguments or the return, skip, keep, ask for a fault or an injected
call, set want_return). A Python model written from syscall-trap.h's contract runs the same
calls. After every call the two must agree on: the events delivered, in order (every field each
consumer sees), the target's result (flags, return value, arguments, rewind registers), the slot
counts (pending, held, full, stale), the data released, and the data still live.

The model also checks the contract itself: a consumer's data only ever comes back to that
consumer with that call, and everything a consumer attaches is released exactly once (ASan
catches a double release on the C side; every sequence ends with cancel_all and nothing live).

The calls come from a guest simulator: tasks (64-bit, i386, AArch64, AArch32 A32 and T32) on four
vCPUs that trap, return, restart, take signals, fault and resume, run injected calls, vfork, die
(with and without forget_task) and execve, while one to six consumers (modifiers and observers)
are added and removed, between global events (cancel_all, watch changes, record/replay mode) and
random calls. A failing sequence is shrunk to a short one and printed. Then directed cases (the
paths one at a time, with the events they must give) and the table's capacity.

    python3 tests/static/syscall_consumers.py [--seqs N] [--seed S]
    (nix build .#checks.x86_64-linux.syscall-consumers)

Needs a C compiler with ASan and UBSan, pkg-config and GLib. SYSCALL_MODEL_SEQS sets the number of
random sequences (default 10000).
"""
import argparse
import os
import random
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRAP = ROOT / "engine/src/system/syscall-trap.c"
HEADER = ROOT / "engine/src/include/system/syscall-trap.h"

M64 = (1 << 64) - 1
NCPU = 4
SLOTS = 4096
NUMS = 8192
SENT = 0x5E5E5E5E                       # the target's *ret before an enter
IS32 = {1, 3, 4, 7, 9}                  # i386, arm, arm_oabi, ppc, riscv32
ENTER, RETURN = 1, 2                    # watch flags
F_ARGS, F_SKIP, F_RET, F_FAULT, F_INJECT, F_REWIND, F_RESTORE = 1, 2, 4, 8, 16, 32, 64
PENDING, HELD_ENTER, HELD_RETURN, INJECT, INJECT_RET = range(5)
MAXC = 6                                # consumer ids 1..MAXC
FULL, ENTER_ONLY = 1, 2                 # a consumer's ops: enter, ret and drop; or no ret
OBSERVER, SKIPPED = 1, 2                # add_consumer flags
X32 = 0x40000000

# A consumer's decision, applied when it gets an event:
# task, data, want (0 leave, 1 false, 2 true), argmask, argval, skip, retset, retval, keep,
# fault, fault_addr, fault_write, inject, inject_num
NODEC = (0,) * 14

RESULT = (["r", "ret"] + [f"arg{i}" for i in range(6)] + ["rw.abi", "rw.num"] +
          [f"rw.arg{i}" for i in range(6)] + ["rw.len", "pending", "held", "full", "stale", "seq",
                                              "fault_addr", "fault_write", "trap_on", "live",
                                              "events"])
EVENT = (["cons", "kind", "kmmu", "abi", "num"] + [f"args{i}" for i in range(6)] +
         ["pc", "ret", "percpu", "skip", "skipped", "want_return", "restart", "keep", "resumed",
          "fault", "fault_write", "fault_addr", "inject", "inject_num", "inject_args0",
          "injected", "inject_ret", "task", "data"])


def u64(v):
    return v & M64


def signed(v):
    v &= M64
    return v - (1 << 64) if v >> 63 else v


def mask(abi, v):
    return v & 0xFFFFFFFF if abi in IS32 else v & M64


def sext(abi, v):
    if abi in IS32:
        v &= 0xFFFFFFFF
        return u64(v - (1 << 32)) if v & 0x80000000 else v
    return v & M64


def forks(abi, num):
    """fork, vfork, clone and clone3."""
    if abi == 0:
        num &= ~X32
    return num in {0: {56, 57, 58, 435}, 1: {2, 120, 190, 435}, 2: {220, 435},
                   3: {2, 120, 190, 435}, 4: {2, 120, 190, 435}}.get(abi, ())


# --- the C side ------------------------------------------------------------------------------

STUBS = r'''
#include <glib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define unlikely(x) __builtin_expect(!!(x), 0)
#define qatomic_read(p) (*(p))
#define qatomic_set(p, v) (*(p) = (v))
#define qatomic_inc(p) (++*(p))
#define qatomic_dec(p) (--*(p))
#define qatomic_rcu_read(p) (*(p))
#define qatomic_rcu_set(p, v) (*(p) = (v))
#define RCU_READ_LOCK_GUARD()
struct rcu_head { int unused; };
#define g_free_rcu(p, field) g_free(p)
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
typedef GMutex QemuMutex;
#define qemu_mutex_init g_mutex_init
#define qemu_mutex_lock g_mutex_lock
#define qemu_mutex_unlock g_mutex_unlock
static inline void lock_guard_end(GMutex **m) { g_mutex_unlock(*m); }
#define QEMU_LOCK_GUARD(m) \
    GMutex *lock_guard_ __attribute__((cleanup(lock_guard_end))) = (g_mutex_lock(m), (m))
typedef uint64_t vaddr;
typedef struct CPUState {
    uint64_t syscall_current;
    uint32_t syscall_kmmu, syscall_inject_abis;
} CPUState;
#define NCPU 4
static CPUState cpus[NCPU];
#define CPU_FOREACH(c) for ((c) = cpus; (c) < cpus + NCPU; (c)++)
static bool write_allowed = true;
static bool replay_analysis_write_allowed(void) { return write_allowed; }
'''

HARNESS = r'''
#define MAXC 6
#define EVLEN 30
typedef struct { uint64_t id; } Data;
typedef struct {
    uint64_t task, data, want, argmask, argval, skip, retset, retval, keep, fault, faddr, fwrite,
             inject, inum;
} Dec;
static Dec dec[MAXC + 1];
static SyscallTrapConsumer *cons[MAXC + 1];
static int cid[MAXC + 1] = { 0, 1, 2, 3, 4, 5, 6 };
static uint64_t ev[64][EVLEN];
static int ndeliv;
static long live;
static GArray *dropped;

static void record(CPUState *cpu, SyscallTrap *sc, int id, int kind)
{
    uint64_t *e;
    int n = 0;

    if (ndeliv == 64) {
        abort();
    }
    e = ev[ndeliv++];
#define P(x) e[n++] = (uint64_t)(x)
    P(id); P(kind); P(cpu->syscall_kmmu); P(sc->abi); P(sc->num);
    for (int i = 0; i < 6; i++) {
        P(sc->args[i]);
    }
    P(sc->pc); P(sc->ret); P(sc->percpu); P(sc->skip); P(sc->skipped); P(sc->want_return);
    P(sc->restart); P(sc->keep); P(sc->resumed); P(sc->fault); P(sc->fault_write);
    P(sc->fault_addr); P(sc->inject); P(sc->inject_num); P(sc->inject_args[0]); P(sc->injected);
    P(sc->inject_ret); P(sc->task); P(sc->data ? ((Data *)sc->data)->id : 0);
#undef P
}

static void apply(SyscallTrap *sc, const Dec *d)
{
    if (d->task) {
        sc->task = d->task;
    }
    if (d->data && !sc->data) {
        Data *x = malloc(sizeof(*x));
        x->id = d->data;
        sc->data = x;
        live++;
    }
    if (d->want) {
        sc->want_return = d->want == 2;
    }
    for (int i = 0; i < 6; i++) {
        if (d->argmask >> i & 1) {
            sc->args[i] = d->argval + i;
        }
    }
    if (d->skip) {
        sc->skip = true;
        sc->ret = d->retval;
    }
    if (d->retset) {
        sc->ret = d->retval;
    }
    if (d->keep) {
        sc->keep = true;
    }
    if (d->fault) {
        sc->fault = true;
        sc->fault_addr = d->faddr;
        sc->fault_write = d->fwrite;
    }
    if (d->inject) {
        sc->inject = true;
        sc->inject_num = d->inum;
        for (int i = 0; i < 6; i++) {
            sc->inject_args[i] = d->inum * 16 + i;
        }
    }
}

static void c_enter(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    int id = *(int *)opaque;

    record(cpu, sc, id, 1);
    apply(sc, &dec[id]);
}

static void c_ret(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    int id = *(int *)opaque;

    record(cpu, sc, id, 2);
    apply(sc, &dec[id]);
}

static void c_drop(SyscallTrap *sc, void *opaque)
{
    Data *d = sc->data;

    g_array_append_val(dropped, d->id);
    free(d);
    live--;
}

static const SyscallTrapOps ops[3] = {
    [1] = { c_enter, c_ret, c_drop },
    [2] = { c_enter, NULL, c_drop },
};

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}

int main(void)
{
    static char line[16384];

    setvbuf(stdout, NULL, _IOLBF, 0);
    dropped = g_array_new(false, false, sizeof(uint64_t));
    while (fgets(line, sizeof(line), stdin)) {
        uint64_t v[128] = { 0 }, r = 0, ret = SENT, args[6] = { 0 }, held = 0, pending, full,
                 stale;
        SyscallTrapRewind rw = { 0 };
        char *p = line + 1;
        int n = 0;

        while (n < 128) {
            char *e;
            uint64_t x = strtoull(p, &e, 10);
            if (e == p) {
                break;
            }
            v[n++] = x;
            p = e;
        }
        ndeliv = 0;
        g_array_set_size(dropped, 0);
        switch (line[0]) {
        case 'E': {
            CPUState *cpu = &cpus[v[0]];
            cpu->syscall_inject_abis = v[1];
            memcpy(args, &v[4], sizeof(args));
            memcpy(&dec[1], &v[16], sizeof(Dec) * MAXC);
            r = syscall_trap_enter(cpu, v[2], v[3], args, v[10], v[11], v[12], v[13], v[14],
                                   v[15], &ret);
            break;
        }
        case 'X':
            ret = v[6];
            memcpy(&dec[1], &v[7], sizeof(Dec) * MAXC);
            r = syscall_trap_return(&cpus[v[0]], v[1], v[2], v[3], v[4], v[5], &ret, &rw);
            break;
        case 'F':
            r = syscall_trap_forget_task(v[0]);
            break;
        case 'A':
            r = syscall_trap_cancel_all();
            break;
        case 'C':
            if (v[0]) {
                r = !cons[v[1]];
                if (r) {
                    cons[v[1]] = syscall_trap_add_consumer(&ops[v[2]], &cid[v[1]], v[3]);
                }
            } else {
                r = cons[v[1]] && syscall_trap_remove_consumer(cons[v[1]]);
                if (r) {
                    cons[v[1]] = NULL;
                }
            }
            break;
        case 'W':
            r = syscall_trap_watch(cons[v[0]], v[1], (int64_t)v[2], v[3]);
            break;
        case 'U':
            if (cons[v[0]]) {
                syscall_trap_unwatch_all(cons[v[0]]);
            }
            break;
        case 'M':
            write_allowed = v[0];
            break;
        case 'R':
            syscall_trap_cancel_all();
            for (int i = 1; i <= MAXC; i++) {
                if (cons[i] && !syscall_trap_remove_consumer(cons[i])) {
                    fprintf(stderr, "reset: consumer %d not removed\n", i);
                    return 4;
                }
                cons[i] = NULL;
            }
            syscall_trap_on = false;
            sc_full = sc_stale = sc_seq = 0;
            write_allowed = true;
            memset(cpus, 0, sizeof(cpus));
            break;
        default:
            fprintf(stderr, "bad command: %s", line);
            return 2;
        }
        for (unsigned i = 0; i < SC_SLOTS; i++) {
            held += sc_slots[i].pc && sc_slots[i].state != SC_PENDING;
        }
        if (held != sc_held) {
            fprintf(stderr, "sc_held %u, but %" PRIu64 " slots are held\n", sc_held, held);
            return 3;
        }
        syscall_trap_stats(&pending, &full, &stale);
        printf("%" PRIu64 " %" PRIu64, r, ret);
        for (int i = 0; i < 6; i++) {
            printf(" %" PRIu64, args[i]);
        }
        printf(" %" PRIu64 " %" PRIu64, (uint64_t)rw.abi, (uint64_t)rw.num);
        for (int i = 0; i < 6; i++) {
            printf(" %" PRIu64, rw.args[i]);
        }
        printf(" %u %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64
               " %d %d %ld %d", rw.len, pending, held, full, stale, sc_seq, sc_fault_addr,
               sc_fault_write, syscall_trap_on, live, ndeliv);
        for (int i = 0; i < ndeliv; i++) {
            for (int j = 0; j < EVLEN; j++) {
                printf(" %" PRIu64, ev[i][j]);
            }
        }
        if (dropped->len) {
            qsort(dropped->data, dropped->len, sizeof(uint64_t), cmp_u64);
        }
        printf(" %u", dropped->len);
        for (unsigned i = 0; i < dropped->len; i++) {
            printf(" %" PRIu64, g_array_index(dropped, uint64_t, i));
        }
        printf("\n");
    }
    return 0;
}
'''


def source():
    """The test program: stubs, the header's types, the trap's code, the harness."""
    h = HEADER.read_text()
    c = TRAP.read_text()
    header = h[h.index("/* the same values"):h.index("#endif /* SYSTEM_SYSCALL_TRAP_H */")]
    core = c[c.index("#define SC_SLOTS"):c.index("\nvoid syscall_trap_fault(")]
    admin = c[c.index("\nunsigned syscall_trap_forget_task("):c.index("\nbool syscall_trap_read(")]
    return STUBS + f"#define SENT {SENT}\n" + header + core + admin + HARNESS


def build(out):
    exe = out / "trap"
    (out / "trap.c").write_text(source())
    glib = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0"],
                                               text=True))
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-O1", "-g", "-Wall",
                    "-Wno-unused-function", "-Wno-unused-variable", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", str(out / "trap.c"), "-o", str(exe), *glib],
                   check=True)
    return exe


def run_c(exe, cmds):
    """The C side's result for each command, as tuples; fewer if it stopped (with stderr)."""
    text = "".join(" ".join([c[0]] + [str(u64(x)) for x in c[1:]]) + "\n" for c in cmds)
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:abort_on_error=0",
               UBSAN_OPTIONS="print_stacktrace=1")
    p = subprocess.run([str(exe)], input=text, capture_output=True, text=True, env=env)
    outs = [tuple(int(x) for x in line.split()) for line in p.stdout.splitlines()]
    return outs, (p.stderr if p.returncode else "")


# --- the model -------------------------------------------------------------------------------

def new_sc(**kw):
    """What a call's consumers share."""
    sc = dict(abi=0, num=0, args=(0,) * 6, pc=0, ret=0, percpu=0, skip=False, skipped=False,
              restart=False, fault=False, fault_write=False, fault_addr=0, inject=False,
              inject_num=0, inject_args=(0,) * 6, injected=False, inject_ret=0)
    sc.update(kw)
    return sc


class ModelError(AssertionError):
    pass


class Model:
    """syscall-trap.h's contract."""

    def __init__(self):
        self.live = {}                  # data id -> (call id, consumer id) it was attached to
        self.calls = 0
        self.reset()

    def reset(self):
        if self.live:
            raise ModelError(f"reset with data live: {sorted(self.live)}")
        self.slots = {}                 # (k1, k2, pc) -> slot
        self.cons = {}                  # id -> consumer
        self.order = 0
        self.trap_on = False
        self.full = self.stale = self.seq = 0
        self.fault_addr, self.fault_write = 0, False
        self.write_allowed = True
        self.inj = [0] * NCPU

    # the consumers
    def listed(self):
        return sorted(self.cons.items(), key=lambda kv: kv[1]["order"])

    def watches(self, c, abi, num):
        w = c["watch_all"][abi]
        if 0 <= signed(num) < NUMS:
            w |= c["watch"].get((abi, signed(num)), 0)
        return w

    # the slot table
    def drop_part(self, p):
        if p["data"]:
            if p["data"] not in self.live:
                raise ModelError(f"data {p['data']} released twice")
            del self.live[p["data"]]
            self.dropped.append(p["data"])
        p["data"] = 0

    def drop(self, call):
        for p in call["parts"]:
            self.drop_part(p)
        call["parts"] = []

    def evict(self):
        keep = {}
        for k, s in self.slots.items():
            if s["seq"] + SLOTS // 2 >= self.seq or s["state"] in (INJECT, INJECT_RET):
                keep[k] = s
            else:
                self.drop(s["call"])
                self.stale += 1
        self.slots = keep

    def put(self, key, ln, call, state):
        """The slot the call went in, or None (dropped: the table is full)."""
        if len(self.slots) >= SLOTS * 3 // 4:
            self.evict()
        old = self.slots.get(key)
        if old:
            self.stale += 1                 # its return was lost
            self.drop(old["call"])
        elif len(self.slots) >= SLOTS:
            self.full += 1
            self.drop(call)
            return None
        self.seq += 1
        s = self.slots[key] = dict(len=ln, state=state, seq=self.seq, call=call,
                                   num=old["num"] if old else 0,
                                   args=old["args"] if old else (0,) * 6)
        return s

    def hold(self, key, ln, call, state):
        sc = call["sc"]
        self.fault_addr, self.fault_write = sc["fault_addr"], sc["fault_write"]
        sc["fault"] = False
        self.put(key, ln, call, state)
        return F_FAULT

    def pend(self, key, ln, call, kept):
        """The call waits for the return of the parts that ask for it; the others are done."""
        parts = []
        for p in call["parts"]:
            if p["keep"] if kept else p["want"]:
                p.update(want=True, keep=False)
                parts.append(p)
            else:
                self.drop_part(p)
        call["parts"] = parts
        if parts:
            self.put(key, ln, call, PENDING)

    def held(self):
        return sum(s["state"] != PENDING for s in self.slots.values())

    def holder(self, call):
        return next(p for p in call["parts"] if p["order"] == call["holder"])

    # a consumer gets an event
    def view(self, call, p, resumed, kind, kmmu):
        sc = call["sc"]
        if p["data"] and self.live.get(p["data"]) != (call["id"], p["c"]):
            raise ModelError(f"data {p['data']} came back with another call or consumer")
        v = dict(sc, task=p["task"], data=p["data"], want_return=p["want"], keep=False,
                 fault=False, inject=False, resumed=resumed)
        self.events.append((p["c"], kind, kmmu, v["abi"], v["num"], *v["args"], v["pc"],
                            v["ret"], v["percpu"], v["skip"], v["skipped"], v["want_return"],
                            v["restart"], v["keep"], v["resumed"], v["fault"],
                            v["fault_write"], v["fault_addr"], v["inject"], v["inject_num"],
                            v["inject_args"][0], v["injected"], v["inject_ret"], v["task"],
                            v["data"]))
        return v

    def apply(self, call, p, v, dec):
        (task, data, want, argmask, argval, skip, retset, retval, keep, fault, faddr, fwrite,
         inject, inum) = dec
        if task:
            v["task"] = task
        if data and not v["data"]:
            v["data"] = data
            self.live[data] = (call["id"], p["c"])
        if want:
            v["want_return"] = want == 2
        v["args"] = tuple(u64(argval + i) if argmask >> i & 1 else a
                          for i, a in enumerate(v["args"]))
        if skip:
            v["skip"], v["ret"] = True, u64(retval)
        if retset:
            v["ret"] = u64(retval)
        if keep:
            v["keep"] = True
        if fault:
            v["fault"], v["fault_addr"], v["fault_write"] = True, u64(faddr), bool(fwrite)
        if inject:
            v["inject"], v["inject_num"] = True, u64(inum)
            v["inject_args"] = tuple(u64(inum * 16 + i) for i in range(6))
        p["task"], p["data"] = v["task"], v["data"]

    def deliver(self, kind, kmmu, call, p, resumed, decs):
        """True if a modifier asked to hold the call."""
        c = self.cons[p["c"]]
        v = self.view(call, p, resumed, kind, kmmu)
        self.apply(call, p, v, decs[p["c"] - 1])
        if kind == 2:
            p["keep"] = v["keep"]
        else:
            p["want"] = v["want_return"] and c["kind"] == FULL
        if c["flags"] & OBSERVER or not self.write_allowed:
            return False
        sc = call["sc"]
        if v["fault"] or v["inject"]:
            for f in ("fault", "fault_write", "fault_addr", "inject", "inject_num",
                      "inject_args"):
                sc[f] = v[f]
            call["holder"] = c["order"]
            return True
        sc["ret"] = v["ret"]
        if kind == 1:
            sc["args"], sc["skip"] = v["args"], v["skip"]
        return False

    def enter_run(self, call, after, resumed, kmmu, decs):
        sc = call["sc"]
        if resumed:
            p = next((p for p in call["parts"] if p["order"] == after), None)
            held = p is not None and self.deliver(1, kmmu, call, p, True, decs)
            sc.update(injected=False, inject_ret=0)
            if held:
                return True
        for i, c in self.listed():
            if c["order"] <= after:
                continue
            w = self.watches(c, sc["abi"], sc["num"])
            if not w:
                continue
            p = dict(c=i, order=c["order"], task=0, data=0, keep=False,
                     want=bool(w & RETURN) and c["kind"] == FULL)
            call["parts"].append(p)
            if w & ENTER and self.deliver(1, kmmu, call, p, False, decs):
                return True
        return False

    def ret_run(self, call, resumed, kmmu, decs):
        for p in call["parts"]:
            again = resumed and p["order"] == call["holder"]
            if not p["want"]:
                continue
            held = self.deliver(2, kmmu, call, p, again, decs)
            if again:
                call["sc"].update(injected=False, inject_ret=0)
            if held:
                return True
            p["want"] = False
        return False

    def skipped(self, call, kmmu, decs):
        call["sc"]["skipped"] = True
        for p in call["parts"]:
            c = self.cons[p["c"]]
            if p["want"] and c["kind"] == FULL and c["flags"] & SKIPPED:
                v = self.view(call, p, False, 2, kmmu)
                self.apply(call, p, v, decs[p["c"] - 1])

    def refuse(self, call):
        """The holder's injected call can't run: its part ends, the others go on."""
        p = self.holder(call)
        self.drop_part(p)
        p["want"] = False

    # the target's calls
    def enter(self, cpu, inj, abi, num, args, pc, ln, k1, k2, percpu, kmmu, decs):
        self.inj[cpu] = inj
        args = tuple(u64(a) for a in args)
        if not self.cons:
            return 0, SENT, args
        key = (k1, k2, pc)
        after, resumed = 0, False
        if self.held():
            s = self.slots.get(key)
            if s and s["state"] == HELD_ENTER:
                call = self.slots.pop(key)["call"]
                sc = call["sc"]
                if sc["abi"] == abi and sc["num"] == num and call["regs"] == args:
                    # back after the fault, or the injected call: the registers are as they were
                    sc["percpu"] = percpu
                    after, resumed = call["holder"], True
                else:
                    self.drop(call)         # another call at the same address
            elif s and s["state"] in (INJECT, INJECT_RET):
                return 0, SENT, args        # the kernel restarts the injected call
        if not resumed:
            if not any(self.watches(c, abi, num) for c in self.cons.values()):
                return 0, SENT, args
            self.calls += 1
            call = dict(id=self.calls, sc=new_sc(abi=abi, num=num, pc=pc, percpu=percpu,
                                                 args=args),
                        regs=args, holder=0, parts=[])
        sc = call["sc"]
        while self.enter_run(call, after, resumed, kmmu, decs):
            if sc["fault"]:
                return self.hold(key, ln, call, HELD_ENTER), SENT, args
            inum, iargs = sc["inject_num"], sc["inject_args"]
            sc.update(inject=False, injected=False)
            if inj >> abi & 1:
                s = self.put(key, ln, call, INJECT)
                if not s:
                    return 0, SENT, args
                s["num"], s["args"] = num, args
                return F_INJECT, inum, iargs
            self.refuse(call)
            after, resumed = call["holder"], False
        r = 0
        if sc["args"] != args:
            args, r = sc["args"], r | F_ARGS
        if sc["skip"]:
            ret = mask(abi, sc["ret"])
            self.skipped(call, kmmu, decs)
            self.drop(call)
            return r | F_SKIP, ret, args
        self.pend(key, ln, call, False)
        return r, SENT, args

    def restore(self, abi, onum, oargs, v):
        return F_RESTORE | F_RET, mask(abi, v), (abi, onum, *oargs, 0)

    def ret(self, cpu, pc, k1, k2, percpu, kmmu, ret, decs):
        none = (0,) * 9
        key = (k1, k2, pc)
        s = self.slots.get(key)
        if s and s["state"] in (INJECT, INJECT_RET) and not self.write_allowed:
            # host state from before a record/replay transition: never rewind the task
            self.drop(self.slots.pop(key)["call"])
            return 0, ret, none
        if s and s["state"] == INJECT:
            # the injected call is back: the task goes back to the trap, its own call held there
            sc = s["call"]["sc"]
            sc.update(injected=True, inject_ret=sext(sc["abi"], ret))
            s["state"] = HELD_ENTER
            return F_REWIND, ret, (sc["abi"], s["num"], *s["args"], s["len"])
        restart = restore = False
        if s and s["state"] == INJECT_RET:
            sc = s["call"]["sc"]
            sc.update(injected=True, inject_ret=sext(sc["abi"], ret))
            restore = True
        elif not s or s["state"] == HELD_ENTER:
            # the kernel put the task back at the trap: a restart, or a fault taken at the trap
            s = None
            for ln in (2, 4):
                k = (k1, k2, u64(pc + ln))
                t = self.slots.get(k)
                if t and t["len"] == ln and t["state"] in (PENDING, HELD_RETURN):
                    s, key = t, k
                    break
            if not s:
                return 0, ret, none
            restart = True
        elif (s["state"] == PENDING and forks(s["call"]["sc"]["abi"], s["call"]["sc"]["num"]) and
              sext(s["call"]["sc"]["abi"], ret) == 0):
            return 0, ret, none             # a vfork child, on its parent's stack
        resumed = s["state"] != PENDING
        onum, oargs, ln = s["num"], s["args"], s["len"]
        call = self.slots.pop(key)["call"]
        sc = call["sc"]
        abi = sc["abi"]
        v = sext(abi, ret)
        if not restore:
            onum, oargs = sc["num"], sc["args"]
        if not resumed:
            sc.update(ret=v, restart=restart)
        restart = sc["restart"]
        sc.update(percpu=percpu, skipped=False)
        r, rw = 0, none
        while self.ret_run(call, resumed, kmmu, decs):
            resumed = False
            if sc["inject"]:
                inum, iargs = sc["inject_num"], sc["inject_args"]
                sc.update(inject=False, injected=False)
                if not sc["restart"] and self.inj[cpu] >> abi & 1:
                    s = self.put(key, ln, call, INJECT_RET)
                    if s:
                        s["num"], s["args"] = onum, oargs
                        return F_REWIND, ret, (abi, inum, *iargs, ln)
                    return self.restore(abi, onum, oargs, sc["ret"]) if restore else (0, ret, none)
                self.refuse(call)
                continue
            r = self.hold(key, ln, call, HELD_RETURN)
            if restore:
                rr, ret, rw = self.restore(abi, onum, oargs, sc["ret"])
                r |= rr
            return r, ret, rw
        if restore:
            r, ret, rw = self.restore(abi, onum, oargs, sc["ret"])
        elif not restart and sc["ret"] != v:
            r, ret = F_RET, mask(abi, sc["ret"])
        if restart or restore:
            for p in call["parts"]:
                p["keep"] = False
        sc.update(ret=0, restart=False)
        self.pend(key, ln, call, True)
        return r, ret, rw

    def forget(self, task):
        n = 0
        if task and self.slots:
            for k in [k for k, s in self.slots.items()
                      if any(p["task"] == task for p in s["call"]["parts"])]:
                self.drop(self.slots.pop(k)["call"])
                n += 1
        return n

    def cancel_all(self):
        n = len(self.slots)
        for s in self.slots.values():
            self.drop(s["call"])
        self.slots = {}
        self.inj = [0] * NCPU
        self.fault_addr, self.fault_write = 0, False
        return n

    def add(self, i, kind, flags):
        if i in self.cons:
            return 0
        self.order += 1
        self.cons[i] = dict(kind=kind, flags=flags, watch={}, watch_all=[0] * 11,
                            order=self.order | (1 << 63 if flags & OBSERVER else 0))
        return 1

    def remove(self, i):
        c = self.cons.get(i)
        if not c or any(s["state"] in (INJECT, INJECT_RET) and s["call"]["holder"] == c["order"]
                        for s in self.slots.values()):
            return 0
        for k in list(self.slots):
            s = self.slots[k]
            for p in [p for p in s["call"]["parts"] if p["c"] == i]:
                self.drop_part(p)
                s["call"]["parts"].remove(p)
            if not s["call"]["parts"] and s["state"] == PENDING:
                del self.slots[k]
        del self.cons[i]
        if not self.cons:
            self.trap_on = False
            self.cancel_all()
        return 1

    def run(self, cmd):
        """One command: the result tuple the C side prints."""
        op, v = cmd[0], [u64(x) for x in cmd[1:]]
        self.events, self.dropped = [], []
        r, ret, args, rw = 0, SENT, (0,) * 6, (0,) * 9
        if op == "E":
            decs = [tuple(v[16 + 14 * i:30 + 14 * i]) for i in range(MAXC)]
            r, ret, args = self.enter(v[0], v[1], v[2], v[3], v[4:10], v[10], v[11], v[12],
                                      v[13], v[14], v[15], decs)
        elif op == "X":
            decs = [tuple(v[7 + 14 * i:21 + 14 * i]) for i in range(MAXC)]
            r, ret, rw = self.ret(v[0], v[1], v[2], v[3], v[4], v[5], v[6], decs)
        elif op == "F":
            r = self.forget(v[0])
        elif op == "A":
            r = self.cancel_all()
        elif op == "C":
            r = self.add(v[1], v[2], v[3]) if v[0] else self.remove(v[1])
        elif op == "W":
            c, abi, num, flags = self.cons.get(v[0]), v[1], signed(v[2]), v[3] & 3
            r = int(bool(c) and abi < 11 and -1 <= num < NUMS)
            if r:
                if num < 0:
                    c["watch_all"][abi] = flags
                else:
                    c["watch"][(abi, num)] = flags
                self.trap_on |= bool(flags)
        elif op == "U":
            if v[0] in self.cons:
                self.cons[v[0]].update(watch={}, watch_all=[0] * 11)
        elif op == "M":
            self.write_allowed = bool(v[0])
        elif op == "R":
            self.cancel_all()
            self.reset()
        return (u64(r), u64(ret), *args, *rw, len(self.slots), self.held(), self.full,
                self.stale, self.seq, self.fault_addr, int(self.fault_write), int(self.trap_on),
                len(self.live), len(self.events), *[u64(x) for e in self.events for x in e],
                len(self.dropped), *sorted(self.dropped))


def describe(cmd, want, got):
    """The fields that differ, by name."""
    lines = []
    n = len(RESULT)
    for i in range(max(len(want), len(got))):
        a = want[i] if i < len(want) else None
        b = got[i] if i < len(got) else None
        if a != b:
            if i < n:
                name = RESULT[i]
            elif i < n + len(EVENT) * min(want[n - 1], got[n - 1]):
                name = f"event {(i - n) // len(EVENT)}: {EVENT[(i - n) % len(EVENT)]}"
            else:
                name = f"[{i}]"
            lines.append(f"    {name}: model {a}, trap {b}")
    return lines


def fmt_decs(decs):
    return "".join(f" c{i + 1}={tuple(signed(x) if signed(x) < 0 else x for x in d)}"
                   for i, d in enumerate(decs) if any(d))


def fmt(cmd):
    op, v = cmd[0], [signed(x) if signed(x) < 0 else x for x in cmd[1:]]
    if op == "E":
        decs = [cmd[17 + 14 * i:31 + 14 * i] for i in range(MAXC)]
        return (f"E cpu{v[0]} abi{v[2]} num={v[3]} args={v[4:10]} pc={v[10]:#x}/{v[11]} "
                f"key=({v[12]:#x},{v[13]:#x})" + fmt_decs(decs))
    if op == "X":
        decs = [cmd[8 + 14 * i:22 + 14 * i] for i in range(MAXC)]
        return (f"X cpu{v[0]} pc={v[1]:#x} key=({v[2]:#x},{v[3]:#x}) ret={v[6]}" +
                fmt_decs(decs))
    if op == "C":
        return (f"add c{v[1]} kind={v[2]} flags={v[3]}" if v[0] else f"remove c{v[1]}")
    return " ".join([op] + [str(x) for x in v])


# --- the guest simulator ---------------------------------------------------------------------

INJ = {0: 1, 1: 1, 2: 0b1100, 3: 0b1100}       # what the target sets before each enter
NUMS_USED = [0, 1, 2, 3, 15, 39, 56, 58, 60, 93, 173, 202, 220, 435, 7000, 9000, M64]


class Gen:
    """A random sequence of target calls, run through the model as it is made (the simulated
    guest reacts to the trap's answers)."""

    def __init__(self, seed):
        self.rng = random.Random(seed)
        self.m = Model()
        self.cmds = []
        self.ids = 0
        self.tasks = []
        self.k1s = []

    def do(self, *cmd):
        out = self.m.run(cmd)
        self.cmds.append(cmd)
        return out

    def dec1(self, tid, at_return):
        rng = self.rng
        p = rng.random
        data = 0
        if p() < 0.3:
            self.ids += 1
            data = self.ids
        retval = rng.choice([0, 1, 7, M64, u64(-4), u64(-512), 0x80000000, 1 << 40])
        return (tid if p() < 0.5 else 0, data, rng.choice([0] * 8 + [1, 2]),
                rng.randrange(64) if p() < 0.08 else 0, rng.randrange(1 << 20),
                int(not at_return and p() < 0.04), int(p() < 0.1), retval,
                int(at_return and p() < 0.06), int(p() < 0.04), 0x7000 + rng.randrange(16) * 4096,
                rng.randrange(2), int(p() < 0.04), rng.choice([39, 102, 7000]))

    def dec(self, tid, at_return):
        return [x for _ in range(MAXC) for x in self.dec1(tid, at_return)]

    def new_task(self):
        rng = self.rng
        abi = rng.choice([0, 0, 0, 1, 2, 3])
        if self.k1s and rng.random() < 0.4:
            k1 = rng.choice(self.k1s)                   # a thread of an existing process
        else:
            k1 = 0x1000 * (len(self.k1s) + 1)
            self.k1s.append(k1)
        ln = 4 if abi == 2 else rng.choice([2, 4]) if abi == 3 else 2
        base = 0x400000 + 0x1000 * rng.randrange(3)     # shared code: the same pcs, other tasks
        self.ids += 1
        t = dict(tid=self.ids, abi=abi, k1=k1, k2=0x7ff000 - 0x100 * len(self.tasks), ln=ln,
                 pcs=[base + 0x40 * j + ln for j in range(3)], cpu=rng.randrange(NCPU),
                 state="user", call=None)
        self.tasks.append(t)
        return t

    def val(self, t):
        v = self.rng.choice([0, 0, 1, 5, 0x1000, M64, 0xFFFFFFFF, 1 << 33])
        return v & 0xFFFFFFFF if t["abi"] in IS32 else v

    def trap(self, t, pc, num, args):
        out = self.do("E", t["cpu"], INJ[t["abi"]], t["abi"], num, *args, pc, t["ln"], t["k1"],
                      t["k2"], 0xC000 + t["cpu"], 0b110, *self.dec(t["tid"], False))
        r, ret, oargs = out[0], out[1], out[2:8]
        if r & F_FAULT:
            t.update(state="fault_enter", call=(pc, num, args))
        elif r & F_INJECT:
            t.update(state="kernel", call=(pc, ret, oargs))
        elif r & F_SKIP:
            t.update(state="user", call=None)
        else:
            t.update(state="kernel", call=(pc, num, oargs))

    def back(self, t, pc, ret):
        """The task returns to user mode at pc: react to the trap's answer."""
        out = self.do("X", t["cpu"], pc, t["k1"], t["k2"], 0xC000 + t["cpu"], 0b110, ret,
                      *self.dec(t["tid"], True))
        r, rw = out[0], out[8:17]
        call = t["call"]
        if r & F_REWIND:
            # back at the trap instruction with the registers the trap gave
            t.update(state="at_trap", call=(call[0] if call else pc, rw[1], rw[2:8]))
        elif r & F_FAULT:
            t.update(state="fault_return")
        else:
            t.update(state="user", call=None)

    def step_task(self, t):
        rng = self.rng
        p = rng.random()
        st = t["state"]
        if st == "user":
            if p < 0.85:
                num = rng.choice(NUMS_USED)
                self.trap(t, rng.choice(t["pcs"][:2]), num, [self.val(t) for _ in range(6)])
            elif self.m.slots:
                # an interrupt or a fault in user code, returning somewhere else
                self.do("X", t["cpu"], rng.choice(t["pcs"]) + rng.choice([0, 1, -2, -4, 8]),
                        t["k1"], t["k2"], 0, 0, self.val(t), *self.dec(t["tid"], True))
        elif st == "at_trap":
            pc, num, args = t["call"]
            self.trap(t, pc, num, args)
        elif st == "fault_enter":
            # the guest handles the fault at the trap instruction, then the task traps again
            pc, num, args = t["call"]
            self.do("X", t["cpu"], u64(pc - t["ln"]), t["k1"], t["k2"], 0, 0, self.val(t),
                    *self.dec(t["tid"], True))
            t["state"] = "at_trap"
        elif st == "fault_return":
            self.back(t, t["call"][0], self.val(t))
        elif st == "kernel":
            pc, num, args = t["call"]
            if p < 0.6:
                self.back(t, pc, self.val(t))
            elif p < 0.66:
                # a vfork: the child returns 0 first, on the same stack, then the parent
                self.back(t, pc, 0)
                if t["state"] == "user":
                    t.update(state="kernel", call=(pc, num, args))
            elif p < 0.78:
                # restarted: back at the trap instruction, then trap again (restart_syscall for
                # ERESTART_RESTARTBLOCK)
                err = rng.choice([-512, -513, -514, -516])
                self.back(t, u64(pc - t["ln"]), mask(t["abi"], err))
                if t["state"] == "user":
                    t.update(state="at_trap", call=(pc, 219 if err == -516 else num, args))
            elif p < 0.86:
                # a signal: the handler, a call in it, sigreturn (no return), then this call's
                # return
                self.do("X", t["cpu"], 0x500000, t["k1"], t["k2"], 0, 0, mask(t["abi"], -4),
                        *self.dec(0, True))
                self.do("E", t["cpu"], INJ[t["abi"]], t["abi"], {0: 15, 2: 139}.get(t["abi"], 173),
                        *[0] * 6, t["pcs"][2], t["ln"], t["k1"], t["k2"], 0, 0,
                        *self.dec(t["tid"], False))
            elif p < 0.92:
                # killed: the consumers may or may not hear of it
                if rng.random() < 0.5:
                    self.do("F", t["tid"])
                self.tasks.remove(t)
            elif p < 0.96:
                # a successful execve: the call never returns, the task has a new address space
                t["k1"] = 0x1000 * (len(self.k1s) + 1)
                self.k1s.append(t["k1"])
                t.update(state="user", call=None)
            else:
                t["cpu"] = rng.randrange(NCPU)          # migrated
        else:
            raise AssertionError(st)

    def watch(self, i):
        rng = self.rng
        for abi in range(4):
            self.do("W", i, abi, M64, rng.choice([0, 0, 1, 2, 3, 3]))
            for num in rng.sample(NUMS_USED[:-1], 3):
                self.do("W", i, abi, num, rng.randrange(4))

    def add(self):
        rng = self.rng
        free = [i for i in range(1, MAXC + 1) if i not in self.m.cons]
        if free:
            i = rng.choice(free)
            self.do("C", 1, i, rng.choice([FULL, FULL, FULL, ENTER_ONLY]),
                    (OBSERVER if rng.random() < 0.4 else 0) | (SKIPPED if rng.random() < 0.5 else 0))
            self.watch(i)

    def step_global(self):
        rng = self.rng
        p = rng.random()
        if p < 0.15:
            self.do("A")
            for t in self.tasks:
                t.update(state="user", call=None)
        elif p < 0.3 and self.m.cons:
            self.watch(rng.choice(list(self.m.cons)))
        elif p < 0.38 and self.m.cons:
            i = rng.choice(list(self.m.cons))
            self.do("U", i)
            self.watch(i)
        elif p < 0.5:
            self.do("M", int(rng.random() < 0.6))
        elif p < 0.62:
            self.add()
        elif p < 0.74:
            self.do("C", 0, rng.randint(1, MAXC))
            if not self.m.cons:
                for t in self.tasks:
                    t.update(state="user", call=None)
                self.add()
        else:
            # raw calls: any key, any pc, any state
            t = rng.choice(self.tasks)
            pc = rng.choice(t["pcs"]) + rng.choice([0, 2, -2, 4, -4])
            if rng.random() < 0.5:
                self.do("E", t["cpu"], rng.randrange(16), t["abi"], rng.choice(NUMS_USED),
                        *[self.val(t) for _ in range(6)], pc, rng.choice([2, 4]), t["k1"],
                        rng.choice([t["k2"], 1]), 0, 0, *self.dec(0, False))
            else:
                self.do("X", rng.randrange(NCPU), pc, t["k1"], rng.choice([t["k2"], 1]), 0, 0,
                        self.val(t), *self.dec(0, True))

    def sequence(self):
        rng = self.rng
        self.do("R")
        for _ in range(rng.choice([1, 1, 2, 2, 3, 4, 6])):
            self.add()
        for _ in range(rng.randint(1, 6)):
            self.new_task()
        for _ in range(rng.randint(20, 150)):
            if not self.tasks or rng.random() < 0.05:
                self.new_task()
            if rng.random() < 0.06:
                self.step_global()
            else:
                self.step_task(rng.choice(self.tasks))
        self.do("A")
        if self.m.live:
            raise ModelError(f"data live after cancel_all: {sorted(self.m.live)}")
        return self.cmds


# --- running and shrinking -------------------------------------------------------------------

def model_outs(cmds):
    m = Model()
    outs = []
    for c in cmds:
        outs.append(m.run(c))
    return outs


def first_diff(exe, cmds):
    """(index, model, trap, stderr) of the first difference, or None."""
    try:
        want = model_outs(cmds)
    except ModelError as e:
        return (-1, str(e), None, "")
    got, err = run_c(exe, cmds)
    for i, w in enumerate(want):
        g = got[i] if i < len(got) else None
        if g != w:
            return (i, w, g, err)
    return None


def shrink(exe, cmds):
    """A shorter command list that still fails (removing chunks, then single commands)."""
    body = cmds[1:]                     # cmds[0] is R
    n = 2
    while len(body) >= 2:
        chunk = max(1, len(body) // n)
        for i in range(0, len(body), chunk):
            cand = body[:i] + body[i + chunk:]
            if first_diff(exe, [("R",)] + cand):
                body = cand
                n = max(n - 1, 2)
                break
        else:
            if chunk == 1:
                break
            n = min(n * 2, len(body))
    return [("R",)] + body


def report(exe, cmds, title):
    small = shrink(exe, cmds)
    i, want, got, err = first_diff(exe, small)
    print(f"FAIL: {title}; shrunk from {len(cmds)} to {len(small)} commands:")
    for j, c in enumerate(small[:i + 1] if i >= 0 else small):
        print(f"  {j:3}: {fmt(c)}")
    if i < 0:
        print(f"  model: {want}")
    elif got is None:
        print("  the trap stopped:\n" + "\n".join("    " + x for x in err.splitlines()[-30:]))
    else:
        print("\n".join(describe(small[i], want, got)))


def random_sequences(exe, seqs, seed, batch=400):
    done = calls = 0
    for start in range(0, seqs, batch):
        seqs_cmds = []
        for s in range(start, min(seqs, start + batch)):
            try:
                seqs_cmds.append(Gen(seed * 1_000_003 + s).sequence())
            except ModelError as e:
                print(f"FAIL: the model's own check, sequence {s} (seed {seed}): {e}")
                return False
        flat = [c for cmds in seqs_cmds for c in cmds]
        want = [o for cmds in seqs_cmds for o in model_outs(cmds)]
        got, err = run_c(exe, flat)
        if got != want:
            at = 0
            for s, cmds in enumerate(seqs_cmds):
                if got[at:at + len(cmds)] != want[at:at + len(cmds)]:
                    report(exe, cmds, f"random sequence {start + s} (seed {seed})")
                    return False
                at += len(cmds)
            print(f"FAIL: the trap stopped: {err[-3000:]}")
            return False
        done += len(seqs_cmds)
        calls += len(flat)
    print(f"random: {done} sequences, {calls} calls")
    return True


# --- directed cases --------------------------------------------------------------------------

def decs(dec):
    """Each consumer's decision: {id: D(...)}, or one D(...) for consumer 1."""
    if not isinstance(dec, dict):
        dec = {1: dec}
    return [x for i in range(1, MAXC + 1) for x in dec.get(i, NODEC)]


def E(num, pc=0x401002, args=(10, 11, 12, 13, 14, 15), k=(0x1000, 0x7ff000), abi=0, cpu=0,
      ln=2, dec=NODEC, inj=1):
    return ("E", cpu, inj, abi, num, *args, pc, ln, *k, 0xC000, 6, *decs(dec))


def X(ret, pc=0x401002, k=(0x1000, 0x7ff000), cpu=0, dec=NODEC):
    return ("X", cpu, pc, *k, 0xC000, 6, ret, *decs(dec))


def D(task=0, data=0, want=0, argmask=0, argval=0, skip=0, retset=0, retval=0, keep=0, fault=0,
      faddr=0, fwrite=0, inject=0, inum=0):
    return (task, data, want, argmask, argval, skip, retset, retval, keep, fault, faddr, fwrite,
            inject, inum)


def ADD(i, kind=FULL, flags=0):
    """A consumer that watches everything on x86_64."""
    return [("C", 1, i, kind, flags), ("W", i, 0, M64, 3)]


SETUP = [("R",)] + ADD(1)


def evs(out):
    """The events in a result, as dicts."""
    n = len(RESULT)
    return [dict(zip(EVENT, out[n + len(EVENT) * j:n + len(EVENT) * (j + 1)]))
            for j in range(out[n - 1])]


def ev(out):
    """The first event in a result, or None."""
    e = evs(out)
    return e[0] if e else None


def who(out):
    """(consumer, kind) of each event."""
    return [(e["cons"], e["kind"]) for e in evs(out)]


def res(out):
    return dict(zip(RESULT, out))


def dropped(out):
    n = len(RESULT) + len(EVENT) * out[len(RESULT) - 1]
    return list(out[n + 1:n + 1 + out[n]])


def case_restart(o):
    """A call the kernel restarts: the return at the trap instruction is reported as a restart,
    the call traps again and is a new call."""
    assert ev(o[0])["kind"] == 1
    e = ev(o[1])
    assert e["kind"] == 2 and e["restart"] and signed(e["ret"]) == -512, e
    assert ev(o[2])["kind"] == 1 and not ev(o[2])["resumed"]
    assert ev(o[3])["ret"] == 3 and not ev(o[3])["restart"]
    assert res(o[3])["pending"] == 0


def case_fault_enter(o):
    """A fault asked for at the entry: held (no return at the trap instruction), then the same
    call delivered again, resumed, with its data."""
    assert res(o[0])["r"] == F_FAULT and res(o[0])["held"] == 1 and res(o[0])["fault_addr"] == 0x7000
    assert ev(o[1]) is None and res(o[1])["held"] == 1
    e = ev(o[2])
    assert e["resumed"] and e["data"] == 1 and not e["fault"], e
    assert ev(o[3])["data"] == 1 and dropped(o[3]) == [1]


def case_fault_return(o):
    """A fault asked for at the return: held, then the return delivered again, resumed, with the
    value it was held with (an earlier modifier's change survives the hold; the guest's register
    is the same, as nothing changed meanwhile)."""
    assert res(o[1])["r"] == F_FAULT and res(o[1])["held"] == 1
    e = ev(o[2])
    assert e["kind"] == 2 and e["resumed"] and e["ret"] == 7, e
    assert res(o[2])["pending"] == 0


def case_inject_enter(o):
    """A call injected at the entry: the task runs it (no event), comes back to the trap with
    its own registers, and its call is delivered again with the injected call's return."""
    r = res(o[0])
    assert r["r"] == F_INJECT and r["ret"] == 39 and r["arg0"] == 39 * 16, r
    r = res(o[1])
    assert r["r"] == F_REWIND and ev(o[1]) is None, r
    assert (r["rw.num"], r["rw.arg0"], r["rw.len"]) == (0, 10, 2), r
    e = ev(o[2])
    assert e["resumed"] and e["injected"] and e["inject_ret"] == 1234 and e["num"] == 0, e
    assert ev(o[3])["kind"] == 2 and ev(o[3])["ret"] == 5


def case_inject_return(o):
    """A call injected at the return: the task runs it from the trap, and the return is
    delivered again with its answer; the task gets its own registers and return value back."""
    r = res(o[1])
    assert r["r"] == F_REWIND and r["rw.num"] == 39 and r["rw.len"] == 2, r
    assert ev(o[2]) is None and res(o[2])["r"] == 0          # the injected call traps: no event
    e = ev(o[3])
    assert e["kind"] == 2 and e["resumed"] and e["injected"] and e["inject_ret"] == 77, e
    r = res(o[3])
    assert r["r"] == F_RESTORE | F_RET and r["ret"] == 5 and r["rw.num"] == 0, r
    assert r["rw.arg0"] == 10 and r["pending"] == 0, r


def case_keep(o):
    """keep: the return was not the caller's (another task's at the same key): still pending."""
    assert res(o[1])["pending"] == 1 and dropped(o[1]) == []
    e = ev(o[2])
    assert e["kind"] == 2 and e["ret"] == 8 and e["data"] == 3, e
    assert dropped(o[2]) == [3]


def case_skip(o):
    """skip: no kernel entry, the consumer's value returned, no return event (the consumer did
    not ask for skipped calls' returns)."""
    r = res(o[0])
    assert r["r"] == F_SKIP and signed(r["ret"]) == -1 and r["pending"] == 0, r
    assert who(o[0]) == [(1, 1)] and dropped(o[0]) == [4]
    assert ev(o[1]) is None


def case_i386(o):
    """An i386 call: values are 32-bit; a skip's value is truncated, a return sign-extended."""
    assert res(o[1])["ret"] == 0xFFFFFFFF, res(o[1])
    assert ev(o[3])["ret"] == M64 - 3, ev(o[3])     # -4, sign-extended


def case_forget(o):
    """forget_task drops that task's calls, pending or held, and their data."""
    assert res(o[2])["pending"] == 3 and res(o[2])["held"] == 1, res(o[2])
    r = res(o[3])
    assert r["r"] == 2 and dropped(o[3]) == [5, 6] and r["pending"] == 1 and r["held"] == 0, r


def case_replay(o):
    """Record/replay: observers only: skip, fault and new arguments are ignored."""
    r = res(o[1])
    assert r["r"] == 0 and r["pending"] == 1 and r["held"] == 0, r


def case_detach(o):
    """The consumer can't be removed while a task runs a call it injected; after, it can (the
    last one: every call goes)."""
    assert res(o[1])["r"] == F_INJECT
    assert res(o[2])["r"] == 0 and res(o[2])["pending"] == 1
    assert res(o[3])["r"] == F_REWIND
    r = res(o[4])
    assert r["r"] == 1 and r["pending"] == 0 and dropped(o[4]) == [7] and not r["trap_on"], r


def case_sigreturn(o):
    """rt_sigreturn, exit and exit_group are tracked like any call: they return when seccomp or
    a tracer refuses them. A real rt_sigreturn returns to the interrupted code instead, and its
    slot waits until it is evicted or a call at its key replaces it."""
    assert res(o[0])["pending"] == 1 and dropped(o[0]) == [], res(o[0])
    assert res(o[1])["pending"] == 2 and res(o[2])["pending"] == 3
    assert ev(o[3]) is None and res(o[3])["pending"] == 3


def case_vfork(o):
    """vfork: the child's 0 return, on the parent's stack, is not the parent's (0019 reported it
    as the parent's and lost the pid); the parent's comes after."""
    assert ev(o[1]) is None and res(o[1])["pending"] == 1 and dropped(o[1]) == []
    e = ev(o[2])
    assert e["kind"] == 2 and e["ret"] == 1234 and e["data"] == 2, e
    assert res(o[2])["pending"] == 0


def case_two_modifiers(o):
    """Two modifiers and an observer: the second modifier faults, so the call is held for it;
    on the resume it gets the call again, then the observer; the first doesn't again. The
    return goes to all three, in order, each with its own data."""
    assert who(o[0]) == [(1, 1), (2, 1)] and res(o[0])["r"] == F_FAULT, who(o[0])
    assert who(o[1]) == []
    assert who(o[2]) == [(2, 1), (3, 1)], who(o[2])
    e2, e3 = evs(o[2])
    assert e2["resumed"] and e2["data"] == 2 and not e3["resumed"] and e3["data"] == 0
    assert who(o[3]) == [(1, 2), (2, 2), (3, 2)], who(o[3])
    assert [e["data"] for e in evs(o[3])] == [1, 2, 0] and dropped(o[3]) == [1, 2]


def case_observers(o):
    """Observers run after the modifiers (an observer added first, a modifier last), see the
    modifiers' final call, and change nothing."""
    assert who(o[0]) == [(1, 1), (3, 1), (2, 1)], who(o[0])
    m, o1, o2 = evs(o[0])
    assert o1["args0"] == 99 and o2["args0"] == 99 and o2["args1"] == 11 and not o2["skip"]
    r = res(o[0])
    assert r["r"] == F_ARGS and r["arg0"] == 99 and r["arg1"] == 11, r
    assert [e["ret"] for e in evs(o[1])] == [5, 6, 6], evs(o[1])
    r = res(o[1])
    assert r["r"] == F_RET and r["ret"] == 6, r


def case_inject_observed(o):
    """Injected calls with an observer: it sees neither injected call, the task's call once
    (after the holder's resumed entry), and the return once, after the holder's."""
    assert who(o[0]) == [(1, 1)] and res(o[0])["r"] == F_INJECT
    assert who(o[1]) == [] and res(o[1])["r"] == F_REWIND
    assert who(o[2]) == [(1, 1), (2, 1)], who(o[2])
    h, w = evs(o[2])
    assert h["resumed"] and h["injected"] and not w["resumed"] and not w["injected"]
    assert who(o[3]) == [(1, 2)] and res(o[3])["r"] == F_REWIND
    assert who(o[4]) == []
    assert who(o[5]) == [(1, 2), (2, 2)], who(o[5])
    h, w = evs(o[5])
    assert h["injected"] and h["inject_ret"] == 77 and not w["injected"] and w["ret"] == 5
    assert res(o[5])["r"] == F_RESTORE | F_RET and res(o[5])["pending"] == 0


def case_restart_two(o):
    """A restart with two consumers: both get it, both get the new call."""
    assert who(o[1]) == [(1, 2), (2, 2)] and all(e["restart"] for e in evs(o[1]))
    assert who(o[2]) == [(1, 1), (2, 1)] and who(o[3]) == [(1, 2), (2, 2)]
    assert res(o[3])["pending"] == 0


def case_keep_one(o):
    """keep by one consumer: only its part waits for the next return."""
    assert who(o[1]) == [(1, 2), (2, 2)] and res(o[1])["pending"] == 1 and dropped(o[1]) == [4]
    assert who(o[2]) == [(1, 2)] and ev(o[2])["ret"] == 8 and dropped(o[2]) == [3]


def case_skipped_return(o):
    """A skipped call: its return right after the entry, with skipped set, to the consumers that
    ask for it (the observer with SKIPPED; not the skipping modifier, nor the other observer)."""
    assert who(o[0]) == [(1, 1), (2, 1), (3, 1), (2, 2)], who(o[0])
    e = evs(o[0])[3]
    assert e["skipped"] and e["ret"] == M64 and e["skip"], e
    assert res(o[0])["r"] == F_SKIP and dropped(o[0]) == [4] and res(o[0])["pending"] == 0


def case_remove_midcall(o):
    """A consumer removed between the entry and the return: its data goes then, and the return
    goes to the other consumer only."""
    assert res(o[1])["r"] == 1 and dropped(o[1]) == [2] and res(o[1])["pending"] == 1
    assert who(o[2]) == [(1, 2)] and dropped(o[2]) == [1]


def case_add_midcall(o):
    """A consumer added between the entry and the return sees no return of that call."""
    assert who(o[3]) == [(1, 2)], who(o[3])
    assert who(o[4]) == [(1, 1), (2, 1)] and who(o[5]) == [(1, 2), (2, 2)]


def case_holder_removed(o):
    """The modifier holding a fault is removed: the call stays held, and when the task is back
    it goes on to the consumers after it."""
    assert res(o[0])["r"] == F_FAULT and who(o[0]) == [(1, 1)]
    assert res(o[1])["r"] == 1 and res(o[1])["held"] == 1 and dropped(o[1]) == [1]
    assert who(o[3]) == [(2, 1)] and not ev(o[3])["resumed"]
    assert who(o[4]) == [(2, 2)]


def case_hold_args(o):
    """A modifier changes the arguments, the next one faults: on the resume the holder sees the
    first one's arguments, and the kernel gets them."""
    assert res(o[0])["r"] == F_FAULT
    assert evs(o[2])[0]["args0"] == 77 and evs(o[2])[0]["resumed"]
    r = res(o[2])
    assert r["r"] == F_ARGS and r["arg0"] == 77, r


DIRECTED = [
    (case_restart, [E(0), X(u64(-512), pc=0x401000), E(0), X(3)]),
    (case_fault_enter, [E(0, dec=D(data=1, fault=1, faddr=0x7000)), X(0, pc=0x401000), E(0),
                        X(4)]),
    (case_fault_return, [E(0, dec=D(data=2)), X(7, dec=D(fault=1, faddr=0x8000)), X(7)]),
    (case_inject_enter, [E(0, dec=D(inject=1, inum=39)), X(1234), E(0), X(5)]),
    (case_inject_return, [E(0), X(5, dec=D(inject=1, inum=39)),
                          E(39, args=[39 * 16 + i for i in range(6)]), X(77)]),
    (case_keep, [E(0, dec=D(data=3)), X(1, dec=D(keep=1)), X(8)]),
    (case_skip, [E(0, dec=D(data=4, skip=1, retval=M64)), X(0)]),
    (case_i386, [("W", 1, 1, M64, 3), E(20, abi=1, dec=D(skip=1, retval=M64), inj=1),
                 E(20, abi=1), X(0xFFFFFFFC)]),
    (case_forget, [E(0, dec=D(task=9, data=5)),
                   E(0, pc=0x401042, dec=D(task=9, data=6, fault=1, faddr=0x7000)),
                   E(0, k=(0x1000, 0x7fe000), dec=D(task=8)), ("F", 9)]),
    (case_replay, [("M", 0), E(0, dec=D(skip=1, fault=1, argmask=1, argval=99))]),
    (case_detach, [("W", 1, 0, 39, 0), E(0, dec=D(data=7, inject=1, inum=39)), ("C", 0, 1),
                   X(1), ("C", 0, 1)]),
    (case_sigreturn, [E(15, pc=0x500002, dec=D(data=1, want=2)), E(60, pc=0x500042),
                      E(231, pc=0x500082), X(0, pc=0x401002)]),
    (case_vfork, [E(58, dec=D(data=2)), X(0), X(1234)]),
    (case_two_modifiers, ADD(2) + ADD(3, flags=OBSERVER), [
        E(0, dec={1: D(data=1), 2: D(data=2, fault=1, faddr=0x7000)}), X(0, pc=0x401000), E(0),
        X(4)]),
    (case_observers, [("R",)] + ADD(3, flags=OBSERVER) + ADD(2, flags=OBSERVER) + ADD(1), [
        E(0, dec={1: D(argmask=1, argval=99),
                  3: D(skip=1, argmask=2, argval=5, retval=1)}),
        X(5, dec={1: D(retset=1, retval=6), 3: D(retset=1, retval=8)})]),
    (case_inject_observed, ADD(2, flags=OBSERVER), [
        E(0, dec=D(inject=1, inum=39)), X(1234), E(0), X(5, dec=D(inject=1, inum=39)),
        E(39, args=[39 * 16 + i for i in range(6)]), X(77)]),
    (case_restart_two, ADD(2), [E(0), X(u64(-512), pc=0x401000), E(0), X(3)]),
    (case_keep_one, ADD(2), [E(0, dec={1: D(data=3), 2: D(data=4)}), X(1, dec=D(keep=1)),
                             X(8)]),
    (case_skipped_return, ADD(2, flags=OBSERVER | SKIPPED) + ADD(3, flags=OBSERVER), [
        E(0, dec=D(data=4, skip=1, retval=M64))]),
    (case_remove_midcall, ADD(2), [E(0, dec={1: D(data=1), 2: D(data=2)}), ("C", 0, 2), X(5)]),
    (case_add_midcall, [], [E(0)] + ADD(2) + [X(5), E(0), X(6)]),
    (case_holder_removed, ADD(2), [E(0, dec=D(data=1, fault=1, faddr=0x7000)), ("C", 0, 1),
                                   X(0, pc=0x401000), E(0), X(3)]),
    (case_hold_args, ADD(2), [E(0, dec={1: D(argmask=1, argval=77), 2: D(fault=1, faddr=0x7000)}),
                              X(0, pc=0x401000), E(0)]),
]


def directed(exe):
    ok = True
    for case in DIRECTED:
        check, pre, steps = case if len(case) == 3 else (case[0], [], case[1])
        setup = (pre if pre[:1] == [("R",)] else SETUP + pre)
        cmds = setup + steps
        d = first_diff(exe, cmds)
        if d:
            report(exe, cmds, check.__name__)
            ok = False
            continue
        try:
            check(model_outs(cmds)[len(setup):])
        except AssertionError as e:
            print(f"FAIL: {check.__name__}: {check.__doc__.split(chr(10))[0]}\n    {e!r}")
            ok = False
    print(f"directed: {len(DIRECTED)} cases" + ("" if ok else ", FAILED"))
    return ok


def capacity(exe):
    """4096 pending calls and more: at 3/4 full the calls older than the latest 2048 are evicted;
    an evicted call's return finds nothing, and a new call at its key gets its own return."""
    cmds = list(SETUP)
    n = SLOTS + 1
    for i in range(n):
        cmds.append(E(0, k=(0x1000, 0x10000000 + 0x100 * i), dec=D(data=i + 1)))
    cmds.append(X(1, k=(0x1000, 0x10000000)))                       # the first: evicted
    cmds.append(E(0, k=(0x1000, 0x10000000), dec=D(data=n + 1)))    # a new call at its key
    cmds.append(X(2, k=(0x1000, 0x10000000)))
    cmds.append(("A",))
    d = first_diff(exe, cmds)
    if d:
        i, want, got, err = d
        print(f"FAIL: capacity: command {i}: {fmt(cmds[i])}")
        print("\n".join(describe(cmds[i], want, got)) if got else err[-3000:])
        return False
    outs = model_outs(cmds)
    peak = max(res(o)["pending"] for o in outs)
    stale = res(outs[-2])["stale"]
    first_ret = ev(outs[len(SETUP) + n])
    e = ev(outs[len(SETUP) + n + 2])
    ok = (peak < SLOTS and stale > 0 and first_ret is None and e and e["ret"] == 2 and
          e["data"] == n + 1 and not res(outs[-1])["live"])
    print(f"capacity: {n} calls, peak {peak} pending, {stale} evicted" +
          ("" if ok else f", FAILED (first return {first_ret}, new call's {e})"))
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seqs", type=int, default=int(os.environ.get("SYSCALL_MODEL_SEQS", 10000)))
    ap.add_argument("--seed", type=int, default=1)
    opt = ap.parse_args()
    with tempfile.TemporaryDirectory(prefix="syscall-consumers-") as tmp:
        exe = build(Path(tmp))
        ok = directed(exe)
        ok = capacity(exe) and ok
        ok = random_sequences(exe, opt.seqs, opt.seed) and ok
    print("PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
