#include "kernel_osi.h"
#include "osi/windows/wintrospection.h"

bool find_kernel_base_i386(VirtualMemory* vmem, vm_addr_t kpcr, vm_addr_t* base)
{
    vm_addr_t kpcr_self = 0;
    auto status =
        vmem->read_pointer(kpcr + static_offsets::i386::KPCR_SELF_OFFSET, &kpcr_self);
    if (TSTAT_SUCCESS != status) {
        fprintf(stderr, "Failed to read KPCR.Self (%" PRIxVMADDR ")\n", kpcr);
        return false;
    }

    if (kpcr_self != kpcr) {
        fprintf(stderr, "KPCR failed self-validation check (%" PRIxVMADDR " vs %" PRIxVMADDR ")\n", kpcr,
                kpcr_self);
        return false;
    }

    vm_addr_t current_prcb = 0;
    status = vmem->read_pointer(kpcr + static_offsets::i386::KPCR_CURRENT_PRCB_OFFSET,
                                &current_prcb);
    if (TSTAT_SUCCESS != status) {
        fprintf(stderr, "Failed to read KPCR.CurrentPrcb\n");
        return false;
    }

    vm_addr_t idle_thread = 0;
    status = vmem->read_pointer(current_prcb + static_offsets::i386::KPRCB_IDLE_THREAD,
                                &idle_thread);

    if (TSTAT_SUCCESS != status) {
        fprintf(stderr, "Failed to read IdleThread from KPRCB\n");
        return false;
    }

    // Scan down by page for kernel base
    vm_addr_t scanner = idle_thread & 0xFFFFFFFFFFFFF000;
    vm_addr_t lower_bound = scanner - 0x1000000;
    while (scanner > lower_bound) {
        try {
            uint16_t test = 0;
            vmem->read(scanner, &test, 2);
            if (test == 0x5a4d) {
                *base = scanner;
                return true;
            }
            scanner -= 0x1000;
        } catch (...) {
            break;
        }
    }
    fprintf(stderr, "IdleThread scan for kernel base failed\n");
    return false;
}

bool find_kernel_base_amd64(VirtualMemory* vmem, vm_addr_t kpcr, vm_addr_t* base)
{
    vm_addr_t kpcr_self = 0;
    auto status =
        vmem->read_pointer(kpcr + static_offsets::amd64::KPCR_SELF_OFFSET, &kpcr_self);
    if (TSTAT_SUCCESS != status) {
        fprintf(stderr, "Failed to read KPCR.Self (%" PRIxVMADDR ")\n", kpcr);
        return false;
    }

    if (kpcr_self != kpcr) {
        fprintf(stderr, "KPCR failed self-validation check (%" PRIxVMADDR " vs %" PRIxVMADDR ")\n", kpcr,
                kpcr_self);
        return false;
    }

    vm_addr_t current_prcb = 0;
    status = vmem->read_pointer(kpcr + static_offsets::amd64::KPCR_CURRENT_PRCB_OFFSET,
                                &current_prcb);
    if (TSTAT_SUCCESS != status) {
        fprintf(stderr, "Failed to read KPCR.CurrentPrcb\n");
        return false;
    }

    vm_addr_t idle_thread = 0;
    status = vmem->read_pointer(current_prcb + static_offsets::amd64::KPRCB_IDLE_THREAD,
                                &idle_thread);

    if (TSTAT_SUCCESS != status) {
        fprintf(stderr, "Failed to read IdleThread from KPRCB\n");
        return false;
    }

    // Scan down by page for kernel base
    vm_addr_t scanner = idle_thread & 0xFFFFFFFFFFFFF000;
    vm_addr_t lower_bound = scanner - 0x1000000;
    while (scanner > lower_bound) {
        try {
            uint16_t test = 0;
            vmem->read(scanner, &test, 2);
            if (test == 0x5a4d) {
                *base = scanner;
                return true;
            }
            scanner -= 0x1000;
        } catch (...) {
            break;
        }
    }
    fprintf(stderr, "IdleThread scan for kernel base failed\n");
    return false;
}

bool find_kernel_base(VirtualMemory* vmem, vm_addr_t kpcr, vm_addr_t* base)
{
    uint8_t pointer_width = vmem->get_pointer_width();
    if (pointer_width == 4) {
        return find_kernel_base_i386(vmem, kpcr, base);
    } else if (pointer_width == 8) {
        return find_kernel_base_amd64(vmem, kpcr, base);
    } else {
        return false;
    }
}

bool scan_for_kdbg(VirtualMemory* vmem, vm_addr_t kernel_base, vm_addr_t* kdbg)
{
    vm_addr_t scanner = kernel_base;
    uint8_t ptr_width = vmem->get_pointer_width();
    vm_addr_t offset = (ptr_width == 4) ? static_offsets::i386::KDBG_TAG_OFFSET
                                        : static_offsets::amd64::KDBG_TAG_OFFSET;

    if (*kdbg >= kernel_base) {
        uint32_t test = 0;
        auto status = vmem->read(*kdbg + offset, &test, 4);
        if (status == TSTAT_SUCCESS && test == 0x4742444b) {
            return true;
        } else {
            fprintf(stderr, "Non-zero kdbg hint didn't work: %" PRIxVMADDR "\n", *kdbg);
        }
    }

    vm_addr_t maxoffset = scanner + 0x1000000;
    while (scanner < maxoffset) {
        uint32_t data = 0;
        auto status = vmem->read(scanner, &data, 4);
        if (status == TSTAT_SUCCESS && data == 0x4742444b) {
            *kdbg = scanner - offset;
            return true;
        }
        scanner += ptr_width;
    }
    *kdbg = 0;
    return false;
}

bool scan_for_version64(VirtualMemory* vmem, vm_addr_t kdbg, vm_addr_t kernel_base,
                        vm_addr_t* version64)
{
    vm_addr_t dbgkd_off = kdbg & 0xFFFFFFFFFFFFF000;
    vm_addr_t dbgkd_end = dbgkd_off + 0x1000;
    while (dbgkd_off <= dbgkd_end) {
        vm_addr_t find_kernbase = 0;
        vmem->read_pointer(dbgkd_off, &find_kernbase);
        if (find_kernbase == kernel_base) {
            if (vmem->get_pointer_width() == 8) {
                *version64 = dbgkd_off - 0x10;
            } else {
                *version64 = dbgkd_off - 0x10;
            }
            return true;
        }
        dbgkd_off += vmem->get_pointer_width();
    }
    return false;
}

vm_addr_t get_address_active_process_head(VirtualMemory* vmem, vm_addr_t kdbg)
{
    vm_addr_t dbgkd_pslist = kdbg + static_offsets::KDBG_PSACTIVEPROCESSHEAD;
    vm_addr_t psactiveprocesshead = 0;
    vmem->read_pointer(dbgkd_pslist, &psactiveprocesshead);
    vm_addr_t first_entry = 0;
    vmem->read_pointer(psactiveprocesshead, &first_entry);
    return first_entry;
}

vm_addr_t get_address_loaded_module_head(VirtualMemory* vmem, vm_addr_t kdbg)
{
    vm_addr_t dbgkd_modlist = kdbg + static_offsets::KDBG_PSLOADEDMODULELIST;
    vm_addr_t psloadedmodulelist = 0;
    vmem->read_pointer(dbgkd_modlist, &psloadedmodulelist);
    vm_addr_t first_entry = 0;
    vmem->read_pointer(psloadedmodulelist, &first_entry);
    return first_entry;
}
