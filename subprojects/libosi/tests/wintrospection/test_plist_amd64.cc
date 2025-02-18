#include "offset/offset.h"
#include "osi/windows/manager.h"
#include "osi/windows/wintrospection.h"
#include "gtest/gtest.h"
#include <iohal/memory/virtual_memory.h>
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
    {4, {0xfffffa8000c379e0, "System", 4, 0, 0}},
    {220, {0xfffffa80020406f0, "smss.exe", 220, 4, 0}},
    {320, {0xfffffa800232a060, "csrss.exe", 320, 308, 0}},
    {368, {0xfffffa8000ca2060, "wininit.exe", 368, 308, 0}},
    {396, {0xfffffa80023b0600, "csrss.exe", 396, 360, 0}},
    {420, {0xfffffa80023d2740, "services.exe", 420, 368, 0}},
    {436, {0xfffffa80023d6b30, "lsass.exe", 436, 368, 0}},
    {444, {0xfffffa80023e6b30, "lsm.exe", 444, 368, 0}},
    {468, {0xfffffa80023f16e0, "winlogon.exe", 468, 360, 0}},
    {592, {0xfffffa800247ab30, "svchost.exe", 592, 420, 0}},
    {664, {0xfffffa800226a060, "svchost.exe", 664, 420, 0}},
    {704, {0xfffffa80024bb410, "svchost.exe", 704, 420, 0}},
    {828, {0xfffffa80024dbb30, "svchost.exe", 828, 420, 0}},
    {876, {0xfffffa8002585b30, "svchost.exe", 876, 420, 0}},
    {972, {0xfffffa8002670b30, "svchost.exe", 972, 420, 0}},
    {412, {0xfffffa80026a0750, "dwm.exe", 412, 828, 0}},
    {1012, {0xfffffa80026c0b30, "explorer.exe", 1012, 608, 0}},
    {308, {0xfffffa80026d0270, "svchost.exe", 308, 420, 0}},
    {1180, {0xfffffa8002366ab0, "spoolsv.exe", 1180, 420, 0}},
    {1216, {0xfffffa80024eeb30, "taskhost.exe", 1216, 420, 0}},
    {1236, {0xfffffa8002573970, "svchost.exe", 1236, 420, 0}},
    {1456, {0xfffffa80026d4b30, "svchost.exe", 1456, 420, 0}},
    {1760, {0xfffffa800284eb30, "SearchIndexer.", 1760, 420, 0}},
    {1648, {0xfffffa80028cab30, "sppsvc.exe", 1648, 420, 0}},
    {1808, {0xfffffa80026b4b30, "svchost.exe", 1808, 420, 0}},
    {1652, {0xfffffa8000e18b30, "iexplore.exe", 1652, 1012, 1}},
    {1880, {0xfffffa8000d8e060, "setup_wm.exe", 1880, 824, 1}},
    {1524, {0xfffffa8000dd2060, "xpsrchvw.exe", 1524, 1012, 0}},
    {2060, {0xfffffa8000dc64c0, "iexplore.exe", 2060, 1652, 1}},
    {2920, {0xfffffa8000dd6b30, "taskmgr.exe", 2920, 1012, 0}},
    {2824, {0xfffffa8000dafb30, "ehshell.exe", 2824, 1012, 0}},
    {2872, {0xfffffa8000f98b30, "msiexec.exe", 2872, 420, 0}},
    {2808, {0xfffffa8001016b30, "sidebar.exe", 2808, 1012, 0}},
    {3040, {0xfffffa8000faa630, "msiexec.exe", 3040, 2872, 0}},
    {2900, {0xfffffa80017acb30, "TrustedInstall", 2900, 420, 0}},
    {2140, {0xfffffa800173e060, "python.exe", 2140, 1012, 0}},
    {2024, {0xfffffa800147ab30, "conhost.exe", 2024, 396, 0}}};

TEST(TestAmd64Plist, Win7SP1Amd64)
{
    ASSERT_TRUE(testfile) << "Couldn't load input test file!";
    ASSERT_TRUE(access(testfile, R_OK) == 0) << "Could not read input file";

    WindowsKernelManager manager = WindowsKernelManager("windows-64-7sp1");

    auto pmem = load_physical_memory_snapshot(testfile);
    ASSERT_TRUE(pmem != nullptr) << "failed to load physical memory snapshot";

    struct WindowsKernelOSI* kosi = manager.get_kernel_object();

    ASSERT_TRUE(manager.initialize(pmem, 8, 0x1c55a000, 0xfffff8000284cd00))
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
        fprintf(stderr, "usage: %s amd64.raw\n", argv[0]);
        return 3;
    }

    testfile = argv[1];

    return RUN_ALL_TESTS();
}
