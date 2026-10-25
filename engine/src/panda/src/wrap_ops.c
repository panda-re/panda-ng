#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

// #include "panda/panda_api.h"
// #include "panda/common.h"
// #include "sysemu/sysemu.h"

// // for map_memory
// #include "exec/address-spaces.h"
// #include "exec/memory.h"
// #include "qapi/error.h"
// #include "migration/vmstate.h"

// // for panda_{set/get}_library_mode
// #include "qemu/osdep.h"
// #include "sysemu/sysemu.h"
// #include "sysemu/runstate.h"
#include "panda/plugin.h"
#include "panda/callbacks/cb-support.h"
#include "panda/common.h"
#include "panda/wrap_ops.h"
// #include "accel/tcg/cpu-ops.h"


// TODO: needs a lookup per cpu

// bool (*wrapped_cpu_exec_interrupt)(CPUState *cpu, int interrupt_request) = NULL;

// static bool wrap_cpu_exec_interrupt(CPUState * cpu, int interrupt_request){
//     interrupt_request = panda_callbacks_before_handle_interrupt(cpu, interrupt_request);
//     if (wrapped_cpu_exec_interrupt){
//         return wrapped_cpu_exec_interrupt(cpu, interrupt_request);
//     }
//     return false;
// }

// void (*wrapped_cpu_exec_enter)(CPUState *cpu) = NULL;

// static void wrap_cpu_exec_enter(CPUState *cpu){
//     if (wrapped_cpu_exec_enter){
//         wrapped_cpu_exec_enter(cpu);
//     }
//     panda_callbacks_after_cpu_exec_enter(cpu);
// }

// void (*wrapped_cpu_exec_exit)(CPUState *cpu) = NULL;

// static void wrap_cpu_exec_exit(CPUState *cpu){
//     // We haven't implemented the ranBlocks feature yet
//     panda_callbacks_before_cpu_exec_exit(cpu, true);
//     if (wrapped_cpu_exec_exit){
//         wrapped_cpu_exec_exit(cpu);
//     }
// }

void wrap_cpu_ops(void){
    // CPUState *cpu;

    // CPU_FOREACH(cpu) {
    //     TCGCPUOps *tcg_ops = (TCGCPUOps*)cpu->cc->tcg_ops;
    //     TCGCPUOps *copy = malloc(sizeof(TCGCPUOps));
    //     memcpy(copy, tcg_ops, sizeof(TCGCPUOps));

    //     if (tcg_ops == NULL){
    //         printf("tcg_ops is NULL\n");
    //         return;
    //     }

    //     wrapped_cpu_exec_interrupt = copy->cpu_exec_interrupt;
    //     copy->cpu_exec_interrupt = wrap_cpu_exec_interrupt;

    //     wrapped_cpu_exec_enter = copy->cpu_exec_enter;
    //     copy->cpu_exec_enter = wrap_cpu_exec_enter;

    //     wrapped_cpu_exec_exit = copy->cpu_exec_exit;
    //     copy->cpu_exec_exit = wrap_cpu_exec_exit;
    //     cpu->cc->tcg_ops = copy;
    // }
}