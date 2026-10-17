/*
 * Penguin OSI: the guest kernel's process state, read by the host
 *
 * Processes, open files and memory mappings, from the kernel's own data
 * structures (task_struct, cred, mm_struct and its maple tree or VMA list,
 * files_struct/fdtable, dentry, mount), read from guest memory.  No guest
 * code runs.  The layout (offsets, symbols) comes from the kernel's ISF,
 * through penguin_osi_set_layout(); the walks are the kernel's:
 * fs/d_path.c's d_path() for names (d_dname for sockets, pipes, anon
 * inodes; " (deleted)"), fs/proc/task_mmu.c's for mappings.
 *
 * Kernel memory never faults: a read of it is a TLB lookup, or a page walk.
 * Each call reads the kernel as it is at that moment, and keeps the pages
 * it touched for the length of the call.  A task's user memory (arguments,
 * environment) is read through the task's page tables: a page that is not
 * present fails the read (no fault is taken here; the syscall engine's
 * commands take one for the current task).
 *
 * The same walks serve the syscall engine's filters (penguin-syscalls.h:
 * exe_filter, PENGUIN_SC_SUBJECT_FD_PATH) and Penguin's OSI calls.  Any
 * thread; the vCPU must not run during a call (callbacks, or a stopped VM).
 * 64-bit little-endian kernels (x86_64, AArch64).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef SYSTEM_PENGUIN_OSI_H
#define SYSTEM_PENGUIN_OSI_H

#include "qemu/typedefs.h"

#define PENGUIN_OSI_AT_FDCWD (-100)

/*
 * Byte offsets in the kernel's structures (-1: this kernel has no such
 * field) and kernel addresses (0: none).  Nested fields are flattened:
 * dentry_name_len is offsetof(struct dentry, d_name.len).
 */
struct penguin_osi_layout {
    /* task_struct */
    int32_t task_pid, task_tgid, task_comm, task_real_parent, task_cred;
    int32_t task_mm, task_fs, task_files, task_start_time, task_tasks;
    int32_t task_signal, task_thread_node;  /* 6.7+: signal->thread_head */
    int32_t task_thread_group;              /* before */
    int32_t signal_thread_head;
    int32_t cred_uid, cred_gid, cred_euid, cred_egid;
    /* mm_struct */
    int32_t mm_pgd, mm_map_count, mm_start_code, mm_end_code, mm_start_data;
    int32_t mm_end_data, mm_start_brk, mm_brk, mm_start_stack, mm_arg_start;
    int32_t mm_arg_end, mm_env_start, mm_env_end, mm_mmap_base, mm_task_size;
    int32_t mm_exe_file;
    int32_t mm_mt_root;                     /* 6.1+: mm_mt.ma_root */
    int32_t mm_mmap;                        /* before: the VMA list */
    int32_t mm_context_flags;               /* x86: MM_CONTEXT_HAS_VSYSCALL */
    /* fs_struct, files */
    int32_t fs_root_mnt, fs_root_dentry, fs_pwd_mnt, fs_pwd_dentry;
    int32_t files_fdt, fdt_max_fds, fdt_fd;
    int32_t file_mnt, file_dentry;          /* f_path.mnt, f_path.dentry */
    /* dentry, inode, mounts */
    int32_t dentry_parent, dentry_name_len, dentry_name_name, dentry_op;
    int32_t dentry_inode, dentry_hash_pprev;
    int32_t inode_ino;
    int32_t mount_mnt, mount_parent, mount_mountpoint, vfsmount_root;
    int32_t dops_dname;
    /* vm_area_struct */
    int32_t vma_start, vma_end, vma_flags, vma_pgoff, vma_file, vma_ops;
    int32_t vma_private, vma_mm, vma_next;
    int32_t vmops_name, special_mapping_name;
    /* maple_node: range_64 (leaves too) and arange_64 */
    int32_t mr64_pivot, mr64_slot, mr64_meta_end, mr64_pivots;
    int32_t ma64_pivot, ma64_slot, ma64_meta_end, ma64_pivots;
    /* symbols */
    uint64_t init_task, gate_vma;
    uint64_t fn_sockfs_dname, fn_pipefs_dname, fn_anon_inodefs_dname;
    uint64_t fn_simple_dname, fn_dmabuffs_dname;
    uint64_t fn_special_mapping_name, fn_gate_vma_name;
    /*
     * Page tables, for another task's user memory, through the direct map.
     * The kernel's variables, read when first needed (the kernel sets them
     * at boot): x86_64's page_offset_base and __pgtable_l5_enabled (0: the
     * 4-level default base); AArch64's vabits_actual (0: 48) and
     * memstart_addr.
     */
    uint64_t pt_page_offset_base, pt_l5_enabled;
    uint64_t pt_vabits_actual, pt_memstart_addr;
    bool pt_x86;
    /*
     * The kernel image (_text.._end) and the variables that place it in
     * physical memory: x86_64's phys_base, AArch64's kimage_voffset.  With
     * the direct map's, kernel addresses there translate without a page
     * walk.
     */
    uint64_t kimg_start, kimg_end, pt_phys_base, pt_kimage_voffset;
    /* enum maple_type's values (lib/maple_tree.c node encoding) */
    int32_t maple_leaf_64, maple_range_64, maple_arange_64;
    /*
     * Where /proc's rules changed with no structure to show it: the kernel's
     * release string (init_uts_ns.name.release: 6.6 made the heap's bounds
     * strict), and stack_guard_gap (4.12, and the stable kernels the fix
     * went to: before it, /proc hid a stack's guard page).  A 4.12+ release
     * has the gap whether or not the symbol is known.
     */
    uint64_t uts_release, stack_guard_gap;
    /*
     * With a d_dname function above unknown (a kernel lifted with exports
     * only), the filesystem's magic picks the name it would have printed:
     * dentry.d_sb, super_block.s_magic.
     */
    int32_t dentry_sb, sb_magic;
};

struct penguin_osi_proc {
    uint64_t task, mm, pgd;
    int64_t pid, tgid, ppid;                /* ppid: real_parent's tgid */
    uint64_t create_time;                   /* start_time, ns */
    uint32_t uid, gid, euid, egid;
    char comm[16];
    uint64_t map_count, start_brk, brk, start_stack, start_code, end_code;
    uint64_t start_data, end_data, arg_start, arg_end, env_start, env_end;
    uint64_t mmap_base, task_size;
};

/* names: offsets into the caller's string buffer, NUL-terminated */
struct penguin_osi_fd {
    int32_t fd;
    uint32_t name;
    uint64_t file;
};

struct penguin_osi_vma {
    uint64_t start, end, flags, offset;     /* offset: of the file, or 0 */
    uint64_t vma, file;
    uint32_t name;
    char perms[5];                          /* "rwxp", as /proc */
};

bool penguin_osi_set_layout(const struct penguin_osi_layout *layout);

/* the task with this pid (a thread id), or 0 */
uint64_t penguin_osi_task(CPUState *cs, int64_t pid);
bool penguin_osi_proc(CPUState *cs, uint64_t task, struct penguin_osi_proc *out);

/*
 * Every process (thread group leader), as init_task's list has them:
 * fills @max of @tasks and returns how many there are (-1: unreadable).
 */
int penguin_osi_procs(CPUState *cs, uint64_t *tasks, int max);

/*
 * The task's open files from fd @start (PENGUIN_OSI_AT_FDCWD: its working
 * directory alone), at most @max, with their d_path() names in @strs.
 * Returns how many were filled, or -1.
 */
int penguin_osi_fds(CPUState *cs, uint64_t task, int32_t start,
                    struct penguin_osi_fd *out, int max,
                    char *strs, uint32_t slen);

/* one name, into @buf; its length, or -1 (no such fd, unreadable) */
int penguin_osi_fd_path(CPUState *cs, uint64_t task, int32_t fd,
                        char *buf, uint32_t len);
int penguin_osi_exe_path(CPUState *cs, uint64_t task, char *buf, uint32_t len);

/*
 * The task's mappings in address order, named as /proc/<pid>/maps names
 * them (names in @strs).  Returns how many there are (-1: unreadable);
 * fills at most @max.
 */
int penguin_osi_mappings(CPUState *cs, uint64_t task,
                         struct penguin_osi_vma *out, int max,
                         char *strs, uint32_t slen);

/* how a name is matched: as the syscall filters' string types */
enum penguin_osi_match {
    PENGUIN_OSI_MATCH_EXACT,
    PENGUIN_OSI_MATCH_CONTAINS,
    PENGUIN_OSI_MATCH_STARTSWITH,
    PENGUIN_OSI_MATCH_ENDSWITH,
};

/*
 * Whether the name of the task's fd @fd, or of its executable (mm->exe_file),
 * matches @pat (@plen bytes): 1 yes, 0 no, -1 no such file or unreadable.
 * The name is built from its last component up, as d_path() builds it: an
 * exact or ends-with match stops at the first component that differs, so
 * a miss usually costs the file's and its dentry's reads alone.
 */
int penguin_osi_fd_path_match(CPUState *cs, uint64_t task, int32_t fd,
                              int how, const char *pat, uint32_t plen);
int penguin_osi_exe_match(CPUState *cs, uint64_t task, int how,
                          const char *pat, uint32_t plen);

/*
 * The task's user memory at @va, through its page tables, without faults:
 * how many bytes were read before a page that is not present (-1: none).
 */
int penguin_osi_user_read(CPUState *cs, uint64_t task, uint64_t va,
                          void *buf, uint32_t len);

/*
 * Kernel memory by the kernel's own direct map and image (no TLB, no page
 * walk): false if @va..@va+@len is not all there, or the map is not known
 * yet (then read it another way).  The syscall engine's reads of the
 * current task use it.
 */
bool penguin_osi_kread(CPUState *cs, uint64_t va, void *buf, uint32_t len);

/*
 * Instrumentation, off until the first call; every thread's (exact with
 * one vCPU): page lookups, [0] by the kernel's own map (direct map,
 * image), [1] the call's cache, [2] the vCPU's TLB, [3] a page walk and
 * copy; [4] the matchers' calls and [5] the host ticks spent in them;
 * [6] the host's tick counter now.  @out has 7.
 */
void penguin_osi_counters(uint64_t *out);

#endif /* SYSTEM_PENGUIN_OSI_H */
