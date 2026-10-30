#!/usr/bin/env python3
"""Compile actual observer bridge against fake engine boundary; no engine/guest build.
python3 -I tests/static/syscalls2/observer_test.py
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile


def function(text, name):
    match = re.search(r'^\w[^\n]*\b' + re.escape(name) + r'\([^;]*?\)\s*\{',
                      text, re.MULTILINE)
    assert match, name
    return text[match.start():text.index('\n}', match.end()) + 2]


ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "engine/src/panda/src/syscall-observer.c"
CORE = ROOT / "engine/src/system/syscall-trap.c"

with tempfile.TemporaryDirectory(prefix="sc-observer-") as tmp:
    out = Path(tmp)
    common = r'''
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
typedef struct CPUState { bool stopped; } CPUState;
static CPUState cpus[2];
static CPUState *first_cpu = &cpus[0];
static CPUState *current_cpu;
static bool bql = true, main_thread = true, running;
#define CPU_FOREACH(cpu) for (int idx=0; idx<2 && ((cpu)=&cpus[idx],1); ++idx)
static bool bql_locked(void) { return bql; }
static bool qemu_in_main_thread(void) { return main_thread; }
static bool cpu_is_stopped(CPUState *c) { return c->stopped; }
typedef struct { void *plugin; bool unload, exported_symbols; } panda_plugin;
enum { SYSCALL_ABI_X86_64, SYSCALL_TRAP_NUMS=8192, SYSCALL_TRAP_ENTER=1, SYSCALL_TRAP_RETURN=2 };
typedef struct SyscallTrap {
 int abi; int64_t num; uint64_t args[6], pc; int64_t ret;
 bool restart, want_return, keep; void *data;
} SyscallTrap;
typedef struct SyscallTrapOps {
 void (*enter)(CPUState *, SyscallTrap *, void *);
 void (*ret)(CPUState *, SyscallTrap *, void *);
 void (*drop)(SyscallTrap *, void *);
} SyscallTrapOps;
static bool syscall_trap_set_consumer(const SyscallTrapOps *, void *);
static bool syscall_trap_clear_consumer(const SyscallTrapOps *, void *);
static void syscall_trap_watch(int,int64_t,unsigned);
'''
    (out / "fake.h").write_text(common)
    for header in ("qemu/osdep.h", "qemu/main-loop.h", "hw/core/cpu.h", "system/cpus.h", "system/runstate.h", "system/syscall-trap.h", "panda/plugin.h"):
        path = out / header
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("")
    harness = r'''
#include "fake.h"
#include "BRIDGE"
int nb_panda_plugins = 2;
panda_plugin panda_plugins[2];
int panda_main_walk_depth;
static bool refuse_release;
static const SyscallTrapOps *active;
static void *active_opaque;
static SyscallTrap pending;
static int drops, entries, returns, watches;
static bool reentrant_register, reentrant_watch;
static int owner, other, value;
static void *enter(CPUState *cpu, const PandaSyscallObservation *event, void *opaque) {
 assert(event->num==123 && event->args[5]==600 && opaque==&value); ++entries; return &value;
}
static void ret(CPUState *cpu, const PandaSyscallObservation *event, void *cookie, void *opaque) {
 assert(cookie==&value && event->ret==-38); ++returns;
}
static const PandaSyscallObserverOps ops;
static void drop(void *cookie, void *opaque) {
 assert(cookie==&value && opaque==&value && panda_main_walk_depth > 0); ++drops;
 reentrant_register=panda_syscall_observer_register(&other,&ops,&value);
 reentrant_watch=panda_syscall_observer_watch(&owner,0,-1,3);
}
static const PandaSyscallObserverOps ops={enter,ret,drop};
static bool syscall_trap_set_consumer(const SyscallTrapOps *ops, void *opaque) {
 if(active && active!=ops) return false; active=ops;active_opaque=opaque;return true;
}
static bool syscall_trap_clear_consumer(const SyscallTrapOps *ops, void *opaque) {
 if(refuse_release || active!=ops || active_opaque!=opaque) return false;
 if(pending.data) { active->drop(&pending,NULL); pending.data=NULL; }
 active=NULL; return true;
}
static void syscall_trap_watch(int abi,int64_t num,unsigned flags) { ++watches; }
#define LOG_ERROR(...) ((void)0)
static int deleted, closed, resets_removed;
static void panda_unregister_transient_reset(void *owner) { resets_removed++; }
static void panda_delete_plugin(int i) { assert(!pending.data); deleted++; }
static int dlclose(void *owner) { assert(!active && !pending.data); closed++; return 0; }
CLOSE
int main(void) {
 panda_plugins[0].plugin=&owner; panda_plugins[1].plugin=&other;
 assert(panda_syscall_observer_unregister(&other));
 first_cpu=NULL;bql=false;main_thread=false;
 current_cpu=&cpus[0];
 assert(!panda_syscall_observer_register(&owner,&ops,&value));
 current_cpu=NULL;
 assert(panda_syscall_observer_register(&owner,&ops,&value));
 assert(panda_syscall_observer_watch(&owner,0,-1,3));
 assert(panda_syscall_observer_unregister(&owner));
 first_cpu=&cpus[0];
 assert(!panda_syscall_observer_register(&owner,&ops,&value));
 bql=true;main_thread=true;
 assert(!panda_syscall_observer_register(&owner,&ops,&value)); // active CPU, nonrunning runstate
 cpus[0].stopped=cpus[1].stopped=true;
 main_thread=false;assert(!panda_syscall_observer_register(&owner,&ops,&value));main_thread=true;
 bql=false;assert(!panda_syscall_observer_register(&owner,&ops,&value));bql=true;
 cpus[0].stopped=false;
 running=true;assert(!panda_syscall_observer_register(&owner,&ops,&value));
 cpus[0].stopped=cpus[1].stopped=true;
 assert(panda_syscall_observer_register(&owner,&ops,&value));
 assert(!panda_syscall_observer_register(&other,&ops,&value));
 assert(!panda_syscall_observer_watch(&other,0,-1,3));
 assert(!panda_syscall_observer_watch(&owner,1,-1,3));
 assert(!panda_syscall_observer_watch(&owner,0,-1,2)); // return needs entry cookie
 assert(panda_syscall_observer_watch(&owner,0,-1,3));
 pending.num=123;pending.args[5]=600;pending.want_return=true;
 active->enter(&cpus[0],&pending,NULL);assert(pending.data==&value && pending.want_return);
 pending.ret=-38;active->ret(&cpus[0],&pending,NULL);assert(returns==1);
 bql=false;assert(!panda_syscall_observer_unregister(&owner));bql=true;
 refuse_release=true;
 assert(!panda_syscall_observer_unregister(&owner));
 assert(active && observer_owner==&owner && pending.data==&value && !drops);
 assert(!panda_main_walk_depth && !observer_changing);
 refuse_release=false;
 assert(panda_syscall_observer_unregister(&owner));
 assert(drops==1 && !reentrant_register && !reentrant_watch && !active);
 assert(panda_syscall_observer_register(&owner,&ops,&value));
 pending.want_return=false;active->enter(&cpus[0],&pending,NULL);
 assert(!pending.want_return);assert(panda_syscall_observer_unregister(&owner));
 assert(drops==2);assert(panda_syscall_observer_unregister(&owner));
 assert(panda_syscall_observer_register(&owner,&ops,&value));
 pending.want_return=true;active->enter(&cpus[0],&pending,NULL);
 refuse_release=true;
 assert(!dlclose_plugin(0));
 assert(!deleted && !closed && !resets_removed && drops==2 && pending.data);
 refuse_release=false;
 assert(dlclose_plugin(0));
 assert(deleted==1 && closed==1 && resets_removed==1 && drops==3 && !pending.data);
 static const SyscallTrapOps conflicting={0};active=&conflicting;
 assert(!panda_syscall_observer_register(&owner,&ops,&value));
 return 0;
}
'''.replace("BRIDGE", str(SOURCE)).replace(
        "CLOSE", function((ROOT / "engine/src/panda/src/callbacks.c").read_text(),
                          "dlclose_plugin"))
    (out / "test.c").write_text(harness)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-misleading-indentation",
                    "-Wno-missing-field-initializers",  # exercise legacy three-field source initializer
                    "-I", str(out), "-I", str(ROOT / "engine/src/include"), str(out / "test.c"), "-o", str(out / "test")], check=True)
    subprocess.run([str(out / "test")], check=True)
    print("PASS: actual observer bridge preinit/ownership/quiescence, cookie drain, refusal, real dlclose ordering and conflict")

    # Use the real bridge plus the real syscall_trap_return removal/keep/drop
    # path: a bridge-only test would miss accidental cookie destruction.
    retain = r'''
#include "fake.h"
#include "BRIDGE"
int nb_panda_plugins;
panda_plugin panda_plugins[1];
int panda_main_walk_depth;
static bool syscall_trap_set_consumer(const SyscallTrapOps *p, void *o) { return true; }
static bool syscall_trap_clear_consumer(const SyscallTrapOps *p, void *o) { return true; }
static void syscall_trap_watch(int a, int64_t n, unsigned f) {}
static unsigned accepted, completed, freed;
static int cookie;
static bool accept(CPUState *cpu, const PandaSyscallObservation *e, void *p, void *o) {
    assert(p == &cookie); accepted++; return cpu == &cpus[0];
}
static void complete(CPUState *cpu, const PandaSyscallObservation *e, void *p, void *o) {
    assert(p == &cookie); completed++;
}
static void release(void *p, void *o) { assert(p == &cookie); freed++; }
'''.replace("BRIDGE", str(SOURCE))
    # Core state machinery is stubbed, but the ownership decisions below are
    # extracted verbatim from the real return function, including restart logic.
    core_return = function(CORE.read_text(), "syscall_trap_return")
    start = core_return.index('    sc.keep = false;')
    end = core_return.index('    if (!restart && sc.ret != v)')
    ownership = core_return[start:end]
    assert 'if (sc.keep && !restart)' in ownership
    retain += r'''
static unsigned stored;
static bool replay_analysis_write_allowed(void) { return false; }
static const SyscallTrapOps *ops = &trap_observer_ops;
static void *sc_opaque;
static void sc_add(uint64_t k1, uint64_t k2, uint64_t key, unsigned len,
                   SyscallTrap *sc, int state) { stored++; }
static void sc_drop(SyscallTrap *sc) { ops->drop(sc, NULL); sc->data = NULL; }
static int sc_hold(uint64_t a, uint64_t b, uint64_t c, unsigned d, SyscallTrap *sc, int s) { assert(false); return 0; }
static int sc_ret_after(CPUState *cpu, uint64_t a, uint64_t b, uint64_t key,
                        unsigned len, SyscallTrap *sc, int64_t n, uint64_t *args,
                        bool restore, uint64_t *ret, void *rw) { assert(false); return 0; }
enum { SC_PENDING, SC_HELD_RETURN };
static int core_finish(CPUState *cpu, SyscallTrap sc) {
    uint64_t k1=0,k2=0,key=0,ret_value=0,*ret=&ret_value;
    unsigned len=2;
    bool restart=sc.restart;
    int64_t v=sc.ret;
    void *rw=NULL;
    uint32_t kmmu=0;
OWNERSHIP
    return 0;
}
int main(void) {
    observer_ops.ret = complete; observer_ops.drop = release;
    observer_ops.accept_return = accept;
    SyscallTrap sc = {.data=&cookie};
    core_finish(&cpus[1], sc); /* child/wrong exact thread */
    assert(accepted==1 && completed==0 && freed==0 && stored==1);
    core_finish(&cpus[0], sc); /* parent consumes retained invocation */
    assert(accepted==2 && completed==1 && freed==1 && stored==1);
    core_finish(&cpus[1], sc); /* rejection, then external cancellation */
    assert(freed==1 && stored==2);
    observer_drop(&sc,NULL);
    assert(freed==2);
    sc.restart=true;
    core_finish(&cpus[1],sc); /* restart bypasses acceptance, drops once */
    assert(accepted==3 && completed==2 && freed==3 && stored==2);
    observer_ops.accept_return=NULL; sc.restart=false;
    core_finish(&cpus[1],sc); /* legacy trailing NULL accepts */
    assert(completed==3 && freed==4);
    return 0;
}
'''.replace('OWNERSHIP', ownership)
    # Extend the fake boundary only for fields the extracted core tail accesses.
    retain = retain.replace('cpu->syscall_kmmu', 'stub_kmmu')
    retain = retain.replace('static unsigned stored;', 'static uint32_t stub_kmmu; static unsigned stored;')
    (out / "fake.h").write_text(common.replace('bool restart, want_return, keep;',
        'bool restart, want_return, keep, inject, fault;'))
    (out / "retain.c").write_text(retain)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-I", str(out),
                    "-I", str(ROOT / "engine/src/include"), str(out / "retain.c"),
                    "-o", str(out / "retain")], check=True)
    subprocess.run([str(out / "retain")], check=True)
    print("PASS: actual core return keep/drop path retains wrong-task cookie, accepts parent, cancels once, preserves restart/NULL predicate semantics")

    callbacks = (ROOT / "engine/src/panda/src/callbacks.c").read_text()
    unload = function(callbacks, "panda_do_unload_plugin")
    assert unload.index("panda_syscall_observer_unregister") < unload.index("dlsym")
    assert "if (!dlclose_plugin(nb_panda_plugins - 1))" in callbacks
    marked = function(callbacks, "panda_unload_marked")
    marked_test = r'''
#include <stdbool.h>
#include <assert.h>
static int nb_panda_plugins = 2, visits[2];
static struct { bool unload; } panda_plugins[] = {{true}, {true}};
static bool panda_plugin_to_unload = true;
static bool panda_vcpus_pause(void) { return true; }
static void panda_vcpus_resume(bool paused) { assert(paused); }
static void panda_do_unload_plugin(int i) { visits[i]++; /* refused, retain */ }
'''
    marked_test += marked + r'''
int main(void) {
    panda_unload_marked();
    assert(visits[0] == 1 && visits[1] == 1 && !panda_plugin_to_unload);
    assert(nb_panda_plugins == 2);
}
'''
    (out / "marked.c").write_text(marked_test)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", str(out / "marked.c"),
                    "-o", str(out / "marked")], check=True)
    subprocess.run([str(out / "marked")], check=True, timeout=10)
    print("PASS: refused unload visits each owner once, retains registry and clears pending marker")

    # Compile the actual hooks removal helper: preserve unrelated queued/live
    # records while removing every matching callback (no TB execution here).
    helper = function((ROOT / "plugins/hooks/hooks.cpp").read_text(), "remove_hooks_by_callback")
    hooks_test = r'''
#include <algorithm>
#include <cassert>
#include <vector>
#include <unordered_map>
#include <set>
struct CPUState {};
struct TranslationBlock {};
struct hook {
    int type, id;
    struct { void (*before_tcg_codegen)(CPUState *, TranslationBlock *, hook *); } cb;
    bool operator<(const hook &other) const { return id < other.id; }
};
#define PANDA_CB_BEFORE_TCG_CODEGEN 7
static std::vector<hook> per_guest_cpu_temp_hooks[16];
static std::unordered_map<unsigned, std::set<hook>> per_guest_cpu_hooks[16];
static unsigned flushed;
static void panda_do_flush_tb(void) { flushed++; }
static void owner(CPUState *, TranslationBlock *, hook *) {}
static void other(CPUState *, TranslationBlock *, hook *) {}
'''
    hooks_test += helper + r'''
int main() {
    for (int i=0; i<16; i++) {
        hook ours = {7, 1, {owner}}, theirs = {7, 2, {other}};
        per_guest_cpu_temp_hooks[i] = {ours, theirs};
        per_guest_cpu_hooks[i][123] = {ours, theirs};
    }
    remove_hooks_by_callback(owner);
    for (int i=0; i<16; i++) {
        assert(per_guest_cpu_temp_hooks[i].size() == 1);
        assert(per_guest_cpu_temp_hooks[i][0].cb.before_tcg_codegen == other);
        assert(per_guest_cpu_hooks[i][123].size() == 1);
        assert(per_guest_cpu_hooks[i][123].begin()->cb.before_tcg_codegen == other);
    }
    assert(flushed == 1);
    remove_hooks_by_callback(owner);
    assert(flushed == 2);
}
'''
    (out / "hooks.cpp").write_text(hooks_test)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", str(out / "hooks.cpp"),
                    "-o", str(out / "hooks")], check=True)
    subprocess.run([str(out / "hooks")], check=True)
    print("PASS: actual hooks helper removes matching queued/live records, preserves unrelated owners, is idempotent")

    source = (ROOT / "plugins/syscalls2/syscalls2.cpp").read_text()
    reset = function(source, "syscalls2_reset_transient")
    assert "panda_register_transient_reset(self, syscalls2_reset_transient, nullptr)" in source
    assert function(source, "uninit_plugin").count("syscalls2_reset_transient(nullptr)") == 1
    reset_test = r'''
#include <cassert>
#include <cstring>
#include <map>
struct CPUState {};
struct TranslationBlock {};
struct hook {};
static std::map<int,int> running_syscalls;
static bool loaded = true, symbol = true, native_backend = true;
static int subscription_count = 7, removed, lookups;
static void hook_syscall_return(CPUState *, TranslationBlock *, hook *) {}
static void *panda_get_plugin_by_name(const char *name) {
    assert(!strcmp(name, "hooks")); lookups++;
    return loaded ? (void *)1 : nullptr;
}
static void remove(void (*callback)(CPUState *, TranslationBlock *, hook *)) {
    assert(callback == hook_syscall_return); removed++;
}
static void *dlsym(void *owner, const char *name) {
    assert(owner && !strcmp(name, "remove_hooks_by_callback"));
    return symbol ? (void *)remove : nullptr;
}
'''
    reset_test += reset + r'''
int main() {
    for (bool native : {false, true}) {
        native_backend = native;
        running_syscalls[1] = 2;
        syscalls2_reset_transient(nullptr);
        assert(running_syscalls.empty() && native_backend == native);
        assert(subscription_count == 7);
    }
    assert(removed == 2 && lookups == 2);
    syscalls2_reset_transient(nullptr); // idempotent
    assert(removed == 3);
    loaded = false;
    running_syscalls[3] = 4;
    syscalls2_reset_transient(nullptr);
    assert(running_syscalls.empty() && removed == 3 && lookups == 4);
    loaded = true; symbol = false;
    running_syscalls[5] = 6;
    syscalls2_reset_transient(nullptr);
    assert(running_syscalls.empty() && removed == 3 && lookups == 5);
}
'''
    (out / "reset.cpp").write_text(reset_test)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", str(out / "reset.cpp"),
                    "-o", str(out / "reset")], check=True)
    subprocess.run([str(out / "reset")], check=True)
    print("PASS: actual syscalls2 reset clears legacy/native fallback maps, re-resolves live hooks, preserves backend/subscriptions and tolerates unloaded hooks")
