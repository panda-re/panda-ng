#include <cstdio>
#include <cstdlib>

#include "iohal/memory/physical_memory.h"
#include "iohal/memory/virtual_memory.h"
#include "vm_i386.h"

namespace i386_translator
{

#define HW_PTE_MASK 0xfffff000

static inline pm_addr_t get_page_directory_index(vm_addr_t vaddr)
{
    uint64_t mask = 0x3FF;
    return (vaddr >> 22) & mask;
}

static inline pm_addr_t get_page_table_index(vm_addr_t vaddr)
{
    uint64_t mask = 0x3FF;
    return ((vaddr >> 12) & mask);
}

static inline pm_addr_t get_byte_offset(vm_addr_t vaddr) // index * sizeof(QUAD)
{
    uint64_t byteoffsetmask = 0xFFF;
    return (vaddr & byteoffsetmask);
}

static inline bool entry_present(vm_addr_t entry, TranslateProfile profile)
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

static inline bool is_large_page(pm_addr_t entry) { return ((entry & (1 << 7)) > 0); }

static inline pm_addr_t get_pde(struct PhysicalMemory* pmem, vm_addr_t addr,
                                pm_addr_t pdt_base_addr)
{
    size_t size_of_pde = 4;
    auto pde_index = get_page_directory_index(addr);
    auto pde_addr = pdt_base_addr + (pde_index * size_of_pde);

    pm_addr_t pde = 0;
    pmem->read(pmem, pde_addr, (uint8_t*)&pde, size_of_pde);
    return pde;
}

static inline pm_addr_t get_pte(struct PhysicalMemory* pmem, vm_addr_t addr,
                                pm_addr_t pt_base_addr)
{
    pt_base_addr = (pt_base_addr & HW_PTE_MASK);
    size_t size_of_pte = 4;
    auto pte_index = get_page_table_index(addr);
    auto pte_addr = pt_base_addr + (pte_index * size_of_pte);

    pm_addr_t pte = 0;
    pmem->read(pmem, pte_addr, (uint8_t*)&pte, size_of_pte);
    return pte;
}

TranslateStatus translate_address(struct PhysicalMemory* pm, vm_addr_t vm_addr,
                                  pm_addr_t* pm_addr, pm_addr_t asid,
                                  TranslateProfile profile)
{
    // Read the base address of the page directory pointer table (pdpt) from cr4
    auto pde = get_pde(pm, vm_addr, asid);
    if (!entry_present(pde, profile)) {
        if (is_large_page(pde)) {
            return TSTAT_PAGED_OUT; // TODO validate this algorithm
        }
        return TSTAT_INVALID_ADDRESS; // TODO check if paged out
    }

    // Handle large pages
    if (is_large_page(pde)) {
        *pm_addr = ((pde & 0xffc00000) | (vm_addr & ~0xffc00000));
        return TSTAT_SUCCESS;
    }

    // Read the base address of the page table (PT) from the PDT
    auto pte = get_pte(pm, vm_addr, pde);
    if (!entry_present(pte, profile)) {
        return TSTAT_PAGED_OUT; // TODO check if paged out
    }

    // Read the physical page offset from the PT
    *pm_addr = ((pte >> 12) << 12) | get_byte_offset(vm_addr);
    return TSTAT_SUCCESS;
}
} // namespace i386_translator
