/* SPDX-License-Identifier: GPL-2.0-or-later
 * Opt-in Linux x64 adapter. All kernel sites are user-validated runtime VAs;
 * this file never resolves guessed symbols or infers parentage from retpc.
 */
#pragma once
#include "thread_tracker.h"
#include "osi/osi_types.h"
#include <map>
#include <cstdlib>
#include <mutex>

static bool exact_backend;
static uint64_t exact_epoch = 1, exact_errors;
static std::recursive_mutex exact_lock;
static syscalls2::ThreadReturnTracker<syscall_ctx_t> exact_tracker(4096, 1);
using ExactKey = syscalls2::ThreadKey;
using ExactCurrent = bool (*)(CPUState *, OsiThreadIdentity *);
using ExactByTask = bool (*)(CPUState *, target_ptr_t, OsiThreadIdentity *);
using ExactHookCallback = uint64_t (*)(CPUState *, uint64_t, void *);
static ExactCurrent exact_current;
static ExactByTask exact_by_task;
static uint64_t (*exact_hook)(uint64_t, ExactHookCallback, void *);
static bool (*exact_unhook)(uint64_t);
static uint64_t exact_birth_site, exact_death_site, exact_exec_site;
static uint64_t exact_kernel_hooks[3];

struct ExactCookie {
    uint64_t sequence;
    bool completed = false;
};
struct ExactRecord {
    ExactKey origin;
    uint64_t retpc;
    bool spawn;
    bool parent_pending = true;
    bool child_pending = false;
    uint64_t child_hook = 0;
    std::optional<ExactKey> child;
    target_ptr_t child_asid = 0;
};
static std::map<uint64_t, ExactRecord> exact_records;

static void exact_error(const char *message) {
    ++exact_errors;
    if (exact_errors <= 8) fprintf(stderr, "syscalls2 exact-thread: %s\n", message);
}

static bool exact_identity(CPUState *cpu, OsiThreadIdentity *id,
                           ExactKey *key, target_ptr_t task = 0) {
    bool ok = task ? exact_by_task(cpu, task, id) : exact_current(cpu, id);
    if (!ok || !id->task || !id->lifetime_valid ||
        id->lifetime_format == OSI_THREAD_LIFETIME_UNAVAILABLE) {
        exact_error("checked thread identity unavailable; refusing correlation");
        return false;
    }
    *key = {exact_epoch, id->task, id->lifetime, (uint32_t)id->lifetime_format};
    return true;
}

static void exact_remove(uint64_t sequence) {
    auto i = exact_records.find(sequence);
    if (i != exact_records.end()) {
        if (i->second.child_hook) exact_unhook(i->second.child_hook);
        exact_records.erase(i);
    }
    exact_tracker.drop(sequence);
}

/* Called only while register state is synchronized by penguin_kernel_hook.
 * Native return switch reads the immutable synthetic event via native_frame. */
static uint64_t exact_child_return(CPUState *cpu, uint64_t pc, void *opaque);

static ExactCookie *exact_capture(CPUState *cpu, const syscall_ctx_t *ctx) {
    std::lock_guard<std::recursive_mutex> guard(exact_lock);
    OsiThreadIdentity id = {};
    ExactKey origin;
    if (!exact_identity(cpu, &id, &origin)) return nullptr;
    bool spawn = ctx->double_return || ctx->no == 56 || ctx->no == 57 ||
                 ctx->no == 58 || ctx->no == 435;
    auto sequence = exact_tracker.begin(origin, ctx->retaddr, *ctx, spawn);
    if (!sequence) {
        exact_error("return tracker full or identity invalid");
        return nullptr;
    }
    exact_records.emplace(*sequence, ExactRecord{origin, ctx->retaddr, spawn});
    return new ExactCookie{*sequence};
}

static bool exact_accept(CPUState *cpu, const PandaSyscallObservation *,
                          ExactCookie *cookie) {
    std::lock_guard<std::recursive_mutex> guard(exact_lock);
    auto i = exact_records.find(cookie->sequence);
    if (i == exact_records.end()) return true; // stale cookie: completion is a no-op
    OsiThreadIdentity id = {};
    ExactKey current;
    return exact_identity(cpu, &id, &current) && current == i->second.origin;
}

static std::optional<syscalls2::ThreadReturnTracker<syscall_ctx_t>::Delivery>
exact_parent_return(CPUState *cpu, const PandaSyscallObservation *event,
                    ExactCookie *cookie) {
    std::lock_guard<std::recursive_mutex> guard(exact_lock);
    if (event->restart) {
        exact_remove(cookie->sequence);
        return std::nullopt;
    }
    auto i = exact_records.find(cookie->sequence);
    if (i == exact_records.end()) return std::nullopt;
    OsiThreadIdentity id = {};
    ExactKey current;
    if (!exact_identity(cpu, &id, &current) || !(current == i->second.origin)) {
        exact_error("parent completion identity mismatch");
        return std::nullopt;
    }
    if (i->second.retpc != event->pc || !i->second.parent_pending) {
        exact_error("parent cookie return site mismatch");
        exact_remove(cookie->sequence);
        return std::nullopt;
    }
    auto delivery = exact_tracker.take(current, event->pc);
    if (!delivery || delivery->invocation != cookie->sequence || delivery->child) {
        exact_error("ambiguous or absent parent return; invocation discarded");
        exact_remove(cookie->sequence);
        return std::nullopt;
    }
    if (i->second.spawn && event->ret > 0 && !i->second.child) {
        exact_error("successful spawn missing validated birth; child delivery unavailable");
    }
    cookie->completed = true;
    i->second.parent_pending = false;
    if (!i->second.child_pending) exact_remove(cookie->sequence);
    return delivery;
}

static void exact_drop(ExactCookie *cookie) {
    if (!cookie) return;
    std::lock_guard<std::recursive_mutex> guard(exact_lock);
    auto i = exact_records.find(cookie->sequence);
    if (!cookie->completed && i != exact_records.end() && i->second.parent_pending)
        exact_remove(cookie->sequence);
    delete cookie;
}

/* wake_up_new_task(child): current is the actual creator. Ambiguous pending
 * creator invocations are not resolved by "newest", return PC or real_parent. */
static uint64_t exact_birth(CPUState *cpu, uint64_t, void *) {
    std::lock_guard<std::recursive_mutex> guard(exact_lock);
    OsiThreadIdentity parent_id = {}, child_id = {};
    ExactKey parent, child;
    if (!exact_identity(cpu, &parent_id, &parent)) return 0;
    CPUArchState *env = (CPUArchState *)panda_cpu_env(cpu);
    if (!exact_identity(cpu, &child_id, &child, env->regs[R_EDI])) return 0;
    if (!child_id.mm_valid || !child_id.asid_valid || !child_id.asid) {
        exact_error("child has no non-wildcard checked ASID");
        return 0;
    }
    auto found = exact_records.end();
    for (auto i = exact_records.begin(); i != exact_records.end(); ++i) {
        if (i->second.origin == parent && i->second.spawn && i->second.parent_pending) {
            if (found != exact_records.end()) {
                exact_error("ambiguous creator invocation at observed birth");
                return 0;
            }
            found = i;
        }
    }
    if (found == exact_records.end()) {
        exact_error("observed birth has no pending creator invocation");
        return 0;
    }
    if (!exact_tracker.birth(found->first, parent, child)) {
        exact_error("birth relationship rejected");
        return 0;
    }
    /* Hook is address-global, but callback admits only this exact observed
     * child's identity AND current nonzero ASID; no wildcard return matching. */
    found->second.child_pending = true;
    found->second.child = child;
    found->second.child_asid = child_id.asid;
    found->second.child_hook = exact_hook(found->second.retpc, exact_child_return,
                                         (void *)(uintptr_t)found->first);
    if (!found->second.child_hook) {
        exact_error("synchronized child hook unavailable; invocation discarded");
        exact_remove(found->first);
    }
    return 0;
}

static uint64_t exact_invalidate(CPUState *cpu, uint64_t, void *) {
    std::lock_guard<std::recursive_mutex> guard(exact_lock);
    OsiThreadIdentity id = {};
    ExactKey current;
    if (!exact_identity(cpu, &id, &current)) {
        /* Without identity, retaining any old invocation is unsafe. */
        while (!exact_records.empty()) exact_remove(exact_records.begin()->first);
        return 0;
    }
    exact_tracker.death(current); // exec/death invalidate this exact thread's branches
    for (auto i = exact_records.begin(); i != exact_records.end();) {
        uint64_t sequence = i++->first;
        auto &record = exact_records.at(sequence);
        if (record.origin == current) record.parent_pending = false;
        if (record.child && *record.child == current) {
            record.child_pending = false;
            if (record.child_hook) exact_unhook(record.child_hook);
            record.child_hook = 0;
        }
        if (!record.parent_pending && !record.child_pending) exact_remove(sequence);
    }
    return 0;
}

static void exact_reset(void) {
    std::lock_guard<std::recursive_mutex> guard(exact_lock);
    while (!exact_records.empty()) exact_remove(exact_records.begin()->first);
    if (++exact_epoch == 0) abort();
    exact_tracker.reset(exact_epoch);
}

static void exact_shutdown(void) {
    exact_reset();
    for (auto &hook : exact_kernel_hooks) {
        if (hook) exact_unhook(hook);
        hook = 0;
    }
    exact_backend = false;
}
