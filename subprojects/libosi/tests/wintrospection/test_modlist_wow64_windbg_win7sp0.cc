#include "iohal/memory/virtual_memory.h"
#include "offset/offset.h"
#include "osi/windows/manager.h"
#include "osi/windows/wintrospection.h"
#include "gtest/gtest.h"
#include <set>
#include <unistd.h>

#include <dirent.h>
#include <iostream>
#include <map>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

char* testfile = nullptr;
char* dlllistfile = nullptr;

struct ModuleInfo {
    uint64_t base;
    uint32_t size;
    uint16_t loadcount;
    std::string path;
};

int MAX_BUFFER_SIZE = 65536;
std::map<uint64_t, std::vector<struct ModuleInfo>> EXPECTED_RESULTS;
std::map<uint64_t, std::vector<struct ModuleInfo>> WOW64_EXPECTED_RESULTS = {
    {2316,
     {{0x00f80000, 0x00097000, 0x0000ffff,
       "C:\\Program Files (x86)\\Debugging Tools for Windows (x86)\\windbg.exe"},
      {0x77320000, 0x00180000, 0x0000ffff, "C:\\Windows\\SysWOW64\\ntdll.dll"},
      {0x76d30000, 0x00100000, 0x0000ffff, "C:\\Windows\\syswow64\\kernel32.dll"},
      {0x75600000, 0x00046000, 0x0000ffff, "C:\\Windows\\syswow64\\KERNELBASE.dll"},
      {0x75560000, 0x000a0000, 0x0000ffff, "C:\\Windows\\syswow64\\ADVAPI32.dll"},
      {0x769b0000, 0x000ac000, 0x0000ffff, "C:\\Windows\\syswow64\\msvcrt.dll"},
      {0x767a0000, 0x00019000, 0x0000ffff, "C:\\Windows\\SysWOW64\\sechost.dll"},
      {0x76e30000, 0x000f0000, 0x0000ffff, "C:\\Windows\\syswow64\\RPCRT4.dll"},
      {0x74e90000, 0x00060000, 0x0000ffff, "C:\\Windows\\syswow64\\SspiCli.dll"},
      {0x74e80000, 0x0000c000, 0x0000ffff, "C:\\Windows\\syswow64\\CRYPTBASE.dll"},
      {0x74f80000, 0x00090000, 0x0000ffff, "C:\\Windows\\syswow64\\GDI32.dll"},
      {0x75010000, 0x00100000, 0x0000ffff, "C:\\Windows\\syswow64\\USER32.dll"},
      {0x763b0000, 0x0000a000, 0x0000ffff, "C:\\Windows\\syswow64\\LPK.dll"},
      {0x75370000, 0x0009d000, 0x0000ffff, "C:\\Windows\\syswow64\\USP10.dll"},
      {0x72460000, 0x0039b000, 0x0000ffff,
       "C:\\Program Files (x86)\\Debugging Tools for Windows (x86)\\dbgeng.dll"},
      {0x73af0000, 0x00141000, 0x0000ffff,
       "C:\\Program Files (x86)\\Debugging Tools for Windows (x86)\\dbghelp.dll"},
      {0x73ae0000, 0x00009000, 0x0000ffff, "C:\\Windows\\system32\\VERSION.dll"},
      {0x767c0000, 0x0015c000, 0x0000ffff, "C:\\Windows\\syswow64\\ole32.dll"},
      {0x75650000, 0x00c49000, 0x0000ffff, "C:\\Windows\\syswow64\\SHELL32.dll"},
      {0x76cd0000, 0x00057000, 0x0000ffff, "C:\\Windows\\syswow64\\SHLWAPI.dll"},
      {0x722c0000, 0x0019e000, 0x0000ffff,
       "C:\\Windows\\WinSxS\\x86_microsoft.windows."
       "common-controls_6595b64144ccf1df_6.0.7600."
       "16385_none_421189da2b7fabfc\\COMCTL32.dll"},
      {0x73ac0000, 0x00012000, 0x0000ffff, "C:\\Windows\\system32\\MPR.dll"},
      {0x76740000, 0x00060000, 0x00000002, "C:\\Windows\\system32\\IMM32.DLL"},
      {0x76c00000, 0x000cc000, 0x00000001, "C:\\Windows\\syswow64\\MSCTF.dll"},
      {0x73a40000, 0x00080000, 0x00000003, "C:\\Windows\\system32\\uxtheme.dll"},
      {0x739a0000, 0x00094000, 0x00000001, "C:\\Windows\\system32\\MSFTEDIT.DLL"},
      {0x73980000, 0x00013000, 0x00000001, "C:\\Windows\\system32\\dwmapi.dll"}}}};

void initialize_expected_results(
    char* dlllistfile,
    std::map<uint64_t, std::vector<struct ModuleInfo>>& EXPECTED_RESULTS)
{
    rapidjson::Document document;
    const char* json = dlllistfile;

    FILE* fp = fopen(json, "r");
    char readBuffer[MAX_BUFFER_SIZE];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    document.ParseStream(is);

    assert(document.IsObject());
    rapidjson::Value& rows = document["rows"];
    assert(rows.IsArray());

    for (rapidjson::Value::ConstValueIterator itr = rows.Begin(); itr != rows.End();
         ++itr) {
        assert((*itr).IsArray());

        uint64_t pid = (*itr)[0].GetUint64();
        auto it = EXPECTED_RESULTS.find(pid);

        if (strcmp((*itr)[4].GetString(), "Error reading PEB for pid") == 0) {
            if (it == EXPECTED_RESULTS.end()) {
                std::vector<struct ModuleInfo> vect;
                EXPECTED_RESULTS[pid] = vect;
            }
            continue;
        }

        struct ModuleInfo* mod = new struct ModuleInfo();
        mod->base = (*itr)[1].GetUint64();
        mod->size = (*itr)[2].GetUint();
        mod->loadcount = (*itr)[3].GetUint();
        mod->path = (*itr)[4].GetString();

        if (it != EXPECTED_RESULTS.end()) {
            (it->second).push_back((*mod));
        } else {
            std::vector<struct ModuleInfo> vect;
            vect.push_back((*mod));

            EXPECTED_RESULTS[pid] = vect;
        }
    }

    fclose(fp);
}

bool find_match(struct WindowsModuleEntry* me, std::vector<struct ModuleInfo>& mi)
{
    uint64_t base_addr = module_entry_get_base_address(me);
    for (auto& entry : mi) {
        if (base_addr == entry.base) {
            EXPECT_EQ(module_entry_get_modulesize(me), entry.size)
                << "ModuleSize mismatch";
            EXPECT_EQ(module_entry_get_loadcount(me), entry.loadcount)
                << "LoadCount mismatch";
            EXPECT_EQ(std::string(module_entry_get_dllpath(me)), entry.path)
                << "dllpath mismatch at address " << module_entry_get_base_address(me);
            return true;
        }
    }
    return false;
}

void handle_proces_modlist_wow64(struct WindowsKernelOSI* wintro,
                                 struct WindowsProcess* p)
{
    auto pid = process_get_pid(p);
    auto candidate = EXPECTED_RESULTS.find(pid);
    auto wow64_candidate = WOW64_EXPECTED_RESULTS.find(pid);
    ASSERT_TRUE(candidate != EXPECTED_RESULTS.end()) << "Failed to find PID";
    ASSERT_TRUE(wow64_candidate != WOW64_EXPECTED_RESULTS.end()) << "Failed to find PID";
    auto& entry = candidate->second;
    auto& wow64entry = wow64_candidate->second;
    uint32_t module_count = 0;
    auto modlist = get_module_list(wintro, process_get_eprocess(p), process_is_wow64(p));

    if (modlist) {
        auto me = module_list_next(modlist);
        while (me) {
            EXPECT_TRUE((find_match(me, entry) || find_match(me, wow64entry)))
                << "Did not find a match for " << module_entry_get_base_address(me);
            module_count++;
            free_module_entry(me);
            me = module_list_next(modlist);
        }
    } else {
        ASSERT_TRUE(entry.size() == 0)
            << "Didn't find a module list where one was expected.";
        ASSERT_TRUE(wow64entry.size() == 0)
            << "Didn't find a module list where one was expected (wow64).";
    }

    fprintf(stderr, "WOW64: %u vs %lu for pid %lu\n", module_count,
            entry.size() + wow64entry.size(), pid);
    ASSERT_EQ(module_count, (entry.size() + wow64entry.size()))
        << "Found an unexpected number of modules for PID: " << pid;
    free_module_list(modlist);
}
void handle_proces_modlist(struct WindowsKernelOSI* wintro, struct WindowsProcess* p)
{
    auto pid = process_get_pid(p);
    auto candidate = EXPECTED_RESULTS.find(pid);
    ASSERT_TRUE(candidate != EXPECTED_RESULTS.end()) << "Failed to find PID";
    auto& entry = candidate->second;

    uint32_t module_count = 0;
    auto modlist = get_module_list(wintro, process_get_eprocess(p), process_is_wow64(p));

    if (modlist) {
        auto me = module_list_next(modlist);
        while (me) {
            EXPECT_TRUE(find_match(me, entry))
                << "Did not find a match for " << module_entry_get_base_address(me);
            module_count++;
            free_module_entry(me);
            me = module_list_next(modlist);
        }
    } else {
        ASSERT_TRUE(entry.size() == 0)
            << "Didn't find a module list where one was expected.";
    }

    fprintf(stderr, "%u vs %lu for pid %lu\n", module_count, entry.size(), pid);
    ASSERT_EQ(module_count, entry.size())
        << "Found an unexpected number of modules for PID: " << pid;
    free_module_list(modlist);
}

TEST(TestWOW64windbgPlist, Win7SP0amd64)
{
    ASSERT_TRUE(testfile) << "Couldn't load input test file!";
    ASSERT_TRUE(access(testfile, R_OK) == 0) << "Could not read input file";

    WindowsKernelManager manager = WindowsKernelManager("windows-64-7sp0");

    auto pmem = load_physical_memory_snapshot(testfile);
    ASSERT_TRUE(pmem != nullptr) << "failed to load physical memory snapshot";

    struct WindowsKernelOSI* kosi = manager.get_kernel_object();

    ASSERT_TRUE(manager.initialize(pmem, 8, 0x187000, 0xfffff80002c3fd00))
        << "Failed to initialize kernel osi";

    initialize_expected_results(dlllistfile, EXPECTED_RESULTS);

    auto plist = get_process_list(kosi);
    ASSERT_TRUE(plist != nullptr) << "Failed to get process list";

    int numProc = 0;
    auto process = process_list_next(plist);
    if (process == nullptr) {
        numProc = 1; // represents system with pid = 4
    } else {
        for (process; process != nullptr; process = process_list_next(plist)) {
            numProc++;
            if (process_get_pid(process) == 2316) {
                handle_proces_modlist_wow64(kosi, process);
            } else {
                handle_proces_modlist(kosi, process);
            }
            free_process(process);
        }
    }

    ASSERT_TRUE(process_list_next(plist) >= nullptr) << "Found too little processes";

    free_process_list(plist);
    pmem->free(pmem);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (argc != 3) {
        fprintf(stderr, "usage: %s snapshot groundtruth\n", argv[0]);
        return 3;
    }

    testfile = argv[1];
    dlllistfile = argv[2];

    return RUN_ALL_TESTS();
}
