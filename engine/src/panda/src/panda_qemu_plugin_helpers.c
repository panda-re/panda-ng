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
#include "accel/tcg/cpu-ops.h"
#include "tcg/tcg.h"
#include "panda/panda_qemu_plugin_helpers.h"
#include "panda/hooks.h"
#include "qemu/plugin.h"
#include "qemu/plugin-memory.h"
#include "exec/memopidx.h"
#include "qemu/rcu.h"
#include "system/memory.h"


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

static bool panda_any_enabled(panda_cb_type type);

unsigned panda_bridge_wants(void){
    unsigned wants = 0;
    if (panda_has_callback_registered(PANDA_CB_INSN_TRANSLATE)) {
        wants |= PANDA_BRIDGE_INSN;
    }
    if (panda_any_enabled(PANDA_CB_START_BLOCK_EXEC)
        || panda_any_enabled(PANDA_CB_BEFORE_BLOCK_EXEC)
        || panda_any_enabled(PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT)) {
        wants |= PANDA_BRIDGE_BLOCK;
    }
    if (panda_any_enabled(PANDA_CB_END_BLOCK_EXEC)
        || panda_any_enabled(PANDA_CB_AFTER_BLOCK_EXEC)) {
        wants |= PANDA_BRIDGE_END;
    }
    return wants;
}

uint64_t panda_bridge_virt_to_phys(CPUState *cpu, uint64_t vaddr){
    hwaddr pa = panda_virt_to_phys(cpu, vaddr);
    return pa == (hwaddr) -1 ? (uint64_t) -1 : (uint64_t) pa;
}

/* On a TLB miss before the access: the page walk (as panda_virt_to_phys, privileged if need be)
 * and, if @is_io, whether the access will go to a device. That is what tlb_set_page_full decides
 * (TLB_MMIO) when the access fills the TLB: a load goes to a device unless the page is RAM or ROMD
 * (a ROM device's reads come from its RAM), a store unless the page is RAM (a ROMD write goes to
 * the device). The walk's transaction attributes pick the address space, as the fill does. */
static uint64_t before_phys_walk(CPUState *cpu, uint64_t vaddr, bool is_store, bool *is_io){
    TranslateForDebugResult res;
    uint64_t page = vaddr & TARGET_PAGE_MASK;
    bool ok = cpu_translate_for_debug(cpu, page, &res);

    if (!ok && enter_priv(cpu)) {
        ok = cpu_translate_for_debug(cpu, page, &res);
        exit_priv(cpu);
    }
    if (!ok) {
        return (uint64_t) -1;
    }
    hwaddr pa = (res.physaddr & TARGET_PAGE_MASK) | (vaddr & ~TARGET_PAGE_MASK);
    if (is_io) {
        AddressSpace *as = cpu_get_address_space(cpu, cpu_asidx_from_attrs(cpu, res.attrs));
        hwaddr xlat, len = 1;
        MemoryRegion *mr;

        RCU_READ_LOCK_GUARD();
        mr = address_space_translate(as, pa, &xlat, &len, is_store, res.attrs);
        *is_io = !memory_region_is_ram(mr) && (is_store || !memory_region_is_romd(mr));
    }
    return pa;
}

/* As qemu_plugin_get_hwaddr, without its complaint on a TLB miss: before the access, the entry it
 * is about to use may not be there yet (first touch since a flush). Only then walk the page tables.
 * A hit is also the faithful answer: it is the translation the access itself will use (a walk
 * would see page-table changes the guest hasn't flushed from its TLB yet). Checked against the
 * walk on every access over cold boots of ubuntu-24.04 x86_64 and aarch64 and debian-13 x86_64
 * plus tests/mem.py: ~3.2e9 hits, no mismatch. A hit's is_io is the entry's TLB_MMIO for this
 * direction; a miss looks the region up (before_phys_walk), only when @is_io is asked for. */
uint64_t panda_bridge_before_phys(CPUState *cpu, uint32_t info, uint64_t vaddr, bool is_store,
                                  bool *is_io){
    struct qemu_plugin_hwaddr h;

    if (is_io) {
        *is_io = false;
    }
    if (tlb_plugin_lookup(cpu, vaddr, get_mmuidx(info), is_store, &h)) {
        if (is_io) {
            *is_io = h.is_io;
        }
        return h.phys_addr;
    }
    return before_phys_walk(cpu, vaddr, is_store, is_io);
}

int panda_memcb_rw(bool is_store){
    int rw = 0;
    if (is_store ? panda_has_callback_registered(PANDA_CB_VIRT_MEM_BEFORE_WRITE)
                   || panda_has_callback_registered(PANDA_CB_PHYS_MEM_BEFORE_WRITE)
                   || panda_has_callback_registered(PANDA_CB_MMIO_BEFORE_WRITE)
                 : panda_has_callback_registered(PANDA_CB_VIRT_MEM_BEFORE_READ)
                   || panda_has_callback_registered(PANDA_CB_PHYS_MEM_BEFORE_READ)) {
        rw |= QEMU_PLUGIN_MEM_BEFORE;
    }
    if (is_store ? panda_has_callback_registered(PANDA_CB_VIRT_MEM_AFTER_WRITE)
                   || panda_has_callback_registered(PANDA_CB_PHYS_MEM_AFTER_WRITE)
                 : panda_has_callback_registered(PANDA_CB_VIRT_MEM_AFTER_READ)
                   || panda_has_callback_registered(PANDA_CB_PHYS_MEM_AFTER_READ)
                   || panda_has_callback_registered(PANDA_CB_MMIO_AFTER_READ)) {
        rw |= QEMU_PLUGIN_MEM_AFTER;
    }
    return rw ? rw | (is_store ? QEMU_PLUGIN_MEM_W : QEMU_PLUGIN_MEM_R) : 0;
}

static bool panda_any_enabled(panda_cb_type type);

/* Enabled, not merely registered: the bridge walks the page tables before every access of that
 * direction when this is true. A registered-but-disabled phys_mem_before_* callback (api.py,
 * mem.py: registered disabled, enabled in a window) keeps its instrumentation, so enabling it
 * needs no retranslation, but must not cost a software page walk per access while disabled: on a
 * HZ=1000 guest (ubuntu-24.04) that walk made the guest so slow its serial console stopped
 * answering. */
bool panda_memcb_phys_before(bool is_store){
    return panda_any_enabled(is_store ? PANDA_CB_PHYS_MEM_BEFORE_WRITE
                                      : PANDA_CB_PHYS_MEM_BEFORE_READ);
}

/* mmio_before_write enabled: before each store the bridge asks whether it goes to a device (a TLB
 * lookup, a page walk and region lookup on a miss). Registered and disabled: no lookup. */
bool panda_memcb_mmio_before_write(void){
    return panda_any_enabled(PANDA_CB_MMIO_BEFORE_WRITE);
}

bool panda_memcb_before_enabled(bool is_store){
    return is_store ? panda_any_enabled(PANDA_CB_VIRT_MEM_BEFORE_WRITE)
                      || panda_any_enabled(PANDA_CB_PHYS_MEM_BEFORE_WRITE)
                      || panda_any_enabled(PANDA_CB_MMIO_BEFORE_WRITE)
                    : panda_any_enabled(PANDA_CB_VIRT_MEM_BEFORE_READ)
                      || panda_any_enabled(PANDA_CB_PHYS_MEM_BEFORE_READ);
}

/* io_prepare's test: a device access from an instruction that is not the last of its TB makes it
 * rewind the TB (cpu_io_recompile) and run the instruction again in a TB of its own. */
bool panda_bridge_io_rewinds(CPUState *cpu){
    return !cpu->neg.can_do_io;
}

/* Only invalidate_opt retains state, to avoid invalidating its fresh translation.
 * No block-entry/end pairing, deferred callbacks or per-block stores. */
#define PANDA_BLOCK_SLOTS 1024
static struct {
    bool skip;
    uint64_t pc;
} invalidated[PANDA_BLOCK_SLOTS];

target_ulong panda_tb_pc(CPUState *cpu, TranslationBlock *tb)
{
    if (!tb) {
        return 0;
    }
    if (tb_pc_exact(tb)) {
        return tb->pc;
    }
    cpu = cpu ? cpu : current_cpu;
    return cpu ? cpu->cc->get_pc(cpu) : 0;
}

static bool panda_any_enabled(panda_cb_type type){
    for (panda_cb_list *plist = panda_cbs[type]; plist != NULL; plist = plist->next) {
        if (qatomic_read(&plist->enabled)) {
            return true;
        }
    }
    return false;
}

/* Called by TCG at the outgoing edge, after guest operations and PC updates. */
void panda_block_end(CPUState *cpu, TranslationBlock *tb)
{
    panda_callbacks_end_block_exec(cpu, tb);
    /* Compatibility spelling: normal completion only, like end_block_exec. */
    if (unlikely(panda_cbs[PANDA_CB_AFTER_BLOCK_EXEC])) {
        panda_callbacks_after_block_exec_dispatch(cpu, tb, TB_EXIT_IDX0);
    }
}

G_NORETURN void panda_block_leave_at_start(CPUState *cpu, TranslationBlock *tb);

/* before_block_exec_invalidate_opt: a callback asked for this block to be translated again before
 * it runs. Drop its TB (the part on its first page is enough to make the lookup miss) and leave
 * it; the next start at this pc is the new TB's and isn't asked again, as in PANDA's
 * after_find_fast. */
static void panda_block_invalidate(CPUState *cpu, TranslationBlock *tb){
    tb_page_addr_t start = tb_page_addr0(tb);
    tb_page_addr_t page_last = start | ~(tb_page_addr_t) TARGET_PAGE_MASK;
    tb_page_addr_t last = start + tb->size - 1;

    if (cpu->cpu_index < PANDA_BLOCK_SLOTS) {
        invalidated[cpu->cpu_index].skip = true;
        invalidated[cpu->cpu_index].pc = panda_current_pc(cpu);
    }
    if (start != (tb_page_addr_t) -1) {
        tb_invalidate_phys_range(cpu, start, MIN(last, page_last));
    }
    panda_block_leave_at_start(cpu, tb);
}

/* Leave a block from its start callback, before its first instruction. Under icount, the TB's
 * prologue (gen_tb_start, before the plugin's TB-start callback) has already taken its whole
 * instruction count from the budget, and cpu_loop_exit_noexc doesn't give it back
 * (cpu_restore_state would): give it back here, or each such exit counts tb->icount instructions
 * that never ran, and a replay drifts from its recording. The PC is the block's (CF_PCREL, or
 * panda_block_sync_pc), unless a callback set another one to go there. */
void panda_block_leave_at_start(CPUState *cpu, TranslationBlock *tb){
    if (tb_cflags(tb) & CF_USE_ICOUNT) {
        cpu->neg.icount_decr.u16.low += tb->icount;
    }
    cpu_loop_exit_noexc(cpu);
}

/* Without CF_PCREL (mips, ppc, any page with a TCG hook), goto_tb chaining leaves the PC store to
 * the unchained exit: at a chained block's start the CPU's PC is still that of some earlier block.
 * Make it this block's, as cpu_tb_exec does for a TB that exits at its entry, before anything reads
 * it (the callbacks, panda_current_pc, cpu_loop_exit_noexc's next lookup). Such code never reads
 * the PC from the CPU state, so this changes nothing for the block itself. CF_PCREL code keeps the
 * PC current. Only here, i.e. only while a block-exec callback is enabled. */
static inline void panda_block_sync_pc(CPUState *cpu, TranslationBlock *tb){
    if (!(tb_cflags(tb) & CF_PCREL)) {
        const TCGCPUOps *ops = cpu->cc->tcg_ops;
        if (ops->synchronize_from_tb) {
            ops->synchronize_from_tb(cpu, tb);
        } else {
            cpu->cc->set_pc(cpu, tb->pc);
        }
    }
}

void panda_block_start(CPUState *cpu, TranslationBlock *tb)
{
    panda_block_sync_pc(cpu, tb);
    if (unlikely(panda_cbs[PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT])) {
        unsigned index = cpu->cpu_index;
        if (index < PANDA_BLOCK_SLOTS && invalidated[index].skip
            && invalidated[index].pc == panda_current_pc(cpu)) {
            invalidated[index].skip = false;
        } else {
            if (index < PANDA_BLOCK_SLOTS) {
                invalidated[index].skip = false;
            }
            if (panda_callbacks_before_block_exec_invalidate_opt(cpu, tb)) {
                panda_block_invalidate(cpu, tb);
            }
        }
    }
    if (unlikely(panda_cbs[PANDA_CB_BEFORE_BLOCK_EXEC])) {
        panda_callbacks_before_block_exec_dispatch(cpu, tb);
    }
    panda_callbacks_start_block_exec(cpu, tb);
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
