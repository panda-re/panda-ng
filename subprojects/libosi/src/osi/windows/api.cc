#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctype.h>
#include <dlfcn.h>
#include <exception>
#include <functional>
#include <libgen.h>
#include <map>
#include <memory>
#include <set>
#include <stdint.h>
#include <vector>

#include <inttypes.h>

#include "iohal/memory/virtual_memory.h"
#include <offset/i_t.h>
#include <offset/offset.h>

#include "osi/windows/iterator.h"
#include "osi/windows/manager.h"
#include "osi/windows/ustring.h"
#include "osi/windows/wintrospection.h"

#include "windows_handles.h"
#include "windows_static_offsets.h"

// Thrown when PEB cannot be loaded
class NoPebException : public std::exception
{
public:
    const char* what() const throw() { return "Could not read PEB"; }
};

struct WindowsProcessList {
    uint64_t head;
    uint64_t ptr;
    WindowsKernelOSI* kosi;
};

struct WindowsProcess {
    uint64_t eprocess_address;
    char shortname[17];
    char* cmdline;
    uint64_t pid;
    uint64_t ppid;
    uint64_t asid;
    uint64_t createtime;
    uint64_t base_vba;
    bool is_wow64;
};

struct WindowsModuleList {
    std::unique_ptr<WindowsProcessManager> posi;
    std::vector<std::pair<uint64_t, bool>>* module_list;
    uint16_t idx;
};

#define MAX_PATH_SIZE 4096
struct WindowsModuleEntry {
    uint64_t module_entry;
    uint64_t base_address;
    uint32_t checksum;
    uint64_t entrypoint;
    uint32_t flags;
    uint32_t timedatestamp;
    uint16_t loadcount;
    uint32_t modulesize;
    char dllpath[MAX_PATH_SIZE];
    char dllname[MAX_PATH_SIZE];
    bool is_wow64;
};

struct WindowsHandleObject {
    uint8_t type_index;
    uint64_t pointer;
    char* type_name;
    std::unique_ptr<WindowsProcessManager> posi;
};

struct GetModuleByAddrCallbackData {
    std::unique_ptr<WindowsProcessManager> windowsProcessManager;
    uint64_t target_address;
    WindowsModuleEntry* found_module;
};

struct GetModuleBaseAddressByNameCallbackData {
    std::unique_ptr<WindowsProcessManager> windowsProcessManager;
    const char* target_name;
    uint64_t found_base_address;
};

struct HasModulePrefixCallbackData {
    std::unique_ptr<WindowsProcessManager> windowsProcessManager;
    const char* prefix;
    size_t prefix_len;
    bool found;
};

using ModuleIterCallback =
    std::function<bool(uint64_t mod_address, bool is_wow64, void* callback_data)>;

using ModuleIterFailureCallback = std::function<void(void* callback_data)>;

uint64_t get_next_process_link(struct WindowsKernelOSI* kosi, uint64_t start_address);

struct WindowsModuleEntry* create_module_entry(WindowsProcessManager* posi,
                                               uint64_t module_entry_addr, bool is_wow64);

std::string maybe_parse_unicode_string(osi::ustring& ustr)
{
    try {
        return ustr.as_utf8();
    } catch (...) {
        return std::string("");
    }
}

struct WindowsProcessList* get_process_list(struct WindowsKernelOSI* kosi, bool skip_head)
{
    auto plist = new struct WindowsProcessList;

    auto activeprocesslinks =
        offset_of(kosi->kernel_tlib, translate(kosi->kernel_tlib, "_EPROCESS"),
                  "ActiveProcessLinks");

    plist->head = kosi->details.PsActiveProcessHead - activeprocesslinks->offset;

    if (skip_head) {
        plist->head = get_next_process_link(kosi, plist->head);
    }

    plist->ptr = 0;
    plist->kosi = kosi;

    free_member_result(activeprocesslinks);
    return plist;
}

const static uint64_t FAILING_ITERATION = 0xffffffffffffffff;
uint64_t get_next_process_link(struct WindowsKernelOSI* kosi, uint64_t start_address)
{
    auto vmem = kosi->system_vmem;
    auto tlib = kosi->kernel_tlib;
    osi::i_t eproc(vmem, tlib, start_address, "_EPROCESS");
    osi::iterator process_itr(eproc, "ActiveProcessLinks");
    // maximum number of attempts before bailing
    for (unsigned int ix = 0; ix < 3; ++ix) {
        try {
            auto next_process = *process_itr++;
            auto dtb = next_process["Pcb"]["DirectoryTableBase"].getu();
            // _EPROCESS.is_valid from volatility
            if (dtb == 0 || (dtb % 0x20) != 0) {
                continue;
            }

            auto peb = next_process("Peb");

            if (kosi->details.system_eprocess != 0) {
                // we know which process is the system process, so we can do a proper
                // check if this is a valid process. all processes except System should
                // have a valid _PEB pointer. The only time system_eprocess should be
                // zero is during initialization.
                uint64_t address = next_process.get_address();

                if (peb.get_address() == 0 && address != kosi->details.system_eprocess) {
                    continue;
                }
            }

            return next_process.get_address();
        } catch (...) {
            continue;
        }
    }
    return FAILING_ITERATION;
}

struct WindowsProcess* process_list_next(struct WindowsProcessList* plist)
{
    try {
        if (plist->ptr == plist->head) {
            return nullptr;
        } else if (plist->ptr == FAILING_ITERATION) {
            return nullptr;
        } else if (plist->ptr == 0) {
            plist->ptr = plist->head;
        }

        auto p = create_process(plist->kosi, plist->ptr);
        plist->ptr = get_next_process_link(plist->kosi, plist->ptr);

        return p;
    } catch (...) {
        return nullptr;
    }
    return nullptr;
}

static void sanitize_process_name(char* process_name, size_t nbytes)
{
    for (size_t ix = 0; ix < nbytes; ++ix) {
        if (process_name[ix] == 0) {
            break;
        }
        if (!isprint(process_name[ix])) {
            process_name[ix] = '?';
        }
    }
}

struct WindowsProcess* create_process(struct WindowsKernelOSI* kosi,
                                      uint64_t eprocess_address)
{
    auto p = new WindowsProcess();

    p->eprocess_address = eprocess_address;
    auto vmem = std::make_shared<VirtualMemory>(*kosi->system_vmem);
    auto tlib = kosi->kernel_tlib;
    osi::i_t eproc(vmem, tlib, eprocess_address, "_EPROCESS");

    try {
        eproc["ImageFileName"].getx(p->shortname, 16);
        sanitize_process_name(p->shortname, 16);
        // const char shortname[16];
        p->pid = eproc["UniqueProcessId"].getu();
        p->ppid = eproc["InheritedFromUniqueProcessId"].getu();
        p->asid = eproc["Pcb"]["DirectoryTableBase"].getu();
        p->createtime = eproc["CreateTime"].get64();
        if (vmem->get_pointer_width() == 4) {
            p->is_wow64 = false;
        } else {
            p->is_wow64 = (eproc["Wow64Process"].getu() != 0);
        }
    } catch (std::runtime_error) {
        // no point if we can't get basic info
        free_process(p);
        return nullptr;
    }

    // use correct ASID when reading from PEB
    eproc.get_virtual_memory_shared()->set_asid(p->asid);

    osi::i_t peb;
    try {
        if (p->is_wow64) {
            peb = eproc("Wow64Process").set_type("_PEB32");
        } else {
            peb = eproc("Peb");
        }
    } catch (std::runtime_error) {
        // bail if the PEB isn't readable. user can still see other attrs
        p->cmdline = nullptr;
        p->base_vba = 0;
        return p;
    }

    try {
        p->base_vba = peb["ImageBaseAddress"].getu();
    } catch (std::runtime_error) {
        p->base_vba = 0;
    }

    try {
        osi::i_t params;
        if (p->is_wow64) {
            params = osi::i_t(peb.get_virtual_memory_shared(), peb.get_type_library(),
                              peb["ProcessParameters"].get32(),
                              "_RTL_USER_PROCESS_PARAMETERS");
        } else {
            params = peb("ProcessParameters");
        }

        auto cmdline_ustring = osi::ustring(params["CommandLine"]);
        p->cmdline = new char[cmdline_ustring.get_maximum_length()]();

        std::string cmdline = maybe_parse_unicode_string(cmdline_ustring);
        strncpy(p->cmdline, cmdline.c_str(), cmdline_ustring.get_maximum_length() - 1);

    } catch (std::runtime_error) {
        p->cmdline = nullptr;
    }

    return p;
}

struct WindowsProcess* create_process_from_asid(struct WindowsKernelOSI* kosi,
                                                uint64_t asid)
{
    auto vmem = kosi->system_vmem;
    auto __attribute__((unused)) tlib = kosi->kernel_tlib; /* this should probably just be deleted, but I don't want to chck for side-effects */

    auto plist = get_process_list(kosi);
    auto process = process_list_next(plist);

    while (process != nullptr) {
        uint64_t eproc_address = 0;
        uint64_t proc_asid = process_get_asid(process);
        if (proc_asid == asid) {
            eproc_address = process_get_eprocess(process);
        }
        free_process(process);

        if (eproc_address != 0) {
            free_process_list(plist);
            return create_process(kosi, eproc_address);
        }
        process = process_list_next(plist);
    }
    free_process_list(plist);
    return nullptr;
}

uint64_t get_pid_from_asid(struct WindowsKernelOSI* kosi, uint64_t asid)
{
    auto vmem = kosi->system_vmem;
    auto __attribute__((unused)) tlib = kosi->kernel_tlib; /* this should probably just be deleted, but I don't want to chck for side-effects */

    auto plist = get_process_list(kosi);
    auto process = process_list_next(plist);

    while (process != nullptr) {
        uint64_t eproc_address = 0;
        uint64_t proc_asid = process_get_asid(process);
        if (proc_asid == asid) {
            eproc_address = process_get_eprocess(process);
        }
        free_process(process);

        if (eproc_address != 0) {
            free_process_list(plist);
            auto vmem = kosi->system_vmem;
            auto tlib = kosi->kernel_tlib;
            osi::i_t eproc(vmem, tlib, eproc_address, "_EPROCESS");
            return eproc["UniqueProcessId"].getu();
        }
        process = process_list_next(plist);
    }
    free_process_list(plist);
    return 0;
}

uint64_t get_eproc_addr_from_asid(struct WindowsKernelOSI* kosi, uint64_t asid)
{
    auto vmem = kosi->system_vmem;
    auto __attribute__((unused)) tlib = kosi->kernel_tlib; /* this should probably just be deleted, but I don't want to chck for side-effects */

    auto plist = get_process_list(kosi);
    auto process = process_list_next(plist);

    while (process != nullptr) {
        uint64_t eproc_address = 0;
        uint64_t proc_asid = process_get_asid(process);
        if (proc_asid == asid) {
            eproc_address = process_get_eprocess(process);
        }
        free_process(process);

        if (eproc_address != 0) {
            free_process_list(plist);
            return eproc_address;
        }
        process = process_list_next(plist);
    }
    free_process_list(plist);
    return 0;
}

void free_process(struct WindowsProcess* p)
{
    if (p) {
        if (p->cmdline) {
            delete[] p->cmdline;
        }
        delete p;
    }
}

void free_process_list(struct WindowsProcessList* plist)
{
    if (plist) {
        delete plist;
    }
}

uint64_t process_get_eprocess(const struct WindowsProcess* plist)
{
    return plist->eprocess_address;
}

uint64_t process_get_pid(const struct WindowsProcess* plist) { return plist->pid; }

uint64_t process_get_ppid(const struct WindowsProcess* plist) { return plist->ppid; }

const char* process_get_shortname(const struct WindowsProcess* p) { return p->shortname; }

const char* process_get_cmdline(const struct WindowsProcess* p) { return p->cmdline; }

uint64_t process_get_asid(const struct WindowsProcess* p) { return p->asid; }

uint64_t process_createtime(const struct WindowsProcess* p) { return p->createtime; }

uint64_t process_get_base(const struct WindowsProcess* p) { return p->base_vba; }

bool process_is_wow64(const struct WindowsProcess* p) { return p->is_wow64; }

// Module iterator failure callback for get_module_list
// Cleanup when corrupted module list is detected
void get_module_list_failure(void* callback_data)
{
    auto mlist = static_cast<WindowsModuleList*>(callback_data);
    mlist->module_list->clear();
}

// Module iterator callback for get_module_list
// Add module to the list of loaded modules
bool get_module_list_callback(uint64_t mod_address, bool is_wow64, void* callback_data)
{
    auto mlist = static_cast<WindowsModuleList*>(callback_data);
    mlist->module_list->push_back(std::pair<uint64_t, bool>(mod_address, is_wow64));
    return true;
}

// iterate_modules helper, see iterate_modules()
// returns true if module iteration should continue
template <class T1>
bool iter_module_list(osi::i_t& ldr_table, T1& pitr, void* callback_data, bool is_wow64,
                      ModuleIterCallback module_iter_callback,
                      ModuleIterFailureCallback module_iter_failure_callback)
{

    std::set<uint64_t> modules;
    bool rval = true;

    pitr++; // skip head_sentinel

    do {
        auto entry = *pitr;
        auto mod_address = entry.get_address();
        auto insert_result = modules.insert(mod_address);
        if (!insert_result.second) {
            fprintf(stderr, "WARNING: Found an anomaly (duplicated module), "
                            "jumping out...");
            // Fail hard, we've only seen this when the list is corrupted
            if (module_iter_failure_callback) {
                module_iter_failure_callback(callback_data);
            }

            // Previous version of this file would attempt to iterate non-wow module
            // list when wow64 module list was corrupted, so return true here to keep
            // the behavior consistent.  Returning false would stop iterating the list
            // of modules.
            return true;
        }

        // Process the module, return value indicates if iteratation should continue
        rval = module_iter_callback(mod_address, is_wow64, callback_data);

        if ((!rval) || (!pitr.has_next())) {
            break;
        }

        pitr++;

    } while (*pitr != ldr_table);

    return rval;
}

// Get the InLoadOrderModuleList (wow64)
osi::i_t get_in_load_order_module_list_wow(osi::i_t& proc)
{
    uint32_t peb32_address = proc["Wow64Process"].get32();
    osi::i_t peb32 = osi::i_t(proc.get_virtual_memory_shared(), proc.get_type_library(),
                              peb32_address, "_PEB32");
    uint32_t ldr32_address = peb32["Ldr"].get32();
    osi::i_t ldr32 = osi::i_t(proc.get_virtual_memory_shared(), proc.get_type_library(),
                              ldr32_address, "_PEB_LDR_DATA32");
    return ldr32["InLoadOrderModuleList"].set_type("_LDR_DATA_TABLE_ENTRY32");
}

// Get the InLoadOrderModuleList (non wow64)
osi::i_t get_in_load_order_module_list(osi::i_t& proc)
{
    auto peb = proc("Peb");
    if (peb.get_address() == 0) {
        throw NoPebException();
    }

    auto ldr = peb("Ldr");
    return ldr["InLoadOrderModuleList"].set_type("_LDR_DATA_TABLE_ENTRY");
}

// Iterate over the loaded modules, call module_iter_callback for each module
// module_iter_callback should return false if the iteration should not continue
// If an anomaly is found in the module list, module_iter_failure_callback will be called
bool iterate_modules(bool is_wow64, osi::i_t& proc, void* callback_data,
                     ModuleIterCallback callback,
                     ModuleIterFailureCallback failureCallback)
{
    if (is_wow64) {
        try {
            auto ldr_table32 = get_in_load_order_module_list_wow(proc);
            osi::iterator32 pitr(ldr_table32, "InLoadOrderLinks");
            iter_module_list(ldr_table32, pitr, callback_data, true, callback,
                             failureCallback);
        } catch (const std::exception& e) {
            std::cerr << e.what();
        }
    }

    try {
        auto ldr_table = get_in_load_order_module_list(proc);
        osi::iterator pitr(ldr_table, "InLoadOrderLinks");
        iter_module_list(ldr_table, pitr, callback_data, false, callback,
                         failureCallback);
    } catch (const std::exception& e) {
        // TODO Make sure this is paged out and not just generic failure
        return false;
    }

    return true;
}

// Get the list of loaded modules
struct WindowsModuleList* get_module_list(struct WindowsKernelOSI* kosi,
                                          uint64_t process_address, bool is_wow64)
{
    auto mlist =
        (struct WindowsModuleList*)std::calloc(1, sizeof(struct WindowsModuleList));
    if (!mlist) {
        return nullptr;
    }
    mlist->module_list = new std::vector<std::pair<uint64_t, bool>>();

    mlist->posi = std::unique_ptr<WindowsProcessManager>(new WindowsProcessManager());

    if (!(mlist->posi->initialize(kosi, process_address))) {
        free_module_list(mlist);
        return nullptr;
    }

    mlist->idx = 0;
    auto proc = mlist->posi->get_process();

    if (iterate_modules(is_wow64, proc, mlist, get_module_list_callback,
                        get_module_list_failure)) {
        return mlist;
    }

    free_module_list(mlist);
    return nullptr;
}

// Module iterator callback for get_module_by_addr
// Returns false to stop iteration if module contains the address being searched for
bool get_module_by_addr_callback(uint64_t mod_address, bool is_wow64, void* callback_data)
{

    auto* data = static_cast<GetModuleByAddrCallbackData*>(callback_data);

    auto data_table_entry = data->windowsProcessManager->get_type(
        mod_address, is_wow64 ? "_LDR_DATA_TABLE_ENTRY32" : "_LDR_DATA_TABLE_ENTRY");

    uint64_t base_address = is_wow64 ? data_table_entry["DllBase"].get32()
                                     : data_table_entry["DllBase"].getu();

    // Is this the module being searched for?
    if (data->target_address >= base_address) {
        auto modulesize = data_table_entry["SizeOfImage"].get32();
        if (data->target_address < (base_address + modulesize)) {
            data->found_module = create_module_entry(data->windowsProcessManager.get(),
                                                     mod_address, is_wow64);
            // found target module, stop iterating
            return false;
        }
    }

    // Continue searching
    return true;
}

// Module iterator callback for has_module_prefix
// Returns false to stop iteration if module name starts with the prefix being searched
// for
bool has_module_prefix_callback(uint64_t mod_address, bool is_wow64, void* callback_data)
{

    auto* data = static_cast<HasModulePrefixCallbackData*>(callback_data);

    auto data_table_entry = data->windowsProcessManager->get_type(
        mod_address, is_wow64 ? "_LDR_DATA_TABLE_ENTRY32" : "_LDR_DATA_TABLE_ENTRY");

    osi::ustring dllname(data_table_entry["BaseDllName"]);
    std::string dllname_utf8 = maybe_parse_unicode_string(dllname);

    // Is this the module being searched for?
    if (0 == strncmp(data->prefix, dllname_utf8.c_str(), data->prefix_len)) {
        data->found = true;
        // found target module, stop iterating
        return false;
    }

    // Continue searching
    return true;
}

// Module iterator callback for get_module_base_address_by_name
// Returns false to stop iteration if module name matches the name being searched for
bool get_module_base_address_by_name_callback(uint64_t mod_address, bool is_wow64,
                                              void* callback_data)
{

    auto* data = static_cast<GetModuleBaseAddressByNameCallbackData*>(callback_data);

    auto data_table_entry = data->windowsProcessManager->get_type(
        mod_address, is_wow64 ? "_LDR_DATA_TABLE_ENTRY32" : "_LDR_DATA_TABLE_ENTRY");

    osi::ustring dllname(data_table_entry["BaseDllName"]);
    std::string dllname_utf8 = maybe_parse_unicode_string(dllname);

    // Is this the module being searched for?
    /*
            previously used g_strcmp0() which is the only reason for glib dependency.
            the only difference is the handling of NULL pointers.
            std::basic_string<T>::c_str() is assured to be non-NULL, so the only question
            is whether data->target_name is null. The actual sort order here is irrelevant
            (g_strcmp0 puts NULL first) because we only test for equality.
        */
    if (data->target_name && !strcmp(data->target_name, dllname_utf8.c_str())) {
        data->found_base_address = is_wow64 ? data_table_entry["DllBase"].get32()
                                            : data_table_entry["DllBase"].getu();
        // found target module, stop iterating
        return false;
    }

    // Continue searching
    return true;
}

// Returns module entry that matches addr
struct WindowsModuleEntry* get_module_by_addr(struct WindowsKernelOSI* kosi,
                                              uint64_t process_address, bool is_wow64,
                                              uint64_t addr)
{
    GetModuleByAddrCallbackData callback_data = {
        std::unique_ptr<WindowsProcessManager>(new WindowsProcessManager()), addr};

    if (callback_data.windowsProcessManager->initialize(kosi, process_address)) {

        auto proc = callback_data.windowsProcessManager->get_process();

        if (iterate_modules(is_wow64, proc, &callback_data, get_module_by_addr_callback,
                            nullptr)) {

            return callback_data.found_module;
        }
    }
    return nullptr;
}

// Returns true if a module is loaded whose name starts with prefix
bool has_module_prefix(struct WindowsKernelOSI* kosi, uint64_t process_address,
                       bool is_wow64, const char* prefix)
{
    HasModulePrefixCallbackData callback_data = {
        std::unique_ptr<WindowsProcessManager>(new WindowsProcessManager()), prefix,
        strlen(prefix)};

    if (callback_data.windowsProcessManager->initialize(kosi, process_address)) {

        auto proc = callback_data.windowsProcessManager->get_process();

        if (iterate_modules(is_wow64, proc, &callback_data, has_module_prefix_callback,
                            nullptr)) {

            return callback_data.found;
        }
    }
    return false;
}

// Returns base address for module with specified name
uint64_t get_module_base_address_by_name(struct WindowsKernelOSI* kosi,
                                         uint64_t process_address, bool is_wow64,
                                         const char* name)
{

    GetModuleBaseAddressByNameCallbackData callback_data = {
        std::unique_ptr<WindowsProcessManager>(new WindowsProcessManager()), name};

    if (callback_data.windowsProcessManager->initialize(kosi, process_address)) {

        auto proc = callback_data.windowsProcessManager->get_process();

        if (iterate_modules(is_wow64, proc, &callback_data,
                            get_module_base_address_by_name_callback, nullptr)) {

            return callback_data.found_base_address;
        }
    }
    return 0;
}

struct WindowsModuleEntry* create_module_entry(WindowsProcessManager* posi,
                                               uint64_t module_entry_addr, bool is_wow64)
{
    auto mentry =
        (struct WindowsModuleEntry*)std::calloc(1, sizeof(struct WindowsModuleEntry));
    mentry->module_entry = module_entry_addr;

    // No point if we can't capture these
    try {
        if (is_wow64) {
            auto data_table_entry =
                posi->get_type(module_entry_addr, "_LDR_DATA_TABLE_ENTRY32");

            mentry->base_address = data_table_entry["DllBase"].get32();
            mentry->modulesize = data_table_entry["SizeOfImage"].get32();
            mentry->checksum = data_table_entry["CheckSum"].get32();
            mentry->entrypoint = data_table_entry["EntryPoint"].get32();
            mentry->flags = data_table_entry["Flags"].get32();
            mentry->timedatestamp = data_table_entry["TimeDateStamp"].get32();
            mentry->loadcount = data_table_entry["LoadCount"].get16();

            osi::ustring dllname(data_table_entry["BaseDllName"]);
            std::string dllname_utf8 = maybe_parse_unicode_string(dllname);
            strncpy(mentry->dllname, dllname_utf8.c_str(), MAX_PATH_SIZE - 1);

            osi::ustring dllpath(data_table_entry["FullDllName"]);
            std::string dllpath_utf8 = maybe_parse_unicode_string(dllpath);
            strncpy(mentry->dllpath, dllpath_utf8.c_str(), MAX_PATH_SIZE - 1);
        } else {
            auto data_table_entry =
                posi->get_type(module_entry_addr, "_LDR_DATA_TABLE_ENTRY");

            mentry->base_address = data_table_entry["DllBase"].getu();
            mentry->modulesize = data_table_entry["SizeOfImage"].get32();
            mentry->checksum = data_table_entry["CheckSum"].get32();
            mentry->entrypoint = data_table_entry["EntryPoint"].getu();
            mentry->flags = data_table_entry["Flags"].get32();
            mentry->timedatestamp = data_table_entry["TimeDateStamp"].get32();
            mentry->loadcount = data_table_entry["LoadCount"].get16();

            osi::ustring dllname(data_table_entry["BaseDllName"]);
            std::string dllname_utf8 = maybe_parse_unicode_string(dllname);
            strncpy(mentry->dllname, dllname_utf8.c_str(), MAX_PATH_SIZE - 1);

            osi::ustring dllpath(data_table_entry["FullDllName"]);
            std::string dllpath_utf8 = maybe_parse_unicode_string(dllpath);
            strncpy(mentry->dllpath, dllpath_utf8.c_str(), MAX_PATH_SIZE - 1);
        }
    } catch (...) {
        free_module_entry(mentry);
        mentry = nullptr;
    }
    return mentry;
}

void free_module_entry(struct WindowsModuleEntry* me)
{
    if (me) {
        std::free(me);
    }
}

struct WindowsModuleEntry* module_list_next(struct WindowsModuleList* mlist)
{
    if (mlist->module_list->size() == 0) {
        return nullptr;
    } else if (mlist->module_list->size() <= mlist->idx) {
        return nullptr;
    }

    auto mod_pair = (*(mlist->module_list))[mlist->idx++];
    auto mod_address = mod_pair.first;
    bool mod_iswow64 = mod_pair.second;
    // TOOD push this down into module_entry ctor
    auto mod_entry = create_module_entry(mlist->posi.get(), mod_address, mod_iswow64);
    if (!mod_entry) {
        // skip invalid
        return module_list_next(mlist);
    }
    return mod_entry;
}

struct WindowsProcessOSI* module_list_get_osi(struct WindowsModuleList* mlist)
{
    return mlist->posi->get_process_object();
}

void free_module_list(struct WindowsModuleList* mlist)
{
    if (mlist) {
        delete mlist->module_list;
        std::free(mlist);
    }
}

uint64_t module_entry_get_module_entry(struct WindowsModuleEntry* me)
{
    return me->module_entry;
}

uint64_t module_entry_get_base_address(struct WindowsModuleEntry* me)
{
    return me->base_address;
}

uint32_t module_entry_get_checksum(struct WindowsModuleEntry* me) { return me->checksum; }

uint64_t module_entry_get_entrypoint(struct WindowsModuleEntry* me)
{
    return me->entrypoint;
}

uint32_t module_entry_get_flags(struct WindowsModuleEntry* me) { return me->flags; }

uint32_t module_entry_get_timedatestamp(struct WindowsModuleEntry* me)
{
    return me->timedatestamp;
}

uint16_t module_entry_get_loadcount(struct WindowsModuleEntry* me)
{
    return me->loadcount;
}

uint32_t module_entry_get_modulesize(struct WindowsModuleEntry* me)
{
    return me->modulesize;
}

bool module_entry_is_wow64(struct WindowsModuleEntry* me) { return me->is_wow64; }

const char* module_entry_get_dllpath(struct WindowsModuleEntry* me)
{
    return me->dllpath;
}

const char* module_entry_get_dllname(struct WindowsModuleEntry* me)
{
    return me->dllname;
}

static osi::i_t kosi_get_current_process_object(struct WindowsKernelOSI* kosi)
{
    osi::i_t kpcr =
        osi::i_t(kosi->system_vmem, kosi->kernel_tlib, kosi->details.kpcr, "_KPCR");

    osi::i_t eprocess;
    if (kosi->system_vmem->get_pointer_width() == 4) {
        const char* profile = get_type_library_profile(kosi->kernel_tlib);

        auto thread = kpcr["PrcbData"]("CurrentThread");
        if (strncmp(profile, "windows-32-xp", 13) == 0 ||
            strncmp(profile, "windows-32-2000", 15) == 0) {
            // Windows 2k & XP
            eprocess =
                thread.set_type("_ETHREAD")("ThreadsProcess").set_type("_EPROCESS");
        } else {
            // Windows 7+
            eprocess = thread("Process").set_type("_EPROCESS");
        }
    } else {
        auto ethread = kpcr["Prcb"]("CurrentThread").set_type("_ETHREAD");
        eprocess = ethread.set_type("_KTHREAD")("Process").set_type("_EPROCESS");
    }
    return eprocess;
}

uint64_t kosi_get_current_process_address(struct WindowsKernelOSI* kosi)
{
    auto eprocess = kosi_get_current_process_object(kosi);
    return eprocess.get_address();
}

struct WindowsProcess* kosi_get_current_process(struct WindowsKernelOSI* kosi)
{
    auto eprocess = kosi_get_current_process_object(kosi);
    return create_process(kosi, eprocess.get_address());
}

uint64_t kosi_get_current_tid(struct WindowsKernelOSI* kosi)
{
    osi::i_t kpcr(kosi->system_vmem, kosi->kernel_tlib, kosi->details.kpcr, "_KPCR");

    osi::i_t ethread;
    if (kosi->system_vmem->get_pointer_width() == 4) {
        ethread = kpcr["PrcbData"]("CurrentThread");
    } else {
        ethread = kpcr["Prcb"]("CurrentThread");
    }
    ethread.set_type("_ETHREAD");

    return ethread["Cid"]["UniqueThread"].getu();
}

struct WindowsHandleObject* resolve_handle(struct WindowsKernelOSI* kosi, uint64_t handle)
{
    struct WindowsHandleObject* h =
        (struct WindowsHandleObject*)std::calloc(1, sizeof(struct WindowsHandleObject));

    h->posi = std::make_unique<WindowsProcessManager>();
    if (!(h->posi->initialize(kosi, kosi_get_current_process_address(kosi)))) {
        free_handle(h);
        return nullptr;
    }
    auto posi = h->posi->get_process_object();

    bool win2k = false;
    bool winxp = false;

    const char* profile = get_type_library_profile(kosi->kernel_tlib);
    if (strncmp(profile, "windows-32-2000", 15) == 0) {
        win2k = true;
    } else if (strncmp(profile, "windows-32-xp", 13) == 0) {
        winxp = true;
    }

    osi::i_t obj_header;
    if (win2k) {
        obj_header = resolve_handle_table_entry_win2000(posi, handle);
    } else {
        obj_header =
            resolve_handle_table_entry(posi, handle, kosi->details.pointer_width > 4);
    }

    if (!obj_header.get_address()) {
        free_handle(h);
        return nullptr;
    }

    try {
        h->pointer = obj_header["Body"].get_address();
        if (win2k || winxp) {
            auto type = obj_header("Type");
            auto name = osi::ustring(type["Name"]);
            std::string name_str = maybe_parse_unicode_string(name);

            h->type_index = type["Index"].get8();
            h->type_name = strdup(name_str.c_str());
        } else {
            // Windows 7 (+)
            h->type_index = obj_header["TypeIndex"].get8();
            h->type_name =
                translate_enum(kosi->kernel_tlib, "ObTypeIndexTable", h->type_index);
        }
    } catch (std::runtime_error) {
        free_handle(h);
        return nullptr;
    }

    return h;
}

uint64_t handle_get_pointer(struct WindowsHandleObject* handle)
{
    return handle->pointer;
}

uint8_t handle_get_type(struct WindowsHandleObject* handle) { return handle->type_index; }

const char* handle_get_typename(struct WindowsHandleObject* handle)
{
    return handle->type_name;
}

struct WindowsProcessOSI* handle_get_context(struct WindowsHandleObject* handle)
{
    return handle->posi->get_process_object();
}

void free_handle(struct WindowsHandleObject* handle)
{
    if (handle) {
        std::free(handle->type_name);
        std::free(handle);
    }
}

static inline uint16_t get_page_shift()
{
    // For 4K pages
    return 12;
}

const std::pair<uint64_t, uint64_t> NO_MATCH = {0, 0};
static inline std::pair<uint64_t, uint64_t>
find_vad_range(osi::i_t& eprocess, struct WindowsProcessOSI* process_osi, uint64_t addr)
{
    auto page_shift = get_page_shift();
    uint64_t target_vpn = addr >> page_shift;
    auto vad_root = eprocess["VadRoot"];
    auto working = vad_root["BalancedRoot"];

    while (working.get_address() != 0) {
        // Does this node contain target_vpn?
        auto starting_vpn = working["StartingVpn"].getu();
        auto ending_vpn = working["EndingVpn"].getu();
        if ((starting_vpn <= target_vpn) && (target_vpn <= ending_vpn)) {
            working = working.set_type("_MMVAD");
            auto proto_pte = working["FirstPrototypePte"].getu();
            if (proto_pte == 0) {
                fprintf(stderr, "Failed to read prototype pte\n");
                return NO_MATCH;
            }

            uint64_t vad_offset = addr - (starting_vpn << page_shift);
            uint64_t vad_pte_index = vad_offset >> page_shift;
            uint64_t pte_addr =
                proto_pte + (vad_pte_index * process_osi->vmem->get_pointer_width());

            osi::i_t mmpte(process_osi->vmem, process_osi->tlib, pte_addr, "_MMPTE");
            uint64_t pte = mmpte.getu();
            return std::pair<uint64_t, uint64_t>(pte_addr, pte);
        }
        // Check the left arm
        if (target_vpn < starting_vpn) {
            auto left_child = working("LeftChild");
            if (left_child.get_address() == 0) {
                return NO_MATCH;
            }
            working = left_child;
            continue;
        } else if (ending_vpn < target_vpn) {
            // Check the right arm
            auto right_child = working("RightChild");
            if (right_child.get_address() == 0) {
                return NO_MATCH;
            }
            working = right_child;
            continue;
        } else {
            return NO_MATCH;
        }
    }

    return NO_MATCH;
}

static bool valid_pte(uint64_t pte)
{
    if ((pte & (0x1 << 0)) == 1) {
        return true;
    }
    // If it is not valid and a prototype, bail
    if ((pte & (0x1 << 10)) != 0) {
        return false;
    }
    // if it is not valid and is in transition, good to go
    if ((pte & (0x1 << 11)) != 0) {
        return true;
    }

    return false;
}

TranslateStatus process_vmem_read(struct WindowsProcessOSI* process_osi, vm_addr_t addr,
                                  void* buffer, uint64_t size)
{
    // Try to read the page normally
    auto status = process_osi->vmem->read(addr, buffer, size);
    if (TRANSLATE_SUCCEEDED(status)) {
        return status;
    }

    if (status != TSTAT_PAGED_OUT) {
        return status;
    }

    // This logic has only been figured for Windows 7
    const char* profile = get_type_library_profile(process_osi->tlib);
    if (strncmp(profile, "windows-32-7", 12) != 0 &&
        strncmp(profile, "windows-64-7", 12) != 0) {
        return status;
    }

    // Try to handle the page fault
    osi::i_t proc(process_osi->vmem, process_osi->tlib, process_osi->eprocess_address,
                  "_EPROCESS");
    uint64_t bytes_to_read = size;
    while (bytes_to_read > 0) {
        auto vad = find_vad_range(proc, process_osi, addr);
        auto pte_addr = std::get<0>(vad);
        auto pte = std::get<1>(vad);
        if (!valid_pte(pte)) {
            fprintf(stderr, "Failed to read from pte %" PRIx64 " %" PRIx64 "!\n", static_cast<uint64_t>(pte_addr), static_cast<uint64_t>(pte));
            return TSTAT_GENERIC_FAILURE;
        }
        uint64_t chunk_size = 0xfff - (addr & 0xfff);
        chunk_size = std::min(chunk_size, bytes_to_read);
        if (chunk_size == 0) {
            chunk_size = bytes_to_read;
        }
        uint64_t physaddr = (pte & 0xffffffffff000) + (addr & 0xfff);
        auto pmem = process_osi->kosi->pmem;
        pmem->read(pmem, physaddr, (uint8_t*)buffer, chunk_size);
        addr += chunk_size;
        buffer = (void*)(((uint8_t*)buffer) + chunk_size);
        bytes_to_read -= chunk_size;
    }
    return TSTAT_SUCCESS;
}
