/*
 * The configurable machine's per-target CPU setup (hw/core/configurable-cpu.c,
 * compiled per target; the machine itself is target-independent).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef HW_CORE_CONFIGURABLE_CPU_H
#define HW_CORE_CONFIGURABLE_CPU_H

#include "hw/core/irq.h"

typedef struct ConfigurableCpuOps {
    /*
     * Create and realize a CPU of @cpu_type with what a board would give
     * it: its timers, its interrupt inputs. NULL with @errp set if the
     * type can't run on the configurable machine.
     */
    CPUState *(*create)(const char *cpu_type, Error **errp);
    /* Its interrupt inputs, for 'cpu:N' (NULL: the CPU's GPIO inputs). */
    int (*irq_count)(CPUState *cpu);
    qemu_irq (*irq_in)(CPUState *cpu, int n);
    /* After cpu_reset(), at every machine reset (NULL: nothing more). */
    void (*reset)(CPUState *cpu);
    /* The ELF e_machine -kernel must have. */
    int (*elf_machine)(CPUState *cpu);
} ConfigurableCpuOps;

/*
 * This target's ops, or NULL: the configurable machine has no CPU setup
 * for it (yet) and refuses to build.
 */
const ConfigurableCpuOps *configurable_cpu_ops(void);

#endif
