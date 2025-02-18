#include "iohal/memory/virtual_memory.h"
#include "offset/offset.h"
#include "osi/windows/manager.h"
#include "osi/windows/wintrospection.h"
#include <cstdlib>
#include <ctype.h>
#include <stdio.h>
#include <vector>

int main(int argc, char* argv[])
{
    if (argc != 7) {
        fprintf(stderr, "usage: %s memfile dtb kpcr pid addr size\n", argv[0]);
        return 3;
    }

    const char* testfile = argv[1];
    uint64_t asid_arg = std::strtoull(argv[2], NULL, 16);
    uint64_t kpcr = std::strtoull(argv[3], NULL, 16);
    uint32_t target_pid = std::atoi(argv[4]);
    uint64_t addr = std::strtoull(argv[5], NULL, 16);
    uint64_t size = atoi(argv[6]);

    WindowsKernelManager manager = WindowsKernelManager("windows-64-7sp1");

    auto pmem = load_physical_memory_snapshot(testfile);
    if (pmem == nullptr) {
        fprintf(stderr, "Failed to read physical memory snapshot\n");
        return 1;
    }
    if (!manager.initialize(pmem, 8, asid_arg, kpcr)) {
        fprintf(stderr, "Failed to initialize windows\n");
        return 3;
    }

    auto proc_manager = WindowsProcessManager();
    if (!proc_manager.initialize(manager.get_kernel_object(), 0, target_pid)) {
        fprintf(stderr, "Could not initialize with target pid: %u\n", target_pid);
        return 7;
    }
    auto posi = proc_manager.get_process_object();

    auto bytes = std::vector<uint8_t>(size);
    auto status = process_vmem_read(posi, addr, bytes.data(), size);
    if (!TRANSLATE_SUCCEEDED(status)) {
        fprintf(stderr, "Failed to read memory with status: %d\n", status);
        return 3;
    }

    size_t bytes_written = 0;
    while (bytes_written < size) {
        fprintf(stderr, "%016lx: ", addr);
        for (size_t ix = 0; ix < 16 && bytes_written < size; ++ix) {
            fprintf(stderr, "%02x ", bytes[bytes_written]);
            bytes_written += 1;
        }
        fprintf(stderr, "| ");
        for (size_t jx = 0; jx < 16; ++jx) {
            char c = bytes[bytes_written - 16 + jx];
            if (isprint(c)) {
                fprintf(stderr, "%c", c);
            } else {
                fprintf(stderr, ".");
            }
        }
        fprintf(stderr, "\n");
    }

    return 0;
}
