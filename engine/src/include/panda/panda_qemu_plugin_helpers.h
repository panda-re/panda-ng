struct qemu_plugin_tb;
CPUState *panda_cpu_by_index(int index);
CPUState *panda_cpu_in_translate(void);
TranslationBlock *panda_get_tb(struct qemu_plugin_tb *tb);

/* What the TCG plugin bridge instruments a TB for, from the PANDA callbacks registered (enabled or
 * not: enabling one later needs no retranslation). Nothing registered, nothing instrumented. */
#define PANDA_BRIDGE_INSN        1  /* insn_translate (which asks for insn_exec) */
#define PANDA_BRIDGE_BLOCK       2  /* a block-exec callback: call panda_block_start at each block's start */
unsigned panda_bridge_wants(void);
/* The qemu_plugin_mem_rw (direction and phases) to register for reads (is_store false) or
 * writes; 0 for none. */
int panda_memcb_rw(bool is_store);
/* The physical address of @vaddr by a page walk, or -1: for a before-access event, whose page
 * may not be in the TLB yet (the plugin API then has no hwaddr). */
uint64_t panda_bridge_virt_to_phys(CPUState *cpu, uint64_t vaddr);
/* Is a phys_mem_before_read (is_store false) or _write callback registered? */
bool panda_memcb_phys_before(bool is_store);
/* Block-exec callbacks, all served from the block's own code, so TB chaining can stay on.
 * panda_block_start runs at a block's first instruction (the bridge's TB-start callback): the
 * block before it on this vCPU has ended (end_block_exec, then after_block_exec with TB_EXIT_IDX0);
 * then before_block_exec_invalidate_opt (true: drop the TB and translate it again), before_block_exec,
 * start_block_exec. QEMU has no hook after a block's last instruction, so the block stays pending
 * and ends at whatever the vCPU does next (panda_end_block_flush): start another block, chained or
 * not; return from the TB (cpu_tb_exec's after_block_exec hook); go idle or exit. Leaving the block
 * by longjmp (a fault, an exception its last instruction raises, cpu_loop_exit_noexc) ends it with
 * TB_EXIT_REQUESTED (panda_callbacks_block_longjmp, from cpu_exec_longjmp_cleanup), before the
 * exception is delivered. The CPU state is then the block's final state. A block whose start
 * callback left it (panda_break_exec) gets after_block_exec but no end_block_exec. A code cache
 * flush drops what is pending: its TBs are gone. A block that exits at its entry (an interrupt
 * pending) never starts and gets no callback. */
void panda_block_start(CPUState *cpu, TranslationBlock *tb);
/* Leave a block from its start (invalidate_opt true, panda_break_exec in start_block_exec):
 * cpu_loop_exit_noexc, with the block's icount budget given back first. */
void panda_block_leave_at_start(CPUState *cpu, TranslationBlock *tb);   /* noreturn */
void panda_end_block_flush(CPUState *cpu);
void panda_end_block_drop_all(void);
/* The bridge passes a setter for its per-vCPU gate words (cpu_index -1: every vCPU's). Its
 * block-start callback is conditional on the vCPU's word, so registered-but-disabled callbacks cost
 * a compare per block. Enabling a block-exec callback opens every vCPU's gate at once; disabling
 * the last one closes a vCPU's gate at its next block start, which ends the block it was running
 * (its after/end_block_exec run then, if still enabled). panda_block_gate: is one enabled? */
void panda_bridge_set_block_gate(void (*set)(int cpu_index, uint64_t on));
uint64_t panda_block_gate(void);
