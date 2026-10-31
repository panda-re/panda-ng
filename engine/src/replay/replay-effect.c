/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "hw/core/cpu.h"
#include "qemu/rcu.h"
#include "system/cpus.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "system/replay.h"
#include "system/replay-effect.h"
#include "system/analysis-policy.h"
#include "replay-internal.h"

#define RAM_EFFECT_VERSION 1
#define RAM_EFFECT_WRITE 1
#define RAM_EFFECT_OK 0
#define RAM_EFFECT_DENIED 1

typedef struct ReplayRamEffect {
    uint64_t address;
    uint32_t length;
    uint8_t result;
    bool prepared;
    bool pending;
    uint8_t data[];
} ReplayRamEffect;

static unsigned ram_effect_pending;

static bool ram_effect_shape(uint64_t address, uint32_t length)
{
    return length && length <= REPLAY_RAM_EFFECT_MAX &&
           address <= UINT64_MAX - (length - 1);
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

bool replay_analysis_ram_write_request(uint64_t address, const uint8_t *data,
                                        uint32_t length)
{
    CPUState *cpu;
    if (!qemu_in_main_thread() || current_cpu || !bql_locked() ||
        !replay_mutex_locked() || replay_mode != REPLAY_MODE_RECORD ||
        !replay_events_enabled() || !replay_file ||
        !analysis_policy_has(ANALYSIS_CAP_RR) || !data ||
        !ram_effect_shape(address, length) ||
        ram_effect_pending >= REPLAY_RAM_EFFECT_PENDING_MAX) {
        return false;
    }
    CPU_FOREACH(cpu) {
        if (!cpu_is_stopped(cpu)) return false;
    }
    ReplayRamEffect *effect = g_malloc0(sizeof(*effect) + length);
    effect->address = address;
    effect->length = length;
    effect->pending = true;
    memcpy(effect->data, data, length);
    ram_effect_pending++;
    replay_add_event(REPLAY_ASYNC_EVENT_ANALYSIS_EFFECT, effect, NULL, 0);
    return true;
}

void replay_analysis_effect_prepare(void *opaque)
{
    ReplayRamEffect *effect = opaque;
    g_assert(bql_locked() && replay_mutex_locked());
    effect->result = ram_effect_preflight(effect) ? RAM_EFFECT_OK : RAM_EFFECT_DENIED;
    effect->prepared = true;
}

void replay_analysis_effect_save(void *opaque)
{
    ReplayRamEffect *effect = opaque;
    g_assert(effect->prepared);
    replay_put_byte(RAM_EFFECT_VERSION);
    replay_put_byte(RAM_EFFECT_WRITE);
    replay_put_byte(effect->result);
    replay_put_qword(effect->address);
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
    uint64_t address = replay_get_qword();
    uint32_t length = replay_get_dword();
    if (version != RAM_EFFECT_VERSION || opcode != RAM_EFFECT_WRITE ||
        result > RAM_EFFECT_DENIED || !ram_effect_shape(address, length)) {
        replay_sync_error("invalid analysis RAM effect payload");
    }
    ReplayRamEffect *effect = g_malloc0(sizeof(*effect) + length);
    effect->address = address;
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
