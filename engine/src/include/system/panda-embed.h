/*
 * The embedding API: hooks for a host program that drives the engine as a
 * library (through CFFI, say).
 *
 * These hooks are intentionally small C ABI surfaces, usable without
 * PANDA's internal headers: guest hypercalls, register access, snapshots,
 * reset requests, QMP commands and MMIO regions.
 */
#ifndef QEMU_SYSTEM_PANDA_EMBED_H
#define QEMU_SYSTEM_PANDA_EMBED_H

#include "qemu/typedefs.h"

typedef int (*panda_guest_hypercall_cb_t)(CPUState *cs, uint64_t nr,
                                          uint64_t a0, uint64_t a1,
                                          uint64_t a2, uint64_t a3,
                                          uint64_t a4, uint64_t a5,
                                          uint64_t *ret, void *opaque);

typedef int (*panda_kvm_hypercall_cb_t)(CPUState *cs, uint64_t nr,
                                        uint64_t a0, uint64_t a1,
                                        uint64_t a2, uint64_t a3,
                                        uint64_t a4, uint64_t a5,
                                        uint64_t *ret);

typedef uint64_t (*panda_mmio_read_cb_t)(uint64_t addr, unsigned size,
                                         void *opaque);
typedef void (*panda_mmio_write_cb_t)(uint64_t addr, uint64_t data,
                                      unsigned size, void *opaque);

void panda_set_guest_hypercall_callback(panda_guest_hypercall_cb_t cb,
                                        void *opaque);
void panda_register_guest_hypercall(uint64_t nr);
void panda_unregister_guest_hypercall(uint64_t nr);
void panda_clear_guest_hypercalls(void);
bool panda_guest_hypercall_registered(uint64_t nr);
void panda_set_kvm_hypercall_callback(panda_kvm_hypercall_cb_t cb);
/* The KVM consumer can replace a return value, and has no observer
 * declaration. Replay refuses it; PANDA observers use its checked registry. */
bool panda_hypercall_replay_safe(Error **errp);
/*
 * Whether panda_handle_guest_hypercall() would hand @nr to a callback.
 * Lets a hardware accelerator answer unwanted numbers without pulling the
 * vCPU state in first (arm64 KVM: ~330 ONE_REG ioctls each way).
 */
bool panda_guest_hypercall_wanted(uint64_t nr);

bool panda_handle_guest_hypercall(CPUState *cs, uint64_t nr,
                                  uint64_t a0, uint64_t a1,
                                  uint64_t a2, uint64_t a3,
                                  uint64_t a4, uint64_t a5,
                                  uint64_t *ret);

int panda_add_mmio_region(uint64_t base, uint64_t size,
                          const char *name,
                          panda_mmio_read_cb_t read_cb,
                          panda_mmio_write_cb_t write_cb,
                          void *opaque);

/*
 * Guest register access by GDB core-feature register number. Reads append
 * the register bytes (target byte order) into @buf and return the register
 * width; writes consume exactly the register width from @buf. Both return
 * a negative value on failure.
 */
int panda_read_guest_reg(CPUState *cs, int regnum, uint8_t *buf,
                         int buf_len);
int panda_write_guest_reg(CPUState *cs, int regnum, const uint8_t *buf,
                          int len);

/*
 * Direct CPUArchState access is panda_cpu_env (panda/panda_api.h).
 * panda_sync_cpu_state must be called before env reads (and to make env
 * writes stick) under hardware accelerators; it is a no-op under TCG. Env
 * writes are not logged and break record/replay: write registers with
 * panda_write_guest_reg or the guest-write API (panda/guest-write.h).
 */
void panda_sync_cpu_state(CPUState *cs);

/*
 * Save/load an internal VM snapshot by name. Minimal C ABI wrappers around
 * save_snapshot()/load_snapshot() for an embedding host. Must be called
 * with the BQL held from the main loop context. Return true on success.
 */
bool panda_save_snapshot(const char *name);
bool panda_load_snapshot(const char *name);

/*
 * Schedule a save (load=false) or load (load=true) snapshot to run on the
 * main loop. Safe to call from a vCPU thread; fire-and-forget.
 */
void panda_schedule_snapshot(const char *name, bool load);

/*
 * Reset request callback. Called synchronously from qemu_system_reset_request()
 * before the reset executes, when a guest-initiated reset is detected. Lets
 * the host tear down and reinitialize plugins before QEMU resets the VM.
 */
typedef void (*panda_reset_request_cb_t)(int reason, void *opaque);
void panda_set_reset_request_callback(panda_reset_request_cb_t cb,
                                         void *opaque);

/*
 * QMP command callback. Called from qmp_dispatch() when an incoming QMP
 * command is not a built-in QEMU command, letting the host service custom
 * QMP commands. @command is the command name, @args is the JSON-encoded
 * arguments object, and on success the handler sets *@result to a malloc'd
 * JSON string (ownership transferred to the caller) or leaves it NULL for an
 * empty response. Returns true if the host handled the command.
 */
typedef bool (*panda_qmp_cb_t)(const char *command, const char *args,
                               char **result, void *opaque);
void panda_set_qmp_callback(panda_qmp_cb_t cb, void *opaque);

/*
 * Dispatch a QMP command to the host's QMP callback if one is registered.
 * Defined in system/panda-embed.c and referenced as a weak symbol from
 * qapi/qmp-dispatch.c (which is also linked into tools like qemu-nbd where
 * panda-embed.c is absent). Returns true if the command was handled.
 */
bool panda_handle_qmp(const char *command, const char *args, char **result);

#endif /* QEMU_SYSTEM_PANDA_EMBED_H */
