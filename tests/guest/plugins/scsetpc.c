/*
 * scsetpc: qemu_plugin_set_pc() from a syscall event's entry callback must
 * resume the task there, not deliver the trap again.
 *
 * On the first n= (default 50) getppid entries the callback skips the call
 * with qemu_plugin_set_pc(sc->pc), just past the trap.  An inline per-vCPU
 * instruction count tells the next entry on that vCPU apart: a re-delivered
 * trap enters again with no instruction executed since the set_pc.
 * Guest: sc-bench (getppid in a loop).  At exit it logs
 *   SETPC done=D clean=C redelivered=R
 * and passes when D > 0, R == 0 and C >= D - 1 (the last may be pending).
 */
#include <glib.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

typedef struct {
    bool armed;                 /* set_pc done, the next entry not seen */
    uint64_t at;                /* the instruction count at the set_pc */
} Vcpu;

static struct qemu_plugin_scoreboard *insns, *vcpus;
static int64_t nr[QEMU_PLUGIN_SYSCALL_ABI_COUNT];
static unsigned limit = 50;
static unsigned done, clean, redelivered;

static void tb_trans(struct qemu_plugin_tb *tb, void *ud)
{
    for (size_t i = 0; i < qemu_plugin_tb_n_insns(tb); i++) {
        qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(
            qemu_plugin_tb_get_insn(tb, i), QEMU_PLUGIN_INLINE_ADD_U64,
            qemu_plugin_scoreboard_u64(insns), 1);
    }
}

static void enter(qemu_plugin_id_t id, unsigned int vcpu,
                  struct qemu_plugin_syscall *sc, void *ud)
{
    Vcpu *v = qemu_plugin_scoreboard_find(vcpus, vcpu);
    uint64_t n = *(uint64_t *)qemu_plugin_scoreboard_find(insns, vcpu);

    if (v->armed) {
        v->armed = false;
        if (n == v->at) {
            __atomic_add_fetch(&redelivered, 1, __ATOMIC_RELAXED);
        } else {
            __atomic_add_fetch(&clean, 1, __ATOMIC_RELAXED);
        }
    }
    sc->want_return = false;
    if (sc->abi >= QEMU_PLUGIN_SYSCALL_ABI_COUNT || nr[sc->abi] != sc->num ||
        __atomic_fetch_add(&done, 1, __ATOMIC_RELAXED) >= limit) {
        return;
    }
    v->armed = true;
    v->at = n;
    qemu_plugin_set_pc(sc->pc);
}

static void at_exit(void *ud)
{
    unsigned d = MIN(done, limit);
    g_autofree char *s = g_strdup_printf(
        "SETPC done=%u clean=%u redelivered=%u %s\n", d, clean, redelivered,
        d && !redelivered && clean + 1 >= d ? "PASS" : "FAIL");

    qemu_plugin_outs(s);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    for (int i = 0; i < QEMU_PLUGIN_SYSCALL_ABI_COUNT; i++) {
        nr[i] = -1;
    }
    nr[QEMU_PLUGIN_SYSCALL_ABI_X86_64] = 110;
    nr[QEMU_PLUGIN_SYSCALL_ABI_I386] = 64;
    nr[QEMU_PLUGIN_SYSCALL_ABI_AARCH64] = 173;
    nr[QEMU_PLUGIN_SYSCALL_ABI_ARM] = 64;
    for (int i = 0; i < argc; i++) {
        if (!strncmp(argv[i], "n=", 2)) {
            limit = strtoul(argv[i] + 2, NULL, 0);
        }
    }
    insns = qemu_plugin_scoreboard_new(sizeof(uint64_t));
    vcpus = qemu_plugin_scoreboard_new(sizeof(Vcpu));
    qemu_plugin_register_vcpu_tb_trans_cb(id, tb_trans, NULL);
    if (!qemu_plugin_register_syscall_event_cb(id, enter, NULL, NULL)) {
        fprintf(stderr, "scsetpc: no syscall events on this target\n");
        return -1;
    }
    for (int i = 0; i < QEMU_PLUGIN_SYSCALL_ABI_COUNT; i++) {
        if (nr[i] >= 0) {
            qemu_plugin_syscall_watch(id, i, nr[i], QEMU_PLUGIN_SYSCALL_ENTER);
        }
    }
    qemu_plugin_register_atexit_cb(id, at_exit, NULL);
    return 0;
}
