#pragma once

#include <stdint.h>

// BEGIN_PYPANDA_NEEDS_THIS -- do not delete this comment bc pypanda
// api autogen needs it.  And don't put any compiler directives
// between this and END_PYPANDA_NEEDS_THIS except includes of other
// files in this directory that contain subsections like this one.

// A process or thread lifecycle event (osi_isf's on_osi_isf_event, with lifecycle=true).
// A task is named by (tid, start_time): ids are reused, start times (nanoseconds since boot) are not.
enum {
    OSI_ISF_FORK = 1,       // a new process (fork, vfork, clone without CLONE_THREAD)
    OSI_ISF_THREAD = 2,     // a new thread (clone with CLONE_THREAD)
    OSI_ISF_EXEC = 3,       // a process started a new program
    OSI_ISF_EXIT = 4,       // a task ended (exit_status); group_exit when it was the process's last
    OSI_ISF_REPARENT = 5,   // a process got a new parent (ppid; old_ppid the one that exited)
    OSI_ISF_PID_REUSE = 6   // tid was an earlier task's (that one's start time: old_start), before its FORK/THREAD
};

typedef struct OsiIsfEvent {
    int32_t kind;
    int32_t pid, tid;              // the task the event is about: its process id and its own id
    uint64_t start_time;           // its start time (0: unknown)
    uint64_t process_start;        // its process's (the group leader's) start time
    uint64_t taskd;                // its task_struct
    int32_t creator_pid, creator_tid;  // FORK, THREAD: who called fork/clone (-1: not from a system call)
    uint64_t creator_start;
    int32_t ppid;                  // FORK, THREAD: the parent at creation; REPARENT: the new parent
    int32_t old_ppid;              // REPARENT
    uint64_t clone_flags;          // FORK, THREAD: clone's or clone3's flags; vfork: CLONE_VM|CLONE_VFORK; fork: 0
    const char *syscall;           // FORK, THREAD, EXEC: the call ("fork", "clone", "execve", ...); NULL if none
    int32_t exit_status;           // EXIT: the wait status (exit code << 8, or the signal); -1 unknown
    int32_t group_exit;            // EXIT: 1 the process's last task, 0 not, -1 unknown
    uint64_t old_start;            // PID_REUSE: the earlier task's start time
    const char *old_exe;           // EXEC: the program before (NULL if unknown)
    const char *new_exe;           // EXEC: the program now (NULL if unknown)
    const char *filename;          // EXEC: the path given to execve (NULL if unreadable)
} OsiIsfEvent;

PPP_CB_TYPEDEF(void, on_osi_isf_event, CPUState *cpu, const OsiIsfEvent *ev);

// END_PYPANDA_NEEDS_THIS -- do not delete this comment!
