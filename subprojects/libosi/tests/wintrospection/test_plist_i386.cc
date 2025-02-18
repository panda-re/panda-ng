#include "iohal/memory/virtual_memory.h"
#include "offset/offset.h"
#include "osi/windows/manager.h"
#include "osi/windows/wintrospection.h"
#include "gtest/gtest.h"
#include <set>
#include <unistd.h>

#include <iostream>
#include <map>

char* testfile = nullptr;

struct ProcessInfo {
    uint64_t offset;
    std::string name;
    uint64_t pid;
    uint64_t ppid;
    bool wow64;
};

std::map<uint64_t, struct ProcessInfo> EXPECTED_RESULTS = {
    {4, {0x84234a48, "System", 4, 0, 0}},
    {212, {0x8506e2f0, "smss.exe", 212, 4, 0}},
    {296, {0x85635b90, "csrss.exe", 296, 288, 0}},
    {344, {0x85629d40, "wininit.exe", 344, 288, 0}},
    {356, {0x842bab10, "csrss.exe", 356, 336, 0}},
    {384, {0x85658030, "winlogon.exe", 384, 336, 0}},
    {428, {0x85666818, "services.exe", 428, 344, 0}},
    {440, {0x85670030, "lsass.exe", 440, 344, 0}},
    {452, {0x85672030, "lsm.exe", 452, 344, 0}},
    {552, {0x842888a0, "svchost.exe", 552, 428, 0}},
    {632, {0x8574a6e8, "svchost.exe", 632, 428, 0}},
    {676, {0x85764578, "svchost.exe", 676, 428, 0}},
    {788, {0x84283828, "svchost.exe", 788, 428, 0}},
    {832, {0x85791b18, "svchost.exe", 832, 428, 0}},
    {920, {0x857a7030, "svchost.exe", 920, 428, 0}},
    {1036, {0x857c3738, "svchost.exe", 1036, 428, 0}},
    {1132, {0x857fa850, "spoolsv.exe", 1132, 428, 0}},
    {1168, {0x8580b990, "svchost.exe", 1168, 428, 0}},
    {1276, {0x85840518, "svchost.exe", 1276, 428, 0}},
    {1424, {0x85841030, "taskhost.exe", 1424, 428, 0}},
    {1520, {0x858b5030, "dwm.exe", 1520, 788, 0}},
    {1528, {0x858b6790, "explorer.exe", 1528, 1492, 0}},
    {312, {0x859405a8, "dinotify.exe", 312, 100, 0}},
    {288, {0x8594a030, "rundll32.exe", 288, 268, 0}},
    {276, {0x85982cd8, "rundll32.exe", 276, 256, 0}},
    {1736, {0x859acd40, "SearchIndexer.", 1736, 428, 0}},
    {1316, {0x85850478, "SearchProtocol", 1316, 1736, 0}},
    {1392, {0x8594fa88, "SearchFilterHo", 1392, 1736, 0}},
    {1100, {0x843b8d40, "mscorsvw.exe", 1100, 428, 0}},
    {184, {0x858b1d18, "sppsvc.exe", 184, 428, 0}},
    {1072, {0x8435f030, "svchost.exe", 1072, 428, 0}}};

TEST(TestI386Plist, Win7SP1I386)
{
    ASSERT_TRUE(testfile) << "Couldn't load input test file!";
    ASSERT_TRUE(access(testfile, R_OK) == 0) << "Could not read input file";

    WindowsKernelManager manager = WindowsKernelManager("windows-32-7sp1");

    auto pmem = load_physical_memory_snapshot(testfile);
    ASSERT_TRUE(pmem != nullptr) << "failed to load physical memory snapshot";

    struct WindowsKernelOSI* kosi = manager.get_kernel_object();

    ASSERT_TRUE(manager.initialize(pmem, 4, 0x185000, 0x82933c00))
        << "Failed to initialize kernel osi";

    auto plist = get_process_list(kosi);
    ASSERT_TRUE(plist != nullptr) << "Failed to get process list";

    for (unsigned int ix = 0; ix < EXPECTED_RESULTS.size(); ++ix) {
        auto process = process_list_next(plist);
        ASSERT_TRUE(process != nullptr) << "Didn't find enough processes";
        auto pid = process_get_pid(process);
        auto candidate = EXPECTED_RESULTS.find(pid);
        ASSERT_TRUE(candidate != EXPECTED_RESULTS.end()) << "Failed to find PID";
        auto& entry = candidate->second;
        ASSERT_EQ(entry.offset, process_get_eprocess(process)) << "_EPROCESS mismatch";
        ASSERT_EQ(strcmp(entry.name.c_str(), process_get_shortname(process)), 0)
            << "shortname mismatch";
        ASSERT_EQ(entry.pid, process_get_pid(process)) << "PID mismatch";
        ASSERT_EQ(entry.ppid, process_get_ppid(process)) << "PPID mismatch";
        ASSERT_EQ(entry.wow64, process_is_wow64(process)) << "WOW64 mismatch";
        // ASSERT_EQ(entry.asid, process_get_asid(process)) << "ASID mismatch";
        // ASSERT_EQ(entry.createtime, process_get_createtime(process)) << "createtime
        // mismatch";
        free_process(process);
    }
    ASSERT_TRUE(process_list_next(plist) == nullptr) << "Found too many processes";

    free_process_list(plist);
    pmem->free(pmem);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (argc != 2) {
        fprintf(stderr, "usage: %s i386.raw\n", argv[0]);
        return 3;
    }

    testfile = argv[1];

    return RUN_ALL_TESTS();
}
