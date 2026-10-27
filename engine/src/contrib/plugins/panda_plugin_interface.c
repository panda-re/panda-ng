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

static void block_start_cb(unsigned int cpu_index, void *udata)
{
    panda_block_start(panda_cpu_by_index(cpu_index), (TranslationBlock *) udata);
}

// cpu_index -1: every vCPU's gate (PANDA opens them all at once and closes each lazily, from the
// vCPU's own block start; see panda_bridge_set_block_gate)
static void set_gate(int cpu_index, uint64_t on)
{
    if (cpu_index >= 0) {
        qemu_plugin_u64_set(gate, cpu_index, on);
        return;
    }
    for (int i = 0; i < qemu_plugin_num_vcpus(); i++) {
        qemu_plugin_u64_set(gate, i, on);
    }
}

static void vcpu_init(unsigned int cpu_index, void *userdata)
{
    qemu_plugin_u64_set(gate, cpu_index, panda_block_gate());
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
//
// Device (MMIO) accesses get the general memory callbacks too, with the device's physical address,
// as in classic PANDA. They also get the dedicated ones: mmio_after_read after a load from a
// device, mmio_before_write before a store to one. Up to QEMU 11.1.0 qemu_plugin_hwaddr_is_io()
// never reported MMIO; from 11.1.2 (upstream 14375dd) it does. A store has no hwaddr before the
// access: panda_bridge_before_phys says whether it goes to a device, asked while an
// mmio_before_write callback is enabled. The value the mmio callbacks get is a copy: the plugin
// API gives the loaded or stored value but has no way to change it, so a callback that writes
// *val changes nothing the guest or the device sees (classic PANDA used the changed value).
// Ordered as classic PANDA's: mmio_after_read runs before the after-read callbacks (it ran in
// the device read, inside the load), mmio_before_write after the before-write ones (in the device
// write). 16-byte accesses get no mmio callback: the value doesn't fit their uint64_t.
//
// A device access from an instruction that isn't its TB's last doesn't happen as such: QEMU
// rewinds the TB and runs the instruction again alone (panda_bridge_io_rewinds). Its before events
// would come twice, the first time for an access that never happened; those are dropped, which
// takes the bridge's TLB lookup before each such access while a before-access callback is enabled.
static void vcpu_mem(unsigned int cpu_index, qemu_plugin_meminfo_t info,
                     uint64_t vaddr, void *udata){
    CPUState *cpu = panda_cpu_by_index(cpu_index);
    bool is_store = qemu_plugin_mem_is_store(info);
    bool is_before = qemu_plugin_mem_is_before(info);
    uint64_t paddr = (uint64_t) -1;
    bool is_io = false;
    if (!is_before) {
        struct qemu_plugin_hwaddr *hwaddr_info = qemu_plugin_get_hwaddr(info, vaddr);
        if (hwaddr_info) {
            paddr = qemu_plugin_hwaddr_phys_addr(hwaddr_info);
            is_io = qemu_plugin_hwaddr_is_io(hwaddr_info);
        }
    } else if (panda_memcb_before_enabled(is_store)) {
        // Before the access its page may not be in the TLB yet, where the plugin API looks (and
        // complains if it isn't): the TLB entry if there is one, else a page walk; only for an
        // enabled phys or mmio callback, or when a device access would be rewound: from an
        // instruction that isn't its TB's last, a device access doesn't happen; QEMU runs the
        // instruction again alone, and the before events come again then. Report only those.
        bool rewinds = panda_bridge_io_rewinds(cpu);
        bool want_io = rewinds || (is_store && panda_memcb_mmio_before_write());
        if (want_io || panda_memcb_phys_before(is_store)) {
            paddr = panda_bridge_before_phys(cpu, info, vaddr, is_store, want_io ? &is_io : NULL);
        }
        if (is_io && rewinds) {
            return;
        }
    } else {
        return;     // no before-access callback enabled
    }
    // Before a load there is no value; the size is the access's.
    qemu_plugin_mem_value m = qemu_plugin_mem_get_value(info);
    size_t size;
    void *data;
    uint64_t val;

    switch (m.type){
        case QEMU_PLUGIN_MEM_VALUE_U8:
            size = 1;
            data = &m.data.u8;
            val = m.data.u8;
            break;
        case QEMU_PLUGIN_MEM_VALUE_U16:
            size = 2;
            data = &m.data.u16;
            val = m.data.u16;
            break;
        case QEMU_PLUGIN_MEM_VALUE_U32:
            size = 4;
            data = &m.data.u32;
            val = m.data.u32;
            break;
        case QEMU_PLUGIN_MEM_VALUE_U64:
            size = 8;
            data = &m.data.u64;
            val = m.data.u64;
            break;
        case QEMU_PLUGIN_MEM_VALUE_U128:
            size = 16;
            data = &m.data.u128;
            val = 0;
            is_io = false;
            break;
        default:
            assert(false);
    }
    if (is_io && !is_store && !is_before) {
        panda_callbacks_mmio_after_read(cpu, paddr, vaddr, size, &val);
    }
    panda_callbacks_mem_access(cpu, (uint64_t) udata, vaddr, paddr, size, data, is_store, is_before);
    if (is_io && is_store && is_before) {
        panda_callbacks_mmio_before_write(cpu, paddr, vaddr, size, &val);
    }
}


// Instruments only for the PANDA callbacks registered (panda_bridge_wants): with none, a TB gets
// no callbacks and its address is never read, so a CF_PCREL TB stays shared between the virtual
// addresses that map it. Block callbacks don't read it either (they get the block's address from
// panda_tb_pc); reading an instruction's address (insn_translate, memory callbacks) binds the TB
// to it (patch 0035): those callbacks are keyed by address. Callbacks get synced registers
// (RW_REGS): PANDA plugins read and write the CPU state directly.
static void vcpu_tb_trans(struct qemu_plugin_tb *tb, void *userdata)
{
    CPUState *cpu = panda_cpu_in_translate();
    unsigned wants = panda_bridge_wants();
    int mem_rw[2] = { panda_memcb_rw(false), panda_memcb_rw(true) };
    size_t n_insns = qemu_plugin_tb_n_insns(tb);

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
