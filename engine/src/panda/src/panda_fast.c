/*
 * panda_fast_snap / panda_fast_restore: a snapshot of RAM and device state in host memory, for
 * fuzzing loops. No disk state.
 *
 * Device state: the devices' vmstate (every non-iterable savevm section, no RAM) in a buffer,
 * loaded after a system reset, as COLO checkpoints do.
 *
 * RAM: the snapshot keeps a copy of every RAM block. A restore copies back only the target pages
 * written since the last snapshot or restore. Those come from QEMU's DIRTY_MEMORY_MIGRATION bitmap,
 * the one live migration uses: TCG sets it on the first store to a clean page (notdirty_write) and
 * DMA/device writes set it through invalidate_and_set_dirty, so it covers both. It is armed with
 * GLOBAL_DIRTY_DIRTY_RATE, not GLOBAL_DIRTY_MIGRATION: savevm/loadvm take and drop
 * GLOBAL_DIRTY_MIGRATION themselves, and a savevm's log_stop would turn our tracking off. (Same
 * choice as rehosting/qemu_builder's fastsnap dirty-track.c.)
 *
 * The bitmap is shared with migration, which consumes it (migration_bitmap_sync clears it). So
 * each tracked block keeps its first page's bit set as a sentinel: a restore that finds a sentinel
 * clear, or the dirty-rate flag off, knows someone else read the bitmap and copies everything back
 * once (then re-arms). A loadvm writes RAM without dirty bits, so after_loadvm marks the tracking
 * invalid the same way.
 *
 * Blocks the bitmap can't vouch for are compared page by page instead: ROM devices (pflash in I/O
 * mode writes its storage directly, unlogged) and blocks that aren't migratable (the DMA path
 * doesn't log them).
 *
 * Cost when no fast snapshot exists: none. Nothing is armed, every page's migration bit stays set
 * from RAM creation (and TCG's store path keeps setting it), so no TLB entry gets TLB_NOTDIRTY for
 * it and nothing here runs. While armed: one slow-path store per page written per iteration.
 *
 * Main loop, BQL held; the VM is stopped for snap and restore.
 */
#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "qemu/rcu.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "migration/savevm.h"
#include "migration/qemu-file.h"
#include "io/channel-buffer.h"
#include "system/memory.h"
#include "system/physmem.h"
#include "system/ramblock.h"
#include "system/ramlist.h"
#include "system/runstate.h"
#include "system/system.h"
#include "system/cpus.h"
#include "exec/target_page.h"
#include "exec/tb-flush.h"
#include "exec/cputlb.h"
#include "exec/translation-block.h"
#include "hw/core/cpu.h"
#ifdef TARGET_ARM
#include "target/arm/internals.h"
#endif
#include "panda/plugin.h"
#include "panda/panda_api.h"

typedef struct FastBlock {
    RAMBlock *rb;
    uint8_t *copy;
    ram_addr_t offset;  /* ram_addr of the block */
    size_t len;
    bool tracked;       /* restored from the dirty bitmap, else compared page by page */
} FastBlock;

static GArray *fast_blocks;     /* FastBlock */
static uint8_t *fast_dev;
static size_t fast_dev_len;
static bool fast_armed;         /* we hold GLOBAL_DIRTY_DIRTY_RATE */
static bool fast_valid;         /* the bitmap has every page written since RAM == the copy */

static struct {
    uint64_t snaps, restores, full_restores;
    uint64_t last_pages;        /* pages copied by the last restore */
    bool last_full;
    int64_t last_reset_us, last_ram_us, last_dev_us;  /* the last restore's parts */
} fast_stats;

static size_t page_size(void) { return qemu_target_page_size(); }

static bool block_trackable(RAMBlock *rb) {
    return qemu_ram_is_migratable(rb) && !rb->mr->rom_device;
}

/* A page's RAM changed under its TBs: invalidate them if it has any (as invalidate_and_set_dirty). */
static void fast_invalidate_code(ram_addr_t ra, size_t len) {
    if (physical_memory_range_includes_clean(ra, len, 1 << DIRTY_MEMORY_CODE)) {
        tb_invalidate_phys_range(NULL, ra, ra + len - 1);
    }
}

/*
 * Walk a tracked block's migration bits: clear each run of dirty pages and copy it, from the
 * snapshot to RAM (restore) or from RAM to the snapshot (snap). Returns the pages copied.
 */
static uint64_t fast_walk(FastBlock *b, bool restore) {
    DirtyMemoryBlocks *dmb = qatomic_rcu_read(&ram_list.dirty_memory[DIRTY_MEMORY_MIGRATION]);
    unsigned bits = qemu_target_page_bits();
    uint8_t *host = qemu_ram_get_host_addr(b->rb);
    uint64_t page = b->offset >> bits;
    uint64_t end = (b->offset + b->len + page_size() - 1) >> bits;
    uint64_t copied = 0;

    while (page < end) {
        unsigned long idx = page / DIRTY_MEMORY_BLOCK_SIZE;
        uint64_t base = idx * DIRTY_MEMORY_BLOCK_SIZE;
        unsigned long lim = MIN(end, base + DIRTY_MEMORY_BLOCK_SIZE) - base;
        unsigned long *map = dmb->blocks[idx];
        unsigned long o = find_next_bit(map, lim, page - base);

        while (o < lim) {
            unsigned long e = find_next_zero_bit(map, lim, o);
            ram_addr_t ra = (ram_addr_t)(base + o) << bits;
            size_t off = ra - b->offset;
            size_t len = MIN((size_t)(e - o) << bits, b->len - off);

            bitmap_test_and_clear_atomic(map, o, e - o);
            if (restore) {
                memcpy(host + off, b->copy + off, len);
                fast_invalidate_code(ra, len);
            } else {
                memcpy(b->copy + off, host + off, len);
            }
            copied += e - o;
            o = find_next_bit(map, lim, e);
        }
        page = base + lim;
    }
    return copied;
}

/* Untracked block: copy back the pages that differ from the snapshot. */
static uint64_t fast_compare_restore(FastBlock *b) {
    uint8_t *host = qemu_ram_get_host_addr(b->rb);
    size_t ps = page_size();
    uint64_t copied = 0;

    for (size_t off = 0; off < b->len; off += ps) {
        size_t len = MIN(ps, b->len - off);
        if (memcmp(host + off, b->copy + off, len)) {
            memcpy(host + off, b->copy + off, len);
            fast_invalidate_code(b->offset + off, len);
            copied++;
        }
    }
    return copied;
}

/* Every tracked block's first page stays dirty: if migration consumed the bitmap, it's clear. */
static void fast_set_sentinels(void) {
    for (guint i = 0; i < fast_blocks->len; i++) {
        FastBlock *b = &g_array_index(fast_blocks, FastBlock, i);
        if (b->tracked) {
            physical_memory_set_dirty_flag(b->offset, DIRTY_MEMORY_MIGRATION);
        }
    }
}

static bool fast_sentinels_ok(void) {
    for (guint i = 0; i < fast_blocks->len; i++) {
        FastBlock *b = &g_array_index(fast_blocks, FastBlock, i);
        if (b->tracked && !physical_memory_get_dirty_flag(b->offset, DIRTY_MEMORY_MIGRATION)) {
            return false;
        }
    }
    return true;
}

/* RAM equals the copy now: clear the tracked blocks' bits (re-arming TCG's TLB) and the sentinels. */
static void fast_rearm(void) {
    for (guint i = 0; i < fast_blocks->len; i++) {
        FastBlock *b = &g_array_index(fast_blocks, FastBlock, i);
        if (b->tracked) {
            physical_memory_test_and_clear_dirty(b->offset, b->len, DIRTY_MEMORY_MIGRATION, NULL);
        }
    }
    fast_set_sentinels();
    fast_valid = true;
}

/*
 * Pages whose bits were cleared must get TLB_NOTDIRTY back in TCG's TLBs, or stores through an
 * entry filled while the page was dirty go unlogged. The VM is stopped: the flush runs before
 * each vCPU executes again.
 */
static void fast_tlb_rearm(void) {
    CPUState *cs;
    CPU_FOREACH(cs) {
        tlb_flush(cs);
    }
}

/* Called when RAM changed without dirty bits (loadvm). */
void panda_fast_ram_untracked(void) {
    fast_valid = false;
}

static bool fast_tracking_ok(void) {
    return fast_valid && (global_dirty_tracking & GLOBAL_DIRTY_DIRTY_RATE) && fast_sentinels_ok();
}

static void fast_free_blocks(void) {
    if (fast_blocks) {
        for (guint i = 0; i < fast_blocks->len; i++) {
            g_free(g_array_index(fast_blocks, FastBlock, i).copy);
        }
        g_array_free(fast_blocks, true);
        fast_blocks = NULL;
    }
}

/* The snapshot's blocks are still RAM's blocks, same sizes, same order. */
static bool fast_layout_same(void) {
    RAMBlock *rb;
    guint i = 0;

    RCU_READ_LOCK_GUARD();
    RAMBLOCK_FOREACH(rb) {
        FastBlock *b;
        if (i >= fast_blocks->len) {
            return false;
        }
        b = &g_array_index(fast_blocks, FastBlock, i++);
        if (b->rb != rb || b->len != qemu_ram_get_used_length(rb) ||
            b->offset != qemu_ram_get_offset(rb)) {
            return false;
        }
    }
    return i == fast_blocks->len;
}

static void fast_copy_all(void) {
    RAMBlock *rb;

    fast_free_blocks();
    fast_blocks = g_array_new(false, false, sizeof(FastBlock));
    RCU_READ_LOCK_GUARD();
    RAMBLOCK_FOREACH(rb) {
        FastBlock b = {
            .rb = rb, .offset = qemu_ram_get_offset(rb), .len = qemu_ram_get_used_length(rb),
            .tracked = block_trackable(rb),
        };
        b.copy = g_malloc(b.len);
        memcpy(b.copy, qemu_ram_get_host_addr(rb), b.len);
        g_array_append_val(fast_blocks, b);
    }
}

static int fast_save_devices(void) {
    Error *err = NULL;
    QIOChannelBuffer *bioc = qio_channel_buffer_new(fast_dev_len ? fast_dev_len : 4096);
    QEMUFile *f = qemu_file_new_output(QIO_CHANNEL(bioc));

    if (qemu_save_device_state(f, &err) < 0) {
        error_report_err(err);
        qemu_fclose(f);
        object_unref(OBJECT(bioc));
        return -1;
    }
    qemu_fflush(f);
    g_free(fast_dev);
    fast_dev_len = bioc->usage;
    fast_dev = g_memdup2(bioc->data, fast_dev_len);
    qemu_fclose(f);
    object_unref(OBJECT(bioc));
    return 0;
}

static int fast_load_devices(void) {
    Error *err = NULL;
    QIOChannelBuffer *bioc = qio_channel_buffer_new(fast_dev_len);
    QEMUFile *f;
    int ret;

    memcpy(bioc->data, fast_dev, fast_dev_len);
    bioc->usage = fast_dev_len;
    f = qemu_file_new_input(QIO_CHANNEL(bioc));
    ret = qemu_load_device_state(f, &err);
    qemu_fclose(f);
    object_unref(OBJECT(bioc));
    if (ret < 0) {
        error_report_err(err);
        return -1;
    }
    return 0;
}

/*
 * Hold GLOBAL_DIRTY_DIRTY_RATE. Someone else may have dropped it since (calc-dirty-rate's
 * dirty-bitmap mode takes and drops the same flag): then the bitmap is stale too.
 */
static int fast_arm(void) {
    Error *err = NULL;

    if (fast_armed && (global_dirty_tracking & GLOBAL_DIRTY_DIRTY_RATE)) {
        return 0;
    }
    if (!memory_global_dirty_log_start(GLOBAL_DIRTY_DIRTY_RATE, &err)) {
        error_report_err(err);
        return -1;
    }
    fast_armed = true;
    fast_valid = false;
    return 0;
}

int panda_fast_snap(void) {
    if (runstate_is_running()) {
        vm_stop(RUN_STATE_SAVE_VM);
    }
    if (fast_arm() < 0) {
        return -1;
    }

    if (fast_blocks && fast_layout_same() && fast_tracking_ok()) {
        /* the copy differs from RAM only in the pages written since: copy those */
        for (guint i = 0; i < fast_blocks->len; i++) {
            FastBlock *b = &g_array_index(fast_blocks, FastBlock, i);
            if (b->tracked) {
                fast_walk(b, false);
            } else {
                memcpy(b->copy, qemu_ram_get_host_addr(b->rb), b->len);
            }
        }
        fast_set_sentinels();
        fast_tlb_rearm();
    } else {
        fast_copy_all();
        fast_rearm();
    }
    if (fast_save_devices() < 0) {
        return -1;
    }
    fast_stats.snaps++;
    return 0;
}

int panda_fast_restore(void) {
    CPUState *cs;
    bool full;
    int64_t t0, t1, t2;
    uint64_t pages = 0;

    if (!fast_blocks || !fast_dev) {
        return -1;
    }
    if (runstate_is_running()) {
        vm_stop(RUN_STATE_RESTORE_VM);
    }
    if (!fast_layout_same()) {
        error_report("panda_fast_restore: RAM blocks changed since panda_fast_snap");
        return -1;
    }
    full = !fast_tracking_ok();
    if (fast_arm() < 0) {
        return -1;
    }

    /*
     * Reset first, then RAM: reset re-copies ROM blobs that live in RAM (rom_reset), e.g. the
     * arm virt DTB at the base of RAM, which the guest has reused by now. loadvm does the same
     * (reset, then the RAM section). Those writes are logged, so the walk below undoes them.
     */
    t0 = g_get_monotonic_time();
    qemu_system_reset(SHUTDOWN_CAUSE_SNAPSHOT_LOAD);
    t1 = g_get_monotonic_time();

    if (full) {
        for (guint i = 0; i < fast_blocks->len; i++) {
            FastBlock *b = &g_array_index(fast_blocks, FastBlock, i);
            memcpy(qemu_ram_get_host_addr(b->rb), b->copy, b->len);
            pages += DIV_ROUND_UP(b->len, page_size());
        }
        tb_flush__exclusive_or_serial();  /* code in RAM may have changed under the TBs */
        fast_rearm();
        fast_stats.full_restores++;
    } else {
        for (guint i = 0; i < fast_blocks->len; i++) {
            FastBlock *b = &g_array_index(fast_blocks, FastBlock, i);
            pages += b->tracked ? fast_walk(b, true) : fast_compare_restore(b);
        }
        fast_set_sentinels();
        fast_tlb_rearm();
    }
    t2 = g_get_monotonic_time();
    fast_stats.restores++;
    fast_stats.last_pages = pages;
    fast_stats.last_full = full;

    if (fast_load_devices() < 0) {
        return -1;
    }
    fast_stats.last_reset_us = t1 - t0;
    fast_stats.last_ram_us = t2 - t1;
    fast_stats.last_dev_us = g_get_monotonic_time() - t2;
#ifdef TARGET_ARM
    /*
     * Recompute the generic timers once every device (the GIC) is loaded, as the KVM->TCG
     * handoff in target/arm/machine.c does. Not what fixed the aarch64 hang (that was the DTB
     * the reset wrote over RAM), but cheap insurance that a timer that had fired drives its line.
     */
    CPU_FOREACH(cs) {
        arm_gt_recalc_el1_timers(ARM_CPU(cs));
    }
#endif
    return 0;
}

void panda_fast_delete(void) {
    fast_free_blocks();
    g_free(fast_dev);
    fast_dev = NULL;
    fast_dev_len = 0;
    fast_valid = false;
    if (fast_armed) {
        memory_global_dirty_log_stop(GLOBAL_DIRTY_DIRTY_RATE);
        fast_armed = false;
    }
}

int64_t panda_fast_verify(void) {
    size_t ps = page_size();
    int64_t differ = 0;

    if (!fast_blocks) {
        return -1;
    }
    for (guint i = 0; i < fast_blocks->len; i++) {
        FastBlock *b = &g_array_index(fast_blocks, FastBlock, i);
        uint8_t *host = qemu_ram_get_host_addr(b->rb);
        for (size_t off = 0; off < b->len; off += ps) {
            if (memcmp(host + off, b->copy + off, MIN(ps, b->len - off))) {
                if (differ < 8) {
                    size_t j = 0;
                    while (host[off + j] == b->copy[off + j]) {
                        j++;
                    }
                    fprintf(stderr, "panda_fast_verify: %s+0x%zx differs at +0x%zx "
                            "(snapshot %02x, RAM %02x; migration bit %d, code bit %d)\n",
                            qemu_ram_get_idstr(b->rb), off, j, b->copy[off + j], host[off + j],
                            physical_memory_get_dirty_flag(b->offset + off,
                                                           DIRTY_MEMORY_MIGRATION),
                            physical_memory_get_dirty_flag(b->offset + off,
                                                           DIRTY_MEMORY_CODE));
                }
                differ++;
            }
        }
    }
    return differ;
}

const char *panda_fast_info(void) {
    static char buf[1024];
    size_t n;

    n = snprintf(buf, sizeof(buf),
                 "snaps=%" PRIu64 " restores=%" PRIu64 " full_restores=%" PRIu64
                 " last_pages=%" PRIu64 " last_full=%d last_us=%" PRId64 ",%" PRId64 ",%" PRId64
                 " tracking=%d blocks=",
                 fast_stats.snaps, fast_stats.restores, fast_stats.full_restores,
                 fast_stats.last_pages, fast_stats.last_full, fast_stats.last_reset_us,
                 fast_stats.last_ram_us, fast_stats.last_dev_us,
                 fast_blocks && fast_tracking_ok());
    for (guint i = 0; fast_blocks && i < fast_blocks->len && n < sizeof(buf); i++) {
        FastBlock *b = &g_array_index(fast_blocks, FastBlock, i);
        n += snprintf(buf + n, sizeof(buf) - n, "%s%s:%zu:%s", i ? "," : "",
                      qemu_ram_get_idstr(b->rb), b->len, b->tracked ? "dirty" : "compare");
    }
    return buf;
}
