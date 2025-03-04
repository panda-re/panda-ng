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
#include <vector>
#include <threads.h>

// These need to be extern "C" so that the ABI is compatible with
// QEMU/PANDA, which is written in C
extern "C" {
bool init_plugin(void *);
void uninit_plugin(void *);
#include "hooks_int_fns.h"
#include "hw_proc_id/hw_proc_id_ext.h"
}

using namespace std;

bool operator==(const struct hook &a, const struct hook &b){
    return memcmp(&a, &b, sizeof(struct hook)) == 0;
}

/*
 * The set wants to know if our elements are the same. We only want
 * this to happen in the case that our structs are actual duplicates.
 * Otherwise we want them ordered by address and then asid and so on.
 */
bool operator<(const struct hook &a, const struct hook &b){
    return tie(a.addr, a.asid, a.type, a.cb.before_block_exec) < tie(b.addr, b.asid, b.type, b.cb.before_block_exec);//, b.km, b.enabled);
}


thread_local vector<struct hook> temp_hooks;
thread_local unordered_map<uint64_t, set<struct hook>> hooks;
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

void add_hook(struct hook* h) {
    temp_hooks.push_back(*h);
}

void cb_tcg_codegen_middle_filter(unsigned int vcpu_index, void *_tb) {
    CPUState *cpu = panda_cpu_by_index(vcpu_index);
    TranslationBlock *tb = (TranslationBlock *)_tb;
    uint64_t pc = panda_current_pc(cpu);
    if (! temp_hooks.empty()){ 
        for (auto &h: temp_hooks) { 
            hooks[h.asid].insert(h); 
        } 
        temp_hooks.clear(); 
    } 
    target_ulong asid = get_id(cpu); 
    bool in_kernel = panda_in_kernel_external(cpu); 
    struct hook hook_container; 
    memset(&hook_container, 0, sizeof(hook_container)); 
    hook_container.addr = pc;
    set<struct hook>::iterator it; 
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
    if ((! temp_hooks.empty())){ [[unlikely]]
        for (auto &h: temp_hooks) {
            hooks[h.asid].insert(h);
        }
        temp_hooks.clear();
    }
    bool in_kernel = panda_in_kernel_external(cpu);
    struct hook hook_container;
    set<target_ulong> inserted_addresses;
    memset(&hook_container, 0, sizeof(hook_container));
    uint64_t tb_vaddr = qemu_plugin_tb_vaddr(tb);
    uint64_t tb_size = qemu_plugin_tb_size(tb);
    hook_container.addr = tb_vaddr;
    for (auto& a : hooks){
        target_ulong asid = a.first;
        set<struct hook>::iterator it;
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
                                TranslationBlock *real_tb = panda_get_tb(tb);
                                qemu_plugin_register_vcpu_insn_exec_cb(insn, cb_tcg_codegen_middle_filter, QEMU_PLUGIN_CB_NO_REGS, (void*) real_tb);
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
    panda_register_callback(_self, PANDA_CB_BLOCK_TRANSLATE, block_translate_cb);
    return true;
}

void uninit_plugin(void *self) {
    // if we don't clear tb's when this exits we have TBs which can call
    // into our exited plugin.
    disable_hooking();
}
