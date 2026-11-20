/*
 * syscalls2's compatibility layer: the exported ppp_add_cb_* / ppp_remove_cb_* functions of every
 * callback (generated/syscall_ppp_boilerplate_*.cpp), on the subscriber lists (sc2_subs.h). A
 * positional callback (on_sys_read_enter(cpu, pc, fd, buf, count)) is called through the invoker
 * of its type, which converts the decoded arguments a[] to that type's; the compiler emits one
 * invoker per distinct type.
 */
#pragma once
#include <cstddef>
#include <utility>
#include "sc2_subs.h"

/* The invokers: one per callback type (the compiler emits one per distinct type). */
template <typename F> struct sc2_sig;
template <typename... A> struct sc2_sig<void (*)(CPUState *, target_ulong, A...)> {
    typedef void (*F)(CPUState *, target_ulong, A...);
    static constexpr int nargs = sizeof...(A);
    template <std::size_t... I>
    static inline void call(sc2_fn fn, CPUState *cpu, target_ulong pc, const uint64_t *a,
                            std::index_sequence<I...>)
    {
        ((F)fn)(cpu, pc, (A)a[I]...);
    }
    static SC2_HOT void inv(sc2_fn fn, void *, CPUState *cpu, target_ulong pc, const uint64_t *a)
    {
        call(fn, cpu, pc, a, std::index_sequence_for<A...>{});
    }
    static sc2_fn ptr() { return (sc2_fn)&inv; }
};
template <typename... A> struct sc2_sig<void (*)(void *, CPUState *, target_ulong, A...)> {
    typedef void (*F)(void *, CPUState *, target_ulong, A...);
    static constexpr int nargs = sizeof...(A);
    template <std::size_t... I>
    static inline void call(sc2_fn fn, void *ctx, CPUState *cpu, target_ulong pc,
                            const uint64_t *a, std::index_sequence<I...>)
    {
        ((F)fn)(ctx, cpu, pc, (A)a[I]...);
    }
    static SC2_HOT void inv(sc2_fn fn, void *ctx, CPUState *cpu, target_ulong pc, const uint64_t *a)
    {
        call(fn, ctx, cpu, pc, a, std::index_sequence_for<A...>{});
    }
    static sc2_fn ptr() { return (sc2_fn)&inv; }
};
template <typename F> struct sc2_sig_nr;
template <> struct sc2_sig_nr<sc2_plain_nr> {
    static sc2_fn ptr() { return nullptr; }
};
template <> struct sc2_sig_nr<void (*)(void *, CPUState *, target_ulong, target_ulong)> {
    typedef void (*F)(void *, CPUState *, target_ulong, target_ulong);
    static SC2_HOT void inv(sc2_fn fn, void *ctx, CPUState *cpu, target_ulong pc, target_ulong no)
    {
        ((F)fn)(ctx, cpu, pc, no);
    }
    static sc2_fn ptr() { return (sc2_fn)&inv; }
};
template <typename F> struct sc2_sig_ctx;
template <typename I, typename C> struct sc2_sig_ctx<void (*)(CPUState *, target_ulong, I, C)> {
    typedef void (*F)(CPUState *, target_ulong, I, C);
    static SC2_HOT void inv(sc2_fn fn, void *, CPUState *cpu, target_ulong pc, const void *info,
                    const void *sctx)
    {
        ((F)fn)(cpu, pc, (I)info, (C)sctx);
    }
    static sc2_fn ptr() { return (sc2_fn)&inv; }
};
template <typename I, typename C>
struct sc2_sig_ctx<void (*)(void *, CPUState *, target_ulong, I, C)> {
    typedef void (*F)(void *, CPUState *, target_ulong, I, C);
    static SC2_HOT void inv(sc2_fn fn, void *ctx, CPUState *cpu, target_ulong pc, const void *info,
                    const void *sctx)
    {
        ((F)fn)(ctx, cpu, pc, (I)info, (C)sctx);
    }
    static sc2_fn ptr() { return (sc2_fn)&inv; }
};


/* The exported functions of a callback, as PPP_CB_BOILERPLATE made them. */
#define SC2_SHIM_(cb, list, sig, nargs)                                                       \
    void ppp_add_cb_##cb(cb##_t f) {                                                          \
        sc2_add(list, (sc2_fn)f, nullptr, false, sig<cb##_t>::ptr(), nargs(cb##_t), -1);       \
    }                                                                                         \
    void ppp_add_cb_##cb##_slot(cb##_t f, int slot) {                                         \
        sc2_add(list, (sc2_fn)f, nullptr, false, sig<cb##_t>::ptr(), nargs(cb##_t), slot);     \
    }                                                                                         \
    bool ppp_remove_cb_##cb(cb##_t f) {                                                       \
        return sc2_remove(list, (sc2_fn)f, nullptr, false);                                   \
    }                                                                                         \
    void ppp_add_cb_##cb##_with_context(cb##_with_context_t f, void *c) {                     \
        sc2_add(list, (sc2_fn)f, c, true, sig<cb##_with_context_t>::ptr(),                     \
                nargs(cb##_with_context_t), -1);                                              \
    }                                                                                         \
    void ppp_add_cb_##cb##_slot_with_context(cb##_with_context_t f, int slot, void *c) {      \
        sc2_add(list, (sc2_fn)f, c, true, sig<cb##_with_context_t>::ptr(),                     \
                nargs(cb##_with_context_t), slot);                                            \
    }                                                                                         \
    bool ppp_remove_cb_##cb##_with_context(cb##_with_context_t f, void *c) {                  \
        return sc2_remove(list, (sc2_fn)f, c, true);                                          \
    }
#define SC2_NARGS(t) ((int)sc2_sig<t>::nargs)
#define SC2_NOARGS(t) (-1)
/* A call's own callback */
#define SC2_SHIM(cb, name, dir) SC2_SHIM_(cb, SC2_LIST(name, dir), sc2_sig, SC2_NARGS)
/* on_unknown_sys_*: (cpu, pc, callno), on the numbers no table has */
#define SC2_SHIM_NR(cb, name, dir) SC2_SHIM_(cb, SC2_LIST(name, dir), sc2_sig_nr, SC2_NOARGS)
/* on_all_sys_*: (cpu, pc, callno), every call */
#define SC2_SHIM_ALL(cb, l) SC2_SHIM_(cb, SC2_XLIST(l), sc2_sig_nr, SC2_NOARGS)
/* on_all_sys_*2: (cpu, pc, info, ctx), every call */
#define SC2_SHIM_CTX(cb, l) SC2_SHIM_(cb, SC2_XLIST(l), sc2_sig_ctx, SC2_NOARGS)
