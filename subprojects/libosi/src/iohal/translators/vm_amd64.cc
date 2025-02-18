#include <cstdio>
#include <cstdlib>

#include "iohal/memory/physical_memory.h"
#include "iohal/memory/virtual_memory.h"
#include "vm_amd64.h"

#define HW_ENTRY_MASK 0xffffffffff000

namespace amd64_translator
{

typedef pm_addr_t (*IndexFunction)(vm_addr_t);

//# PML4 0xFF8000000000 >> 39
//# DPTR 0X007FC0000000 >> 30
//# DIRP 0x00003FE00000 >> 21
//# TABL 0x0000001FF000 >> 12

static inline pm_addr_t pml4_index(vm_addr_t vaddr)
{
    uint64_t mask = 0xFF8000000000;
    return ((vaddr & mask) >> 39);
}

static inline pm_addr_t
page_directory_pointer_index(vm_addr_t vaddr) // index * sizeof(QUAD)
{
    uint64_t mask = 0x007FC0000000;
    return ((vaddr & mask) >> 30);
}

static inline pm_addr_t page_directory_index(vm_addr_t vaddr) // index * sizeof(QUAD)
{
    uint64_t mask = 0x00003FE00000;
    return ((vaddr & mask) >> 21);
}

static inline pm_addr_t page_table_index(vm_addr_t vaddr) // index * sizeof(QUAD)
{
    uint64_t mask = 0x0000001FF000;
    return ((vaddr & mask) >> 12);
}

static inline pm_addr_t get_byte_offset(vm_addr_t vaddr) // index * sizeof(QUAD)
{
    uint64_t byteoffsetmask = 0xFFF;
    return (vaddr & byteoffsetmask);
}

static inline bool entry_present(pm_addr_t entry) { return ((entry & 1) == 1); }

static inline pm_addr_t get_2MB_byte_offset(vm_addr_t vaddr)
{
    uint64_t byteoffsetmask = 0x1fffff;
    return (vaddr & byteoffsetmask);
}

static inline pm_addr_t get_1GB_byte_offset(vm_addr_t vaddr)
{
    uint64_t byteoffsetmask = 0x3FFFFFFF;
    return (vaddr & byteoffsetmask);
}

static inline bool is_large_page(pm_addr_t entry) { return ((entry & (1 << 7)) > 0); }

static inline bool page_table_entry_present(vm_addr_t entry, TranslateProfile profile)
{
    bool present = ((entry & 1) == 1);
    bool in_transition = ((entry & (1 << 11)) && !(entry & (1 << 10)));
    bool global = (entry & (1 << 8));

    if (profile == TPROF_GENERIC_LINUX) {
        return present || global;
    } else if (profile == TPROF_GENERIC_WINDOWS) {
        return present || in_transition;
    }
    return present;
}

static inline pm_addr_t get_entry(struct PhysicalMemory* pmem, vm_addr_t addr,
                                  IndexFunction index_func, pm_addr_t base_addr)
{
    base_addr = (base_addr & HW_ENTRY_MASK);
    auto index = index_func(addr);
    auto entry_addr = base_addr + (index * 8);
    pm_addr_t entry = 0;
    pmem->read(pmem, entry_addr, (uint8_t*)&entry, 8);
    return entry;
}

TranslateStatus translate_address(struct PhysicalMemory* pm, vm_addr_t vm_addr,
                                  pm_addr_t* pm_addr, pm_addr_t asid,
                                  TranslateProfile profile)
{
    // Read the base address of the page directory pointer table (pdpt) from cr4
    auto pml4e = get_entry(pm, vm_addr, pml4_index, asid);
    if (!entry_present(pml4e)) {
        return TSTAT_INVALID_ADDRESS; // TODO check if paged out
    }

    auto pdpt_base = pml4e & HW_ENTRY_MASK;
    auto pdpte = get_entry(pm, vm_addr, page_directory_pointer_index, pdpt_base);
    if (!entry_present(pdpte)) {
        return TSTAT_INVALID_ADDRESS; // TODO check if paged out
    }

    // large page bit set->1GB page
    if (is_large_page(pdpte)) {
        *pm_addr = (pdpte & 0xFFFFFC0000000) + get_1GB_byte_offset(vm_addr);
        return TSTAT_SUCCESS;
    }

    auto pd_base = pdpte & HW_ENTRY_MASK;
    auto pde = get_entry(pm, vm_addr, page_directory_index, pd_base);
    if (!page_table_entry_present(pde, profile)) {
        return TSTAT_INVALID_ADDRESS; // TODO check if paged out
    }

    if (is_large_page(pde)) {
        *pm_addr = (pde & 0xFFFFFFFF00000) + get_2MB_byte_offset(vm_addr);
        return TSTAT_SUCCESS;
    }

    /// Read the base address of the page table (PT) from the PDT
    auto pt_base = pde & HW_ENTRY_MASK;
    auto pte = get_entry(pm, vm_addr, page_table_index, pt_base);
    if (!page_table_entry_present(pte, profile)) {
        return TSTAT_PAGED_OUT; // TODO check if paged out
    }

    /// Read the physical page offset from the PT
    *pm_addr = (pte & HW_ENTRY_MASK) + get_byte_offset(vm_addr);
    return TSTAT_SUCCESS;
}
} // namespace amd64_translator
