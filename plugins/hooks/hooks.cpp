/* PANDABEGINCOMMENT
 * 
 * Authors:
 *  Luke Craig                  luke.craig@ll.mit.edu
 *  Andrew Fasano               andrew.fasano@ll.mit.edu
 *  Nick Gregory                ngregory@nyu.edu
 * 
 * This work is licensed under the terms of the GNU GPL, version 2. 
 * See the COPYING file in the top-level directory. 
 * 
PANDAENDCOMMENT */


// This needs to be defined before anything is included in order to get
// the PRIx64 macro
#define __STDC_FORMAT_MACROS

#include "panda.h"
#include <iostream>
#include <unordered_map>
#include <osi/osi_types.h>
#include <set>
#include <queue>
#include <deque>
#include <vector>
#include <tuple>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>

// These need to be extern "C" so that the ABI is compatible with
// QEMU/PANDA, which is written in C
extern "C" {
bool init_plugin(void *);
void uninit_plugin(void *);
#include "hooks_int_fns.h"
#include "hw_proc_id/hw_proc_id_ext.h"
}

bool operator==(const struct hook &a, const struct hook &b){
    return memcmp(&a, &b, sizeof(struct hook)) == 0;
}

/*
 * The set wants to know if our elements are the same. We only want
 * this to happen in the case that our structs are actual duplicates.
 * Otherwise we want them ordered by address and then asid and so on.
 */
bool operator<(const struct hook &a, const struct hook &b){
    return std::tie(a.addr, a.asid, a.type, a.cb.before_block_exec) < std::tie(b.addr, b.asid, b.type, b.cb.before_block_exec);//, b.km, b.enabled);
}


/*
 * Hooks added and not yet in every vCPU's sets: one log for all vCPUs. add_hook appends to it
 * under added_lock, on any thread (a callback on some vCPU, the main loop, or Python before the
 * vCPUs exist). Each vCPU keeps a cursor and, when it translates a block or runs a hooked
 * instruction, copies the entries past it into its own sets; with nothing new that is a compare,
 * no lock. Entries sit in fixed-size chunks that never move, so readers take no lock: added_count
 * is published after the entry is written. A chunk is freed once every vCPU has read past it, so
 * a vCPU that never translates (halted) keeps the log growing by one entry per add, as its own
 * list did before.
 */
constexpr unsigned ADDED_CHUNK = 256;
struct added_chunk {
    struct hook hooks[ADDED_CHUNK];
    std::atomic<added_chunk *> next{nullptr};
};
struct added_cursor {
    added_chunk *chunk = nullptr;       // holds entry `seen`, or ends just before it (vCPU-owned)
    uint64_t base = 0;                  // the index of chunk->hooks[0] (vCPU-owned)
    std::atomic<uint64_t> seen{0};      // entries copied into the sets; add_hook reads it
};
static std::mutex added_lock;
// The oldest chunk kept, the index of its first entry, and the newest chunk: under added_lock.
static added_chunk *added_first = new added_chunk;
static uint64_t added_first_base;
static added_chunk *added_last = added_first;
static std::atomic<uint64_t> added_count{0};
static added_cursor added_cursors[16];
static std::unordered_map<uint64_t, std::set<struct hook> > per_guest_cpu_hooks[16];
panda_cb block_translate_cb;

// Handle to self
void* self = NULL;

// Enable and disable callbacks
void enable_hooking() {
    assert(self != NULL);
    panda_enable_callback(self, PANDA_CB_BLOCK_TRANSLATE, block_translate_cb);
}

void disable_hooking() {
    assert(self != NULL);
    panda_disable_callback(self, PANDA_CB_BLOCK_TRANSLATE, block_translate_cb);
}

// Free the chunks every vCPU has read past. Called with added_lock held.
static void free_read_chunks(void)
{
    int n = qemu_plugin_num_vcpus();
    if (n == 0) {
        return;                         // no vCPU yet: every entry is still to be read
    }
    uint64_t min = UINT64_MAX;
    for (int i = 0; i < n; ++i) {
        assert(i < sizeof(added_cursors)/sizeof(added_cursors[0]));
        min = std::min(min, added_cursors[i].seen.load(std::memory_order_acquire));
    }
    // A vCPU whose cursor is at a chunk's end may still hold it, to follow `next`: free a chunk
    // only once every vCPU has read an entry of the one after it.
    while (added_first != added_last && added_first_base + ADDED_CHUNK < min) {
        added_chunk *c = added_first;
        added_first = c->next.load(std::memory_order_relaxed);
        added_first_base += ADDED_CHUNK;
        delete c;
    }
}

void add_hook(struct hook* h) {
    {
        std::lock_guard<std::mutex> g(added_lock);
        uint64_t n = added_count.load(std::memory_order_relaxed);
        if (n % ADDED_CHUNK == 0 && n != 0) {
            added_chunk *c = new added_chunk;
            added_last->next.store(c, std::memory_order_release);
            added_last = c;
            free_read_chunks();
        }
        added_last->hooks[n % ADDED_CHUNK] = *h;
        added_count.store(n + 1, std::memory_order_release);
    }
    // Hooks are found at translation (cb_block_translate_callback), keyed by virtual address. A
    // CF_PCREL block is shared by every mapping of its code, so one translated where no hook was
    // (libc at another address in another process) would run here without it: translate this
    // page per virtual address, and drop blocks already translated over the hook. After the hook
    // is in the log: a vCPU that translates the block again reads the log first.
    panda_mark_hook_address(h->addr);
}

// Copy the hooks added since the last call into vCPU @vcpu's sets. Only that vCPU's thread calls it.
static void take_new_hooks(unsigned vcpu, std::unordered_map<uint64_t, std::set<struct hook> > &hooks)
{
    assert(vcpu < sizeof(added_cursors)/sizeof(added_cursors[0]));
    added_cursor &c = added_cursors[vcpu];
    uint64_t n = added_count.load(std::memory_order_acquire);
    uint64_t seen = c.seen.load(std::memory_order_relaxed);
    if (seen == n) { [[likely]]
        return;
    }
    if (!c.chunk) {
        // First read. A vCPU that existed before any chunk was freed starts at entry 0; one added
        // later (hotplug) starts at the oldest entry kept.
        std::lock_guard<std::mutex> g(added_lock);
        c.chunk = added_first;
        c.base = added_first_base;
        seen = std::max(seen, c.base);
    }
    for (; seen < n; ++seen) {
        if (seen == c.base + ADDED_CHUNK) {
            c.chunk = c.chunk->next.load(std::memory_order_acquire);
            c.base = seen;
        }
        const struct hook &h = c.chunk->hooks[seen - c.base];
        hooks[h.asid].insert(h);
    }
    c.seen.store(seen, std::memory_order_release);
}

// An instrumented hook address: the insn callback's udata. The callback can't take the pc from the
// CPU: these callbacks don't sync registers (NO_REGS), so inside a block the CPU's pc is still the
// block's start, and a hook there or past it never matched. Sites live as long as the plugin (one
// per instrumented translation of a hooked instruction), per translating vCPU like the hook sets.
struct hook_site {
    TranslationBlock *tb;
    uint64_t addr;
};
static std::deque<hook_site> hook_sites[16];

void cb_tcg_codegen_middle_filter(unsigned int vcpu_index, void *_site) {
    CPUState *cpu = panda_cpu_by_index(vcpu_index);
    hook_site *site = (hook_site *)_site;
    TranslationBlock *tb = site->tb;
    uint64_t pc = site->addr;
    auto& hooks = per_guest_cpu_hooks[vcpu_index];
    take_new_hooks(vcpu_index, hooks);
    target_ulong asid = get_id(cpu); 
    bool in_kernel = panda_in_kernel_external(cpu); 
    struct hook hook_container; 
    memset(&hook_container, 0, sizeof(hook_container)); 
    hook_container.addr = pc;
    std::set<struct hook>::iterator it; 
    hook_container.asid = asid; 
    it = hooks[asid].lower_bound(hook_container); 
    while(it != hooks[asid].end() && it->addr == pc){
        auto h = (hook*)&(*it);
        if (h->enabled){
            if (h->asid == asid){
                if (h->km == MODE_ANY || (in_kernel && h->km == MODE_KERNEL_ONLY) || (!in_kernel && h->km == MODE_USER_ONLY)){
                    (*(h->cb.before_tcg_codegen))(cpu, tb, h);
                    if (!h->enabled){
                        it = hooks[asid].erase(it);
                        continue;
                    }
                }
            }
        }
        ++it;
    } 
    if (asid != 0){ 
        asid = 0;
        hook_container.asid = asid;
        it = hooks[asid].lower_bound(hook_container);
        while(it != hooks[asid].end() && it->addr == pc){
            auto h = (hook*)&(*it);
            if (h->enabled){
                if (h->asid == asid){
                    if (h->km == MODE_ANY || (in_kernel && h->km == MODE_KERNEL_ONLY) || (!in_kernel && h->km == MODE_USER_ONLY)){
                        (*(h->cb.before_tcg_codegen))(cpu, tb, h);
                        if (!h->enabled){
                            it = hooks[asid].erase(it);
                            continue;
                        }
                    }
                }
            }
            ++it;
        }
    }
}

void cb_block_translate_callback (CPUState* cpu, struct qemu_plugin_tb *tb) {
    auto& hooks = per_guest_cpu_hooks[cpu->cpu_index];
    take_new_hooks(cpu->cpu_index, hooks);
    bool in_kernel = panda_in_kernel_external(cpu);
    struct hook hook_container;
    std::set<target_ulong> inserted_addresses;
    memset(&hook_container, 0, sizeof(hook_container));
    // The block's address without reading it through the plugin API, which would bind every TB
    // to its address (patch 0034) and stop CF_PCREL sharing: at translation the CPU's pc is the
    // block's start. Hooked pages translate per virtual address (add_hook); a TB that gets a hook
    // is bound below as well.
    uint64_t tb_vaddr = panda_current_pc(cpu);
    uint64_t tb_size = qemu_plugin_tb_size(tb);
    hook_container.addr = tb_vaddr;
    for (auto& a : hooks){
        target_ulong asid = a.first;
        std::set<struct hook>::iterator it;
        hook_container.asid = asid;
        it = hooks[asid].lower_bound(hook_container); 
        while(it != hooks[asid].end() && it->addr < tb_vaddr + tb_size){
            auto h = (hook*)&(*it);
            if (h->enabled) { [[likely]]
                if (h->asid == asid){ 
                    if (h->km == MODE_ANY || (in_kernel && h->km == MODE_KERNEL_ONLY) || (!in_kernel && h->km == MODE_USER_ONLY)){
                        auto exclude = inserted_addresses.find(h->addr);
                        if (exclude == inserted_addresses.end()){
                            struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn_by_vaddr(tb, h->addr);
                            if (insn != NULL){
                                qemu_plugin_tb_vaddr(tb);   // hooked: bind the TB to this address
                                auto& sites = hook_sites[cpu->cpu_index];
                                sites.push_back({panda_get_tb(tb), h->addr});
                                qemu_plugin_register_vcpu_insn_exec_cb(insn, cb_tcg_codegen_middle_filter, QEMU_PLUGIN_CB_NO_REGS, (void*) &sites.back());
                                inserted_addresses.insert(h->addr);
                            }
                        } 
                    }
                } 
            }else{
                it = hooks[asid].erase(it);
                continue;
            }
            ++it;
        }
    }
}


bool init_plugin(void *_self) {
    // On init, register a callback but don't enable it
    self = _self;
    panda_enable_precise_pc();
    panda_require("hw_proc_id");
    assert(init_hw_proc_id_api());
    block_translate_cb.block_translate = cb_block_translate_callback;
    panda_register_callback_modes(_self, PANDA_CB_BLOCK_TRANSLATE, block_translate_cb,
            panda_callback_supported_modes(PANDA_CB_BLOCK_TRANSLATE), true);
    return true;
}

void uninit_plugin(void *self) {
    // if we don't clear tb's when this exits we have TBs which can call
    // into our exited plugin.
    disable_hooking();
}
