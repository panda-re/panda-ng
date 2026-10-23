#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <qemu-plugin.h>
typedef void* CPUState;
typedef void* Monitor;
typedef void* MachineState;
typedef uint64_t hwaddr;
typedef uint64_t target_ulong;
#include <../panda/callbacks/cb-support.h>
#include <../panda/panda_qemu_plugin_helpers.h>
#include <../qemu/compiler.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;
QEMU_PLUGIN_EXPORT int (*external_plugin_install)(qemu_plugin_id_t id, const qemu_info_t *info,int argc, char **argv);


// A block starts: panda_block_start ends the one before it and runs the block-exec callbacks.
// Conditional on the gate: registered callbacks that are all disabled cost a compare per block.
static struct qemu_plugin_scoreboard *gate_sb;
static qemu_plugin_u64 gate;
static uint64_t gate_on;

static void block_start_cb(unsigned int cpu_index, void *udata)
{
    panda_block_start(panda_cpu_by_index(cpu_index), (TranslationBlock *) udata);
}

static void set_gate(uint64_t on)
{
    gate_on = on;
    for (int i = 0; i < qemu_plugin_num_vcpus(); i++) {
        qemu_plugin_u64_set(gate, i, on);
    }
}

static void vcpu_init(unsigned int cpu_index, void *userdata)
{
    qemu_plugin_u64_set(gate, cpu_index, gate_on);
}

static void vcpu_idle_or_exit(unsigned int cpu_index, void *userdata)
{
    panda_end_block_flush(panda_cpu_by_index(cpu_index));
}

static void code_flushed(void *userdata)
{
    panda_end_block_drop_all();
}

static void insn_exec(unsigned int cpu_index, void *udata)
{
    CPUState *cpu = panda_cpu_by_index(cpu_index);
    panda_callbacks_insn_exec(cpu, (uint64_t) udata);
}

// udata is the accessing instruction's pc.
static void vcpu_mem(unsigned int cpu_index, qemu_plugin_meminfo_t info,
                     uint64_t vaddr, void *udata){
    CPUState *cpu = panda_cpu_by_index(cpu_index);
    bool is_store = qemu_plugin_mem_is_store(info);
    bool is_before = qemu_plugin_mem_is_before(info);
    uint64_t paddr = (uint64_t) -1;
    if (!is_before) {
        struct qemu_plugin_hwaddr *hwaddr_info = qemu_plugin_get_hwaddr(info, vaddr);
        // MMIO accesses have no RAM behind them; mmio_after_read is not served from here yet.
        if (hwaddr_info && qemu_plugin_hwaddr_is_io(hwaddr_info)) {
            return;
        }
        if (hwaddr_info) {
            paddr = qemu_plugin_hwaddr_phys_addr(hwaddr_info);
        }
    } else if (panda_memcb_phys_before(is_store)) {
        // Before the access its page may not be in the TLB, where the plugin API looks (and
        // complains if it isn't): walk the page tables, and only for a phys callback.
        paddr = panda_bridge_virt_to_phys(cpu, vaddr);
    }
    // Before a load there is no value; the size is the access's.
    qemu_plugin_mem_value m = qemu_plugin_mem_get_value(info);
    size_t size;
    void *data;

    switch (m.type){
        case QEMU_PLUGIN_MEM_VALUE_U8:
            size = 1;
            data = &m.data.u8;
            break;
        case QEMU_PLUGIN_MEM_VALUE_U16:
            size = 2;
            data = &m.data.u16;
            break;
        case QEMU_PLUGIN_MEM_VALUE_U32:
            size = 4;
            data = &m.data.u32;
            break;
        case QEMU_PLUGIN_MEM_VALUE_U64:
            size = 8;
            data = &m.data.u64;
            break;
        case QEMU_PLUGIN_MEM_VALUE_U128:
            size = 16;
            data = &m.data.u128;
            break;
        default:
            assert(false);
    }
    panda_callbacks_mem_access(cpu, (uint64_t) udata, vaddr, paddr, size, data, is_store, is_before);
}


// Instruments only for the PANDA callbacks registered (panda_bridge_wants): with none, a TB gets
// no callbacks and its address is never read, so a CF_PCREL TB stays shared between the virtual
// addresses that map it. Callbacks get synced registers (RW_REGS): PANDA plugins read and write
// the CPU state directly.
static void vcpu_tb_trans(struct qemu_plugin_tb *tb, void *userdata)
{
    CPUState *cpu = panda_cpu_in_translate();
    unsigned wants = panda_bridge_wants();
    int mem_rw[2] = { panda_memcb_rw(false), panda_memcb_rw(true) };
    size_t n_insns = qemu_plugin_tb_n_insns(tb);

    if (wants & PANDA_BRIDGE_TB_PC) {
        qemu_plugin_tb_vaddr(tb);   // binds the TB to its address (patch 0035): tb->pc is right
    }
    if ((wants & PANDA_BRIDGE_INSN) || mem_rw[0] || mem_rw[1]) {
        for (size_t i = 0; i < n_insns; i++) {
            struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
            uint64_t insn_pc = qemu_plugin_insn_vaddr(insn);
            if ((wants & PANDA_BRIDGE_INSN) && panda_callbacks_insn_translate(cpu, insn_pc)) {
                qemu_plugin_register_vcpu_insn_exec_cb(insn, insn_exec, QEMU_PLUGIN_CB_RW_REGS,
                                                       (void *) insn_pc);
            }
            for (int s = 0; s < 2; s++) {
                if (mem_rw[s]) {
                    qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem, QEMU_PLUGIN_CB_RW_REGS,
                                                     (enum qemu_plugin_mem_rw) mem_rw[s], (void *) insn_pc);
                }
            }
        }
    }

    TranslationBlock *real_tb = panda_get_tb(tb);
    if (wants & PANDA_BRIDGE_BLOCK) {
        qemu_plugin_register_vcpu_tb_exec_cond_cb(tb, block_start_cb, QEMU_PLUGIN_CB_RW_REGS,
                                                  QEMU_PLUGIN_COND_NE, gate, 0, real_tb);
    }

    panda_callbacks_block_translate(cpu, tb);
}

// static void vcpu_init(qemu_plugin_id_t id, unsigned int vcpu_index)
// {
//     printf("vcpu_init %d\n", vcpu_index);
// }

// static void vcpu_exit(qemu_plugin_id_t id, unsigned int vcpu_index)
// {
//     printf("vcpu_exit %d\n", vcpu_index);
// }

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    gate_sb = qemu_plugin_scoreboard_new(sizeof(uint64_t));
    gate = qemu_plugin_scoreboard_u64(gate_sb);
    qemu_plugin_register_vcpu_init_cb(id, vcpu_init, NULL);
    panda_bridge_set_block_gate(set_gate);
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans, NULL);
    qemu_plugin_register_vcpu_idle_cb(id, vcpu_idle_or_exit, NULL);
    qemu_plugin_register_vcpu_exit_cb(id, vcpu_idle_or_exit, NULL);
    qemu_plugin_register_flush_cb(id, code_flushed, NULL);
    return 0;
}
