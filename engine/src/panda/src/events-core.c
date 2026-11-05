/*
 * The core callback types as registry events: "cb.<type>" in PANDA_EVENT_NS.
 *
 * The signature is the callback's parameters (cb-defs.h), for listings and identity; dispatch
 * still goes through the core lists (MAKE_CALLBACK), which are these events' subscribers. A
 * callback's return value is not part of it. Types with no trampoline (before_loadvm and the
 * replay_* types) are left out, and registering them is refused.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "panda/plugin.h"
#include "panda/events-internal.h"

#define CORE(type, name, sig) { PANDA_CB_##type, #name, sig, false }

const PandaCoreEventDef panda_core_events[] = {
    CORE(BEFORE_BLOCK_TRANSLATE, before_block_translate, "cpu:ptr,pc:u64"),
    CORE(AFTER_BLOCK_TRANSLATE, after_block_translate, "cpu:ptr,tb:ptr"),
    CORE(BLOCK_TRANSLATE, block_translate, "cpu:ptr,tb:ptr"),
    CORE(BEFORE_BLOCK_EXEC_INVALIDATE_OPT, before_block_exec_invalidate_opt, "cpu:ptr,tb:ptr"),
    CORE(BEFORE_TCG_CODEGEN, before_tcg_codegen, "cpu:ptr,tb:ptr"),
    CORE(BEFORE_BLOCK_EXEC, before_block_exec, "cpu:ptr,tb:ptr"),
    CORE(AFTER_BLOCK_EXEC, after_block_exec, "cpu:ptr,tb:ptr,exit_code:u8"),
    CORE(INSN_TRANSLATE, insn_translate, "cpu:ptr,pc:u64"),
    CORE(INSN_EXEC, insn_exec, "cpu:ptr,pc:u64"),
    CORE(AFTER_INSN_TRANSLATE, after_insn_translate, "cpu:ptr,pc:u64"),
    CORE(AFTER_INSN_EXEC, after_insn_exec, "cpu:ptr,pc:u64"),
    CORE(VIRT_MEM_BEFORE_READ, virt_mem_before_read, "cpu:ptr,pc:u64,addr:u64,size:u64"),
    CORE(VIRT_MEM_BEFORE_WRITE, virt_mem_before_write,
         "cpu:ptr,pc:u64,addr:u64,size:u64,buf:ptr"),
    CORE(PHYS_MEM_BEFORE_READ, phys_mem_before_read, "cpu:ptr,pc:u64,addr:u64,size:u64"),
    CORE(PHYS_MEM_BEFORE_WRITE, phys_mem_before_write,
         "cpu:ptr,pc:u64,addr:u64,size:u64,buf:ptr"),
    CORE(VIRT_MEM_AFTER_READ, virt_mem_after_read, "cpu:ptr,pc:u64,addr:u64,size:u64,buf:ptr"),
    CORE(VIRT_MEM_AFTER_WRITE, virt_mem_after_write, "cpu:ptr,pc:u64,addr:u64,size:u64,buf:ptr"),
    CORE(PHYS_MEM_AFTER_READ, phys_mem_after_read, "cpu:ptr,pc:u64,addr:u64,size:u64,buf:ptr"),
    CORE(PHYS_MEM_AFTER_WRITE, phys_mem_after_write, "cpu:ptr,pc:u64,addr:u64,size:u64,buf:ptr"),
    CORE(MMIO_AFTER_READ, mmio_after_read, "cpu:ptr,physaddr:u64,vaddr:u64,size:u64,val:ptr"),
    CORE(MMIO_BEFORE_WRITE, mmio_before_write, "cpu:ptr,physaddr:u64,vaddr:u64,size:u64,val:ptr"),
    CORE(HD_READ, hd_read, "cpu:ptr"),
    CORE(HD_WRITE, hd_write, "cpu:ptr"),
    { PANDA_CB_GUEST_HYPERCALL, "guest_hypercall", "cpu:ptr", true },
    CORE(MONITOR, monitor, "mon:ptr,cmd:ptr"),
    CORE(QMP, qmp, "command:ptr,args:ptr,result:ptr"),
    CORE(CPU_RESTORE_STATE, cpu_restore_state, "cpu:ptr,tb:ptr"),
    CORE(ASID_CHANGED, asid_changed, "cpu:ptr,old_asid:u64,new_asid:u64"),
    CORE(AFTER_CPU_EXEC_ENTER, after_cpu_exec_enter, "cpu:ptr"),
    CORE(BEFORE_CPU_EXEC_EXIT, before_cpu_exec_exit, "cpu:ptr,ran_block:u8"),
    CORE(AFTER_MACHINE_INIT, after_machine_init, "cpu:ptr"),
    CORE(AFTER_LOADVM, after_loadvm, "cpu:ptr"),
    CORE(TOP_LOOP, top_loop, "cpu:ptr"),
    CORE(DURING_MACHINE_INIT, during_machine_init, "machine:ptr"),
    CORE(MAIN_LOOP_WAIT, main_loop_wait, ""),
    CORE(PRE_SHUTDOWN, pre_shutdown, ""),
    CORE(UNASSIGNED_IO_READ, unassigned_io_read, "cpu:ptr,pc:u64,addr:u64,size:u64,val:ptr"),
    CORE(UNASSIGNED_IO_WRITE, unassigned_io_write, "cpu:ptr,pc:u64,addr:u64,size:u64,val:u64"),
    CORE(BEFORE_HANDLE_EXCEPTION, before_handle_exception, "cpu:ptr,exception_index:i32"),
    CORE(BEFORE_HANDLE_INTERRUPT, before_handle_interrupt, "cpu:ptr,interrupt_request:i32"),
    CORE(START_BLOCK_EXEC, start_block_exec, "cpu:ptr,tb:ptr"),
    CORE(END_BLOCK_EXEC, end_block_exec, "cpu:ptr,tb:ptr"),
    CORE(SYSCALL_ENTER, syscall_enter, "cpu:ptr,sc:ptr"),
    CORE(SYSCALL_RETURN, syscall_return, "cpu:ptr,sc:ptr"),
};

const size_t panda_core_events_count = ARRAY_SIZE(panda_core_events);

uint32_t panda_core_event_modes(int type)
{
    return panda_callback_supported_modes(type);
}
