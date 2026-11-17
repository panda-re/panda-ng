/* PANDABEGINCOMMENT
 *
 * Authors:
 *  Tim Leek               tleek@ll.mit.edu
 *  Ryan Whelan            rwhelan@ll.mit.edu
 *  Joshua Hodosh          josh.hodosh@ll.mit.edu
 *  Michael Zhivich        mzhivich@ll.mit.edu
 *  Brendan Dolan-Gavitt   brendandg@gatech.edu
 *  Luke Craig             luke.craig@ll.mit.edu
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 * See the COPYING file in the top-level directory.
 *
PANDAENDCOMMENT */
#define __STDC_FORMAT_MACROS

// The return of some linux system calls is not always handled
// correctly. This needs further investigation.
// Uncomment next lines to enable debug prints for tracking of system
// call context. Only for x86
//#define SYSCALL_RETURN_DEBUG
//#define PANDA_LOG_LEVEL PANDA_LOG_DEBUG

#include "panda.h"
#include "panda/syscall-pair.h"

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <functional>
#include <string>
#include <algorithm>
#include <memory>
#include <vector>
#include <iostream>
#include <sstream>
#include <limits>
#include <atomic>
#include <mutex>
#include <set>
#include <tuple>
#include <map>

#include "syscalls2.h"
#include "syscalls2_info.h"
#include "hw_proc_id/hw_proc_id_ext.h"
#if SYSCALLS2_HOOK_RETURNS
#include "hooks/hooks_int_fns.h"
#endif

target_ulong syscalls_get_id(CPUState *cpu){
    return get_id(cpu);
}

#if SYSCALLS2_ENGINE
/* The call being delivered on this vCPU (syscalls2_current_call; the engine's arguments) */
static thread_local const PandaSyscallCall *cur_call;
static thread_local uint32_t cur_flags;
#endif

void syscall_callback(unsigned int vcpu_index, void *udata);

#if SYSCALLS2_HOOK_RETURNS
void (*hooks_add_hook)(struct hook*);
#endif
extern "C" {
bool init_plugin(void *);
void uninit_plugin(void *);
void registerExecPreCallback(void (*callback)(CPUState*, target_ulong));

// API calls
#include "syscalls2_int_fns.h"

// PPP code
#include "syscalls_ext_typedefs.h"
#include "generated/syscall_ppp_boilerplate_enter.cpp"
#include "generated/syscall_ppp_boilerplate_return.cpp"
#include "generated/syscall_ppp_register_enter.cpp"
#include "generated/syscall_ppp_register_return.cpp"
}

// Forward declarations
int32_t get_s32_generic(CPUState *cpu,  syscall_ctx *ctx, uint32_t argnum);
int64_t get_s64_generic(CPUState *cpu,  syscall_ctx *ctx, uint32_t argnum);
int32_t get_return_s32_generic(CPUState *cpu,  syscall_ctx *ctx, uint32_t argnum);
int64_t get_return_s64_generic(CPUState *cpu,  syscall_ctx *ctx, uint32_t argnum);
target_long get_return_val_x86(CPUState *cpu);
target_long get_return_val_x64(CPUState *cpu);
target_long get_return_val_arm(CPUState *cpu);
target_long get_return_val_mips(CPUState *cpu);
target_long get_return_val_ppc(CPUState *cpu);
uint32_t get_return_32_windows_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_return_32_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_return_64_windows_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_return_64_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_linux_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_linux_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_linux_arm(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_linux_mips(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_linux_riscv(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_linux_ppc(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_windows_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_linux_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_linux_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_linux_arm(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_linux_mips(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_linux_riscv(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_linux_ppc(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_windows_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
target_ulong calc_retaddr_windows_x86(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_windows_x64(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_linux_x86(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_linux_x86_int80(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_linux_x64(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_linux_arm(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_linux_mips(CPUState *cpu, target_ulong pc); // TODO
target_ulong calc_retaddr_linux_riscv(CPUState *cpu, target_ulong pc);
target_long get_return_val_riscv(CPUState *cpu);
target_ulong calc_retaddr_linux_ppc(CPUState *cpu, target_ulong pc);

void syscall_enter_linux_mips64(CPUState *cpu, int profile, target_ptr_t pc, int static_callno);
void syscall_return_linux_mips64(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx);

Profile profiles[PROFILE_LAST] = {
    { /* PROFILE_LINUX_X86 */
        .enter_switch = syscall_enter_switch_linux_x86,
        .return_switch = syscall_return_switch_linux_x86,
        .get_return_val = get_return_val_x86,
        .calc_retaddr = calc_retaddr_linux_x86,
        .get_32 = get_32_linux_x86,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_x86,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_x86,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_x86,
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x80,
    },
    { /* PROFILE_LINUX_X86_INT80 */
        .enter_switch = syscall_enter_switch_linux_x86,
        .return_switch = syscall_return_switch_linux_x86,
        .get_return_val = get_return_val_x86,
        .calc_retaddr = calc_retaddr_linux_x86_int80,
        .get_32 = get_32_linux_x86,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_x86,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_x86,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_x86,
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x80,
    },
    { /* PROFILE_LINUX_ARM */
        .enter_switch = syscall_enter_switch_linux_arm,
        .return_switch = syscall_return_switch_linux_arm,
        .get_return_val = get_return_val_arm,
        .calc_retaddr = calc_retaddr_linux_arm,
        .get_32 = get_32_linux_arm,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_arm,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_arm,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_arm,
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x80,
    },
    {   /* PROFILE_LINUX_AARCH64 */
        .enter_switch = syscall_enter_switch_linux_arm64,
        .return_switch = syscall_return_switch_linux_arm64,
        .get_return_val = get_return_val_arm,
        .calc_retaddr = calc_retaddr_linux_arm,
        .get_32 = get_32_linux_arm,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_arm,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_arm,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_arm,
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x80,
    },
    {   /* Linux MIPS32 */
        .enter_switch = syscall_enter_switch_linux_mips,
        .return_switch = syscall_return_switch_linux_mips,
        .get_return_val = get_return_val_mips,
        .calc_retaddr = calc_retaddr_linux_mips,
        .get_32 = get_32_linux_mips,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_mips,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_mips,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_mips,
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x80,
    },
    {   /* Linux MIPS64N32: identical to MIPS64 */
        .enter_switch = syscall_enter_linux_mips64,
        .return_switch = syscall_return_linux_mips64,
        .get_return_val = get_return_val_mips,
        .calc_retaddr = calc_retaddr_linux_mips,
        .get_32 = get_32_linux_mips,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_mips,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_mips,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_mips,
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x80,
    },
    {   /* Linux MIPS64 */
        .enter_switch = syscall_enter_linux_mips64,
        .return_switch = syscall_return_linux_mips64,
        .get_return_val = get_return_val_mips,
        .calc_retaddr = calc_retaddr_linux_mips,
        .get_32 = get_32_linux_mips,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_mips,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_mips,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_mips,
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x80,
    },
    {   /* PROFILE_WINDOWS_2000_X86 */
        .enter_switch = syscall_enter_switch_windows_2000_x86,
        .return_switch = syscall_return_switch_windows_2000_x86,
        .get_return_val = get_return_val_x86,
        .calc_retaddr = calc_retaddr_windows_x86,
        .get_32 = get_32_windows_x86,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_windows_x86,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_return_32_windows_x86,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_return_64_windows_x86,
        .get_return_s64 = get_return_s64_generic,
#if defined(TARGET_I386)
        .windows_return_addr_register = R_ESP,
#else
        .windows_return_addr_register = -1,
#endif
        .windows_arg_offset = 0,
        .syscall_interrupt_number = 0x2E,
    },
    {   /* PROFILE_WINDOWS_XPSP2_X86 */
        .enter_switch = syscall_enter_switch_windows_xpsp2_x86,
        .return_switch = syscall_return_switch_windows_xpsp2_x86,
        .get_return_val = get_return_val_x86,
        .calc_retaddr = calc_retaddr_windows_x86,
        .get_32 = get_32_windows_x86,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_windows_x86,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_return_32_windows_x86,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_return_64_windows_x86,
        .get_return_s64 = get_return_s64_generic,
#if defined(TARGET_I386)
        .windows_return_addr_register = R_EDX,
#else
        .windows_return_addr_register = -1,
#endif
        .windows_arg_offset = 8,
        .syscall_interrupt_number = 0x80,
    },
    {   /* PROFILE_WINDOWS_XPSP3_X86 */
        .enter_switch = syscall_enter_switch_windows_xpsp3_x86,
        .return_switch = syscall_return_switch_windows_xpsp3_x86,
        .get_return_val = get_return_val_x86,
        .calc_retaddr = calc_retaddr_windows_x86,
        .get_32 = get_32_windows_x86,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_windows_x86,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_return_32_windows_x86,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_return_64_windows_x86,
        .get_return_s64 = get_return_s64_generic,
#if defined(TARGET_I386)
        .windows_return_addr_register = R_EDX,
#else
        .windows_return_addr_register = -1,
#endif
        .windows_arg_offset = 8,
        .syscall_interrupt_number = 0x80,
    },
    {   /* PROFILE_WINDOWS_7_X86 */
        .enter_switch = syscall_enter_switch_windows_7_x86,
        .return_switch = syscall_return_switch_windows_7_x86,
        .get_return_val = get_return_val_x86,
        .calc_retaddr = calc_retaddr_windows_x86,
        .get_32 = get_32_windows_x86,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_windows_x86,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_return_32_windows_x86,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_return_64_windows_x86,
        .get_return_s64 = get_return_s64_generic,
#if defined(TARGET_I386)
        .windows_return_addr_register = R_EDX,
#else
        .windows_return_addr_register = -1,
#endif
        .windows_arg_offset = 8,
        .syscall_interrupt_number = 0x80,
    },
    {   /* PROFILE_WINDOWS_7_X64 */
        .enter_switch = syscall_enter_switch_windows_7_x64,
        .return_switch = syscall_return_switch_windows_7_x64,
        .get_return_val = get_return_val_x64,
        .calc_retaddr = calc_retaddr_windows_x64,
        .get_32 = get_32_windows_x64,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_windows_x64,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_return_32_windows_x64,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_return_64_windows_x64,
        .get_return_s64 = get_return_s64_generic,
#if defined(TARGET_X86_64)
        .windows_return_addr_register = R_ECX,
#else
        .windows_return_addr_register = -1,
#endif
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x80,
    },
    {   /* PROFILE_LINUX_X64 */
        .enter_switch = syscall_enter_switch_linux_x64,
        .return_switch = syscall_return_switch_linux_x64,
        .get_return_val = get_return_val_x86,
        .calc_retaddr = calc_retaddr_linux_x64,
        .get_32 = get_32_linux_x64,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_x64,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_x64,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_x64,
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x80,
    },
    {   /* PROFILE_FREEBSD_X64 */
        .enter_switch = syscall_enter_switch_freebsd_x64,
        .return_switch = syscall_return_switch_freebsd_x64,
        .get_return_val = get_return_val_x86,
        .calc_retaddr = calc_retaddr_linux_x64, // Not auto-gen, using the Linux impl
        .get_32 = get_32_linux_x64,             // Not auto-gen, using the Linux impl
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_x64,             // Not auto-gen, using the Linux impl
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_x64,      // Not auto-gen, using the Linux impl
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_x64,      // Not auto-gen, using the Linux impl
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x80,
    },
    {   /* PROFILE_LINUX_RISCV64 */
        .enter_switch = syscall_enter_switch_linux_riscv64,
        .return_switch = syscall_return_switch_linux_riscv64,
        .get_return_val = get_return_val_riscv,
        .calc_retaddr = calc_retaddr_linux_riscv,
        .get_32 = get_32_linux_riscv,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_riscv,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_riscv,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_riscv,
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = -1,
    },
    {   /* PROFILE_LINUX_PPC */
        .enter_switch = syscall_enter_switch_linux_ppc,
        .return_switch = syscall_return_switch_linux_ppc,
        .get_return_val = get_return_val_ppc,
        .calc_retaddr = calc_retaddr_linux_ppc,
        .get_32 = get_32_linux_ppc,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_ppc,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_ppc,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_ppc,
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x0,
    },
    {   /* PROFILE_LINUX_PPC64 */
        .enter_switch = syscall_enter_switch_linux_ppc64,
        .return_switch = syscall_return_switch_linux_ppc64,
        .get_return_val = get_return_val_ppc,
        .calc_retaddr = calc_retaddr_linux_ppc,
        .get_32 = get_32_linux_ppc,
        .get_s32 = get_s32_generic,
        .get_64 = get_64_linux_ppc,
        .get_s64 = get_s64_generic,
        .get_return_32 = get_32_linux_ppc,
        .get_return_s32 = get_return_s32_generic,
        .get_return_64 = get_64_linux_ppc,
        .get_return_s64 = get_return_s64_generic,
        .windows_return_addr_register = -1,
        .windows_arg_offset = -1,
        .syscall_interrupt_number = 0x0,
    }
};

static ProfileType default_profile;
bool load_info = false;

// Reinterpret the ulong as a long. Arch and host specific.
target_long get_return_val_x86(CPUState *cpu){
#if defined(TARGET_I386)
    // this should work for X86_64 as well, as PANDA uses R_EAX to access RAX
    // and target_ulong changes size based on the target
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    return static_cast<target_long>(env->regs[R_EAX]);
#endif
    return 0;
}

target_long get_return_val_x64(CPUState *cpu){
    return get_return_val_x86(cpu);
}

target_long get_return_val_arm(CPUState *cpu){
#if defined(TARGET_ARM)
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
#if !defined(TARGET_AARCH64)
    // arm: reg[0]
    return static_cast<target_long>(env->regs[0]);
#else
    // aarch64: xregs[0]
    return static_cast<target_long>(env->xregs[0]);
#endif

#endif
    return 0;
}

target_long get_return_val_mips(CPUState *cpu){
#if defined(TARGET_MIPS)
    // Return values are in $v0, $v1 (regs 2 and 3 respectively)
    // $v0 only for almost all for Linux syscalls
    // $v1 returns 2nd file descriptor only for pipe(2) - we'll just ignore this edge case
    // See: https://www.linux-mips.org/wiki/Syscall
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    return static_cast<target_long>(env->active_tc.gpr[2]);
#endif
    return 0;
}

target_long get_return_val_riscv(CPUState *cpu){
#if defined(TARGET_RISCV)
    // a0
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    return static_cast<target_long>(env->gpr[10]);
#endif
    return 0;
}

target_long get_return_val_ppc(CPUState *cpu){
#if defined(TARGET_PPC)
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    return static_cast<target_long>(env->gpr[3]); // r3 holds return value
#endif
    return 0;
}

target_ulong mask_retaddr_to_pc(target_ulong retaddr){
    target_ulong mask = std::numeric_limits<target_ulong>::max() -1;
    return retaddr & mask;
}

// Return address calculations
target_ulong calc_retaddr_windows_x86(CPUState* cpu, target_ulong pc) {
#if defined(TARGET_I386)
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    target_ulong retaddr = 0;
    assert(profiles[default_profile].windows_return_addr_register >= 0);
    panda_virtual_memory_rw(cpu, env->regs[profiles[default_profile].windows_return_addr_register], (uint8_t *) &retaddr, 4, false);
    return retaddr;
#else
    // shouldn't happen
    assert (1==0);
#endif
}

target_ulong calc_retaddr_windows_x64(CPUState* cpu, target_ulong pc) {
#if defined(TARGET_I386) && defined(TARGET_X86_64)
    // at least in 64-bit Windows 7, at the time this is called, the stub
    // containing the syscall instruction has not yet executed, so the return
    // address isn't in a register anywhere - but as syscall is 2 bytes...
    target_ulong retaddr = pc + 2;
    return retaddr;
#else
    // shouldn't happen
    assert (1==0);
#endif
}

target_ulong calc_retaddr_linux_x86(CPUState* cpu, target_ulong pc) {
#if defined(TARGET_I386)
    // For Linux system calls using sysenter, we need to look on the stack.
    // https://reverseengineering.stackexchange.com/questions/2869/how-to-use-sysenter-under-linux
    target_ulong ret = 0x0;
    target_ulong ret_ptr =
        ((CPUX86State *)panda_cpu_env(cpu))->regs[R_ESP] + 0x0C;
    panda_virtual_memory_read(cpu, ret_ptr, (uint8_t *)&ret, sizeof(ret));
    // it's unclear why this sometimes fails on read
    if (ret == 0){
        return ret;
    } else {
        return pc + 2;
    }
#else
    // shouldn't happen
    assert (1==0);
#endif
}

target_ulong calc_retaddr_linux_x86_int80(CPUState* cpu, target_ulong pc) {
#if defined(TARGET_I386)
    return pc+2;
#else
    // shouldn't happen
    assert (1==0);
#endif
}

target_ulong calc_retaddr_linux_x64(CPUState* cpu, target_ulong pc) {
#if defined(TARGET_X86_64)
    // no reason to check because 0x0f 0x05 is the only way here
    // unsigned char buf[2] = {};
    // panda_virtual_memory_rw(cpu, pc, buf, 2, 0);
    // Check if the instruction is syscall (0F 05)
    // if ((0x0F == buf[0]) && (0x05 == buf[1])) {
        // syscall expects the return address to be in RCX, but sometimes RCX is
    	// still 0 at this point; so calculate the return address from the pc
    return pc  + 2;
    // }
    // shouldn't happen
    // else {
        // assert(1==0);
    // }
#else
    // shouldn't happen
    assert (1==0);
#endif
}

target_ulong calc_retaddr_linux_arm(CPUState* cpu, target_ulong pc) {
#if defined(TARGET_ARM)
    // Normal syscalls: return addr is stored in LR
    // Except that we haven't run the SWI instruction yet! LR is where libc will return to!
    //return mask_retaddr_to_pc(env->regs[14]);

    // Fork, exec

    // 32-bit and 64-bit ARM both have thumb field in CPUARMState
    uint8_t offset = 0;
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    bool in_thumb_mode = (env->thumb == 1);
    if(in_thumb_mode){
        offset = 2;
    } else {
        offset = 4; // Note: this is NOT 8 for AARCH64!
    }

#if defined(TARGET_AARCH64)
    //64-bit specific
    // if (!in_thumb_mode) {
    //     unsigned char buf[4] = {};
    //     panda_virtual_memory_rw(cpu, pc, buf, 4, 0);
    //     if (!((buf[0] == 0x01)  && (buf[1] == 0) && (buf[2] == 0) && (buf[3] == 0xd4))) {
    //         assert((1==0) && "Tried to calculate AARCH64 ret addr when instr was not a syscall!");
    //     }
    // }
#else
    // 32-bit specific
    // TODO: check syscall encoding here?
    // If so, check both EABI and OABI!

    if (in_thumb_mode) {
        unsigned char buf[2] = {};
        panda_virtual_memory_rw(cpu, pc, buf, 2, 0);
        if (!(buf[1] == 0xDF && buf[0] == 0)) {
            assert((1==0) && "Tried to calculate THUMB ret addr when instr was not a syscall!");
        }
    }
#endif

    return mask_retaddr_to_pc(pc + offset);
#else
    // shouldnt happen
    assert (1==0);
#endif
}

target_ulong calc_retaddr_linux_mips(CPUState* cpu, target_ulong pc) {
#if defined(TARGET_MIPS)
    // We use PC+4 to grab the instruction after the syscall
    // note we previously incorrectly thought we needed to use $EPC,
    // which is a special register for co-processor 0 to store the
    // PC to return to after exceptions. But then we tested it and
    // that was incorrect.
    return pc +  4;
#else
    // shouldnt happen
    assert (1==0);
#endif
}

target_ulong calc_retaddr_linux_riscv(CPUState* cpu, target_ulong pc) {
    // ecall is 4 bytes (no compressed form); sepc+4 is where the kernel returns
    return pc + 4;
}

target_ulong calc_retaddr_linux_ppc(CPUState* cpu, target_ulong pc) {
#if defined(TARGET_PPC)
    // We use PC+4 to grab the instruction after the syscall
    return pc + 4;
#else
    // shouldnt happen
    assert (1==0);
#endif
}

// Argument getting (at syscall entry)
uint32_t get_linux_x86_argnum(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#if defined(TARGET_I386)
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
#if SYSCALLS2_ENGINE
    if (cur_call && ctx->profile == PROFILE_LINUX_X86 && argnum < 6) {
        // a vDSO entry: the engine read the arguments where the kernel does (argument 6 on
        // the stack; with syscall, argument 2 in ebp)
        return (uint32_t)cur_call->args[argnum];
    }
#endif
    switch (argnum) {
    case 0:
        return env->regs[R_EBX];
        break;
    case 1:
        return env->regs[R_ECX];
        break;
    case 2:
        return env->regs[R_EDX];
        break;
    case 3:
        return env->regs[R_ESI];
        break;
    case 4:
        return env->regs[R_EDI];
        break;
    case 5:
        return env->regs[R_EBP];
        break;
    }
    assert (1==0);
#endif
    return 0;
}

// Argument getting (at syscall entry)
uint64_t get_linux_x64_argnum(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#if defined(TARGET_X86_64)
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);

    // PANDA uses x86 register names to get at the x64 registers
    switch (argnum) {
    case 0:
    	// RDI
        return env->regs[R_EDI];
        break;
    case 1:
    	// RSI
        return env->regs[R_ESI];
        break;
    case 2:
    	// RDX
        return env->regs[R_EDX];
        break;
    case 3:
    	// R10
        return env->regs[10];
        break;
    case 4:
    	// R8
        return env->regs[8];
        break;
    case 5:
    	// R9
        return env->regs[9];
        break;
    }
    assert (1==0);
#endif
    return 0;
}

static uint32_t get_win_syscall_arg(CPUState* cpu, syscall_ctx* ctx, int nr) {
#if defined(TARGET_I386) && !defined(TARGET_X86_64)
    // At sysenter on 32-bit Windows7, args start at env->regs[R_EDX]+8
    // At INT 0x2E on Windows 2000, args start at env->regs[R_EDX]
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    uint32_t arg = 0;
    assert(profiles[default_profile].windows_arg_offset >= 0);
    panda_virtual_memory_rw(cpu, env->regs[R_EDX] + profiles[default_profile].windows_arg_offset + (4*nr),
                            (uint8_t *) &arg, 4, false);
    return arg;
#endif
    return 0;
}

uint32_t get_32_linux_x86 (CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    assert (argnum < 6);
    return (uint32_t) get_linux_x86_argnum(cpu, ctx, argnum);
}
uint32_t get_32_linux_x64 (CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    assert (argnum < 6);
    return (uint32_t) (get_linux_x64_argnum(cpu, ctx, argnum) & 0xFFFFFFFF);
}
uint32_t get_32_linux_arm (CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#ifdef TARGET_ARM
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);

#if defined(TARGET_AARCH64)
    // aarch64 regs in x0-x5
    if (ctx->profile == PROFILE_LINUX_AARCH64) {
        assert (argnum < 6);
        return (uint32_t) env->xregs[argnum];
    }
#endif
    // arm32 regs in r0-r6
    assert (argnum < 7);
    return (uint32_t) env->regs[argnum];
#else
    return 0;
#endif
}

uint32_t get_32_linux_riscv (CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return (uint32_t) get_64_linux_riscv(cpu, ctx, argnum);
}

uint32_t get_32_linux_mips (CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#ifdef TARGET_MIPS
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);

    assert (argnum < 8);
    if (ctx->no >= 4000 && ctx->no < 5000) {
        if (argnum < 4) {
            // Args 1-4 in $a0-$a3 which are regs 4-7 in gpr
            return (uint32_t) env->active_tc.gpr[argnum+4];
        } else {
            // Args 5-8 on the stack
            // 4 <= argnum < 8
            uint32_t buf;
            target_ulong arg_stack_addr = env->active_tc.gpr[29] + 16 + ((argnum - 4) * 4);
            int res = panda_virtual_memory_read(cpu, arg_stack_addr, (uint8_t*)&buf, 4);
            if (res < 0) {
                // TODO: we need an error propagation methodology in this codebase, func sig assumes success
                buf = 0;
            }
            return buf;
        }
#ifdef TARGET_MIPS64
    } else if (ctx->no >= 6000 && ctx->no < 7000) {
        // Args 1-8 in $a0-$a7 which are regs 4-11 in gpr
        return (uint32_t) env->active_tc.gpr[argnum+4];
    } else if (ctx->no >= 5000 && ctx->no < 6000) {
      // We're on the N64 ABI for a 64-bit guest but we want a 32 bit value
      // E.g., mips sys_inotify_add_watch has a u32 argument even on 64-bit guests. I guess
      return get_64_linux_mips(cpu, ctx, argnum) & 0xffffffff;
#endif
    } else {
        assert(0); // Unknown ABI. Should be unreachable
    }

#else
    return 0;
#endif
}

uint32_t get_32_linux_ppc (CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#ifdef TARGET_PPC
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    assert (argnum < 8);
    return (uint32_t) (env->gpr[3 + argnum] & 0xFFFFFFFF);
#else
    return 0;
#endif
}

uint32_t get_32_windows_x86 (CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return (uint32_t) get_win_syscall_arg(cpu, ctx, argnum);
}

uint32_t get_32_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#if defined(TARGET_X86_64)
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    uint32_t argval = 0;
    // haven't seen any structures in first 4 args to a system call, or any
    // floating point values, so don't have to worry about those special cases
    switch (argnum)
    {
        case 0:
            argval = (uint32_t)((env->regs[R_ECX]) & 0xFFFFFFFF);
            break;
        case 1:
            argval = (uint32_t)((env->regs[R_EDX]) & 0xFFFFFFFF);
            break;
        case 2:
            argval = (uint32_t)((env->regs[8]) & 0xFFFFFFFF);
            break;
        case 3:
            argval = (uint32_t)((env->regs[9]) & 0xFFFFFFFF);
            break;
        default:
            // the rest are on the stack, 8 bytes apart
            uint64_t argval8b = 0;
            panda_virtual_memory_rw(cpu, env->regs[R_ESP]+8*(argnum+1),
                    (uint8_t *)&argval8b, 8, false);
            argval = (uint32_t)(argval8b & 0xFFFFFFFF);
    }
    return argval;
#else
    return 0;
#endif
}

uint64_t get_64_linux_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    assert (argnum < 6);
    return (((uint64_t) get_linux_x86_argnum(cpu, ctx, argnum)) << 32) | (get_linux_x86_argnum(cpu, ctx, argnum));
}

uint64_t get_64_linux_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    assert (argnum < 6);
    return (uint64_t) get_linux_x64_argnum(cpu, ctx, argnum);
}

uint64_t get_64_linux_riscv(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#ifdef TARGET_RISCV
    // args in a0-a5 (x10-x15)
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    assert (argnum < 6);
    return (uint64_t) env->gpr[10 + argnum];
#else
    return 0;
#endif
}

uint64_t get_64_linux_arm(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#ifdef TARGET_ARM
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
#if !defined(TARGET_AARCH64)
    // arm32 regs in r0-r6
    assert (argnum < 7);
    return (((uint64_t) env->regs[argnum]) << 32) | (env->regs[argnum+1]);
#else
    // aarch64 fits 64 bit registers in regs in x0-x5
    assert (argnum < 6);
    return (uint64_t) env->xregs[argnum];
#endif
#else
    return 0;
#endif
}

uint64_t get_64_linux_mips(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#ifdef TARGET_MIPS
    // A 64-bit guest may use n32 or n64 ABIs
    // Args 1-8 in $a0-$a7 which are regs 4-11 in gpr
    // With N32 ABI we should only return 32 bits worth of data
    assert (argnum < 8);
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    return (uint64_t) env->active_tc.gpr[argnum+4];
#else
    return 0;
#endif
}

uint64_t get_64_linux_ppc(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#ifdef TARGET_PPC
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
#if defined(TARGET_PPC64)
    assert (argnum < 8);
    return (uint64_t) env->gpr[3 + argnum];
#else
    assert (argnum < 7);
    return (((uint64_t) env->gpr[3 + argnum]) << 32) | (uint32_t)(env->gpr[3 + argnum + 1]);
#endif
#else
    return 0;
#endif
}

uint64_t get_64_windows_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    assert (false && "64-bit arguments not supported on Windows 7 x86");
    return 0;
}

uint64_t get_64_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#if defined(TARGET_X86_64)
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    uint64_t argval = 0;
    // haven't seen any structures in first 4 args to a system call, or any
    // floating point values, so don't have to worry about those special cases
    switch (argnum)
    {
        case 0:
            argval = env->regs[R_ECX];
            break;
        case 1:
            argval = env->regs[R_EDX];
            break;
        case 2:
            argval = env->regs[8];
            break;
        case 3:
            argval = env->regs[9];
            break;
        default:
            // the rest are on the stack, 8 bytes apart
            panda_virtual_memory_rw(cpu, env->regs[R_ESP]+8*(argnum+1),
                    (uint8_t *)&argval, 8, false);
    }
    return argval;
#else
    return 0;
#endif
}

// Argument getting (at syscall return)
static uint32_t get_win_syscall_return_arg(CPUState* cpu, syscall_ctx *ctx, int nr) {
#if defined(TARGET_I386)
    // At sysenter on Windows7, args start at env->regs[R_EDX]+8
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    uint32_t arg = 0;
    panda_virtual_memory_rw(cpu, env->regs[R_ESP] + 4 + (4*nr),
                            (uint8_t *) &arg, 4, false);
    return arg;
#else
    return 0;
#endif
}

uint32_t get_return_32_windows_x86 (CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return get_win_syscall_return_arg(cpu, ctx, argnum);
}

uint32_t get_return_32_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    // suspect the part of the profile this is for is dead code
    LOG_WARNING("get_return_32_windows_x64, returning dummy value from presumed dead code\n");
    return 0;
}
uint64_t get_return_64_windows_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    assert (false && "64-bit arguments not supported on Windows 7 x86");
}

uint64_t get_return_64_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    // suspect the part of the profile this is for is dead code
    LOG_WARNING("get_return_64_windows_x64, returning dummy value from presumed dead code\n");
    return 0;
}

// Wrappers
target_long get_return_val (CPUState *cpu, syscall_ctx *ctx) {
    return profiles[ctx->profile].get_return_val(cpu);
}
target_ulong calc_retaddr (CPUState *cpu, syscall_ctx *ctx, target_ulong pc) {
#if SYSCALLS2_ENGINE
    if (cur_call) {
        return cur_call->pc + cur_call->len;    /* the engine's: where the call returns */
    }
#endif
    return profiles[ctx->profile].calc_retaddr(cpu, pc);
}
uint32_t get_32(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return profiles[ctx->profile].get_32(cpu, ctx, argnum);
}
int32_t get_s32(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return profiles[ctx->profile].get_s32(cpu, ctx, argnum);
}
uint64_t get_64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return profiles[ctx->profile].get_64(cpu, ctx, argnum);
}
int64_t get_s64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return profiles[ctx->profile].get_s64(cpu, ctx, argnum);
}
uint32_t get_return_32 (CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return profiles[ctx->profile].get_return_32(cpu, ctx, argnum);
}
int32_t get_return_s32(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return profiles[ctx->profile].get_return_s32(cpu, ctx, argnum);
}
uint64_t get_return_64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return profiles[ctx->profile].get_return_64(cpu, ctx, argnum);
}
int64_t get_return_s64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return profiles[ctx->profile].get_return_s64(cpu, ctx, argnum);
}

int32_t get_s32_generic(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return (int32_t) get_32(cpu, ctx, argnum);
}

int64_t get_s64_generic(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return (int64_t) get_64(cpu, ctx, argnum);
}

int32_t get_return_s32_generic(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return (int32_t) get_return_32(cpu, ctx, argnum);
}

int64_t get_return_s64_generic(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return (int64_t) get_return_64(cpu, ctx, argnum);
}

void sysinfo_load(int profile){
    const gchar *arch = "unknown";
#if defined(TARGET_I386)
    arch = "x86";

// x86_64 logic
#if defined(TARGET_X86_64)
    if (profile == PROFILE_WINDOWS_7_X64 || profile == PROFILE_LINUX_X64
        || profile == PROFILE_FREEBSD_X64){
        arch = "x64";
    } else if (profile == PROFILE_LINUX_X86 || profile == PROFILE_WINDOWS_2000_X86
        || profile == PROFILE_WINDOWS_XPSP2_X86 || profile == PROFILE_WINDOWS_XPSP3_X86
        || profile == PROFILE_WINDOWS_7_X86) {
        arch = "x86";
    }
#endif

#elif defined(TARGET_ARM)
    arch = "arm";

// aarch64 logic
#if defined(TARGET_AARCH64)
    if (profile == PROFILE_LINUX_AARCH64) {
        arch = "arm64";
    }
#endif

#elif defined(TARGET_MIPS)
    arch = "mips";

// mips64 logic
#if defined(TARGET_MIPS64)
    if (profile == PROFILE_LINUX_MIPS64) {
        arch = "mips64";
    }else if (profile == PROFILE_LINUX_MIPS64N32){
        arch = "mips64n32";
    }
#endif
#elif defined(TARGET_RISCV64)
    arch = "riscv64";

#elif defined(TARGET_PPC)
    arch = "ppc";

// ppc64 logic
#if defined(TARGET_PPC64)
    if (profile == PROFILE_LINUX_PPC64) {
        arch = "ppc64";
    }
#endif
#else
    // will fail on dlopen because dso file won't exist
    arch = "unknown";
#endif
    load_syscall_info(arch, (syscall_info_t**)&profiles[profile].syscall_info, (syscall_meta_t**)&profiles[profile].syscall_meta);
}

void sysinfo_load_profile(int profile, syscall_info_t **syscall_info, syscall_meta_t **syscall_meta){
    if (profiles[profile].syscall_info == NULL){
        sysinfo_load(profile);
    }
    *syscall_info = (syscall_info_t*)profiles[profile].syscall_info;
    *syscall_meta = (syscall_meta_t*)profiles[profile].syscall_meta;
    if (*syscall_info == NULL){
        printf("syscall_info is NULL\n");
    }
}

/**
 * MIPS64 supports 3 ABIs: o32, n32, and n64. This complicates our efforts.
*/
void syscall_enter_linux_mips64(CPUState *cpu, int profile, target_ptr_t pc, int static_callno) {
    #if defined(TARGET_MIPS) && defined(TARGET_MIPS64)
    if (static_callno == -1){
        CPUArchState *env = (CPUArchState*) panda_cpu_env(cpu);
        static_callno = env->active_tc.gpr[2]; 
    }
    if (static_callno >= 4000 && static_callno <= 4999) {
        profile = PROFILE_LINUX_MIPS32;
		syscall_enter_switch_linux_mips(cpu, profile, pc, static_callno);
	}else if (static_callno >= 5000 && static_callno <= 5999) {
        profile = PROFILE_LINUX_MIPS64;
        syscall_enter_switch_linux_mips64(cpu, profile, pc, static_callno);
    }else if (static_callno >= 6000 && static_callno <= 6999) {
        profile = PROFILE_LINUX_MIPS64N32;
        syscall_enter_switch_linux_mips64n32(cpu, profile, pc, static_callno);
    }else{
        assert("syscall_enter_linux_mips64: static_callno not found");
    }
    #endif
}


void syscall_return_linux_mips64(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx) {
    #if defined(TARGET_MIPS) && defined(TARGET_MIPS64)
    if (ctx->no >= 4000 && ctx->no <= 4999) {
		syscall_return_switch_linux_mips(cpu, pc, ctx);
	}else if (ctx->no >= 5000 && ctx->no <= 5999) {
        syscall_return_switch_linux_mips64(cpu, pc, ctx);
    }else if (ctx->no >= 6000 && ctx->no <= 6999) {
        syscall_return_switch_linux_mips64n32(cpu, pc, ctx);
    }else{
        assert("syscall_return_linux_mips64: ctx->no not found");
    }
    #endif
}


static std::vector<void (*)(CPUState*, target_ulong)> preExecCallbacks;

void registerExecPreCallback(void (*callback)(CPUState*, target_ulong)){
    preExecCallbacks.push_back(callback);
}


#if SYSCALLS2_HOOK_RETURNS
/**
 * @brief Map holding the context of ongoing system calls, for the guests whose
 * returns syscalls2 tracks with hooks (not SYSCALLS2_ENGINE's). An unfinished
 * system call is identified by its return address and the asid of the process
 * that invoked it; threads of one process making the same call share the key.
 */
context_map_t running_syscalls;
std::mutex running_syscalls_lock;

#if defined(SYSCALL_RETURN_DEBUG)
/**
 * @brief Returns a string representation of a context_map_t container.
 */
static inline std::string context_map_t_dump(context_map_t &cm) {
    const syscall_info_t *si = syscall_info;
    const syscall_meta_t *sm = syscall_meta;
    std::stringstream ss;
    ss << "{";
    for (auto ctxi = cm.begin(); ctxi != cm.end(); ++ctxi) {
	    syscall_ctx_t *ctx = &ctxi->second;
	    ss << " ";
	    if (si == NULL || ctx->no > sm->max_generic) {
	        ss << ctx->no;
	    } else {
	        ss << si[ctx->no].name;
	    }
	    ss << ":" << std::hex << ctx->asid;
	    ss << ",";
    }
    ss.seekp(-1, ss.cur);
    ss << " }";
    return ss.str();
}
#endif

/**
 * @brief Checks if the translation block that is about to be executed
 * matches the return address of an executing system call.
 */
void hook_syscall_return(CPUState *cpu, TranslationBlock *tb, struct hook* h) {
    target_ulong current_asid = get_id(cpu);
    target_ulong pc = panda_tb_pc(cpu, tb);
    auto k = std::make_pair(pc, current_asid);
    [[maybe_unused]] int no = -1;
    // Other vCPUs enter and return calls concurrently: take this call's context out under the
    // lock, and run the callbacks on the copy without it.
    syscall_ctx_t ctx;
    bool found = false, again = false;
    {
        std::lock_guard<std::mutex> g(running_syscalls_lock);
        auto ctxi = running_syscalls.find(k);
        if (ctxi == running_syscalls.end()) { [[unlikely]]
            k = std::make_pair(pc, 0);
            ctxi = running_syscalls.find(k);
        }
        if (ctxi != running_syscalls.end()) {
            found = true;
            ctx = ctxi->second;
            if (ctxi->second.double_return) {
                ctxi->second.double_return = false;
                again = true;
            } else {
                running_syscalls.erase(ctxi);
            }
        }
    }
    if (found) {
        no = ctx.no;
        profiles[ctx.profile].return_switch(cpu, pc, &ctx);
        if (again) {
            return;
        }
    }
#if defined(SYSCALL_RETURN_DEBUG)
    if (no >= 0) {
#if PANDA_LOG_LEVEL >= PANDA_LOG_DEBUG
        // If not guarded we get unused variable warning
        const syscall_info_t *si = syscall_info;
        const syscall_meta_t *sm = syscall_meta;
        std::lock_guard<std::mutex> g(running_syscalls_lock);
        std::string remaining = context_map_t_dump(running_syscalls);
        LOG_DEBUG("returned: %s:" TARGET_PTR_FMT, (no > sm->max_generic ? "N/A" : si[no].name), get_id(cpu));
        LOG_DEBUG("remaining %zu: %s\n", running_syscalls.size(), remaining.c_str());
#endif
    }
#endif
    h->enabled = false;
    return;
}
#endif /* SYSCALLS2_HOOK_RETURNS */

#ifdef DEBUG
static std::map<target_ulong,target_ulong> syscallCounter;
static uint32_t impossibleToReadPCs = 0;
#endif

// Check if the instruction is sysenter (0F 34),
// syscall (0F 05) or int 0x80 (CD 80)
target_ulong doesBlockContainSyscall(CPUState *cpu, struct qemu_plugin_tb *tb, int* static_callno, enum ProfileType* type) {
    unsigned char buf[4] = {};
    int n = qemu_plugin_tb_n_insns(tb);
    struct qemu_plugin_insn *last = qemu_plugin_tb_get_insn(tb, n - 1);
    // The last instruction's address without qemu_plugin_insn_vaddr, which would bind every TB to
    // its address (engine patch 0034) and stop CF_PCREL sharing: the block starts at the CPU's pc
    // (at translation) and its instructions are contiguous. block_translate binds the TBs that
    // hold a system call, whose instrumentation carries the address.
    target_ulong UNUSED(pc) = panda_current_pc(cpu);
    for (int i = 0; i < n - 1; i++) {
        pc += qemu_plugin_insn_size(qemu_plugin_tb_get_insn(tb, i));
    }
    int size = qemu_plugin_insn_size(last);
    // syscalls are at most 4 bytes long
    if (size > 4) {
        return 0;
    }
    qemu_plugin_insn_data(last, &buf, size);
#if defined(TARGET_I386)
    // Check if the instruction is syscall (0F 05)
    if (buf[0]== 0x0F && buf[1] == 0x05) {
        return pc;
    }
    // Check if the instruction is int 0x80 (CD 80)
    else if (buf[0]== 0xCD && buf[1] == profiles[default_profile].syscall_interrupt_number) {
        *type = PROFILE_LINUX_X86_INT80;
        return pc;
    }
    // Check if the instruction is sysenter (0F 34)
    else if (buf[0]== 0x0F && buf[1] == 0x34) {
#if defined(TARGET_X86_64)
        if (*type == PROFILE_LINUX_X64){
            *type = PROFILE_LINUX_X86;
        }else{
            LOG_WARNING("32-bit sysenter found in 64-bit replay - ignoring\n");
            return 0;
        }
#endif
        return pc;
    }
    else {
        return 0;
    }
#elif defined(TARGET_ARM)

#if defined(TARGET_AARCH64)
    // AARCH64 - No thumb mode, syscall is 01 00 00 d4
    // Check for ARM mode syscall
    if ( (buf[0] == 0x01)  && (buf[1] == 0) && (buf[2] == 0) && (buf[3] == 0xd4) ) {
        return pc;
    }

#endif
    // ARM32
    // Check for ARM mode syscall
    CPUArchState *env = panda_cpu_env(cpu);
    if(env->thumb == 0) {
        // EABI
        if ( ((buf[3] & 0x0F) ==  0x0F)  && (buf[2] == 0) && (buf[1] == 0) && (buf[0] == 0) ) {
#ifdef TARGET_AARCH64
            *type = PROFILE_LINUX_ARM;
#endif
            return pc;
        }
#if defined(CAPTURE_ARM_OABI)
        // ARM's old ABI - On by default for all arm profiles (might this cause false-positives?)
        // Here the syscall number is encoded within the instruction so we set static_callno.
        // Note this is the only case where static_callno is used.
        // Instruction will look like 0xFF90XXXX where XXXX is the syscall number
        else if (((buf[3] & 0x0F) == 0x0F)  && (buf[2] == 0x90)) {  // old ABI
            *static_callno = (buf[1]<<8) + (buf[0]);
#ifdef TARGET_AARCH64
            *type = PROFILE_LINUX_ARM;
#endif
            return pc;
        }
#endif
    }
    else {
        // the buffer size is 4, but the read size is 2. Adjust for that
        // in thumb mode.
        // check for Thumb mode syscall
        if (buf[1] == 0xDF && buf[0] == 0){
#ifdef TARGET_AARCH64
            *type = PROFILE_LINUX_ARM;
#endif
            return pc;
        }
    }
    // Arm32/aarch64 - not a match
    return 0;
#elif defined(TARGET_MIPS)


    // ifdef guard prevents us from misinterpreting "syscall" as "jal 0x0000" or "ehb"
    #if TARGET_BIG_ENDIAN == 1
        // 32-bit MIPS "syscall" instruction - big endian
        if ((buf[0] == 0x00) && (buf[1] == 0x00) && (buf[2] == 0x00) && (buf[3] == 0x0c))
            return pc;
    #else
        // 32-bit MIPS "syscall" instruction - little endian
        if ((buf[3] == 0x00) && (buf[2] == 0x00) && (buf[1] == 0x00) && (buf[0] == 0x0c))
            return pc;
    #endif

    return 0;

#elif defined(TARGET_RISCV64)
    // ecall: 73 00 00 00 (no compressed form)
    if (size == 4 && buf[0] == 0x73 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0) {
        return pc;
    }
    return 0;
#elif defined(TARGET_PPC)
    // sc (0x44000002) or scv 0 (0x44000001, POWER9+; glibc uses it when the kernel offers it).
    // Both return to the next instruction. The bytes are in the guest's current byte order,
    // which MSR[LE] gives (ppc64el guests run little-endian on the big-endian target).
    if (size != 4) {
        return 0;
    }
    CPUArchState *env_ppc = (CPUArchState *)panda_cpu_env(cpu);
    uint32_t insn = (env_ppc->msr & 1)   // MSR[LE] is bit 0
        ? (uint32_t)buf[0] | (uint32_t)buf[1] << 8 | (uint32_t)buf[2] << 16 | (uint32_t)buf[3] << 24
        : (uint32_t)buf[3] | (uint32_t)buf[2] << 8 | (uint32_t)buf[1] << 16 | (uint32_t)buf[0] << 24;
    if (insn == 0x44000002 || insn == 0x44000001) {
        return pc;
    }
    return 0;
#else
    return 0; // helpful as a catchall for other architectures
#endif
}

/* A system call instruction as translated: the syscall callback's udata. A block translated on one
 * vCPU runs on every vCPU, so what the callback needs travels with the instrumentation. Interned
 * (set nodes never move), so retranslating a block adds nothing. */
struct sysinfo {
    target_ulong pc;
    enum ProfileType profile;
    int callno;
    bool operator<(const sysinfo &o) const {
        return std::tie(pc, profile, callno) < std::tie(o.pc, o.profile, o.callno);
    }
};
static std::set<sysinfo> syscall_sites;
static std::mutex syscall_sites_lock;

void block_translate(CPUState *cpu, struct qemu_plugin_tb *tb){
    int static_callno = -1; // Set to non -1 if syscall num can be
                            // statically identified
    enum ProfileType profile = default_profile;

    target_ulong res = doesBlockContainSyscall(cpu, tb, &static_callno, &profile);
#ifdef DEBUG
    if(res == (target_ulong) -1){
        impossibleToReadPCs++;
    }
#endif
    if(res != 0 && res != (target_ulong) -1){
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn_by_vaddr(tb, res);
        if (insn != NULL){
            qemu_plugin_insn_vaddr(insn);   // bind the TB: the callback is keyed by this address
            const sysinfo *site;
            {
                std::lock_guard<std::mutex> g(syscall_sites_lock);
                site = &*syscall_sites.insert({res, profile, static_callno}).first;
            }
            qemu_plugin_register_vcpu_insn_exec_cb(insn, syscall_callback, QEMU_PLUGIN_CB_NO_REGS, (void *)site);
        }
    }
}

// This will be called directly from the TCG stream for blocks that contain a
// syscall (as identified by doesBlockContainSyscall). Inserted into TCG by
// before_tcg_codegen.
void syscall_callback(unsigned int vcpu_index, void *udata){
    CPUState *cpu = panda_cpu_by_index(vcpu_index);
    const sysinfo *site = (const sysinfo *)udata;
    target_ulong pc = site->pc;
    enum ProfileType profile = site->profile;
    int callno = site->callno;
#if defined(TARGET_PPC) && defined(TARGET_PPC64)
    CPUArchState *env_ppc = (CPUArchState *)panda_cpu_env(cpu);
    // Check MSR_SF (bit 63) to see if we're in 64-bit mode. If not, it's a 32-bit syscall.
    if (!(env_ppc->msr & (1ULL << 63))) {
        profile = PROFILE_LINUX_PPC;
    } else {
        profile = PROFILE_LINUX_PPC64;
    }
#endif
#if defined(TARGET_I386) && defined(TARGET_X86_64)
#define MSR_EFER_SCE   (1 << 0)
#define MSR_EFER_LMA   (1 << 10)
#define DESC_L_SHIFT    21 /* x86_64 only : 64 bit code segment */
#define DESC_L_MASK     (1 << DESC_L_SHIFT)
    if (panda_os_familyno == OS_WINDOWS) {
        CPUArchState *env = (CPUArchState *)panda_cpu_env(cpu);
        if ((env->efer & MSR_EFER_SCE) && (env->efer & MSR_EFER_LMA)) {
            // shouldn't happen, as WOW should covert the system calls and
            // leave compatibility mode before executing them, but just in case...
            if (!(env->segs[R_CS].flags & DESC_L_MASK)) {
                LOG_WARNING("x86_64 windows replay is in compatibility mode!\n");
                return;
            }
        } else {
            LOG_WARNING("x86_64 windows replay not in long mode or syscalls not enabled - not executing callback\n");
            return;
        }
    }
#endif

#if defined(SYSCALL_RETURN_DEBUG) && defined(TARGET_I386)
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
    int no = env->regs[R_EAX];
    const syscall_info_t *si = syscall_info;
    const syscall_meta_t *sm = syscall_meta;
#endif
        // run any code we need to update our state
        for(const auto callback : preExecCallbacks){
            callback(cpu, pc);
        }
        // Call into autogenerated code for the current syscall!
        profiles[profile].enter_switch(cpu, profile, pc, callno);

#if defined(SYSCALL_RETURN_DEBUG) && defined(TARGET_I386) && SYSCALLS2_HOOK_RETURNS
    if (no >= 0 && !si[no].noreturn) {
        std::lock_guard<std::mutex> g(running_syscalls_lock);
        std::string remaining = context_map_t_dump(running_syscalls);
        const char *c = (rr_get_guest_instr_count() > 7726588867 ? "X" : "");
        LOG_DEBUG("started%s: %s:" TARGET_PTR_FMT, c, (no > sm->max_generic ? "N/A" : si[no].name), get_id(cpu));
        LOG_DEBUG("remaining %zu: %s\n", running_syscalls.size(), remaining.c_str());
    }
#endif
#ifdef DEBUG
        syscallCounter[get_id(cpu)]++;
#endif
    // free(sysinfo);
}


/* ### Engine pairing (SYSCALLS2_ENGINE) ################################## */
/*
 * Linux guests: the engine recognizes the calls of the ABIs declared here, keeps each one whose
 * return a callback wants (with its syscall_ctx_t, PandaSyscallCall data), and pairs it with its
 * return by task (panda/syscall-pair.h). syscalls2 runs its
 * generated switches from the engine's deliveries: the entry before the trap, the return once
 * the task is back in user mode at the return address. A return the kernel restarts (the
 * task traps again: a new entry) and an execve that succeeded have no return callback, as
 * before; a vfork or clone child's first return (CHILD) is delivered as a return of its
 * caller's call (the caller's own return follows).
 */
#if SYSCALLS2_ENGINE
thread_local syscall_ctx_t *syscalls2_engine_store;
thread_local bool syscalls2_engine_keep;

static bool engine_mode;
#define SC2_NB 1024                         /* numbers 0-1023 apart; the rest share one */
#if defined(TARGET_X86_64)
#define SC2_NABI 4                          /* x86_64, i386: int 0x80, sysenter, compat syscall */
#elif defined(TARGET_I386)
#define SC2_NABI 2                          /* int 0x80, sysenter */
#elif defined(TARGET_AARCH64)
#define SC2_NABI 2                          /* AArch64, AArch32 */
#else
#define SC2_NABI 1
#endif
#if defined(TARGET_MIPS)
#undef SC2_NB
#define SC2_NB 7000                         /* o32 4000-, n64 5000-, n32 6000- */
#endif
static int engine_abi[SC2_NABI];                        /* the engine's handles, by our ABI id */
static ProfileType engine_profile[SC2_NABI];
static uint8_t engine_want[SC2_NABI][SC2_NB + 1];      /* what the engine was told */

static void engine_call(void *opaque, CPUState *cpu, PandaSyscallCall *c, int64_t ret,
                        uint32_t flags)
{
    syscall_ctx_t *ctx = (syscall_ctx_t *)c->data;

    cur_call = c;
    cur_flags = flags;
    if (!(flags & PANDA_SYSCALL_RETURN)) {
        unsigned i = (uint64_t)c->nr < SC2_NB ? (unsigned)c->nr : SC2_NB;
        uint8_t want;

        syscalls2_engine_store = ctx;
        syscalls2_engine_keep = false;
        for (const auto callback : preExecCallbacks) {
            callback(cpu, c->pc);
        }
        profiles[engine_profile[c->abi]].enter_switch(cpu, engine_profile[c->abi], c->pc, -1);
        syscalls2_engine_store = nullptr;
        cur_call = nullptr;
        /* what the next call of this number will want: the engine keeps it at once */
        want = PANDA_SYSCALL_WANT_ENTRY | (syscalls2_engine_keep ? PANDA_SYSCALL_WANT_RETURN : 0);
        if (engine_want[c->abi][i] != want) {
            engine_want[c->abi][i] = want;
            panda_syscall_want(engine_abi[c->abi], c->nr, want);
        }
        /* this call: as the engine did (kept or not), or the other way (last: the engine
         * copies or drops the call) */
        if (syscalls2_engine_keep != (c->keytype != PANDA_TASK_KEY_NONE)) {
            panda_syscall_keep(cpu, c, syscalls2_engine_keep);
        }
        return;
    }
    if (!(flags & (PANDA_SYSCALL_RESTART | PANDA_SYSCALL_ENDED))) {
        profiles[ctx->profile].return_switch(cpu, c->pc + c->len, ctx);
    }
    cur_call = nullptr;
}

bool syscalls2_current_call(CPUState *cpu, uint64_t *id, uint32_t *flags)
{
    if (!cur_call) {
        return false;
    }
    *id = cur_call->id;
    *flags = cur_flags;
    return true;
}

static void engine_set(uint8_t *set, unsigned n)
{
    set[n >> 3] |= 1u << (n & 7);
}

/* A Linux ABI's facts: the calls that never return to their caller, execve, the calls that
 * start a child at their return address */
struct LinuxAbi {
    uint32_t form;
    ProfileType profile;
    uint8_t nr_reg, ret_reg, args[6], ret_skip;
    std::vector<unsigned> noreturn, ctx_replace, shared_child;
    uint8_t err_flag;                       /* PandaSyscallAbi.err_flag */
    uint8_t arg_off[6];
};

#if defined(TARGET_X86_64)
/*
 * Linux x86_64's task key: the per-CPU word the kernel loads its stack pointer from at
 * entry_SYSCALL_64 (pcpu_hot.top_of_stack, cpu_current_top_of_stack, or TSS.sp1 on older
 * kernels): the running task's kernel stack. Its offset from KERNEL_GS_BASE is read from the
 * entry code at LSTAR (a gs-relative load into RSP); the word's physical address comes from the
 * kernel's page tables, which under KPTI are the 4 KiB below the user ones.
 */
static bool x86_walk(uint64_t root, uint64_t va, bool la57, uint64_t *pa)
{
    static const uint64_t ADDR = 0x000ffffffffff000ull;
    int levels = la57 ? 5 : 4;
    uint64_t table = root & ADDR, e = 0;

    for (int l = levels - 1; l >= 0; l--) {
        unsigned idx = (va >> (12 + 9 * l)) & 0x1ff;

        if (panda_physical_memory_read(table + idx * 8, (uint8_t *)&e, 8) != 0 || !(e & 1)) {
            return false;
        }
        if ((l == 2 || l == 1) && (e & 0x80)) {         /* a 1 GiB or 2 MiB page */
            uint64_t size = 1ull << (12 + 9 * l);
            *pa = (e & ADDR & ~(size - 1)) | (va & (size - 1));
            return true;
        }
        table = e & ADDR;
    }
    *pa = table | (va & 0xfff);
    return true;
}

static uint64_t x86_task_word(void *opaque, CPUState *cpu, uint64_t base)
{
    CPUArchState *env = (CPUArchState *)panda_cpu_env(cpu);
    uint8_t code[160];
    int64_t off = 0;
    bool found = false;
    uint64_t va, pa, cr3 = env->cr[3];
    bool la57 = env->cr[4] & (1ull << 12);

    if (!env->lstar || panda_virtual_memory_read(cpu, env->lstar, code, sizeof(code)) != 0) {
        return 0;
    }
    for (unsigned i = 0; i + 9 <= sizeof(code) && !found; i++) {
        int32_t d;

        /* mov %gs:disp32, %rsp: 65 48 8b 24 25 disp32 */
        if (code[i] == 0x65 && code[i + 1] == 0x48 && code[i + 2] == 0x8b &&
            code[i + 3] == 0x24 && code[i + 4] == 0x25) {
            memcpy(&d, &code[i + 5], 4);
            off = d;
            found = true;
        /* mov %gs:disp32(%rip), %rsp: 65 48 8b 25 rel32 */
        } else if (code[i] == 0x65 && code[i + 1] == 0x48 && code[i + 2] == 0x8b &&
                   code[i + 3] == 0x25) {
            memcpy(&d, &code[i + 4], 4);
            off = (int64_t)(env->lstar + i + 8) + d;
            found = true;
        }
    }
    if (!found) {
        return 0;
    }
    va = base + off;
    if (x86_walk(cr3, va, la57, &pa) || ((cr3 & 0x1000) && x86_walk(cr3 & ~0x1000ull, va, la57, &pa))) {
        return pa;
    }
    return 0;
}

/*
 * A 32-bit task enters through its vDSO's __kernel_vsyscall: push ecx, edx, ebp; then
 * "mov %esp, %ebp; sysenter" (Intel) or "mov %ecx, %ebp; syscall" (AMD), then "int $0x80".
 * The kernel reads argument 6 where ebp was pushed (at ebp for sysenter, which loses esp; at
 * esp for syscall), takes argument 2 from ebp after a syscall (which loses ecx), returns past the
 * int $0x80 and restarts a call there. syscalls2 reports these under its i386 profile.
 */
#define SC2_I386_EXITS { 1, 252, 119, 173 }, { 11, 358 }, { 2, 120, 190, 435 }
static const LinuxAbi linux_abis[SC2_NABI] = {
    { PANDA_SYSCALL_FORM_X86_SYSCALL, PROFILE_LINUX_X64, 0, 0, { 7, 6, 2, 10, 8, 9 }, 0,
      { 60, 231, 15 }, { 59, 322 }, { 56, 57, 58, 435 } },
    { PANDA_SYSCALL_FORM_X86_INT80, PROFILE_LINUX_X86_INT80, 0, 0, { 3, 1, 2, 6, 7, 5 }, 0,
      SC2_I386_EXITS },
    { PANDA_SYSCALL_FORM_X86_SYSENTER, PROFILE_LINUX_X86, 0, 0,
      { 3, 1, 2, 6, 7, PANDA_SYSCALL_ARG_MEM(5) }, 2, SC2_I386_EXITS },
    { PANDA_SYSCALL_FORM_X86_SYSCALL32, PROFILE_LINUX_X86, 0, 0,
      { 3, 5, 2, 6, 7, PANDA_SYSCALL_ARG_MEM(4) }, 2, SC2_I386_EXITS },
};
#elif defined(TARGET_I386)
/* A 32-bit kernel: int 0x80, and the vDSO's sysenter (as on x86_64 above) */
#define SC2_I386_EXITS { 1, 252, 119, 173 }, { 11, 358 }, { 2, 120, 190, 435 }
static const LinuxAbi linux_abis[SC2_NABI] = {
    { PANDA_SYSCALL_FORM_X86_INT80, PROFILE_LINUX_X86_INT80, 0, 0, { 3, 1, 2, 6, 7, 5 }, 0,
      SC2_I386_EXITS },
    { PANDA_SYSCALL_FORM_X86_SYSENTER, PROFILE_LINUX_X86, 0, 0,
      { 3, 1, 2, 6, 7, PANDA_SYSCALL_ARG_MEM(5) }, 2, SC2_I386_EXITS },
};
#elif defined(TARGET_AARCH64)
static const LinuxAbi linux_abis[SC2_NABI] = {
    { PANDA_SYSCALL_FORM_A64_SVC, PROFILE_LINUX_AARCH64, 8, 0, { 0, 1, 2, 3, 4, 5 }, 0,
      { 93, 94, 139 }, { 221, 281 }, { 220, 435 } },
    { PANDA_SYSCALL_FORM_A32_SVC, PROFILE_LINUX_ARM, 7, 0, { 0, 1, 2, 3, 4, 5 }, 0,
      { 1, 248, 119, 173 }, { 11, 387 }, { 2, 120, 190, 435 } },
};
#elif defined(TARGET_ARM)
/* An AArch32 kernel (EABI) */
static const LinuxAbi linux_abis[SC2_NABI] = {
    { PANDA_SYSCALL_FORM_A32_SVC, PROFILE_LINUX_ARM, 7, 0, { 0, 1, 2, 3, 4, 5 }, 0,
      { 1, 248, 119, 173 }, { 11, 387 }, { 2, 120, 190, 435 } },
};
#elif defined(TARGET_RISCV64)
static const LinuxAbi linux_abis[SC2_NABI] = {
    { PANDA_SYSCALL_FORM_RISCV_ECALL, PROFILE_LINUX_RISCV64, 17, 10, { 10, 11, 12, 13, 14, 15 },
      0, { 93, 94, 139 }, { 221, 281 }, { 220, 435 } },
};
#elif defined(TARGET_MIPS)
/*
 * One trap for every ABI: numbers 4000- are o32, 5000- n64, 6000- n32 (mips64 targets; the
 * switch picks the profile by number). An error sets $a3 and leaves the positive error number in
 * $v0. n32 and n64 pass arguments 5 and 6 in $a4 and $a5 ($8, $9); o32 on the stack, 16 and 20
 * bytes above $sp, which syscalls2's own argument getters read: the o32 declaration has four
 * arguments (the engine reading two words of user memory at every entry cost a fifth of a
 * storm's throughput), and an o32 task's on mips64 are syscalls2's to read too.
 */
#define SC2_MIPS_O32_EXITS { 4001, 4246, 4119, 4193 }, { 4011, 4356 }, { 4120, 4435 }
#if defined(TARGET_MIPS64)
static const LinuxAbi linux_abis[SC2_NABI] = {
    { PANDA_SYSCALL_FORM_MIPS_SYSCALL, PROFILE_LINUX_MIPS64, 2, 2, { 4, 5, 6, 7, 8, 9 }, 0,
      { 4001, 4246, 4119, 4193, 5058, 5205, 5211, 6058, 6205, 6211 },
      { 4011, 4356, 5057, 5316, 6057, 6320 }, { 4120, 4435, 5055, 5435, 6055, 6435 },
      PANDA_SYSCALL_ERR_REG(7) },
};
#else
static const LinuxAbi linux_abis[SC2_NABI] = {
    { PANDA_SYSCALL_FORM_MIPS_SYSCALL, PROFILE_LINUX_MIPS32, 2, 2, { 4, 5, 6, 7, 0xff, 0xff }, 0,
      SC2_MIPS_O32_EXITS, PANDA_SYSCALL_ERR_REG(7) },
};
#endif
#endif

#if defined(TARGET_I386) && !defined(TARGET_X86_64)
/*
 * Linux i386's task key: the TSS's word that holds the running task's kernel stack top, at TR's
 * base. Since 4.19 that is sp1 (+12): sp0 is the CPU's entry stack, next to the TSS in its
 * cpu_entry_area; before, sp0 (+4). The TSS is mapped in every page table (the entry needs it
 * under PTI too), so the current one translates it.
 */
static uint64_t i386_tss_word(void *opaque, CPUState *cpu, uint64_t base)
{
    uint32_t sp0 = 0, d;
    hwaddr pa;

    if (!base || panda_virtual_memory_read(cpu, base + 4, (uint8_t *)&sp0, 4) != 0) {
        return 0;
    }
    d = sp0 > base ? sp0 - (uint32_t)base : (uint32_t)base - sp0;
    pa = panda_virt_to_phys(cpu, base + (d < 0x10000 ? 12 : 4));
    return pa == (hwaddr)-1 ? 0 : pa;
}
#endif

#if defined(TARGET_MIPS)
/*
 * Linux MIPS's task key: kernelsp[cpu], the running task's kernel stack top, which every exception
 * entry loads (get_saved_sp): "(d)mfc0 k0, Context or XContext; lui k1, %hi(kernelsp); (d)srl k0,
 * k0, n; (d)addu k1, k1, k0; lw/ld sp, %lo(kernelsp)(k1)". The array's address, the register and
 * the shift are read from that sequence in the kernel's text (scanned once, at physical 0-64 MiB);
 * the CPU's index is that register shifted. kernelsp is in KSEG0/CKSEG0. (A uniprocessor kernel
 * loads it without the index: not decoded, the call key is used.)
 */
struct MipsKsp {
    uint64_t pa;                            /* kernelsp's physical address; 0: not found */
    unsigned reg, shift;                    /* CP0 4 (Context) or 20 (XContext), >> shift */
};
static std::atomic<int> mips_ksp_state{0};  /* 0 not scanned, 1 scanning or done */
static MipsKsp mips_ksp;
static std::mutex mips_ksp_lock;

static MipsKsp mips_find_kernelsp(void)
{
    std::vector<uint8_t> buf(1 << 16);
    std::map<std::tuple<uint64_t, unsigned, unsigned>, unsigned> seen;
    uint32_t w[8] = {};
    MipsKsp best = {};
    unsigned n = 0;

    for (uint64_t pa = 0; pa < (64ull << 20); pa += buf.size()) {
        if (panda_physical_memory_read(pa, buf.data(), buf.size()) != 0) {
            break;
        }
        for (size_t i = 0; i < buf.size(); i += 4) {
            const uint8_t *b = &buf[i];
            memmove(w, w + 1, 7 * sizeof(w[0]));
            w[7] = TARGET_BIG_ENDIAN ? (uint32_t)b[3] | b[2] << 8 | b[1] << 16 | (uint32_t)b[0] << 24
                                     : (uint32_t)b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24;
            /* w[0] (d)mfc0 k0, rd; w[1] lui k1, hi; w[2] (d)srl(32) k0, k0, sa; w[3] (d)addu k1,
             * k1, k0; the load of the stack pointer within the next 4 */
            if ((w[0] & 0xffdf07ff) != 0x401a0000 || (w[1] & 0xffff0000) != 0x3c1b0000 ||
                (w[3] != 0x037ad821 && w[3] != 0x037ad82d)) {
                continue;
            }
            uint32_t sh = w[2];
            unsigned sa;
            if ((sh & 0xffffffc0) == ((0x001ad000 | ((sh >> 6) & 31) << 6) & 0xffffffc0) &&
                ((sh & 0x3f) == 0x02 || (sh & 0x3f) == 0x3a || (sh & 0x3f) == 0x3e)) {
                sa = ((sh >> 6) & 31) + ((sh & 0x3f) == 0x3e ? 32 : 0);
            } else {
                continue;
            }
            for (unsigned k = 4; k < 8; k++) {
                uint32_t op = w[k] >> 26, base = (w[k] >> 21) & 31;
                if ((op == 0x23 || op == 0x37) && base == 27) {
                    int64_t va = (int32_t)((w[1] & 0xffff) << 16) + (int16_t)(w[k] & 0xffff);
                    seen[{(uint64_t)va & 0x1fffffff, (w[0] >> 11) & 31, sa}]++;
                    break;
                }
            }
        }
    }
    for (auto &kv : seen) {
        if (kv.second > n) {
            n = kv.second;
            best = { std::get<0>(kv.first), std::get<1>(kv.first), std::get<2>(kv.first) };
        }
    }
    return best;
}

static uint64_t mips_kernelsp_word(void *opaque, CPUState *cpu, uint64_t base)
{
    CPUArchState *env = (CPUArchState *)panda_cpu_env(cpu);
    uint64_t r;

    if (mips_ksp_state.load(std::memory_order_acquire) != 2) {
        std::lock_guard<std::mutex> g(mips_ksp_lock);
        if (mips_ksp_state.load(std::memory_order_relaxed) != 2) {
            mips_ksp = mips_find_kernelsp();
            mips_ksp_state.store(2, std::memory_order_release);
        }
    }
    if (!mips_ksp.pa) {
        return 0;
    }
    r = mips_ksp.reg == 20 ? (uint64_t)env->CP0_XContext : (uint64_t)env->CP0_Context;
    return mips_ksp.pa + (r >> mips_ksp.shift);
}
#endif

/* Linux's task key, per target (docs/design/syscall-pairing.md) */
static const PandaTaskKeyDecl LINUX_TASK_KEY = {
    sizeof(PandaTaskKeyDecl),
#if defined(TARGET_X86_64)
    PANDA_TASK_KEY_WORD, PANDA_TASK_REG_X86_KERNEL_GS_BASE, x86_task_word, nullptr,
#elif defined(TARGET_I386)
    PANDA_TASK_KEY_WORD, PANDA_TASK_REG_X86_TR_BASE, i386_tss_word, nullptr,
#elif defined(TARGET_AARCH64)
    PANDA_TASK_KEY_REG, PANDA_TASK_REG_ARM_SP_EL1, nullptr, nullptr,
#elif defined(TARGET_ARM)
    PANDA_TASK_KEY_REG, PANDA_TASK_REG_ARM_SP_SVC, nullptr, nullptr,       /* SP_svc */
#elif defined(TARGET_RISCV64)
    PANDA_TASK_KEY_REG, PANDA_TASK_REG_RISCV_SSCRATCH, nullptr, nullptr,   /* current */
#elif defined(TARGET_MIPS)
    PANDA_TASK_KEY_WORD, PANDA_TASK_REG_MIPS_EBASE, mips_kernelsp_word, nullptr,
#else
    PANDA_TASK_KEY_NONE, 0, nullptr, nullptr,
#endif
};

/* Declare the Linux ABIs and the task key. False if the engine refuses (the hooks path is not
 * built for this target, so syscalls2 does not load). */
static bool engine_init(void *self, bool task_key)
{
    if (!panda_syscall_consumer(self, engine_call, nullptr, sizeof(syscall_ctx_t))) {
        return false;
    }
    if (task_key && LINUX_TASK_KEY.form != PANDA_TASK_KEY_NONE &&
        !panda_task_key_declare(self, &LINUX_TASK_KEY)) {
        return false;
    }
    for (unsigned a = 0; a < SC2_NABI; a++) {
        const LinuxAbi &L = linux_abis[a];
        static uint8_t sets[SC2_NABI][3][SC2_NB / 8 + 1];
        PandaSyscallAbi abi = {};

        for (unsigned n : L.noreturn) engine_set(sets[a][0], n);
        for (unsigned n : L.ctx_replace) engine_set(sets[a][1], n);
        for (unsigned n : L.shared_child) engine_set(sets[a][2], n);
        abi.size = sizeof(abi);
        abi.id = a;
        abi.form = L.form;
        abi.nbits = SC2_NB;
        abi.nr_reg = L.nr_reg;
        abi.nr_width = 32;          /* Linux takes the number as an int */
        abi.ret_reg = L.ret_reg;
        abi.nargs = 6;
        while (abi.nargs && L.args[abi.nargs - 1] == 0xff) {
            abi.nargs--;                    /* the rest are not declared */
        }
        memcpy(abi.arg_reg, L.args, 6);
        abi.ret_skip = L.ret_skip;
        abi.err_flag = L.err_flag;
        memcpy(abi.arg_off, L.arg_off, 6);
        abi.err_min = -4095;
        abi.noreturn = sets[a][0];
        abi.ctx_replace = sets[a][1];
        abi.shared_child = sets[a][2];
        engine_profile[a] = L.profile;
        engine_abi[a] = panda_syscall_abi_declare(self, &abi);
        if (engine_abi[a] < 0) {
            return false;
        }
        /* every entry (the generated switches run for each one, as before); returns are
         * learned per number from the callbacks, at each entry */
        for (unsigned n = 0; n <= SC2_NB; n++) {
            engine_want[a][n] = PANDA_SYSCALL_WANT_ENTRY;
            panda_syscall_want(engine_abi[a], n, PANDA_SYSCALL_WANT_ENTRY);
        }
    }
    engine_mode = true;
    return true;
}
#else
bool syscalls2_current_call(CPUState *cpu, uint64_t *id, uint32_t *flags)
{
    return false;
}
#endif /* SYSCALLS2_ENGINE */


/* ### API calls ######################################################## */
/*!
 * @brief Returns a pointer to the meta-information for the specified syscall.
 */
target_long get_syscall_retval(CPUState *cpu) {
    return profiles[default_profile].get_return_val(cpu);
}

/*!
 * @brief Returns a pointer to the meta-information for the specified syscall.
 */
const syscall_info_t *get_syscall_info(uint32_t callno) {
    if (!load_info){
        return NULL;
    }
    if (profiles[default_profile].syscall_info == NULL){
        sysinfo_load(default_profile);
    }
    syscall_info_t *info = (syscall_info_t*)profiles[default_profile].syscall_info;
    return (const syscall_info_t *)&info[callno];
}

/*!
 * @brief Returns a pointer to the array containing the meta-information
 * for all syscalls.
 */
const syscall_meta_t *get_syscall_meta(void) { 
    if (!load_info){
        return NULL;
    }
    if (profiles[default_profile].syscall_meta == NULL){
        sysinfo_load(default_profile);
    }
    return (const syscall_meta_t*)profiles[default_profile].syscall_meta;
}


/* ### Plugin bootstrapping ############################################# */
/* Host-only transient state: subscriptions, profile and backend selection survive.
 * Return hooks stay: one whose syscall was dropped finds no entry and disables itself. */
static void syscalls2_reset_transient(void *)
{
#if SYSCALLS2_HOOK_RETURNS
    std::lock_guard<std::mutex> g(running_syscalls_lock);
    running_syscalls.clear();
#endif
    /* the engine's pending calls go with the engine's reset */
}

bool init_plugin(void *self) {
// Don't bother if we're not on a supported target
#if defined(TARGET_I386) || defined(TARGET_ARM) || defined(TARGET_MIPS) || defined(TARGET_RISCV64) || defined(TARGET_PPC)
    panda_arg_list *plugin_args = panda_get_args(PLUGIN_NAME);

    // Unused in some architectures
    [[maybe_unused]]const char *abi = panda_parse_string_opt(plugin_args, "abi", NULL, "Syscall ABI if a nonstandard value is used. Currently supported for mips(64) with values: n64, n32, and o32");

    default_profile = PROFILE_LAST;
    if (panda_os_familyno == OS_UNKNOWN)
    {
        std::cerr << PANDA_MSG "ERROR No OS profile specified. You can choose one with the -os switch, eg: '-os linux-32-debian-3.2.81-486' or '-os  windows-32-7sp[01]' " << std::endl;
        return false;
    }
    else if (panda_os_familyno == OS_LINUX) {

#if defined(TARGET_I386)
#if !defined(TARGET_X86_64)
        std::cerr << PANDA_MSG "using profile for linux x86 32-bit" << std::endl;
        default_profile = PROFILE_LINUX_X86;
#else
        std::cerr << PANDA_MSG "using profile for linux x64 64-bit" << std::endl;
        default_profile = PROFILE_LINUX_X64;
#endif
#endif
#if defined(TARGET_ARM)
#if !defined(TARGET_AARCH64)
        std::cerr << PANDA_MSG "using profile for linux arm" << std::endl;
        default_profile = PROFILE_LINUX_ARM;
#else
        std::cerr << PANDA_MSG "using profile for linux aarch64" << std::endl;
        default_profile = PROFILE_LINUX_AARCH64;
#endif
#endif
    
#if defined(TARGET_MIPS)
#if defined(TARGET_MIPS64)
        std::cerr << PANDA_MSG "using profile for linux mips64" << std::endl;
        default_profile = PROFILE_LINUX_MIPS64;
#else
        std::cerr << PANDA_MSG "using profile for linux mips32" << std::endl;
        default_profile = PROFILE_LINUX_MIPS32;
#endif
#endif
#if defined(TARGET_RISCV64)
        std::cerr << PANDA_MSG "using profile for linux riscv64" << std::endl;
        default_profile = PROFILE_LINUX_RISCV64;
#endif

#if defined(TARGET_PPC)
#if !defined(TARGET_PPC64)
        std::cerr << PANDA_MSG "using profile for linux ppc" << std::endl;
        default_profile = PROFILE_LINUX_PPC;
#else
        std::cerr << PANDA_MSG "using profile for linux ppc64" << std::endl;
        default_profile = PROFILE_LINUX_PPC64;
#endif
#endif
    } else if (panda_os_familyno == OS_WINDOWS) {
        if ((panda_os_bits != 32) && (0 != strncmp(panda_os_variant, "7", 1))) {
            std::cerr << PANDA_MSG "only windows 7 supported for 64-bit windows" << std::endl;
            return false;
        }
#if defined(TARGET_I386)
#if !defined(TARGET_X86_64)
        if (0 == strcmp(panda_os_variant, "xpsp2")) {
            std::cerr << PANDA_MSG "using profile for windows sp2 x86 32-bit" << std::endl;
            default_profile = PROFILE_WINDOWS_XPSP2_X86;
        }
        if (0 == strcmp(panda_os_variant, "xpsp3")) {
            std::cerr << PANDA_MSG "using profile for windows sp3 x86 32-bit" << std::endl;
            default_profile = PROFILE_WINDOWS_XPSP3_X86;
        }
        if (0 == strncmp(panda_os_variant, "7", 1)) {
            std::cerr << PANDA_MSG "using profile for windows 7 x86 32-bit" << std::endl;
            default_profile = PROFILE_WINDOWS_7_X86;
        }
        if (0 == strcmp(panda_os_variant, "2000")) {
            std::cerr << PANDA_MSG "using profile for windows 2000 x86 32-bit" << std::endl;
            default_profile = PROFILE_WINDOWS_2000_X86;
        }
#else
        if (0 == strncmp(panda_os_variant, "7", 1)) {
            std::cerr << PANDA_MSG "using profile for windows 7 x64 64-bit" << std::endl;
            default_profile = PROFILE_WINDOWS_7_X64;
        }
#endif
#endif
    } else if (panda_os_familyno == OS_FREEBSD) {
#if defined(TARGET_X86_64)
    std::cerr << PANDA_MSG "using profile for freebsd x64 64-bit" << std::endl;
    default_profile = PROFILE_FREEBSD_X64;
#else
    std::cerr << PANDA_MSG "ERROR: using profile for freebsd x86 32-bit not yet supported!" << std::endl;
    //default_profile = PROFILE_FREEBSD_X86;
    return false;
#endif
    }

    // make sure a system calls profile has been loaded
    if(default_profile == PROFILE_LAST){
        std::cerr << PANDA_MSG "ERROR Couldn't find a syscall profile for the specified OS" << std::endl;
        return false;
    }

    // parse arguments and initialize callbacks & info api
#if SYSCALLS2_ENGINE
    // Linux: the engine recognizes and pairs the calls (task_key=false: pair with the call
    // key, for tests)
    const char *tk = panda_parse_string_opt(plugin_args, "task_key", "true",
        "Linux: pair returns by task key (true) or by call key (false)");
    bool task_key = strcmp(tk, "false") != 0 && strcmp(tk, "0") != 0;
    if (panda_os_familyno == OS_LINUX) {
        if (!engine_init(self, task_key)) {
            std::cerr << PANDA_MSG "ERROR the engine refused the system-call declarations" << std::endl;
            panda_syscall_consumer_release(self);
            panda_free_args(plugin_args);
            return false;
        }
    } else
#endif
    {
#if SYSCALLS2_HOOK_RETURNS
        panda_cb pcb;
        pcb.block_translate = block_translate;
        if (!panda_register_callback_modes(self, PANDA_CB_BLOCK_TRANSLATE, pcb,
                panda_callback_supported_modes(PANDA_CB_BLOCK_TRANSLATE), true)) {
            panda_free_args(plugin_args);
            return false;
        }
#else
        std::cerr << PANDA_MSG "ERROR no system-call profile for this guest" << std::endl;
        panda_free_args(plugin_args);
        return false;
#endif
    }

    // load system call info
    if (panda_parse_bool_opt(plugin_args, "load-info", "Load systemcall information for the selected os.")) {
        load_info = true;
#if defined(TARGET_PPC) && defined(TARGET_PPC64)
        // Ensure the 32-bit profile info is loaded, as we may drop into 32-bit compat mode
        sysinfo_load(PROFILE_LINUX_PPC);
#endif
    }

#if defined(SYSCALL_RETURN_DEBUG)
    assert((syscall_info != NULL) && "syscall return debugging requires loading syscall info");
#endif
    panda_require("hw_proc_id");
    assert(init_hw_proc_id_api());

    // done parsing arguments
    panda_free_args(plugin_args);
#if SYSCALLS2_HOOK_RETURNS
    if (!SYSCALLS2_ENGINE || panda_os_familyno != OS_LINUX) {
        void *hooks = panda_get_plugin_by_name("hooks");
        if (hooks == NULL){
            panda_require("hooks");
            hooks = panda_get_plugin_by_name("hooks");
        }
        hooks_add_hook = (void(*)(struct hook*)) dlsym(hooks, "add_hook");
        if (!hooks_add_hook) {
            std::cerr << PANDA_MSG "syscalls2 requires the hooks plugin" << std::endl;
            // Failed initialization does not run uninit_plugin.
            panda_unregister_callbacks(self);
            return false;
        }
    }
#endif
    if (!panda_register_transient_reset(self, syscalls2_reset_transient, nullptr)) {
        std::cerr << PANDA_MSG "syscalls2 requires a transient reset registration" << std::endl;
#if SYSCALLS2_ENGINE
        panda_syscall_consumer_release(self);
#endif
        panda_unregister_callbacks(self);
        return false;
    }
#elif defined(TARGET_RISCV32) || defined(TARGET_LOONGARCH)
    fprintf(stderr,"The syscalls plugin is not currently supported on this platform.\n");
    return false;
#else //not x86/arm/mips/ppc
    #error
    fprintf(stderr,"The syscalls plugin is not currently supported on this platform.\n");
    return false;
#endif // x86/arm/mips

    // Plugin is good to load - now let's clear the cache to make
    // sure there aren't any previously-translated TCG blocks
    // which already have (uninstrumented) syscalls.
    panda_do_flush_tb();
    return true;
}

void uninit_plugin(void *self) {
    syscalls2_reset_transient(nullptr);
#if SYSCALLS2_ENGINE
    if (engine_mode) {
        panda_syscall_consumer_release(self);
        engine_mode = false;
    }
#endif
    // if we don't clear tb's when this exits we have TBs which can call
    // into our exited plugin.
    panda_do_flush_tb();
#ifdef DEBUG
    std::cout << PANDA_MSG "DEBUG syscall count per asid:";
    for(const auto &asid_count : syscallCounter){
        std::cout << asid_count.first << "=" << asid_count.second <<", ";
    }
    std::cout<< std::endl;
    if(impossibleToReadPCs){
        std::cout << PANDA_MSG "DEBUG some instructions couldn't be read on insn_exec: " << impossibleToReadPCs << std::endl;
    }
#endif
}

/* vim:set tabstop=4 softtabstop=4 expandtab: */
