/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SYSTEM_REPLAY_EFFECT_H
#define SYSTEM_REPLAY_EFFECT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "qemu/typedefs.h"

#define REPLAY_RAM_EFFECT_MAX (64 * 1024)
#define REPLAY_RAM_EFFECT_PENDING_MAX 64
/* Cap on one register write, not a property of any target: it covers SVE's
 * largest Z register (2048 bits). A longer one is refused at request time.
 * Register writes count against REPLAY_RAM_EFFECT_PENDING_MAX too. */
#define REPLAY_REG_EFFECT_MAX 256

/* Request admission only: main thread/BQL, every vCPU stopped, RECORD and RR
 * provisioned. Copies bytes, never writes synchronously. False has no effect.
 * Apply runs at the existing deterministic TCG scheduler async-event boundary
 * under BQL + replay mutex, before another guest instruction budget. Not on
 * the requester thread. End-record flush cancels unsaved requests.
 */
bool replay_analysis_ram_write_request(uint64_t address, const uint8_t *data,
                                        uint32_t length);
/* Same admission as RAM writes. At the apply boundary the recording checks the
 * vCPU, register and size and makes the write; the log records whether the
 * register's setter accepted it. Replay writes only what was accepted. */
bool replay_analysis_reg_write_request(int cpu_index, int reg,
                                        const uint8_t *data, uint32_t length);
void replay_analysis_effect_prepare(void *opaque);
void replay_analysis_effect_save(void *opaque);
void *replay_analysis_effect_load(void);
void replay_analysis_effect_run(void *opaque);
void replay_analysis_effect_cancel(void *opaque);

/*
 * Guest writes made by a callback (include/panda/guest-write.h), one
 * EVENT_GUEST_WRITE at the callback's instruction. A site is a point where
 * mutating callbacks run on the vCPU thread at a deterministic instruction (a
 * guest hypercall, for now). Recording brackets the mutators with begin/end:
 * the write APIs below admit and log what they write, in order. A write into
 * guest state that does not go through them (a raw store into env) is not
 * logged, so replay diverges: it is unsupported. Replay does not
 * run the mutators: replay_guest_write_replay() reads the event and repeats
 * each write with the same call, so its translation side effects (TLB fills,
 * page-table accessed/dirty bits) happen again too. Writes outside these APIs
 * are refused during record/replay.
 */
#define REPLAY_GUEST_WRITE_BYTES_MAX (16 * 1024 * 1024)

typedef enum ReplayGuestSite {
    REPLAY_GUEST_SITE_HYPERCALL = 1,
} ReplayGuestSite;

/* Recording only, on the vCPU thread with the replay mutex: true if a scope
 * was opened, and replay_guest_write_end() must then close it. */
bool replay_guest_write_begin(CPUState *cpu, ReplayGuestSite site);
/* @handled and @ret: the site's answer (a hypercall's), replayed as recorded. */
void replay_guest_write_end(CPUState *cpu, bool handled, uint64_t ret);
/* Replaying only: apply the recorded writes; returns handled, sets *ret. */
bool replay_guest_write_replay(CPUState *cpu, ReplayGuestSite site, uint64_t *ret);

/* For guest-write APIs during record/replay: 0 (GUEST_WRITE_OK) if a write of
 * @length may go ahead now, else the refusal. While recording inside a scope on
 * this thread, within REPLAY_GUEST_WRITE_BYTES_MAX, for memory and registers
 * (@cpu NULL) or the env of the scope's vCPU (@cpu); while replay repeats a
 * write. An admitted write must be logged with the calls below. Without
 * record/replay there is no scope: callers apply their own policy. */
int replay_guest_write_admitted(CPUState *cpu, size_t length);
/* No-ops unless recording in a scope. @result is the write's GuestWriteResult. */
void replay_guest_write_log_vaddr(CPUState *cpu, uint64_t address,
                                  const void *data, size_t length, int result);
void replay_guest_write_log_ram(uint64_t address, const void *data, size_t length);
void replay_guest_write_log_reg(CPUState *cpu, int reg, const uint8_t *data,
                                int length, int result);

/*
 * The vCPU env's layout, for env writes: CPUArchState and the
 * structs reachable from it through pointer fields. Generated per target from
 * the bindgen bindings (tools/guest-layout/gen.py); registered by the target's
 * panda/src/guest-write.c.
 */
typedef struct GuestLayoutRange {
    uint32_t offset, size;
} GuestLayoutRange;

typedef struct GuestLayoutPointer {
    uint32_t offset;    /* of the pointer field */
    int32_t type;       /* pointee: index in GuestLayout.types, or -1 for scalars */
    uint32_t elem_size; /* pointee element size */
    int32_t length;     /* -1: one element; else GuestLayout.length() entry */
} GuestLayoutPointer;

typedef struct GuestLayoutType {
    const char *name;
    uint32_t size;
    const GuestLayoutPointer *follow;   /* pointers a path may follow */
    uint32_t nfollow;
    const GuestLayoutRange *host;       /* every host pointer, sorted: never written */
    uint32_t nhost;
} GuestLayoutType;

typedef struct GuestLayoutField {
    uint32_t offset, size;
    const char *name;
} GuestLayoutField;

typedef struct GuestLayout {
    uint32_t hash;
    const GuestLayoutType *types;       /* types[0] is CPUArchState */
    uint32_t ntypes;
    const GuestLayoutField *fields;     /* CPUArchState's, for messages */
    uint32_t nfields;
    size_t (*length)(CPUState *cpu, int entry);
    void (*sync)(CPUState *cpu);        /* rebuild state derived from env */
} GuestLayout;

void replay_guest_layout_register(const GuestLayout *layout);
/* Write @length bytes at @offset of what @derefs reach from @cpu's env, after
 * replay_guest_write_admitted(); logs it. GuestWriteResult. */
int replay_guest_write_env(CPUState *cpu, const uint32_t *derefs, unsigned n,
                           uint32_t offset, const void *data, uint32_t length);
int replay_guest_write_sync(CPUState *cpu);

#endif
