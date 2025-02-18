#ifndef __SPARSE_PMEM_H
#define __SPARSE_PMEM_H

#include <map>
#include <stddef.h>

#include <iohal/memory/common.h>
#include <iohal/memory/physical_memory.h>

class SparsePhysicalMemory
{
private:
    std::map<pm_addr_t, uint8_t> m_bytemap;
    pm_addr_t m_max_address;

public:
    static const uint32_t STPM_TAG = 0x41918171;
    SparsePhysicalMemory(pm_addr_t max) : m_max_address(max) {}

    pm_addr_t get_max_address();
    uint8_t get_byte(pm_addr_t);
    void set_range(pm_addr_t start, const uint8_t* bytes, uint64_t size);
};

// struct PhysicalMemory helper functions
pm_addr_t get_sparse_physical_memory_upper_bound(struct PhysicalMemory*);

bool read_sparse_physical_memory(struct PhysicalMemory*, pm_addr_t addr, uint8_t* buffer,
                                 uint64_t size);

void free_sparse_physical_memory(struct PhysicalMemory*);

struct PhysicalMemory* createSparsePhysicalMemory(uint64_t size);

#endif
