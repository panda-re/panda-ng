#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "iohal/memory/physical_memory.h"
#include "iohal/memory/virtual_memory.h"
#include "offset/offset.h"
#include "gtest/gtest.h"
#include <offset/i_t.h>

char* testfile = nullptr;

TEST(ItAmd64Test, LoadTestFile)
{
    ASSERT_TRUE(testfile) << "Couldn't find input test file!";

    ASSERT_TRUE(access(testfile, R_OK) == 0) << "Could not read input file";

    auto pmem = load_physical_memory_snapshot(testfile);
    ASSERT_TRUE(pmem != nullptr) << "failed to load physical memory snapshot";

    pmem->free(pmem);
}

TEST(ItAmd64Test, KpcrAmd64)
{
    ASSERT_TRUE(testfile) << "Couldn't find input test file!";

    auto pmem = load_physical_memory_snapshot(testfile);
    ASSERT_TRUE(pmem != nullptr) << "failed to load physical memory snapshot";

    auto tlib = load_type_library("windows-64-7sp1");
    ASSERT_TRUE(tlib != nullptr) << "Could not locate requested profile";

    auto vmem = std::make_shared<VirtualMemory>(pmem, 64, 0x1c55a000, false, "unknown");

    vm_addr_t kpcr_address = 0xfffff8000284cd00;
    osi::i_t kpcr = osi::i_t(vmem, tlib, kpcr_address, "_KPCR");

    vm_addr_t selfptr = kpcr["Self"].getu();
    ASSERT_EQ(selfptr, kpcr_address) << "KPCR does not seem valid!";

    auto idle_thread = kpcr("CurrentPrcb")["IdleThread"];
    ASSERT_EQ(idle_thread.getu(), 0xFFFFF8000285ACC0);

    // Scan down by page for kernel base
    vm_addr_t scanner = idle_thread.getu() & 0xFFFFFFFFFFFFF000;
    vm_addr_t lower_bound = scanner - 0x1000000;
    while (scanner > lower_bound) {
        try {
            uint16_t test = 0;
            vmem->read(scanner, &test, 2);
            if (test == 0x5a4d) {
                break;
            }
            scanner -= 0x1000;
        } catch (...) {
            break;
        }
    }

    vm_addr_t kernel_base = 0xfffff8000265a000;
    ASSERT_EQ(scanner, kernel_base);
    // 0xfffff8000284cd00

    pmem->free(pmem);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (argc != 2) {
        fprintf(stderr, "usage: %s amd64.raw\n", argv[0]);
        return 3;
    }

    testfile = argv[1];

    return RUN_ALL_TESTS();
}
