/*
 * The PANDA event registry (include/panda/events.h).
 *
 * Events are kept by UUID and never freed: an id a consumer holds stays valid when its publisher
 * unloads (the event is then undeclared, and its subscriptions PENDING). Writers (declare,
 * subscribe, unsubscribe, enable, unload) are serialized by events_lock. Each event publishes its
 * active subscriptions, in dispatch order, as an array that emitters read without a lock, as core
 * callback lists are walked: emitters are on a vCPU (inside cpu_exec, an RCU read-side critical
 * section) or hold the BQL. A replaced array and an unsubscribed subscription are freed after an
 * RCU grace period, and a subscription also only once no open instance still holds it.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/atomic.h"
#include "qemu/rcu.h"
#include "qemu/thread.h"
#include "qemu/lockable.h"
#include "qemu/error-report.h"
#include "system/analysis-policy.h"
#include "system/kvm.h"
#include "system/replay.h"
#include "panda/plugin.h"
#include "panda/admission.h"
#include "panda/events-internal.h"

/* 40c975de-b1b1-4814-923b-7b613e2fef15 */
const panda_event_uuid PANDA_EVENT_NS = { {
    0x40, 0xc9, 0x75, 0xde, 0xb1, 0xb1, 0x48, 0x14,
    0x92, 0x3b, 0x7b, 0x61, 0x3e, 0x2f, 0xef, 0x15,
} };

typedef struct PandaEvent PandaEvent;
typedef struct PandaSub PandaSub;
typedef struct PandaEventInstance PandaEventInstance;

/* A subscription's data size is part of the snapshot, so an open sizes and lays out the same
 * blocks it hands out even if the size changes meanwhile. */
typedef struct PandaSubArray {
    struct rcu_head rcu;
    unsigned n;
    struct {
        PandaSub *sub;
        size_t data_size;
    } s[];
} PandaSubArray;

struct PandaEvent {
    panda_event_uuid uuid;
    panda_event_uuid ns;
    char *name;
    char *hashed;               /* the canonical hashed signature: what the UUID covers */
    char *full;                 /* the publisher's canonical signature, or the first finder's */
    uint32_t args_size;
    uint32_t modes;
    uint32_t capability;
    uint32_t flags;
    bool declared;
    void *owner;                /* the publisher, while declared */
    char *owner_name;           /* kept for messages after the publisher is gone */
    int core_type;              /* the panda_cb_type of a core event, or -1 */
    GPtrArray *subs;            /* every live subscription, any state, in subscription order */
    PandaSubArray *active;      /* RCU: the ACTIVE ones, in dispatch order */
};

struct PandaSub {
    struct rcu_head rcu;
    PandaEvent *ev;
    void *owner;
    panda_event_fn fn;
    void *ctx;
    uint32_t modes;
    uint32_t flags;
    int priority;
    uint64_t seq;
    size_t data_size;
    bool enabled;
    bool live;                  /* false once unsubscribed: a dispatch still holding it skips it */
    panda_sub_state state;
    const char *reason;
    GPtrArray *reasons;         /* every reason given out: freed with the subscription */
    int refs;                   /* the registry's, and one per open instance */
};

typedef struct PandaInstanceEntry {
    PandaSub *sub;
    void *data;
} PandaInstanceEntry;

struct PandaEventInstance {
    PandaEvent *ev;
    unsigned n;
    PandaInstanceEntry e[];
};

static QemuMutex events_lock;
static GHashTable *events_by_uuid;
static GPtrArray *events_all;           /* in creation order, for listings and name lookups */
static PandaEvent **core_events;
static int core_events_max = -1;
static uint64_t sub_seq;

static void events_declare_core(void);

static guint uuid_hash(gconstpointer p)
{
    const uint8_t *b = p;
    return b[0] | b[1] << 8 | b[2] << 16 | (guint)b[3] << 24;
}

static gboolean uuid_equal(gconstpointer a, gconstpointer b)
{
    return !memcmp(a, b, sizeof(panda_event_uuid));
}

static void registry_init(void)
{
    static gsize done;

    if (g_once_init_enter(&done)) {
        qemu_mutex_init(&events_lock);
        events_by_uuid = g_hash_table_new(uuid_hash, uuid_equal);
        events_all = g_ptr_array_new();
        events_declare_core();
        g_once_init_leave(&done, 1);
    }
}

/* --- Signatures and identity ---------------------------------------------------------------- */

static const struct {
    const char *name;
    uint32_t size;
} sig_types[] = {
    { "u8", 1 }, { "u16", 2 }, { "u32", 4 }, { "u64", 8 },
    { "i8", 1 }, { "i16", 2 }, { "i32", 4 }, { "i64", 8 },
    { "ptr", sizeof(void *) },
};

typedef struct SigField {
    char *name;
    const char *type;
    bool appended;
} SigField;

static void sig_fields_free(GArray *fields)
{
    for (guint i = 0; i < fields->len; i++) {
        g_free(g_array_index(fields, SigField, i).name);
    }
    g_array_free(fields, true);
}

static bool sig_ident(const char *s)
{
    if (!*s || !(g_ascii_isalpha(*s) || *s == '_')) {
        return false;
    }
    for (; *s; s++) {
        if (!(g_ascii_isalnum(*s) || *s == '_')) {
            return false;
        }
    }
    return true;
}

/* Fields in order; NULL with a reason if malformed. */
static GArray *sig_split(const char *sig, char **why)
{
    GArray *fields = g_array_new(false, false, sizeof(SigField));
    char **parts = g_strsplit(sig, "|", -1);
    bool ok = true;

    if (g_strv_length(parts) > 2) {
        *why = g_strdup_printf("signature \"%s\": more than one '|'", sig);
        ok = false;
    }
    for (int p = 0; ok && parts[p]; p++) {
        char **items = g_strsplit(parts[p], ",", -1);
        for (int i = 0; ok && items[i]; i++) {
            char *item = g_strstrip(items[i]);
            if (!*item) {
                continue;               /* the macros leave a trailing comma */
            }
            char *colon = strchr(item, ':');
            SigField f = { .appended = p == 1 };
            if (!colon) {
                *why = g_strdup_printf("signature \"%s\": field \"%s\" has no type", sig, item);
                ok = false;
                break;
            }
            *colon = 0;
            const char *name = g_strstrip(item), *type = g_strstrip(colon + 1);
            if (!sig_ident(name)) {
                *why = g_strdup_printf("signature \"%s\": \"%s\" is not a field name", sig, name);
                ok = false;
                break;
            }
            for (size_t t = 0; t < G_N_ELEMENTS(sig_types); t++) {
                if (!strcmp(type, sig_types[t].name)) {
                    f.type = sig_types[t].name;
                }
            }
            if (!f.type) {
                *why = g_strdup_printf("signature \"%s\": field %s has unknown type \"%s\" "
                                       "(u8..u64, i8..i64, ptr)", sig, name, type);
                ok = false;
                break;
            }
            for (guint j = 0; j < fields->len; j++) {
                if (!strcmp(g_array_index(fields, SigField, j).name, name)) {
                    *why = g_strdup_printf("signature \"%s\": field %s appears twice", sig, name);
                    ok = false;
                }
            }
            if (ok) {
                f.name = g_strdup(name);
                g_array_append_val(fields, f);
            }
        }
        g_strfreev(items);
    }
    g_strfreev(parts);
    if (!ok) {
        sig_fields_free(fields);
        return NULL;
    }
    return fields;
}

static uint32_t sig_type_size(const char *type)
{
    for (size_t t = 0; t < G_N_ELEMENTS(sig_types); t++) {
        if (!strcmp(type, sig_types[t].name)) {
            return sig_types[t].size;
        }
    }
    g_assert_not_reached();
}

bool panda_event_signature_parse(const char *sig, char **hashed, char **full,
                                 uint32_t *layout_size, char **why)
{
    GArray *fields = sig ? sig_split(sig, why) : NULL;

    if (!sig) {
        *why = g_strdup("no signature");
    }
    if (!fields) {
        return false;
    }

    GString *h = g_string_new(NULL), *a = g_string_new(NULL);
    uint32_t off = sizeof(uint32_t), align = sizeof(uint32_t);     /* the size field */
    for (guint i = 0; i < fields->len; i++) {
        SigField *f = &g_array_index(fields, SigField, i);
        GString *out = f->appended ? a : h;
        uint32_t sz = sig_type_size(f->type);
        g_string_append_printf(out, "%s%s:%s", out->len ? "," : "", f->name, f->type);
        off = ROUND_UP(off, sz) + sz;
        align = MAX(align, sz);
    }
    sig_fields_free(fields);
    if (layout_size) {
        *layout_size = ROUND_UP(off, align);
    }
    if (full) {
        *full = a->len ? g_strdup_printf("%s|%s", h->str, a->str) : g_strdup(h->str);
    }
    g_string_free(a, true);
    if (hashed) {
        *hashed = g_string_free(h, false);
    } else {
        g_string_free(h, true);
    }
    return true;
}

void panda_event_uuid_derive(const panda_event_uuid *ns, const char *name, const char *hashed,
                             panda_event_uuid *out)
{
    GChecksum *sha1 = g_checksum_new(G_CHECKSUM_SHA1);
    uint8_t digest[20];
    gsize len = sizeof(digest);

    g_checksum_update(sha1, ns->b, sizeof(ns->b));
    g_checksum_update(sha1, (const guchar *)name, strlen(name) + 1);    /* with its NUL */
    g_checksum_update(sha1, (const guchar *)hashed, strlen(hashed));
    g_checksum_get_digest(sha1, digest, &len);
    g_checksum_free(sha1);
    memcpy(out->b, digest, sizeof(out->b));
    out->b[6] = (out->b[6] & 0x0f) | 0x50;      /* version 5 */
    out->b[8] = (out->b[8] & 0x3f) | 0x80;      /* RFC 4122 variant */
}

static void uuid_format(const panda_event_uuid *u, char out[37])
{
    const uint8_t *b = u->b;
    snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
             b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

/* What a consumer built with @want lacks or has wrong against the publisher's @have. */
static char *sig_diff(const char *have, const char *want)
{
    char *why = NULL;
    GArray *h = sig_split(have, &why), *w = sig_split(want, &why);
    GString *d = g_string_new(NULL);

    g_free(why);
    for (guint i = 0; h && w && i < MAX(h->len, w->len); i++) {
        SigField *hf = i < h->len ? &g_array_index(h, SigField, i) : NULL;
        SigField *wf = i < w->len ? &g_array_index(w, SigField, i) : NULL;
        if (hf && wf && !strcmp(hf->name, wf->name) && strcmp(hf->type, wf->type)) {
            g_string_append_printf(d, "; field %u (%s) is %s, was %s", i, hf->name, hf->type,
                                   wf->type);
        } else if (hf && wf && strcmp(hf->name, wf->name)) {
            g_string_append_printf(d, "; field %u is %s:%s, was %s:%s", i, hf->name, hf->type,
                                   wf->name, wf->type);
        } else if (hf && !wf) {
            g_string_append_printf(d, "; field %u (%s:%s) is new", i, hf->name, hf->type);
        } else if (!hf && wf) {
            g_string_append_printf(d, "; field %u (%s:%s) is gone", i, wf->name, wf->type);
        }
    }
    if (h) {
        sig_fields_free(h);
    }
    if (w) {
        sig_fields_free(w);
    }
    return g_string_free(d, false);
}

/* --- Events -------------------------------------------------------------------------------- */

static PandaEvent *event_get(const panda_event_uuid *ns, const char *name, const char *hashed)
{
    panda_event_uuid id;
    PandaEvent *ev;

    panda_event_uuid_derive(ns, name, hashed, &id);
    ev = g_hash_table_lookup(events_by_uuid, &id);
    if (!ev) {
        ev = g_new0(PandaEvent, 1);
        ev->uuid = id;
        ev->ns = *ns;
        ev->name = g_strdup(name);
        ev->hashed = g_strdup(hashed);
        ev->core_type = -1;
        ev->subs = g_ptr_array_new();
        g_hash_table_insert(events_by_uuid, &ev->uuid, ev);
        g_ptr_array_add(events_all, ev);
    }
    return ev;
}

/* The declared event with @ev's namespace and name under another signature, if any. */
static PandaEvent *event_rival(PandaEvent *ev)
{
    for (guint i = 0; i < events_all->len; i++) {
        PandaEvent *o = g_ptr_array_index(events_all, i);
        if (o != ev && o->declared && !strcmp(o->name, ev->name) &&
            !memcmp(&o->ns, &ev->ns, sizeof(ev->ns))) {
            return o;
        }
    }
    return NULL;
}

static const char *owner_name(void *owner)
{
    const char *name = owner ? panda_event_owner_name(owner) : NULL;
    return name ? name : "?";
}

/* --- Subscriptions ------------------------------------------------------------------------- */

static int sub_phase(const PandaSub *s)
{
    return (s->flags & PANDA_SUB_OBSERVE_ORIGINAL) ? 0 : (s->flags & PANDA_SUB_MUTATE) ? 1 : 2;
}

static gint sub_order(gconstpointer a, gconstpointer b)
{
    const PandaSub *x = *(PandaSub * const *)a, *y = *(PandaSub * const *)b;
    int px = sub_phase(x), py = sub_phase(y);

    if (px != py) {
        return px - py;
    }
    if (px == 1 && x->priority != y->priority) {
        return x->priority < y->priority ? -1 : 1;
    }
    return x->seq < y->seq ? -1 : x->seq > y->seq;
}

static void event_publish(PandaEvent *ev)
{
    GPtrArray *act = g_ptr_array_new();
    PandaSubArray *a = NULL, *old;

    for (guint i = 0; i < ev->subs->len; i++) {
        PandaSub *s = g_ptr_array_index(ev->subs, i);
        if (s->state == PANDA_SUB_ACTIVE) {
            g_ptr_array_add(act, s);
        }
    }
    if (act->len) {
        g_ptr_array_sort(act, sub_order);
        a = g_malloc(sizeof(*a) + act->len * sizeof(a->s[0]));
        a->n = act->len;
        for (guint i = 0; i < act->len; i++) {
            a->s[i].sub = g_ptr_array_index(act, i);
            a->s[i].data_size = a->s[i].sub->data_size;
        }
    }
    g_ptr_array_free(act, true);
    old = ev->active;
    qatomic_rcu_set(&ev->active, a);
    if (old) {
        g_free_rcu(old, rcu);
    }
}

static void sub_set(PandaSub *s, panda_sub_state state, char *reason)
{
    bool changed = s->state != state || g_strcmp0(s->reason, reason);

    if (!changed) {
        g_free(reason);
        return;
    }
    g_ptr_array_add(s->reasons, reason);
    qatomic_set(&s->reason, reason);
    qatomic_set(&s->state, state);
    if (state == PANDA_SUB_MISMATCH || state == PANDA_SUB_REFUSED) {
        warn_report("PANDA event %s: %s's subscription is %s: %s", s->ev->name,
                    owner_name(s->owner), panda_sub_state_name(state), reason);
    }
}

static void sub_eval(PandaSub *s)
{
    PandaEvent *ev = s->ev;

    if (!s->enabled) {
        sub_set(s, PANDA_SUB_DISABLED, g_strdup("disabled by its owner"));
    } else if (ev->declared && ev->core_type >= 0) {
        sub_set(s, PANDA_SUB_REFUSED, g_strdup_printf(
            "%s is a core callback: register it with panda_register_callback "
            "(subscribing through the registry needs its argument struct)", ev->name));
    } else if (ev->declared && (s->flags & PANDA_SUB_MUTATE) &&
               !(ev->flags & PANDA_EVENT_MUTABLE)) {
        sub_set(s, PANDA_SUB_REFUSED, g_strdup("the event does not accept mutators"));
    } else if (ev->declared) {
        PandaAdmit a = panda_admit(ev->modes, ev->capability, ev->flags & PANDA_EVENT_RR_MUTATE,
                                   s->modes, !(s->flags & PANDA_SUB_MUTATE));
        if (a == PANDA_ADMIT_OK) {
            sub_set(s, PANDA_SUB_ACTIVE, g_strdup_printf("published by %s", ev->owner_name));
        } else {
            sub_set(s, PANDA_SUB_REFUSED, g_strdup(panda_admit_reason(a)));
        }
    } else {
        PandaEvent *rival = event_rival(ev);
        if (rival) {
            char *diff = sig_diff(rival->full, ev->full);
            sub_set(s, PANDA_SUB_MISMATCH, g_strdup_printf(
                "%s: %s publishes %s; %s was built with %s%s", ev->name, rival->owner_name,
                rival->full, owner_name(s->owner), ev->full, diff));
            g_free(diff);
        } else if (ev->owner_name) {
            sub_set(s, PANDA_SUB_PENDING, g_strdup_printf(
                "%s: its publisher %s was unloaded", ev->name, ev->owner_name));
        } else {
            sub_set(s, PANDA_SUB_PENDING, g_strdup_printf(
                "%s (%s): no loaded plugin publishes it", ev->name, ev->full));
        }
    }
}

static void event_eval(PandaEvent *ev)
{
    for (guint i = 0; i < ev->subs->len; i++) {
        sub_eval(g_ptr_array_index(ev->subs, i));
    }
    event_publish(ev);
}

/* @ev and every event with its namespace and name: a declare or an unload changes them all. */
static void event_eval_rivals(PandaEvent *ev)
{
    for (guint i = 0; i < events_all->len; i++) {
        PandaEvent *o = g_ptr_array_index(events_all, i);
        if (o == ev || (!strcmp(o->name, ev->name) && !memcmp(&o->ns, &ev->ns, sizeof(ev->ns)))) {
            event_eval(o);
        }
    }
}

static void sub_free(PandaSub *s)
{
    g_ptr_array_free(s->reasons, true);
    g_free(s);
}

static void sub_unref(PandaSub *s)
{
    if (qatomic_fetch_dec(&s->refs) == 1) {
        call_rcu(s, sub_free, rcu);
    }
}

/* --- Publishers ---------------------------------------------------------------------------- */

static PandaEvent *event_declare(void *owner, const char *owner_label, const panda_event_desc *desc)
{
    char *hashed, *full, *why = NULL;
    uint32_t layout;
    PandaEvent *ev;

    if (!desc || desc->size < sizeof(panda_event_desc) || !desc->ns || !desc->name ||
        !*desc->name) {
        error_report("PANDA event: declare refused: malformed descriptor");
        return NULL;
    }
    if (!panda_event_signature_parse(desc->signature, &hashed, &full, &layout, &why)) {
        error_report("PANDA event %s: declare refused: %s", desc->name, why);
        g_free(why);
        return NULL;
    }
    if (desc->args_size != layout) {
        error_report("PANDA event %s: declare refused: args_size is %u, but signature %s lays "
                     "out as %u bytes", desc->name, desc->args_size, full, layout);
        g_free(hashed);
        g_free(full);
        return NULL;
    }
    if (desc->flags & PANDA_EVENT_RR_MUTATE && !(desc->flags & PANDA_EVENT_MUTABLE)) {
        error_report("PANDA event %s: declare refused: RR_MUTATE without MUTABLE", desc->name);
        g_free(hashed);
        g_free(full);
        return NULL;
    }
    ev = event_get(desc->ns, desc->name, hashed);
    g_free(hashed);
    if (ev->declared) {
        error_report("PANDA event %s (%s): declare refused: already published by %s",
                     desc->name, full, ev->owner_name);
        g_free(full);
        return NULL;
    }
    if (event_rival(ev)) {
        PandaEvent *r = event_rival(ev);
        error_report("PANDA event %s: declare refused: %s publishes it as %s", desc->name,
                     r->owner_name, r->full);
        g_free(full);
        return NULL;
    }
    if (ev->full && strcmp(ev->full, full)) {
        /* A consumer found it with other appended fields: tell it before it reads them. */
        char *diff = sig_diff(full, ev->full);
        if (*diff) {
            warn_report("PANDA event %s: published as %s, found as %s%s", desc->name, full,
                        ev->full, diff);
        }
        g_free(diff);
    }
    g_free(ev->full);
    ev->full = full;
    ev->args_size = desc->args_size;
    ev->modes = desc->modes;
    ev->capability = desc->capability;
    ev->flags = desc->flags;
    ev->owner = owner;
    g_free(ev->owner_name);
    ev->owner_name = g_strdup(owner_label);
    ev->declared = true;
    event_eval_rivals(ev);
    return ev;
}

panda_event_id panda_event_declare(void *owner, const panda_event_desc *desc)
{
    registry_init();
    QEMU_LOCK_GUARD(&events_lock);
    return event_declare(owner, owner_name(owner), desc);
}

static void events_declare_core(void)
{
    for (size_t i = 0; i < panda_core_events_count; i++) {
        core_events_max = MAX(core_events_max, panda_core_events[i].type);
    }
    core_events = g_new0(PandaEvent *, core_events_max + 1);
    for (size_t i = 0; i < panda_core_events_count; i++) {
        const PandaCoreEventDef *d = &panda_core_events[i];
        uint32_t modes = panda_core_event_modes(d->type), layout;
        char *name, *why = NULL;

        if (!modes) {
            continue;
        }
        name = g_strdup_printf("cb.%s", d->name);
        if (!panda_event_signature_parse(d->signature, NULL, NULL, &layout, &why)) {
            g_error("PANDA core event %s: %s", name, why);
        }
        panda_event_desc desc = {
            .size = sizeof(desc), .ns = &PANDA_EVENT_NS, .name = name,
            .signature = d->signature, .args_size = layout, .modes = modes,
            .capability = 0,
            .flags = d->rr_mutate ? PANDA_EVENT_MUTABLE | PANDA_EVENT_RR_MUTATE
                                  : PANDA_EVENT_MUTABLE,
        };
        PandaEvent *ev = event_declare(NULL, "panda", &desc);
        g_assert(ev);
        ev->core_type = d->type;
        core_events[d->type] = ev;
        g_free(name);
    }
}

panda_event_id panda_core_event(int type)
{
    registry_init();
    return type >= 0 && type <= core_events_max ? core_events[type] : NULL;
}

bool panda_event_wanted(panda_event_id ev)
{
    return ev && qatomic_rcu_read(&ev->active) != NULL;
}

void panda_event_emit(panda_event_id ev, void *args)
{
    PandaSubArray *a = ev ? qatomic_rcu_read(&ev->active) : NULL;

    if (!a) {
        return;
    }
    for (unsigned i = 0; i < a->n; i++) {
        PandaSub *s = a->s[i].sub;
        if (qatomic_read(&s->live)) {
            s->fn(s->ctx, args, NULL);
        }
    }
}

panda_event_instance panda_event_open(panda_event_id ev, void *args)
{
    PandaSubArray *a = ev ? qatomic_rcu_read(&ev->active) : NULL;
    PandaEventInstance *inst;
    size_t off;

    if (!a) {
        return NULL;
    }
    off = ROUND_UP(sizeof(*inst) + a->n * sizeof(inst->e[0]), 16);
    size_t total = off;
    for (unsigned i = 0; i < a->n; i++) {
        total += ROUND_UP(a->s[i].data_size, 16);
    }
    inst = g_malloc0(total);
    inst->ev = ev;
    for (unsigned i = 0; i < a->n; i++) {
        PandaSub *s = a->s[i].sub;
        size_t sz = a->s[i].data_size;
        if (!qatomic_read(&s->live)) {
            continue;
        }
        qatomic_inc(&s->refs);
        inst->e[inst->n].sub = s;
        inst->e[inst->n].data = sz ? (char *)inst + off : NULL;
        off += ROUND_UP(sz, 16);
        inst->n++;
        s->fn(s->ctx, args, inst->e[inst->n - 1].data);
    }
    return inst;
}

void panda_event_drop(panda_event_instance inst)
{
    if (!inst) {
        return;
    }
    for (unsigned i = 0; i < inst->n; i++) {
        sub_unref(inst->e[i].sub);
    }
    g_free(inst);
}

void panda_event_close(panda_event_id ev, panda_event_instance inst, void *args)
{
    if (!inst) {
        return;
    }
    g_assert(inst->ev == ev);
    for (unsigned i = 0; i < inst->n; i++) {
        PandaSub *s = inst->e[i].sub;
        if (qatomic_read(&s->live) && qatomic_read(&s->state) == PANDA_SUB_ACTIVE) {
            s->fn(s->ctx, args, inst->e[i].data);
        }
    }
    panda_event_drop(inst);
}

/* --- Consumers ----------------------------------------------------------------------------- */

panda_event_id panda_event_find(const panda_event_uuid *ns, const char *name, const char *signature)
{
    char *hashed, *full, *why = NULL;
    PandaEvent *ev;

    if (!ns || !name || !*name) {
        error_report("PANDA event: find needs a namespace and a name");
        return NULL;
    }
    if (!panda_event_signature_parse(signature, &hashed, &full, NULL, &why)) {
        error_report("PANDA event %s: %s", name, why);
        g_free(why);
        return NULL;
    }
    registry_init();
    QEMU_LOCK_GUARD(&events_lock);
    ev = event_get(ns, name, hashed);
    g_free(hashed);
    if (!ev->full) {
        ev->full = full;
    } else {
        if (ev->declared) {
            char *diff = sig_diff(ev->full, full);
            if (*diff) {
                warn_report("PANDA event %s: published as %s, found as %s%s", name, ev->full,
                            full, diff);
            }
            g_free(diff);
        }
        g_free(full);
    }
    return ev;
}

panda_event_id panda_event_lookup(const panda_event_uuid *id)
{
    PandaEvent *ev;

    registry_init();
    QEMU_LOCK_GUARD(&events_lock);
    ev = g_hash_table_lookup(events_by_uuid, id);
    return ev && ev->declared ? ev : NULL;
}

panda_event_id panda_event_lookup_name(const char *name)
{
    PandaEvent *found = NULL;
    GString *all = g_string_new(NULL);
    unsigned n = 0;

    registry_init();
    QEMU_LOCK_GUARD(&events_lock);
    for (guint i = 0; i < events_all->len; i++) {
        PandaEvent *ev = g_ptr_array_index(events_all, i);
        if (ev->declared && !strcmp(ev->name, name)) {
            char ns[37];
            uuid_format(&ev->ns, ns);
            g_string_append_printf(all, "%s %s (%s, from %s)", n ? ";" : "", ns, ev->full,
                                   ev->owner_name);
            found = ev;
            n++;
        }
    }
    if (n > 1) {
        warn_report("PANDA event %s is ambiguous: published in%s", name, all->str);
        found = NULL;
    }
    g_string_free(all, true);
    return found;
}

void panda_event_uuid_of(panda_event_id ev, panda_event_uuid *out)
{
    *out = ev->uuid;
}

const char *panda_event_name(panda_event_id ev)
{
    return ev->name;
}

panda_sub panda_subscribe(void *owner, panda_event_id ev, panda_event_fn fn, void *ctx,
                          uint32_t modes, uint32_t flags)
{
    uint32_t kind = flags & (PANDA_SUB_OBSERVE | PANDA_SUB_MUTATE | PANDA_SUB_OBSERVE_ORIGINAL);
    PandaSub *s;

    if (!ev || !fn || flags != kind || !kind || (kind & (kind - 1))) {
        error_report("PANDA event %s: subscribe needs an event, a function, and exactly one of "
                     "OBSERVE, OBSERVE_ORIGINAL and MUTATE", ev ? ev->name : "(none)");
        return NULL;
    }
    registry_init();
    QEMU_LOCK_GUARD(&events_lock);
    s = g_new0(PandaSub, 1);
    s->ev = ev;
    s->owner = owner;
    s->fn = fn;
    s->ctx = ctx;
    s->modes = modes;
    s->flags = flags;
    s->seq = sub_seq++;
    s->enabled = true;
    s->live = true;
    s->refs = 1;
    s->state = PANDA_SUB_PENDING;
    s->reasons = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(ev->subs, s);
    event_eval(ev);
    return s;
}

panda_sub_state panda_sub_status(panda_sub sub, const char **reason)
{
    panda_sub_state state = qatomic_read(&sub->state);

    if (reason) {
        *reason = qatomic_read(&sub->reason);
    }
    return state;
}

bool panda_sub_enable(panda_sub sub, bool enabled)
{
    QEMU_LOCK_GUARD(&events_lock);
    if (!sub->live) {
        return false;
    }
    sub->enabled = enabled;
    event_eval(sub->ev);
    return !enabled || sub->state == PANDA_SUB_ACTIVE;
}

bool panda_sub_set_priority(panda_sub sub, int priority)
{
    QEMU_LOCK_GUARD(&events_lock);
    if (!sub->live || !(sub->flags & PANDA_SUB_MUTATE)) {
        return false;
    }
    sub->priority = priority;
    event_publish(sub->ev);
    return true;
}

bool panda_sub_set_data_size(panda_sub sub, size_t size)
{
    QEMU_LOCK_GUARD(&events_lock);
    /* Before the publisher declares, the flag is not known: checked again in describe. */
    if (!sub->live || (sub->ev->declared && !(sub->ev->flags & PANDA_EVENT_INSTANCES))) {
        return false;
    }
    sub->data_size = size;
    event_publish(sub->ev);
    return true;
}

static void sub_remove(PandaSub *s)
{
    g_ptr_array_remove(s->ev->subs, s);
    qatomic_set(&s->live, false);
    event_publish(s->ev);
    sub_unref(s);
}

bool panda_unsubscribe(panda_sub sub)
{
    if (!sub) {
        return false;
    }
    QEMU_LOCK_GUARD(&events_lock);
    if (!sub->live) {
        return false;
    }
    sub_remove(sub);
    return true;
}

/* --- Engine side --------------------------------------------------------------------------- */

void panda_events_owner_gone(void *owner)
{
    if (!owner) {
        return;
    }
    registry_init();
    QEMU_LOCK_GUARD(&events_lock);
    for (guint i = 0; i < events_all->len; i++) {
        PandaEvent *ev = g_ptr_array_index(events_all, i);
        for (guint j = 0; j < ev->subs->len;) {
            PandaSub *s = g_ptr_array_index(ev->subs, j);
            if (s->owner == owner) {
                sub_remove(s);
            } else {
                j++;
            }
        }
    }
    for (guint i = 0; i < events_all->len; i++) {
        PandaEvent *ev = g_ptr_array_index(events_all, i);
        if (ev->declared && ev->owner == owner) {
            ev->declared = false;
            ev->owner = NULL;
            event_eval_rivals(ev);
        }
    }
}

bool panda_events_rr_safe(bool replay, char **why)
{
    registry_init();
    QEMU_LOCK_GUARD(&events_lock);
    for (guint i = 0; i < events_all->len; i++) {
        PandaEvent *ev = g_ptr_array_index(events_all, i);
        for (guint j = 0; j < ev->subs->len; j++) {
            PandaSub *s = g_ptr_array_index(ev->subs, j);
            if (s->state != PANDA_SUB_ACTIVE) {
                continue;
            }
            if (!(s->modes & PANDA_MODE_RR) ||
                ((s->flags & PANDA_SUB_MUTATE) && !(ev->flags & PANDA_EVENT_RR_MUTATE))) {
                *why = g_strdup_printf("PANDA event %s: %s's subscription %s for %s", ev->name,
                                       owner_name(s->owner),
                                       !(s->modes & PANDA_MODE_RR)
                                           ? "does not declare PANDA_MODE_RR"
                                           : "is a mutator, and its mutations are not logged",
                                       replay ? "replay" : "recording");
                return false;
            }
        }
    }
    return true;
}

void panda_events_reevaluate(void)
{
    registry_init();
    QEMU_LOCK_GUARD(&events_lock);
    for (guint i = 0; i < events_all->len; i++) {
        event_eval(g_ptr_array_index(events_all, i));
    }
}

const char *panda_sub_state_name(panda_sub_state state)
{
    switch (state) {
    case PANDA_SUB_ACTIVE:      return "active";
    case PANDA_SUB_PENDING:     return "pending";
    case PANDA_SUB_MISMATCH:    return "mismatch";
    case PANDA_SUB_REFUSED:     return "refused";
    case PANDA_SUB_DISABLED:    return "disabled";
    }
    return "?";
}

static void json_str(GString *out, const char *s)
{
    g_string_append_c(out, '"');
    for (; s && *s; s++) {
        unsigned char c = *s;
        if (c == '"' || c == '\\') {
            g_string_append_printf(out, "\\%c", c);
        } else if (c < 0x20) {
            g_string_append_printf(out, "\\u%04x", c);
        } else {
            g_string_append_c(out, c);
        }
    }
    g_string_append_c(out, '"');
}

void panda_events_describe_sub(GString *out, bool json, const char *owner, const char *state,
                               const char *reason, const char *kind, uint32_t modes)
{
    if (!json) {
        g_string_append_printf(out, "    %-20s %-8s %-16s modes=%#x  %s\n", owner, state, kind,
                               modes, reason ? reason : "");
        return;
    }
    if (out->str[out->len - 1] != '[') {
        g_string_append_c(out, ',');
    }
    g_string_append(out, "{\"owner\":");
    json_str(out, owner);
    g_string_append(out, ",\"state\":");
    json_str(out, state);
    g_string_append(out, ",\"kind\":");
    json_str(out, kind);
    g_string_append_printf(out, ",\"modes\":%u,\"reason\":", modes);
    json_str(out, reason);
    g_string_append_c(out, '}');
}

static const char *sub_kind(const PandaSub *s)
{
    return (s->flags & PANDA_SUB_MUTATE) ? "mutate"
         : (s->flags & PANDA_SUB_OBSERVE_ORIGINAL) ? "observe-original" : "observe";
}

char *panda_events_describe(bool json)
{
    GString *out = g_string_new(json ? "{\"events\":[" : "");

    registry_init();
    QEMU_LOCK_GUARD(&events_lock);
    for (guint i = 0; i < events_all->len; i++) {
        PandaEvent *ev = g_ptr_array_index(events_all, i);
        char uuid[37], ns[37];

        uuid_format(&ev->uuid, uuid);
        uuid_format(&ev->ns, ns);
        if (json) {
            g_string_append(out, i ? ",{\"uuid\":" : "{\"uuid\":");
            json_str(out, uuid);
            g_string_append(out, ",\"ns\":");
            json_str(out, ns);
            g_string_append(out, ",\"name\":");
            json_str(out, ev->name);
            g_string_append(out, ",\"signature\":");
            json_str(out, ev->full);
            g_string_append(out, ",\"publisher\":");
            json_str(out, ev->declared ? ev->owner_name : NULL);
            g_string_append_printf(out, ",\"declared\":%s,\"core\":%s,\"args_size\":%u,"
                                   "\"modes\":%u,\"capability\":%u,\"flags\":%u,"
                                   "\"subscriptions\":[",
                                   ev->declared ? "true" : "false",
                                   ev->core_type >= 0 ? "true" : "false", ev->args_size,
                                   ev->modes, ev->capability, ev->flags);
        } else {
            g_string_append_printf(out, "%s %s %s(%s)  %s  modes=%#x flags=%#x\n", uuid,
                                   ev->declared ? ev->owner_name : "(not declared)",
                                   ev->name, ev->full ? ev->full : "", ns, ev->modes, ev->flags);
        }
        if (ev->core_type >= 0) {
            panda_core_event_describe_subs(ev->core_type, out, json);
        }
        for (guint j = 0; j < ev->subs->len; j++) {
            PandaSub *s = g_ptr_array_index(ev->subs, j);
            panda_events_describe_sub(out, json, owner_name(s->owner),
                                      panda_sub_state_name(s->state), s->reason, sub_kind(s),
                                      s->modes);
        }
        if (json) {
            g_string_append(out, "]}");
        }
    }
    if (json) {
        g_string_append(out, "]}");
    }
    return g_string_free(out, false);
}
