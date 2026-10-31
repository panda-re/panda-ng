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
