
#include <cstdio>
#include <cstdlib>

#include "iohal/memory/common.h"
#include "iohal/memory/physical_memory.h"
#include "iohal/memory/virtual_memory.h"

VirtualMemory::VirtualMemory(struct PhysicalMemory* pmem, uint8_t bits, pm_addr_t asid,
                             bool pae, const char* profile)
{
    m_pmem = pmem;
    m_vmtrans =
        std::make_unique<VirtualMemoryTranslator>(m_pmem, bits, asid, pae, profile);
    m_pointer_width = (bits == 32) ? 4 : 8;
    m_pae = pae;
}

TranslateStatus VirtualMemory::read(vm_addr_t addr, void* buffer, uint64_t size)
{
    pm_addr_t paddr = 0;

    // TODO make this more performant. the current setup is to make sure we can read
    // across page sizes
    while (size > 0) {
        auto tstatus = m_vmtrans->translate(addr, &paddr, 0, m_pae);
        if (!(tstatus == TSTAT_SUCCESS)) {
            return tstatus;
        }

        size_t to_page_end = 4096 - (paddr % 4096);
        size_t to_read = (to_page_end < size) ? to_page_end : size;
        if (!(m_pmem->read(m_pmem, paddr, (uint8_t*)buffer, to_read))) {
            return TSTAT_GENERIC_FAILURE;
        }
        addr += to_read;
        buffer = (void*)(((uintptr_t)buffer) + to_read);
        size -= to_read;
    }
    return TSTAT_SUCCESS;
}

TranslateStatus VirtualMemory::read_pointer(vm_addr_t addr, vm_addr_t* buffer)
{
    pm_addr_t paddr = 0;
    auto tstatus = m_vmtrans->translate(addr, &paddr, 0, m_pae);
    if (tstatus != TSTAT_SUCCESS) {
        return tstatus;
    }

    if (m_pointer_width == 4) {
        uint32_t pointer = 0;
        if (m_pmem->read(m_pmem, paddr, (uint8_t*)&pointer, 4)) {
            *buffer = pointer;
            return TSTAT_SUCCESS;
        }
    } else if (m_pointer_width == 8) {
        uint64_t pointer = 0;
        if (m_pmem->read(m_pmem, paddr, (uint8_t*)&pointer, 8)) {
            *buffer = pointer;
            return TSTAT_SUCCESS;
        }
    }

    return TSTAT_GENERIC_FAILURE;
}

uint8_t VirtualMemory::get_pointer_width() { return m_pointer_width; }

uint8_t VirtualMemory::get_bits() { return 8 * m_pointer_width; }

uint64_t VirtualMemory::set_asid(uint64_t new_asid)
{
    return m_vmtrans->set_asid(new_asid);
}

uint64_t VirtualMemory::get_asid() { return m_vmtrans->get_asid(); }

VirtualMemory::VirtualMemory(const VirtualMemory& other)
{
    m_pointer_width = other.m_pointer_width;
    m_pmem = other.m_pmem;
    m_pae = other.m_pae;
    if (!other.m_vmtrans) {
        m_vmtrans.reset(nullptr);
    } else {
        m_vmtrans = std::make_unique<VirtualMemoryTranslator>(*other.m_vmtrans);
    }
}
