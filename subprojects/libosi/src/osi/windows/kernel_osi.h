#ifndef __OSI_KERNEL
#define __OSI_KERNEL

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <libgen.h>
#include <map>
#include <stdint.h>

#include "osi/windows/iterator.h"
#include "windows_static_offsets.h"
#include <iohal/memory/virtual_memory.h>
#include <offset/i_t.h>
#include <offset/offset.h>

bool find_kernel_base(VirtualMemory* vmem, vm_addr_t kpcr, vm_addr_t* base);
bool find_kernel_base_i386(VirtualMemory* vmem, vm_addr_t kpcr, vm_addr_t* base);
bool find_kernel_base_amd64(VirtualMemory* vmem, vm_addr_t kpcr, vm_addr_t* base);
bool scan_for_kdbg(VirtualMemory* vmem, vm_addr_t kernel_base, vm_addr_t* kdbg);
bool scan_for_version64(VirtualMemory* vmem, vm_addr_t kdbg, vm_addr_t kernel_base,
                        vm_addr_t* version64);
vm_addr_t get_address_active_process_head(VirtualMemory* vmem, vm_addr_t kdbg);
vm_addr_t get_address_loaded_module_head(VirtualMemory* vmem, vm_addr_t kdbg);

#endif // __OSI_KERNEL
