/* PANDABEGINCOMMENT
 *
 * Authors:
 *  Luke Craig             luke.craig@ll.mit.edu
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 * See the COPYING file in the top-level directory.
 *
PANDAENDCOMMENT */

#include "panda.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <atomic>
#include "json.hpp"

using namespace std;

extern "C" {
    #include "hypersyscalls.h"
    bool init_plugin(void *);
    void uninit_plugin(void *);
    void hc_syscall_enter(CPUState *cpu);
    void hc_syscall_return(CPUState *cpu);
    void hc_setup_syscall(CPUState *cpu);
    #include <hypercaller/hypercaller.h>
}
#ifdef DEBUG_HYPERSYSCALLS
#define log(...) printf(__VA_ARGS__)
#else
#define log(...)
#endif

// syscall_nr -> syscall_info
unordered_map<uint64_t, struct syscall_prototype> syscall_info_table;

// store the callbacks that are registered before the table is initialized
vector<struct syscall_hook> preregistered_callbacks;

atomic<ID> id_counter = 0;
unordered_map<ID, struct syscall_hook> syscall_hooks;

vector<ID> syscall_all_enter_cbs;
vector<ID> syscall_all_return_cbs;

unordered_map<uint64_t, vector<ID>> syscall_enter_cbs;
unordered_map<uint64_t, vector<ID>> syscall_return_cbs;

void (*hypercall_register_hypercall)(uint32_t magic, hypercall_t);
bool table_initialized = false;


string read_str(CPUState* cpu, target_ulong ptr){
    string buf = "";
    char tmp;
    while (true){
        if (panda_virtual_memory_read(cpu, ptr, (uint8_t*)&tmp,1) == MEMTX_OK){
            buf += tmp;
            if (tmp == '\x00'){
                break;
            }
            ptr+=1;
        }else{
            break;
        }
    }
    return buf;
}

void do_register_syscall(struct syscall_hook *cb){
    bool any_set = false;
    cb->enabled = true;
    int id;
    syscall_hooks[id=++id_counter] = *cb;
    if (cb->on_all){
        if (cb->on_enter){
            syscall_all_enter_cbs.push_back(id);
            any_set = true;
        }
        if (cb->on_return){
            syscall_all_return_cbs.push_back(id);
            any_set = true;
        }
    }else {
        for (auto &syscall : syscall_info_table){
            if (strcmp(syscall.second.name, cb->name) == 0){
                if (cb->on_enter){
                    syscall_return_cbs[syscall.first].push_back(id);
                    any_set = true;
                }
                if (cb->on_return){
                    syscall_return_cbs[syscall.first].push_back(id);
                    any_set = true;
                }
            }
        }
    }
    if (!any_set){
        printf("No syscall found with name %s\n", cb->name);
    }
}

void register_syscall_cb(struct syscall_hook *cb){
    if (table_initialized){
        do_register_syscall(cb);
    }else{
        preregistered_callbacks.push_back(*cb);
    }
}

void log_syscall(struct syscall_prototype *prototype, struct syscall *sysret){
    log("%s %ld (", prototype->name, sysret->nr);
    for (int i = 0; i < prototype->nargs; i++){
        log("%s %s %" PRIx64 " , ", prototype->types[i],  prototype->names[i], sysret->args[i]);
    }
    log(")\n");
}

void loop_run_cbs(CPUState *cpu, vector<ID> &cbs, struct syscall_prototype* syscall_proto, 
            struct syscall *sysret){
    for (auto id : cbs){
        auto h = syscall_hooks.find(id);
        if (h == syscall_hooks.end()){
            log("hook not found\n");
            continue;
        }
        log("loop_run_cbs\n");
        auto &hook = h->second;
        if (!hook.enabled){
            log("hook disabled\n");
            continue;
        }
        log("running cb\n");
        hook.cb(cpu, syscall_proto, sysret, &hook);
    }
}

void hc_syscall(CPUState *cpu, unordered_map<uint64_t, vector<ID>> &cb_map, vector<ID> &all_cbs){
    if (!table_initialized){
        printf("table not initialized\n");
        return;
    }
    int reg1 = panda_get_syscall_arg(cpu, 1);
    
    struct syscall sysret;
    log("reg1: %x\n", reg1);
    if (panda_virtual_memory_read(cpu, reg1, (uint8_t*)&sysret, sizeof(struct syscall)) != MEMTX_OK){
        printf("failed to read sysret\n");
        panda_set_retval(cpu, (target_ulong) 0);
        return;
    }
    if (sysret.known_magic != SYSCALL_HC_KNOWN_MAGIC){
        log("FATAL: struct syscall out of sync with plugin!!\n");
        return;
    }

    auto syscall_proto_search = syscall_info_table.find(sysret.nr);
    if (syscall_proto_search == syscall_info_table.end()){
        log("syscall %ld not registered\n", sysret.nr);
        return;
    }
    auto syscall_proto = syscall_proto_search->second;

    log_syscall(&syscall_proto, &sysret);

    struct syscall clean_copy;
    memcpy(&clean_copy, &sysret, sizeof(struct syscall));
    
    loop_run_cbs(cpu, all_cbs, &syscall_proto, &sysret);

    auto cbs = cb_map.find(sysret.nr);
    if (cbs != cb_map.end()){
        loop_run_cbs(cpu, cbs->second, &syscall_proto, &sysret);
    }

    if (memcmp(&clean_copy, &sysret, sizeof(struct syscall)) != 0){
        log("syscall modified\n");
        panda_virtual_memory_write(cpu, reg1, (uint8_t*)&sysret, sizeof(struct syscall));
    }
    panda_set_retval(cpu, 0);
}

/**
 * This function is called when a syscall is entered
 */
void hc_syscall_enter(CPUState *cpu){
    hc_syscall(cpu, syscall_enter_cbs, syscall_all_enter_cbs);
}

/**
 * This function is called when a syscall is returning
 */
void hc_syscall_return(CPUState *cpu){
    hc_syscall(cpu, syscall_return_cbs, syscall_all_return_cbs);
}

/**
 * This function receives a definition of a syscall in JSON format from the guest via hypercall
 */
void hc_setup_syscall(CPUState *cpu){
    int reg1 = panda_get_syscall_arg(cpu, 1);

    if (table_initialized){
        log("table already initialized\n");
        return;
    }

    if (reg1 == 0){
        log("table initialized\n");
        table_initialized = true;
        for (auto &cb : preregistered_callbacks){
            do_register_syscall(&cb);
        }
        return;
    }
    string buf = read_str(cpu, reg1);
    auto j = nlohmann::json::parse(buf);
    struct syscall_prototype sysinfo;
    sysinfo.name = strdup(j["name"].get<string>().c_str());
    sysinfo.syscall_nr = j["syscall_nr"];
    log("%s %ld (", sysinfo.name, sysinfo.syscall_nr);
    int i = 0;
    for (auto& arg : j["args"]){
        sysinfo.types[i] = strdup(arg[0].get<string>().c_str());
        sysinfo.names[i] = strdup(arg[1].get<string>().c_str());
        i++;
    }
    sysinfo.nargs = i;
    log(")\n");
    syscall_info_table[sysinfo.syscall_nr] = sysinfo;
}

bool init_plugin(void *self) {
    void *hypercaller = panda_get_plugin_by_name("hypercaller");
    if (hypercaller == NULL){
      panda_require("hypercaller");
      hypercaller = panda_get_plugin_by_name("hypercaller");
    }
    hypercall_register_hypercall = (void (*)(uint32_t, hypercall_t)) dlsym(hypercaller, "register_hypercall");
    hypercall_register_hypercall(IGLOO_HYP_SETUP_SYSCALL, hc_setup_syscall);
    hypercall_register_hypercall(IGLOO_HYP_SYSCALL_ENTER, hc_syscall_enter);
    hypercall_register_hypercall(IGLOO_HYP_SYSCALL_RETURN, hc_syscall_return);
    return true;
}

void uninit_plugin(void *self) {}

/* vim:set tabstop=4 softtabstop=4 expandtab: */
