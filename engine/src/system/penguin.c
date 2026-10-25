#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "system/replay.h"
#include "system/penguin.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "system/hw_accel.h"
#include "system/runstate.h"
#include "migration/snapshot.h"
#include "qemu/main-loop.h"
#include "qemu/aio.h"
#include "exec/gdbstub.h"

typedef struct PenguinMmioRegion {
    MemoryRegion mr;
    MemoryRegionOps ops;
    penguin_mmio_read_cb_t read_cb;
    penguin_mmio_write_cb_t write_cb;
    void *opaque;
    char *name;
} PenguinMmioRegion;

static penguin_guest_hypercall_cb_t penguin_guest_hypercall_cb;
static void *penguin_guest_hypercall_opaque;
static kvm_penguin_hypercall_cb_t kvm_penguin_hypercall_cb;
static GHashTable *penguin_guest_hypercall_numbers;

static void penguin_guest_hypercalls_init(void)
{
    if (!penguin_guest_hypercall_numbers) {
        penguin_guest_hypercall_numbers = g_hash_table_new_full(g_int64_hash,
                                                                g_int64_equal,
                                                                g_free, NULL);
    }
}

void __attribute__((visibility("default")))
penguin_register_guest_hypercall(uint64_t nr)
{
    uint64_t *key;

    penguin_guest_hypercalls_init();
    if (g_hash_table_contains(penguin_guest_hypercall_numbers, &nr)) {
        return;
    }

    key = g_new(uint64_t, 1);
    *key = nr;
    g_hash_table_add(penguin_guest_hypercall_numbers, key);
}

void __attribute__((visibility("default")))
penguin_unregister_guest_hypercall(uint64_t nr)
{
    if (penguin_guest_hypercall_numbers) {
        g_hash_table_remove(penguin_guest_hypercall_numbers, &nr);
    }
}

void __attribute__((visibility("default")))
penguin_clear_guest_hypercalls(void)
{
    if (penguin_guest_hypercall_numbers) {
        g_hash_table_remove_all(penguin_guest_hypercall_numbers);
    }
}

bool __attribute__((visibility("default")))
penguin_guest_hypercall_registered(uint64_t nr)
{
    return penguin_guest_hypercall_numbers &&
           g_hash_table_contains(penguin_guest_hypercall_numbers, &nr);
}

void __attribute__((visibility("default")))
set_penguin_guest_hypercall_callback(penguin_guest_hypercall_cb_t cb,
                                     void *opaque)
{
    penguin_guest_hypercall_cb = cb;
    penguin_guest_hypercall_opaque = opaque;
}

void __attribute__((visibility("default")))
set_kvm_penguin_hypercall_callback(kvm_penguin_hypercall_cb_t cb)
{
    kvm_penguin_hypercall_cb = cb;
}

bool penguin_guest_hypercall_wanted(uint64_t nr)
{
    return (penguin_guest_hypercall_cb &&
            penguin_guest_hypercall_registered(nr)) ||
           kvm_penguin_hypercall_cb;
}

bool penguin_handle_guest_hypercall(CPUState *cs, uint64_t nr,
                                    uint64_t a0, uint64_t a1,
                                    uint64_t a2, uint64_t a3,
                                    uint64_t a4, uint64_t a5,
                                    uint64_t *ret)
{
    if (penguin_guest_hypercall_cb &&
        penguin_guest_hypercall_registered(nr)) {
        return penguin_guest_hypercall_cb(cs, nr, a0, a1, a2, a3, a4, a5,
                                          ret,
                                          penguin_guest_hypercall_opaque) == 0;
    }

    if (kvm_penguin_hypercall_cb) {
        return kvm_penguin_hypercall_cb(cs, nr, a0, a1, a2, a3, a4, a5,
                                        ret) == 0;
    }

    return false;
}

static uint64_t penguin_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    PenguinMmioRegion *region = opaque;

    if (!region->read_cb) {
        return 0;
    }

    return region->read_cb(addr, size, region->opaque);
}

static void penguin_mmio_write(void *opaque, hwaddr addr, uint64_t data,
                               unsigned size)
{
    PenguinMmioRegion *region = opaque;

    if (!region->write_cb) {
        return;
    }

    region->write_cb(addr, data, size, region->opaque);
}

int __attribute__((visibility("default")))
penguin_qemu_add_mmio_region(uint64_t base, uint64_t size,
                             const char *name,
                             penguin_mmio_read_cb_t read_cb,
                             penguin_mmio_write_cb_t write_cb,
                             void *opaque)
{
    PenguinMmioRegion *region;

    if (!size || !name || (!read_cb && !write_cb)) {
        return -1;
    }

    region = g_new0(PenguinMmioRegion, 1);
    region->read_cb = read_cb;
    region->write_cb = write_cb;
    region->opaque = opaque;
    region->name = g_strdup(name);
    region->ops.read = penguin_mmio_read;
    region->ops.write = penguin_mmio_write;
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
penguin_read_guest_reg(CPUState *cs, int regnum, uint8_t *buf, int buf_len)
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
penguin_write_guest_reg(CPUState *cs, int regnum, const uint8_t *buf, int len)
{
    if (!cs || !buf || len <= 0) {
        return -1;
    }

    cpu_synchronize_state(cs);
    if (gdb_write_register(cs, (uint8_t *)buf, regnum) <= 0) {
        return -1;
    }
    return 0;
}

void __attribute__((visibility("default")))
*penguin_cpu_env(CPUState *cs)
{
    /*
     * CPUArchState immediately follows CPUState in ArchCPU; cpu-target.c
     * validates this layout for every target. Mirrors cpu_env() without
     * needing target-specific types in common code.
     */
    if (!cs) {
        return NULL;
    }
    return (void *)(cs + 1);
}

void __attribute__((visibility("default")))
penguin_sync_cpu_state(CPUState *cs)
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
 * minimal C ABI (bool return, no Monitor/QDict/strList) so Penguin's CFFI
 * layer can drive savevm/loadvm directly. They must be called with the BQL
 * held and from the main loop context (Penguin schedules them onto the main
 * loop). save_snapshot() stops and restarts the VM internally; loadvm needs
 * the VM stopped first and the runstate restored afterwards, as HMP does.
 */
bool __attribute__((visibility("default")))
penguin_save_snapshot(const char *name)
{
    Error *err = NULL;
    bool ok;

    /*
     * Penguin only takes stopped-VM snapshots, so ignore live-migration-only
     * blockers (notably the vhost-user vsock backend's lack of LOG_SHMFD
     * dirty-page logging). Genuinely unmigratable devices are still refused.
     */
    migration_snapshot_set_ignore_blockers(true);
    ok = save_snapshot(name, true, NULL, false, NULL, &err);
    migration_snapshot_set_ignore_blockers(false);
    if (!ok) {
        if (err) {
            error_reportf_err(err, "penguin_save_snapshot: ");
        }
        return false;
    }
    return true;
}

bool __attribute__((visibility("default")))
penguin_load_snapshot(const char *name)
{
    RunState saved_state = runstate_get();
    Error *err = NULL;

    /* refused (while recording or replaying) before the VM stops: it keeps running */
    if (!replay_can_load_snapshot(&err)) {
        error_reportf_err(err, "penguin_load_snapshot: ");
        return false;
    }
    vm_stop(RUN_STATE_RESTORE_VM);

    if (!load_snapshot(name, NULL, false, NULL, &err)) {
        if (err) {
            error_reportf_err(err, "penguin_load_snapshot: ");
        }
        return false;
    }

    load_snapshot_resume(saved_state);
    return true;
}

typedef struct PenguinSnapshotReq {
    char *name;
    bool load;
} PenguinSnapshotReq;

static void penguin_snapshot_bh(void *opaque)
{
    PenguinSnapshotReq *req = opaque;

    if (req->load) {
        penguin_load_snapshot(req->name);
    } else {
        penguin_save_snapshot(req->name);
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
penguin_schedule_snapshot(const char *name, bool load)
{
    PenguinSnapshotReq *req = g_new0(PenguinSnapshotReq, 1);

    req->name = g_strdup(name);
    req->load = load;
    aio_bh_schedule_oneshot(qemu_get_aio_context(), penguin_snapshot_bh, req);
}

static penguin_reset_request_cb_t penguin_reset_request_cb;
static void *penguin_reset_request_opaque;

void __attribute__((visibility("default")))
set_penguin_reset_request_callback(penguin_reset_request_cb_t cb, void *opaque)
{
    penguin_reset_request_cb = cb;
    penguin_reset_request_opaque = opaque;
}

void penguin_invoke_reset_request_callback(ShutdownCause reason)
{
    if (penguin_reset_request_cb) {
        penguin_reset_request_cb((int)reason, penguin_reset_request_opaque);
    }
}

static penguin_qmp_cb_t penguin_qmp_cb;
static void *penguin_qmp_opaque;

void __attribute__((visibility("default")))
set_penguin_qmp_callback(penguin_qmp_cb_t cb, void *opaque)
{
    penguin_qmp_cb = cb;
    penguin_qmp_opaque = opaque;
}

bool __attribute__((visibility("default")))
penguin_handle_qmp(const char *command, const char *args, char **result)
{
    if (penguin_qmp_cb) {
        return penguin_qmp_cb(command, args, result, penguin_qmp_opaque);
    }
    return false;
}
