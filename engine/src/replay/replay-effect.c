/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "hw/core/cpu.h"
#include "qemu/rcu.h"
#include "exec/gdbstub.h"
#include "system/cpus.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "system/replay.h"
#include "system/replay-effect.h"
#include "system/analysis-policy.h"
#include "system/hw_accel.h"
#include "system/syscall-trap.h"
#include "exec/cputlb.h"
#include "qemu/error-report.h"
#include "panda/guest-write.h"
#include "replay-internal.h"

#define RAM_EFFECT_VERSION 1
#define RAM_EFFECT_WRITE 1
#define REG_EFFECT_WRITE 2
#define RAM_EFFECT_OK 0
#define RAM_EFFECT_DENIED 1

typedef struct ReplayRamEffect {
    uint8_t opcode;
    uint64_t address;   /* RAM: guest physical address */
    int32_t cpu_index;  /* register: vCPU and gdbstub register number */
    int32_t reg;
    uint32_t length;
    uint8_t result;
    bool prepared;
    bool applied;       /* register write already made while recording */
    bool pending;
    uint8_t data[];
} ReplayRamEffect;

static unsigned ram_effect_pending;

static bool ram_effect_shape(uint64_t address, uint32_t length)
{
    return length && length <= REPLAY_RAM_EFFECT_MAX &&
           address <= UINT64_MAX - (length - 1);
}

static bool reg_effect_shape(int32_t cpu_index, int32_t reg, uint32_t length)
{
    return cpu_index >= 0 && reg >= 0 && length && length <= REPLAY_REG_EFFECT_MAX;
}

/* BQL + replay mutex prevent topology/guest execution between preflight,
 * serialization and apply. Reject ROM/RAM devices: no MMIO/partial writes. */
static bool ram_effect_preflight(const ReplayRamEffect *effect)
{
    uint64_t address = effect->address;
    hwaddr remaining = effect->length;
    RCU_READ_LOCK_GUARD();
    while (remaining) {
        hwaddr offset, chunk = remaining;
        MemoryRegion *mr = address_space_translate(&address_space_memory, address,
            &offset, &chunk, true, MEMTXATTRS_UNSPECIFIED);
        if (!chunk || !memory_region_is_ram(mr) || mr->readonly ||
            memory_region_is_ram_device(mr) || memory_region_is_romd(mr) ||
            !memory_access_is_direct(mr, true, MEMTXATTRS_UNSPECIFIED)) {
            return false;
        }
        address += chunk;
        remaining -= chunk;
    }
    return true;
}

/* The vCPU exists and the register's gdbstub size is exactly the payload. */
static CPUState *reg_effect_cpu(const ReplayRamEffect *effect)
{
    CPUState *cpu = qemu_get_cpu(effect->cpu_index);
    if (!cpu || effect->reg >= cpu->gdb_num_regs) {
        return NULL;
    }
    GByteArray *buf = g_byte_array_new();
    int size = gdb_read_register(cpu, buf, effect->reg);
    g_byte_array_free(buf, true);
    return size > 0 && (uint32_t)size == effect->length ? cpu : NULL;
}

/* A register's setter may still refuse the write (ARM sysregs, some aarch64
 * setters such as pauth's, return 0), so only the write itself says whether it
 * applies; it is deterministic in the CPU state the replay reproduces. */
static bool reg_effect_write(const ReplayRamEffect *effect)
{
    CPUState *cpu = reg_effect_cpu(effect);
    return cpu && gdb_write_register(cpu, (uint8_t *)effect->data, effect->reg) ==
                  (int)effect->length;
}

static bool effect_admissible(void)
{
    CPUState *cpu;
    if (!qemu_in_main_thread() || current_cpu || !bql_locked() ||
        !replay_mutex_locked() || replay_mode != REPLAY_MODE_RECORD ||
        !replay_events_enabled() || !replay_file ||
        !analysis_policy_has(ANALYSIS_CAP_RR) ||
        ram_effect_pending >= REPLAY_RAM_EFFECT_PENDING_MAX) {
        return false;
    }
    CPU_FOREACH(cpu) {
        if (!cpu_is_stopped(cpu)) return false;
    }
    return true;
}

static void effect_queue(ReplayRamEffect *effect, const uint8_t *data)
{
    effect->pending = true;
    memcpy(effect->data, data, effect->length);
    ram_effect_pending++;
    replay_add_event(REPLAY_ASYNC_EVENT_ANALYSIS_EFFECT, effect, NULL, 0);
}

bool replay_analysis_ram_write_request(uint64_t address, const uint8_t *data,
                                        uint32_t length)
{
    if (!data || !ram_effect_shape(address, length) || !effect_admissible()) {
        return false;
    }
    ReplayRamEffect *effect = g_malloc0(sizeof(*effect) + length);
    effect->opcode = RAM_EFFECT_WRITE;
    effect->address = address;
    effect->length = length;
    effect_queue(effect, data);
    return true;
}

bool replay_analysis_reg_write_request(int cpu_index, int reg,
                                        const uint8_t *data, uint32_t length)
{
    if (!data || !reg_effect_shape(cpu_index, reg, length) || !effect_admissible()) {
        return false;
    }
    ReplayRamEffect *effect = g_malloc0(sizeof(*effect) + length);
    effect->opcode = REG_EFFECT_WRITE;
    effect->cpu_index = cpu_index;
    effect->reg = reg;
    effect->length = length;
    effect_queue(effect, data);
    return true;
}

/* Recording: called just before the event is saved and run, with nothing in
 * between, so a register write is made here and the log records whether it took. */
void replay_analysis_effect_prepare(void *opaque)
{
    ReplayRamEffect *effect = opaque;
    g_assert(bql_locked() && replay_mutex_locked());
    if (effect->opcode == REG_EFFECT_WRITE) {
        effect->result = reg_effect_write(effect) ? RAM_EFFECT_OK : RAM_EFFECT_DENIED;
        effect->applied = true;
    } else {
        effect->result = ram_effect_preflight(effect) ? RAM_EFFECT_OK : RAM_EFFECT_DENIED;
    }
    effect->prepared = true;
}

void replay_analysis_effect_save(void *opaque)
{
    ReplayRamEffect *effect = opaque;
    g_assert(effect->prepared);
    replay_put_byte(RAM_EFFECT_VERSION);
    replay_put_byte(effect->opcode);
    replay_put_byte(effect->result);
    if (effect->opcode == REG_EFFECT_WRITE) {
        replay_put_dword(effect->cpu_index);
        replay_put_dword(effect->reg);
    } else {
        replay_put_qword(effect->address);
    }
    replay_put_dword(effect->length);
    /* Explicit bounded length above, no second unbounded array length. */
    for (uint32_t i = 0; i < effect->length; i++) replay_put_byte(effect->data[i]);
}

void *replay_analysis_effect_load(void)
{
    if (!analysis_policy_has(ANALYSIS_CAP_RR)) {
        replay_sync_error("analysis RAM effect requires RR capability");
    }
    uint8_t version = replay_get_byte();
    uint8_t opcode = replay_get_byte();
    uint8_t result = replay_get_byte();
    uint64_t address = 0;
    int32_t cpu_index = 0, reg = 0;
    if (opcode == REG_EFFECT_WRITE) {
        cpu_index = (int32_t)replay_get_dword();
        reg = (int32_t)replay_get_dword();
    } else {
        address = replay_get_qword();
    }
    uint32_t length = replay_get_dword();
    if (version != RAM_EFFECT_VERSION || result > RAM_EFFECT_DENIED ||
        !(opcode == RAM_EFFECT_WRITE ? ram_effect_shape(address, length) :
          opcode == REG_EFFECT_WRITE && reg_effect_shape(cpu_index, reg, length))) {
        replay_sync_error("invalid analysis RAM effect payload");
    }
    ReplayRamEffect *effect = g_malloc0(sizeof(*effect) + length);
    effect->opcode = opcode;
    effect->address = address;
    effect->cpu_index = cpu_index;
    effect->reg = reg;
    effect->length = length;
    effect->result = result;
    effect->prepared = true;
    for (uint32_t i = 0; i < length; i++) effect->data[i] = replay_get_byte();
    return effect;
}

void replay_analysis_effect_run(void *opaque)
{
    ReplayRamEffect *effect = opaque;
    g_assert(bql_locked() && replay_mutex_locked() && effect->prepared);
    if (effect->opcode == REG_EFFECT_WRITE) {
        /* Between instruction budgets the vCPU is not inside a TB; gdbstub
         * writes go through the target's own register setter. A write that
         * was refused while recording is not attempted in replay. */
        if (!effect->applied && effect->result == RAM_EFFECT_OK &&
            !reg_effect_write(effect)) {
            replay_sync_error("analysis register effect was applied in the recording "
                              "but its write fails in replay");
        }
        replay_analysis_effect_cancel(effect);
        return;
    }
    uint8_t result = ram_effect_preflight(effect) ? RAM_EFFECT_OK : RAM_EFFECT_DENIED;
    if (result != effect->result) replay_sync_error("analysis RAM effect preflight mismatch");
    if (result == RAM_EFFECT_OK) {
        /* Standard RAM path invalidates translated code and sets dirty bits
         * (including fast-snapshot dirty tracking); do not memcpy raw RAM. */
        if (address_space_write(&address_space_memory, effect->address,
                MEMTXATTRS_UNSPECIFIED, effect->data, effect->length) != MEMTX_OK) {
            replay_sync_error("analysis RAM effect write failed after preflight");
        }
    }
    replay_analysis_effect_cancel(effect);
}

void replay_analysis_effect_cancel(void *opaque)
{
    ReplayRamEffect *effect = opaque;
    if (effect->pending) {
        g_assert(ram_effect_pending);
        ram_effect_pending--;
    }
    g_free(effect);
}

/* Guest writes made by callbacks */

#define GUEST_WRITE_VERSION 1
enum {
    GUEST_OP_VADDR = 1, /* penguin_syscall_write_memory: syscall_trap_access */
    GUEST_OP_RAM,       /* PANDA physical/virtual write, by guest physical address */
    GUEST_OP_REG,       /* register setter, gdbstub register number */
    GUEST_OP_ENV,       /* bytes in env, or through pointers from it */
    GUEST_OP_SYNC,      /* rebuild what the target derives from env */
};

typedef struct GuestOp {
    uint8_t op;
    uint8_t result;     /* GuestWriteResult while recording */
    int32_t cpu_index;
    int32_t reg;
    uint64_t address;   /* VADDR, RAM; ENV: offset in the last struct */
    uint8_t nderefs;
    uint32_t derefs[GUEST_WRITE_PATH_MAX];
    uint32_t length;
    uint8_t *data;
} GuestOp;

typedef struct GuestScope {
    CPUState *cpu;
    ReplayGuestSite site;
    bool applying;      /* replay repeating the logged writes */
    size_t bytes;
    GuestOp *ops;
    unsigned nops, capacity;
} GuestScope;

static __thread GuestScope *guest_scope;
static const GuestLayout *guest_layout;

void replay_guest_layout_register(const GuestLayout *layout)
{
    guest_layout = layout;
}

static void guest_add(GuestScope *scope, const GuestOp *op)
{
    if (scope->nops == scope->capacity) {
        scope->capacity = scope->capacity ? scope->capacity * 2 : 8;
        scope->ops = g_renew(GuestOp, scope->ops, scope->capacity);
    }
    scope->ops[scope->nops++] = *op;
}

static void guest_scope_free(GuestScope *scope)
{
    for (unsigned i = 0; i < scope->nops; i++) {
        g_free(scope->ops[i].data);
    }
    g_free(scope->ops);
    g_free(scope);
}

static void guest_log(GuestOp op, const void *data)
{
    GuestScope *scope = guest_scope;
    if (!scope || scope->applying) {
        return;
    }
    /* Admission checked the budget; a write that skipped it is a bug. */
    g_assert(op.length <= REPLAY_GUEST_WRITE_BYTES_MAX - scope->bytes);
    op.data = g_malloc(op.length);
    if (op.length) {
        memcpy(op.data, data, op.length);
    }
    guest_add(scope, &op);
    scope->bytes += op.length;
}

int replay_guest_write_admitted(CPUState *cpu, size_t length)
{
    GuestScope *scope = guest_scope;

    if (scope && scope->applying) {
        return GUEST_WRITE_OK;
    }
    if (!scope || (cpu && cpu != scope->cpu)) {
        return GUEST_WRITE_EPERM;
    }
    return length <= REPLAY_GUEST_WRITE_BYTES_MAX - scope->bytes ?
           GUEST_WRITE_OK : GUEST_WRITE_E2BIG;
}

void replay_guest_write_log_vaddr(CPUState *cpu, uint64_t address,
                                  const void *data, size_t length, int result)
{
    guest_log((GuestOp){ .op = GUEST_OP_VADDR, .result = result,
                         .cpu_index = cpu ? cpu->cpu_index : -1, .reg = -1,
                         .address = address, .length = length }, data);
}

void replay_guest_write_log_ram(uint64_t address, const void *data, size_t length)
{
    guest_log((GuestOp){ .op = GUEST_OP_RAM, .result = GUEST_WRITE_OK,
                         .cpu_index = -1, .reg = -1, .address = address,
                         .length = length }, data);
}

void replay_guest_write_log_reg(CPUState *cpu, int reg, const uint8_t *data,
                                int length, int result)
{
    guest_log((GuestOp){ .op = GUEST_OP_REG, .result = result,
                         .cpu_index = cpu ? cpu->cpu_index : -1, .reg = reg,
                         .length = MAX(length, 0) }, data);
}

/* Env writes */

static const GuestLayoutPointer *guest_follow(const GuestLayoutType *type,
                                              uint32_t offset)
{
    for (uint32_t i = 0; i < type->nfollow; i++) {
        if (type->follow[i].offset == offset) {
            return &type->follow[i];
        }
    }
    return NULL;
}

static bool guest_overlaps_host(const GuestLayoutType *type, uint64_t offset,
                                uint64_t length)
{
    uint32_t lo = 0, hi = type->nhost;
    while (lo < hi) {   /* the first range ending after offset */
        uint32_t mid = lo + (hi - lo) / 2;
        const GuestLayoutRange *r = &type->host[mid];
        if ((uint64_t)r->offset + r->size <= offset) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo < type->nhost && type->host[lo].offset < offset + length;
}

/* Where a path leads: each step follows a pointer field the layout lists for
 * the struct reached so far; the range must fit the last one and miss its host
 * pointers. Same answer in record and replay, which have the same layout. */
static int guest_env_target(CPUState *cpu, const uint32_t *derefs, unsigned n,
                            uint32_t offset, uint32_t length, uint8_t **target)
{
    const GuestLayout *layout = guest_layout;
    const GuestLayoutType *type;
    uint8_t *base;
    uint64_t size;

    if (!layout || !cpu || n > GUEST_WRITE_PATH_MAX || !length) {
        return GUEST_WRITE_EINVAL;
    }
    type = &layout->types[0];
    base = (uint8_t *)cpu_env(cpu);
    size = type->size;
    for (unsigned i = 0; i < n; i++) {
        const GuestLayoutPointer *p = type ? guest_follow(type, derefs[i]) : NULL;
        void *next;
        if (!p) {
            return GUEST_WRITE_EINVAL;
        }
        memcpy(&next, base + p->offset, sizeof(next));
        if (!next) {
            return GUEST_WRITE_EFAULT;
        }
        base = next;
        if (p->type >= 0) {
            type = &layout->types[p->type];
            size = type->size;
        } else {
            type = NULL;
            size = (uint64_t)p->elem_size *
                   (p->length >= 0 ? layout->length(cpu, p->length) : 1);
        }
    }
    if ((uint64_t)offset + length > size ||
        (type && guest_overlaps_host(type, offset, length))) {
        return GUEST_WRITE_EINVAL;
    }
    *target = base + offset;
    return GUEST_WRITE_OK;
}

int replay_guest_write_env(CPUState *cpu, const uint32_t *derefs, unsigned n,
                           uint32_t offset, const void *data, uint32_t length)
{
    uint8_t *target;
    int result = guest_env_target(cpu, derefs, n, offset, length, &target);
    GuestOp op = {
        .op = GUEST_OP_ENV, .result = result, .cpu_index = cpu->cpu_index,
        .reg = -1, .address = offset, .nderefs = n, .length = length,
    };

    /* A malformed path changes nothing and fails the same way in replay */
    if (result == GUEST_WRITE_EINVAL) {
        return result;
    }
    if (result == GUEST_WRITE_OK) {
        memcpy(target, data, length);
    }
    if (n) {
        memcpy(op.derefs, derefs, n * sizeof(*derefs));
    }
    guest_log(op, data);
    return result;
}

int replay_guest_write_sync(CPUState *cpu)
{
    if (!cpu) {
        return GUEST_WRITE_EINVAL;
    }
    if (guest_layout && guest_layout->sync) {
        guest_layout->sync(cpu);
    }
    tlb_flush(cpu);
    guest_log((GuestOp){ .op = GUEST_OP_SYNC, .result = GUEST_WRITE_OK,
                         .cpu_index = cpu->cpu_index, .reg = -1 }, NULL);
    return GUEST_WRITE_OK;
}

static int guest_apply(const GuestOp *o)
{
    CPUState *cpu = qemu_get_cpu(o->cpu_index);
    vaddr bad;

    switch (o->op) {
    case GUEST_OP_VADDR:
        return cpu && syscall_trap_access(cpu, o->address, o->data, o->length,
                                          true, &bad) ?
               GUEST_WRITE_OK : GUEST_WRITE_EFAULT;
    case GUEST_OP_RAM:
        return address_space_write(&address_space_memory, o->address,
                                   MEMTXATTRS_UNSPECIFIED, o->data,
                                   o->length) == MEMTX_OK ?
               GUEST_WRITE_OK : GUEST_WRITE_EFAULT;
    case GUEST_OP_REG:
        if (!cpu) {
            return GUEST_WRITE_EINVAL;
        }
        return gdb_write_register(cpu, o->data, o->reg) > 0 ?
               GUEST_WRITE_OK : GUEST_WRITE_EACCES;
    case GUEST_OP_ENV:
        return replay_guest_write_env(cpu, o->derefs, o->nderefs, o->address,
                                      o->data, o->length);
    case GUEST_OP_SYNC:
        return replay_guest_write_sync(cpu);
    }
    return GUEST_WRITE_EINVAL;
}

/* The scope */

static bool guest_env_written(const GuestScope *scope)
{
    for (unsigned i = 0; i < scope->nops; i++) {
        if (scope->ops[i].op == GUEST_OP_ENV &&
            scope->ops[i].result == GUEST_WRITE_OK) {
            return true;
        }
    }
    return false;
}

bool replay_guest_write_begin(CPUState *cpu, ReplayGuestSite site)
{
    if (replay_mode != REPLAY_MODE_RECORD || !cpu) {
        return false;
    }
    g_assert(!guest_scope && replay_mutex_locked());
    cpu_synchronize_state(cpu);
    GuestScope *scope = g_malloc0(sizeof(*scope));
    scope->cpu = cpu;
    scope->site = site;
    guest_scope = scope;
    return true;
}

void replay_guest_write_end(CPUState *cpu, bool handled, uint64_t ret)
{
    GuestScope *scope = guest_scope;
    g_assert(scope && scope->cpu == cpu && replay_mutex_locked());

    guest_scope = NULL;
    /* Raw env bytes can stale what the TLB cached; replay flushes here too */
    if (guest_env_written(scope)) {
        tlb_flush(cpu);
    }

    replay_save_instructions();
    replay_put_event(EVENT_GUEST_WRITE);
    replay_put_byte(GUEST_WRITE_VERSION);
    replay_put_word(scope->site);
    replay_put_dword(guest_layout ? guest_layout->hash : 0);
    replay_put_byte(handled);
    replay_put_qword(ret);
    replay_put_dword(scope->nops);
    for (unsigned i = 0; i < scope->nops; i++) {
        const GuestOp *o = &scope->ops[i];
        replay_put_byte(o->op);
        replay_put_byte(o->result);
        replay_put_dword(o->cpu_index);
        replay_put_dword(o->reg);
        replay_put_qword(o->address);
        replay_put_byte(o->nderefs);
        for (unsigned d = 0; d < o->nderefs; d++) {
            replay_put_dword(o->derefs[d]);
        }
        /* Explicit bounded length, as for deferred effects */
        replay_put_dword(o->length);
        for (uint32_t b = 0; b < o->length; b++) replay_put_byte(o->data[b]);
    }
    guest_scope_free(scope);
}

static G_NORETURN void guest_invalid(GuestScope *scope, const char *error)
{
    guest_scope = NULL;
    guest_scope_free(scope);
    replay_sync_error(error);
}

bool replay_guest_write_replay(CPUState *cpu, ReplayGuestSite site, uint64_t *ret)
{
    g_assert(replay_mode == REPLAY_MODE_PLAY && replay_mutex_locked() &&
             !guest_scope);
    replay_account_executed_instructions();
    if (!replay_next_event_is(EVENT_GUEST_WRITE)) {
        replay_sync_error("missing guest-write event in the replay log");
    }
    GuestScope *scope = g_malloc0(sizeof(*scope));
    scope->cpu = cpu;
    scope->site = site;
    if (replay_get_byte() != GUEST_WRITE_VERSION) {
        guest_invalid(scope, "invalid guest-write payload");
    }
    if (replay_get_word() != site) {
        guest_invalid(scope, "the guest-write event is for another kind of site");
    }
    uint32_t hash = replay_get_dword();
    bool handled = replay_get_byte();
    *ret = replay_get_qword();
    uint32_t n = replay_get_dword();
    bool env_ops = false;
    for (uint32_t i = 0; i < n; i++) {
        GuestOp o = { 0 };
        o.op = replay_get_byte();
        o.result = replay_get_byte();
        o.cpu_index = (int32_t)replay_get_dword();
        o.reg = (int32_t)replay_get_dword();
        o.address = replay_get_qword();
        o.nderefs = replay_get_byte();
        if (o.op < GUEST_OP_VADDR || o.op > GUEST_OP_SYNC ||
            o.nderefs > GUEST_WRITE_PATH_MAX) {
            guest_invalid(scope, "invalid guest-write payload");
        }
        for (unsigned d = 0; d < o.nderefs; d++) {
            o.derefs[d] = replay_get_dword();
        }
        o.length = replay_get_dword();
        if (o.length > REPLAY_GUEST_WRITE_BYTES_MAX - scope->bytes) {
            guest_invalid(scope, "invalid guest-write payload");
        }
        scope->bytes += o.length;
        o.data = g_malloc(o.length);
        for (uint32_t b = 0; b < o.length; b++) o.data[b] = replay_get_byte();
        guest_add(scope, &o);
        env_ops |= o.op == GUEST_OP_ENV || o.op == GUEST_OP_SYNC;
    }
    replay_finish_event();
    if (env_ops && (!guest_layout || hash != guest_layout->hash)) {
        guest_invalid(scope, "the recording wrote CPU state with a different "
                      "CPU layout (another engine build)");
    }

    cpu_synchronize_state(cpu);
    scope->applying = true;
    guest_scope = scope;
    for (unsigned i = 0; i < scope->nops; i++) {
        if (guest_apply(&scope->ops[i]) != scope->ops[i].result) {
            guest_invalid(scope, "guest write: a write's result differs in replay");
        }
    }
    guest_scope = NULL;
    if (guest_env_written(scope)) {
        tlb_flush(cpu);
    }
    guest_scope_free(scope);
    return handled;
}
