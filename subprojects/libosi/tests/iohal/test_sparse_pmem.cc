#include "sparse_pmem.h"
#include "gtest/gtest.h"

// Sanity check that the object can be included, allocated, and freed
TEST(SparsePmemTest, PmemAllocate)
{
    struct PhysicalMemory* pmem = createSparsePhysicalMemory(2048);
    ASSERT_TRUE(pmem != nullptr) << "Could not allocate physical memory object!";
    pmem->free(pmem);
}

TEST(SparsePmemTest, PmemSparseRead)
{
    uint8_t target_data[8] = {0x12, 0x43, 0x99, 0xa1, 0x00, 0xb2, 0x00, 0x00};

    // Initialize physical memory with some data
    struct PhysicalMemory* pmem = createSparsePhysicalMemory(2048 * 1024);
    auto spm = (SparsePhysicalMemory*)pmem->opaque;
    spm->set_range(1024, target_data, 6);

    // Read it back out
    uint8_t output_data[8] = {0};
    ASSERT_TRUE(pmem->read(pmem, 1024, output_data, 8))
        << "Failed to read physical memory";

    // Make sure it matches
    bool failed = false;
    for (size_t ix = 0; ix < 8; ++ix) {
        failed = failed | (target_data[ix] != output_data[ix]);
    }
    ASSERT_TRUE(!failed) << "Failed to read data back out";

    pmem->free(pmem);
}

TEST(SparsePmemTest, PmemOOBRead)
{
    size_t max_addr = 2048 * 1024;
    struct PhysicalMemory* pmem = createSparsePhysicalMemory(max_addr);

    // Read it back out
    uint8_t* output_data = (uint8_t*)calloc(1, 0x10);
    ASSERT_TRUE(!pmem->read(pmem, max_addr - 0x8, output_data, 16))
        << "Failed to read physical memory";
    free(output_data);
    pmem->free(pmem);
}
