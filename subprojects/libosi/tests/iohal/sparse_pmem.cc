#include <cstdlib>
#include <cstring>
#include <stddef.h>

#include "sparse_pmem.h"
#include <iohal/memory/common.h>
#include <iohal/memory/physical_memory.h>

void SparsePhysicalMemory::set_range(pm_addr_t start, const uint8_t* bytes, uint64_t size)
{
    for (size_t ix = 0; ix < size; ++ix) {
        pm_addr_t addr = start + ix;
        if ((uintptr_t)addr > m_max_address) {
            return;
        }
        m_bytemap[addr] = bytes[ix];
    }
}

pm_addr_t SparsePhysicalMemory::get_max_address() { return m_max_address; }

uint8_t SparsePhysicalMemory::get_byte(pm_addr_t addr)
{
    auto candidate = m_bytemap.find(addr);
    if (candidate != m_bytemap.end()) {
        return candidate->second;
    }
    return 0;
}

pm_addr_t get_sparse_physical_memory_upper_bound(struct PhysicalMemory* pmem)
{
    auto stpm = (SparsePhysicalMemory*)pmem->opaque;
    return stpm->get_max_address();
}

bool read_sparse_physical_memory(struct PhysicalMemory* pmem, pm_addr_t addr,
                                 uint8_t* buffer, uint64_t size)
{
    auto stpm = (SparsePhysicalMemory*)pmem->opaque;
    auto max_addr = stpm->get_max_address();

    for (size_t ix = 0; ix < size; ++ix) {
        auto current_addr = addr + ix;
        if ((uintptr_t)current_addr > max_addr) {
            return false;
        }
        buffer[ix] = stpm->get_byte(current_addr);
    }
    return true;
}

void free_sparse_physical_memory(struct PhysicalMemory* pmem)
{
    auto stpm = (SparsePhysicalMemory*)pmem->opaque;
    delete stpm;
    // Memset here for a few reasons:
    //  - Ensure use-after-free bugs fail early (i.e. on a null pointer deref rather
    //    than on stale data)
    //  - The caller will still have an invalid pointer to pmem, so mistakes are more
    //    likely
    //  - This code is only used in testing, so it isn't performance critical
    std::memset(pmem, 0, sizeof(PhysicalMemory));
    std::free(pmem);
}

struct PhysicalMemory* createSparsePhysicalMemory(uint64_t size)
{
    // Allocate the backing physical memory object
    SparsePhysicalMemory* stpm = new SparsePhysicalMemory(size);
    if (stpm == nullptr) {
        return nullptr;
    }

    // Allocate the wrapper object
    auto pmem = (struct PhysicalMemory*)std::calloc(1, sizeof(struct PhysicalMemory));
    if (!pmem) {
        delete stpm;
        return nullptr;
    }
    pmem->tagvalue = SparsePhysicalMemory::STPM_TAG;
    pmem->opaque = stpm;
    pmem->upper_bound = get_sparse_physical_memory_upper_bound;
    pmem->read = read_sparse_physical_memory;
    pmem->free = free_sparse_physical_memory;
    return pmem;
}
