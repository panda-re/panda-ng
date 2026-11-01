/*
 * The guest-write API (include/panda/guest-write.h) for this target: admission
 * and logging are system/replay-effect.h's; this file has what depends on the
 * target, which is CPUArchState's layout map (guest-layout.gen.h, generated)
 * and the return register.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "panda/common.h"
#include "panda/plugin.h"
#include "panda/guest-write.h"
#include "exec/gdbstub.h"
#include "exec/cputlb.h"
#include "system/hw_accel.h"
#include "system/replay-effect.h"
#include "qemu/timer.h"     /* QEMUTimer: the map takes sizeof every type it lists */

#include "guest-layout.gen.h"

static void guest_layout_sync(CPUState *cs)
{
#if defined(TARGET_ARM)
    arm_rebuild_hflags(cpu_env(cs));
#endif
}

static GuestLayout guest_layout = {
    .types = guest_layout_types,
    .ntypes = ARRAY_SIZE(guest_layout_types),
    .fields = guest_layout_fields,
    .nfields = ARRAY_SIZE(guest_layout_fields),
    .length = guest_layout_length,
    .sync = guest_layout_sync,
};

/* FNV-1a over 32-bit words */
static uint32_t guest_layout_mix(uint32_t hash, uint32_t word)
{
    for (int i = 0; i < 4; i++) {
        hash = (hash ^ ((word >> (8 * i)) & 0xff)) * 16777619u;
    }
    return hash;
}

/*
 * The map as this build compiled it: offsets and sizes depend on the host, so
 * a recording replays only where the hash, and so the layout, is the same.
 */
static uint32_t guest_layout_hash(void)
{
    uint32_t hash = 2166136261u;

    for (size_t i = 0; i < ARRAY_SIZE(guest_layout_types); i++) {
        const GuestLayoutType *t = &guest_layout_types[i];

        hash = guest_layout_mix(hash, t->size);
        for (uint32_t j = 0; j < t->nhost; j++) {
            hash = guest_layout_mix(hash, t->host[j].offset);
            hash = guest_layout_mix(hash, t->host[j].size);
        }
        for (uint32_t j = 0; j < t->nfollow; j++) {
            hash = guest_layout_mix(hash, t->follow[j].offset);
            hash = guest_layout_mix(hash, t->follow[j].type);
            hash = guest_layout_mix(hash, t->follow[j].elem_size);
            hash = guest_layout_mix(hash, t->follow[j].length);
        }
    }
    for (size_t i = 0; i < ARRAY_SIZE(guest_layout_fields); i++) {
        hash = guest_layout_mix(hash, guest_layout_fields[i].offset);
        hash = guest_layout_mix(hash, guest_layout_fields[i].size);
    }
    return hash;
}

static void __attribute__((constructor)) guest_layout_init(void)
{
    guest_layout.hash = guest_layout_hash();
    replay_guest_layout_register(&guest_layout);
}

/* Without record/replay, PANDA's policy; during it, the logging scope */
static int guest_admit(CPUState *cpu, size_t len)
{
    return panda_guest_mutation_allowed() ? GUEST_WRITE_OK
                                          : replay_guest_write_admitted(cpu, len);
}

GuestWriteResult panda_guest_write_virt(CPUState *cpu, uint64_t addr,
                                        const void *buf, size_t len)
{
    int admitted = guest_admit(NULL, len);

    if (!cpu || !buf || !len || len > INT_MAX) {
        return GUEST_WRITE_EINVAL;
    }
    if (admitted != GUEST_WRITE_OK) {
        return admitted;
    }
    return panda_virtual_memory_rw(cpu, addr, (uint8_t *)buf, len, true) ?
           GUEST_WRITE_EFAULT : GUEST_WRITE_OK;
}

GuestWriteResult panda_guest_write_phys(uint64_t addr, const void *buf, size_t len)
{
    int admitted = guest_admit(NULL, len);

    if (!buf || !len || len > INT_MAX) {
        return GUEST_WRITE_EINVAL;
    }
    if (admitted != GUEST_WRITE_OK) {
        return admitted;
    }
    return panda_physical_memory_rw(addr, (uint8_t *)buf, len, true) == MEMTX_OK ?
           GUEST_WRITE_OK : GUEST_WRITE_EFAULT;
}

/* The register's gdbstub size, or -1 if @cpu has no such register */
static int guest_reg_size(CPUState *cpu, int regnum)
{
    g_autoptr(GByteArray) value = g_byte_array_new();

    if (regnum < 0 || regnum >= cpu->gdb_num_regs) {
        return -1;
    }
    return gdb_read_register(cpu, value, regnum) > 0 ? (int)value->len : -1;
}

GuestWriteResult panda_guest_write_reg(CPUState *cpu, int regnum,
                                       const void *buf, size_t len)
{
    int admitted, result;

    if (!cpu || !buf) {
        return GUEST_WRITE_EINVAL;
    }
    cpu_synchronize_state(cpu);
    if (guest_reg_size(cpu, regnum) != (int)len) {
        return GUEST_WRITE_EINVAL;
    }
    admitted = guest_admit(NULL, len);
    if (admitted != GUEST_WRITE_OK) {
        return admitted;
    }
    result = gdb_write_register(cpu, (uint8_t *)buf, regnum) > 0 ?
             GUEST_WRITE_OK : GUEST_WRITE_EACCES;
    replay_guest_write_log_reg(cpu, regnum, buf, len, result);
    return result;
}

GuestWriteResult panda_guest_write_env_path(CPUState *cpu, const uint32_t *derefs,
                                            size_t n, size_t offset,
                                            const void *buf, size_t len)
{
    int admitted;

    if (!cpu || !buf || n > GUEST_WRITE_PATH_MAX || (n && !derefs) ||
        offset > UINT32_MAX || len > UINT32_MAX) {
        return GUEST_WRITE_EINVAL;
    }
    admitted = guest_admit(cpu, len);
    if (admitted != GUEST_WRITE_OK) {
        return admitted;
    }
    return replay_guest_write_env(cpu, derefs, n, offset, buf, len);
}

GuestWriteResult panda_guest_write_env(CPUState *cpu, size_t offset,
                                       const void *buf, size_t len)
{
    return panda_guest_write_env_path(cpu, NULL, 0, offset, buf, len);
}

GuestWriteResult panda_guest_env_sync(CPUState *cpu)
{
    int admitted;

    if (!cpu) {
        return GUEST_WRITE_EINVAL;
    }
    admitted = guest_admit(cpu, 0);
    return admitted == GUEST_WRITE_OK ? replay_guest_write_sync(cpu) : admitted;
}

/* Where panda_set_retval's register is in env, and its width */
static size_t guest_retval_field(CPUState *cpu, size_t *len)
{
    CPUArchState *env = cpu_env(cpu);
    void *field;
#if defined(TARGET_I386)
    field = &env->regs[R_EAX];
    *len = sizeof(env->regs[R_EAX]);
#elif defined(TARGET_ARM)
#if defined(TARGET_AARCH64)
    /* in AArch64 state regs[] is only the AArch32 view */
    if (is_a64(env)) {
        field = &env->xregs[0];
        *len = sizeof(env->xregs[0]);
        return (uint8_t *)field - (uint8_t *)env;
    }
#endif
    field = &env->regs[0];
    *len = sizeof(env->regs[0]);
#elif defined(TARGET_PPC)
    field = &env->gpr[3];
    *len = sizeof(env->gpr[3]);
#elif defined(TARGET_MIPS)
    field = &env->active_tc.gpr[MIPS_V0];
    *len = sizeof(env->active_tc.gpr[MIPS_V0]);
#elif defined(TARGET_LOONGARCH)
    field = &env->gpr[4];
    *len = sizeof(env->gpr[4]);
#elif defined(TARGET_RISCV)
    field = &env->gpr[10];
    *len = sizeof(env->gpr[10]);
#else
#error "guest_retval_field() not implemented for target architecture."
#endif
    return (uint8_t *)field - (uint8_t *)env;
}

GuestWriteResult panda_guest_set_retval(CPUState *cpu, uint64_t value)
{
    size_t len, offset;
    uint8_t bytes[8];

    if (!cpu) {
        return GUEST_WRITE_EINVAL;
    }
    offset = guest_retval_field(cpu, &len);
    /* the low len bytes of value, in the host's order, as an assignment stores them */
#if HOST_BIG_ENDIAN
    stq_be_p(bytes, value);
    return panda_guest_write_env(cpu, offset, bytes + sizeof(bytes) - len, len);
#else
    stq_le_p(bytes, value);
    return panda_guest_write_env(cpu, offset, bytes, len);
#endif
}

int panda_guest_reg_lookup(CPUState *cpu, const char *name, size_t *len)
{
    g_autoptr(GArray) regs = NULL;
    int found = -1;

    if (!cpu || !name) {
        return -1;
    }
    regs = gdb_get_register_list(cpu);
    for (guint i = 0; i < regs->len && found < 0; i++) {
        GDBRegDesc *desc = &g_array_index(regs, GDBRegDesc, i);
        if (desc->name && !strcmp(desc->name, name)) {
            found = desc->gdb_reg;
        }
    }
    if (found >= 0 && len) {
        int size = guest_reg_size(cpu, found);
        *len = size > 0 ? size : 0;
    }
    return found;
}

const char *panda_guest_write_strerror(GuestWriteResult result)
{
    static const char *const names[] = {
        [GUEST_WRITE_OK] = "ok",
        [GUEST_WRITE_EPERM] = "not admitted: no mutation scope (observer, replay, "
                              "or a site that does not log writes)",
        [GUEST_WRITE_EINVAL] = "invalid vCPU, register, length or path",
        [GUEST_WRITE_EFAULT] = "address does not translate or is not RAM",
        [GUEST_WRITE_EACCES] = "the target refused the register write",
        [GUEST_WRITE_E2BIG] = "past the per-site write budget",
    };
    return (unsigned)result < ARRAY_SIZE(names) ? names[result] : "unknown";
}
