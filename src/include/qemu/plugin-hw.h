/*
 * Plugin guest-memory access and breakpoints for accelerators without a
 * softmmu TLB or instrumentation (KVM).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef QEMU_PLUGIN_HW_H
#define QEMU_PLUGIN_HW_H

#include "exec/hwaddr.h"
#include "exec/mmu-access-type.h"
#include "exec/vaddr.h"

typedef struct QemuPluginHwOps {
    /*
     * Translate @addr for an @access_type data access, as unprivileged code
     * would (@user) or at the current privilege.  Returns 0 and the guest
     * physical address in @phys; 1 if a guest fault could make it accessible
     * (@fault describes it for @raise); -1 if it is not accessible and no
     * fault would change that.
     */
    int (*translate)(CPUState *cpu, vaddr addr, MMUAccessType access_type,
                     bool user, hwaddr *phys, uint64_t *fault);
    /* Queue that fault for delivery at the current instruction. */
    void (*raise)(CPUState *cpu, vaddr addr, uint64_t fault);
    /* Open or close a user-access window (x86 EFLAGS.AC under SMAP). */
    void (*window)(CPUState *cpu, bool open);
} QemuPluginHwOps;

#ifdef CONFIG_PLUGIN
void qemu_plugin_set_hw_ops(const QemuPluginHwOps *ops);
/* A plugin wants breakpoint callbacks (and guest software breakpoints). */
bool qemu_plugin_bp_enabled(void);
/* Execution breakpoints requested by plugins */
const uint64_t *qemu_plugin_hw_breakpoints(int *n);
bool qemu_plugin_is_hw_breakpoint(uint64_t pc);
/*
 * Run the plugin breakpoint callback for @cpu (state synchronized, BQL
 * held).  Returns true if the plugin consumed the event.
 */
bool qemu_plugin_vcpu_bp(CPUState *cpu, uint64_t pc, bool sw);
#else
static inline void qemu_plugin_set_hw_ops(const QemuPluginHwOps *ops) { }
static inline bool qemu_plugin_bp_enabled(void) { return false; }
static inline const uint64_t *qemu_plugin_hw_breakpoints(int *n)
{
    *n = 0;
    return NULL;
}
static inline bool qemu_plugin_is_hw_breakpoint(uint64_t pc) { return false; }
static inline bool qemu_plugin_vcpu_bp(CPUState *cpu, uint64_t pc, bool sw)
{
    return false;
}
#endif

#endif
