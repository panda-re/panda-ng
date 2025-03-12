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
#include <map>
#include <hypercaller/hypercaller.h>
#include "syscalls_hc.h"

hypercall_t* register_hypercall;

extern "C" {
    bool init_plugin(void *);
    void uninit_plugin(void *);
}

map<uint64_t, struct syscall_info> sysinfo;

void initialize_syscalls(struct syscall_info **info, int length){
    for (int i=0; i<length; i++){
        sysinfo[info[i]->syscall_nr] = info[i];
    }
}

/* ### Plugin bootstrapping ############################################# */
bool init_plugin(void *self) {
    void *hypercaller = panda_get_plugin_by_name("hypercaller");
    if (hypercaller == NULL){
      panda_require("hypercaller");
      hypercaller = panda_get_plugin_by_name("hypercaller");
    }
    register_hypercall = (hypercall_t*)dlsym(hypercaller, "register_hypercall");
    return true;
}

void uninit_plugin(void *self) {}

/* vim:set tabstop=4 softtabstop=4 expandtab: */
