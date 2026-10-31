/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SYSTEM_REPLAY_EFFECT_H
#define SYSTEM_REPLAY_EFFECT_H
#include <stdbool.h>
#include <stdint.h>

#define REPLAY_RAM_EFFECT_MAX (64 * 1024)
#define REPLAY_RAM_EFFECT_PENDING_MAX 64
/* Cap on one register write, not a property of any target: it covers SVE's
 * largest Z register (2048 bits). A longer one is refused at request time.
 * Register writes count against REPLAY_RAM_EFFECT_PENDING_MAX too. */
#define REPLAY_REG_EFFECT_MAX 256

/* Request admission only: main thread/BQL, every vCPU stopped, RECORD and RR
 * provisioned. Copies bytes, never writes synchronously. False has no effect.
 * Apply runs at the existing deterministic TCG scheduler async-event boundary
 * under BQL + replay mutex, before another guest instruction budget. Not on
 * the requester thread. End-record flush cancels unsaved requests.
 */
bool replay_analysis_ram_write_request(uint64_t address, const uint8_t *data,
                                        uint32_t length);
/* Same admission as RAM writes. At the apply boundary the recording checks the
 * vCPU, register and size and makes the write; the log records whether the
 * register's setter accepted it. Replay writes only what was accepted. */
bool replay_analysis_reg_write_request(int cpu_index, int reg,
                                        const uint8_t *data, uint32_t length);
void replay_analysis_effect_prepare(void *opaque);
void replay_analysis_effect_save(void *opaque);
void *replay_analysis_effect_load(void);
void replay_analysis_effect_run(void *opaque);
void replay_analysis_effect_cancel(void *opaque);

#endif
