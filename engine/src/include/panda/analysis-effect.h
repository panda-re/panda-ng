/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PANDA_ANALYSIS_EFFECT_H
#define PANDA_ANALYSIS_EFFECT_H
#include <stdbool.h>
#include <stdint.h>

/* Ordered deferred physical RAM write. See system/replay-effect.h for request
 * admission versus scheduler apply semantics. Not a vCPU callback mutation API.
 * Successful admission is not application success: the ND event records the
 * apply-boundary all-RAM preflight result (OK/ACCESS_DENIED), bytes and address.
 * A denied event changes no bytes; replay verifies the same result, then applies
 * logged successful events without the original analysis plugin/requester.
 */
#ifdef __cplusplus
extern "C" {
#endif
bool panda_analysis_ram_write(uint64_t address, const uint8_t *data,
                              uint32_t length);
/* Ordered deferred register write, same admission and ordering as RAM writes.
 * reg is the CPU's gdbstub register number and length must equal that
 * register's gdbstub size; the event records the apply-boundary check result
 * (OK/ACCESS_DENIED) and replay applies it to the same vCPU and register. */
bool panda_analysis_reg_write(int cpu_index, int reg, const uint8_t *data,
                              uint32_t length);
#ifdef __cplusplus
}
#endif
#endif
