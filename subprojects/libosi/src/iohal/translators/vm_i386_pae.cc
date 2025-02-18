#include <cstdio>
#include <cstdlib>

#include "iohal/memory/physical_memory.h"
#include "iohal/memory/virtual_memory.h"
#include "vm_i386_pae.h"

namespace i386_pae_translator
{

#define HW_PTE_MASK 0xFFFFFFFFFF000

static inline pm_addr_t get_page_directory_pointer_index(vm_addr_t vaddr)
{
    uint64_t pdpimask = 0xC0000000;
    return ((vaddr & pdpimask) >> 30);
}

static inline pm_addr_t get_page_directory_index(vm_addr_t vaddr) // index * sizeof(QUAD)
{
    uint64_t pdimask = 0x3FE00000;
    return ((vaddr & pdimask) >> 21);
}

static inline pm_addr_t get_page_table_index(vm_addr_t vaddr) // index * sizeof(QUAD)
{
    uint64_t ptimask = 0x1FF000;
    return ((vaddr & ptimask) >> 12);
}

static inline pm_addr_t get_byte_offset(vm_addr_t vaddr) // index * sizeof(QUAD)
{
    uint64_t byteoffsetmask = 0xFFF;
    return (vaddr & byteoffsetmask);
}

static inline pm_addr_t get_2MB_byte_offset(vm_addr_t vaddr)
{
    uint64_t byteoffsetmask = 0x1FFFFF;
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

static inline pm_addr_t get_pdpe(PhysicalMemory* pmem, vm_addr_t addr,
                                 pm_addr_t pdpt_base_addr)
{
    int size_of_pdpe = 8;
    auto pdpe_index = get_page_directory_pointer_index(addr);
    auto pdpe_addr = pdpt_base_addr + (pdpe_index * size_of_pdpe);
    pm_addr_t pdpe_entry = 0;
    pmem->read(pmem, pdpe_addr, (uint8_t*)&pdpe_entry, size_of_pdpe);
    return pdpe_entry;
}

static inline pm_addr_t get_pde(struct PhysicalMemory* pmem, vm_addr_t addr,
                                pm_addr_t pdt_base_addr)
{
    size_t size_of_pde = 8;
    auto pde_index = get_page_directory_index(addr);
    auto pde_addr = pdt_base_addr + (pde_index * size_of_pde);

    pm_addr_t pde = 0;
    pmem->read(pmem, pde_addr, (uint8_t*)&pde, size_of_pde);
    return pde;
}

static inline pm_addr_t get_pte(struct PhysicalMemory* pmem, vm_addr_t addr,
                                pm_addr_t pt_base_addr)
{
    size_t size_of_pte = 8;
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
    uint64_t cr3_pfn = asid & 0xFFFFFFE0;
    auto pdp_entry = get_pdpe(pm, vm_addr, cr3_pfn);
    if (!entry_present(pdp_entry, profile)) {
        return TSTAT_INVALID_ADDRESS;
    }

    uint64_t pdp_pfn = pdp_entry & 0xFFFFFFFFFF000;
    auto pde = get_pde(pm, vm_addr, pdp_pfn);
    if (!entry_present(pde, profile)) {
        return TSTAT_INVALID_ADDRESS; // TODO check if paged out
    }

    // Handle large pages
    if (is_large_page(pde)) {
        *pm_addr = (pde & 0xFFFFFFFE00000) + get_2MB_byte_offset(vm_addr);
        return TSTAT_SUCCESS;
    }

    uint64_t pd_pfn = pde & HW_PTE_MASK;

    // Read the base address of the page table (PT) from the PDT
    auto pte = get_pte(pm, vm_addr, pd_pfn);
    if (!entry_present(pte, profile)) {
        return TSTAT_PAGED_OUT; // TODO check if paged out
    }

    // Read the physical page offset from the PT
    *pm_addr = (pte & HW_PTE_MASK) + get_byte_offset(vm_addr);
    return TSTAT_SUCCESS;
}
} // namespace i386_pae_translator
