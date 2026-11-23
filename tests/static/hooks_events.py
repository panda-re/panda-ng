#!/usr/bin/env python3
"""The hooks plugin on the engine's address hooks, publishing hooks.hit: host-only.

No engine and no guest. plugins/hooks/hooks.cpp is compiled whole, with ASan and UBSan, against
stub headers and a fake engine: panda/address-hook.h's calls record the hooks inserted per
address, a "hit" calls them the way the engine's helper does (with the block set for
panda_address_hook_tb), and panda_free_rcu waits for the test's grace period, so a read of a freed
hook or array before it is caught. The registry is the real engine/src/panda/src/events.c, built
with tests/static/events_registry.py's stubs.

It checks:
- hooks.hit is declared under PANDA_EVENT_NS with the signature hooks_events.h builds, a UUID
  that Python's uuid5 derives too, and a struct the size the signature implies. A subscription
  made before the plugin loads binds when it declares; one under another signature is a MISMATCH,
  and a mutator is REFUSED;
- a subscriber gets the struct (size, cpu, block, pc, hook) after the hook's function, once per
  hook that ran, and nothing when disabled; without subscribers nothing is emitted;
- the filters (asid, kernel/user mode) and the order (hooks with an asid first);
- each hook type is called through the cb member its type names (after_block_exec with an exit
  code, before_block_translate with the address);
- the bookkeeping: one engine hook per address, removed when its last hook goes and inserted
  again by the next add; a duplicate add_hook is ignored and a disabled one is not added; a hook
  disabled in its function, or from outside, is removed; add_hook from inside a hook at the same
  address; enable_hooking/disable_hooking; a hit does not republish or free anything; unload
  removes every engine hook and leaks nothing.

    python3 tests/static/hooks_events.py

Needs a C and a C++ compiler with ASan and UBSan, pkg-config and GLib.
"""
import os
import shlex
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import events_registry as er  # noqa: E402

PANDA_TEST = dict(area="hooks", tier="static", needs=["cc", "c++", "glib"], nix_check="hooks-events")

ROOT = er.ROOT
SRC = er.SRC
HOOKS = ROOT / "plugins/hooks"
SIGNATURE = "cpu:ptr<CPUState>,tb:ptr<TranslationBlock>,pc:u64,hook:ptr,"

# The plugin's view of the engine (normally the generated panda-ng/panda_cpp_<arch>.h).
PLUGIN_STUBS = {
"panda.h": r'''
#pragma once
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
extern "C" {
typedef uint64_t target_ulong;
typedef uint64_t target_ptr_t;
typedef struct CPUState { uint64_t asid; bool kernel; } CPUState;
typedef struct TranslationBlock { int id; } TranslationBlock;
struct qemu_plugin_tb;
typedef enum panda_cb_type {
    PANDA_CB_BEFORE_TCG_CODEGEN, PANDA_CB_BEFORE_BLOCK_EXEC, PANDA_CB_START_BLOCK_EXEC,
    PANDA_CB_BEFORE_BLOCK_TRANSLATE, PANDA_CB_AFTER_BLOCK_EXEC,
    PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT,
} panda_cb_type;
enum { PANDA_MODE_ANALYSIS = 1, PANDA_MODE_FUZZ = 2, PANDA_MODE_RR = 4, PANDA_MODE_KVM = 8 };
void panda_require(const char *name);
void panda_enable_precise_pc(void);
bool panda_in_kernel_external(const CPUState *cpu);
}
''',
"hw_proc_id/hw_proc_id_ext.h": r'''
#pragma once
target_ulong get_id(CPUState *cpu);
bool init_hw_proc_id_api(void);
''',
}

# events.c's other half (callbacks.c in the engine), and the grace period.
GLUE = r'''
#include "qemu/osdep.h"
#include "qemu/rcu.h"
#include "system/system.h"
#include "panda/events-internal.h"

struct rcu_head *rcu_deferred;
/* The machine is up from the start (events_registry.py has the cases before it is). */
bool stub_machine_up = true;
static void __attribute__((constructor)) stub_machine_ready(void) { panda_events_machine_up(); }
bool env_frozen = true, env_analysis = true, env_kvm;
uint32_t env_caps;
int replay_mode;

const PandaCoreEventDef panda_core_events[] = { { 0, "alpha", "cpu:ptr,pc:u64", false } };
const size_t panda_core_events_count = 1;
uint32_t panda_core_event_modes(int type) { return 7; }
const char *panda_event_owner_name(void *owner) { return owner ? "plugin" : NULL; }
void panda_core_event_describe_subs(int type, GString *out, bool json) {}
/* The registry's startup check waits for the machine; it is up already. */
void qemu_add_machine_init_done_notifier(Notifier *notify) { }

void events_rcu_flush(void);
void events_rcu_flush(void)
{
    while (rcu_deferred) {
        struct rcu_head *h = rcu_deferred;
        rcu_deferred = h->next;
        h->func(h);
    }
}
'''

HARNESS = r'''
#include <map>
#include <string>
#include <vector>
extern "C" void events_rcu_flush(void);
extern "C" void panda_events_owner_gone(void *owner);     /* events-internal.h: the unload */

static int fails;
static std::string last;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL line %d %s: ", __LINE__, \
    #cond); printf(__VA_ARGS__); printf(" (last calls: %s)\n", last.c_str()); } } while (0)

/* --- the fake engine ------------------------------------------------------------------------ */

struct PandaAddressHook { uint64_t pc; panda_address_hook_fn fn; void *opaque; bool live; };
static std::map<uint64_t, std::vector<PandaAddressHook *>> engine;
static std::vector<PandaAddressHook *> engine_all;
static int inserts, removes;
static TranslationBlock *running_tb;
static std::vector<void *> deferred;
static int frees;

extern "C" PandaAddressHook *panda_address_hook_insert(uint64_t pc, panda_address_hook_fn fn,
                                                       void *opaque)
{
    PandaAddressHook *h = new PandaAddressHook{pc, fn, opaque, true};
    engine[pc].push_back(h);
    engine_all.push_back(h);
    inserts++;
    return h;
}

extern "C" void panda_address_hook_remove(PandaAddressHook *h)
{
    if (!h) {
        return;
    }
    auto &v = engine[h->pc];
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (*it == h) {
            v.erase(it);
            break;
        }
    }
    h->live = false;                    /* freed at the end: a pass may still hold it */
    removes++;
}

extern "C" struct TranslationBlock *panda_address_hook_tb(struct CPUState *cpu)
{
    return running_tb;
}

extern "C" void panda_free_rcu(void *p)
{
    if (p) {
        deferred.push_back(p);
        frees++;
    }
}

static void grace(void)
{
    for (void *p : deferred) {
        free(p);
    }
    deferred.clear();
    events_rcu_flush();
}

static int live_engine_hooks(uint64_t pc)
{
    auto it = engine.find(pc);
    return it == engine.end() ? 0 : (int)it->second.size();
}

/* The engine's helper: every hook at @pc, in a snapshot taken before the first is called. */
static TranslationBlock tbs[4] = {{0}, {1}, {2}, {3}};
static void hit(CPUState *cpu, uint64_t pc, int tb);
static void hit(CPUState *cpu, uint64_t pc) { hit(cpu, pc, 0); }
static void hit(CPUState *cpu, uint64_t pc, int tb)
{
    std::vector<PandaAddressHook *> pass = engine[pc];
    running_tb = &tbs[tb];
    for (PandaAddressHook *h : pass) {
        if (h->live) {
            h->fn(cpu, pc, h->opaque);
        }
    }
    running_tb = NULL;
}

/* --- the rest of the plugin's world ---------------------------------------------------------- */

extern "C" void panda_require(const char *name) {}
extern "C" void panda_enable_precise_pc(void) {}
extern "C" bool panda_in_kernel_external(const CPUState *cpu) { return cpu->kernel; }
static int get_ids;
target_ulong get_id(CPUState *cpu) { get_ids++; return cpu->asid; }
bool init_hw_proc_id_api(void) { return true; }

/* --- hook functions and a subscriber --------------------------------------------------------- */

static std::vector<std::string> log_;
static void note(const char *what, uint64_t pc) { log_.push_back(std::string(what) + "@" +
                                                                  std::to_string(pc)); }
static std::string joined(void)
{
    std::string s;
    for (auto &x : log_) {
        s += (s.empty() ? "" : " ") + x;
    }
    log_.clear();
    last = s;
    return s;
}

static CPUState *expect_cpu;
static TranslationBlock *got_tb;
static void fa(CPUState *cpu, TranslationBlock *tb, struct hook *h)
{
    CHECK(cpu == expect_cpu, "cpu");
    got_tb = tb;
    note(h->context ? (const char *)h->context : "a", h->addr);
}
static void fb(CPUState *cpu, TranslationBlock *tb, struct hook *h) { note("b", h->addr); }
/* Two more like fa: a duplicate is the same function at the same address, asid and type, whatever
 * its mode (as the old plugin's std::set compared them). */
static void fk(CPUState *cpu, TranslationBlock *tb, struct hook *h) { fa(cpu, tb, h); }
static void fu(CPUState *cpu, TranslationBlock *tb, struct hook *h) { fa(cpu, tb, h); }
static void fonce(CPUState *cpu, TranslationBlock *tb, struct hook *h)
{
    note("once", h->addr);
    h->enabled = false;
}
static struct hook *kept;
static void fkeep(CPUState *cpu, TranslationBlock *tb, struct hook *h)
{
    note("keep", h->addr);
    kept = h;
}
static void fadder(CPUState *cpu, TranslationBlock *tb, struct hook *h);
static void hit(CPUState *cpu, uint64_t pc, int tb);
static int racing;
static void frace(CPUState *cpu, TranslationBlock *tb, struct hook *h)
{
    note("race", h->addr);
    if (!racing++) {
        hit(cpu, h->addr, 0);       /* another vCPU's pass at the same address, inside this one */
    }
}

static struct hook mk(uint64_t addr, void (*fn)(CPUState *, TranslationBlock *, struct hook *),
                      uint64_t asid = 0, enum kernel_mode km = MODE_ANY, const char *ctx = NULL)
{
    struct hook h;
    memset(&h, 0xa5, sizeof(h));            /* as syscalls2 leaves context: unset */
    h.addr = addr;
    h.asid = asid;
    h.type = PANDA_CB_START_BLOCK_EXEC;
    h.cb.before_tcg_codegen = fn;
    h.km = km;
    h.enabled = true;
    h.context = (void *)ctx;
    return h;
}

static void fadder(CPUState *cpu, TranslationBlock *tb, struct hook *h)
{
    note("adder", h->addr);
    h->enabled = false;
    struct hook n = mk(h->addr, fb);
    add_hook(&n);                           /* republishes the array this pass is walking */
}

struct seen {
    int n;
    uint32_t size;
    void *cpu, *tb, *hook;
    uint64_t pc;
    uint64_t hook_addr;
    bool hook_enabled;
};
static seen seen_;
static void on_event(void *ctx, void *args, void *data)
{
    struct panda_hooks_hit_args *a = (struct panda_hooks_hit_args *)args;
    CHECK(ctx == &seen_, "ctx");
    CHECK(data == NULL, "data");
    seen_.n++;
    seen_.size = a->size;
    seen_.cpu = a->cpu;
    seen_.tb = a->tb;
    seen_.pc = a->pc;
    seen_.hook = a->hook;
    seen_.hook_addr = ((struct hook *)a->hook)->addr;
    seen_.hook_enabled = ((struct hook *)a->hook)->enabled;
    note("event", a->pc);
}

static void *const OTHER = (void *)2;

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    void *const SELF = (void *)1;
    const char *reason = NULL;

    /* Before the plugin loads: found, PENDING; under the 10-30 example's signature too. */
    panda_event_id early = panda_hooks_hit_event();
    panda_sub pend = panda_subscribe(OTHER, early, on_event, &seen_, PANDA_MODE_ANALYSIS,
                                     PANDA_SUB_OBSERVE);
    CHECK(panda_sub_status(pend, &reason) == PANDA_SUB_PENDING, "%s", reason);
    panda_event_id old = panda_event_find(&PANDA_EVENT_NS, "hooks.hit", "cpu:ptr,pc:u64,hook:ptr");
    panda_sub mism = panda_subscribe(OTHER, old, on_event, &seen_, PANDA_MODE_ANALYSIS,
                                     PANDA_SUB_OBSERVE);
    /* Untyped pointers: another identity, since ptr<T> targets are hashed. */
    panda_event_id untyped = panda_event_find(&PANDA_EVENT_NS, "hooks.hit",
                                              "cpu:ptr,tb:ptr,pc:u64,hook:ptr");
    panda_sub mism2 = panda_subscribe(OTHER, untyped, on_event, &seen_, PANDA_MODE_ANALYSIS,
                                      PANDA_SUB_OBSERVE);

    CHECK(init_plugin(SELF), "init");
    panda_event_id ev = panda_event_lookup_name("hooks.hit");
    CHECK(ev != NULL && ev == early && ev == hit_event, "declared, the same event as found");
    CHECK(panda_sub_status(pend, &reason) == PANDA_SUB_ACTIVE, "pending binds: %s", reason);
    CHECK(panda_sub_status(mism, &reason) == PANDA_SUB_MISMATCH, "%s", reason);
    printf("MISMATCH %s\n", reason ? reason : "");
    CHECK(panda_sub_status(mism2, &reason) == PANDA_SUB_MISMATCH, "%s", reason);
    printf("MISMATCH2 %s\n", reason ? reason : "");
    panda_sub mut = panda_subscribe(OTHER, ev, on_event, &seen_, PANDA_MODE_ANALYSIS,
                                    PANDA_SUB_MUTATE);
    CHECK(!mut || panda_sub_status(mut, &reason) == PANDA_SUB_REFUSED, "a mutator is refused");
    panda_sub kvm = panda_subscribe(OTHER, ev, on_event, &seen_, PANDA_MODE_KVM,
                                    PANDA_SUB_OBSERVE);
    CHECK(!kvm || panda_sub_status(kvm, &reason) == PANDA_SUB_REFUSED, "KVM is refused");
    panda_event_uuid u;
    panda_event_uuid_of(ev, &u);
    printf("UUID ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", u.b[i]);
    }
    printf("\nSIG %s\nSIZE %zu\n", PANDA_HOOKS_HIT_SIGNATURE, sizeof(struct panda_hooks_hit_args));
    CHECK(panda_unsubscribe(pend) && panda_unsubscribe(mism) && panda_unsubscribe(mism2),
          "unsubscribe");
    if (mut) panda_unsubscribe(mut);
    if (kvm) panda_unsubscribe(kvm);
    grace();

    CPUState user = {0x1000, false}, kern = {0x1000, true}, other = {0x2000, false};
    expect_cpu = &user;

    /* No subscriber: the hook runs, nothing is emitted, and get_id isn't needed. */
    CHECK(!panda_event_wanted(ev), "nobody wants it");
    struct hook a = mk(0x400100, fa);
    add_hook(&a);
    CHECK(live_engine_hooks(0x400100) == 1 && inserts == 1, "one engine hook");
    hit(&user, 0x400100, 2);
    CHECK(joined() == "a@4194560", "the hook ran");
    CHECK(got_tb == &tbs[2], "its block");
    CHECK(get_ids == 0, "no asid filter, no get_id: %d", get_ids);
    a.enabled = false;                      /* the caller's copy: the plugin's is unaffected */
    hit(&user, 0x400100);
    CHECK(joined() == "a@4194560", "add_hook copied the struct");

    /* A subscriber gets the struct, after the hook's function. */
    panda_sub sub = panda_subscribe(OTHER, ev, on_event, &seen_, PANDA_MODE_ANALYSIS,
                                    PANDA_SUB_OBSERVE);
    CHECK(panda_sub_status(sub, &reason) == PANDA_SUB_ACTIVE, "%s", reason);
    CHECK(panda_event_wanted(ev), "wanted");
    int frees0 = frees;
    hook_list *l0 = sites[0x400100]->list;
    hit(&user, 0x400100, 1);
    CHECK(joined() == "a@4194560 event@4194560", "hook, then event");
    CHECK(seen_.n == 1 && seen_.size == sizeof(struct panda_hooks_hit_args), "size %u",
          seen_.size);
    CHECK(seen_.cpu == &user && seen_.tb == &tbs[1] && seen_.pc == 0x400100, "cpu, tb, pc");
    CHECK(seen_.hook_addr == 0x400100 && seen_.hook == l0->h[0], "the plugin's hook");
    CHECK(frees == frees0 && sites[0x400100]->list == l0, "a hit frees and republishes nothing");

    /* Disabled subscription: nothing emitted. */
    CHECK(panda_sub_enable(sub, false), "disable");
    CHECK(!panda_event_wanted(ev), "unwanted when disabled");
    hit(&user, 0x400100);
    CHECK(joined() == "a@4194560" && seen_.n == 1, "no event while disabled");
    CHECK(panda_sub_enable(sub, true), "enable");
    seen_.n = 0;

    /* A duplicate is ignored; another function at the address is a second hook. */
    struct hook a2 = mk(0x400100, fa);
    add_hook(&a2);
    hit(&user, 0x400100);
    CHECK(joined() == "a@4194560 event@4194560", "duplicate ignored");
    struct hook b = mk(0x400100, fb);
    add_hook(&b);
    CHECK(live_engine_hooks(0x400100) == 1 && inserts == 1, "still one engine hook");
    hit(&user, 0x400100);
    CHECK(joined() == "a@4194560 event@4194560 b@4194560 event@4194560", "two hooks, in order");
    CHECK(seen_.n == 3, "an event per hook that ran");

    /* asid and mode filters; hooks with an asid run first. */
    struct hook s1 = mk(0x500000, fa, 0, MODE_ANY, "any");
    struct hook s2 = mk(0x500000, fa, 0x1000, MODE_ANY, "asid1000");
    struct hook s3 = mk(0x500000, fk, 0, MODE_KERNEL_ONLY, "kernel");
    struct hook s4 = mk(0x500000, fu, 0, MODE_USER_ONLY, "user");
    struct hook s6 = mk(0x500000, fa, 0, MODE_KERNEL_ONLY, "dup");     /* s1's duplicate */
    struct hook s5 = mk(0x500000, fa, 0x2000, MODE_USER_ONLY, "asid2000user");
    add_hook(&s1); add_hook(&s2); add_hook(&s3); add_hook(&s4); add_hook(&s5); add_hook(&s6);
    CHECK(panda_sub_enable(sub, false), "quiet");
    hit(&user, 0x500000);
    CHECK(joined() == "asid1000@5242880 any@5242880 user@5242880", "user, asid 1000");
    expect_cpu = &kern;
    hit(&kern, 0x500000);
    CHECK(joined() == "asid1000@5242880 any@5242880 kernel@5242880", "kernel, asid 1000");
    expect_cpu = &other;
    hit(&other, 0x500000);
    CHECK(joined() == "asid2000user@5242880 any@5242880 user@5242880", "user, asid 2000");
    CHECK(get_ids == 3, "get_id once per hit: %d", get_ids);
    expect_cpu = &user;
    CHECK(panda_sub_enable(sub, true), "loud");

    /* A disabled hook is not added. */
    struct hook d = mk(0x600000, fa);
    d.enabled = false;
    add_hook(&d);
    CHECK(live_engine_hooks(0x600000) == 0 && sites.count(0x600000) == 0, "not added");

    /* Disabled in its function: runs once (its event sees it disabled), then its engine hook goes;
     * the next add inserts one again. */
    struct hook o = mk(0x700000, fonce);
    add_hook(&o);
    int ins = inserts;
    seen_.n = 0;
    hit(&user, 0x700000);
    CHECK(joined() == "once@7340032 event@7340032", "once");
    CHECK(seen_.n == 1 && !seen_.hook_enabled, "the event after the hook disabled itself");
    CHECK(live_engine_hooks(0x700000) == 0 && removes == 1, "engine hook removed");
    CHECK(sites[0x700000]->list == NULL, "no hooks left");
    hit(&user, 0x700000);
    CHECK(joined() == "", "gone");
    grace();
    add_hook(&o);
    CHECK(live_engine_hooks(0x700000) == 1 && inserts == ins + 1, "inserted again");
    hit(&user, 0x700000);
    CHECK(joined() == "once@7340032 event@7340032", "fires again once");
    grace();

    /* Disabled from outside, through a pointer a hook function kept: dropped at the next hit,
     * the other hook at the address stays. */
    struct hook k = mk(0x800000, fkeep);
    struct hook k2 = mk(0x800000, fb);
    add_hook(&k); add_hook(&k2);
    CHECK(panda_sub_enable(sub, false), "quiet");
    hit(&user, 0x800000);
    CHECK(joined() == "keep@8388608 b@8388608", "both");
    kept->enabled = false;
    hit(&user, 0x800000);
    CHECK(joined() == "b@8388608", "kept one skipped");
    CHECK(sites[0x800000]->list && sites[0x800000]->list->n == 1, "and dropped");
    CHECK(live_engine_hooks(0x800000) == 1, "the address keeps its engine hook");

    /* add_hook from a hook function at its own address, mid-pass: the pass keeps walking the old
     * array (ASan: not freed before the grace period); the new hook runs from the next hit. */
    struct hook ad = mk(0x900000, fadder);
    struct hook ad2 = mk(0x900000, fa, 0, MODE_ANY, "after");
    add_hook(&ad); add_hook(&ad2);
    hit(&user, 0x900000);
    CHECK(joined() == "adder@9437184 after@9437184", "this pass: %s", "the old array");
    hit(&user, 0x900000);
    CHECK(joined() == "after@9437184 b@9437184", "next pass: the new hook, the adder gone");
    grace();

    /* Two vCPUs at one address: the inner pass removes a hook that disabled itself while the
     * outer pass still holds it in its array (ASan: freed only after the grace period). */
    struct hook r1 = mk(0xa00000, frace);
    struct hook r2 = mk(0xa00000, fonce);
    add_hook(&r1); add_hook(&r2);
    hit(&user, 0xa00000);
    CHECK(joined() == "race@10485760 race@10485760 once@10485760", "the outer pass skips it");
    CHECK(sites[0xa00000]->list->n == 1, "removed once");
    grace();

    /* disable_hooking / enable_hooking. */
    disable_hooking();
    hit(&user, 0x400100);
    CHECK(joined() == "", "disabled");
    enable_hooking();
    hit(&user, 0x400100);
    CHECK(joined() == "a@4194560 b@4194560", "enabled");

    /* Each type is called through the cb member it names: after_block_exec with an exit code
     * (0), before_block_translate with the hooked address, invalidate_opt's result ignored. */
    struct hook ta = mk(0xb00000, NULL), tt = mk(0xb00000, NULL), ti = mk(0xb00000, NULL);
    ta.type = PANDA_CB_AFTER_BLOCK_EXEC;
    ta.cb.after_block_exec = [](CPUState *, TranslationBlock *tb, uint8_t code, struct hook *h) {
        CHECK(code == 0 && tb == &tbs[2], "after_block_exec arguments");
        note("abe", h->addr);
    };
    tt.type = PANDA_CB_BEFORE_BLOCK_TRANSLATE;
    tt.cb.before_block_translate = [](CPUState *, target_ptr_t pc, struct hook *h) {
        CHECK(pc == h->addr, "before_block_translate pc");
        note("bbt", pc);
    };
    ti.type = PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT;
    ti.cb.before_block_exec_invalidate_opt = [](CPUState *, TranslationBlock *, struct hook *h) {
        note("inv", h->addr);
        return true;
    };
    add_hook(&ta); add_hook(&tt); add_hook(&ti);
    hit(&user, 0xb00000, 2);
    CHECK(joined() == "abe@11534336 bbt@11534336 inv@11534336", "types");

    /* Unload: no engine hook is left, the frees are the engine's, nothing leaks. */
    CHECK(panda_unsubscribe(sub), "unsubscribe");
    uninit_plugin(SELF);
    int left = 0;
    for (auto &e : engine) {
        left += e.second.size();
    }
    CHECK(left == 0, "engine hooks left: %d", left);
    panda_events_owner_gone(SELF);
    grace();
    for (PandaAddressHook *h : engine_all) {
        delete h;
    }
    printf("RESULT %d checks failed\n", fails);
    return fails ? 1 : 0;
}
'''


def build(out):
    cstubs = out / "cstubs"
    for name, text in er.STUBS.items():
        (cstubs / name).parent.mkdir(parents=True, exist_ok=True)
        (cstubs / name).write_text(text)
    pstubs = out / "pstubs"
    for name, text in PLUGIN_STUBS.items():
        (pstubs / name).parent.mkdir(parents=True, exist_ok=True)
        (pstubs / name).write_text(text)
    (out / "glue.c").write_text(GLUE)
    # The harness includes the plugin's source, so it can look at its sites.
    (out / "harness.cpp").write_text(f'#include "{HOOKS / "hooks.cpp"}"\n' + HARNESS)
    glib = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0"],
                                               text=True))
    san = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-g", "-O1"]
    cc, cxx = os.environ.get("CC", "cc"), os.environ.get("CXX", "c++")
    objs = []
    for src in (SRC / "panda/src/events.c", out / "glue.c"):
        obj = out / (src.stem + ".o")
        subprocess.run([cc, "-std=gnu11", "-c", "-Wall", "-Werror", "-Wno-unused-parameter",
                        *san, "-I", str(cstubs), "-I", str(SRC / "include"), str(src), "-o",
                        str(obj), *glib], check=True)
        objs.append(str(obj))
    exe = out / "hooks_test"
    # -fpermissive and the warnings are the plugin build's (plugins/meson.build).
    subprocess.run([cxx, "-std=c++17", "-Wall", "-Werror", "-Wno-sign-compare",
                    "-Wno-invalid-offsetof", "-Wno-unused-parameter", "-fpermissive", *san,
                    "-I", str(pstubs), "-I", str(HOOKS), "-I", str(SRC / "include"),
                    str(out / "harness.cpp"), *objs, "-o", str(exe), *glib], check=True)
    return exe


def main():
    with tempfile.TemporaryDirectory(prefix="hooks-events-") as tmp:
        exe = build(Path(tmp))
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:abort_on_error=0",
                   UBSAN_OPTIONS="print_stacktrace=1")
        p = subprocess.run([str(exe)], capture_output=True, text=True, env=env)
        print(p.stdout, end="")             # the C checks print their own FAIL lines
        lines = p.stdout.splitlines()
        c_fails = [l for l in lines if l.startswith("FAIL ")]
        fails = []
        if p.returncode and not c_fails:
            fails.append(f"exit {p.returncode}")
        out = dict(l.split(" ", 1) for l in lines
                   if l.split(" ", 1)[0] in ("UUID", "SIG", "SIZE", "MISMATCH", "MISMATCH2"))
        parsed = er.parse_sig(SIGNATURE)
        if out.get("SIG") != SIGNATURE:
            fails.append(f"signature {out.get('SIG')!r}, want {SIGNATURE!r}")
        if parsed is None or out.get("SIZE") != str(parsed[2]):
            fails.append(f"struct size {out.get('SIZE')}, the signature's layout {parsed}")
        want = er.uuid_of(er.PANDA_NS, "hooks.hit", parsed[0] if parsed else "").hex
        if out.get("UUID") != want:
            fails.append(f"uuid {out.get('UUID')}, Python's uuid5 {want}")
        if "field 1 is tb:ptr<TranslationBlock>, was pc:u64" not in out.get("MISMATCH", ""):
            fails.append(f"mismatch reason: {out.get('MISMATCH')!r}")
        m2 = out.get("MISMATCH2", "")
        if "field 0 (cpu) is ptr<CPUState>, was ptr" not in m2 or \
                "field 1 (tb) is ptr<TranslationBlock>, was ptr" not in m2:
            fails.append(f"untyped mismatch reason: {m2!r}")
        # The shared vectors hold hooks.hit's identity as the header builds it.
        rows = [r for r in er.read_vectors() if uuid.UUID(r[0]) == er.PANDA_NS
                and r[1] == "hooks.hit"]
        if not rows or any(r[2] != SIGNATURE.rstrip(",") or r[4] != str(uuid.UUID(want))
                           for r in rows):
            fails.append(f"event-uuid-vectors.tsv hooks.hit rows {rows}, want "
                         f"{SIGNATURE.rstrip(',')} {uuid.UUID(want)}")
        for f in fails:
            print("FAIL", f)
        if p.returncode or fails:
            print(p.stderr[-4000:])
        n = len(c_fails) + len(fails)
        print(f"hooks events: {n} failures")
        return 1 if n else 0


if __name__ == "__main__":
    sys.exit(main())
