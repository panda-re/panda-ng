#ifndef __LIBINTRO_WINTROSPECTION_H
#define __LIBINTRO_WINTROSPECTION_H

#include <iohal/memory/virtual_memory.h>
#include <memory>
#include <stdbool.h>
#include <stdint.h>

struct WindowsKernelDetails {
    uint8_t pointer_width;
    uint64_t kpcr;
    uint64_t kernelbase;
    uint64_t PsActiveProcessHead;
    uint64_t PsLoadedModuleList;
    uint64_t kdbg;
    uint64_t version64;
    uint64_t system_asid;
    uint64_t system_eprocess;
    uint64_t swapcontext_offset;
};

struct WindowsKernelOSI {
    struct StructureTypeLibrary* kernel_tlib;
    struct PhysicalMemory* pmem;
    std::shared_ptr<VirtualMemory> system_vmem;
    struct WindowsKernelDetails details;
};

struct WindowsProcessOSI {
    struct StructureTypeLibrary* tlib;
    struct WindowsKernelOSI* kosi;
    std::shared_ptr<VirtualMemory> vmem;
    vm_addr_t eprocess_address;
    uint64_t createtime;
    uint64_t pid;
};

struct WindowsProcess;
struct WindowsModuleEntry;
struct WindowsProcessList;
struct WindowsModuleList;
struct WindowsHandleObject;

uint64_t kosi_get_current_process_address(struct WindowsKernelOSI* kosi);
struct WindowsProcess* kosi_get_current_process(struct WindowsKernelOSI* kosi);
uint64_t kosi_get_current_tid(struct WindowsKernelOSI* kosi);

struct WindowsProcessList* get_process_list(struct WindowsKernelOSI* kosi,
                                            bool skip_head = true);
struct WindowsProcess* process_list_next(struct WindowsProcessList* plist);
struct WindowsProcess* create_process(struct WindowsKernelOSI* kosi,
                                      uint64_t eprocess_address);
struct WindowsProcess* create_process_from_asid(struct WindowsKernelOSI* kosi,
                                                uint64_t asid);
uint64_t get_pid_from_asid(struct WindowsKernelOSI* kosi, uint64_t asid);
uint64_t get_eproc_addr_from_asid(struct WindowsKernelOSI* kosi, uint64_t asid);
void free_process_list(struct WindowsProcessList* plist);

uint64_t process_get_eprocess(const struct WindowsProcess*);
const char* process_get_shortname(const struct WindowsProcess*);
const char* process_get_cmdline(const struct WindowsProcess*);
uint64_t process_get_pid(const struct WindowsProcess*);
uint64_t process_get_ppid(const struct WindowsProcess*);
uint64_t process_get_asid(const struct WindowsProcess*);
uint64_t process_get_base(const struct WindowsProcess*);
uint64_t process_createtime(const struct WindowsProcess*);
TranslateStatus process_vmem_read(struct WindowsProcessOSI*, vm_addr_t addr, void* buffer,
                                  uint64_t size);
bool process_is_wow64(const struct WindowsProcess*);
void free_process(struct WindowsProcess*);

const uint8_t MODULELIST_LOAD_ORDER = 0;
struct WindowsModuleList* get_module_list(struct WindowsKernelOSI* kosi, uint64_t address,
                                          bool iswow);
struct WindowsModuleEntry* get_module_by_addr(struct WindowsKernelOSI* kosi,
                                              uint64_t process_address, bool is_wow64,
                                              uint64_t addr);
uint64_t get_module_base_address_by_name(struct WindowsKernelOSI* kosi,
                                         uint64_t process_address, bool is_wow64,
                                         const char* name);
bool has_module_prefix(struct WindowsKernelOSI* kosi, uint64_t process_address,
                       bool is_wow64, const char* prefix);
struct WindowsModuleEntry* module_list_next(struct WindowsModuleList*);
struct WindowsProcessOSI* module_list_get_osi(struct WindowsModuleList* mlist);
void free_module_list(struct WindowsModuleList* mlist);

uint64_t module_entry_get_module_entry(struct WindowsModuleEntry*);
uint64_t module_entry_get_base_address(struct WindowsModuleEntry*);
uint32_t module_entry_get_checksum(struct WindowsModuleEntry*);
uint64_t module_entry_get_entrypoint(struct WindowsModuleEntry*);
uint32_t module_entry_get_flags(struct WindowsModuleEntry*);
uint32_t module_entry_get_timedatestamp(struct WindowsModuleEntry*);
uint16_t module_entry_get_loadcount(struct WindowsModuleEntry*);
uint32_t module_entry_get_modulesize(struct WindowsModuleEntry*);
bool module_entry_is_wow64(struct WindowsModuleEntry*);
const char* module_entry_get_dllpath(struct WindowsModuleEntry*);
const char* module_entry_get_dllname(struct WindowsModuleEntry*);
void free_module_entry(struct WindowsModuleEntry*);

struct WindowsHandleObject* resolve_handle(struct WindowsKernelOSI*, uint64_t);
void free_handle(struct WindowsHandleObject* handle);

uint64_t handle_get_pointer(struct WindowsHandleObject* handle);
uint8_t handle_get_type(struct WindowsHandleObject* handle);
const char* handle_get_typename(struct WindowsHandleObject* handle);
struct WindowsProcessOSI* handle_get_context(struct WindowsHandleObject* handle);

#endif
