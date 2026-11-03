/*
 * PANDA events: one registry for core callbacks, plugin-published events and syscalls.
 *
 * An event is identified by a UUIDv5 over its publisher's namespace, its name, a NUL byte and the
 * hashed part of its argument signature. Only an incompatible argument change alters the UUID, so a
 * consumer built against old arguments never binds to the new event: it is told what differs.
 *
 * Publishers declare an event once and emit it; consumers find it (before or after the publisher
 * loads) and subscribe as an observer or a mutator. Every subscription has a state and a reason.
 * Admission (modes, capability, KVM, record/replay) is checked at subscribe time with the same
 * policy as core callbacks (panda/admission.h), never per dispatch.
 *
 * Arguments are one struct per event, starting with `uint32_t size`. A publisher's header gives
 * one field-list macro per event, from which PANDA_EVENT_STRUCT and PANDA_EVENT_SIGNATURE build the
 * struct and the signature string, so the two cannot drift:
 *
 *     #define HOOKS_HIT_ARGS(F) F(cpu, ptr) F(pc, u64) F(hook, ptr)
 *     PANDA_EVENT_STRUCT(hooks_hit_args, HOOKS_HIT_ARGS);
 *     ev = panda_event_find(&HOOKS_NS, "hooks.hit", PANDA_EVENT_SIGNATURE(HOOKS_HIT_ARGS));
 *     sub = panda_subscribe(self, ev, on_hit, ctx, PANDA_MODE_ANALYSIS, PANDA_SUB_OBSERVE);
 *
 * Signature grammar: comma-separated name:type fields, types u8..u64, i8..i64 and ptr, laid out
 * with natural alignment after `size`. Fields after a `|` are appended: they are not hashed, and a
 * consumer reads one only if PANDA_EVENT_HAS(args, field).
 *
 * Core callbacks (panda_register_callback) are declared here as "cb.<type>" events in the PANDA
 * namespace, and the core callback lists are their subscribers; their dispatch is unchanged.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_EVENTS_H
#define PANDA_EVENTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct panda_event_uuid {
    uint8_t b[16];
} panda_event_uuid;

/* The namespace of PANDA's own events: core callbacks, and the in-tree plugins'. */
extern const panda_event_uuid PANDA_EVENT_NS;

typedef struct PandaEvent *panda_event_id;      /* never freed: valid for the whole run */
typedef struct PandaSub *panda_sub;
typedef struct PandaEventInstance *panda_event_instance;

/* panda_event_desc.flags */
enum {
    PANDA_EVENT_MUTABLE   = 1u << 0,    /* mutators may subscribe */
    PANDA_EVENT_RR_MUTATE = 1u << 1,    /* the source logs mutations: mutators admitted in rr */
    PANDA_EVENT_INSTANCES = 1u << 2,    /* emitted as open/close pairs (panda_event_open) */
};

typedef struct panda_event_desc {
    uint32_t size;                  /* sizeof(struct panda_event_desc) */
    const panda_event_uuid *ns;
    const char *name;               /* "hooks.hit" */
    const char *signature;          /* PANDA_EVENT_SIGNATURE(...) */
    uint32_t args_size;             /* sizeof the argument struct; checked against the signature */
    uint32_t modes;                 /* PANDA_MODE_* the event fires in (KVM only if it does) */
    uint32_t capability;            /* ANALYSIS_CAP_* needed besides analysis, or 0 */
    uint32_t flags;                 /* PANDA_EVENT_* */
} panda_event_desc;

/* panda_subscribe flags: exactly one of OBSERVE, OBSERVE_ORIGINAL and MUTATE. */
enum {
    PANDA_SUB_OBSERVE          = 1u << 0,   /* runs after the mutators, sees the final arguments */
    PANDA_SUB_MUTATE           = 1u << 1,   /* runs in priority, then subscription, order */
    PANDA_SUB_OBSERVE_ORIGINAL = 1u << 2,   /* an observer that runs before the mutators */
};

typedef enum panda_sub_state {
    PANDA_SUB_ACTIVE,
    PANDA_SUB_PENDING,      /* the event is not declared (yet, or its publisher unloaded) */
    PANDA_SUB_MISMATCH,     /* declared under another signature: the reason has the diff */
    PANDA_SUB_REFUSED,      /* admission failed: the reason names the rule */
    PANDA_SUB_DISABLED,     /* panda_sub_enable(sub, false) */
} panda_sub_state;

/*
 * A subscriber. @args is the event's argument struct: an observer must not write it (nor guest
 * state through it). @data is this subscription's instance data for an INSTANCES event (zeroed at
 * open, the same bytes at close; NULL if it asked for none), else NULL.
 */
typedef void (*panda_event_fn)(void *ctx, void *args, void *data);

/* Publishers. Declaring returns NULL (with the reason logged) on a malformed descriptor or a
 * second publisher of the same UUID. */
panda_event_id panda_event_declare(void *owner, const panda_event_desc *desc);
void panda_event_emit(panda_event_id ev, void *args);
/* True if anything would run: lets a source skip building the arguments. */
bool panda_event_wanted(panda_event_id ev);

/* INSTANCES events: open dispatches like emit and returns NULL if nobody subscribed. Close
 * dispatches to the subscriptions the open reached that are still active, with their data, then
 * frees it. Drop frees an instance that will not close. */
panda_event_instance panda_event_open(panda_event_id ev, void *args);
void panda_event_close(panda_event_id ev, panda_event_instance inst, void *args);
void panda_event_drop(panda_event_instance inst);

/* Consumers. Find never fails on a well-formed signature: before the publisher loads the event is
 * a placeholder, and subscriptions to it are PENDING. */
panda_event_id panda_event_find(const panda_event_uuid *ns, const char *name, const char *signature);
panda_event_id panda_event_lookup(const panda_event_uuid *id);   /* declared events only */
panda_event_id panda_event_lookup_name(const char *name);        /* NULL if none, or ambiguous */
void panda_event_uuid_of(panda_event_id ev, panda_event_uuid *out);
const char *panda_event_name(panda_event_id ev);

panda_sub panda_subscribe(void *owner, panda_event_id ev, panda_event_fn fn, void *ctx,
                          uint32_t modes, uint32_t flags);
/* The reason stays valid until the subscription next changes state; copy it to keep it. */
panda_sub_state panda_sub_status(panda_sub sub, const char **reason);
bool panda_sub_enable(panda_sub sub, bool enabled);
bool panda_sub_set_priority(panda_sub sub, int priority);       /* mutators; lower runs first */
bool panda_sub_set_data_size(panda_sub sub, size_t size);       /* INSTANCES events only */
bool panda_unsubscribe(panda_sub sub);

/* Every event and subscription, as text or JSON (g_free the result). */
char *panda_events_describe(bool json);
const char *panda_sub_state_name(panda_sub_state state);

/* --- Field-list macros ------------------------------------------------------------------- */

#define PANDA_EVENT_CTYPE_u8  uint8_t
#define PANDA_EVENT_CTYPE_u16 uint16_t
#define PANDA_EVENT_CTYPE_u32 uint32_t
#define PANDA_EVENT_CTYPE_u64 uint64_t
#define PANDA_EVENT_CTYPE_i8  int8_t
#define PANDA_EVENT_CTYPE_i16 int16_t
#define PANDA_EVENT_CTYPE_i32 int32_t
#define PANDA_EVENT_CTYPE_i64 int64_t
#define PANDA_EVENT_CTYPE_ptr void *

#define PANDA_EVENT_FIELD_(name, type) PANDA_EVENT_CTYPE_##type name;
#define PANDA_EVENT_SIGF_(name, type) #name ":" #type ","

/* struct tag { uint32_t size; <fields> }; and its signature ("a:u8,b:u64," - the parser
 * ignores the trailing comma). The _EXT forms add appended (unhashed) fields after `|`. */
#define PANDA_EVENT_STRUCT(tag, FIELDS) \
    struct tag { uint32_t size; FIELDS(PANDA_EVENT_FIELD_) }
#define PANDA_EVENT_SIGNATURE(FIELDS) ("" FIELDS(PANDA_EVENT_SIGF_))
#define PANDA_EVENT_STRUCT_EXT(tag, FIELDS, MORE) \
    struct tag { uint32_t size; FIELDS(PANDA_EVENT_FIELD_) MORE(PANDA_EVENT_FIELD_) }
#define PANDA_EVENT_SIGNATURE_EXT(FIELDS, MORE) \
    ("" FIELDS(PANDA_EVENT_SIGF_) "|" MORE(PANDA_EVENT_SIGF_))

/* An appended field is present if the publisher's struct reaches past it. */
#define PANDA_EVENT_HAS(args, field) \
    ((args)->size >= offsetof(__typeof__(*(args)), field) + sizeof((args)->field))

#ifdef __cplusplus
}
#endif

#endif
