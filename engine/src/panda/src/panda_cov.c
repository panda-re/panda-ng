/*
 * Edge coverage for fuzzing loops: AFL's map[cur ^ prev]++, prev = cur >> 1, over translated
 * blocks, in a byte map in host memory that C (panda_cov_map) and pandare2
 * (Panda.fast_coverage_map) read directly.
 *
 * The scheme is AFL's (the edge hash, the byte counters, a power-of-two map, 64 KiB by default) so
 * that anything that consumes an AFL map can read this one; no AFL code is used.
 *
 * Block id (cur): a hash of the block's physical address (ram_addr of its first instruction),
 * masked to the map. Not its virtual address: a CF_PCREL block (x86_64, aarch64) is one TB for
 * every virtual address that maps its code, translated once, so an id baked in at translation can
 * only be one that all of those addresses share. Physical addresses are the same in every lap of a
 * fast-restore loop (RAM is the snapshot's), so ids are stable within a run of a snapshot. A block
 * not in RAM (code run from a ROM device in I/O mode) falls back to its virtual address.
 *
 * Filter: [lo, hi) on the block's virtual address at translation (hi == 0: every block), e.g.
 * user space only. A filtered block is not instrumented (costs nothing) and doesn't update prev.
 *
 * How a block logs its edge: a QEMU plugin callback (the PANDA bridge,
 * contrib/plugins/panda_plugin_interface.c, registers one per block while coverage is on, with
 * the block's id as its data and no register sync), which runs panda_cov_hit's three lines.
 * It gives the same maps as emitting the update as TCG ops into each block (rehosting/qemu_builder
 * fastsnap's way); the two costs have not been compared.
 *
 * Across a fast restore: prev is execution state and goes back to its value at panda_fast_snap,
 * so every lap's first edge is the same edge. The map is the caller's and a restore leaves it
 * alone: a fuzzer reads a lap's map after the lap and before it restores, then clears it
 * (panda_cov_clear), or keeps it to accumulate coverage over many laps. Rewinding it with the
 * snapshot would count the edges run before the snapshot into every lap.
 *
 * Turning coverage on or off, or changing the filter, flushes the translated code (blocks are
 * instrumented when they are translated): from the main loop with the VM stopped at once,
 * otherwise before the vCPUs run on.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "exec/translation-block.h"
#include "exec/tb-flush.h"
#include "exec/translator.h"
#include "tcg/tcg.h"
#include "hw/core/cpu.h"
#include "system/runstate.h"
#include "panda/plugin.h"
#include "panda/panda_api.h"
#include "panda/panda_qemu_plugin_helpers.h"

#define COV_MAP_MIN (1u << 8)
#define COV_MAP_MAX (1u << 24)

PandaCov panda_cov;
static uint32_t cov_size = PANDA_COV_MAP_DEFAULT;
static uint32_t cov_snap_prev[PANDA_COV_MAX_CPUS];
static bool cov_snap_valid;

/* splitmix64's finalizer: any fixed mix would do, the same address gives the same id every run */
static uint32_t cov_hash(uint64_t key)
{
    uint64_t z = key + 0x9e3779b97f4a7c15ULL;

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return (uint32_t)(z ^ (z >> 31));
}

/* Blocks are instrumented when translated: drop the translations made with the old setting. */
static void cov_flush(void)
{
    if (!runstate_is_running()) {
        tb_flush__exclusive_or_serial();
    } else if (first_cpu) {
        queue_tb_flush(first_cpu);
    }
}

int panda_cov_enable(uint32_t map_size, uint64_t lo, uint64_t hi)
{
    if (map_size < COV_MAP_MIN || map_size > COV_MAP_MAX || (map_size & (map_size - 1))) {
        error_report("panda_cov_enable: map size %u is not a power of two in [%u, %u]",
                     map_size, COV_MAP_MIN, COV_MAP_MAX);
        return -1;
    }
    if (panda_cov.map && map_size != cov_size) {
        /* blocks already translated hold the old mask (and, with emitted ops, the pointer) */
        error_report("panda_cov_enable: the map is already %u bytes; its size is fixed once made",
                     cov_size);
        return -1;
    }
    if (!panda_cov.map) {
        cov_size = map_size;
        panda_cov.map = g_malloc0(cov_size);
        panda_cov.mask = cov_size - 1;
    }
    panda_cov.lo = lo;
    panda_cov.hi = hi;
    memset(panda_cov.prev, 0, sizeof(panda_cov.prev));
    panda_cov.on = true;
    cov_flush();
    return 0;
}

void panda_cov_disable(void)
{
    if (panda_cov.on) {
        panda_cov.on = false;
        cov_flush();
    }
}

uint8_t *panda_cov_map(void)
{
    return panda_cov.map;
}

uint32_t panda_cov_map_size(void)
{
    return panda_cov.map ? cov_size : 0;
}

void panda_cov_clear(void)
{
    if (panda_cov.map) {
        memset(panda_cov.map, 0, cov_size);
    }
}

/* At translation (the bridge's tb_trans): should this block log its edge, and with which id? */
bool panda_cov_block(uint32_t *cur)
{
    const DisasContextBase *db = tcg_ctx->plugin_db;
    TranslationBlock *tb = db->tb;
    uint64_t pc = db->pc_first;
    tb_page_addr_t pa;

    if (!panda_cov.on || (panda_cov.hi && (pc < panda_cov.lo || pc >= panda_cov.hi))) {
        return false;
    }
    pa = tb_page_addr0(tb);
    *cur = cov_hash(pa != (tb_page_addr_t)-1 ? pa : pc) & panda_cov.mask;
    return true;
}

/* panda_fast_snap and panda_fast_restore: prev is execution state, the map is not. */
void panda_cov_snap(void)
{
    memcpy(cov_snap_prev, panda_cov.prev, sizeof(cov_snap_prev));
    cov_snap_valid = true;
}

void panda_cov_restore(void)
{
    if (cov_snap_valid) {
        memcpy(panda_cov.prev, cov_snap_prev, sizeof(cov_snap_prev));
    }
}
