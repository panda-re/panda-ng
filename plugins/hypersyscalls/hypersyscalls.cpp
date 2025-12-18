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
#include <set>
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

bool debug = false;

#define log(...) if (debug) {printf(__VA_ARGS__);}

// syscall_nr -> syscall_info
unordered_map<uint64_t, struct syscall_prototype> syscall_info_table;

// store the callbacks that are registered before the table is initialized
vector<struct syscall_hook> preregistered_callbacks;

atomic<ID> id_counter{0};
unordered_map<ID, struct syscall_hook> syscall_hooks;

vector<ID> syscall_all_enter_cbs;
vector<ID> syscall_all_return_cbs;

vector<ID> syscall_unknown_enter_cbs;
vector<ID> syscall_unknown_return_cbs;

unordered_map<uint64_t, vector<ID>> syscall_enter_cbs;
unordered_map<uint64_t, vector<ID>> syscall_return_cbs;

set<uint64_t> verified_unknown_syscalls;

void (*hypercall_register_hypercall)(uint32_t magic, hypercall_t);
bool table_initialized = false;
target_ulong comm_offset = 0;

vector<struct syscall_prototype> missing_syscalls;

static string read_str(CPUState* cpu, target_ulong ptr){
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

static bool do_register_syscall(struct syscall_hook *cb){
    if (cb->on_unknown && cb->on_all){
        printf("Cannot set both on_unknown and on_all\n");
        return false;
    }

    if (cb->on_unknown && strlen(cb->name) > 0){
        printf("Cannot set both on_unknown and name\n");
        return false;
    }
    
    bool any_set = false;
    cb->enabled = true;
    id_counter++;
    int id = id_counter;
    syscall_hooks[id] = *cb;

    if (cb->on_unknown){
        if (cb->on_enter){
            syscall_unknown_enter_cbs.push_back(id);
            any_set = true;
        }
        if (cb->on_return){
            syscall_unknown_return_cbs.push_back(id);
            any_set = true;
        }
    } else if (cb->on_all){
        if (cb->on_enter){
            syscall_all_enter_cbs.push_back(id);
            any_set = true;
        }
        if (cb->on_return){
            syscall_all_return_cbs.push_back(id);
            any_set = true;
        }
    } else {
        for (auto &syscall : syscall_info_table){
            if (strcmp(syscall.second.name, cb->name) == 0){
                if (cb->on_enter){
                    syscall_enter_cbs[syscall.first].push_back(id);
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
        log("No syscall found with name %s\n", cb->name);
        return false;
    }
    return true;
}

void register_syscall_cb(struct syscall_hook *cb){
    if (table_initialized){
        do_register_syscall(cb);
    }else{
        preregistered_callbacks.push_back(*cb);
    }
}

static void log_syscall(struct syscall_prototype *prototype, struct syscall *sysret, bool is_enter){
    if (is_enter){
        log("enter %s %ld (", prototype->name, sysret->nr);
    }else{
        log("return %s %ld (",  prototype->name, sysret->nr);
    }
    for (int i = 0; i < prototype->nargs; i++){
        log("%s %s %" PRIx64 " , ",
                    prototype->types[i],  prototype->names[i], sysret->args[i]);
    }
    log(")");
    if (is_enter){
        log("\n");
    }else{
        log("= %" PRIx64 "\n", (target_long)sysret->retval);
    }
}

static void loop_run_cbs(CPUState *cpu, vector<ID> &cbs, struct syscall_prototype* syscall_proto, 
            struct syscall *sysret, bool is_enter){
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
        if (is_enter && !hook.on_enter){
            continue;
        }
        if (!is_enter && !hook.on_return){
            continue;
        }

        if (hook.filter_args_enabled){
            bool skip = false;
            for (int i = 0; i < syscall_proto->nargs; i++){
                if (hook.filter_arg[i]){
                    if (sysret->args[i] != hook.arg_filter[i]){
                        log("arg filter failed for arg %d\n", i);
                        skip = true;
                        break;
                    }
                }
            }
            if (skip){
                log("skipping syscall\n");
                continue;
            }
        }

        if (hook.comm_filter_enabled){
            char comm[16];
            if (comm_offset == 0){
                log("comm offset not set\n");
                continue;
            }
            panda_virtual_memory_read(cpu, sysret->task + comm_offset, (uint8_t*)comm, sizeof(comm));
            if (strncmp(comm, hook.comm_filter, sizeof(comm)) != 0){
                log("comm filter failed expected %s got %s\n", hook.comm_filter, comm);
                continue;
            }
        }

        log("running cb\n");
        hook.cb(cpu, syscall_proto, sysret, &hook);
    }
}

/**
 * When a new syscall is registered after the fact we need to iterate over 
 * hooks to see if any of them match the new syscall.
 */
void apply_new_syscall_late_register(int nr){
    for (const auto &pair: syscall_hooks){
        auto id = pair.first;
        auto &hook = pair.second;
        if (hook.on_unknown || hook.on_all){
            continue;
        }
        if (strcmp(hook.name, syscall_info_table[nr].name) == 0){
            if (hook.on_enter){
                syscall_enter_cbs[nr].push_back(id);
            }
            if (hook.on_return){
                syscall_return_cbs[nr].push_back(id);
            }
        }
    }
}

/**
 * Sometimes we don't get a syscall number (-1) associated with a syscall
 * 
 * We use the syscall name to find the syscall number and register it
 * 
 * If this matches we apply new syscalls.
 */
bool try_register(CPUState *cpu, struct syscall *sysret){
    auto it = verified_unknown_syscalls.find(sysret->nr);
    if (it != verified_unknown_syscalls.end()){
        log("syscall %ld already known to not be in set\n", sysret->nr);
        return false;
    }
    uint64_t name_ptr = sysret->name_ptr;
    string name = read_str(cpu, name_ptr);
    log("try_register name: %s\n", name.c_str());
    for (struct syscall_prototype &sysinfo : missing_syscalls){
        // compare "sys_name"(+4) with "name"

        const char *name_ptr = name.c_str();
        while (*name_ptr == '_'){
            name_ptr++;
        }
        if (strcmp(sysinfo.name+4, name_ptr) == 0){
            sysinfo.syscall_nr = sysret->nr;
            syscall_info_table[sysret->nr] = sysinfo;
            log("registered syscall after the fact %s %ld\n", sysinfo.name, sysinfo.syscall_nr);
            apply_new_syscall_late_register(sysret->nr);
            return true;
        }
    }
    log("syscall %ld not registered and will not be registered\n", sysret->nr);
    verified_unknown_syscalls.insert(sysret->nr);
    return false;
}

void hc_syscall(CPUState *cpu, bool on_enter){
    if (!table_initialized){
        printf("table not initialized\n");
        return;
    }
    uint64_t reg1 = panda_get_syscall_arg(cpu, 1);
    
    struct syscall sysret;
    log("reg1: %llx\n", reg1);
    if (panda_virtual_memory_read(cpu, reg1, (uint8_t*)&sysret, sizeof(struct syscall)) != MEMTX_OK){
        printf("failed to read sysret\n");
        panda_set_retval(cpu, (target_ulong) 0);
        return;
    }
    if (sysret.known_magic != SYSCALL_HC_KNOWN_MAGIC){
        log("FATAL: struct syscall out of sync with plugin!!\n");
        return;
    }
    log("syscall %ld\n", sysret.nr);
    auto syscall_proto_search = syscall_info_table.find(sysret.nr);
    if (syscall_proto_search == syscall_info_table.end()){
        if (!try_register(cpu, &sysret)){
            log("syscall %ld not registered\n", sysret.nr);
            loop_run_cbs(cpu, on_enter ? syscall_unknown_enter_cbs : syscall_unknown_return_cbs,
                        nullptr, &sysret, on_enter);
            return;
        }else{
            syscall_proto_search = syscall_info_table.find(sysret.nr);
        }
    }
    auto syscall_proto = syscall_proto_search->second;

    log_syscall(&syscall_proto, &sysret, on_enter);

    struct syscall clean_copy;
    memcpy(&clean_copy, &sysret, sizeof(struct syscall));
    
    loop_run_cbs(cpu, on_enter ? syscall_all_enter_cbs: syscall_all_return_cbs, 
                &syscall_proto, &sysret, on_enter);

    auto cb_map = on_enter ? syscall_enter_cbs : syscall_return_cbs;
    auto cbs = cb_map.find(sysret.nr);
    if (cbs != cb_map.end()){
        loop_run_cbs(cpu, cbs->second, &syscall_proto, &sysret, on_enter);
    }

    if (memcmp(&clean_copy, &sysret, sizeof(struct syscall)) != 0){
        log("syscall modified\n");
        panda_virtual_memory_write(cpu, reg1, (uint8_t *)&sysret, sizeof(struct syscall));
    }
    panda_set_retval(cpu, 0);
}

/**
 * This function initializes the offset to comm
 */
void hc_setup_task_comm(CPUState *cpu){
    comm_offset = panda_get_syscall_arg(cpu, 1);
}

/**
 * This function is called when a syscall is entered
 */
void hc_syscall_enter(CPUState *cpu){
    hc_syscall(cpu, true);
}

/**
 * This function is called when a syscall is returning
 */
void hc_syscall_return(CPUState *cpu){
    hc_syscall(cpu, false);
}

/**
 * This function receives a definition of a syscall in JSON format from the guest via hypercall
 */
void hc_setup_syscall(CPUState *cpu){
    target_ulong reg1 = panda_get_syscall_arg(cpu, 1);

    log("reg1: %llx\n", reg1);

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
    log("reg1: %llx\n", reg1);
    string buf = read_str(cpu, reg1);
    log("buf: %s\n", buf.c_str());
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
    if (sysinfo.syscall_nr == -1){
        log("syscall %s not registered, but saving for later\n", sysinfo.name);
        missing_syscalls.push_back(sysinfo);
    }else{
        syscall_info_table[sysinfo.syscall_nr] = sysinfo;
    }
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
    hypercall_register_hypercall(IGLOO_HYP_SETUP_TASK_COMM, hc_setup_task_comm);
    panda_arg_list *plugin_args = panda_get_args(PLUGIN_NAME);
    debug = panda_parse_bool_opt(plugin_args, "debug", "Enable debug output");
    return true;
}

void uninit_plugin(void *self) {}

/* vim:set tabstop=4 softtabstop=4 expandtab: */
