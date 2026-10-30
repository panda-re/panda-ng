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
#include "hypercaller.h"
}

struct Handler {
    hypercall_t cb;
    bool observe_only;
};
std::unordered_map<target_ulong, Handler> hypercalls;
static void *plugin_self;
/* One bridge per kind: during record/replay the engine skips the mutator bridge
 * and keeps running the observer bridge. The mutator bridge is enabled only
 * while a mutating handler exists, so begin warns only about real mutators. */
static panda_cb observer_bridge, mutator_bridge;
static unsigned mutators;

bool debug = false;

#define log(...) if (debug) {printf(__VA_ARGS__);}


void register_hypercall(uint32_t magic, hypercall_t hyp){
    if (!register_hypercall_observer(magic, hyp, false)) {
        fprintf(stderr, "hypercaller: registration refused (duplicate magic)\n");
        abort();
    }
}

bool register_hypercall_observer(uint32_t magic, hypercall_t hyp, bool observe_only) {
    if (!hyp || hypercalls.count(magic)) {
        return false;
    }
    hypercalls[magic] = {hyp, observe_only};
    if (!observe_only && mutators++ == 0) {
        panda_enable_callback(plugin_self, PANDA_CB_GUEST_HYPERCALL, mutator_bridge);
    }
    return true;
}

void unregister_hypercall(uint32_t magic){
    auto it = hypercalls.find(magic);
    if (it == hypercalls.end()) {
        return;
    }
    bool observe_only = it->second.observe_only;
    hypercalls.erase(it);
    if (!observe_only && --mutators == 0) {
        panda_disable_callback(plugin_self, PANDA_CB_GUEST_HYPERCALL, mutator_bridge);
    }
}

static bool dispatch(CPUState *cpu, bool observe_only) {
    target_ulong magic = panda_get_syscall_arg(cpu, 0);
#if defined(TARGET_AARCH64)
    CPUArchState *env = panda_cpu_env(cpu);
    // For AARCH64 in ARM32 mode we need to use the magic in r7
    if (!env->aarch64){
        magic = env->regs[7];
    }
#endif
    log("guest_hypercall: magic = %x\n", magic);
    auto it = hypercalls.find(magic);
    if (it != hypercalls.end() && it->second.observe_only == observe_only){
        it->second.cb(cpu);
        return true;
    }
    return false;
}

static bool guest_hypercall_observer(CPUState *cpu) {
    return dispatch(cpu, true);
}

static bool guest_hypercall_mutator(CPUState *cpu) {
    return dispatch(cpu, false);
}

bool init_plugin(void *self) {
    plugin_self = self;
    panda_arg_list *plugin_args = panda_get_args(PLUGIN_NAME);
    debug = panda_parse_bool_opt(plugin_args, "debug", "Enable debug output");
    uint32_t modes = panda_callback_supported_modes(PANDA_CB_GUEST_HYPERCALL);
    observer_bridge.guest_hypercall = guest_hypercall_observer;
    mutator_bridge.guest_hypercall = guest_hypercall_mutator;
    if (!panda_register_callback_modes(self, PANDA_CB_GUEST_HYPERCALL, observer_bridge, modes, true) ||
        !panda_register_callback_modes(self, PANDA_CB_GUEST_HYPERCALL, mutator_bridge, modes, false)) {
        return false;
    }
    panda_disable_callback(self, PANDA_CB_GUEST_HYPERCALL, mutator_bridge);
    return true;
}

void uninit_plugin(void *self) {}
