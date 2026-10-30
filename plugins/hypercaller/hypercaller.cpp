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

// These need to be extern "C" so that the ABI is compatible with
// QEMU/PANDA, which is written in C
extern "C" {
bool init_plugin(void *);
void uninit_plugin(void *);
bool hypercall(CPUState *cpu);
#include "hypercaller.h"
}

struct Handler {
    hypercall_t cb;
    bool observe_only;
};
std::unordered_map<target_ulong, Handler> hypercalls;
static void *plugin_self;
static panda_cb bridge;

static bool update_access() {
    bool observer = true;
    for (const auto &entry : hypercalls) {
        observer &= entry.second.observe_only;
    }
    return panda_set_callback_observe_only(plugin_self, PANDA_CB_GUEST_HYPERCALL,
                                          bridge, observer);
}

bool debug = false;

#define log(...) if (debug) {printf(__VA_ARGS__);}


void register_hypercall(uint32_t magic, hypercall_t hyp){
    if (!register_hypercall_observer(magic, hyp, false)) {
        fprintf(stderr, "hypercaller: registration refused (duplicate magic or mutator during replay)\n");
        abort();
    }
}

bool register_hypercall_observer(uint32_t magic, hypercall_t hyp, bool observe_only) {
    if (!hyp || hypercalls.count(magic)) {
        return false;
    }
    hypercalls[magic] = {hyp, observe_only};
    if (!update_access()) {
        hypercalls.erase(magic);
        return false;
    }
    return true;
}

void unregister_hypercall(uint32_t magic){
    hypercalls.erase(magic);
    update_access();
}

bool guest_hypercall(CPUState *cpu) {
    target_ulong magic = panda_get_syscall_arg(cpu, 0);
#if defined(TARGET_AARCH64)
    CPUArchState *env = panda_cpu_env(cpu);
    // For AARCH64 in ARM32 mode we need to use the magic in r7
    if (!env->aarch64){
        magic = env->regs[7];
    }
#endif
    log("guest_hypercall: magic = %x\n", magic);
    if (hypercalls.find(magic) != hypercalls.end()){
        hypercalls[magic].cb(cpu);
        return true;
    }
    return false;
}

bool init_plugin(void *self) {
    plugin_self = self;
    panda_arg_list *plugin_args = panda_get_args(PLUGIN_NAME);
    debug = panda_parse_bool_opt(plugin_args, "debug", "Enable debug output");
    bridge.guest_hypercall = guest_hypercall;
    return panda_register_callback_modes(self, PANDA_CB_GUEST_HYPERCALL, bridge,
            panda_callback_supported_modes(PANDA_CB_GUEST_HYPERCALL), true);
}

void uninit_plugin(void *self) {}
