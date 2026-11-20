/*!
 * @file guest.h
 * @brief osi_isf's state and guest-memory readers, shared by the OSI provider (osi_isf.cpp), the
 * process facts (facts.cpp) and the lifecycle events (lifecycle.cpp).
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "caps.h"
#include "isf.h"

#include "panda.h"

struct hook;

namespace osiisf {

extern isf::Profile prof;
extern osicaps::Symbols syms;
extern osicaps::Plan plan;

constexpr uint64_t PAGE_SIZE_GUEST = 4096;

// Offsets from the plan: -1 where no chosen strategy declared the member.
struct Offsets {
    int64_t task_tasks, task_pid, task_tgid, task_comm, task_comm_size, task_mm, task_real_parent,
        task_group_leader, task_start_time, ts_sec, ts_nsec, ts_width;
    int64_t mm_pgd, mm_mt, mm_mmap, mm_start_brk, mm_brk, mm_start_stack;
    int64_t mt_root;
    int64_t r64_pivot, r64_slot, r64_npivot, r64_nslot;
    int64_t a64_pivot, a64_slot, a64_npivot, a64_nslot;
    int64_t vma_start, vma_end, vma_next, vma_mm, vma_flags, vma_file, vma_pgoff;
    int64_t file_dentry, dentry_parent, dentry_name;
    int64_t file_mnt, mount_mnt, mnt_parent, mnt_mountpoint, mnt_root;
    int64_t mod_list, mod_name, mod_name_size, mod_mem, mod_mem_size, mm_base, mm_size, mod_text,
        mod_core_base, mod_core_size;
    int64_t ti_task, thread_size;
};
extern Offsets off;

// Capabilities, as decided by the plan (all false until the ISF is paired with the kernel; the
// lifecycle ones also until lifecycle=true turns them on).
struct Caps {
    bool tasks, pid, name, ppid, thread_group, asid, start_time, current, threads, file_paths, mappings,
        mapping_files, mount_paths, heap_stack, modules, task_change;
    bool lifecycle, exit_codes, reparent, pid_reuse;
    bool cmdline, exe_path, fs_paths, creds, cap_sets, open_files, sockets, vma_details;
};
extern Caps cap;

bool read_mem(CPUState *cpu, uint64_t addr, void *buf, size_t len);
bool read_uint(CPUState *cpu, uint64_t addr, size_t size, uint64_t *out);
uint64_t ptr_at(CPUState *cpu, uint64_t addr);
uint64_t field_ptr(CPUState *cpu, uint64_t base, int64_t o);
uint64_t field_uint(CPUState *cpu, uint64_t base, int64_t o, size_t size);
int32_t field_int(CPUState *cpu, uint64_t base, int64_t o);
std::string read_cstr(CPUState *cpu, uint64_t addr, size_t max);
bool is_kernel_ptr(uint64_t p);

uint64_t current_task(CPUState *cpu);
uint64_t group_leader(CPUState *cpu, uint64_t ts);
std::vector<uint64_t> all_tasks(CPUState *cpu);
uint64_t start_time(CPUState *cpu, uint64_t ts);
std::vector<uint64_t> process_vmas(CPUState *cpu, uint64_t ts);
std::string dentry_name(CPUState *cpu, uint64_t dentry);

// A path from a dentry and its mount (struct vfsmount), up to root_dentry in root_mnt when
// root_dentry is given (a process's root), else to the root of the mount tree; without
// mount_paths, to the root of the dentry's own filesystem. detached (when given, with root_mnt)
// is set when the walk ended at the top of another mount tree than root_mnt's: a file of an
// internal mount (pipefs, sockfs, shmem, ...).
std::string path_of(CPUState *cpu, uint64_t dentry, uint64_t vfsmnt, uint64_t root_dentry = 0,
                    uint64_t root_mnt = 0, bool *detached = nullptr);
uint64_t file_dentry(CPUState *cpu, uint64_t file);
uint64_t file_vfsmnt(CPUState *cpu, uint64_t file);

bool paired(CPUState *cpu);
// Paired and the capability chosen; logs once why it isn't available.
bool want(CPUState *cpu, bool &c, const char *name);
uint64_t slid(uint64_t a);
void add_kernel_hook(uint64_t addr, void (*fn)(CPUState *, TranslationBlock *, struct hook *));

// facts.cpp
void facts_choose();
std::vector<uint64_t> threads_of(CPUState *cpu, uint64_t leader);
uint64_t find_task(CPUState *cpu, int32_t tid);
std::string exe_of(CPUState *cpu, uint64_t ts);   // the executable's path, else ""
std::string facts_json(CPUState *cpu, int32_t pid, uint64_t start, const std::string &which);
// The first argument of the kernel function the CPU is at the entry of.
uint64_t entry_arg0(CPUState *cpu);

// lifecycle.cpp
extern bool lifecycle_requested;
void lifecycle_init();                     // at load, when lifecycle=true: syscalls2's callbacks
void lifecycle_start(CPUState *cpu);       // once paired, when lifecycle=true
void lifecycle_task_change(CPUState *cpu); // at every task change (finish_task_switch)
void lifecycle_program_start(CPUState *cpu);   // a program started (exec)

}  // namespace osiisf
