/*
 * TCG address hooks: translator and vCPU-setup interface.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ACCEL_TCG_HOOKS_INTERNAL_H
#define ACCEL_TCG_HOOKS_INTERNAL_H

#include "exec/translator.h"

extern bool tcg_hooks_enabled;

/* Emit the hook call, if @db->pc_next has a site.  After insn_start. */
void tcg_hooks_gen(DisasContextBase *db);

/* Index a newly linked block (hooks in use), forget all on a flush. */
void tcg_hooks_tb_linked(TranslationBlock *tb);
void tcg_hooks_tb_flush(void);

/* Clear CF_PCREL from a new vCPU's cflags once hooks are in use. */
void tcg_hooks_vcpu_cflags(CPUState *cpu);

#endif
