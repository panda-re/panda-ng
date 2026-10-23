#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "panda/debug.h"
#include "panda/plugin.h"
#include "panda/common.h"
#include "panda/callbacks/cb-support.h"
#include "exec/translator.h"
#include "exec/translation-block.h"
#include "accel/tcg/cpu-loop.h"
#include "tcg/tcg.h"
#include "panda/panda_qemu_plugin_helpers.h"
#include "panda/hooks.h"


CPUState* panda_cpu_by_index(int index){
    CPUState *cpu = qemu_get_cpu(index);
    return cpu;
}

CPUState* panda_cpu_in_translate(void){
    return tcg_ctx->cpu;
}

TranslationBlock * panda_get_tb(struct qemu_plugin_tb *tb){
    const DisasContextBase *db = tcg_ctx->plugin_db;
    return db->tb;
}

static bool panda_has_callback_registered(panda_cb_type type){
    return panda_cbs[type] != NULL;
}

unsigned panda_bridge_wants(void){
    unsigned wants = 0;
    if (panda_has_callback_registered(PANDA_CB_INSN_TRANSLATE)) {
        wants |= PANDA_BRIDGE_INSN;
    }
    /* Every block-exec callback runs from the block's start (panda_block_start); before/after_block_exec
     * no longer need TB chaining off. */
    if (panda_has_callback_registered(PANDA_CB_START_BLOCK_EXEC)
        || panda_has_callback_registered(PANDA_CB_END_BLOCK_EXEC)
        || panda_has_callback_registered(PANDA_CB_BEFORE_BLOCK_EXEC)
        || panda_has_callback_registered(PANDA_CB_AFTER_BLOCK_EXEC)
        || panda_has_callback_registered(PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT)) {
        wants |= PANDA_BRIDGE_BLOCK;
    }
    if (wants & PANDA_BRIDGE_BLOCK
        || panda_has_callback_registered(PANDA_CB_BEFORE_BLOCK_EXEC)
        || panda_has_callback_registered(PANDA_CB_AFTER_BLOCK_EXEC)
        || panda_has_callback_registered(PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT)
        || panda_has_callback_registered(PANDA_CB_AFTER_BLOCK_TRANSLATE)
        || panda_has_callback_registered(PANDA_CB_BEFORE_TCG_CODEGEN)
        || panda_has_callback_registered(PANDA_CB_CPU_RESTORE_STATE)) {
        wants |= PANDA_BRIDGE_TB_PC;
    }
    return wants;
}

uint64_t panda_bridge_virt_to_phys(CPUState *cpu, uint64_t vaddr){
    hwaddr pa = panda_virt_to_phys(cpu, vaddr);
    return pa == (hwaddr) -1 ? (uint64_t) -1 : (uint64_t) pa;
}

int panda_memcb_rw(bool is_store){
    int rw = 0;
    if (is_store ? panda_has_callback_registered(PANDA_CB_VIRT_MEM_BEFORE_WRITE)
                   || panda_has_callback_registered(PANDA_CB_PHYS_MEM_BEFORE_WRITE)
                 : panda_has_callback_registered(PANDA_CB_VIRT_MEM_BEFORE_READ)
                   || panda_has_callback_registered(PANDA_CB_PHYS_MEM_BEFORE_READ)) {
        rw |= QEMU_PLUGIN_MEM_BEFORE;
    }
    if (is_store ? panda_has_callback_registered(PANDA_CB_VIRT_MEM_AFTER_WRITE)
                   || panda_has_callback_registered(PANDA_CB_PHYS_MEM_AFTER_WRITE)
                 : panda_has_callback_registered(PANDA_CB_VIRT_MEM_AFTER_READ)
                   || panda_has_callback_registered(PANDA_CB_PHYS_MEM_AFTER_READ)) {
        rw |= QEMU_PLUGIN_MEM_AFTER;
    }
    return rw ? rw | (is_store ? QEMU_PLUGIN_MEM_W : QEMU_PLUGIN_MEM_R) : 0;
}

bool panda_memcb_phys_before(bool is_store){
    return panda_has_callback_registered(is_store ? PANDA_CB_PHYS_MEM_BEFORE_WRITE
                                                  : PANDA_CB_PHYS_MEM_BEFORE_READ);
}

/* Per-vCPU block in flight: started (its before_block_exec ran), its after_block_exec (and, if its
 * start_block_exec didn't leave it, its end_block_exec) not yet run. */
typedef struct PandaPendingBlock {
    TranslationBlock *tb;
    bool end;                 /* start_block_exec returned: end_block_exec is owed too */
    bool skip_invalidate;     /* the next block start at skip_pc is the retranslation */
    uint64_t skip_pc;
} PandaPendingBlock;

#define PANDA_END_SLOTS 1024
static PandaPendingBlock pending[PANDA_END_SLOTS];

static bool panda_any_enabled(panda_cb_type type){
    for (panda_cb_list *plist = panda_cbs[type]; plist != NULL; plist = plist->next) {
        if (plist->enabled) {
            return true;
        }
    }
    return false;
}

static void panda_block_end(CPUState *cpu, uint8_t exit_code){
    if (cpu->cpu_index < PANDA_END_SLOTS) {
        PandaPendingBlock *p = &pending[cpu->cpu_index];
        TranslationBlock *tb = p->tb;
        if (unlikely(tb)) {
            bool end = p->end;
            p->tb = NULL;
            if (end) {
                panda_callbacks_end_block_exec(cpu, tb);
            }
            panda_callbacks_after_block_exec_dispatch(cpu, tb, exit_code);
        }
    }
}

void panda_end_block_flush(CPUState *cpu){
    panda_block_end(cpu, TB_EXIT_IDX0);
}

void panda_callbacks_block_longjmp(CPUState *cpu){
    panda_block_end(cpu, TB_EXIT_REQUESTED);
}

void panda_end_block_drop_all(void){
    for (int i = 0; i < PANDA_END_SLOTS; i++) {
        pending[i].tb = NULL;
    }
}

/* before_block_exec_invalidate_opt: a callback asked for this block to be translated again before
 * it runs. Drop its TB (the part on its first page is enough to make the lookup miss) and leave
 * it; the next start at this pc is the new TB's and isn't asked again, as in PANDA's
 * after_find_fast. */
static void panda_block_invalidate(CPUState *cpu, TranslationBlock *tb, PandaPendingBlock *p){
    tb_page_addr_t start = tb_page_addr0(tb);
    tb_page_addr_t page_last = start | ~(tb_page_addr_t) TARGET_PAGE_MASK;
    tb_page_addr_t last = start + tb->size - 1;

    p->skip_invalidate = true;
    p->skip_pc = panda_current_pc(cpu);
    if (start != (tb_page_addr_t) -1) {
        tb_invalidate_phys_range(cpu, start, MIN(last, page_last));
    }
    cpu_loop_exit_noexc(cpu);
}

void panda_block_start(CPUState *cpu, TranslationBlock *tb){
    PandaPendingBlock *p = cpu->cpu_index < PANDA_END_SLOTS ? &pending[cpu->cpu_index] : NULL;

    panda_end_block_flush(cpu);     /* the block before this one ran to its end */
    if (unlikely(!p)) {
        panda_callbacks_before_block_exec_dispatch(cpu, tb);
        panda_callbacks_start_block_exec(cpu, tb);
        panda_callbacks_after_block_exec_dispatch(cpu, tb, TB_EXIT_IDX0);   /* no slot: early */
        return;
    }
    if (unlikely(panda_cbs[PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT])) {
        if (p->skip_invalidate && p->skip_pc == panda_current_pc(cpu)) {
            p->skip_invalidate = false;
        } else {
            p->skip_invalidate = false;
            if (panda_callbacks_before_block_exec_invalidate_opt(cpu, tb)) {
                panda_block_invalidate(cpu, tb, p);
            }
        }
    }
    panda_callbacks_before_block_exec_dispatch(cpu, tb);
    p->tb = tb;
    p->end = false;
    panda_callbacks_start_block_exec(cpu, tb);  /* may leave the block (panda_break_exec) */
    p->end = true;
}

/* The bridge's gate: its block-start callback is conditional on a per-vCPU word it owns, which
 * PANDA sets to 1 while any block-exec callback is enabled. */
static void (*block_gate_set)(uint64_t on);
static uint64_t block_gate_on;

void panda_bridge_set_block_gate(void (*set)(uint64_t on)){
    block_gate_set = set;
    panda_block_cbs_changed();
}

static const panda_cb_type block_cb_types[] = {
    PANDA_CB_BEFORE_BLOCK_EXEC, PANDA_CB_AFTER_BLOCK_EXEC, PANDA_CB_START_BLOCK_EXEC,
    PANDA_CB_END_BLOCK_EXEC, PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT,
};

bool panda_is_block_cb_type(int type){
    for (size_t i = 0; i < ARRAY_SIZE(block_cb_types); i++) {
        if ((int) block_cb_types[i] == type) {
            return true;
        }
    }
    return false;
}

void panda_block_cbs_changed(void){
    uint64_t on = 0;
    for (size_t i = 0; i < ARRAY_SIZE(block_cb_types); i++) {
        if (panda_any_enabled(block_cb_types[i])) {
            on = 1;
            break;
        }
    }
    block_gate_on = on;
    if (block_gate_set) {
        block_gate_set(on);
    }
}

uint64_t panda_block_gate(void){
    return block_gate_on;
}
