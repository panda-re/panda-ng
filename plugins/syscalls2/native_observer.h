/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* Types/functions come from the engine's flattened panda.h ABI. The generated
 * headers omit macros; keep only these public flag values here. */
#ifndef PANDA_SYSCALL_OBSERVER_X86_64
#define PANDA_SYSCALL_OBSERVER_X86_64 0
#define PANDA_SYSCALL_OBSERVER_ENTER 1
#define PANDA_SYSCALL_OBSERVER_RETURN 2
#endif

/* Thread-local dispatch frame, not fake guest registers. Generated ABI decoding
 * stays authoritative; only its lazy get_* sources use this immutable snapshot.
 */
struct Syscalls2NativeFrame {
    const PandaSyscallObservation *event;
    syscall_ctx_t *context;
    void *cookie = nullptr;
};
static thread_local Syscalls2NativeFrame *native_frame;
static bool native_backend;
#if defined(TARGET_X86_64)
#include "exact_thread_backend.h"
#endif

static void *native_enter(CPUState *cpu, const PandaSyscallObservation *event, void *)
{
    if (event->abi != PANDA_SYSCALL_OBSERVER_X86_64 || event->num < 0 ||
        event->num > INT_MAX || event->pc < 2) {
        return nullptr;
    }
    for (auto callback : preExecCallbacks) {
        callback(cpu, event->pc - 2);
    }
    Syscalls2NativeFrame frame = {event, nullptr};
    auto *previous = native_frame;
    native_frame = &frame;
    syscall_enter_switch_linux_x64(cpu, PROFILE_LINUX_X64, event->pc - 2, (int)event->num);
    native_frame = previous;
#if defined(TARGET_X86_64)
    if (exact_backend) return frame.cookie;
#endif
    return frame.context;
}

static void native_return(CPUState *cpu, const PandaSyscallObservation *event, void *cookie, void *)
{
#if defined(TARGET_X86_64)
    if (exact_backend) {
        if (!cookie) return;
        auto delivery = exact_parent_return(cpu, event, static_cast<ExactCookie *>(cookie));
        if (!delivery) return;
        Syscalls2NativeFrame frame = {event, nullptr};
        auto *previous = native_frame;
        native_frame = &frame;
        syscall_return_switch_linux_x64(cpu, event->pc, &delivery->payload);
        native_frame = previous;
        return;
    }
#endif
    // Core removes restart slots. No PPP completion; drop frees this context,
    // then a real retrap creates a fresh entry/context (never keep across keys).
    if (!cookie || event->restart) {
        return;
    }
    Syscalls2NativeFrame frame = {event, nullptr};
    auto *previous = native_frame;
    native_frame = &frame;
    syscall_return_switch_linux_x64(cpu, event->pc, static_cast<syscall_ctx_t *>(cookie));
    native_frame = previous;
}

static void native_drop(void *cookie, void *)
{
#if defined(TARGET_X86_64)
    if (exact_backend) {
        exact_drop(static_cast<ExactCookie *>(cookie));
        return;
    }
#endif
    delete static_cast<syscall_ctx_t *>(cookie);
}

#if defined(TARGET_X86_64)
static uint64_t exact_child_return(CPUState *cpu, uint64_t pc, void *opaque) {
    std::lock_guard<std::recursive_mutex> guard(exact_lock);
    uint64_t sequence = (uintptr_t)opaque;
    auto i = exact_records.find(sequence);
    if (i == exact_records.end() || !i->second.child_pending) return 0;
    OsiThreadIdentity id = {};
    ExactKey current;
    if (!exact_identity(cpu, &id, &current) || !i->second.child ||
        !(current == *i->second.child) || !id.asid_valid ||
        id.asid != i->second.child_asid || get_id(cpu) != i->second.child_asid) return 0;
    auto delivery = exact_tracker.take(current, pc);
    if (!delivery || delivery->invocation != sequence || !delivery->child) {
        exact_error("ambiguous child return; invocation discarded");
        exact_remove(sequence);
        return 0;
    }
    i->second.child_pending = false;
    if (i->second.child_hook) exact_unhook(i->second.child_hook);
    i->second.child_hook = 0;
    if (!i->second.parent_pending) exact_remove(sequence);
    PandaSyscallObservation event = {};
    event.abi = PANDA_SYSCALL_OBSERVER_X86_64;
    event.num = delivery->payload.no;
    event.pc = pc;
    event.ret = (int64_t)((CPUArchState *)panda_cpu_env(cpu))->regs[R_EAX];
    Syscalls2NativeFrame frame = {&event, nullptr};
    auto *previous = native_frame;
    native_frame = &frame;
    syscall_return_switch_linux_x64(cpu, pc, &delivery->payload);
    native_frame = previous;
    return 0; // observation only; never redirect kernel-hook execution
}
#endif

static bool native_accept_return(CPUState *cpu, const PandaSyscallObservation *event,
                                  void *cookie, void *) {
#if defined(TARGET_X86_64)
    if (exact_backend && cookie)
        return exact_accept(cpu, event, static_cast<ExactCookie *>(cookie));
#endif
    return true;
}
static const PandaSyscallObserverOps native_ops = {native_enter, native_return, native_drop, native_accept_return};

bool syscalls2_capture_native_context(CPUState *cpu, syscall_ctx_t *ctx)
{
    if (!native_frame) {
        return false; // default generated dispatcher unchanged
    }
#if defined(TARGET_X86_64)
    if (exact_backend) {
        native_frame->cookie = exact_capture(cpu, ctx);
        return true; // even rejection must never fall back to legacy correlation
    }
#endif
    // Keep the existing return hook for clone/fork families. Core keep only
    // retains parent CR3/RSP and cannot correlate a child's different context.
    // Include historical calls whose generated metadata does not mark double.
    if (ctx->double_return || ctx->no == 56 || ctx->no == 57 || ctx->no == 58 || ctx->no == 435) {
        return false;
    }
    native_frame->context = new syscall_ctx_t(*ctx);
    return true;
}
