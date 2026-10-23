/*!
 * @file panda/common.h
 * @brief Common PANDA utility functions.
 *
 * @note Functions that are both simple and frequently called are
 * defined here as inlines. Functions that are either complex or
 * infrequently called are decalred here and defined in `src/common.c`.
 */
#pragma once
#if !defined(__cplusplus)
#include <stdint.h>
#include <stdbool.h>
#else
#include <cstdint>
#include <cstdbool>
#endif
#include "qemu/osdep.h"
#include "qemu/accel.h"
#include "cpu.h"
#include "system/address-spaces.h"
#include "exec/target_page.h"
#include "system/memory.h"
#include "panda/types.h"
#include "gdbstub/internals.h"

/*
 * @brief Branch predition hint macros.
 */
#if !defined(likely)
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#if !defined(unlikely)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#ifdef __cplusplus
extern "C" {
#endif
    
#if defined(TARGET_MIPS)
#define MIPS_HFLAG_KSU    0x00003 /* kernel/supervisor/user mode mask   */
#define MIPS_HFLAG_KM     0x00000 /* kernel mode flag                   */
/*
 *  Register values from: http://www.cs.uwm.edu/classes/cs315/Bacon/Lecture/HTML/ch05s03.html
 */
#define MIPS_SP           29      /* value for MIPS stack pointer offset into GPR */
#define MIPS_V0           2
#define MIPS_V1           3
#endif

void panda_cleanup(void);
void panda_set_os_name(char *os_name);
void panda_before_find_fast(void);
void panda_disas(FILE *out, void *code, unsigned long size);
void panda_break_main_loop(void);
MemoryRegion* panda_find_ram(void);
    
extern bool panda_exit_loop;
extern bool panda_break_vl_loop_req;


target_ulong panda_current_asid(CPUState *env);
    
target_ulong panda_current_pc(CPUState *cpu);

Int128 panda_find_max_ram_address(void);

int panda_physical_memory_rw(hwaddr addr, uint8_t *buf, int len, bool is_write);
int panda_physical_memory_read(hwaddr addr, uint8_t *buf, int len);
int panda_physical_memory_write(hwaddr addr, uint8_t *buf, int len);
hwaddr panda_virt_to_phys(CPUState * env, target_ulong addr);
bool enter_priv(CPUState * cpu);
void exit_priv(CPUState * cpu);
int panda_virtual_memory_rw(CPUState * cpu, target_ulong addr,
                            uint8_t * buf, int len, bool is_write);
int panda_virtual_memory_read(CPUState * env, target_ulong addr,
                              uint8_t * buf, int len);
int panda_virtual_memory_write(CPUState * env, target_ulong addr,
                               uint8_t * buf, int len);
void *panda_map_virt_to_host(CPUState * env, target_ulong addr, int len);
// MemTxResult PandaPhysicalAddressToRamOffset(ram_addr_t* out, hwaddr addr, bool is_write);
// MemTxResult PandaVirtualAddressToRamOffset(ram_addr_t* out, CPUState* cpu, target_ulong addr, bool is_write);

bool panda_in_kernel_mode(const CPUState *cpu);
bool panda_in_kernel(const CPUState *cpu);
bool address_in_kernel_code_linux(target_ulong addr);
bool panda_in_kernel_code_linux(CPUState * cpu);
target_ulong panda_current_ksp(CPUState * cpu);
target_ulong panda_current_sp(const CPUState *cpu);
target_ulong panda_get_retval(const CPUState *cpu);
void panda_set_retval(CPUState *cpu, target_ulong arg);
target_ulong panda_get_syscall_arg(CPUState *cpu, int arg);
void panda_set_syscall_arg(CPUState *cpu, int arg, target_ulong value);
#ifdef __cplusplus
}
#endif

/* vim:set tabstop=4 softtabstop=4 expandtab: */
