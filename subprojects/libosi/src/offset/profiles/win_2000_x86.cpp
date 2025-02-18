/**
 * WARNING: This profile is incomplete, pieced together based on what was needed.
 *          I did not add very much information that is not currently being used,
 *          as this was a manual process.
 */
#include "offset/offset.h"
#include <map>
#include <string>
#define POINTER 0x80000000

#include "win_2000_x86.h"

namespace windows_2000_x86
{

enum Type : unsigned int {
    UNKNOWN,
    _LIST_ENTRY,
    _UNICODE_STRING,
    _DISPATCHER_HEADER,
    _PEB_LDR_DATA,
    _LDR_DATA_TABLE_ENTRY,
    _CURDIR,
    _RTL_USER_PROCESS_PARAMETERS,
    _FILE_OBJECT,
    _OBJECT_TYPE,
    _OBJECT_HEADER,
    _HANDLE_TABLE,
    _CLIENT_ID,
    _ETHREAD,
    _KPRCB,
    _KPCR,
    _PEB,
    _KPROCESS,
    _EPROCESS
};

static std::map<std::string, std::pair<int, unsigned int>> OFFSET[] = {
    {}, // UNKNOWN
    {
        // _LIST_ENTRY
        {"Flink", {0x0, _LIST_ENTRY | POINTER}},
        {"Blink", {0x4, _LIST_ENTRY | POINTER}},
    },
    {
        // _UNICODE_STRING
        {"Buffer", {0x4, UNKNOWN | POINTER}},
        {"Length", {0x0, UNKNOWN}},
        {"MaximumLength", {0x2, UNKNOWN}},
    },
    {
        // _DISPATCHER_HEADER
        {"Type", {0x0, UNKNOWN}},
        {"Size", {0x2, UNKNOWN}},
    },
    {
        // _PEB_LDR_DATA
        {"InLoadOrderModuleList", {0xc, _LIST_ENTRY}},
    },
    {
        // _LDR_DATA_TABLE_ENTRY
        {"InLoadOrderLinks", {0x0, _LIST_ENTRY}},
        {"DllBase", {0x18, UNKNOWN | POINTER}},
        {"SizeOfImage", {0x20, UNKNOWN}},
        {"EntryPoint", {0x1c, UNKNOWN | POINTER}},
        {"FullDllName", {0x24, _UNICODE_STRING}},
        {"BaseDllName", {0x2c, _UNICODE_STRING}},
        {"Flags", {0x34, UNKNOWN}},
        {"LoadCount", {0x38, UNKNOWN}},
        {"CheckSum", {0x40, UNKNOWN}},
        {"TimeDateStamp", {0x44, UNKNOWN}},
    },
    {
        // _CURDIR
        {"DosPath", {0x0, _UNICODE_STRING}},
        {"Handle", {0x8, UNKNOWN | POINTER}},
    },
    {
        // _RTL_USER_PROCESS_PARAMETERS
        {"CurrentDirectory", {0x24, _CURDIR}},
        {"CommandLine", {0x40, _UNICODE_STRING}},
        {"ImagePathName", {0x38, _UNICODE_STRING}},
        {"DllPath", {0x30, _UNICODE_STRING}},
        {"DesktopInfo", {0x78, _UNICODE_STRING}},
        {"StandardInput", {0x18, UNKNOWN | POINTER}},
        {"StandardOutput", {0x1c, UNKNOWN | POINTER}},
        {"StandardError", {0x20, UNKNOWN | POINTER}},
    },
    {
        // _FILE_OBJECT
        {"FileName", {0x30, _UNICODE_STRING}},
        {"CurrentByteOffset", {0x38, UNKNOWN}},
    },
    {
        // _OBJECT_TYPE
        {"Name", {0x40, _UNICODE_STRING}}, // unverified?
        {"Index", {0x4c, UNKNOWN}},        // unverified?
    },
    {
        // _OBJECT_HEADER
        {"Body", {0x18, UNKNOWN}},
        {"Type", {0x8, _OBJECT_TYPE | POINTER}},
    },
    {
        // _HANDLE_TABLE
        {"TableCode", {0x0, UNKNOWN}},
        {"Layer1", {0x8, UNKNOWN}},
    },
    {
        // _CLIENT_ID
        {"UniqueProcess", {0x0, UNKNOWN | POINTER}},
        {"UniqueThread", {0x4, UNKNOWN | POINTER}},
    },
    {
        // _ETHREAD
        {"Cid", {0x1e0, _CLIENT_ID}},
        {"ThreadsProcess", {0x22C, _EPROCESS | POINTER}},
    },
    {
        // _KPRCB
        {"MinorVersion", {0x0, UNKNOWN}},
        {"MajorVersion", {0x2, UNKNOWN}},
        {"CurrentThread", {0x4, _ETHREAD | POINTER}},
        {"NextThread", {0x8, _ETHREAD | POINTER}},
        {"IdleThread", {0xc, _ETHREAD | POINTER}},
    },
    {
        // _KPCR
        {"SelfPcr", {0x1c, _KPCR | POINTER}},
        {"Prcb", {0x20, _KPRCB | POINTER}},
        {"PrcbData", {0x120, _KPRCB}},
    },
    {
        // _PEB
        {"ProcessParameters", {0x10, _RTL_USER_PROCESS_PARAMETERS | POINTER}},
        {"ImageBaseAddress", {0x8, UNKNOWN | POINTER}},
        {"Ldr", {0xc, _PEB_LDR_DATA | POINTER}},
    },
    {
        // _KPROCESS
        {"Header", {0x0, _DISPATCHER_HEADER}},
        {"DirectoryTableBase", {0x18, UNKNOWN}},
    },
    {
        // _EPROCESS
        {"Pcb", {0x0, _KPROCESS}},
        {"UniqueProcessId", {0x9c, UNKNOWN | POINTER}},
        {"InheritedFromUniqueProcessId", {0x1c8, UNKNOWN | POINTER}},
        {"ImageFileName", {0x1fc, UNKNOWN}},
        {"ObjectTable", {0x128, _HANDLE_TABLE | POINTER}},
        {"ActiveProcessLinks", {0xa0, _LIST_ENTRY}},
        {"Peb", {0x1b0, _PEB | POINTER}},
        {"CreateTime", {0x88, UNKNOWN}},
        {"ExitTime", {0x90, UNKNOWN}},
        {"VadRoot", {0x194, UNKNOWN | POINTER}},
        {"VadHint", {0x198, UNKNOWN | POINTER}},
        {"VirtualSize", {0xc8, UNKNOWN}},
    },
};

std::map<std::string, unsigned int> TRANSLATE = {
    {"UNKNOWN", 0},
    {"_LIST_ENTRY", _LIST_ENTRY},
    {"_UNICODE_STRING", _UNICODE_STRING},
    {"_DISPATCHER_HEADER", _DISPATCHER_HEADER},
    {"_PEB_LDR_DATA", _PEB_LDR_DATA},
    {"_LDR_DATA_TABLE_ENTRY", _LDR_DATA_TABLE_ENTRY},
    {"_CURDIR", _CURDIR},
    {"_RTL_USER_PROCESS_PARAMETERS", _RTL_USER_PROCESS_PARAMETERS},
    {"_FILE_OBJECT", _FILE_OBJECT},
    {"_OBJECT_TYPE", _OBJECT_TYPE},
    {"_OBJECT_HEADER", _OBJECT_HEADER},
    {"_HANDLE_TABLE", _HANDLE_TABLE},
    {"_CLIENT_ID", _CLIENT_ID},
    {"_ETHREAD", _ETHREAD},
    {"_KPRCB", _KPRCB},
    {"_KPCR", _KPCR},
    {"_PEB", _PEB},
    {"_KPROCESS", _KPROCESS},
    {"_EPROCESS", _EPROCESS}};

static std::map<std::string, std::map<long, std::string>> ENUM = {{}};

uint64_t translate_type(const char* tname)
{
    const std::string tname_str(tname);
    auto search = TRANSLATE.find(tname_str);
    if (search != TRANSLATE.end()) {
        return search->second;
    }
    return INVALID_TYPE;
}

uint64_t offset_of_member(uint64_t tid, const char* mname)
{
    if (tid >= sizeof OFFSET) {
        return INVALID_OFFSET;
    }
    const auto& type_offsets = OFFSET[tid];
    const std::string mname_str(mname);
    auto search = type_offsets.find(mname_str);
    if (search != type_offsets.end()) {
        return search->second.first;
    }
    return INVALID_OFFSET;
}

uint64_t type_of_member(uint64_t tid, const char* mname)
{
    if (tid >= sizeof OFFSET) {
        return INVALID_OFFSET;
    }
    const auto& type_offsets = OFFSET[tid];
    const std::string mname_str(mname);
    auto search = type_offsets.find(mname_str);
    if (search != type_offsets.end()) {
        return search->second.second;
    }
    return INVALID_OFFSET;
}

std::string translate_enum(const char* ename, long idx)
{
    auto search = ENUM.find(std::string(ename));

    if (search != ENUM.end()) {
        auto name = search->second.find(idx);
        if (name != search->second.end()) {
            return name->second;
        }
    }

    return "unknown";
}
} // namespace windows_2000_x86
