/* PANDABEGINCOMMENT
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 * See the COPYING file in the top-level directory.
 *
PANDAENDCOMMENT */
/*!
 * @file lifecycle.cpp
 * @brief Process and thread lifecycle events (lifecycle=true): creation (fork, vfork, clone,
 * clone3), exec, exit with its status, reparenting and pid reuse, delivered to on_osi_isf_event.
 *
 * Creation comes from the system calls, through syscalls2 on the engine's pairing: at the entry
 * the creator, its parent and the clone flags (a child's parent can change later: K_CLONE_PARENT,
 * or the creator's exit); at the return the child's id, or, for a vfork or K_CLONE_VM child, its
 * first return. A child that is seen before its creator's call returns (at a task change, an
 * exec, an exit, its own calls) is reported then, from the pending call that made it, so that a
 * task's creation always comes before its other events. Exec: the old program at execve's entry,
 * the new one where program_start sees it (a hook in the kernel's ELF loader, else
 * proc_start_linux). Exit: a hook on do_exit, whose argument is the
 * wait status, or the exit system calls. Reparenting: an exiting process's children, checked at
 * the following task changes. Pid reuse: tasks are known by (id, start time).
 */
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>

#include "guest.h"
#include "panda/syscall-pair.h"

extern "C" {
#include "osi_isf_types.h"
PPP_PROT_REG_CB(on_osi_isf_event);
PPP_CB_BOILERPLATE(on_osi_isf_event);
}

namespace osiisf {

bool lifecycle_requested;

namespace {

constexpr uint64_t K_CLONE_VM = 0x100, K_CLONE_VFORK = 0x4000, K_CLONE_PARENT = 0x8000, K_CLONE_THREAD = 0x10000;
constexpr int ORPHAN_CHECKS = 1000;   // task changes to wait for an exiting process's children to move

enum Sys { S_FORK, S_VFORK, S_CLONE, S_CLONE3, S_EXECVE, S_EXECVEAT, S_EXIT, S_EXIT_GROUP, S_N };
const char *const SYS_NAMES[S_N] = {"fork", "vfork", "clone", "clone3", "execve", "execveat", "exit", "exit_group"};

bool (*sc_current_call)(CPUState *, uint64_t *, uint32_t *);
bool (*sc_current_args)(CPUState *, int64_t *, uint64_t *);
target_long (*sc_retval)(CPUState *);
bool started, exit_by_hook, exit_by_syscalls;
uint64_t sym_do_exit;
int64_t o_signal, o_live;

struct Task {
    int32_t pid = -1, tid = -1;
    uint64_t start = 0, pstart = 0;
};

Task task_info(CPUState *cpu, uint64_t ts) {
    Task t;
    t.tid = field_int(cpu, ts, off.task_pid);
    t.pid = field_int(cpu, ts, off.task_tgid);
    if (cap.start_time) {
        t.start = start_time(cpu, ts);
        t.pstart = start_time(cpu, group_leader(cpu, ts));
    }
    return t;
}

// A fork/vfork/clone/clone3 call between its entry and its return.
struct Clone {
    Sys sys;
    uint64_t creator, creator_parent;   // task_structs: the caller and its real parent
    Task ct;
    int32_t cppid;                      // the caller's parent's process id
    uint64_t flags;
    bool flags_known;
    std::set<uint64_t> reported;        // children already reported
};

struct Exec {
    uint64_t id, mm;   // the call, and the address space it was made in
    Sys sys;
    std::string old_exe, filename;
    bool has_old, has_filename;
};

struct Orphan {
    uint64_t ts, start;
    int32_t pid, old_ppid;
    int checks;
};

// An event and the strings it points to, delivered once the lock is released.
struct Ev {
    OsiIsfEvent e;
    std::string sys, old_exe, new_exe, filename;
    bool has_sys = false, has_old = false, has_new = false, has_filename = false;
};

std::mutex mu;
std::map<uint64_t, Clone> clones;                // by call id
std::map<int32_t, Clone> awaiting;               // returned calls whose child wasn't found yet, by its id
std::map<int32_t, Exec> execs;                   // by process id
std::map<int32_t, uint64_t> exec_mm;             // process id -> the mm its last EXEC was reported in
std::unordered_map<int32_t, uint64_t> known;     // task id -> start time of the task last seen with it
std::vector<Orphan> orphans;
thread_local std::vector<Ev> outq;

Ev new_ev(int kind, const Task &t, uint64_t ts) {
    Ev v;
    memset(&v.e, 0, sizeof(v.e));
    v.e.kind = kind;
    v.e.pid = t.pid;
    v.e.tid = t.tid;
    v.e.start_time = t.start;
    v.e.process_start = t.pstart;
    v.e.taskd = ts;
    v.e.creator_pid = v.e.creator_tid = -1;
    v.e.ppid = v.e.old_ppid = -1;
    v.e.exit_status = -1;
    v.e.group_exit = -1;
    return v;
}

void deliver(CPUState *cpu) {
    std::vector<Ev> q;
    q.swap(outq);
    for (Ev &v : q) {
        v.e.syscall = v.has_sys ? v.sys.c_str() : NULL;
        v.e.old_exe = v.has_old ? v.old_exe.c_str() : NULL;
        v.e.new_exe = v.has_new ? v.new_exe.c_str() : NULL;
        v.e.filename = v.has_filename ? v.filename.c_str() : NULL;
        PPP_RUN_CB(on_osi_isf_event, cpu, &v.e);
    }
}

int32_t tgid_of(CPUState *cpu, uint64_t ts) { return ts ? field_int(cpu, ts, off.task_tgid) : -1; }

// Records the task as known, reporting a reuse of its id.
void note_task(const Task &t, uint64_t ts) {
    auto it = known.find(t.tid);
    if (cap.pid_reuse && it != known.end() && it->second != t.start) {
        Ev v = new_ev(OSI_ISF_PID_REUSE, t, ts);
        v.e.old_start = it->second;
        outq.push_back(v);
    }
    known[t.tid] = t.start;
}

void report_child(CPUState *cpu, Clone &c, uint64_t child) {
    c.reported.insert(child);
    Task t = task_info(cpu, child);
    note_task(t, child);
    bool thread = c.flags_known ? (c.flags & K_CLONE_THREAD) != 0 : t.pid == c.ct.pid;
    Ev v = new_ev(thread ? OSI_ISF_THREAD : OSI_ISF_FORK, t, child);
    v.e.creator_pid = c.ct.pid;
    v.e.creator_tid = c.ct.tid;
    v.e.creator_start = c.ct.start;
    // The parent at creation, from the caller as it was at the entry.
    v.e.ppid = (c.flags & (K_CLONE_PARENT | K_CLONE_THREAD)) || (!c.flags_known && thread) ? c.cppid : c.ct.pid;
    v.e.clone_flags = c.flags;
    v.sys = SYS_NAMES[c.sys];
    v.has_sys = true;
    outq.push_back(v);
}

// A task seen for the first time (or with an id an earlier task had): its creation, from the
// pending call that made it, else only noted (made before the events started, or by a kernel
// thread).
void ensure_known(CPUState *cpu, uint64_t ts) {
    if (!ts) return;
    int32_t tid = field_int(cpu, ts, off.task_pid);
    if (tid <= 0) return;   // the idle tasks
    uint64_t st = cap.start_time ? start_time(cpu, ts) : 0;
    auto it = known.find(tid);
    if (it != known.end() && it->second == st) return;
    // The call returned this id before the child could be found (a thread, without the thread
    // list to look it up in; or a child in another pid namespace, by chance with the same id).
    auto aw = awaiting.find(tid);
    if (aw != awaiting.end()) {
        Clone c = aw->second;
        awaiting.erase(aw);
        report_child(cpu, c, ts);
        return;
    }
    uint64_t rp = field_ptr(cpu, ts, off.task_real_parent);
    int32_t pid = tgid_of(cpu, ts);
    for (auto &[id, c] : clones) {
        // A call makes one child: a call that reported its child can't be this one's (a
        // CLONE_PARENT child's parent can have a fork of its own pending, whose child ran first).
        if (!c.reported.empty()) continue;
        bool via_parent = c.flags_known ? (c.flags & (K_CLONE_PARENT | K_CLONE_THREAD)) != 0 : pid == c.ct.pid;
        if (via_parent ? rp != c.creator_parent : rp != c.creator) continue;
        if (via_parent && (c.flags & K_CLONE_THREAD) && pid != c.ct.pid) continue;
        report_child(cpu, c, ts);
        return;
    }
    note_task(task_info(cpu, ts), ts);
}

// An exiting process's children, to report when they get another parent.
void collect_orphans(CPUState *cpu, int32_t pid) {
    if (!cap.reparent) return;
    for (uint64_t ts : all_tasks(cpu)) {
        if (tgid_of(cpu, ts) == pid) continue;
        if (tgid_of(cpu, field_ptr(cpu, ts, off.task_real_parent)) != pid) continue;
        orphans.push_back({ts, cap.start_time ? start_time(cpu, ts) : 0, tgid_of(cpu, ts), pid, 0});
    }
}

void check_orphans(CPUState *cpu) {
    for (size_t i = 0; i < orphans.size();) {
        Orphan &o = orphans[i];
        bool same = tgid_of(cpu, o.ts) == o.pid && (!cap.start_time || start_time(cpu, o.ts) == o.start);
        int32_t now = same ? tgid_of(cpu, field_ptr(cpu, o.ts, off.task_real_parent)) : -1;
        if (same && now != o.old_ppid && now > 0) {
            Task t = task_info(cpu, o.ts);
            Ev v = new_ev(OSI_ISF_REPARENT, t, o.ts);
            v.e.ppid = now;
            v.e.old_ppid = o.old_ppid;
            outq.push_back(v);
        } else if (same && ++o.checks < ORPHAN_CHECKS) {
            i++;
            continue;
        }
        orphans.erase(orphans.begin() + i);
    }
}

std::string program_of(CPUState *cpu, uint64_t ts) {
    std::string e = exe_of(cpu, ts);
    if (e.empty() && cap.name) e = read_cstr(cpu, ts + off.task_comm, off.task_comm_size);
    return e;
}

void exited(CPUState *cpu, uint64_t ts, int32_t status, int group) {
    Task t = task_info(cpu, ts);
    Ev v = new_ev(OSI_ISF_EXIT, t, ts);
    v.e.exit_status = status;
    v.e.group_exit = group;
    outq.push_back(v);
    for (auto it = clones.begin(); it != clones.end();)
        it = it->second.creator == ts ? clones.erase(it) : std::next(it);
    if (group == 1) {
        execs.erase(t.pid);
        exec_mm.erase(t.pid);
    }
    // Children move within the process while it has tasks left (no change of parent process
    // to report), so an exit not known to be the last notes them too.
    if (group != 0) collect_orphans(cpu, t.pid);
}

std::map<int32_t, std::set<int32_t>> exiting;   // process id -> its tasks that started exiting

// 1: the last of the process's tasks to start exiting, 0: not, -1: unknown. Every task of the
// group that the thread list still holds must have entered do_exit (signal->live, read at the
// entry, would race: a task that entered before can still be on its way to its decrement).
int last_of_group(CPUState *cpu, uint64_t ts) {
    int32_t pid = tgid_of(cpu, ts);
    std::set<int32_t> &ex = exiting[pid];
    ex.insert(field_int(cpu, ts, off.task_pid));
    int last = -1;
    if (cap.threads) {
        last = 1;
        for (uint64_t t : threads_of(cpu, group_leader(cpu, ts)))
            if (!ex.count(field_int(cpu, t, off.task_pid))) last = 0;
    } else if (o_live >= 0) {
        uint64_t sig = field_ptr(cpu, ts, o_signal);
        if (sig) last = field_int(cpu, sig, o_live) <= 1;
    }
    if (last == 1) exiting.erase(pid);
    return last;
}

void exec_event(CPUState *cpu, uint64_t ts, std::map<int32_t, Exec>::iterator e) {
    Task t = task_info(cpu, ts);
    uint64_t mm = field_ptr(cpu, ts, off.task_mm);
    auto r = exec_mm.find(t.pid);
    if (e == execs.end() && r != exec_mm.end() && r->second == mm) return;   // reported already
    exec_mm[t.pid] = mm;
    Ev v = new_ev(OSI_ISF_EXEC, t, ts);
    if (e != execs.end()) {
        v.sys = SYS_NAMES[e->second.sys];
        v.has_sys = true;
        v.old_exe = e->second.old_exe;
        v.has_old = e->second.has_old;
        v.filename = e->second.filename;
        v.has_filename = e->second.has_filename;
        execs.erase(e);
    }
    v.new_exe = program_of(cpu, ts);
    v.has_new = !v.new_exe.empty();
    outq.push_back(v);
}

// An execve whose program has started though program_start didn't see it start (proc_start_linux
// reads the auxiliary vector from user memory, which may not be readable): the task has another mm.
void exec_seen(CPUState *cpu, uint64_t ts) {
    if (execs.empty() || !ts) return;
    auto e = execs.find(tgid_of(cpu, ts));
    if (e == execs.end()) return;
    uint64_t mm = field_ptr(cpu, ts, off.task_mm);
    if (mm && mm != e->second.mm) exec_event(cpu, ts, e);
}

void on_enter(CPUState *cpu, Sys sys) {
    if (!cap.lifecycle) return;
    uint64_t id;
    uint32_t fl;
    int64_t nr;
    uint64_t a[6];
    if (!sc_current_call(cpu, &id, &fl) || !sc_current_args(cpu, &nr, a)) return;
    uint64_t ts = current_task(cpu);
    if (!ts) return;
    {
        std::lock_guard<std::mutex> l(mu);
        ensure_known(cpu, ts);
        exec_seen(cpu, ts);
        switch (sys) {
        case S_FORK: case S_VFORK: case S_CLONE: case S_CLONE3: {
            Clone c;
            c.sys = sys;
            c.creator = ts;
            c.creator_parent = field_ptr(cpu, ts, off.task_real_parent);
            c.ct = task_info(cpu, ts);
            c.cppid = tgid_of(cpu, c.creator_parent);
            c.flags_known = true;
            c.flags = sys == S_FORK ? 0 : sys == S_VFORK ? K_CLONE_VM | K_CLONE_VFORK : a[0] & 0xffffffffu;
            if (sys == S_CLONE3) {
                // struct clone_args: flags is its first u64, in the caller's memory.
                uint64_t f = 0;
                c.flags_known = read_uint(cpu, a[0], 8, &f);
                c.flags = f;
            }
            clones[id] = c;
            break;
        }
        case S_EXECVE: case S_EXECVEAT: {
            Exec e;
            e.id = id;
            e.mm = field_ptr(cpu, ts, off.task_mm);
            e.sys = sys;
            e.old_exe = program_of(cpu, ts);
            e.has_old = !e.old_exe.empty();
            uint64_t fn = sys == S_EXECVE ? a[0] : a[1];
            e.filename = read_cstr(cpu, fn, 4096);
            e.has_filename = fn && !e.filename.empty();
            execs[tgid_of(cpu, ts)] = e;
            break;
        }
        case S_EXIT: case S_EXIT_GROUP:
            if (exit_by_syscalls) {
                int group = sys == S_EXIT_GROUP ? 1 : last_of_group(cpu, ts);
                exited(cpu, ts, (int32_t)((a[0] & 0xff) << 8), group);
            }
            break;
        default:
            break;
        }
    }
    deliver(cpu);
}

void on_return(CPUState *cpu, Sys sys) {
    if (!cap.lifecycle) return;
    uint64_t id;
    uint32_t fl;
    if (!sc_current_call(cpu, &id, &fl)) return;
    int64_t ret = sc_retval ? (int64_t)sc_retval(cpu) : -1;
    {
        std::lock_guard<std::mutex> l(mu);
        if (sys == S_EXECVE || sys == S_EXECVEAT) {
            // A return: the exec failed.
            int32_t pid = tgid_of(cpu, current_task(cpu));
            auto e = execs.find(pid);
            if (e != execs.end() && e->second.id == id) execs.erase(e);
        } else {
            auto it = clones.find(id);
            if (it != clones.end()) {
                if (fl & PANDA_SYSCALL_CHILD) {
                    // A vfork or K_CLONE_VM child's first return: the child is the current task.
                    uint64_t child = current_task(cpu);
                    if (child && !it->second.reported.count(child)) report_child(cpu, it->second, child);
                } else {
                    // The caller's return: the child's id (in the caller's pid namespace).
                    if (ret > 0 && it->second.reported.empty()) {
                        uint64_t child = find_task(cpu, (int32_t)ret);
                        if (child) {
                            report_child(cpu, it->second, child);
                        } else {
                            // Reported when it is first seen.
                            if (awaiting.size() >= 256) awaiting.erase(awaiting.begin());
                            awaiting[(int32_t)ret] = it->second;
                        }
                    }
                    clones.erase(it);
                }
            }
        }
    }
    deliver(cpu);
}

template <int K>
void sys_enter(CPUState *cpu, target_ulong pc) { on_enter(cpu, (Sys)K); }

template <int K>
void sys_return(CPUState *cpu, target_ulong pc) { on_return(cpu, (Sys)K); }

// Registers fn on syscalls2's callback name, where the guest's profiles have that call. The
// callbacks' typed arguments aren't used (syscalls2_current_args gives them as at the entry), so
// one function serves every profile's prototype.
bool sc_register(void *sc2, const std::string &cb, void *fn) {
    auto add = (void (*)(void *))dlsym(sc2, ("ppp_add_cb_" + cb).c_str());
    if (!add) return false;
    add(fn);
    return true;
}

void exit_hook(CPUState *cpu, TranslationBlock *tb, struct hook *h) {
    if (!cap.lifecycle || !exit_by_hook) return;
    uint64_t ts = current_task(cpu);
    if (!ts) return;
    int32_t code = (int32_t)(uint32_t)entry_arg0(cpu);
    {
        std::lock_guard<std::mutex> l(mu);
        ensure_known(cpu, ts);
        exec_seen(cpu, ts);
        exited(cpu, ts, code, last_of_group(cpu, ts));
    }
    deliver(cpu);
}

}  // namespace

void lifecycle_init() {
    void *sc2 = panda_get_plugin_by_name("syscalls2");
    if (!sc2) {
        fprintf(stderr, PLUGIN_NAME ": lifecycle=true needs syscalls2\n");
        return;
    }
    sc_current_call = (decltype(sc_current_call))dlsym(sc2, "syscalls2_current_call");
    sc_current_args = (decltype(sc_current_args))dlsym(sc2, "syscalls2_current_args");
    sc_retval = (decltype(sc_retval))dlsym(sc2, "get_syscall_retval");
    if (!sc_current_call || !sc_current_args) {
        fprintf(stderr, PLUGIN_NAME ": lifecycle=true needs syscalls2 with syscalls2_current_args\n");
        return;
    }
    void *enters[S_N] = {(void *)sys_enter<S_FORK>, (void *)sys_enter<S_VFORK>, (void *)sys_enter<S_CLONE>,
                         (void *)sys_enter<S_CLONE3>, (void *)sys_enter<S_EXECVE>, (void *)sys_enter<S_EXECVEAT>,
                         (void *)sys_enter<S_EXIT>, (void *)sys_enter<S_EXIT_GROUP>};
    void *returns[S_N] = {(void *)sys_return<S_FORK>, (void *)sys_return<S_VFORK>, (void *)sys_return<S_CLONE>,
                          (void *)sys_return<S_CLONE3>, (void *)sys_return<S_EXECVE>,
                          (void *)sys_return<S_EXECVEAT>, nullptr, nullptr};
    // Callbacks are registered now, before the guest runs; they do nothing until the ISF is
    // paired with the kernel. The guest's arch has some of these calls (aarch64 has no fork).
    std::string got;
    for (int k = 0; k < S_N; k++) {
        if (sc_register(sc2, std::string("on_sys_") + SYS_NAMES[k] + "_enter", enters[k])) got += std::string(" ") + SYS_NAMES[k];
        if (returns[k]) sc_register(sc2, std::string("on_sys_") + SYS_NAMES[k] + "_return", returns[k]);
    }
    printf(PLUGIN_NAME ": lifecycle events from system calls:%s\n", got.c_str());
}

void lifecycle_start(CPUState *cpu) {
    if (started) return;
    started = true;
    const std::string &ex = plan.strategy("exit_codes");
    exit_by_hook = cap.exit_codes && ex == "do_exit hook";
    exit_by_syscalls = cap.exit_codes && ex == "exit system calls";
    o_signal = plan.off("task_struct", "signal");
    o_live = plan.off("signal_struct", "live.counter");
    sym_do_exit = slid(plan.sym("do_exit"));
    if (exit_by_hook) add_kernel_hook(sym_do_exit, exit_hook);
    // Every task alive now is known: only later ones are reported created.
    std::lock_guard<std::mutex> l(mu);
    for (uint64_t ts : all_tasks(cpu)) {
        std::vector<uint64_t> th = threads_of(cpu, ts);
        if (th.empty()) th.push_back(ts);
        for (uint64_t t : th) {
            int32_t tid = field_int(cpu, t, off.task_pid);
            if (tid > 0) known[tid] = cap.start_time ? start_time(cpu, t) : 0;
        }
    }
    printf(PLUGIN_NAME ": lifecycle: %zu tasks alive; exits from %s\n", known.size(),
           exit_by_hook ? "a do_exit hook" : exit_by_syscalls ? "exit system calls" : "nothing (exit_codes unavailable)");
}

void lifecycle_task_change(CPUState *cpu) {
    if (!started) return;
    uint64_t ts = current_task(cpu);
    {
        std::lock_guard<std::mutex> l(mu);
        ensure_known(cpu, ts);
        exec_seen(cpu, ts);
        if (!orphans.empty()) check_orphans(cpu);
    }
    deliver(cpu);
}

void lifecycle_program_start(CPUState *cpu) {
    if (!started) return;
    uint64_t ts = current_task(cpu);
    if (!ts) return;
    {
        std::lock_guard<std::mutex> l(mu);
        ensure_known(cpu, ts);
        exec_event(cpu, ts, execs.find(tgid_of(cpu, ts)));
    }
    deliver(cpu);
}

}  // namespace osiisf
