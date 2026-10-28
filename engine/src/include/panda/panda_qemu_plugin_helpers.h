struct qemu_plugin_tb;
CPUState *panda_cpu_by_index(int index);
CPUState *panda_cpu_in_translate(void);
TranslationBlock *panda_get_tb(struct qemu_plugin_tb *tb);

/* Insn/memory shape follows registration; block shape follows enable state.
 * First block enable and last disable flush existing translations. */
#define PANDA_BRIDGE_INSN  1
#define PANDA_BRIDGE_BLOCK 2  /* actual entry */
#define PANDA_BRIDGE_END   4  /* normal completion, before outgoing transfer */
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
/* TCG entry and normal-completion callbacks. End runs before any outgoing
 * link or dispatcher return, after the block's operations and PC update.
 * Faults, exceptions, prologue exits and start-callback aborts have no end
 * event. No deferred delivery, pending-block state, or pairing guarantee.
 * Legacy before/after names dispatch at these same points. */
void panda_block_start(CPUState *cpu, TranslationBlock *tb);
void panda_block_end(CPUState *cpu, TranslationBlock *tb);
void panda_block_leave_at_start(CPUState *cpu, TranslationBlock *tb); /* noreturn */
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
