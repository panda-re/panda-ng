#ifndef __MEMORY_VIRTUAL_MEMORY_H
#define __MEMORY_VIRTUAL_MEMORY_H

#include <iohal/memory/common.h>
#include <iohal/memory/physical_memory.h>
#include <iohal/memory/virtual_memory_translator.h>
#include <memory>

class VirtualMemory
{
public:
    VirtualMemory(const VirtualMemory& other);
    VirtualMemory(struct PhysicalMemory* pmem, uint8_t bits, pm_addr_t asid, bool pae,
                  const char* profile);
    TranslateStatus read(vm_addr_t addr, void* buffer, uint64_t size);
    TranslateStatus read_pointer(vm_addr_t addr, vm_addr_t* buffer);

    uint8_t get_pointer_width();
    uint8_t get_bits();
    uint64_t set_asid(uint64_t new_asid);
    uint64_t get_asid();

private:
    uint8_t m_pointer_width;
    struct PhysicalMemory* m_pmem;
    std::unique_ptr<VirtualMemoryTranslator> m_vmtrans;
    bool m_pae;
};

#endif
