#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "system/replay.h"
#include "system/replay-analysis.h"
#include "system/replay-effect.h"
#include "panda/guest-write.h"
#include "system/panda-embed.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "system/hw_accel.h"
#include "system/runstate.h"
#include "migration/snapshot.h"
#include "qemu/main-loop.h"
#include "qemu/aio.h"
#include "exec/gdbstub.h"

typedef struct PandaMmioRegion {
    MemoryRegion mr;
    MemoryRegionOps ops;
    panda_mmio_read_cb_t read_cb;
    panda_mmio_write_cb_t write_cb;
    void *opaque;
    char *name;
} PandaMmioRegion;

static panda_guest_hypercall_cb_t panda_guest_hypercall_cb;
static void *panda_guest_hypercall_opaque;
static panda_kvm_hypercall_cb_t panda_kvm_hypercall_cb;
static GHashTable *panda_guest_hypercall_numbers;

static void panda_guest_hypercalls_init(void)
{
    if (!panda_guest_hypercall_numbers) {
        panda_guest_hypercall_numbers = g_hash_table_new_full(g_int64_hash,
                                                              g_int64_equal,
                                                              g_free, NULL);
    }
}

void __attribute__((visibility("default")))
panda_register_guest_hypercall(uint64_t nr)
{
    uint64_t *key;

    panda_guest_hypercalls_init();
    if (g_hash_table_contains(panda_guest_hypercall_numbers, &nr)) {
        return;
    }

    key = g_new(uint64_t, 1);
    *key = nr;
    g_hash_table_add(panda_guest_hypercall_numbers, key);
}

void __attribute__((visibility("default")))
panda_unregister_guest_hypercall(uint64_t nr)
{
    if (panda_guest_hypercall_numbers) {
        g_hash_table_remove(panda_guest_hypercall_numbers, &nr);
    }
}

void __attribute__((visibility("default")))
panda_clear_guest_hypercalls(void)
{
    if (panda_guest_hypercall_numbers) {
        g_hash_table_remove_all(panda_guest_hypercall_numbers);
    }
}

bool __attribute__((visibility("default")))
panda_guest_hypercall_registered(uint64_t nr)
{
    return panda_guest_hypercall_numbers &&
           g_hash_table_contains(panda_guest_hypercall_numbers, &nr);
}

/* Allowed during record/replay: its answers are logged (replay-effect.h). */
void __attribute__((visibility("default")))
panda_set_guest_hypercall_callback(panda_guest_hypercall_cb_t cb,
                                   void *opaque)
{
    panda_guest_hypercall_cb = cb;
    panda_guest_hypercall_opaque = opaque;
}

void __attribute__((visibility("default")))
panda_set_kvm_hypercall_callback(panda_kvm_hypercall_cb_t cb)
{
    if (cb && replay_mode != REPLAY_MODE_NONE) {
        error_report("KVM hypercall consumer refused during record/replay");
        return;
    }
    panda_kvm_hypercall_cb = cb;
}

bool panda_hypercall_replay_safe(Error **errp)
{
    if (panda_kvm_hypercall_cb) {
        error_setg(errp, "the KVM hypercall consumer cannot be recorded");
        return false;
    }
    return true;
}

bool panda_guest_hypercall_wanted(uint64_t nr)
{
    return (panda_guest_hypercall_cb &&
            panda_guest_hypercall_registered(nr)) ||
           panda_kvm_hypercall_cb;
}

static bool panda_answer_guest_hypercall(CPUState *cs, uint64_t nr,
                                         uint64_t a0, uint64_t a1,
                                         uint64_t a2, uint64_t a3,
                                         uint64_t a4, uint64_t a5,
                                         uint64_t *ret)
{
    if (panda_guest_hypercall_cb &&
        panda_guest_hypercall_registered(nr)) {
        return panda_guest_hypercall_cb(cs, nr, a0, a1, a2, a3, a4, a5,
                                        ret,
                                        panda_guest_hypercall_opaque) == 0;
    }

    if (panda_kvm_hypercall_cb) {
        return panda_kvm_hypercall_cb(cs, nr, a0, a1, a2, a3, a4, a5,
                                      ret) == 0;
    }

    return false;
}

/*
 * Every call is one hypercall event while recording, answered or not, so
 * replay reads the answer without knowing which consumers the recording had.
 */
bool panda_handle_guest_hypercall(CPUState *cs, uint64_t nr,
                                  uint64_t a0, uint64_t a1,
                                  uint64_t a2, uint64_t a3,
                                  uint64_t a4, uint64_t a5,
                                  uint64_t *ret)
{
    bool handled;

    if (replay_mode == REPLAY_MODE_PLAY) {
        return replay_guest_write_replay(cs, REPLAY_GUEST_SITE_HYPERCALL, ret);
    }
    if (!replay_guest_write_begin(cs, REPLAY_GUEST_SITE_HYPERCALL)) {
        return panda_answer_guest_hypercall(cs, nr, a0, a1, a2, a3, a4, a5,
                                            ret);
    }
    *ret = 0;
    handled = panda_answer_guest_hypercall(cs, nr, a0, a1, a2, a3, a4, a5,
                                           ret);
    replay_guest_write_end(cs, handled, handled ? *ret : 0);
    return handled;
}

static uint64_t panda_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    PandaMmioRegion *region = opaque;

    if (!region->read_cb) {
        return 0;
    }

    return region->read_cb(addr, size, region->opaque);
}

static void panda_mmio_write(void *opaque, hwaddr addr, uint64_t data,
                             unsigned size)
{
    PandaMmioRegion *region = opaque;

    if (!region->write_cb) {
        return;
    }

    region->write_cb(addr, data, size, region->opaque);
}

int __attribute__((visibility("default")))
panda_add_mmio_region(uint64_t base, uint64_t size,
                      const char *name,
                      panda_mmio_read_cb_t read_cb,
                      panda_mmio_write_cb_t write_cb,
                      void *opaque)
{
    PandaMmioRegion *region;

    if (!replay_analysis_write_allowed() ||
        !size || !name || (!read_cb && !write_cb)) {
        return -1;
    }

    region = g_new0(PandaMmioRegion, 1);
    region->read_cb = read_cb;
    region->write_cb = write_cb;
    region->opaque = opaque;
    region->name = g_strdup(name);
    region->ops.read = panda_mmio_read;
    region->ops.write = panda_mmio_write;
    region->ops.endianness = DEVICE_NATIVE_ENDIAN;
    region->ops.valid.min_access_size = 1;
    region->ops.valid.max_access_size = 8;
    region->ops.impl.min_access_size = 1;
    region->ops.impl.max_access_size = 8;

    memory_region_init_io(&region->mr, NULL, &region->ops, region,
                          region->name, size);
    memory_region_add_subregion_overlap(get_system_memory(), base,
                                        &region->mr, -1000);
    return 0;
}

int __attribute__((visibility("default")))
panda_read_guest_reg(CPUState *cs, int regnum, uint8_t *buf, int buf_len)
{
    GByteArray *bytes;
    int len;

    if (!cs || !buf || buf_len <= 0) {
        return -1;
    }

    cpu_synchronize_state(cs);
    bytes = g_byte_array_new();
    len = gdb_read_register(cs, bytes, regnum);
    if (len <= 0 || len > buf_len) {
        g_byte_array_free(bytes, true);
        return -1;
    }
    memcpy(buf, bytes->data, len);
    g_byte_array_free(bytes, true);
    return len;
}

int __attribute__((visibility("default")))
panda_write_guest_reg(CPUState *cs, int regnum, const uint8_t *buf, int len)
{
    bool ok;

    if (!cs || !buf || len <= 0 ||
        (!replay_analysis_write_allowed() &&
         replay_guest_write_admitted(NULL, len) != GUEST_WRITE_OK)) {
        return -1;
    }

    cpu_synchronize_state(cs);
    ok = gdb_write_register(cs, (uint8_t *)buf, regnum) > 0;
    replay_guest_write_log_reg(cs, regnum, buf, len,
                               ok ? GUEST_WRITE_OK : GUEST_WRITE_EACCES);
    return ok ? 0 : -1;
}

void __attribute__((visibility("default")))
panda_sync_cpu_state(CPUState *cs)
{
    /*
     * Pull register state out of the accelerator (KVM) into env and mark
     * the vCPU dirty so direct env writes are pushed back on next entry.
     * No-op under TCG.
     */
    if (cs) {
        cpu_synchronize_state(cs);
    }
}

/*
 * Snapshot wrappers. These mirror hmp_savevm()/hmp_loadvm() but expose a
 * minimal C ABI (bool return, no Monitor/QDict/strList) so an embedding host
 * can drive savevm/loadvm directly. They must be called with the BQL held
 * and from the main loop context (panda_schedule_snapshot puts them there). save_snapshot() stops and restarts the VM internally; loadvm needs
 * the VM stopped first and the runstate restored afterwards, as HMP does.
 */
bool __attribute__((visibility("default")))
panda_save_snapshot(const char *name)
{
    Error *err = NULL;
    bool ok;

    /*
     * These are stopped-VM snapshots, so ignore live-migration-only
     * blockers (notably the vhost-user vsock backend's lack of LOG_SHMFD
     * dirty-page logging). Genuinely unmigratable devices are still refused.
     */
    migration_snapshot_set_ignore_blockers(true);
    ok = save_snapshot(name, true, NULL, false, NULL, &err);
    migration_snapshot_set_ignore_blockers(false);
    if (!ok) {
        if (err) {
            error_reportf_err(err, "panda_save_snapshot: ");
        }
        return false;
    }
    return true;
}

bool __attribute__((visibility("default")))
panda_load_snapshot(const char *name)
{
    RunState saved_state = runstate_get();
    Error *err = NULL;

    /* refused (while recording or replaying) before the VM stops: it keeps running */
    if (!replay_can_load_snapshot(&err)) {
        error_reportf_err(err, "panda_load_snapshot: ");
        return false;
    }
    vm_stop(RUN_STATE_RESTORE_VM);

    if (!load_snapshot(name, NULL, false, NULL, &err)) {
        if (err) {
            error_reportf_err(err, "panda_load_snapshot: ");
        }
        return false;
    }

    load_snapshot_resume(saved_state);
    return true;
}

typedef struct PandaSnapshotReq {
    char *name;
    bool load;
} PandaSnapshotReq;

static void panda_snapshot_bh(void *opaque)
{
    PandaSnapshotReq *req = opaque;

    if (req->load) {
        panda_load_snapshot(req->name);
    } else {
        panda_save_snapshot(req->name);
    }
    g_free(req->name);
    g_free(req);
}

/*
 * Schedule a savevm (load=false) or loadvm (load=true) to run on the main
 * loop. Safe to call from a vCPU thread (e.g. a guest hypercall handler):
 * the snapshot itself runs in the main loop context where it can stop the
 * vCPUs and pump the loop without deadlocking. Fire-and-forget; success or
 * failure is reported to QEMU stderr by the synchronous wrappers above.
 */
void __attribute__((visibility("default")))
panda_schedule_snapshot(const char *name, bool load)
{
    PandaSnapshotReq *req = g_new0(PandaSnapshotReq, 1);

    req->name = g_strdup(name);
    req->load = load;
    aio_bh_schedule_oneshot(qemu_get_aio_context(), panda_snapshot_bh, req);
}

static panda_reset_request_cb_t panda_reset_request_cb;
static void *panda_reset_request_opaque;

void __attribute__((visibility("default")))
panda_set_reset_request_callback(panda_reset_request_cb_t cb, void *opaque)
{
    panda_reset_request_cb = cb;
    panda_reset_request_opaque = opaque;
}

void panda_invoke_reset_request_callback(ShutdownCause reason)
{
    if (panda_reset_request_cb) {
        panda_reset_request_cb((int)reason, panda_reset_request_opaque);
    }
}

static panda_qmp_cb_t panda_qmp_cb;
static void *panda_qmp_opaque;

void __attribute__((visibility("default")))
panda_set_qmp_callback(panda_qmp_cb_t cb, void *opaque)
{
    panda_qmp_cb = cb;
    panda_qmp_opaque = opaque;
}

bool __attribute__((visibility("default")))
panda_handle_qmp(const char *command, const char *args, char **result)
{
    if (panda_qmp_cb) {
        return panda_qmp_cb(command, args, result, panda_qmp_opaque);
    }
    return false;
}
