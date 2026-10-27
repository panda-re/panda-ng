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
 * writes; 0 for none. mmio_after_read needs every load's after event and mmio_before_write every
 * store's before event: whether the access went (or goes) to a device is known per access. */
int panda_memcb_rw(bool is_store);
/* The physical address of @vaddr by a page walk, or -1: for a before-access event, whose page
 * may not be in the TLB yet (the plugin API then has no hwaddr). */
uint64_t panda_bridge_virt_to_phys(CPUState *cpu, uint64_t vaddr);
/* The physical address of @vaddr for a before-access event (@info: its qemu_plugin_meminfo_t), or
 * -1: the softmmu TLB entry the access is about to use (its mmu index and direction), and only on
 * a TLB miss a page walk (as panda_bridge_virt_to_phys). For RAM and I/O alike. With @is_io, also
 * whether the access will go to a device rather than RAM (the TLB entry's TLB_MMIO for that
 * direction; on a miss, the memory region at the address); false when there is no address. */
uint64_t panda_bridge_before_phys(CPUState *cpu, uint32_t info, uint64_t vaddr, bool is_store,
                                  bool *is_io);
/* Is a phys_mem_before_read (is_store false) or _write callback enabled? (Registered and
 * disabled: no page walk.) */
bool panda_memcb_phys_before(bool is_store);
/* Is an mmio_before_write callback enabled? (Registered and disabled: no lookup before stores.) */
bool panda_memcb_mmio_before_write(void);
/* Is a before-access callback for loads (is_store false) or stores enabled? */
bool panda_memcb_before_enabled(bool is_store);
/* Would a device access by the instruction running now be rewound? An instruction that is not the
 * last of its TB may not access a device: io_prepare rewinds the TB (cpu_io_recompile) and runs the
 * instruction again alone. Its before-access events then came twice, the first time for an access
 * that never happened; the bridge drops that first one. */
bool panda_bridge_io_rewinds(CPUState *cpu);
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
/* Edge coverage (panda/src/panda_cov.c). While on, the bridge registers a callback per translated
 * block that panda_cov_block accepts (id in *cur, it says which), which does AFL's
 * map[cur ^ prev]++, prev = cur >> 1 for its vCPU. */
#define PANDA_COV_MAX_CPUS 256
#define PANDA_COV_MAP_DEFAULT (1u << 16)
typedef struct PandaCov {
    uint8_t *map;
    uint32_t mask;
    bool on;
    uint64_t lo, hi;                        /* the filter: [lo, hi) on the block's address, hi 0: all */
    uint32_t prev[PANDA_COV_MAX_CPUS];      /* per vCPU (index modulo PANDA_COV_MAX_CPUS) */
} PandaCov;
extern PandaCov panda_cov;
bool panda_cov_block(uint32_t *cur);
