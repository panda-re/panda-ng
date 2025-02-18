#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <libgen.h>
#include <map>
#include <memory>
#include <stdint.h>
#include <vector>

#include "iohal/memory/common.h"
#include "iohal/memory/physical_memory.h"
#include "iohal/memory/virtual_memory.h"

#include "offset/i_t.h"
#include "offset/offset.h"

#include "osi/linux/iterator.h"
#include "osi/linux/lintrospection.h"

#include "kernel_osi.h"

struct LinuxTask {
    uint64_t address;
    uint64_t tid;
    uint64_t pid;
    uint64_t ppid;
    uint64_t asid;
    uint64_t createtime;
    char shortname[17];
};
typedef LinuxTask task;

struct LinuxMemoryRegion {
    uint64_t address;   // this struct
    uint64_t next_area; // next struct
    uint64_t next_file; // next struct with a file

    uint64_t base_address;
    uint64_t virtual_size;
    uint64_t protections;
    uint64_t mtime;
    char path[MAX_MODULE_PATH_SIZE];
    char name[MAX_MODULE_PATH_SIZE];

    uint64_t file_start_addr;
};

/*
 * HELPERS
 *
 */
static void sanitize_process_name(char* process_name, size_t nbytes)
{
    for (size_t ix = 0; ix < nbytes; ++ix) {
        if (process_name[ix] == 0) {
            break;
        }
        /*
            previously used g_ascii_isprint() which is the only reason for glib dependency.
            the only difference is the glib function returns false for non-ascii characters
            and takes a char value instead of an int.
        */
        if (!isascii(process_name[ix]) || !isprint(process_name[ix])) {
            process_name[ix] = '?';
        }
    }
}

void get_name_from_path(char* path, char* name)
{
    memset(name, '\0', MAX_MODULE_PATH_SIZE);

    auto path_str = std::string(path);
    auto idx = path_str.find_last_of("/");
    if (idx != std::string::npos) {
        std::string substr = path_str.substr(idx + 1);
        strncpy(name, substr.c_str(), path_str.length() - idx);
    }
}

osi::i_t get_mm_if_exists(osi::i_t task_struct, bool* success)
{
    osi::i_t mm_struct = task_struct("mm").set_type("mm_struct");

    if (mm_struct.get_address()) {
        *success = true;
        return mm_struct;
    }

    *success = false;
    return osi::i_t();
}

bool has_pointer(osi::i_t _struct, std::string _field)
{
    if (_struct[_field].getu()) {
        return true;
    }
    return false;
}

osi::i_t get_next_file_area(osi::i_t vm_area_struct, bool* success)
{
    if (!has_pointer(vm_area_struct, "vm_next")) {
        *success = false;
        return vm_area_struct;
    }

    auto current = vm_area_struct("vm_next");

    while (true) {
        if (has_pointer(current, "vm_file")) {
            *success = true;
            break;
        }

        if (!has_pointer(current, "vm_next")) {
            *success = false;
            break;
        }

        current = current("vm_next");
    }

    return current;
}

/*
 * This has been useful in a couple places. Takes in a dentry
 * and returns the associated file path. Good for stringifying
 * modules and file descriptors
 * see __dentry_path in fs/dcache.c in linux kernel
 *
 */
void get_dentry_path(osi::i_t dentry, char* result)
{
    typedef struct dentry_path {
        char final[MAX_MODULE_PATH_SIZE];
        char combined[MAX_MODULE_PATH_SIZE];
        char temp[MAX_MODULE_PATH_SIZE];
    } dentry_path;

    uint64_t character_limit = MAX_MODULE_PATH_SIZE - 1;
    uint64_t current_size = 0;

    dentry_path* dpath = (dentry_path*)std::calloc(1, sizeof(dentry_path));

    // iterate over the dentries in reverse order. for the path /usr/lib/test.so
    // this would be test.so -> usr -> lib
    osi::i_t parent = dentry("d_parent");
    do {
        auto qstr = dentry["d_name"].set_type("qstr");
        uint64_t size = qstr["size"].get32();

        // the extra 1 in the calculations is for the path separator (/)
        if ((size + current_size + 1) >= character_limit) {
            size = (character_limit - current_size - 1);
        }

        memset(dpath->temp, 0, MAX_MODULE_PATH_SIZE);
        /* Don't want to figure out if this has some useful side-effect right now. I presume it actually just leaks memory */
        auto __attribute__((unused)) qname = qstr("name").getx(dpath->temp, size);

        memset(dpath->combined, '\0', MAX_MODULE_PATH_SIZE);
        strcpy(dpath->combined, "/");
        strncat(dpath->combined, dpath->temp, size + 1);
        strncat(dpath->combined, dpath->final, current_size);

        current_size = current_size + size + 1;

        memset(dpath->final, '\0', MAX_MODULE_PATH_SIZE);
        strncpy(dpath->final, dpath->combined, current_size);

        if (current_size >= character_limit) {
            break;
        }

        dentry = parent;
        parent = dentry("d_parent");
    } while (dentry.get_address() != parent.get_address());

    strncpy(result, dpath->final, character_limit);
    std::free(dpath);
}

/*
 * PUBLIC API FOR PROCESSES
 *
 */
task* create_task_from_memory(osi::i_t task_struct)
{
    auto p = new task;
    p->address = task_struct.get_address();

    task_struct["comm"].getx(p->shortname, 16);
    sanitize_process_name(p->shortname, 16);

    p->tid = task_struct["pid"].get32();
    p->pid = task_struct["tgid"].get32();
    p->ppid = task_struct("real_parent")["tgid"].get32();
    p->createtime = task_struct["start_time"].set_type("timespec")["tv_sec"].getu();
    p->asid = task_struct.get_virtual_memory()->get_asid();

    return p;
}

uint64_t get_current_thread_address(struct LinuxKernelOSI* kosi,
                                    uint64_t kernel_stack_pointer)
{
    osi::i_t thread_info(
        kosi->system_vmem, kosi->kernel_tlib,
        GET_THREAD_INFO_FROM_ESP0(kernel_stack_pointer, kosi->details->pointer_width),
        "thread_info");
    return thread_info("task").get_address();
}

uint64_t get_current_thread_pid(struct LinuxKernelOSI* kosi,
                                uint64_t kernel_stack_pointer, uint64_t asid)
{
    auto addr = get_current_thread_address(kosi, kernel_stack_pointer);

    struct LinuxProcessOSI* posi = new struct LinuxProcessOSI;
    init_process_osi(kosi, posi, addr, asid);

    osi::i_t task(posi->vmem, posi->tlib, addr, "task_struct");
    auto tid = task["pid"].get32();

    uninit_process_osi(posi);
    delete posi;

    return tid;
}

bool init_process_osi(struct LinuxKernelOSI* kosi, struct LinuxProcessOSI* posi,
                      uint64_t process_address, uint64_t asid)
{
    osi::i_t task;
    posi->vmem = std::make_shared<VirtualMemory>(*kosi->system_vmem);
    posi->vmem->set_asid(asid);
    posi->tlib = kosi->kernel_tlib;

    try {
        task = osi::i_t(posi->vmem, posi->tlib, process_address, "task_struct");
        posi->process_address = task("group_leader").get_address();
    } catch (std::runtime_error& e) {
        task = osi::i_t(kosi->system_vmem, kosi->kernel_tlib, process_address,
                        "task_struct");
        posi->process_address = task("group_leader").get_address();
        posi->vmem->set_asid(kosi->details->initial_task_asid);
    }

    posi->kernel_proc = false;
    if (task["flags"].get32() & KERNEL_THREAD_MASK) {
        posi->kernel_proc = true;
    }
    return true;
}

void uninit_process_osi(struct LinuxProcessOSI* process_osi)
{
    process_osi->vmem.reset();
}

task* create_process(struct LinuxProcessOSI* posi)
{
    osi::i_t proc(posi->vmem, posi->tlib, posi->process_address, "task_struct");
    return create_task_from_memory(proc);
}

task* create_thread(struct LinuxProcessOSI* posi, uint64_t thread_address)
{
    osi::i_t thread(posi->vmem, posi->tlib, thread_address, "task_struct");
    return create_task_from_memory(thread);
}

uint64_t task_get_address(const task* t) { return t->address; }

uint64_t task_get_pid(const task* t) { return t->pid; }

uint64_t task_get_tid(const task* t) { return t->tid; }

uint64_t task_get_ppid(const task* t) { return t->ppid; }

const char* task_get_shortname(const task* t) { return t->shortname; }

uint64_t task_get_asid(const task* t) { return t->asid; }

uint64_t task_get_createtime(const task* t) { return t->createtime; }

void free_task(task* t)
{
    if (t) {
        delete t;
    }
}

/*
 * MEMORY REGION HELPERS
 *
 */
osi::i_t get_first_region_obj(struct LinuxProcessOSI* posi)
{
    if (posi->kernel_proc) {
        return osi::i_t();
    }
    osi::i_t task_struct(posi->vmem, posi->tlib, posi->process_address, "task_struct");

    bool success;
    auto mm = get_mm_if_exists(task_struct, &success);
    if (!success) {
        fprintf(stderr, "No memory management structs exist for %#" PRIxVMADDR "\n",
                posi->process_address);
        return osi::i_t();
    }
    return mm("mmap").set_type("vm_area_struct");
}

struct LinuxMemoryRegion* create_memory_region_from_memory(osi::i_t vm_area_struct)
{
    auto memory_region = new struct LinuxMemoryRegion();
    memory_region->address = vm_area_struct.get_address();

    auto start_addr = vm_area_struct["vm_start"].getu();
    auto end_addr = vm_area_struct["vm_end"].getu();

    memory_region->base_address = start_addr;
    memory_region->virtual_size = (end_addr - start_addr);

    memory_region->protections = vm_area_struct["vm_flags"].getu();

    if (has_pointer(vm_area_struct, "vm_file")) {
        auto file = vm_area_struct("vm_file");
        auto path = file["f_path"].set_type("path");
        get_dentry_path(path("dentry"), memory_region->path);

        get_name_from_path(memory_region->path, memory_region->name);

        auto inode = file("f_inode");
        memory_region->mtime = inode["i_mtime"].getu();
    }

    memory_region->next_area = vm_area_struct["vm_next"].getu();

    bool success;
    auto next = get_next_file_area(vm_area_struct, &success);
    if (!success) {
        memory_region->next_file = 0;
    } else {
        memory_region->next_file = next.get_address();
    }

    auto offset = vm_area_struct["vm_pgoff"].getu() << 12;
    memory_region->file_start_addr = start_addr - offset;

    return memory_region;
}

/*
 * PUBLIC API FOR MODULES
 *
 */
struct LinuxMemoryRegion* get_first_memory_region(struct LinuxProcessOSI* posi)
{
    auto vm_area = get_first_region_obj(posi);

    if (vm_area.get_address())
        return create_memory_region_from_memory(vm_area);

    return nullptr;
}

struct LinuxMemoryRegion* memory_region_next(struct LinuxProcessOSI* posi,
                                             struct LinuxMemoryRegion* mentry)
{
    if (!mentry->next_area) {
        return nullptr;
    }

    osi::i_t vm_area_struct(posi->vmem, posi->tlib, mentry->next_area, "vm_area_struct");
    return create_memory_region_from_memory(vm_area_struct);
}

struct LinuxMemoryRegion* get_first_module_entry(struct LinuxProcessOSI* posi)
{
    auto vm_area = get_first_region_obj(posi);

    if (!vm_area.get_address())
        return nullptr;

    if (!has_pointer(vm_area, "vm_file")) {
        bool success;
        vm_area = get_next_file_area(vm_area, &success);
        if (!success) {
            return nullptr;
        }
    }

    return create_memory_region_from_memory(vm_area);
}

struct LinuxMemoryRegion* module_entry_next(struct LinuxProcessOSI* posi,
                                            struct LinuxMemoryRegion* mentry)
{
    if (!mentry->next_file) {
        return nullptr;
    }

    osi::i_t vm_area_struct(posi->vmem, posi->tlib, mentry->next_file, "vm_area_struct");
    return create_memory_region_from_memory(vm_area_struct);
}

uint64_t region_get_address(const struct LinuxMemoryRegion* m) { return m->address; }

uint64_t region_get_base_address(const struct LinuxMemoryRegion* m)
{
    return m->base_address;
}

uint64_t region_get_virtual_size(const struct LinuxMemoryRegion* m)
{
    return m->virtual_size;
}

uint64_t region_get_mtime(const struct LinuxMemoryRegion* m) { return m->mtime; }

uint64_t region_get_protections(const struct LinuxMemoryRegion* m)
{
    return m->protections;
}

const char* region_get_path(const struct LinuxMemoryRegion* m) { return m->path; }

const char* region_get_name(const struct LinuxMemoryRegion* m) { return m->name; }

uint64_t region_get_file_start(const struct LinuxMemoryRegion* m)
{
    return m->file_start_addr;
}

void free_region(struct LinuxMemoryRegion* m)
{
    if (m) {
        delete m;
    }
}
