/* PANDABEGINCOMMENT
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 * See the COPYING file in the top-level directory.
 *
PANDAENDCOMMENT */
/*!
 * @file facts.cpp
 * @brief Facts about a process: its threads and address space, arguments and environment,
 * executable, cwd and root, credentials and capability sets, open files (with sockets and pipes)
 * and memory map, each from its capability's chosen strategy (caps.cpp). A fact whose capability
 * isn't available, or whose memory can't be read, is reported unavailable with the reason.
 */
#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <sstream>

#include <nlohmann/json.hpp>

#include "guest.h"

namespace osiisf {

namespace {

constexpr int MAX_THREADS = 1 << 16;
constexpr int MAX_FDS = 1 << 16;
constexpr uint64_t MAX_ARGS_BYTES = 256 * 1024;

// Offsets of the facts' strategies; -1 where no chosen strategy declared the member.
struct {
    int64_t task_signal, sig_thread_head, task_thread_node, task_thread_group;
    bool thread_head;
    int64_t mm_arg_start, mm_arg_end, mm_env_start, mm_env_end, mm_exe_file;
    int64_t task_fs, fs_root, fs_pwd, path_mnt, path_dentry, fs_seq, fs_rootmnt, fs_pwdmnt;
    bool fs_paths_struct;
    int64_t task_cred, ids[8];
    bool ids_in_task;
    enum { CapsNone, CapsU64, CapsU32Array, CapsU32 } caps_kind;
    int64_t caps[4], caps_count[4];
    bool caps_in_task;
    int64_t task_files, files_fdt, fdt_max_fds, fdt_fd, file_flags, file_pos, dentry_inode, inode_ino,
        inode_ino_width, inode_mode, dentry_sb, sb_type, fstype_name;
    int64_t file_private, sock_type, sock_sk, sk_family, sk_state, sk_rcv_saddr, sk_daddr, sk_num, sk_dport,
        sk_v6_rcv, sk_v6_daddr, unix_addr, uaddr_len, uaddr_name, sun_path;
} fo;

const char *const ID_NAMES[8] = {"uid", "gid", "suid", "sgid", "euid", "egid", "fsuid", "fsgid"};
const char *const CAP_NAMES[4] = {"effective", "permitted", "inheritable", "bounding"};
const char *const CAP_MEMBERS[4] = {"cap_effective", "cap_permitted", "cap_inheritable", "cap_bset"};

nlohmann::json unavailable(const std::string &why) { return {{"unavailable", why}}; }

// The capability's reason, for a fact it doesn't give.
nlohmann::json no_cap(const char *c) {
    if (!plan.ok(c)) return unavailable(std::string(c) + ": " + plan.why(c));
    return unavailable(std::string(c) + ": not available");
}

std::string hex(uint64_t v) {
    char b[24];
    snprintf(b, sizeof(b), "0x%" PRIx64, v);
    return b;
}

}  // namespace

void facts_choose() {
    auto o = [](const char *t, const char *p) { return plan.off(t, p); };
    fo.task_signal = o("task_struct", "signal");
    fo.thread_head = plan.strategy("threads") == "signal->thread_head";
    fo.sig_thread_head = o("signal_struct", "thread_head");
    fo.task_thread_node = o("task_struct", "thread_node");
    fo.task_thread_group = o("task_struct", "thread_group");

    fo.mm_arg_start = o("mm_struct", "arg_start");
    fo.mm_arg_end = o("mm_struct", "arg_end");
    fo.mm_env_start = o("mm_struct", "env_start");
    fo.mm_env_end = o("mm_struct", "env_end");
    fo.mm_exe_file = o("mm_struct", "exe_file");

    fo.task_fs = o("task_struct", "fs");
    fo.fs_paths_struct = plan.strategy("fs_paths") == "fs_struct paths";
    fo.fs_root = o("fs_struct", "root");
    fo.fs_pwd = o("fs_struct", "pwd");
    fo.path_mnt = o("path", "mnt");
    fo.path_dentry = o("path", "dentry");
    fo.fs_seq = o("fs_struct", "seq.sequence|seq.seqcount.sequence");
    fo.fs_rootmnt = o("fs_struct", "rootmnt");
    fo.fs_pwdmnt = o("fs_struct", "pwdmnt");

    fo.ids_in_task = plan.strategy("creds") == "task_struct ids";
    fo.task_cred = o("task_struct", "cred");
    for (int i = 0; i < 8; i++) {
        std::string n = ID_NAMES[i];
        fo.ids[i] = fo.ids_in_task ? o("task_struct", n.c_str()) : o("cred", (n + ".val|" + n).c_str());
    }
    const std::string &cs = plan.strategy("cap_sets");
    fo.caps_kind = cs == "cred u64" ? fo.CapsU64 : cs == "cred u32[]" || cs == "task_struct u32[]" ? fo.CapsU32Array
                 : cs == "task_struct u32" ? fo.CapsU32 : fo.CapsNone;
    fo.caps_in_task = cs.rfind("task_struct", 0) == 0;
    for (int i = 0; i < 4; i++) {
        std::string m = CAP_MEMBERS[i];
        const char *t = fo.caps_in_task ? "task_struct" : "cred";
        std::string path = fo.caps_kind == fo.CapsU64 ? m + ".val" : fo.caps_kind == fo.CapsU32Array ? m + ".cap" : m;
        fo.caps[i] = plan.off(t, path);
        fo.caps_count[i] = plan.count(t, path);
    }

    fo.task_files = o("task_struct", "files");
    fo.files_fdt = o("files_struct", "fdt");
    fo.fdt_max_fds = o("fdtable", "max_fds");
    fo.fdt_fd = o("fdtable", "fd");
    fo.file_flags = o("file", "f_flags");
    fo.file_pos = o("file", "f_pos");
    fo.dentry_inode = o("dentry", "d_inode");
    fo.inode_ino = o("inode", "i_ino");
    fo.inode_ino_width = plan.width("inode", "i_ino");
    fo.inode_mode = o("inode", "i_mode");
    fo.dentry_sb = o("dentry", "d_sb");
    fo.sb_type = o("super_block", "s_type");
    fo.fstype_name = o("file_system_type", "name");

    fo.file_private = o("file", "private_data");
    fo.sock_type = o("socket", "type");
    fo.sock_sk = o("socket", "sk");
    fo.sk_family = o("inet_sock", "sk.__sk_common.skc_family");
    fo.sk_state = o("inet_sock", "sk.__sk_common.skc_state");
    const std::string &ss = plan.strategy("sockets");
    if (ss == "sock_common") {
        fo.sk_rcv_saddr = o("inet_sock", "sk.__sk_common.skc_rcv_saddr");
        fo.sk_daddr = o("inet_sock", "sk.__sk_common.skc_daddr");
        fo.sk_num = o("inet_sock", "sk.__sk_common.skc_num");
        fo.sk_dport = o("inet_sock", "sk.__sk_common.skc_dport");
    } else if (ss == "inet_sock inet_*") {
        fo.sk_rcv_saddr = o("inet_sock", "sk.__sk_common.skc_rcv_saddr|inet_rcv_saddr");
        fo.sk_daddr = o("inet_sock", "sk.__sk_common.skc_daddr|inet_daddr");
        fo.sk_num = o("inet_sock", "inet_num");
        fo.sk_dport = o("inet_sock", "inet_dport");
    } else {
        fo.sk_rcv_saddr = o("inet_sock", "rcv_saddr");
        fo.sk_daddr = o("inet_sock", "daddr");
        fo.sk_num = o("inet_sock", "num");
        fo.sk_dport = o("inet_sock", "dport");
    }
    fo.sk_v6_rcv = o("inet_sock", "sk.__sk_common.skc_v6_rcv_saddr");
    fo.sk_v6_daddr = o("inet_sock", "sk.__sk_common.skc_v6_daddr");
    fo.unix_addr = o("unix_sock", "addr");
    fo.uaddr_len = o("unix_address", "len");
    fo.uaddr_name = o("unix_address", "name");
    fo.sun_path = o("sockaddr_un", "sun_path");
}

/* ******************************************************************
 Threads and tasks
****************************************************************** */

std::vector<uint64_t> threads_of(CPUState *cpu, uint64_t leader) {
    std::vector<uint64_t> out;
    if (!cap.threads || !leader) return out;
    if (fo.thread_head) {
        uint64_t head = field_ptr(cpu, leader, fo.task_signal);
        if (!head) return out;
        head += fo.sig_thread_head;
        for (uint64_t n = ptr_at(cpu, head); n && n != head && is_kernel_ptr(n) && out.size() < MAX_THREADS;
             n = ptr_at(cpu, n))
            out.push_back(n - fo.task_thread_node);
    } else {
        uint64_t head = leader + fo.task_thread_group;
        out.push_back(leader);
        for (uint64_t n = ptr_at(cpu, head); n && n != head && is_kernel_ptr(n) && out.size() < MAX_THREADS;
             n = ptr_at(cpu, n))
            out.push_back(n - fo.task_thread_group);
    }
    return out;
}

// The task with this id (a thread's own id; a process's id is its leader's).
uint64_t find_task(CPUState *cpu, int32_t tid) {
    if (!cap.tasks || !cap.pid) return 0;
    for (uint64_t ts : all_tasks(cpu)) {
        if (field_int(cpu, ts, off.task_pid) == tid) return ts;
        if (!cap.threads) continue;
        for (uint64_t t : threads_of(cpu, ts))
            if (field_int(cpu, t, off.task_pid) == tid) return t;
    }
    return 0;
}

uint64_t find_process(CPUState *cpu, int32_t pid) {
    if (!cap.tasks || !cap.pid) return 0;
    for (uint64_t ts : all_tasks(cpu))
        if (field_int(cpu, ts, off.task_tgid) == pid && field_int(cpu, ts, off.task_pid) == pid) return ts;
    return 0;
}

/* ******************************************************************
 Paths
****************************************************************** */

namespace {

struct Root {
    uint64_t dentry = 0, mnt = 0;
};

// The process's root (fs_struct), to end paths there as /proc does.
Root root_of(CPUState *cpu, uint64_t ts) {
    Root r;
    if (!cap.fs_paths) return r;
    uint64_t fs = field_ptr(cpu, ts, fo.task_fs);
    if (!fs) return r;
    if (fo.fs_paths_struct) {
        r.dentry = field_ptr(cpu, fs + fo.fs_root, fo.path_dentry);
        r.mnt = field_ptr(cpu, fs + fo.fs_root, fo.path_mnt);
    } else {
        r.dentry = field_ptr(cpu, fs, fo.fs_root);
        r.mnt = field_ptr(cpu, fs, fo.fs_rootmnt);
    }
    return r;
}

uint64_t inode_of(CPUState *cpu, uint64_t dentry) { return field_ptr(cpu, dentry, fo.dentry_inode); }

std::string fs_type(CPUState *cpu, uint64_t dentry) {
    if (fo.dentry_sb < 0 || fo.sb_type < 0 || fo.fstype_name < 0) return "";
    uint64_t t = field_ptr(cpu, field_ptr(cpu, dentry, fo.dentry_sb), fo.sb_type);
    return read_cstr(cpu, field_ptr(cpu, t, fo.fstype_name), 64);
}

const char *mode_kind(uint64_t mode) {
    switch (mode & 0170000) {
    case 0140000: return "socket";
    case 0120000: return "symlink";
    case 0100000: return "file";
    case 0060000: return "block";
    case 0040000: return "dir";
    case 0020000: return "char";
    case 0010000: return "fifo";
    default: return "unknown";
    }
}

}  // namespace

std::string exe_of(CPUState *cpu, uint64_t ts) {
    if (!cap.exe_path) return "";
    uint64_t f = field_ptr(cpu, field_ptr(cpu, ts, off.task_mm), fo.mm_exe_file);
    if (!f) return "";
    return path_of(cpu, file_dentry(cpu, f), file_vfsmnt(cpu, f));
}

/* ******************************************************************
 The facts
****************************************************************** */

namespace {

nlohmann::json threads_json(CPUState *cpu, uint64_t leader) {
    if (!cap.threads) return no_cap("threads");
    nlohmann::json a = nlohmann::json::array();
    for (uint64_t t : threads_of(cpu, leader)) {
        nlohmann::json e;
        e["tid"] = field_int(cpu, t, off.task_pid);
        if (cap.name) e["name"] = read_cstr(cpu, t + off.task_comm, off.task_comm_size);
        if (cap.start_time) e["start_time"] = start_time(cpu, t);
        e["mm"] = hex(field_ptr(cpu, t, off.task_mm));
        e["taskd"] = hex(t);
        a.push_back(e);
    }
    return a;
}

// NUL-separated strings in [start, end) of the process's memory, which must be the current
// address space.
nlohmann::json user_strings(CPUState *cpu, uint64_t ts, int64_t o_start, int64_t o_end) {
    uint64_t mm = field_ptr(cpu, ts, off.task_mm);
    if (!mm) return unavailable("no address space (a kernel thread)");
    uint64_t cur = current_task(cpu);
    if (!cur || field_ptr(cpu, cur, off.task_mm) != mm)
        return unavailable("user memory of another address space than the current one");
    uint64_t start = field_ptr(cpu, mm, o_start), end = field_ptr(cpu, mm, o_end);
    if (end < start || end - start > MAX_ARGS_BYTES) return unavailable("the range " + hex(start) + "-" + hex(end) + " is not one");
    std::string buf(end - start, '\0');
    for (uint64_t a = start; a < end;) {
        uint64_t n = std::min<uint64_t>(end - a, PAGE_SIZE_GUEST - (a & (PAGE_SIZE_GUEST - 1)));
        if (!read_mem(cpu, a, &buf[a - start], n))
            return unavailable("user memory at " + hex(a) + " is not readable (paged out, or not in the TLB)");
        a += n;
    }
    nlohmann::json out = nlohmann::json::array();
    size_t p = 0;
    while (p < buf.size()) {
        size_t z = buf.find('\0', p);
        if (z == std::string::npos) z = buf.size();
        out.push_back(buf.substr(p, z - p));
        p = z + 1;
    }
    return out;
}

nlohmann::json fs_json(CPUState *cpu, uint64_t ts) {
    if (!cap.fs_paths) return no_cap("fs_paths");
    uint64_t fs = field_ptr(cpu, ts, fo.task_fs);
    if (!fs) return unavailable("no fs_struct (an exiting task)");
    for (int tries = 0; tries < 4; tries++) {
        uint64_t s0 = fo.fs_seq >= 0 ? field_uint(cpu, fs, fo.fs_seq, 4) : 0;
        if (s0 & 1) continue;   // a writer is in the middle of it
        uint64_t rd, rm, pd, pm;
        if (fo.fs_paths_struct) {
            rd = field_ptr(cpu, fs + fo.fs_root, fo.path_dentry);
            rm = field_ptr(cpu, fs + fo.fs_root, fo.path_mnt);
            pd = field_ptr(cpu, fs + fo.fs_pwd, fo.path_dentry);
            pm = field_ptr(cpu, fs + fo.fs_pwd, fo.path_mnt);
        } else {
            rd = field_ptr(cpu, fs, fo.fs_root);
            rm = field_ptr(cpu, fs, fo.fs_rootmnt);
            pd = field_ptr(cpu, fs, fo.fs_pwd);
            pm = field_ptr(cpu, fs, fo.fs_pwdmnt);
        }
        if (fo.fs_seq >= 0 && field_uint(cpu, fs, fo.fs_seq, 4) != s0) continue;
        nlohmann::json j;
        // From the root of the mount tree, as /proc shows them to a reader that isn't chrooted.
        j["root"] = path_of(cpu, rd, rm);
        j["cwd"] = path_of(cpu, pd, pm);
        return j;
    }
    return unavailable("fs_struct changed while it was read");
}

nlohmann::json creds_json(CPUState *cpu, uint64_t ts) {
    if (!cap.creds) return no_cap("creds");
    uint64_t base = fo.ids_in_task ? ts : field_ptr(cpu, ts, fo.task_cred);
    if (!base) return unavailable("no cred");
    nlohmann::json j;
    for (int i = 0; i < 8; i++) j[ID_NAMES[i]] = (uint32_t)field_uint(cpu, base, fo.ids[i], 4);
    return j;
}

nlohmann::json caps_json(CPUState *cpu, uint64_t ts) {
    if (!cap.cap_sets) return no_cap("cap_sets");
    uint64_t base = fo.caps_in_task ? ts : field_ptr(cpu, ts, fo.task_cred);
    if (!base) return unavailable("no cred");
    nlohmann::json j;
    for (int i = 0; i < 4; i++) {
        if (fo.caps[i] < 0) continue;
        uint64_t v = 0;
        if (fo.caps_kind == fo.CapsU64) {
            v = field_uint(cpu, base, fo.caps[i], 8);
        } else if (fo.caps_kind == fo.CapsU32Array) {
            for (int64_t k = 0; k < std::min<int64_t>(fo.caps_count[i], 2); k++)
                v |= field_uint(cpu, base, fo.caps[i] + 4 * k, 4) << (32 * k);
        } else {
            v = field_uint(cpu, base, fo.caps[i], 4);
        }
        j[CAP_NAMES[i]] = hex(v);
    }
    return j;
}

std::string ipv4(const uint8_t *b) {
    char s[32];
    snprintf(s, sizeof(s), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return s;
}

std::string ipv6(const uint8_t *b) {
    char s[64];
    snprintf(s, sizeof(s), "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x", b[0], b[1],
             b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    return s;
}

nlohmann::json socket_json(CPUState *cpu, uint64_t file) {
    if (!cap.sockets) return no_cap("sockets");
    uint64_t sock = field_ptr(cpu, file, fo.file_private);
    uint64_t sk = field_ptr(cpu, sock, fo.sock_sk);
    if (!sock || !sk) return unavailable("no struct sock");
    nlohmann::json j;
    uint32_t family = field_uint(cpu, sk, fo.sk_family, 2);
    uint32_t state = field_uint(cpu, sk, fo.sk_state, 1);
    uint32_t type = field_uint(cpu, sock, fo.sock_type, 2);
    j["family"] = family;
    j["type"] = type;
    j["state"] = state;
    j["listening"] = state == 10;   // TCP_LISTEN, which unix sockets use too
    if (family == 2 || family == 10) {   // AF_INET, AF_INET6
        uint8_t l[16] = {0}, r[16] = {0}, dp[2] = {0};
        uint64_t port = field_uint(cpu, sk, fo.sk_num, 2);   // host order
        read_mem(cpu, sk + fo.sk_dport, dp, 2);               // network order
        uint32_t rport = (dp[0] << 8) | dp[1];
        if (family == 2) {
            read_mem(cpu, sk + fo.sk_rcv_saddr, l, 4);
            read_mem(cpu, sk + fo.sk_daddr, r, 4);
            j["local"] = ipv4(l) + ":" + std::to_string(port);
            j["remote"] = ipv4(r) + ":" + std::to_string(rport);
        } else if (fo.sk_v6_rcv >= 0 && fo.sk_v6_daddr >= 0) {
            read_mem(cpu, sk + fo.sk_v6_rcv, l, 16);
            read_mem(cpu, sk + fo.sk_v6_daddr, r, 16);
            j["local"] = "[" + ipv6(l) + "]:" + std::to_string(port);
            j["remote"] = "[" + ipv6(r) + "]:" + std::to_string(rport);
        } else {
            j["local"] = unavailable("IPv6 addresses: no sock_common.skc_v6_* in the ISF");
            j["remote"] = j["local"];
        }
    } else if (family == 1) {   // AF_UNIX
        if (fo.unix_addr < 0 || fo.uaddr_len < 0 || fo.uaddr_name < 0 || fo.sun_path < 0) {
            j["path"] = unavailable("unix socket addresses: unix_sock.addr or unix_address is not in the ISF");
        } else {
            uint64_t ua = field_ptr(cpu, sk, fo.unix_addr);
            if (ua) {
                int32_t len = field_int(cpu, ua, fo.uaddr_len) - (int32_t)fo.sun_path;
                std::string p;
                if (len > 0 && len <= 108) {
                    p.resize(len);
                    if (!read_mem(cpu, ua + fo.uaddr_name + fo.sun_path, &p[0], len)) p.clear();
                }
                if (!p.empty() && p[0] == '\0') p[0] = '@';   // the abstract namespace
                else p = p.substr(0, p.find('\0'));
                j["path"] = p;
            } else {
                j["path"] = nullptr;   // unbound
            }
        }
    }
    return j;
}

nlohmann::json files_json(CPUState *cpu, uint64_t ts) {
    if (!cap.open_files) return no_cap("open_files");
    uint64_t files = field_ptr(cpu, ts, fo.task_files);
    uint64_t fdt = field_ptr(cpu, files, fo.files_fdt);
    if (!files || !fdt) return unavailable("no files_struct (an exiting task, or a kernel thread)");
    int64_t max = std::min<int64_t>(field_uint(cpu, fdt, fo.fdt_max_fds, 4), MAX_FDS);
    uint64_t arr = field_ptr(cpu, fdt, fo.fdt_fd);
    Root root = root_of(cpu, ts);
    nlohmann::json a = nlohmann::json::array();
    std::vector<uint8_t> raw(max * prof.pointer_size);
    bool bulk = max > 0 && read_mem(cpu, arr, raw.data(), raw.size());
    for (int64_t fd = 0; fd < max; fd++) {
        uint64_t f = 0;
        if (bulk) {
            for (int i = 0; i < prof.pointer_size; i++) {
                int k = prof.big_endian ? prof.pointer_size - 1 - i : i;
                f |= (uint64_t)raw[fd * prof.pointer_size + i] << (8 * k);
            }
        } else {
            f = ptr_at(cpu, arr + fd * prof.pointer_size);
        }
        if (!f || !is_kernel_ptr(f)) continue;
        nlohmann::json e;
        e["fd"] = fd;
        uint64_t dentry = file_dentry(cpu, f);
        uint64_t inode = inode_of(cpu, dentry);
        uint64_t ino = field_uint(cpu, inode, fo.inode_ino, fo.inode_ino_width);
        uint64_t mode = field_uint(cpu, inode, fo.inode_mode, 2);
        const char *kind = mode_kind(mode);
        std::string fst = fs_type(cpu, dentry);
        e["kind"] = kind;
        e["ino"] = ino;
        if (!fst.empty()) e["fs"] = fst;
        e["flags"] = (uint32_t)field_uint(cpu, f, fo.file_flags, 4);
        e["pos"] = (int64_t)field_uint(cpu, f, fo.file_pos, 8);
        // Files with no path have the names /proc gives them.
        if (!strcmp(kind, "socket")) {
            e["path"] = "socket:[" + std::to_string(ino) + "]";
            e["socket"] = socket_json(cpu, f);
        } else if (fst == "pipefs") {
            e["kind"] = "pipe";
            e["path"] = "pipe:[" + std::to_string(ino) + "]";
        } else if (fst == "anon_inodefs") {
            e["kind"] = "anon";
            e["path"] = "anon_inode:" + dentry_name(cpu, dentry);
        } else {
            bool detached = false;
            e["path"] = path_of(cpu, dentry, file_vfsmnt(cpu, f), 0, root.mnt, &detached);
            if (detached) e["internal_mount"] = true;
        }
        a.push_back(e);
    }
    return a;
}

nlohmann::json vmas_json(CPUState *cpu, uint64_t ts) {
    if (!cap.vma_details) return no_cap("vma_details");
    nlohmann::json a = nlohmann::json::array();
    for (uint64_t v : process_vmas(cpu, ts)) {
        nlohmann::json e;
        uint64_t flags = field_ptr(cpu, v, off.vma_flags);
        e["start"] = hex(field_ptr(cpu, v, off.vma_start));
        e["end"] = hex(field_ptr(cpu, v, off.vma_end));
        std::string perms = "----";
        if (flags & 1) perms[0] = 'r';     // VM_READ
        if (flags & 2) perms[1] = 'w';     // VM_WRITE
        if (flags & 4) perms[2] = 'x';     // VM_EXEC
        perms[3] = (flags & 0x80) ? 's' : 'p';   // VM_MAYSHARE
        e["perms"] = perms;
        e["flags"] = hex(flags);
        uint64_t f = field_ptr(cpu, v, off.vma_file);
        if (f) {
            e["offset"] = field_ptr(cpu, v, off.vma_pgoff) * PAGE_SIZE_GUEST;
            if (cap.file_paths) {
                uint64_t dentry = file_dentry(cpu, f);
                e["file"] = path_of(cpu, dentry, file_vfsmnt(cpu, f));
                if (fo.dentry_inode >= 0 && fo.inode_ino >= 0)
                    e["ino"] = field_uint(cpu, inode_of(cpu, dentry), fo.inode_ino, fo.inode_ino_width);
            }
        } else {
            e["offset"] = 0;
            e["file"] = nullptr;   // anonymous
        }
        a.push_back(e);
    }
    return a;
}

bool wants(const std::string &which, const char *what) {
    if (which == "all") return true;
    std::stringstream ss(which);
    std::string w;
    while (std::getline(ss, w, ','))
        if (w == what) return true;
    return false;
}

}  // namespace

std::string facts_json(CPUState *cpu, int32_t pid, uint64_t start, const std::string &which) {
    nlohmann::json j;
    j["pid"] = pid;
    if (!cap.tasks || !cap.pid) {
        j["unavailable"] = std::string("no process list: ") + (!cap.tasks ? "tasks: " + plan.why("tasks")
                                                                          : "pid: " + plan.why("pid"));
        return j.dump();
    }
    uint64_t ts = find_process(cpu, pid);
    if (!ts) {
        j["unavailable"] = "no process " + std::to_string(pid);
        return j.dump();
    }
    uint64_t st = cap.start_time ? start_time(cpu, ts) : 0;
    if (start && cap.start_time && st != start) {
        j["unavailable"] = "process " + std::to_string(pid) + " started at " + std::to_string(st) + ", not " +
                           std::to_string(start) + ": the pid was reused";
        return j.dump();
    }
    if (cap.start_time) j["start_time"] = st;
    if (cap.name) j["name"] = read_cstr(cpu, ts + off.task_comm, off.task_comm_size);
    if (cap.ppid) j["ppid"] = field_int(cpu, field_ptr(cpu, ts, off.task_real_parent), off.task_tgid);
    j["taskd"] = hex(ts);
    j["mm"] = hex(field_ptr(cpu, ts, off.task_mm));
    if (wants(which, "threads")) j["threads"] = threads_json(cpu, ts);
    if (wants(which, "cmdline"))
        j["cmdline"] = cap.cmdline ? user_strings(cpu, ts, fo.mm_arg_start, fo.mm_arg_end) : no_cap("cmdline");
    if (wants(which, "environ"))
        j["environ"] = cap.cmdline ? user_strings(cpu, ts, fo.mm_env_start, fo.mm_env_end) : no_cap("cmdline");
    if (wants(which, "exe")) {
        if (!cap.exe_path) j["exe"] = no_cap("exe_path");
        else if (!field_ptr(cpu, ts, off.task_mm)) j["exe"] = unavailable("no address space (a kernel thread)");
        else if (!field_ptr(cpu, field_ptr(cpu, ts, off.task_mm), fo.mm_exe_file)) j["exe"] = unavailable("no exe_file");
        else j["exe"] = exe_of(cpu, ts);
    }
    if (wants(which, "fs")) {
        nlohmann::json fs = fs_json(cpu, ts);
        if (fs.contains("unavailable")) {
            j["cwd"] = fs;
            j["root"] = fs;
        } else {
            j["cwd"] = fs["cwd"];
            j["root"] = fs["root"];
        }
    }
    if (wants(which, "creds")) j["creds"] = creds_json(cpu, ts);
    if (wants(which, "caps")) j["caps"] = caps_json(cpu, ts);
    if (wants(which, "files")) j["files"] = files_json(cpu, ts);
    if (wants(which, "vmas")) j["vmas"] = vmas_json(cpu, ts);
    return j.dump();
}

/* ******************************************************************
 Kernel function arguments
****************************************************************** */

uint64_t entry_arg0(CPUState *cpu) {
    CPUArchState *env = (CPUArchState *)panda_cpu_env(cpu);
#if defined(TARGET_X86_64)
    return env->regs[R_EDI];
#elif defined(TARGET_I386)
    return env->regs[R_EAX];   // the kernel is built with -mregparm=3
#elif defined(TARGET_AARCH64)
    if (env->aarch64) return env->xregs[0];
    return env->regs[0];
#elif defined(TARGET_ARM)
    return env->regs[0];
#elif defined(TARGET_MIPS)
    return env->active_tc.gpr[4];
#elif defined(TARGET_RISCV)
    return env->gpr[10];
#elif defined(TARGET_PPC)
    return env->gpr[3];
#elif defined(TARGET_LOONGARCH64)
    return env->gpr[4];
#else
    (void)env;
    return 0;
#endif
}

}  // namespace osiisf
