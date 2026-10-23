/*
 * TCG address hooks: translator and vCPU-setup interface.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ACCEL_TCG_HOOKS_INTERNAL_H
#define ACCEL_TCG_HOOKS_INTERNAL_H

#include "qemu/bitops.h"
#include "exec/target_page.h"
#include "exec/translation-block.h"
#include "exec/translator.h"

extern bool tcg_hooks_enabled;

/*
 * Guest virtual pages holding a hook site, as a bitmap indexed by a hash
 * of the page number.  Bits are only ever set.  A false hit costs only
 * PC-relative code on that page.
 */
#define TCG_HOOKS_PAGE_BITS 20
extern unsigned long tcg_hooks_pages[];

static inline unsigned long tcg_hooks_page_bit(vaddr pc)
{
    return ((uint64_t)(pc >> TARGET_PAGE_BITS) * 0x9e3779b97f4a7c15ull)
           >> (64 - TCG_HOOKS_PAGE_BITS);
}

/*
 * The cflags to look up or translate a TB at @pc with: no CF_PCREL on a
 * page that holds a hook site, so that code there is translated for its
 * virtual address and gets its hooks.  Elsewhere CF_PCREL stays on.
 */
static inline uint32_t tcg_hooks_cflags(vaddr pc, uint32_t cflags)
{
    if (unlikely(qatomic_read(&tcg_hooks_enabled)) && (cflags & CF_PCREL)) {
        unsigned long b = tcg_hooks_page_bit(pc);

        if (qatomic_read(&tcg_hooks_pages[BIT_WORD(b)]) & BIT_MASK(b)) {
            cflags &= ~CF_PCREL;
        }
    }
    return cflags;
}

/* Mark the pages of sites inserted before the page size was decided. */
void tcg_hooks_vcpu_init(CPUState *cpu);

/* Emit the hook call, if @db->pc_next has a site.  After insn_start. */
void tcg_hooks_gen(DisasContextBase *db);

/* Index a newly linked block (hooks in use), forget all on a flush. */
void tcg_hooks_tb_linked(TranslationBlock *tb);
void tcg_hooks_tb_flush(void);

#endif
