#pragma once

#include "kernel_profile.h"

#define MAX_CPUS 128
#define IGLOO_HYP_OSI_TASK_SWITCH 0x3337

target_ptr_t hypercall_get_current_task_struct(CPUState *cpu);
target_ptr_t hypercall_get_task_struct_next(CPUState *cpu, target_ptr_t ts);
target_ptr_t hypercall_get_group_leader(CPUState *cpu, target_ptr_t ts);
target_ptr_t hypercall_get_file_fds(CPUState *cpu, target_ptr_t files);
bool can_read_current(CPUState *env);
void on_first_syscall(CPUState *cpu, target_ulong pc, target_ulong callno);

const KernelProfile HYPERCALL_PROFILE = {
    .get_current_task_struct = &hypercall_get_current_task_struct,
    .get_task_struct_next = &hypercall_get_task_struct_next,
    .get_group_leader = &hypercall_get_group_leader,
    .get_files_fds = &hypercall_get_file_fds
};

void hc_setup_osi_task_switch(CPUState *cpu);

/* vim:set tabstop=4 softtabstop=4 expandtab: */
