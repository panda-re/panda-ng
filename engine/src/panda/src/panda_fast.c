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
 * Before reading the bitmap, snap and restore sync the dirty log (memory_global_dirty_log_sync),
 * as migration does: loggers other than TCG and QEMU's own DMA (vhost logs, KVM/HVF slots) hand
 * their pages over only then.
 *
 * Writers that can't log at all: a vhost device without VHOST_F_LOG_ALL, or a vhost-user backend
 * without a shared log (IGLOO's vhost-device-vsock has neither), writes guest RAM from another
 * process and nothing ever marks those pages. While such a device exists, a restore compares
 * every page with the snapshot instead (exact, slower: it reads all RAM), and the vhost patch
 * keeps global dirty logging from being turned on in that device (it would break it).
 *
 * The bitmap is shared with migration, which consumes it (migration_bitmap_sync clears it). A
 * precopy notifier (savevm and migration call it at setup and before each bitmap sync) and a
 * memory listener (global dirty logging stopped) mark the tracking invalid; as a backstop each
 * tracked block keeps its first page's bit set as a sentinel, and the dirty-rate flag must still
 * be held. An invalid bitmap makes the next restore copy everything back once (then re-arm).
 * qemu_loadvm_state writes RAM without dirty bits, so it marks the tracking invalid before it
 * starts (a load that fails partway leaves RAM half written).
 *
 * TCG only (KVM/HVF keep their own dirty state; clearing the bitmap here doesn't re-protect their
 * pages), and not under record/replay (a restore rewrites RAM and devices outside the log).
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
#include "migration/misc.h"
#include "hw/virtio/vhost.h"
#include "system/replay.h"
#include "system/tcg.h"
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
#include "hw/core/panda-modules.h"
#ifdef TARGET_ARM
#include "target/arm/internals.h"
#endif
#include "panda/plugin.h"
#include "panda/panda_api.h"
#include "panda/hooks.h"

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
static const char *fast_why;    /* why it isn't (the next restore copies all RAM) */
static bool fast_hooked;        /* the precopy notifier and memory listener are registered */

static struct {
    uint64_t snaps, restores, full_restores, compare_restores;
    uint64_t last_pages;        /* pages copied by the last restore */
    bool last_full;
    const char *last_mode;      /* dirty, full, or compare (an unlogged vhost device) */
    const char *last_why;       /* why the last full restore was full */
    uint64_t last_snap_pages;   /* pages copied by the last snapshot */
    const char *last_snap_mode; /* incremental, full, or compare */
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
    fast_why = NULL;
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

static void fast_invalidate(const char *why) {
    if (fast_valid) {
        fast_valid = false;
        fast_why = why;
    }
}

/* Called when RAM changed without dirty bits. */
void panda_fast_ram_untracked(void) {
    fast_invalidate("untracked");
}

/* qemu_loadvm_state is about to write RAM, unlogged (it may fail partway). */
void panda_before_loadvm(void) {
    fast_invalidate("loadvm");
}

/* savevm or migration is about to read (and clear) the migration bitmap. */
static int fast_precopy_notify(NotifierWithReturn *n, void *data, Error **errp) {
    PrecopyNotifyData *pnd = data;

    if (pnd->reason == PRECOPY_NOTIFY_SETUP || pnd->reason == PRECOPY_NOTIFY_BEFORE_BITMAP_SYNC) {
        fast_invalidate("precopy");
    }
    return 0;
}

/* Every flag dropped: the listeners stopped logging (someone may take a flag again later). */
static void fast_log_global_stop(MemoryListener *listener) {
    fast_invalidate("log-stop");
}

static NotifierWithReturn fast_precopy_notifier = { .notify = fast_precopy_notify };
static MemoryListener fast_listener = {
    .name = "panda-fast",
    .log_global_stop = fast_log_global_stop,
    .priority = MEMORY_LISTENER_PRIORITY_MIN,
};

/* Registered at the first snapshot only: nothing is added for a run that never uses this. */
static void fast_hook(void) {
    if (!fast_hooked) {
        precopy_add_notifier(&fast_precopy_notifier);
        memory_listener_register(&fast_listener, &address_space_memory);
        fast_hooked = true;
    }
}

static bool fast_tracking_ok(void) {
    if (!fast_valid) {
        return false;
    }
    if (!(global_dirty_tracking & GLOBAL_DIRTY_DIRTY_RATE)) {
        fast_invalidate("flag");
        return false;
    }
    if (!fast_sentinels_ok()) {
        fast_invalidate("sentinel");
        return false;
    }
    return true;
}

/* Refuse where the dirty bitmap isn't the whole story, or a restore would break the run. */
static bool fast_supported(const char *fn) {
    if (!tcg_enabled()) {
        error_report("%s: needs TCG (KVM and HVF keep their own dirty state, which a restore "
                     "doesn't reset)", fn);
        return false;
    }
    if (replay_mode != REPLAY_MODE_NONE) {
        error_report("%s: not under record/replay (a restore rewrites RAM and device state "
                     "outside the log)", fn);
        return false;
    }
    return true;
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
    fast_invalidate("armed");
    return 0;
}

int panda_fast_snap(void) {
    uint64_t pages = 0;
    bool unlogged;

    if (!fast_supported("panda_fast_snap")) {
        return -1;
    }
    panda_modules_check_vmstate("fast restore");
    if (runstate_is_running()) {
        vm_stop(RUN_STATE_SAVE_VM);
    }
    fast_hook();
    if (fast_arm() < 0) {
        return -1;
    }
    memory_global_dirty_log_sync(false);
    unlogged = vhost_has_unlogged_dev();

    if (fast_blocks && fast_layout_same() && fast_tracking_ok() && !unlogged) {
        /* the copy differs from RAM only in the pages written since: copy those */
        for (guint i = 0; i < fast_blocks->len; i++) {
            FastBlock *b = &g_array_index(fast_blocks, FastBlock, i);
            if (b->tracked) {
                pages += fast_walk(b, false);
            } else {
                memcpy(b->copy, qemu_ram_get_host_addr(b->rb), b->len);
                pages += DIV_ROUND_UP(b->len, page_size());
            }
        }
        fast_set_sentinels();
        fast_tlb_rearm();
        fast_stats.last_snap_mode = "incremental";
    } else {
        fast_copy_all();
        fast_rearm();
        for (guint i = 0; i < fast_blocks->len; i++) {
            pages += DIV_ROUND_UP(g_array_index(fast_blocks, FastBlock, i).len, page_size());
        }
        fast_stats.last_snap_mode = unlogged ? "compare" : "full";
    }
    fast_stats.last_snap_pages = pages;
    if (fast_save_devices() < 0) {
        /* don't keep this RAM with the previous snapshot's devices */
        panda_fast_delete();
        return -1;
    }
    fast_stats.snaps++;
    return 0;
}

int panda_fast_restore(void) {
    bool full, unlogged;
    int64_t t0, t1, t2;
    uint64_t pages = 0;

    if (!fast_supported("panda_fast_restore")) {
        return -1;
    }
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
    /* collect what other loggers (vhost, KVM) hold before reading the bitmap */
    memory_global_dirty_log_sync(false);
    unlogged = vhost_has_unlogged_dev();
    full = !fast_tracking_ok();
    if (full) {
        fast_stats.last_why = fast_why ? fast_why : "?";
    }
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
        fast_stats.last_mode = "full";
    } else if (unlogged) {
        /*
         * A vhost device that can't log may have written any page it could see: compare them
         * all. The bitmap's pages are among them; clear its bits after.
         */
        for (guint i = 0; i < fast_blocks->len; i++) {
            pages += fast_compare_restore(&g_array_index(fast_blocks, FastBlock, i));
        }
        fast_rearm();  /* also re-arms the TLBs for the pages it clears */
        fast_stats.compare_restores++;
        fast_stats.last_mode = "compare";
    } else {
        for (guint i = 0; i < fast_blocks->len; i++) {
            FastBlock *b = &g_array_index(fast_blocks, FastBlock, i);
            pages += b->tracked ? fast_walk(b, true) : fast_compare_restore(b);
        }
        fast_set_sentinels();
        fast_tlb_rearm();
        fast_stats.last_mode = "dirty";
    }
    t2 = g_get_monotonic_time();
    fast_stats.restores++;
    fast_stats.last_pages = pages;
    fast_stats.last_full = full;

    if (fast_load_devices() < 0) {
        /* RAM is the snapshot's; devices are as the reset left them. */
        error_report("panda_fast_restore: device state failed to load; the VM is reset");
        return -1;
    }
    fast_stats.last_reset_us = t1 - t0;
    fast_stats.last_ram_us = t2 - t1;
    fast_stats.last_dev_us = g_get_monotonic_time() - t2;
#ifdef TARGET_ARM
    CPUState *cs;
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
    if (!fast_layout_same()) {
        /* a block may be gone (unplugged): its RAMBlock is freed */
        error_report("panda_fast_verify: RAM blocks changed since panda_fast_snap");
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
    static char buf[2048];
    size_t n;

    bool same = fast_blocks && fast_layout_same();

    n = snprintf(buf, sizeof(buf),
                 "snaps=%" PRIu64 " restores=%" PRIu64 " full_restores=%" PRIu64
                 " compare_restores=%" PRIu64
                 " last_pages=%" PRIu64 " last_full=%d last_mode=%s last_why=%s"
                 " last_us=%" PRId64 ",%" PRId64 ",%" PRId64
                 " last_snap_pages=%" PRIu64 " last_snap_mode=%s"
                 " tracking=%d unlogged=%d blocks=%s",
                 fast_stats.snaps, fast_stats.restores, fast_stats.full_restores,
                 fast_stats.compare_restores,
                 fast_stats.last_pages, fast_stats.last_full,
                 fast_stats.last_mode ? fast_stats.last_mode : "-",
                 fast_stats.last_why ? fast_stats.last_why : "-",
                 fast_stats.last_reset_us, fast_stats.last_ram_us, fast_stats.last_dev_us,
                 fast_stats.last_snap_pages,
                 fast_stats.last_snap_mode ? fast_stats.last_snap_mode : "-",
                 same && fast_valid && (global_dirty_tracking & GLOBAL_DIRTY_DIRTY_RATE) &&
                 fast_sentinels_ok(),
                 vhost_has_unlogged_dev(), fast_blocks && !same ? "changed" : "");
    /* blocks may be gone (unplugged): describe them only while they are still RAM's */
    for (guint i = 0; same && i < fast_blocks->len && n < sizeof(buf); i++) {
        FastBlock *b = &g_array_index(fast_blocks, FastBlock, i);
        n += snprintf(buf + n, sizeof(buf) - n, "%s%s:%zu:%s", i ? "," : "",
                      qemu_ram_get_idstr(b->rb), b->len, b->tracked ? "dirty" : "compare");
    }
    return buf;
}
