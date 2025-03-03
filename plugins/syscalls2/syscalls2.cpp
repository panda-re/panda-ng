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
#include "hooks/hooks_int_fns.h"

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
#include <threads.h>

#include "syscalls2.h"
#include "syscalls2_info.h"
#include "hw_proc_id/hw_proc_id_ext.h"

target_ulong syscalls_get_id(CPUState *cpu){
    return get_id(cpu);
}

void syscall_callback(unsigned int vcpu_index, void* sysinfo);

void (*hooks_add_hook)(struct hook*);
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
uint32_t get_return_32_windows_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_return_32_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_return_64_windows_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_return_64_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_linux_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_linux_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_linux_arm(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_linux_mips(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_windows_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint64_t get_64_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_linux_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_linux_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_linux_arm(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_linux_mips(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_windows_x86(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
uint32_t get_32_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum);
target_ulong calc_retaddr_windows_x86(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_windows_x64(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_linux_x86(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_linux_x86_int80(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_linux_x64(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_linux_arm(CPUState *cpu, target_ulong pc);
target_ulong calc_retaddr_linux_mips(CPUState *cpu, target_ulong pc); // TODO

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

// Argument getting (at syscall entry)
uint32_t get_linux_x86_argnum(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#if defined(TARGET_I386)
    CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
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

uint32_t get_32_windows_x86 (CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
    return (uint32_t) get_win_syscall_arg(cpu, ctx, argnum);
}

uint32_t get_32_windows_x64(CPUState *cpu, syscall_ctx *ctx, uint32_t argnum) {
#if defined(TARGET_I386)
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


/**
 * @brief Map holding the context of ongoing system calls. An unfinished
 * system call can be uniquely identified by its return address and the
 * asid of the process that invoked it. This pair is used as the key to
 * the map.
 */
context_map_t running_syscalls;

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

#if defined(TARGET_PPC)
#else
/**
 * @brief Checks if the translation block that is about to be executed
 * matches the return address of an executing system call.
 */
void hook_syscall_return(CPUState *cpu, TranslationBlock *tb, struct hook* h) {
    auto k = std::make_pair(tb->pc, get_id(cpu));
    auto ctxi = running_syscalls.find(k);
    [[maybe_unused]] int no = -1;
    if (ctxi == running_syscalls.end()) { [[unlikely]]
        k = std::make_pair(tb->pc, 0);
        ctxi = running_syscalls.find(k);
    }
    if (ctxi != running_syscalls.end()) {
        syscall_ctx_t *ctx = &ctxi->second; 
        no = ctx->no;
        profiles[ctx->profile].return_switch(cpu, tb->pc, ctx);
        if (ctx->double_return){
            ctx->double_return = false;
            return;
        }else{
            running_syscalls.erase(ctxi);
        }
    }
#if defined(SYSCALL_RETURN_DEBUG)
    if (no >= 0) {
#if PANDA_LOG_LEVEL >= PANDA_LOG_DEBUG
        // If not guarded we get unused variable warning
        const syscall_info_t *si = syscall_info;
        const syscall_meta_t *sm = syscall_meta;
        std::string remaining = context_map_t_dump(running_syscalls);
        LOG_DEBUG("returned: %s:" TARGET_PTR_FMT, (no > sm->max_generic ? "N/A" : si[no].name), get_id(cpu));
        LOG_DEBUG("remaining %zu: %s\n", running_syscalls.size(), remaining.c_str());
#endif
    }
#endif
    h->enabled = false;
    return;
}
#endif

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
    target_ulong UNUSED(pc) = qemu_plugin_insn_vaddr(last);
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

#elif defined(TARGET_PPC)
    return 0;
#else
    return 0; // helpful as a catchall for other architectures
#endif
}

struct sysinfo {
    enum ProfileType profile;
    int callno;
};

thread_local std::map<target_ulong, struct sysinfo> syscall_info_map;

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
            syscall_info_map[res] = {profile, static_callno};
            qemu_plugin_register_vcpu_insn_exec_cb(insn, syscall_callback, QEMU_PLUGIN_CB_NO_REGS, (void*) (uint64_t)res);
        }
    }
}

// This will be called directly from the TCG stream for blocks that contain a
// syscall (as identified by doesBlockContainSyscall). Inserted into TCG by
// before_tcg_codegen.
void syscall_callback(unsigned int vcpu_index, void *sysinfo){
    CPUState *cpu = panda_cpu_by_index(vcpu_index);
    target_ulong pc = (target_ulong) (uint64_t)sysinfo;
    enum ProfileType profile = syscall_info_map[pc].profile;
    int callno = syscall_info_map[pc].callno;
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

#if defined(SYSCALL_RETURN_DEBUG) && defined(TARGET_I386)
    if (no >= 0 && !si[no].noreturn) {
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
bool init_plugin(void *self) {
// Don't bother if we're not on a supported target
#if defined(TARGET_I386) || defined(TARGET_ARM) || defined(TARGET_MIPS)
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
    panda_cb pcb;
    pcb.block_translate = block_translate;
    panda_register_callback(self, PANDA_CB_BLOCK_TRANSLATE, pcb);

    // load system call info
    if (panda_parse_bool_opt(plugin_args, "load-info", "Load systemcall information for the selected os.")) {
        load_info = true;
    }

#if defined(SYSCALL_RETURN_DEBUG)
    assert((syscall_info != NULL) && "syscall return debugging requires loading syscall info");
#endif
    panda_require("hw_proc_id");
    assert(init_hw_proc_id_api());

    // done parsing arguments
    panda_free_args(plugin_args);
    void *hooks = panda_get_plugin_by_name("hooks");
	if (hooks == NULL){
		panda_require("hooks");
		hooks = panda_get_plugin_by_name("hooks");
	}
    hooks_add_hook = (void(*)(struct hook*)) dlsym(hooks, "add_hook");
#elif defined(TARGET_PPC)
    fprintf(stderr,"The syscalls plugin is not currently supported on this platform.\n");
    return false;
#else //not x86/arm/mips
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
    //(void) self;
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
