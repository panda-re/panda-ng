/*!
 * @file caps.cpp
 * @brief osi_isf's strategies, their version ranges and inputs, and the plan that resolves them.
 */
#include "caps.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace osicaps {

/* ******************************************************************
 Versions and arches
****************************************************************** */

std::string KVer::str() const {
    if (!known()) return "unknown";
    return std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(c);
}

KVer KVer::parse(const std::string &s) {
    KVer v;
    size_t p = s.find("Linux version ");
    p = p == std::string::npos ? 0 : p + strlen("Linux version ");
    if (p >= s.size() || !isdigit((unsigned char)s[p])) return v;
    int parts[3] = {0, 0, 0};
    for (int i = 0; i < 3 && p < s.size() && isdigit((unsigned char)s[p]); i++) {
        int n = 0;
        while (p < s.size() && isdigit((unsigned char)s[p])) n = n * 10 + (s[p++] - '0');
        parts[i] = n;
        if (p < s.size() && s[p] == '.') p++;
        else break;
    }
    v.a = parts[0];
    v.b = parts[1];
    v.c = parts[2];
    return v;
}

Arch arch_from_name(const std::string &n) {
    if (n == "x86_64" || n == "amd64") return Arch::x86_64;
    if (n == "i386" || n == "x86") return Arch::i386;
    if (n == "aarch64" || n == "arm64") return Arch::aarch64;
    if (n == "arm" || n == "armel" || n == "armhf") return Arch::arm;
    if (n.rfind("mips", 0) == 0) return Arch::mips;
    if (n.rfind("riscv", 0) == 0) return Arch::riscv;
    if (n.rfind("ppc", 0) == 0 || n.rfind("powerpc", 0) == 0) return Arch::ppc;
    if (n.rfind("loongarch", 0) == 0) return Arch::loongarch;
    return Arch::unknown;
}

const char *arch_name(Arch a) {
    switch (a) {
    case Arch::x86_64: return "x86_64";
    case Arch::i386: return "i386";
    case Arch::arm: return "arm";
    case Arch::aarch64: return "aarch64";
    case Arch::mips: return "mips";
    case Arch::riscv: return "riscv";
    case Arch::ppc: return "ppc";
    case Arch::loongarch: return "loongarch";
    default: return "unknown";
    }
}

const char *status_name(Status s) {
    switch (s) {
    case Status::Available: return "available";
    case Status::Untrusted: return "available-untrusted";
    default: return "unavailable";
    }
}

/* ******************************************************************
 The strategies
****************************************************************** */

std::string Need::key() const {
    switch (kind) {
    case NeedKind::Sym: return "sym:" + a;
    case NeedKind::SymPrefix: return "sym:" + a + "*";
    case NeedKind::Field: return "field:" + a + "." + b;
    case NeedKind::Size: return "size:" + a;
    case NeedKind::ThreadSize: return "size:THREAD_SIZE";
    default: return "enum:" + a + "." + b;
    }
}

std::string Need::want() const {
    auto w = [this](const char *what) {
        return width == 0 ? std::string("pointer-sized ") + what
                          : width > 0 ? std::to_string(width) + "-byte " + what : std::string(what);
    };
    switch (kind) {
    case NeedKind::Sym: case NeedKind::SymPrefix: return "address";
    case NeedKind::Size: case NeedKind::ThreadSize: return "size";
    case NeedKind::Enum: return "value";
    default: break;
    }
    switch (shape) {
    case Shape::Ptr: return "pointer";
    case Shape::Int: return w("integer");
    case Shape::List: return "struct list_head";
    case Shape::Chars: return "char array";
    case Shape::Struct: return "struct " + tname;
    case Shape::Array: return w("element array");
    default: return "member";
    }
}

bool Strategy::for_version(const KVer &v) const {
    if (!v.known()) return true;
    if (min.known() && v.key() < min.key()) return false;
    if (max.known() && v.key() >= max.key()) return false;
    return true;
}

bool Strategy::for_arch(Arch a) const {
    if (arches.empty()) return true;
    for (Arch x : arches)
        if (x == a) return true;
    return false;
}

std::string Strategy::range() const {
    if (!min.known() && !max.known()) return "any";
    std::string lo = min.known() ? min.str() : "";
    if (!max.known()) return lo + "+";
    return (lo.empty() ? "<" : lo + "-<") + max.str();
}

namespace {

KVer V(int a, int b, int c = 0) { return KVer{a, b, c}; }
const KVer ANY;

Need Sym(const char *n, bool opt = false) {
    Need x{NeedKind::Sym, n, ""};
    x.optional = opt;
    return x;
}
Need SymPrefix(const char *n) { return Need{NeedKind::SymPrefix, n, ""}; }
Need Fld(const char *t, const char *p, Shape s, int w = -1, const char *tn = "", bool opt = false) {
    Need x{NeedKind::Field, t, p, s, w, tn};
    x.optional = opt;
    return x;
}
Need Ptr(const char *t, const char *p, bool opt = false) { return Fld(t, p, Shape::Ptr, 0, "", opt); }
Need Int(const char *t, const char *p, int w, bool opt = false) { return Fld(t, p, Shape::Int, w, "", opt); }
Need Size(const char *t) { return Need{NeedKind::Size, t, ""}; }
Need ThreadSize() { return Need{NeedKind::ThreadSize, "THREAD_SIZE", ""}; }
Need Enum(const char *e, const char *c) { return Need{NeedKind::Enum, e, c}; }

std::vector<Strategy> make_strategies() {
    using A = Arch;
    std::vector<Strategy> s;
    // The task list: init_task's tasks ring, unchanged since 2.6.
    s.push_back({"tasks", "init_task.tasks", ANY, ANY, {}, {},
                 {Sym("init_task"), Fld("task_struct", "tasks", Shape::List)},
                 "every thread group leader on init_task's tasks list"});
    // pid is the thread id and tgid the process id, both init-namespace numbers in every version
    // (pid namespaces renumber only in struct pid, which /proc in the init namespace also shows).
    s.push_back({"pid", "task_struct.pid/tgid", ANY, ANY, {}, {},
                 {Int("task_struct", "pid", 4), Int("task_struct", "tgid", 4)}, ""});
    // comm's length is the array's (TASK_COMM_LEN), read from the ISF.
    s.push_back({"name", "task_struct.comm", ANY, ANY, {}, {},
                 {Fld("task_struct", "comm", Shape::Chars)}, ""});
    s.push_back({"ppid", "real_parent->tgid", ANY, ANY, {}, {},
                 {Ptr("task_struct", "real_parent"), Int("task_struct", "tgid", 4)}, ""});
    s.push_back({"thread_group", "group_leader", ANY, ANY, {}, {},
                 {Ptr("task_struct", "group_leader")}, "a thread's process is its group leader"});
    s.push_back({"asid", "mm->pgd", ANY, ANY, {}, {},
                 {Ptr("task_struct", "mm"), Ptr("mm_struct", "pgd")},
                 "the physical address of the page directory, as the engine's current_asid"});
    // 3.17 made start_time a u64 of nanoseconds; before, a struct timespec.
    s.push_back({"start_time", "u64 nanoseconds", V(3, 17), ANY, {}, {},
                 {Int("task_struct", "start_time", 8)}, ""});
    s.push_back({"start_time", "struct timespec", V(2, 6), V(3, 17), {}, {},
                 {Fld("task_struct", "start_time", Shape::Struct, -1, "timespec"),
                  Int("timespec", "tv_sec", 0), Int("timespec", "tv_nsec", 0)},
                 "tv_sec * 1e9 + tv_nsec"});

    // A process's threads: 3.14 added signal->thread_head (task->thread_node), 6.7 removed the
    // thread_group ring. The mm is the address space's identity, shared by CLONE_VM tasks.
    s.push_back({"threads", "signal->thread_head", V(3, 14), ANY, {}, {"pid", "thread_group"},
                 {Ptr("task_struct", "signal"), Fld("signal_struct", "thread_head", Shape::List),
                  Fld("task_struct", "thread_node", Shape::List), Ptr("task_struct", "mm")},
                 "every thread of the group on its signal_struct's list"});
    s.push_back({"threads", "thread_group ring", V(2, 6), V(6, 7), {}, {"pid", "thread_group"},
                 {Fld("task_struct", "thread_group", Shape::List), Ptr("task_struct", "mm")},
                 "the group's threads on a ring through the leader"});

    // The current task, per arch and era.
    s.push_back({"current", "pcpu_hot.current_task", V(6, 2), ANY, {A::x86_64, A::i386}, {},
                 {Sym("pcpu_hot"), Ptr("pcpu_hot", "current_task"), Sym("__per_cpu_offset", true)},
                 "a per-CPU variable; __per_cpu_offset is absent on uniprocessor kernels"});
    s.push_back({"current", "current_task", V(2, 6, 30), V(6, 2), {A::x86_64, A::i386}, {},
                 {Sym("current_task|per_cpu__current_task"), Sym("__per_cpu_offset", true)},
                 "a per-CPU variable (per_cpu__current_task before 2.6.34)"});
    s.push_back({"current", "__entry_task", V(4, 10), ANY, {A::aarch64}, {},
                 {Sym("__entry_task"), Sym("__per_cpu_offset", true)},
                 "a per-CPU variable written at every context switch"});
    s.push_back({"current", "sp_el1 thread_info", V(3, 7), V(4, 10), {A::aarch64}, {},
                 {Ptr("thread_info", "task"), ThreadSize()},
                 "thread_info at the base of the kernel stack (SP_EL1), THREAD_SIZE aligned"});
    // 5.18 (CURRENT_POINTER_IN_TPIDRURO): current is in TPIDRURO in the kernel and, for a
    // reader outside it, in the per-CPU __entry_task (SMP) or __current (uniprocessor).
    s.push_back({"current", "__entry_task", V(5, 18), ANY, {A::arm}, {},
                 {Sym("__entry_task"), Sym("__per_cpu_offset", true)},
                 "a per-CPU variable written at every context switch"});
    s.push_back({"current", "__current", V(5, 18), ANY, {A::arm}, {},
                 {Sym("__current")}, "uniprocessor kernels"});
    s.push_back({"current", "svc sp thread_info", V(2, 6), V(5, 18), {A::arm}, {},
                 {Ptr("thread_info", "task"), ThreadSize()},
                 "thread_info at the base of the kernel stack (the SVC-mode sp), THREAD_SIZE aligned"});
    s.push_back({"current", "kernelsp thread_info", V(2, 6), ANY, {A::mips}, {},
                 {Sym("kernelsp"), Ptr("thread_info", "task"), ThreadSize()},
                 "kernelsp[cpu] points into the current task's stack; thread_info at its THREAD_SIZE-aligned base"});
    s.push_back({"current", "tp/sscratch", V(4, 15), ANY, {A::riscv}, {}, {},
                 "tp in the kernel, sscratch in user mode"});

    // Memory mappings: the maple tree since 6.1, the mmap list before.
    s.push_back({"mappings", "maple tree", V(6, 1), ANY, {}, {},
                 {Ptr("task_struct", "mm"), Fld("mm_struct", "mm_mt", Shape::Struct, -1, "maple_tree"),
                  Ptr("maple_tree", "ma_root"), Fld("maple_range_64", "pivot", Shape::Array, 0),
                  Fld("maple_range_64", "slot", Shape::Array, 0), Fld("maple_arange_64", "pivot", Shape::Array, 0),
                  Fld("maple_arange_64", "slot", Shape::Array, 0), Int("vm_area_struct", "vm_start", 0),
                  Int("vm_area_struct", "vm_end", 0), Ptr("vm_area_struct", "vm_mm"),
                  Int("vm_area_struct", "vm_flags", 0, true)}, ""});
    s.push_back({"mappings", "mmap list", V(2, 6), V(6, 1), {}, {},
                 {Ptr("task_struct", "mm"), Ptr("mm_struct", "mmap"), Ptr("vm_area_struct", "vm_next"),
                  Int("vm_area_struct", "vm_start", 0), Int("vm_area_struct", "vm_end", 0),
                  Ptr("vm_area_struct", "vm_mm"), Int("vm_area_struct", "vm_flags", 0, true)}, ""});
    // A file's path: the dentries' names up to the root of its filesystem (f_path since 2.6.20).
    s.push_back({"file_paths", "f_path.dentry", V(2, 6, 20), ANY, {}, {},
                 {Ptr("file", "f_path.dentry"), Ptr("dentry", "d_parent"), Ptr("dentry", "d_name.name")},
                 "the path within the file's filesystem"});
    s.push_back({"file_paths", "f_dentry", V(2, 6), V(2, 6, 20), {}, {},
                 {Ptr("file", "f_dentry"), Ptr("dentry", "d_parent"), Ptr("dentry", "d_name.name")}, ""});
    s.push_back({"mapping_files", "vm_file", ANY, ANY, {}, {"mappings", "file_paths"},
                 {Ptr("vm_area_struct", "vm_file"), Int("vm_area_struct", "vm_pgoff", 0, true)}, ""});
    // Crossing mount points: struct mount (3.3+) wraps the vfsmount; before, vfsmount had it all.
    s.push_back({"mount_paths", "struct mount", V(3, 3), ANY, {}, {"file_paths"},
                 {Ptr("file", "f_path.mnt"), Fld("mount", "mnt", Shape::Struct, -1, "vfsmount"),
                  Ptr("mount", "mnt_parent"), Ptr("mount", "mnt_mountpoint"), Ptr("vfsmount", "mnt_root")},
                 "file paths from the root of the mount tree"});
    s.push_back({"mount_paths", "vfsmount", V(2, 6, 20), V(3, 3), {}, {"file_paths"},
                 {Ptr("file", "f_path.mnt"), Ptr("vfsmount", "mnt_parent"), Ptr("vfsmount", "mnt_mountpoint"),
                  Ptr("vfsmount", "mnt_root")}, "file paths from the root of the mount tree"});
    s.push_back({"heap_stack", "mm_struct brk/start_stack", ANY, ANY, {}, {},
                 {Ptr("task_struct", "mm"), Int("mm_struct", "start_brk", 0), Int("mm_struct", "brk", 0),
                  Int("mm_struct", "start_stack", 0)}, ""});

    // Kernel modules: 6.4 split a module's memory into mem[MOD_*]; 4.5 grouped core_layout.
    s.push_back({"modules", "mem[MOD_TEXT]", V(6, 4), ANY, {}, {},
                 {Sym("modules"), Fld("module", "list", Shape::List), Fld("module", "name", Shape::Chars),
                  Fld("module", "mem", Shape::Array), Size("module_memory"), Ptr("module_memory", "base"),
                  Int("module_memory", "size", 4), Enum("mod_mem_type", "MOD_TEXT")}, ""});
    s.push_back({"modules", "core_layout", V(4, 5), V(6, 4), {}, {},
                 {Sym("modules"), Fld("module", "list", Shape::List), Fld("module", "name", Shape::Chars),
                  Ptr("module", "core_layout.base"), Int("module", "core_layout.size", 4)}, ""});
    s.push_back({"modules", "module_core", V(2, 6), V(4, 5), {}, {},
                 {Sym("modules"), Fld("module", "list", Shape::List), Fld("module", "name", Shape::Chars),
                  Ptr("module", "module_core"), Int("module", "core_size", 4)}, ""});
    s.push_back({"task_change", "finish_task_switch", ANY, ANY, {}, {"current"},
                 {SymPrefix("finish_task_switch")},
                 "a hook on finish_task_switch, which runs first in the task switched to"});
    // Where an exec is seen: a hook in the kernel's ELF loader, once the new program's mm, name,
    // executable and mappings are in place (finalize_exec where the kernel has it, just before
    // the program's registers are set; else set_binfmt, before the vDSO and the auxiliary
    // vector), else the new program's first instruction in user mode (proc_start_linux, which
    // watches every block from execve's entry until then).
    s.push_back({"program_start", "finalize_exec hook", ANY, ANY, {}, {}, {Sym("finalize_exec")},
                 "the new program is set up, just before it starts"});
    s.push_back({"program_start", "set_binfmt hook", ANY, ANY, {}, {}, {Sym("set_binfmt")},
                 "the new program and its interpreter are mapped; the vDSO and the stack's auxiliary vector are not yet"});
    s.push_back({"program_start", "proc_start_linux", ANY, ANY, {}, {}, {},
                 "the new program's first user-mode block (a block callback from execve's entry until then)"});

    // Lifecycle events from system calls (syscalls2 on the engine's pairing; lifecycle=true): the
    // creator and its clone flags at fork/vfork/clone/clone3's entry, the child at its return (or
    // as soon as the child is seen first), execve's old image at its entry and the new one when the
    // program starts.
    s.push_back({"lifecycle", "system calls", ANY, ANY, {}, {"tasks", "pid", "current", "ppid", "thread_group"},
                 {Ptr("task_struct", "real_parent")},
                 "fork, vfork, clone, clone3 and execve through syscalls2 (the engine's pairing)"});
    // Exit statuses: do_exit(code) is every task's end (signals included); its argument is the
    // wait status. signal->live reaching 0 marks the process's end.
    s.push_back({"exit_codes", "do_exit hook", ANY, ANY, {}, {"lifecycle"},
                 {Sym("do_exit"), Ptr("task_struct", "signal"), Int("signal_struct", "live.counter", 4, true)},
                 "a hook on do_exit, its first argument the wait status"});
    s.push_back({"exit_codes", "exit system calls", ANY, ANY, {}, {"lifecycle"}, {},
                 "exit and exit_group entries: voluntary exits only, not deaths by signal"});
    s.push_back({"reparent", "children at the parent's exit", ANY, ANY, {},
                 {"exit_codes", "tasks", "ppid", "task_change"}, {},
                 "an exiting process's children, checked at the following task changes"});
    s.push_back({"pid_reuse", "(pid, start_time)", ANY, ANY, {}, {"lifecycle", "start_time"}, {},
                 "a new task whose id an earlier task had, with another start time"});

    // Process facts. Arguments and environment are user memory: readable only in the current
    // address space, and only where it is paged in.
    s.push_back({"cmdline", "mm arg/env ranges", ANY, ANY, {}, {"pid", "current"},
                 {Ptr("task_struct", "mm"), Int("mm_struct", "arg_start", 0), Int("mm_struct", "arg_end", 0),
                  Int("mm_struct", "env_start", 0), Int("mm_struct", "env_end", 0)},
                 "argv and environment from the process's memory, in the current address space"});
    s.push_back({"exe_path", "mm->exe_file", V(2, 6, 26), ANY, {}, {"file_paths"},
                 {Ptr("task_struct", "mm"), Ptr("mm_struct", "exe_file")}, ""});
    // cwd and root: struct path since 2.6.25 (its lock a rwlock, then a spinlock with a seqcount,
    // 5.10's seqcount_spinlock_t), a dentry and a vfsmount before.
    s.push_back({"fs_paths", "fs_struct paths", V(2, 6, 25), ANY, {}, {"file_paths"},
                 {Ptr("task_struct", "fs"), Fld("fs_struct", "root", Shape::Struct, -1, "path"),
                  Fld("fs_struct", "pwd", Shape::Struct, -1, "path"), Ptr("path", "mnt"), Ptr("path", "dentry"),
                  Int("fs_struct", "seq.sequence|seq.seqcount.sequence", 4, true)},
                 "an odd or changed seqcount means a torn read, read again"});
    s.push_back({"fs_paths", "fs_struct dentries", V(2, 6), V(2, 6, 25), {}, {"file_paths"},
                 {Ptr("task_struct", "fs"), Ptr("fs_struct", "root"), Ptr("fs_struct", "pwd"),
                  Ptr("fs_struct", "rootmnt"), Ptr("fs_struct", "pwdmnt")}, ""});
    // Credentials: task->cred since 2.6.29 (kuid_t/kgid_t wrap them since 3.5, unless the
    // kernel was built without strict type checks before 3.14), the task's own fields before.
    s.push_back({"creds", "task->cred", V(2, 6, 29), ANY, {}, {},
                 {Ptr("task_struct", "cred"), Int("cred", "uid.val|uid", 4), Int("cred", "gid.val|gid", 4),
                  Int("cred", "suid.val|suid", 4), Int("cred", "sgid.val|sgid", 4), Int("cred", "euid.val|euid", 4),
                  Int("cred", "egid.val|egid", 4), Int("cred", "fsuid.val|fsuid", 4),
                  Int("cred", "fsgid.val|fsgid", 4)}, ""});
    s.push_back({"creds", "task_struct ids", V(2, 6), V(2, 6, 29), {}, {},
                 {Int("task_struct", "uid", 4), Int("task_struct", "gid", 4), Int("task_struct", "suid", 4),
                  Int("task_struct", "sgid", 4), Int("task_struct", "euid", 4), Int("task_struct", "egid", 4),
                  Int("task_struct", "fsuid", 4), Int("task_struct", "fsgid", 4)}, ""});
    // Capability sets: kernel_cap_t is a u64 since 6.3, u32 cap[2] from 2.6.25, a u32 before.
    s.push_back({"cap_sets", "cred u64", V(6, 3), ANY, {}, {},
                 {Ptr("task_struct", "cred"), Int("cred", "cap_effective.val", 8), Int("cred", "cap_permitted.val", 8),
                  Int("cred", "cap_inheritable.val", 8), Int("cred", "cap_bset.val", 8, true)}, ""});
    s.push_back({"cap_sets", "cred u32[]", V(2, 6, 29), V(6, 3), {}, {},
                 {Ptr("task_struct", "cred"), Fld("cred", "cap_effective.cap", Shape::Array, 4),
                  Fld("cred", "cap_permitted.cap", Shape::Array, 4), Fld("cred", "cap_inheritable.cap", Shape::Array, 4),
                  Fld("cred", "cap_bset.cap", Shape::Array, 4, "", true)}, ""});
    s.push_back({"cap_sets", "task_struct u32[]", V(2, 6, 25), V(2, 6, 29), {}, {},
                 {Fld("task_struct", "cap_effective.cap", Shape::Array, 4),
                  Fld("task_struct", "cap_permitted.cap", Shape::Array, 4),
                  Fld("task_struct", "cap_inheritable.cap", Shape::Array, 4)}, ""});
    s.push_back({"cap_sets", "task_struct u32", V(2, 6), V(2, 6, 25), {}, {},
                 {Int("task_struct", "cap_effective", 4), Int("task_struct", "cap_permitted", 4),
                  Int("task_struct", "cap_inheritable", 4)}, ""});

    // What a process touches. The fd table: files->fdt since 2.6.14. A file's kind and inode
    // number come from its dentry's inode (f_inode is 3.9+); a pseudo filesystem's name (pipefs,
    // sockfs, anon_inodefs) names the files that have no path.
    s.push_back({"open_files", "files->fdt", V(2, 6, 14), ANY, {}, {"file_paths"},
                 {Ptr("task_struct", "files"), Ptr("files_struct", "fdt"), Int("fdtable", "max_fds", 4),
                  Ptr("fdtable", "fd"), Int("file", "f_flags", 4), Int("file", "f_pos", 8),
                  Ptr("dentry", "d_inode"), Int("inode", "i_ino", 0), Int("inode", "i_mode", 2),
                  Ptr("dentry", "d_sb", true), Ptr("super_block", "s_type", true),
                  Ptr("file_system_type", "name", true)}, ""});
    // Sockets: the inet addresses and ports moved from inet_sock (num, dport, rcv_saddr, daddr;
    // inet_-prefixed from 2.6.33) into sock_common (skc_*: addresses in 2.6.38, ports later).
    auto sock_common = [](std::vector<Need> v) {
        std::vector<Need> c = {Ptr("file", "private_data"), Int("socket", "type", 2), Ptr("socket", "sk"),
                               Int("inet_sock", "sk.__sk_common.skc_family", 2),
                               Int("inet_sock", "sk.__sk_common.skc_state", 1)};
        c.insert(c.end(), v.begin(), v.end());
        for (Need n : {Ptr("unix_sock", "addr", true), Int("unix_address", "len", 4, true),
                       Fld("unix_address", "name", Shape::Any, -1, "", true),
                       Fld("sockaddr_un", "sun_path", Shape::Chars, -1, "", true),
                       Fld("inet_sock", "sk.__sk_common.skc_v6_rcv_saddr", Shape::Struct, -1, "in6_addr", true),
                       Fld("inet_sock", "sk.__sk_common.skc_v6_daddr", Shape::Struct, -1, "in6_addr", true)})
            c.push_back(n);
        return c;
    };
    s.push_back({"sockets", "sock_common", V(2, 6, 38), ANY, {}, {"open_files"},
                 sock_common({Int("inet_sock", "sk.__sk_common.skc_rcv_saddr", 4),
                              Int("inet_sock", "sk.__sk_common.skc_daddr", 4),
                              Int("inet_sock", "sk.__sk_common.skc_num", 2),
                              Int("inet_sock", "sk.__sk_common.skc_dport", 2)}),
                 "unix socket paths and IPv6 addresses where the ISF has them"});
    s.push_back({"sockets", "inet_sock inet_*", V(2, 6, 33), ANY, {}, {"open_files"},
                 sock_common({Int("inet_sock", "sk.__sk_common.skc_rcv_saddr|inet_rcv_saddr", 4),
                              Int("inet_sock", "sk.__sk_common.skc_daddr|inet_daddr", 4),
                              Int("inet_sock", "inet_num", 2), Int("inet_sock", "inet_dport", 2)}), ""});
    s.push_back({"sockets", "inet_sock", V(2, 6), V(2, 6, 33), {}, {"open_files"},
                 sock_common({Int("inet_sock", "rcv_saddr", 4), Int("inet_sock", "daddr", 4),
                              Int("inet_sock", "num", 2), Int("inet_sock", "dport", 2)}), ""});
    // VMA details: vm_flags's VM_READ/WRITE/EXEC/MAYSHARE bits (the same since 2.6) and vm_pgoff.
    s.push_back({"vma_details", "vm_flags, vm_pgoff", ANY, ANY, {}, {"mappings"},
                 {Int("vm_area_struct", "vm_flags", 0), Int("vm_area_struct", "vm_pgoff", 0),
                  Ptr("vm_area_struct", "vm_file"), Ptr("dentry", "d_inode", true), Int("inode", "i_ino", 0, true)},
                 "permissions, the file offset (in 4 KiB pages) and the file's inode"});
    return s;
}

}  // namespace

const std::vector<Strategy> &strategies() {
    static const std::vector<Strategy> s = make_strategies();
    return s;
}

const std::vector<std::string> &capability_names() {
    // Dependencies first.
    static const std::vector<std::string> c = {
        "tasks", "pid", "name", "ppid", "thread_group", "asid", "start_time", "current", "threads",
        "file_paths", "mappings", "mapping_files", "mount_paths", "heap_stack", "modules", "task_change",
        "program_start", "lifecycle", "exit_codes", "reparent", "pid_reuse", "cmdline", "exe_path", "fs_paths", "creds",
        "cap_sets", "open_files", "sockets", "vma_details"};
    return c;
}

/* ******************************************************************
 Symbols
****************************************************************** */

bool Symbols::load_file(const std::string &path, std::string &err) {
    std::ifstream f(path);
    if (!f) {
        err = "cannot open " + path;
        return false;
    }
    std::string line;
    size_t zero = 0, n = 0;
    while (std::getline(f, line)) {
        std::istringstream ls(line);
        std::string addr, type, name, mod;
        if (!(ls >> addr >> type >> name)) continue;
        if (ls >> mod) continue;  // a module's symbol ([mod] in /proc/kallsyms)
        uint64_t a = strtoull(addr.c_str(), nullptr, 16);
        n++;
        if (a == 0) {
            zero++;
            continue;
        }
        file.emplace(name, a);
    }
    if (n == 0) {
        err = path + " has no symbols (System.map or /proc/kallsyms format)";
        return false;
    }
    if (file.empty() && zero) {
        err = path + ": every address is 0 (a /proc/kallsyms read without CAP_SYSLOG)";
        return false;
    }
    file_path = path;
    if (prof) {
        for (const auto &[k, v] : file) {
            auto it = prof->symbols.find(k);
            if (it != prof->symbols.end() && it->second != v) differing++;
        }
    }
    return true;
}

std::optional<uint64_t> Symbols::get(const std::string &name, std::string *source) const {
    auto it = file.find(name);
    if (it != file.end()) {
        if (source) *source = "symbol file";
        return it->second;
    }
    if (prof) {
        if (auto a = prof->symbol(name)) {
            if (source) *source = "ISF";
            return a;
        }
    }
    return std::nullopt;
}

std::optional<uint64_t> Symbols::get_prefix(const std::string &name, std::string *source) const {
    if (auto a = get(name, source)) return a;
    for (const auto &[k, v] : file) {
        if (k.size() > name.size() && k.compare(0, name.size(), name) == 0 && k[name.size()] == '.') {
            if (source) *source = "symbol file";
            return v;
        }
    }
    if (prof) {
        if (auto a = prof->symbol_prefix(name)) {
            if (source) *source = "ISF";
            return a;
        }
    }
    return std::nullopt;
}

bool load(const Config &cfg, isf::Profile &prof, Symbols &syms, Options &opt,
          std::vector<std::string> &notes, std::string &err) {
    if (!prof.load(cfg.isf, err)) return false;
    syms.prof = &prof;
    if (prof.lifted) {
        std::string pp = cfg.provenance;
        if (pp.empty() && !prof.provenance_name.empty()) {
            size_t slash = cfg.isf.rfind('/');
            std::string dir = slash == std::string::npos ? "." : cfg.isf.substr(0, slash);
            std::string cand = dir + "/" + prof.provenance_name;
            if (std::ifstream(cand).good()) pp = cand;
        }
        if (pp.empty()) {
            notes.push_back("a kernel-lift ISF without its provenance.json (provenance=): only members in "
                            "dynamic_verified and inferred_needed count as trusted");
        } else if (!prof.load_provenance(pp, err)) {
            return false;
        } else {
            notes.push_back("provenance from " + pp);
        }
    } else if (!cfg.provenance.empty()) {
        notes.push_back("provenance= ignored: the ISF is not a kernel-lift ISF");
    }
    if (!cfg.symbols.empty()) {
        if (!syms.load_file(cfg.symbols, err)) return false;
        notes.push_back(std::to_string(syms.file.size()) + " symbols from " + cfg.symbols +
                        (syms.differing ? " (" + std::to_string(syms.differing) +
                                          " at other addresses than the ISF's; the file's are used)"
                                        : ""));
    }
    opt.arch = arch_from_name(cfg.arch);
    opt.allow_untrusted = cfg.allow_untrusted;
    opt.thread_size = cfg.thread_size;
    if (!cfg.version.empty()) {
        opt.version = KVer::parse(cfg.version);
        opt.version_source = "version=";
        if (!opt.version.known()) {
            err = "version=" + cfg.version + " is not a kernel version";
            return false;
        }
    } else if (!prof.banner.empty()) {
        opt.version = KVer::parse(prof.banner);
        if (opt.version.known()) opt.version_source = "ISF banner";
    }
    if (!opt.version.known())
        notes.push_back("no kernel version (no linux_banner in the ISF, no version=): strategies are chosen "
                        "by the ISF's contents until the guest's banner gives it");
    return true;
}

/* ******************************************************************
 Resolution
****************************************************************** */

namespace {

bool is_anon_name(const std::string &n) { return n.rfind("unnamed", 0) == 0; }

// The width of an element or member of (kind, name), or -1.
int64_t width_of(const isf::Profile &p, const std::string &kind0, const std::string &name0) {
    auto [kind, name] = p.untypedef(kind0, name0);
    if (kind == "pointer") return p.pointer_size;
    if (kind == "base") return p.base_size(name).value_or(-1);
    if (kind == "enum") return 4;
    if (kind == "struct" || kind == "union") return p.type_size(name).value_or(-1);
    return -1;
}

void resolve_field(const isf::Profile &p, Input &in) {
    const Need &n = in.need;
    // Alternative paths (a member renamed or moved across versions): the first that resolves.
    std::optional<isf::Resolved> r;
    for (size_t st = 0; !r && st <= n.b.size();) {
        size_t bar = n.b.find('|', st);
        std::string path = n.b.substr(st, bar == std::string::npos ? std::string::npos : bar - st);
        r = p.resolve(n.a, path);
        if (r && n.b.find('|') != std::string::npos) in.used = path;
        if (bar == std::string::npos) break;
        st = bar + 1;
    }
    if (!r) {
        in.problem = p.user_types.count(n.a) ? "no member " + n.b : "no type " + n.a;
        return;
    }
    in.present = true;
    in.value = r->offset;
    const isf::Field *f = r->leaf;
    auto [kind, name] = p.untypedef(f->kind, f->type_name);
    in.count = f->count;
    int64_t want = n.width == 0 ? p.pointer_size : n.width;
    std::string is = kind == "base" || kind == "struct" || kind == "union" || kind == "enum" ? kind + " " + name : kind;
    switch (n.shape) {
    case Shape::Ptr:
        in.width = p.pointer_size;
        in.shape_ok = kind == "pointer";
        break;
    case Shape::Int:
        in.width = width_of(p, kind, name);
        in.shape_ok = (kind == "base" || kind == "enum") && in.width == want;
        if (!in.shape_ok && in.width > 0 && (kind == "base" || kind == "enum"))
            is += " (" + std::to_string(in.width) + " bytes)";
        break;
    case Shape::List:
        in.width = p.type_size(name).value_or(0);
        in.shape_ok = kind == "struct" && name == "list_head";
        break;
    case Shape::Chars: {
        auto [sk, sn] = p.untypedef(f->subtype_kind, f->subtype_name);
        in.width = f->count;
        in.shape_ok = kind == "array" && f->count > 0 && sk == "base" && p.base_size(sn).value_or(0) == 1;
        if (kind == "array") is = "array of " + sn + " [" + std::to_string(f->count) + "]";
        break;
    }
    case Shape::Struct: {
        in.width = p.type_size(name).value_or(0);
        std::string alts = "|" + n.tname + "|";
        in.shape_ok = (kind == "struct" || kind == "union") && alts.find("|" + name + "|") != std::string::npos;
        break;
    }
    case Shape::Array: {
        int64_t ew = width_of(p, f->subtype_kind, f->subtype_name);
        in.width = ew;
        in.shape_ok = kind == "array" && f->count > 0 && (n.width < 0 || ew == want);
        if (kind == "array") is = "array of " + std::to_string(ew) + "-byte elements [" + std::to_string(f->count) + "]";
        break;
    }
    default:
        in.shape_ok = true;
    }
    if (!in.shape_ok) {
        in.problem = n.a + "." + (in.used.empty() ? n.b : in.used) + " is " + is + ", not a " + n.want();
        return;
    }
    // Provenance: every named hop must be trusted. kernel-lift classes the members of anonymous
    // structs and unions flattened into the named record that holds them (its anonymous types
    // are clones), so such a member is looked up in that record, at its offset there.
    std::string worst, owner;
    int64_t rel = 0;
    for (const auto &[t, m] : r->hops) {
        int64_t o = 0;
        auto ut = p.user_types.find(t);
        if (ut != p.user_types.end()) {
            auto fi = ut->second.fields.find(m);
            if (fi != ut->second.fields.end()) o = fi->second.offset;
        }
        if (!is_anon_name(t) || owner.empty()) {
            owner = t;
            rel = 0;
        }
        rel += o;
        if (is_anon_name(m)) continue;
        std::string cls = p.field_class(owner, m, rel);
        if (worst.empty() || (isf::Profile::trusted_class(worst) && !isf::Profile::trusted_class(cls))) worst = cls;
    }
    in.cls = worst.empty() ? "dwarf" : worst;
    in.trusted = isf::Profile::trusted_class(in.cls);
    if (!in.trusted) in.problem = n.a + "." + n.b + " is " + in.cls;
}

Input resolve_need(const isf::Profile &p, const Symbols &syms, const Options &o, const Need &n) {
    Input in;
    in.need = n;
    switch (n.kind) {
    case NeedKind::Sym:
    case NeedKind::SymPrefix: {
        // Alternatives split by '|' (per-CPU variables were per_cpu__<name> before 2.6.34).
        std::string src;
        std::optional<uint64_t> a;
        for (size_t st = 0; !a && st <= n.a.size();) {
            size_t bar = n.a.find('|', st);
            std::string name = n.a.substr(st, bar == std::string::npos ? std::string::npos : bar - st);
            a = n.kind == NeedKind::Sym ? syms.get(name, &src) : syms.get_prefix(name, &src);
            if (bar == std::string::npos) break;
            st = bar + 1;
        }
        if (!a) {
            in.problem = "no symbol " + n.a;
            break;
        }
        in.present = in.shape_ok = in.trusted = true;
        in.value = (int64_t)*a;
        in.cls = src;
        break;
    }
    case NeedKind::Field:
        resolve_field(p, in);
        break;
    case NeedKind::Size: {
        auto sz = p.type_size(n.a);
        if (!sz || *sz <= 0) {
            in.problem = "no type " + n.a;
            break;
        }
        in.present = in.shape_ok = true;
        in.value = *sz;
        // kernel-lift verifies members, not record sizes or constants.
        in.cls = p.lifted ? "reference-carried" : "dwarf";
        in.trusted = !p.lifted;
        if (!in.trusted) in.problem = "sizeof(" + n.a + ") is reference-carried";
        break;
    }
    case NeedKind::ThreadSize: {
        // sizeof(union thread_union), else init_stack[] (4.16+, where thread_union may be gone).
        // thread_size= (e.g. kernel-lift's modabi, from the kernel's own code) comes first.
        int64_t sz = o.thread_size > 0 ? o.thread_size : p.type_size("thread_union").value_or(0);
        std::string from = o.thread_size > 0 ? "argument" : "thread_union";
        auto is = p.symbol_arrays.find("init_stack");
        if (sz <= 0 && is != p.symbol_arrays.end() && is->second.first > 0) {
            sz = is->second.first * p.base_size(is->second.second).value_or(0);
            from = "init_stack";
        }
        if (sz <= 0 || (sz & (sz - 1))) {
            in.problem = sz ? "THREAD_SIZE " + std::to_string(sz) + " is not a power of two"
                            : "no thread_union type or init_stack array (THREAD_SIZE)";
            break;
        }
        in.present = in.shape_ok = true;
        in.value = sz;
        in.cls = from == "argument" ? "argument" : p.lifted ? "reference-carried" : "dwarf";
        in.trusted = from == "argument" || !p.lifted;
        if (!in.trusted) in.problem = "THREAD_SIZE (sizeof " + from + ") is reference-carried";
        break;
    }
    case NeedKind::Enum: {
        auto v = p.enum_value(n.a, n.b);
        if (!v) {
            in.problem = "no enum constant " + n.a + "." + n.b;
            break;
        }
        in.present = in.shape_ok = true;
        in.value = *v;
        in.cls = p.lifted ? "reference-carried" : "dwarf";
        in.trusted = !p.lifted;
        if (!in.trusted) in.problem = n.b + " is reference-carried";
        break;
    }
    }
    return in;
}

}  // namespace

void Plan::resolve(const isf::Profile &prof, const Symbols &syms, const Options &o) {
    opt = o;
    isf_kind = prof.lifted ? "kernel-lift" : "dwarf";
    caps.clear();
    values.clear();
    std::map<std::string, Input> cache;
    auto get = [&](const Need &n) {
        // The same member can be wanted in different shapes (start_time: u64 or timespec).
        std::string k = n.key() + "|" + std::to_string((int)n.shape) + "|" + std::to_string(n.width) + "|" + n.tname;
        auto it = cache.find(k);
        if (it == cache.end()) it = cache.emplace(k, resolve_need(prof, syms, o, n)).first;
        Input in = it->second;
        in.need = n;   // the same input can be optional in one strategy and required in another
        return in;
    };
    for (const std::string &name : capability_names()) {
        CapResult cr;
        cr.cap = name;
        bool any_for_arch = false, chosen = false;
        const Attempt *fallback = nullptr;  // usable only with untrusted inputs
        for (const Strategy &st : strategies()) {
            if (st.cap != name || !st.for_arch(o.arch)) continue;
            any_for_arch = true;
            Attempt at{&st, {}, ""};
            for (const Need &n : st.needs) at.inputs.push_back(get(n));
            std::vector<std::string> missing, untrusted;
            for (const Input &in : at.inputs) {
                if (in.need.optional) continue;
                if (!in.present || !in.shape_ok) missing.push_back(in.problem);
                else if (!in.trusted) untrusted.push_back(in.problem);
            }
            for (const std::string &rc : st.requires_caps) {
                const CapResult *dep = cap(rc);
                if (!dep || dep->status == Status::Unavailable) missing.push_back("needs " + rc);
            }
            bool in_range = st.for_version(o.version);
            auto join = [](const std::vector<std::string> &v) {
                std::string s;
                for (const auto &x : v) s += (s.empty() ? "" : "; ") + x;
                return s;
            };
            if (!missing.empty()) {
                at.outcome = "missing: " + join(missing);
            } else if (!in_range) {
                at.outcome = "resolves, but is for " + st.range() + ", not " + o.version.str();
            } else if (!untrusted.empty()) {
                at.outcome = "untrusted: " + join(untrusted);
            } else {
                at.outcome = chosen ? "usable (another was chosen)" : "chosen";
            }
            cr.attempts.push_back(at);
            if (at.outcome == "chosen") {
                chosen = true;
                if (!o.attempt_all) break;
            }
        }
        // The chosen strategy, else the first in range whose only problem is trust.
        for (Attempt &at : cr.attempts) {
            if (at.outcome == "chosen") {
                cr.status = Status::Available;
                cr.strategy = at.strategy;
                cr.inputs = at.inputs;
                break;
            }
            if (!fallback && at.outcome.rfind("untrusted", 0) == 0) fallback = &at;
        }
        if (cr.status == Status::Unavailable && fallback && o.allow_untrusted) {
            cr.status = Status::Untrusted;
            cr.strategy = fallback->strategy;
            cr.inputs = fallback->inputs;
            for (Attempt &at : cr.attempts)
                if (&at == fallback) at.outcome = "chosen (" + at.outcome + ")";
        }
        if (cr.status == Status::Unavailable) {
            if (!any_for_arch) {
                cr.reason = std::string("no strategy for ") + arch_name(o.arch);
            } else if (fallback) {
                cr.reason = fallback->strategy->name + ": " + fallback->outcome.substr(strlen("untrusted: ")) +
                            " (untrusted; allow_untrusted=true uses it)";
            } else {
                for (const Attempt &at : cr.attempts) {
                    if (!at.strategy->for_version(o.version) && at.outcome.rfind("resolves", 0) != 0) continue;
                    cr.reason += (cr.reason.empty() ? "" : " | ") + at.strategy->name + ": " + at.outcome;
                }
                if (cr.reason.empty()) {
                    for (const Attempt &at : cr.attempts)
                        cr.reason += (cr.reason.empty() ? "" : " | ") + at.strategy->name + ": " + at.outcome;
                }
            }
        } else {
            for (const Input &in : cr.inputs) {
                if (in.usable(cr.status == Status::Untrusted)) {
                    values.emplace(in.need.key(), in);
                } else if (in.need.optional) {
                    cr.degraded.push_back(in.problem);
                }
            }
        }
        caps.push_back(cr);
    }
}

const CapResult *Plan::cap(const std::string &name) const {
    for (const CapResult &c : caps)
        if (c.cap == name) return &c;
    return nullptr;
}

bool Plan::ok(const std::string &name) const {
    const CapResult *c = cap(name);
    return c && c->status != Status::Unavailable;
}

const std::string &Plan::why(const std::string &name) const {
    static const std::string none = "unknown capability";
    const CapResult *c = cap(name);
    return c ? c->reason : none;
}

const std::string &Plan::strategy(const std::string &name) const {
    static const std::string none;
    const CapResult *c = cap(name);
    return c && c->strategy ? c->strategy->name : none;
}

const Input *Plan::value(const std::string &key) const {
    auto it = values.find(key);
    return it == values.end() ? nullptr : &it->second;
}

int64_t Plan::off(const std::string &type, const std::string &path) const {
    const Input *in = value("field:" + type + "." + path);
    return in ? in->value : -1;
}

int64_t Plan::count(const std::string &type, const std::string &path) const {
    const Input *in = value("field:" + type + "." + path);
    return in ? in->count : 0;
}

int64_t Plan::width(const std::string &type, const std::string &path) const {
    const Input *in = value("field:" + type + "." + path);
    return in ? in->width : 0;
}

uint64_t Plan::sym(const std::string &name) const {
    const Input *in = value("sym:" + name);
    if (!in) in = value("sym:" + name + "*");
    if (!in) {
        // A symbol with alternatives ("current_task|per_cpu__current_task").
        auto it = values.lower_bound("sym:" + name + "|");
        if (it != values.end() && it->first.rfind("sym:" + name + "|", 0) == 0) in = &it->second;
    }
    return in ? (uint64_t)in->value : 0;
}

int64_t Plan::size(const std::string &type) const {
    const Input *in = value("size:" + type);
    return in ? in->value : 0;
}

int64_t Plan::enum_value(const std::string &e, const std::string &c) const {
    const Input *in = value("enum:" + e + "." + c);
    return in ? in->value : -1;
}

namespace {

nlohmann::json input_json(const Input &in) {
    nlohmann::json j;
    j["input"] = in.need.key().substr(in.need.key().find(':') + 1);
    j["kind"] = in.need.key().substr(0, in.need.key().find(':'));
    j["want"] = in.need.want();
    if (in.need.optional) j["optional"] = true;
    if (in.present) {
        j["value"] = in.value;
        if (in.need.kind == NeedKind::Field) {
            j["width"] = in.width;
            if (in.count) j["count"] = in.count;
        }
    }
    if (!in.cls.empty()) j["class"] = in.cls;
    if (!in.used.empty()) j["used"] = in.used;
    j["ok"] = in.present && in.shape_ok;
    j["trusted"] = in.trusted;
    if (!in.problem.empty()) j["problem"] = in.problem;
    return j;
}

}  // namespace

std::string Plan::json() const {
    nlohmann::json j;
    j["arch"] = arch_name(opt.arch);
    j["version"] = opt.version.str();
    j["version_source"] = opt.version_source.empty() ? "none" : opt.version_source;
    j["isf_kind"] = isf_kind;
    j["allow_untrusted"] = opt.allow_untrusted;
    nlohmann::json cj = nlohmann::json::object();
    for (const CapResult &c : caps) {
        nlohmann::json e;
        e["status"] = status_name(c.status);
        if (c.strategy) {
            e["strategy"] = c.strategy->name;
            e["versions"] = c.strategy->range();
        }
        if (!c.reason.empty()) e["reason"] = c.reason;
        if (!c.degraded.empty()) e["degraded"] = c.degraded;
        e["inputs"] = nlohmann::json::array();
        for (const Input &in : c.inputs) e["inputs"].push_back(input_json(in));
        e["attempts"] = nlohmann::json::array();
        for (const Attempt &at : c.attempts) {
            nlohmann::json a;
            a["strategy"] = at.strategy->name;
            a["versions"] = at.strategy->range();
            a["outcome"] = at.outcome;
            a["inputs"] = nlohmann::json::array();
            for (const Input &in : at.inputs) a["inputs"].push_back(input_json(in));
            e["attempts"].push_back(a);
        }
        cj[c.cap] = e;
    }
    j["capabilities"] = cj;
    return j.dump(1);
}

std::vector<std::string> Plan::lines() const {
    std::vector<std::string> out;
    for (const CapResult &c : caps) {
        std::string l = c.cap + ": ";
        if (c.status == Status::Unavailable) {
            l += "unavailable (" + c.reason + ")";
        } else {
            l += std::string(c.status == Status::Untrusted ? "available from untrusted inputs" : "available") +
                 " (" + c.strategy->name + ", " + c.strategy->range() + ")";
            for (const std::string &d : c.degraded) l += "; without " + d;
        }
        out.push_back(l);
    }
    return out;
}

}  // namespace osicaps
