/*
 * QEMU Plugin API - system-mode syscall events
 *
 * The plugin consumer of the syscall trap core (system/syscall-trap.c).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/plugin.h"
#include "hw/core/cpu.h"
#include "system/syscall-trap.h"

QEMU_BUILD_BUG_ON((int)QEMU_PLUGIN_SYSCALL_ABI_COUNT !=
                  (int)SYSCALL_ABI_COUNT);
QEMU_BUILD_BUG_ON(QEMU_PLUGIN_SYSCALL_ENTER != SYSCALL_TRAP_ENTER);
QEMU_BUILD_BUG_ON(QEMU_PLUGIN_SYSCALL_RETURN != SYSCALL_TRAP_RETURN);

static qemu_plugin_id_t sc_id;
static bool sc_registered;
static qemu_plugin_syscall_event_cb_t sc_enter_cb, sc_return_cb;
static void *sc_udata;

static void sc_to_plugin(struct qemu_plugin_syscall *p, const SyscallTrap *sc)
{
    *p = (struct qemu_plugin_syscall) {
        .abi = sc->abi, .num = sc->num, .pc = sc->pc, .ret = sc->ret,
        .skip = sc->skip, .want_return = sc->want_return,
        .restart = sc->restart, .keep = sc->keep, .cookie = sc->task,
    };
    memcpy(p->args, sc->args, sizeof(p->args));
}

static void sc_from_plugin(SyscallTrap *sc, const struct qemu_plugin_syscall *p)
{
    memcpy(sc->args, p->args, sizeof(sc->args));
    sc->ret = p->ret;
    sc->skip = p->skip;
    sc->want_return = p->want_return && sc_return_cb;
    sc->keep = p->keep;
    sc->task = p->cookie;
}

QEMU_DISABLE_CFI
static void sc_call(qemu_plugin_syscall_event_cb_t cb, CPUState *cpu,
                    SyscallTrap *sc)
{
    struct qemu_plugin_syscall p;

    sc_to_plugin(&p, sc);
    qemu_plugin_set_cb_flags(cpu, QEMU_PLUGIN_CB_RW_REGS_PC);
    cb(sc_id, cpu->cpu_index, &p, sc_udata);
    qemu_plugin_set_cb_flags(cpu, QEMU_PLUGIN_CB_NO_REGS);
    sc_from_plugin(sc, &p);
}

static void sc_enter(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    if (sc_enter_cb) {
        sc_call(sc_enter_cb, cpu, sc);
    }
}

static void sc_ret(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    sc_call(sc_return_cb, cpu, sc);
}

static const SyscallTrapOps sc_ops = {
    .enter = sc_enter,
    .ret = sc_ret,
};

bool qemu_plugin_register_syscall_event_cb(qemu_plugin_id_t id,
                                           qemu_plugin_syscall_event_cb_t enter,
                                           qemu_plugin_syscall_event_cb_t ret,
                                           void *userdata)
{
    if ((sc_registered && sc_id != id) ||
        !syscall_trap_set_consumer(&sc_ops, NULL)) {
        return false;
    }
    sc_id = id;
    sc_registered = true;
    sc_enter_cb = enter;
    sc_return_cb = ret;
    sc_udata = userdata;
    return true;
}

bool qemu_plugin_syscall_watch(qemu_plugin_id_t id,
                               enum qemu_plugin_syscall_abi abi,
                               int64_t num, unsigned flags)
{
    if (!sc_registered || sc_id != id || abi >= QEMU_PLUGIN_SYSCALL_ABI_COUNT ||
        num < -1 || num >= SYSCALL_TRAP_NUMS) {
        return false;
    }
    if (!sc_return_cb) {
        flags &= ~QEMU_PLUGIN_SYSCALL_RETURN;
    }
    syscall_trap_watch(abi, num, flags);
    return true;
}

void qemu_plugin_syscall_stats(uint64_t *pending, uint64_t *full,
                               uint64_t *stale)
{
    syscall_trap_stats(pending, full, stale);
}
