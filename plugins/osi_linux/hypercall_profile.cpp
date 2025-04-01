#include "panda.h"
#include "utils/kernelinfo/kernelinfo.h"
#include "hypercall_profile.h"
#include "osi_linux.h"

extern bool osi_initialized;
extern kernelinfo ki;
target_ptr_t current_task_addr[MAX_CPUS];

/**
 * @brief Retrieves the task_struct address using per cpu information.
 */
target_ptr_t hypercall_get_current_task_struct(CPUState *cpu){
    target_ptr_t cpu_task = current_task_addr[cpu->cpu_index];
    if (cpu_task == 0){
        OG_printf("cpu_task is 0\n");
        return 0;
    }
    return cpu_task;
}

/**
 * @brief Retrieves the address of the following task_struct in the process list.
 */
target_ptr_t hypercall_get_task_struct_next(CPUState *cpu, target_ptr_t task_struct){
    struct_get_ret_t err;
    target_ptr_t tasks;
    err = struct_get(cpu, &tasks, task_struct, ki.task.tasks_offset);
    fixupendian2(tasks);
    // assert(err == struct_get_ret_t::SUCCESS && "failed to get next task");
    if (err != struct_get_ret_t::SUCCESS)
        printf("failed to get next task\n");
    return tasks - ki.task.tasks_offset;
}

/**
 * @brief Retrieves the thread group leader address from task_struct.
 */
target_ptr_t hypercall_get_group_leader(CPUState *cpu, target_ptr_t ts){
    struct_get_ret_t err;
    target_ptr_t group_leader;
    OG_printf("Getting group leader from task_struct at " TARGET_FMT_lx " with offset " TARGET_FMT_lx "\n", ts, (target_ulong)ki.task.group_leader_offset);
    err = struct_get(cpu, &group_leader, ts, ki.task.group_leader_offset);
    fixupendian2(group_leader);
    // assert(err == struct_get_ret_t::SUCCESS && "failed to get group leader for task");
    if (err != struct_get_ret_t::SUCCESS)
        printf("failed to get group leader for task\n");
    return group_leader;
}

/**
 * @brief Retrieves the array of file structs from the files struct.
 * The n-th element of the array corresponds to the n-th open fd.
 */
target_ptr_t hypercall_get_file_fds(CPUState *cpu, target_ptr_t files){
    struct_get_ret_t err;
    target_ptr_t files_fds;
    err = struct_get(cpu, &files_fds, files, {ki.fs.fdt_offset, ki.fs.fd_offset});
    if (err != struct_get_ret_t::SUCCESS) {
        LOG_ERROR("Failed to retrieve file structs (error code: %d)", err);
        return (target_ptr_t)NULL;
    }
    return files_fds;
}

void hc_setup_osi_task_switch(CPUState *cpu){
    // target_ulong prev = panda_get_syscall_arg(cpu, 1);
    target_ulong next = panda_get_syscall_arg(cpu, 2);
    osi_initialized = true;

    if (next == 0){
        OG_printf("next is 0\n");
        return;
    }
    
    // printf("hc_setup_osi_task_switch %d %llx\n", cpu->cpu_index, (long long unsigned int)next);

    current_task_addr[cpu->cpu_index] = next;
}

/* vim:set tabstop=4 softtabstop=4 expandtab: */