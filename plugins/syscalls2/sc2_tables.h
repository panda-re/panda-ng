/*
 * syscalls2's system calls as data: each profile's table (generated/syscall_table_<os>_<arch>.cpp),
 * per number its arguments' kinds, flags and callback name. Pure data, with no dispatch in it:
 * how a call's arguments are made from its raw argument words is the kinds (sc2_decode).
 *
 * A call is dispatched with two lookups: its number's entry here, and the subscriber list of its
 * name (sc2_subs.h). The positional ppp_add_cb_on_sys_* callbacks are called through the invoker
 * of their type (sc2_compat.h).
 */
#pragma once
#include <cstdint>

#include "generated/syscall_names.h"

/* An argument's kind: how its value is made from the raw argument words. The low 5 bits are its
 * (first) word. */
enum {
    SC2_K_U32 = 1,          /* the word's low 32 bits */
    SC2_K_S32 = 2,          /* the word's low 32 bits, sign-extended */
    SC2_K_X64 = 3,          /* the whole word */
    SC2_K_PAIR = 4,         /* a 64-bit argument of a 32-bit ABI: word + 1 is the high half */
};
#define SC2_KIND(op, slot) ((uint8_t)((op) << 5 | (slot)))
#define SC2_U32(s) SC2_KIND(SC2_K_U32, s)
#define SC2_S32(s) SC2_KIND(SC2_K_S32, s)
#define SC2_X64(s) SC2_KIND(SC2_K_X64, s)
#define SC2_PAIR(s) SC2_KIND(SC2_K_PAIR, s)

/* sc2_sys.flags */
enum {
    SC2_F_NORET = 1u << 0,  /* its return is not tracked (context_target.json's panda_noreturn) */
    SC2_F_DOUBLE = 1u << 1, /* syscall_ctx_t.double_return (panda_doublereturn) */
};

/* A number of a profile. name 0 (SC2_N_unknown): not one of its calls. */
struct sc2_sys {
    uint8_t nargs;
    uint8_t flags;
    uint16_t name;
    uint8_t kind[SC2_MAX_ARGS];
};

/* A profile's numbers: base to base + count - 1 in sys[], the few beyond in sparse_nr[]. */
struct sc2_table {
    uint32_t base, count;
    const sc2_sys *sys;
    uint32_t nsparse;
    const uint32_t *sparse_nr;
    const sc2_sys *sparse;
    uint8_t fast_slots;     /* the words the engine captures (MIPS o32: 4, the rest on the stack) */
    uint8_t slots;          /* the most words a call reads */
    uint8_t bits;           /* the ABI's word size */
};

