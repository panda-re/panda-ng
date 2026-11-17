#pragma once
#include <map>
#include <mutex>
typedef struct CPUState CPUState;
typedef struct syscall_ctx syscall_ctx_t;

/*
 * How returns are paired.
 *   SYSCALLS2_ENGINE: Linux guests on the targets below. The engine recognizes the
 *     calls of the ABIs syscalls2 declares (panda/syscall-pair.h), keeps each one whose return
 *     a callback wants with its context, and pairs it with its return (by task, exactly). The
 *     generated enter code stores the context in syscalls2_engine_store and sets
 *     syscalls2_engine_keep.
 *   SYSCALLS2_HOOK_RETURNS: Windows and FreeBSD guests (x86). A hook (the hooks plugin) at the
 *     return address, keyed by (return address, ASID) in running_syscalls.
 */
#if defined(TARGET_I386) || defined(TARGET_ARM) || defined(TARGET_RISCV64) || \
    defined(TARGET_MIPS) || defined(TARGET_PPC)
#define SYSCALLS2_ENGINE 1
#else
#define SYSCALLS2_ENGINE 0
#endif
/* x86 keeps the hook path for its Windows and FreeBSD profiles */
#if defined(TARGET_I386) || !SYSCALLS2_ENGINE
#define SYSCALLS2_HOOK_RETURNS 1
#else
#define SYSCALLS2_HOOK_RETURNS 0
#endif

#if SYSCALLS2_ENGINE
extern thread_local syscall_ctx_t *syscalls2_engine_store;
extern thread_local bool syscalls2_engine_keep;
#else
#define syscalls2_engine_store ((syscall_ctx_t *)nullptr)
#endif

#if SYSCALLS2_HOOK_RETURNS
typedef std::map<std::pair<target_ptr_t, target_ptr_t>, syscall_ctx_t> context_map_t;
/* Calls entered and not yet returned, by return address and ASID. A call may return on another
 * vCPU than it entered on: hold running_syscalls_lock for every access. */
extern context_map_t running_syscalls;
extern std::mutex running_syscalls_lock;

extern void (*hooks_add_hook)(struct hook*);
void hook_syscall_return(CPUState *cpu, TranslationBlock* tb, struct hook* h);
#endif


enum ProfileType {
    PROFILE_LINUX_X86,
    PROFILE_LINUX_X86_INT80,
    PROFILE_LINUX_ARM,
    PROFILE_LINUX_AARCH64,
    PROFILE_LINUX_MIPS32,
    PROFILE_LINUX_MIPS64N32,
    PROFILE_LINUX_MIPS64,
    PROFILE_WINDOWS_2000_X86,
    PROFILE_WINDOWS_XPSP2_X86,
    PROFILE_WINDOWS_XPSP3_X86,
    PROFILE_WINDOWS_7_X86,
    PROFILE_WINDOWS_7_X64,
    PROFILE_LINUX_X64,
    PROFILE_FREEBSD_X64,
    PROFILE_LINUX_RISCV64,
    PROFILE_LINUX_PPC,
    PROFILE_LINUX_PPC64,
    PROFILE_LAST
};

// enter_switch:  the generated function that invokes the enter callback
// return_switch:  the generated function that invokes the return callback
// get_return_val:  function to get the return value for this system call
// calc_retaddr:  function to fetch the address this system call returns to
// get_32, get_s32, get_64 and get_s64:  used at syscall_enter to get the
//   requested argument to the system call as the given type
// get_return_32, get_return_s32, get_return_64, get_return_s64:  not really
//   sure, but maybe like the above 4 but to be called during syscall_return???
// windows_return_addr_register:  used to calculate where to read the return
//   address from (-1 = NA)
// windows_arg_offset:  offset from EDX where args start
// syscall_interrupt_number:  interrupt used for system calls (ignored if NA)
struct Profile {
    void         (*enter_switch)(CPUState *, int profile, target_ulong, int);
    void         (*return_switch)(CPUState *, target_ulong, const syscall_ctx_t *);
    target_long  (*get_return_val )(CPUState *);
    target_ulong (*calc_retaddr )(CPUState *, target_ulong);
    uint32_t     (*get_32 )(CPUState *, syscall_ctx_t*, uint32_t);
    int32_t      (*get_s32)(CPUState *, syscall_ctx_t*,  uint32_t);
    uint64_t     (*get_64)(CPUState *, syscall_ctx_t*, uint32_t);
    int64_t      (*get_s64)(CPUState *, syscall_ctx_t*, uint32_t);
    uint32_t     (*get_return_32 )(CPUState *, syscall_ctx_t*, uint32_t);
    int32_t      (*get_return_s32)(CPUState *, syscall_ctx_t*, uint32_t);
    uint64_t     (*get_return_64)(CPUState *, syscall_ctx_t*, uint32_t);
    int64_t      (*get_return_s64)(CPUState *, syscall_ctx_t*,uint32_t);
    int          windows_return_addr_register;
    int          windows_arg_offset;
    int          syscall_interrupt_number;
    void *syscall_info;
    void *syscall_meta;
};

// In generated, run the following to get this list
// grep -hE '^.*syscall_(enter|return)_switch_[^(]*\(' *.cpp | sed 's/ {$/;/'
void syscall_enter_switch_freebsd_x64(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_linux_arm64(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_linux_riscv64(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_linux_arm(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_linux_mips(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_linux_mips64(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_linux_mips64n32(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_linux_x64(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_linux_x86(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_linux_ppc(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_linux_ppc64(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_windows_2000_x86(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_windows_7_x64(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_windows_7_x86(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_windows_xpsp2_x86(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_enter_switch_windows_xpsp3_x86(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_return_switch_freebsd_x64(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_linux_arm64(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_linux_riscv64(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_linux_arm(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_linux_mips(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_linux_mips64n32(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_linux_mips64(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_linux_x64(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_linux_x86(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_linux_ppc(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_linux_ppc64(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_windows_2000_x86(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_windows_7_x64(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_windows_7_x86(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_windows_xpsp2_x86(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
void syscall_return_switch_windows_xpsp3_x86(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);
