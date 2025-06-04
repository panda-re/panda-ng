/* PANDABEGINCOMMENT
 * 
 * Authors:
 * Luke Craig luke.craig@ll.mit.edu
 * 
 * This work is licensed under the terms of the GNU GPL, version 2. 
 * See the COPYING file in the top-level directory. 
 * 
PANDAENDCOMMENT */
// This needs to be defined before anything is included in order to get
// the PRIx64 macro
#define __STDC_FORMAT_MACROS

#include "panda.h"
#include <unordered_map>
#include <vector>
#include <algorithm>

// These need to be extern "C" so that the ABI is compatible with
// QEMU/PANDA, which is written in C
extern "C" {
bool init_plugin(void *);
void uninit_plugin(void *);
bool hypercall(CPUState *cpu);
#include "hypercaller.h"
}

std::unordered_map<target_ulong, std::vector<hypercall_t>> hypercalls;

bool debug = false;

#define log(...) if (debug) {printf(__VA_ARGS__);}


void register_hypercall(uint32_t magic, hypercall_t hyp){
    log("registering hypercall: magic = 0x%x\n", magic);
    // Add the function to the vector for this magic
    hypercalls[magic].push_back(hyp);
}

void unregister_hypercall(uint32_t magic, hypercall_t hyp){
    auto it = hypercalls.find(magic);
    if (it != hypercalls.end()) {
        auto &vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), hyp), vec.end());
        if (vec.empty()) {
            hypercalls.erase(it);
        }
    }
}

bool guest_hypercall(CPUState *cpu) {
    target_ulong magic = panda_get_syscall_arg(cpu, 0);
    log("guest_hypercall: magic = %x\n", magic);
    auto it = hypercalls.find(magic);
    if (it != hypercalls.end()){
        for (auto &func : it->second) {
            func(cpu);
        }
        return true;
    }
    return false;
}

bool init_plugin(void *self) {
    panda_arg_list *plugin_args = panda_get_args(PLUGIN_NAME);
    debug = panda_parse_bool_opt(plugin_args, "debug", "Enable debug output");
    panda_cb pcb = { .guest_hypercall = guest_hypercall};
    panda_register_callback(self, PANDA_CB_GUEST_HYPERCALL, pcb);
    return true;
}

void uninit_plugin(void *self) {}
