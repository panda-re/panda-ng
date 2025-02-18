#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stddef.h>
#include <unistd.h>

#include "iohal/memory/common.h"
#include "iohal/memory/physical_memory.h"

class FileSnapshotPhysicalMemory
{
private:
    FILE* m_fp;
    pm_addr_t m_max_address;

public:
    static const uint32_t FSPM_TAG = 0x4653504d;

    FileSnapshotPhysicalMemory(const char* fpath)
    {
        m_fp = fopen(fpath, "rb");
        fseek(m_fp, 0, SEEK_END);
        m_max_address = ftell(m_fp);
    }

    pm_addr_t get_max_address();
    uint8_t get_byte(pm_addr_t);
    int close_file_snapshot();
};

int FileSnapshotPhysicalMemory::close_file_snapshot() { return fclose(m_fp); }

pm_addr_t FileSnapshotPhysicalMemory::get_max_address() { return m_max_address; }

uint8_t FileSnapshotPhysicalMemory::get_byte(pm_addr_t addr)
{
    uint8_t retval = 0;
    if (addr < m_max_address) {
        fseek(m_fp, addr, SEEK_SET);
        if (fread(&retval, 1, 1, m_fp) < 1) {
            fprintf(stderr, "Warning: error reading physical address %" PRIxPMADDR "\n",
                    addr);
            return 0;
        }
    }
    return retval;
}

pm_addr_t get_file_snapshot_physical_memory_upper_bound(struct PhysicalMemory* pmem)
{
    auto fspm = (FileSnapshotPhysicalMemory*)pmem->opaque;
    return fspm->get_max_address();
}

bool read_file_snapshot_physical_memory(struct PhysicalMemory* pmem, pm_addr_t addr,
                                        uint8_t* buffer, uint64_t size)
{
    auto fspm = (FileSnapshotPhysicalMemory*)pmem->opaque;
    auto max_addr = fspm->get_max_address();

    for (size_t ix = 0; ix < size; ++ix) {
        auto current_addr = addr + ix;
        if ((uintptr_t)current_addr > max_addr) {
            return false;
        }
        buffer[ix] = fspm->get_byte(current_addr);
    }
    return true;
}

void free_file_snapshot_physical_memory(struct PhysicalMemory* pmem)
{
    auto fspm = (FileSnapshotPhysicalMemory*)pmem->opaque;
    fspm->close_file_snapshot();
    delete fspm;
    // Memset here for a few reasons:
    //  - Ensure use-after-free bugs fail early (i.e. on a null pointer deref rather
    //    than on stale data)
    //  - The caller will still have an invalid pointer to pmem, so mistakes are more
    //    likely
    //  - This code is only used in testing, so it isn't performance critical
    std::memset(pmem, 0, sizeof(PhysicalMemory));
    std::free(pmem);
}

struct PhysicalMemory* load_physical_memory_snapshot(const char* fpath)
{
    // Make sure the file path exists
    if (access(fpath, R_OK) != 0) {
        return nullptr;
    }

    // Allocate the backing physical memory object
    FileSnapshotPhysicalMemory* fspm = new FileSnapshotPhysicalMemory(fpath);
    if (fspm == nullptr) {
        return nullptr;
    }

    // Allocate the wrapper object
    auto pmem = (struct PhysicalMemory*)std::calloc(1, sizeof(struct PhysicalMemory));
    if (!pmem) {
        delete fspm;
        return nullptr;
    }
    pmem->tagvalue = FileSnapshotPhysicalMemory::FSPM_TAG;
    pmem->opaque = fspm;
    pmem->upper_bound = get_file_snapshot_physical_memory_upper_bound;
    pmem->read = read_file_snapshot_physical_memory;
    pmem->free = free_file_snapshot_physical_memory;
    return pmem;
}
