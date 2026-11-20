#pragma once
#include <map>
#include <mutex>
typedef struct CPUState CPUState;
typedef struct syscall_ctx syscall_ctx_t;
struct sc2_table;

/*
 * How returns are paired.
 *   SYSCALLS2_ENGINE: Linux guests on the targets below. The engine recognizes the
 *     calls of the ABIs syscalls2 declares (panda/syscall-pair.h), keeps each one whose return
 *     a callback wants, and pairs it with its return (by task, exactly). syscalls2 tells it
 *     which numbers' entries and returns its subscribers want.
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
struct PandaSyscallCall;
/* Per vCPU, while syscalls2 delivers one of the engine's calls on it (not thread_local: in a
 * plugin each access to one calls __tls_get_addr) */
typedef struct syscalls2_vcpu {
    const struct PandaSyscallCall *call;    /* the call being delivered */
    union {
        struct {
            uint32_t flags;                 /* the delivery's */
            uint32_t changed;               /* a subscription changed during the delivery */
        };
        uint64_t flags_changed;
    };
    uint64_t a[6];                          /* the arguments, for the call's own callbacks */
} __attribute__((aligned(64))) syscalls2_vcpu_t;
#define SYSCALLS2_VCPUS 256
extern syscalls2_vcpu_t syscalls2_vcpus[SYSCALLS2_VCPUS];
#define SYSCALLS2_VCPU(cpu) (&syscalls2_vcpus[(cpu)->cpu_index & (SYSCALLS2_VCPUS - 1)])
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

// table:  the profile's system calls (sc2_tables.h, generated/syscall_table_*.cpp)
// get_return_val:  function to get the return value for this system call
// calc_retaddr:  function to fetch the address this system call returns to
// get_32, get_s32, get_64 and get_s64:  used at syscall_enter to get the
//   requested argument to the system call as the given type (the hooks path's raw argument
//   words, and the API of the same names)
// get_return_32, get_return_s32, get_return_64, get_return_s64:  not really
//   sure, but maybe like the above 4 but to be called during syscall_return???
// windows_return_addr_register:  used to calculate where to read the return
//   address from (-1 = NA)
// windows_arg_offset:  offset from EDX where args start
// syscall_interrupt_number:  interrupt used for system calls (ignored if NA)
struct Profile {
    const sc2_table *table;
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

/* Each profile's table (generated/syscall_table_<os>_<arch>.cpp) */
extern const sc2_table sc2_table_freebsd_x64, sc2_table_linux_arm64, sc2_table_linux_riscv64,
    sc2_table_linux_arm, sc2_table_linux_mips, sc2_table_linux_mips64, sc2_table_linux_mips64n32,
    sc2_table_linux_x64, sc2_table_linux_x86, sc2_table_linux_ppc, sc2_table_linux_ppc64,
    sc2_table_windows_2000_x86, sc2_table_windows_7_x64, sc2_table_windows_7_x86,
    sc2_table_windows_xpsp2_x86, sc2_table_windows_xpsp3_x86;
