/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "hw/core/cpu.h"
#include "system/cpus.h"
#include "system/runstate.h"
#include "system/syscall-trap.h"
#include "panda/plugin.h"
#include "panda/syscall-observer.h"

extern int nb_panda_plugins;
extern panda_plugin panda_plugins[];
extern int panda_main_walk_depth;

static void *observer_owner, *observer_opaque;
static PandaSyscallObserverOps observer_ops;
static bool observer_changing;

static bool observer_quiescent(void)
{
    CPUState *cpu;
    if (current_cpu) {
        return false;
    }
    /* Panda.load_plugin may precede panda_init: no vCPUs or main/BQL yet. */
    if (!first_cpu) {
        return true;
    }
    if (!bql_locked() || !qemu_in_main_thread()) {
        return false;
    }
    CPU_FOREACH(cpu) {
        if (!cpu_is_stopped(cpu)) {
            return false;
        }
    }
    return true;
}

static bool observer_live(void *owner)
{
    for (int i = 0; i < nb_panda_plugins; i++) {
        if (panda_plugins[i].plugin == owner && !panda_plugins[i].unload) {
            return true;
        }
    }
    return false;
}

static PandaSyscallObservation observation(const SyscallTrap *sc)
{
    PandaSyscallObservation event = {
        .abi = sc->abi, .num = sc->num, .pc = sc->pc,
        .ret = sc->ret, .restart = sc->restart,
    };
    memcpy(event.args, sc->args, sizeof(event.args));
    return event;
}

static void observer_enter(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    PandaSyscallObservation event = observation(sc);
    sc->data = observer_ops.enter(cpu, &event, observer_opaque);
    sc->want_return = sc->want_return && sc->data != NULL;
}

static void observer_ret(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    PandaSyscallObservation event = observation(sc);
    if (!event.restart && observer_ops.accept_return &&
        !observer_ops.accept_return(cpu, &event, sc->data, observer_opaque)) {
        sc->keep = true;
        return;
    }
    observer_ops.ret(cpu, &event, sc->data, observer_opaque);
}

static void observer_drop(SyscallTrap *sc, void *opaque)
{
    observer_ops.drop(sc->data, observer_opaque);
}

static const SyscallTrapOps trap_observer_ops = {
    .enter = observer_enter, .ret = observer_ret, .drop = observer_drop,
};

bool panda_syscall_observer_register(void *owner, const PandaSyscallObserverOps *ops, void *opaque)
{
    if (!owner || !ops || !ops->enter || !ops->ret || !ops->drop ||
        observer_owner || observer_changing || !observer_quiescent() ||
        !observer_live(owner)) {
        return false;
    }
    if (!syscall_trap_set_consumer(&trap_observer_ops, NULL)) {
        return false;
    }
    observer_ops = *ops;
    observer_owner = owner;
    observer_opaque = opaque;
    return true;
}

bool panda_syscall_observer_watch(void *owner, int abi, int64_t num, unsigned flags)
{
    if (!owner || owner != observer_owner || observer_changing ||
        !observer_quiescent() || abi != SYSCALL_ABI_X86_64 ||
        num < -1 || num >= SYSCALL_TRAP_NUMS ||
        flags & ~(SYSCALL_TRAP_ENTER | SYSCALL_TRAP_RETURN) ||
        ((flags & SYSCALL_TRAP_RETURN) && !(flags & SYSCALL_TRAP_ENTER))) {
        return false;
    }
    syscall_trap_watch(abi, num, flags);
    return true;
}

bool panda_syscall_observer_unregister(void *owner)
{
    /* Unrelated/default plugins require no syscall bridge preconditions. */
    if (!observer_owner || owner != observer_owner) {
        return true;
    }
    if (observer_changing || !observer_quiescent()) {
        return false;
    }
    observer_changing = true;
    /* Drop handlers may request plugin unload. Keep their owner and dependencies
     * mapped until the outer unload/reset walk (or next main loop) drains it. */
    panda_main_walk_depth++;
    bool cleared = syscall_trap_clear_consumer(&trap_observer_ops, NULL);
    panda_main_walk_depth--;
    if (cleared) {
        observer_owner = observer_opaque = NULL;
        memset(&observer_ops, 0, sizeof(observer_ops));
    }
    observer_changing = false;
    return cleared;
}
