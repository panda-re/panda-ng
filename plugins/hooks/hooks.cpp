/* PANDABEGINCOMMENT
 *
 * Authors:
 *  Luke Craig                  luke.craig@ll.mit.edu
 *  Andrew Fasano               andrew.fasano@ll.mit.edu
 *  Nick Gregory                ngregory@nyu.edu
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 * See the COPYING file in the top-level directory.
 *
PANDAENDCOMMENT */

/*
 * add_hook() on the engine's address hooks (panda/address-hook.h).
 *
 * Each hooked address is a site: one engine hook whose callback (on_hit) walks the hooks added
 * there and filters them in C, by asid (proc_id()) and kernel/user mode, before calling
 * each one's function and emitting hooks.hit (hooks_events.h). The hooks at an address are an
 * array that writers replace and vCPUs read without a lock: the vCPUs run hooks inside an RCU
 * read-side critical section, so a replaced array and a removed hook are freed by the engine after
 * a grace period (panda_free_rcu). Nothing is allocated on a hit.
 *
 * A site stays for the plugin's lifetime once its address is hooked; its engine hook is removed
 * while it has no hooks, and inserted again (with no invalidation) when it gets one.
 */

// This needs to be defined before anything is included in order to get
// the PRIx64 macro
#define __STDC_FORMAT_MACROS

#include "panda.h"
#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

// These need to be extern "C" so that the ABI is compatible with
// QEMU/PANDA, which is written in C
extern "C" {
bool init_plugin(void *);
void uninit_plugin(void *);
#include "hooks_int_fns.h"
#include "hw_proc_id/hw_proc_id_ext.h"
#include "panda/address-hook.h"
}
#include "hooks_events.h"

// The hooks at one address, as published to the vCPUs. Never changed once published.
struct hook_list {
    uint32_t n;
    bool any_asid;          // a hook filters by asid: on_hit needs get_id()
    bool any_mode;          // a hook filters by kernel/user mode
    bool direct;            // one hook, no filter: on_hit's short path
    struct hook *h[];       // the plugin's copies: hooks with an asid first, then asid 0 ones
};

struct hook_site {
    uint64_t addr;
    PandaAddressHook *engine;       // NULL while the site has no hooks (writers only)
    hook_list *list;                // NULL when empty; atomic: read by the vCPUs
};

static std::mutex writers;                              // sites, their lists and engine hooks
static std::unordered_map<uint64_t, hook_site *> sites;
static bool hooking = true;                             // atomic
static panda_event_id hit_event;

// Handle to self
void* self = NULL;

static void on_hit(CPUState *cpu, uint64_t pc, void *opaque);

/*
 * The identifier a hook's asid is compared with: hw_proc_id's get_id(). Outside MIPS that is
 * panda_current_asid(), called directly. On MIPS hw_proc_id caches the task pointer from every
 * kernel block (a start_block_exec callback, which costs every block on every run), so it is
 * loaded only once a hook with an asid is added.
 */
static inline target_ulong proc_id(CPUState *cpu)
{
#if defined(TARGET_MIPS)
    return get_id(cpu);
#else
    return panda_current_asid(cpu);
#endif
}

#if defined(TARGET_MIPS)
static bool have_proc_id;                               // hw_proc_id is loaded (writers)

// With writers held. False if hw_proc_id is not loaded and cannot be loaded from this thread.
static bool need_proc_id(void)
{
    if (have_proc_id) {
        return true;
    }
    if (!panda_get_plugin_by_name("hw_proc_id")) {
        if (current_cpu) {
            // loading a plugin from a vCPU thread is not safe
            return false;
        }
        panda_require("hw_proc_id");
    }
    have_proc_id = init_hw_proc_id_api();
    return have_proc_id;
}
#endif

// A hook's enabled flag: written by its function (on any vCPU) or by whoever kept a pointer to it.
static inline bool hook_enabled(const struct hook *h)
{
    return __atomic_load_n(&h->enabled, __ATOMIC_RELAXED);
}

static hook_list *list_new(uint32_t n)
{
    hook_list *l = (hook_list *)calloc(1, sizeof(hook_list) + n * sizeof(struct hook *));
    assert(l != NULL);
    l->n = n;
    return l;
}

static void list_finish(hook_list *l)
{
    for (uint32_t i = 0; i < l->n; i++) {
        l->any_asid |= l->h[i]->asid != 0;
        l->any_mode |= l->h[i]->km != MODE_ANY;
    }
    l->direct = l->n == 1 && !l->any_asid && !l->any_mode;
}

// Publish @l (NULL if empty) as @s's hooks, with writers held. The old array is freed after the
// vCPUs that may be reading it are done. The engine hook follows: removed while there are none.
static void site_publish(hook_site *s, hook_list *l)
{
    hook_list *old = s->list;

    __atomic_store_n(&s->list, l, __ATOMIC_RELEASE);
    panda_free_rcu(old);
    if (l && !s->engine) {
        s->engine = panda_address_hook_insert(s->addr, on_hit, s);
        if (!s->engine) {
            fprintf(stderr, "hooks: cannot hook 0x%" PRIx64 "\n", s->addr);
        }
    } else if (!l && s->engine) {
        panda_address_hook_remove(s->engine);
        s->engine = NULL;
    }
}

// Drop @s's disabled hooks. From on_hit, which saw one: another vCPU may have dropped it already,
// so this works on the current array, not on the one the caller read.
static void site_drop_disabled(hook_site *s)
{
    std::lock_guard<std::mutex> g(writers);
    hook_list *cur = s->list;

    if (!cur) {
        return;
    }
    // A hook function on another vCPU may disable a hook while this runs, without the lock:
    // read each flag once, and keep what that read said.
    std::vector<struct hook *> kept;
    kept.reserve(cur->n);
    for (uint32_t i = 0; i < cur->n; i++) {
        if (hook_enabled(cur->h[i])) {
            kept.push_back(cur->h[i]);
        }
    }
    if (kept.size() == cur->n) {
        return;
    }
    hook_list *l = kept.empty() ? NULL : list_new(kept.size());
    for (uint32_t i = 0, j = 0; i < cur->n; i++) {
        if (j < kept.size() && kept[j] == cur->h[i]) {
            l->h[j++] = cur->h[i];
        } else {
            panda_free_rcu(cur->h[i]);      // a vCPU may still be in its function
        }
    }
    if (l) {
        list_finish(l);
    }
    site_publish(s, l);
}

// Call @h's function: every hook runs right before the instruction at @pc.
static inline void call_hook(CPUState *cpu, TranslationBlock *tb, uint64_t pc, struct hook *h)
{
    if (h->cb) {
        h->cb(cpu, tb, h);
    }
}

// The engine hook of a site, before the instruction at its address. @pc is the site's address.
static void on_hit(CPUState *cpu, uint64_t pc, void *opaque)
{
    hook_site *s = (hook_site *)opaque;
    hook_list *l = __atomic_load_n(&s->list, __ATOMIC_ACQUIRE);
    TranslationBlock *tb = NULL;
    bool drop = false;

    if (!l || !__atomic_load_n(&hooking, __ATOMIC_RELAXED)) {
        return;
    }
    if (l->direct) {
        // One hook without a filter, the common case: no loop, no filter, no flags.
        struct hook *h = l->h[0];
        if (hook_enabled(h)) {
            tb = (TranslationBlock *)panda_address_hook_tb(cpu);
            call_hook(cpu, tb, pc, h);
            if (panda_event_wanted(hit_event)) {
                struct panda_hooks_hit_args a = {sizeof(a), cpu, tb, pc, h};
                panda_event_emit(hit_event, &a);
            }
            if (hook_enabled(h)) {
                return;
            }
        }
        site_drop_disabled(s);
        return;
    }
    target_ulong asid = l->any_asid ? proc_id(cpu) : 0;
    bool in_kernel = l->any_mode ? panda_in_kernel_external(cpu) : false;
    for (uint32_t i = 0; i < l->n; i++) {
        struct hook *h = l->h[i];
        if (!hook_enabled(h)) {
            drop = true;                    // disabled since it was added: removed below
            continue;
        }
        if (h->asid != 0 && h->asid != asid) {
            continue;
        }
        if (!(h->km == MODE_ANY || (in_kernel && h->km == MODE_KERNEL_ONLY) ||
              (!in_kernel && h->km == MODE_USER_ONLY))) {
            continue;
        }
        if (!tb) {
            tb = (TranslationBlock *)panda_address_hook_tb(cpu);
        }
        call_hook(cpu, tb, pc, h);
        if (panda_event_wanted(hit_event)) {
            struct panda_hooks_hit_args a = {sizeof(a), cpu, tb, pc, h};
            panda_event_emit(hit_event, &a);
        }
        drop |= !hook_enabled(h);               // a hook disabled in its function is removed
    }
    if (drop) {
        site_drop_disabled(s);
    }
}

// Enable and disable callbacks
void enable_hooking() {
    __atomic_store_n(&hooking, true, __ATOMIC_RELAXED);
}

void disable_hooking() {
    __atomic_store_n(&hooking, false, __ATOMIC_RELAXED);
}

// The same hook: every field the caller sets but addr and enabled. km and context count: the same
// function with another mode or context is another hook.
static bool same_hook(const struct hook *a, const struct hook *b)
{
    return a->asid == b->asid && a->km == b->km && a->context == b->context && a->cb == b->cb;
}

void add_hook(struct hook* h) {
    // A disabled hook never runs, and nothing can enable the plugin's copy: there is nothing to
    // add.
    if (h == NULL || !h->enabled) {
        return;
    }
    std::lock_guard<std::mutex> g(writers);
#if defined(TARGET_MIPS)
    if (h->asid != 0 && !need_proc_id()) {
        fprintf(stderr, "hooks: a hook with an asid needs hw_proc_id on MIPS; load it before the "
                "guest runs, or add the first such hook from outside a vCPU. Hook at 0x%" PRIx64
                " not added\n", h->addr);
        return;
    }
#endif
    hook_site *&s = sites[h->addr];
    if (s == NULL) {
        s = (hook_site *)calloc(1, sizeof(*s));
        assert(s != NULL);
        s->addr = h->addr;
    }
    hook_list *cur = s->list;
    uint32_t n = cur ? cur->n : 0, at = n;

    // The same hook at the same address is added once (syscalls2 adds its return hook on every
    // system call).
    for (uint32_t i = 0; i < n; i++) {
        if (hook_enabled(cur->h[i]) && same_hook(cur->h[i], h)) {
            return;
        }
    }
    struct hook *copy = (struct hook *)malloc(sizeof(*copy));
    assert(copy != NULL);
    *copy = *h;
    if (copy->asid != 0) {
        // after the other hooks with an asid, before the asid 0 ones
        for (at = 0; at < n && cur->h[at]->asid != 0; at++) {
        }
    }
    hook_list *l = list_new(n + 1);
    for (uint32_t i = 0, j = 0; i <= n; i++) {
        l->h[i] = i == at ? copy : cur->h[j++];
    }
    list_finish(l);
    site_publish(s, l);
}

bool init_plugin(void *_self) {
    self = _self;

    panda_event_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.size = sizeof(desc);
    desc.ns = &PANDA_EVENT_NS;
    desc.name = PANDA_HOOKS_HIT_NAME;
    desc.signature = PANDA_HOOKS_HIT_SIGNATURE;
    desc.args_size = sizeof(struct panda_hooks_hit_args);
    desc.modes = PANDA_MODE_ANALYSIS | PANDA_MODE_FUZZ | PANDA_MODE_RR;     // TCG: not KVM
    desc.capability = 0;
    desc.flags = 0;                                                         // observers only
    hit_event = panda_event_declare(self, &desc);
    if (hit_event == NULL) {
        fprintf(stderr, "hooks: hooks.hit not declared; hooks still run\n");
    }
    return true;
}

void uninit_plugin(void *self) {
    // The vCPUs are paused while a plugin unloads: no hook is running. Remove the engine hooks so
    // translated code no longer calls into this plugin, and leave the frees to the engine.
    disable_hooking();
    std::lock_guard<std::mutex> g(writers);
    for (auto &it : sites) {
        hook_site *s = it.second;
        if (s->list) {
            for (uint32_t i = 0; i < s->list->n; i++) {
                panda_free_rcu(s->list->h[i]);
            }
        }
        site_publish(s, NULL);
        panda_free_rcu(s);
    }
    sites.clear();
    hit_event = NULL;
}
