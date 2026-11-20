#pragma once

// returns information about the syscall with the specified number
const syscall_info_t *get_syscall_info(uint32_t callno);

// returns meta-information about the syscalls of the guest os
const syscall_meta_t *get_syscall_meta(void);

// Inside an enter or return callback, on Linux guests on x86_64 and aarch64 (the engine pairs
// the returns): the call's id, the same at its entry and its return, and the engine's delivery
// flags (panda/syscall-pair.h: PANDA_SYSCALL_RETURN, _CHILD for a vfork or clone child's
// first return). False elsewhere.
bool syscalls2_current_call(CPUState *cpu, uint64_t *id, uint32_t *flags);

// Inside an enter or return callback where syscalls2_current_call answers: the call's number and
// its six argument registers as they were at its entry (the engine's PandaSyscallCall). False
// elsewhere.
bool syscalls2_current_args(CPUState *cpu, int64_t *nr, uint64_t args[6]);

// BEGIN_PYPANDA_NEEDS_THIS -- do not delete this comment bc pypanda
// api autogen needs it.  And don't put any compiler directives
// between this and END_PYPANDA_NEEDS_THIS except includes of other
// files in this directory that contain subsections like this one.

// returns the system call return value, hiding arch-specific details
target_long get_syscall_retval(CPUState *cpu);

// END_PYPANDA_NEEDS_THIS -- do not delete this comment!
