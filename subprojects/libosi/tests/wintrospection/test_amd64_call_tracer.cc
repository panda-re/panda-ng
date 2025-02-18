#include <offset/i_t.h>
#include <offset/offset.h>

#include "osi/windows/manager.h"
#include "osi/windows/wintrospection.h"
#include "gtest/gtest.h"
#include <iohal/memory/virtual_memory.h>
#include <set>
#include <unistd.h>

#include <iostream>
#include <map>

char* testfile = nullptr;

TEST(TestAmd64CallTracer, Win7SP1Amd64)
{
    ASSERT_TRUE(testfile) << "Couldn't load input test file!";
    ASSERT_TRUE(access(testfile, R_OK) == 0) << "Could not read input file";

    WindowsKernelManager manager = WindowsKernelManager("windows-64-7sp1");

    auto pmem = load_physical_memory_snapshot(testfile);
    ASSERT_TRUE(pmem != nullptr) << "failed to load physical memory snapshot";

    struct WindowsKernelOSI* kosi = manager.get_kernel_object();

    ASSERT_TRUE(manager.initialize(pmem, 8, 0x335ef000, 0xfffff80002834d00))
        << "Failed to initialize kernel osi";

    auto proc = kosi_get_current_process(kosi);

    auto proc_manager = WindowsProcessManager();
    ASSERT_TRUE(proc_manager.initialize(kosi, process_get_pid(proc)));

    auto eproc = proc_manager.get_process();
    ASSERT_TRUE(eproc.get_address() == 0xfffffa80021bb430) << "wrong eproc addr";
    osi::i_t peb = eproc("Peb");
    ASSERT_TRUE(peb.get_address() == 0x7FFFFFDF000) << "wrong peb addr";
    osi::i_t LDR_DATA_TABLE = peb("Ldr");
    ASSERT_TRUE(LDR_DATA_TABLE.get_address() == 0x77BD3640) << "wrong ldr addr";
    osi::i_t ldr_list = LDR_DATA_TABLE["InLoadOrderModuleList"];
    ASSERT_TRUE(ldr_list.get_address() == 0x77BD3650) << "wrong ldr list addr";
    osi::i_t LDR_DATA_ENTRY = ldr_list("Flink").set_type("_LDR_DATA_TABLE_ENTRY");
    ASSERT_TRUE(LDR_DATA_ENTRY.get_address() == 0x002E2010)
        << "wrong ldr list entry addr";
    uint64_t fmodule_base = LDR_DATA_ENTRY["DllBase"].getu();
    uint64_t fmodule_size = LDR_DATA_ENTRY["SizeOfImage"].getu();

    fprintf(stderr, "fmodule_base: %lu\n", fmodule_base);
    fprintf(stderr, "fmodule_size: %lu\n", fmodule_size);

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
