
#include "osi/linux/iterator.h"
#include "osi/linux/lintrospection.h"

#include "iohal/memory/common.h"
#include "iohal/memory/virtual_memory_translator.h"

#include "offset/i_t.h"

#include "kernel_osi.h"

bool get_init_task_addr(struct LinuxKernelOSI* kosi, vm_addr_t thread_info_address,
                        uint64_t* initial_task_addr)
{
    osi::i_t thread_info(kosi->system_vmem, kosi->kernel_tlib, thread_info_address,
                         "thread_info");

    osi::i_t group_leader = thread_info("task").set_type("task_struct")("group_leader");

    if (group_leader.get_address() == 0) {
        return false;
    }

    osi::task_iterator tasks(group_leader, "tasks");
    for (; !tasks.is_init_task(); tasks++) {
    }

    *initial_task_addr = (*tasks).get_address();

    return true;
}

bool initialize_linux_kernel_osi(struct LinuxKernelOSI* kosi, uint64_t kernel_stack_ptr,
                                 bool pae)
{
    auto kdetails = kosi->details;

    if (!kdetails || !kosi) {
        return false;
    }

    if (!(kosi->kernel_tlib) || !(kosi->pmem)) {
        fprintf(
            stderr,
            "The kernel type library and physical memory interface must be provided\n");
        return false;
    }

    if (!(kdetails->pointer_width) || !(kdetails->initial_task_asid) ||
        !kernel_stack_ptr) {
        fprintf(stderr, "The initial task address, it's address space ID, and pointer "
                        "width must be provided\n");
        return false;
    }

    uint64_t thread_info_address =
        GET_THREAD_INFO_FROM_ESP0(kernel_stack_ptr, kdetails->pointer_width);
    auto bits = (kdetails->pointer_width > 4) ? 64 : 32;
    kosi->system_vmem = std::make_shared<VirtualMemory>(
        kosi->pmem, bits, kdetails->initial_task_asid, pae, "linux");

    // Get address of the initial task by iterating over all tasks starting from the
    // current task
    if (!get_init_task_addr(kosi, thread_info_address, &(kdetails->initial_task_addr))) {
        fprintf(stderr, "Unable to find the address of init_task\n");
        return false;
    }

    return true;
}
