/*
 * Translating a guest virtual page for an analysis read or write (panda_mem.c, the plugin
 * helpers' physical-address walks).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_TRANSLATE_INT_H
#define PANDA_TRANSLATE_INT_H

#include "hw/core/cpu.h"

bool panda_translate_page(CPUState *cpu, vaddr page, TranslateForDebugResult *res, bool access);
uint32_t panda_view_mmu_mask(CPUState *cpu);

#endif
