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
#include <tuple>
#include "hypersyscalls.h"
#include "json.hpp"

using namespace std;

extern "C" {
    bool init_plugin(void *);
    void uninit_plugin(void *);
    void hc_syscall_enter(CPUState *cpu);
    void hc_syscall_return(CPUState *cpu);
    void hc_setup_syscall(CPUState *cpu);
    #include <hypercaller/hypercaller.h>
}

struct syscall_info {
    string name;
    uint64_t syscall_nr;
    vector<tuple<string, string>> args;
};

#ifdef DEBUG_HYPERSYSCALLS
#define log(...)
#else
#define log(...) printf(__VA_ARGS__)
#endif

unordered_map<uint64_t, struct syscall_info> registered_syscalls;
void (*hypercall_register_hypercall)(uint32_t magic, hypercall_t);

bool table_initialized = false;

struct igloo_sysret {
	long nr;
	int nargs;
	long args[6];
	// char strings[MAX_ARGS][PATH_MAX];
	long* ret;
} __packed;

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

void hc_syscall_enter(CPUState *cpu){
    struct igloo_sysret sysret;
    int reg1 = panda_get_syscall_arg(cpu, 1);
    if (panda_virtual_memory_read(cpu, reg1, (uint8_t*)&sysret, sizeof(sysret)) != MEMTX_OK){
        printf("failed to read sysret\n");
    }

    auto syscall = registered_syscalls.find(sysret.nr);
    if (syscall == registered_syscalls.end()){
        printf("syscall %ld not registered\n", sysret.nr);
        return;
    }
    printf("enter %s %ld (", syscall->second.name.c_str(), sysret.nr);
    for (int i = 0; i < syscall->second.args.size(); i++){
        printf("%s %s %ld %x, ", std::get<0>(syscall->second.args[i]).c_str(),  std::get<1>(syscall->second.args[i]).c_str(), sysret.args[i]);
    }
    printf(")\n");

}

void hc_syscall_return(CPUState *cpu){
    struct igloo_sysret sysret;
    int reg1 = panda_get_syscall_arg(cpu, 1);
    if (panda_virtual_memory_read(cpu, reg1, (uint8_t*)&sysret, sizeof(sysret)) != MEMTX_OK){
        printf("failed to read sysret\n");
    }

    auto syscall = registered_syscalls.find(sysret.nr);
    if (syscall == registered_syscalls.end()){
        printf("syscall %ld not registered\n", sysret.nr);
        return;
    }
    printf("ret %s %ld (", syscall->second.name.c_str(), sysret.nr);
    for (int i = 0; i < syscall->second.args.size(); i++){
        printf("%s %s %ld %x, ", std::get<0>(syscall->second.args[i]).c_str(),  std::get<1>(syscall->second.args[i]).c_str(), sysret.args[i]);
    }
    printf(")\n");
}

void hc_setup_syscall(CPUState *cpu){
    int reg1 = panda_get_syscall_arg(cpu, 1);

    if (table_initialized){
        log("table already initialized\n");
        return;
    }

    if (reg1 == 0){
        log("table initialized\n");
        table_initialized = true;
        return;
    }
    string buf = read_str(cpu, reg1);
    auto j = nlohmann::json::parse(buf);
    struct syscall_info sysinfo;
    sysinfo.name = j["name"];
    sysinfo.syscall_nr = j["syscall_nr"];
    log("%s %ld (", sysinfo.name.c_str(), sysinfo.syscall_nr);
    for (auto& arg : j["args"]){
        sysinfo.args.push_back(std::make_tuple(arg[0], arg[1]));
        log("%s %s, ", arg[0].get<string>().c_str(), arg[1].get<string>().c_str());
    }
    log(")\n");
    registered_syscalls[sysinfo.syscall_nr] = sysinfo;
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
