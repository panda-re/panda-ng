#ifndef __LIBINTRO_LINTROSPECTION_H
#define __LIBINTRO_LINTROSPECTION_H

#include <iohal/memory/virtual_memory.h>
#include <memory>
#include <offset/i_t.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Some Useful Structs that will be used throughout Lintrospection
 *
 */
struct LinuxKernelDetails {
    uint8_t pointer_width;
    uint64_t initial_task_addr;
    uint64_t initial_task_asid;
};

struct LinuxKernelOSI {
    struct StructureTypeLibrary* kernel_tlib;
    struct PhysicalMemory* pmem;
    std::shared_ptr<VirtualMemory> system_vmem;
    struct LinuxKernelDetails* details;
};

bool initialize_linux_kernel_osi(struct LinuxKernelOSI*, uint64_t, bool);

/*
 *  PROCESS INTROSPECTION
 *
 *  init_process_osi : begin introspection into a process, should be passed kernel
 * introspection info, an empty process introspection object and the address of a task
 * struct -- should call before introspecting into the process, its threads, and internals
 *  uninit_process_osi : calls reset on the virtual memory
 *
 */
struct LinuxProcessOSI {
    struct StructureTypeLibrary* tlib;
    std::shared_ptr<VirtualMemory> vmem;
    vm_addr_t process_address;
    bool kernel_proc;
};

bool init_process_osi(struct LinuxKernelOSI*, struct LinuxProcessOSI*, uint64_t,
                      uint64_t);
void uninit_process_osi(struct LinuxProcessOSI*);

/**
 * Utilities
 *
 */
void get_dentry_path(osi::i_t dentry, char* result);

/*
 *  PROCESSES
 *
 */
struct LinuxTask;

struct LinuxTask* create_task_from_memory(osi::i_t);
uint64_t get_current_thread_pid(struct LinuxKernelOSI*, uint64_t, uint64_t);
uint64_t get_current_thread_address(struct LinuxKernelOSI*, uint64_t);

struct LinuxTask* create_process(struct LinuxProcessOSI*);
struct LinuxTask* create_thread(struct LinuxProcessOSI*, uint64_t);

uint64_t task_get_address(const struct LinuxTask*);
uint64_t task_get_next_process(const struct LinuxTask*);
uint64_t task_get_next_thread(const struct LinuxTask*);
uint64_t task_get_tid(const struct LinuxTask*);
uint64_t task_get_pid(const struct LinuxTask*);
uint64_t task_get_ppid(const struct LinuxTask*);
uint64_t task_get_asid(const struct LinuxTask*);
uint64_t task_get_createtime(const struct LinuxTask*);
const char* task_get_shortname(const struct LinuxTask*);

void free_task(struct LinuxTask*);

/*
 *  MODULES
 *
 */

#define MAX_MODULE_PATH_SIZE 4096

struct LinuxMemoryRegion;

struct LinuxMemoryRegion* get_first_memory_region(struct LinuxProcessOSI* posi);
struct LinuxMemoryRegion* memory_region_next(struct LinuxProcessOSI* posi,
                                             struct LinuxMemoryRegion* mentry);

struct LinuxMemoryRegion* get_first_module_entry(struct LinuxProcessOSI*);
struct LinuxMemoryRegion* module_entry_next(struct LinuxProcessOSI*,
                                            struct LinuxMemoryRegion*);

uint64_t region_get_address(const struct LinuxMemoryRegion*);
uint64_t region_get_base_address(const struct LinuxMemoryRegion*);
uint64_t region_get_mtime(const struct LinuxMemoryRegion*);
uint64_t region_get_virtual_size(const struct LinuxMemoryRegion*);
uint64_t region_get_protections(const struct LinuxMemoryRegion* m);
uint64_t region_get_file_start(const struct LinuxMemoryRegion*);
const char* region_get_path(const struct LinuxMemoryRegion*);
const char* region_get_name(const struct LinuxMemoryRegion*);

void free_region(struct LinuxMemoryRegion*);

#endif
