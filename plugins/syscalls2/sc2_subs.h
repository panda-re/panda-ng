/*
 * syscalls2's subscriber lists: the internal interface between the subscriptions and the dispatch
 * (sc2_dispatch.inc). A list is a callback name's own (SC2_LIST: its numbers in every table), or an
 * "every call" one (SC2_XLIST); sc2_add and sc2_remove change one and recompute the engine's wants
 * for its numbers; the dispatch runs a published list (immutable, read without a lock).
 */
#pragma once
#include "panda.h"
#include "sc2_tables.h"

/*
 * The functions on every delivered call's path skip the call-used register zeroing on return, as
 * the engine's syscall-pair-fast.c does (they hold no array: no stack protector canary either).
 * They keep the frame pointer: an optimize attribute or pragma would stop the compiler inlining
 * std::atomic's accessors into them, which costs more. Everything else in the plugin keeps the
 * toolchain's hardening.
 */
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 11
#define SC2_HOT __attribute__((zero_call_used_regs("skip")))
#else
#define SC2_HOT
#endif

/* --- Subscribers ------------------------------------------------------------------------- */

struct CPUState;
typedef void (*sc2_fn)(void);
/* A call's own callback: its arguments a[] converted to the callback's types */
typedef void (*sc2_inv)(sc2_fn fn, void *ctx, CPUState *cpu, target_ulong pc, const uint64_t *a);
/* on_all_sys_* and on_unknown_sys_*: (cpu, pc, callno). A plain one has none: it is called as it is */
typedef void (*sc2_inv_nr)(sc2_fn fn, void *ctx, CPUState *cpu, target_ulong pc, target_ulong no);
typedef void (*sc2_plain_nr)(CPUState *cpu, target_ulong pc, target_ulong no);
/* on_all_sys_*2: (cpu, pc, info, ctx) */
typedef void (*sc2_inv_ctx)(sc2_fn fn, void *ctx, CPUState *cpu, target_ulong pc, const void *info,
                            const void *sctx);

struct sc2_ent {
    sc2_fn inv;             /* the list's invoker type (above), or NULL */
    sc2_fn fn;
    void *ctx;
};
/* A subscriber list, as published: immutable (a change publishes a new one) */
struct sc2_list {
    uint32_t n;
    sc2_ent e[];
};

/* Directions; the lists that are not a name's */
enum { SC2_ENTER, SC2_RETURN };
enum { SC2_L_ALL_ENTER, SC2_L_ALL_RETURN, SC2_L_ENTER2, SC2_L_RETURN2, SC2_L_COUNT };

/* The registry (syscalls2.cpp): a name's (or a list's) subscribers, kept as the PPP arrays kept
 * them (slots, holes, removal order); each change publishes its list and updates the engine's
 * wants for the name's numbers. @nargs: the callback type's arguments (checked against the
 * tables). */
#define SC2_LIST(name, dir) ((name) * 2 + (dir))
#define SC2_XLIST(l) (SC2_N_COUNT * 2 + (l))
void sc2_add(unsigned list, sc2_fn fn, void *ctx, bool with_ctx, sc2_fn inv, int nargs, int slot);
bool sc2_remove(unsigned list, sc2_fn fn, void *ctx, bool with_ctx);

