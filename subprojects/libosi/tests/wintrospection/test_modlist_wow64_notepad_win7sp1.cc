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
    {1932,
     {{0x00b00000, 0x0028f000, 0x0000ffff,
       "C:\\Users\\lisa\\Desktop\\npp.7.5.4.bin\\notepad++.exe"},
      {0x775f0000, 0x00180000, 0x0000ffff, "C:\\Windows\\SysWOW64\\ntdll.dll"},
      {0x75950000, 0x00110000, 0x0000ffff, "C:\\Windows\\syswow64\\kernel32.dll"},
      {0x76eb0000, 0x00046000, 0x0000ffff, "C:\\Windows\\syswow64\\KERNELBASE.dll"},
      {0x740d0000, 0x0019e000, 0x0000ffff,
       "C:\\Windows\\WinSxS\\x86_microsoft.windows."
       "common-controls_6595b64144ccf1df_6.0.7601."
       "17514_none_41e6975e2bd6f2b2\\COMCTL32.dll"},
      {0x75db0000, 0x000ac000, 0x0000ffff, "C:\\Windows\\syswow64\\msvcrt.dll"},
      {0x77140000, 0x00090000, 0x0000ffff, "C:\\Windows\\syswow64\\GDI32.dll"},
      {0x751b0000, 0x00100000, 0x0000ffff, "C:\\Windows\\syswow64\\USER32.dll"},
      {0x75650000, 0x000a0000, 0x0000ffff, "C:\\Windows\\syswow64\\ADVAPI32.dll"},
      {0x760d0000, 0x00019000, 0x0000ffff, "C:\\Windows\\SysWOW64\\sechost.dll"},
      {0x75c00000, 0x000f0000, 0x0000ffff, "C:\\Windows\\syswow64\\RPCRT4.dll"},
      {0x75150000, 0x00060000, 0x0000ffff, "C:\\Windows\\syswow64\\SspiCli.dll"},
      {0x75140000, 0x0000c000, 0x0000ffff, "C:\\Windows\\syswow64\\CRYPTBASE.dll"},
      {0x756f0000, 0x0000a000, 0x0000ffff, "C:\\Windows\\syswow64\\LPK.dll"},
      {0x76f90000, 0x0009d000, 0x0000ffff, "C:\\Windows\\syswow64\\USP10.dll"},
      {0x755f0000, 0x00057000, 0x0000ffff, "C:\\Windows\\syswow64\\SHLWAPI.dll"},
      {0x76260000, 0x00c4a000, 0x0000ffff, "C:\\Windows\\syswow64\\SHELL32.dll"},
      {0x73fe0000, 0x000eb000, 0x0000ffff, "C:\\Windows\\system32\\dbghelp.dll"},
      {0x760f0000, 0x0011d000, 0x0000ffff, "C:\\Windows\\syswow64\\CRYPT32.dll"},
      {0x752b0000, 0x0000c000, 0x0000ffff, "C:\\Windows\\syswow64\\MSASN1.dll"},
      {0x75380000, 0x0002d000, 0x0000ffff, "C:\\Windows\\syswow64\\WINTRUST.dll"},
      {0x73fd0000, 0x00006000, 0x0000ffff, "C:\\Windows\\system32\\SensApi.dll"},
      {0x752c0000, 0x0007b000, 0x0000ffff, "C:\\Windows\\syswow64\\COMDLG32.dll"},
      {0x75790000, 0x0015c000, 0x0000ffff, "C:\\Windows\\syswow64\\ole32.dll"},
      {0x758f0000, 0x00060000, 0x00000006, "C:\\Windows\\system32\\IMM32.DLL"},
      {0x77030000, 0x000cc000, 0x00000003, "C:\\Windows\\syswow64\\MSCTF.dll"},
      {0x75340000, 0x0002a000, 0x00000001, "C:\\Windows\\syswow64\\imagehlp.dll"},
      {0x73fb0000, 0x00016000, 0x00000001, "C:\\Windows\\system32\\CRYPTSP.dll"},
      {0x73f70000, 0x0003b000, 0x00000001, "C:\\Windows\\system32\\rsaenh.dll"},
      {0x73f30000, 0x00038000, 0x00000001, "C:\\Windows\\system32\\ncrypt.dll"},
      {0x73f10000, 0x00017000, 0x00000007, "C:\\Windows\\system32\\bcrypt.dll"},
      {0x73ed0000, 0x0003d000, 0x00000001, "C:\\Windows\\SysWOW64\\bcryptprimitives.dll"},
      {0x73dc0000, 0x00105000, 0x00000001,
       "C:\\Users\\lisa\\Desktop\\npp.7.5.4.bin\\SciLexer.dll"},
      {0x75cf0000, 0x0008f000, 0x00000007, "C:\\Windows\\syswow64\\OLEAUT32.dll"},
      {0x74560000, 0x00005000, 0x00000001, "C:\\Windows\\system32\\Msimg32.DLL"},
      {0x73d40000, 0x00080000, 0x00000005, "C:\\Windows\\system32\\uxtheme.dll"},
      {0x75700000, 0x00083000, 0x00000001, "C:\\Windows\\syswow64\\CLBCatQ.DLL"},
      {0x73c40000, 0x000f5000, 0x00000001, "C:\\Windows\\system32\\propsys.dll"},
      {0x73c10000, 0x00021000, 0x00000001, "C:\\Windows\\system32\\ntmarta.dll"},
      {0x76210000, 0x00045000, 0x00000001, "C:\\Windows\\syswow64\\WLDAP32.dll"},
      {0x75a60000, 0x0019d000, 0x00000001, "C:\\Windows\\syswow64\\SETUPAPI.dll"},
      {0x75d80000, 0x00027000, 0x00000004, "C:\\Windows\\syswow64\\CFGMGR32.dll"},
      {0x771d0000, 0x00012000, 0x00000001, "C:\\Windows\\syswow64\\DEVOBJ.dll"},
      {0x73bf0000, 0x00013000, 0x00000001, "C:\\Windows\\system32\\dwmapi.dll"},
      {0x73af0000, 0x000fb000, 0x00000001, "C:\\Windows\\system32\\WindowsCodecs.dll"},
      {0x729e0000, 0x002a3000, 0x00000001,
       "C:\\Users\\lisa\\Desktop\\npp.7.5.4.bin\\plugins\\DSpellCheck.dll"},
      {0x73ae0000, 0x00009000, 0x00000001, "C:\\Windows\\system32\\VERSION.dll"},
      {0x754f0000, 0x000f5000, 0x00000002, "C:\\Windows\\syswow64\\WININET.dll"},
      {0x753b0000, 0x00136000, 0x00000001, "C:\\Windows\\syswow64\\urlmon.dll"},
      {0x75ed0000, 0x001fb000, 0x00000002, "C:\\Windows\\syswow64\\iertutil.dll"},
      {0x77100000, 0x00035000, 0x00000001, "C:\\Windows\\syswow64\\WS2_32.dll"},
      {0x775c0000, 0x00006000, 0x00000001, "C:\\Windows\\syswow64\\NSI.dll"},
      {0x72f00000, 0x0001c000, 0x00000001,
       "C:\\Users\\lisa\\Desktop\\npp.7.5.4.bin\\plugins\\mimeTools.dll"},
      {0x72ea0000, 0x00024000, 0x00000001,
       "C:\\Users\\lisa\\Desktop\\npp.7.5.4.bin\\plugins\\NppConverter.dll"},
      {0x10000000, 0x00007000, 0x00000001,
       "C:\\Users\\lisa\\Desktop\\npp.7.5.4.bin\\plugins\\NppExport.dll"}}}};

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

    if (candidate == EXPECTED_RESULTS.end()) {
        fprintf(stderr, "Can't find pid %lu in dlllist\n", pid);
        auto modlist =
            get_module_list(wintro, process_get_eprocess(p), process_is_wow64(p));
        ASSERT_TRUE(!modlist) << "found modules that don't exist for PID" << pid;
        return;
    }

    auto& entry = candidate->second;

    uint32_t module_count = 0;
    auto modlist = get_module_list(wintro, process_get_eprocess(p), process_is_wow64(p));

    if (modlist) {
        auto me = module_list_next(modlist);
        while (me) {
            EXPECT_TRUE(find_match(me, entry))
                << "Did not find a match for " << module_entry_get_base_address(me)
                << "with dllpath " << module_entry_get_dllpath(me);
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

TEST(TestWOW64notepadPlist, Win7SP1amd64)
{
    ASSERT_TRUE(testfile) << "Couldn't load input test file!";
    ASSERT_TRUE(access(testfile, R_OK) == 0) << "Could not read input file";

    WindowsKernelManager manager = WindowsKernelManager("windows-64-7sp1");

    auto pmem = load_physical_memory_snapshot(testfile);
    ASSERT_TRUE(pmem != nullptr) << "failed to load physical memory snapshot";

    initialize_expected_results(dlllistfile, EXPECTED_RESULTS);

    struct WindowsKernelOSI* kosi = manager.get_kernel_object();

    ASSERT_TRUE(manager.initialize(pmem, 8, 0x187000, 0xfffff80002c43d00))
        << "Failed to initialize kernel osi";

    auto plist = get_process_list(kosi);
    ASSERT_TRUE(plist != nullptr) << "Failed to get process list";

    int numProc = 0;
    auto process = process_list_next(plist);
    if (process == nullptr) {
        numProc = 1; // represents system with pid = 4
    } else {
        for (; process != nullptr; process = process_list_next(plist)) {
            numProc++;
            if (process_get_pid(process) == 1932) {
                handle_proces_modlist_wow64(kosi, process);
            } else {
                handle_proces_modlist(kosi, process);
            }
            free_process(process);
        }
    }

    ASSERT_TRUE(numProc >= EXPECTED_RESULTS.size()) << "Found too little processes";

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
