#include "panda/common.h"
#include "system/physmem.h"
#include "system/ramblock.h"
#include "exec/translation-block.h"

/* (not kernel-doc)
 * panda_physical_memory_rw() - Copy data between host and guest.
 * @addr: Guest physical addr of start of read or write.
 * @buf: Host pointer to a buffer either containing the data to be
 *    written to guest memory, or into which data will be copied
 *    from guest memory.
 * @len: The number of bytes to copy
 * @is_write: If true, then buf will be copied into guest
 *    memory, else buf will be copied out of guest memory.
 *
 * Either reads memory out of the guest into a buffer if
 * (is_write==false), or writes data from a buffer into guest memory
 * (is_write==true). Note that buf has to be big enough for read or
 * write indicated by len.
 *
 * Return:
 * * MEMTX_OK      - Read/write succeeded
 * * MEMTX_ERROR   - An error 
 */
/*
 * After a direct write into a RAM region: what address_space_write's invalidate_and_set_dirty
 * does. Invalidate TBs translated from the bytes (stale code ran after a write before), and log
 * the pages dirty (a fast restore copies back only logged pages).
 */
static void panda_physical_memory_written(MemoryRegion *mr, hwaddr addr1, hwaddr len) {
    uint8_t mask = memory_region_get_dirty_log_mask(mr);
    ram_addr_t ra = memory_region_get_ram_addr(mr) + addr1;

    if (mask) {
        mask = physical_memory_range_includes_clean(ra, len, mask);
    }
    if (mask & (1 << DIRTY_MEMORY_CODE)) {
        tb_invalidate_phys_range(NULL, ra, ra + len - 1);
        mask &= ~(1 << DIRTY_MEMORY_CODE);
    }
    physical_memory_set_dirty_range(ra, len, mask);
}

int panda_physical_memory_rw(hwaddr addr, uint8_t *buf, int len,
                                           bool is_write) {
    // return address_space_rw(&address_space_memory, addr, MEMTXATTRS_UNSPECIFIED,
    //                 buf, len, is_write);
    hwaddr l = len;
    hwaddr addr1;
    MemoryRegion *mr = address_space_translate(&address_space_memory, addr,
                                               &addr1, &l, is_write, 
                                               MEMTXATTRS_UNSPECIFIED);

    if (!memory_access_is_direct(mr, is_write, MEMTXATTRS_UNSPECIFIED)) {
        // fail for MMIO regions of physical address space
        return MEMTX_ERROR;
    }
    void *ram_ptr = qemu_map_ram_ptr(mr->ram_block, addr1);

    if (is_write) {
        memcpy(ram_ptr, buf, len);
        panda_physical_memory_written(mr, addr1, len);
    } else {
        memcpy(buf, ram_ptr, len);
    }
    return MEMTX_OK;
}


/* (not kernel-doc)
 * panda_physical_memory_read() - Copy data from guest memory into host buffer.
 * @addr: Guest physical address of start of read.
 * @buf: Host pointer to a buffer into which data will be copied from guest.
 * @len: Number of bytes to copy.
 * 
 * Return: 
 * * MEMTX_OK      - Read succeeded
 * * MEMTX_ERROR   - An error
 */
int panda_physical_memory_read(hwaddr addr,
                                            uint8_t *buf, int len) {
    return panda_physical_memory_rw(addr, buf, len, 0);
}


/* (not kernel-doc)
 * panda_physical_memory_write() - Copy data from host buffer into guest memory.
 * @addr: Guest physical address of start of desired write.
 * @buf: Host pointer to a buffer from which data will be copied into guest.
 * @len: Number of bytes to copy.
 * 
 * Return: 
 * * MEMTX_OK      - Write succeeded
 * * MEMTX_ERROR   - An error
 */
int panda_physical_memory_write(hwaddr addr,
                                             uint8_t *buf, int len) {
    return panda_physical_memory_rw(addr, buf, len, 1);
}


/**
 * panda_virt_to_phys() - Translate guest virtual to physical address.
 * @env: Cpu state.
 * @addr: Guest virtual address.
 *
 * This conversion will fail if asked about a virtual address that is
 * not currently mapped to a physical one in the guest. Good luck on MIPS.
 *
 * Return: A guest physical address.
 */

/* cpu_get_phys_page_debug() before QEMU 11: the page's physical address, or -1 */
static hwaddr phys_page_debug(CPUState *env, target_ulong page)
{
    TranslateForDebugResult res;

    if (!cpu_translate_for_debug(env, page, &res)) {
        return -1;
    }
    return res.physaddr & TARGET_PAGE_MASK;
}

hwaddr panda_virt_to_phys(CPUState *env, target_ulong addr) {
    target_ulong page;
    hwaddr phys_addr;
    bool changed_priv = false;
    page = addr & TARGET_PAGE_MASK;
    phys_addr = phys_page_debug(env, page);
    // As panda_virtual_memory_rw: a kernel address translated from user mode (arm64 EL0
    // can't see TTBR1's mappings) needs privileged mode.
    if (phys_addr == -1 && (changed_priv = enter_priv(env))) {
        phys_addr = phys_page_debug(env, page);
        exit_priv(env);
    }
    if (phys_addr == -1) {
        // no physical page mapped
        return -1;
    }
    phys_addr += (addr & ~TARGET_PAGE_MASK);
    return phys_addr;
}

/* (not kernel-doc)
 * panda_virtual_memory_rw() - Copy data between host and guest.
 * @env: Cpu sate.
 * @addr: Guest virtual addr of start of read or write.
 * @buf: Host pointer to a buffer either containing the data to be
 *    written to guest memory, or into which data will be copied
 *    from guest memory.
 * @len: The number of bytes to copy
 * @is_write: If true, then buf will be copied into guest
 *    memory, else buf will be copied out of guest memory.
 *
 * Either reads memory out of the guest into a buffer if
 * (is_write==false), or writes data from a buffer into guest memory
 * (is_write==true). Note that buf has to be big enough for read or
 * write indicated by len. Also note that if the virtual address is
 * not mapped, then the read or write will fail.
 *
 * We switch into privileged mode if the access fails. The mode is always reset
 * before we return.
 * 
 * Return:
 * * 0      - Read/write succeeded
 * * -1     - An error 
 */
int panda_virtual_memory_rw(CPUState *env, target_ulong addr,
                                          uint8_t *buf, int len, bool is_write) {
    int l;
    int ret;
    hwaddr phys_addr;
    target_ulong page;
    bool changed_priv = false;

    while (len > 0) {
        page = addr & TARGET_PAGE_MASK;
        phys_addr = phys_page_debug(env, page);
        // If we failed and we aren't in priv mode and we CAN go into it, toggle modes and try again
        if (phys_addr == -1  && !changed_priv && (changed_priv=enter_priv(env))) {
            phys_addr = phys_page_debug(env, page);
            //if (phys_addr != -1) printf("[panda dbg] virt->phys failed until privileged mode\n");
        }

        // No physical page mapped, even after potential privileged switch, abort
        if (phys_addr == -1)  {
            if (changed_priv) exit_priv(env); // Cleanup mode if necessary
            return -1;
        }

        l = (page + TARGET_PAGE_SIZE) - addr;
        if (l > len) {
            l = len;
        }
        phys_addr += (addr & ~TARGET_PAGE_MASK);
        ret = panda_physical_memory_rw(phys_addr, buf, l, is_write);

        // Failed and privileged mode wasn't already enabled - enable priv and retry if we can
        if (ret != MEMTX_OK && !changed_priv && (changed_priv = enter_priv(env))) {
            ret = panda_physical_memory_rw(phys_addr, buf, l, is_write);
            //if (ret == MEMTX_OK) printf("[panda dbg] accessing phys failed until privileged mode\n");
        }
        // Still failed, even after potential privileged switch, abort
        if (ret != MEMTX_OK) {
            if (changed_priv) exit_priv(env); // Cleanup mode if necessary
            return ret;
        }

        len -= l;
        buf += l;
        addr += l;
    }
    if (changed_priv) exit_priv(env); // Clear privileged mode if necessary
    return 0;
}


/* (not kernel-doc)
 * panda_virtual_memory_read() - Copy data from guest memory into host buffer.
 * @env: Cpu sate.
 * @addr: Guest virtual address of start of desired read
 * @buf: Host pointer to a buffer into which data will be copied from guest.
 * @len: Number of bytes to copy.
 * 
 * Return:
 * * 0      - Read succeeded
 * * -1     - An error 
 */ 
int panda_virtual_memory_read(CPUState *env, target_ulong addr,
                                            uint8_t *buf, int len) {
    return panda_virtual_memory_rw(env, addr, buf, len, 0);
}

    
/* (not kernel-doc)
 * panda_virtual_memory_write() - Copy data from host buffer into guest memory.
 * @env: Cpu sate.
 * @addr: Guest virtual address of start of desired write.
 * @buf: Host pointer to a buffer from which data will be copied into guest.
 * @len: Number of bytes to copy.
 * 
 * Return:
 * * 0      - Write succeeded
 * * -1     - An error 
 */ 
int panda_virtual_memory_write(CPUState *env, target_ulong addr,
                                             uint8_t *buf, int len) {
    return panda_virtual_memory_rw(env, addr, buf, len, 1);
}


/**
 * panda_map_virt_to_host() - Map guest virtual addresses into host.
 * @env: Cpu state.
 * @addr: Guest virtual address start of range.
 * @len: Length of address range.
 * 
 * Returns a pointer to host memory that is an alias for a range of
 * guest virtual addresses.
 *
 * Return: A host pointer.
 */
void *panda_map_virt_to_host(CPUState *env, target_ulong addr,
                                           int len)
{
    hwaddr phys = panda_virt_to_phys(env, addr);
    hwaddr l = len;
    hwaddr addr1;
    MemoryRegion *mr =
        address_space_translate(&address_space_memory, phys, &addr1, &l, true,
                                MEMTXATTRS_UNSPECIFIED);

    if (!memory_access_is_direct(mr, true, MEMTXATTRS_UNSPECIFIED)) {
        return NULL;
    }

    return qemu_map_ram_ptr(mr->ram_block, addr1);
}


/**
 * PandaPhysicalAddressToRamOffset() - Translate guest physical address to ram offset.
 * @out: A pointer to the ram_offset_t, which will be written by this function.
 * @addr: The guest physical address.
 * @is_write: Is this mapping for ultimate read or write.
 *  
 * This function is useful for callers needing to know not merely the
 * size of physical memory, but the actual largest physical address
 * that might arise given non-contiguous ram map.  Panda's taint
 * system needs it to set up its shadow ram, e.g..
 * 
 * Return: The desired return value is pointed to by out.
 * * MEMTX_OK      - Read succeeded
 * * MEMTX_ERROR   - An error
 */
// MemTxResult PandaPhysicalAddressToRamOffset(ram_addr_t* out, hwaddr addr, bool is_write)
// {
//     hwaddr TranslatedAddress;
//     hwaddr AccessLength = 1;
//     MemoryRegion* mr;
//     ram_addr_t RamOffset;

//     rcu_read_lock();
//     mr = address_space_translate(&address_space_memory, addr, &TranslatedAddress, &AccessLength, is_write, MEMTXATTRS_UNSPECIFIED);

//     if (!mr || !memory_region_is_ram(mr) || memory_region_is_ram_device(mr) || memory_region_is_romd(mr) || (is_write && mr->readonly))
//     {
//         /*
//             We only want actual RAM.
//             I can't find a concrete instance of a RAM Device,
//             but from the docs/comments I can find, this seems
//             like the appropriate check.
//         */
//         rcu_read_unlock();
//         return MEMTX_ERROR;
//     }

//     if ((RamOffset = memory_region_get_ram_addr(mr)) == RAM_ADDR_INVALID)
//     {
//         rcu_read_unlock();
//         return MEMTX_ERROR;
//     }

//     rcu_read_unlock();

//     RamOffset += TranslatedAddress;

//     if (RamOffset >= ram_size)
//     {
//         /*
//             HACK
//             For the moment, the taint system (the only consumer of this) will die in very unfortunate
//             ways if the translated offset exceeds the size of "RAM" (the argument given to -m in
//             qemu's invocation)...
//             Unfortunately there's other "RAM" qemu tracks that's not differentiable in a target-independent
//             way. For instance: the PC BIOS memory and VGA memory. In the future it would probably be easier
//             to modify the taint system to use last_ram_offset() rather tham ram_size, and/or register an
//             address space listener to update it's shadow RAM with qemu's hotpluggable memory.
//             From brief observation, the qemu machine implementations seem to map the system "RAM"
//             people are most likely thinking about when they say "RAM" first, so the ram_addr_t values
//             below ram_size should belong to those memory regions. This isn't required however, so beware.
//         */
//         fprintf(stderr, "PandaPhysicalAddressToRamOffset: Translated Physical Address 0x" TARGET_FMT_plx " has RAM Offset Above ram_size (0x" RAM_ADDR_FMT " >= 0x" RAM_ADDR_FMT ")\n", addr, RamOffset, ram_size);
//         return MEMTX_DECODE_ERROR;
//     }

//     if (out)
//         *out = RamOffset;

//     return MEMTX_OK;
// }


/**
 * PandaVirtualAddressToRamOffset() - Translate guest virtual address to ram offset,
 * @out: A pointer to the ram_offset_t, which will be written by this function.
 * @cpu: Cpu state.
 * @addr: The guest virtual address.
 * @is_write: Is this mapping for ultimate read or write.
 *  
 * This function is useful for callers needing to know not merely the
 * size of virtual memory, but the actual largest virtual address that
 * might arise given non-contiguous ram map.  Panda's taint system
 * needs it to set up its shadow ram.
 * 
 * Return: The desired return value is pointed to by out.
 * * MEMTX_OK      - Read succeeded
 * * MEMTX_ERROR   - An error
 */
// MemTxResult PandaVirtualAddressToRamOffset(ram_addr_t* out, CPUState* cpu, target_ulong addr, bool is_write)
// {
//     hwaddr PhysicalAddress = panda_virt_to_phys(cpu, addr);
//     if (PhysicalAddress == (hwaddr)-1)
//         return MEMTX_ERROR;
//     return PandaPhysicalAddressToRamOffset(out, PhysicalAddress, is_write);
// }
