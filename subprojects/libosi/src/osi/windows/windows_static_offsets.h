#ifndef __LIBINTRO_WINDOWS_STATIC_OFFSETS_H
#define __LIBINTRO_WINDOWS_STATIC_OFFSETS_H

#include <iohal/memory/virtual_memory.h>

namespace static_offsets
{
namespace i386
{
static const vm_addr_t KPCR_SELF_OFFSET = 0x1C;
static const vm_addr_t KPCR_CURRENT_PRCB_OFFSET = 0x20;
static const vm_addr_t KPRCB_IDLE_THREAD = 0x0C;
static const vm_addr_t KDBG_TAG_OFFSET = 0x10;

typedef union EXHANDLE_PARTIAL {
    struct {
        uint32_t TagBits : 2;
        uint32_t LowIndex : 9;
        uint32_t MidIndex : 10;
        uint32_t HighIndex : 10;
        uint32_t KernelFlag : 1;
    };
    uint32_t handle;
} EXHANDLE_PARTIAL;
} // namespace i386

namespace amd64
{
static const vm_addr_t KPCR_SELF_OFFSET = 0x18;
static const vm_addr_t KPCR_CURRENT_PRCB_OFFSET = 0x20;
static const vm_addr_t KPRCB_IDLE_THREAD = 0x18;
static const vm_addr_t KDBG_TAG_OFFSET = 0x10;

typedef union EXHANDLE_PARTIAL {
    struct {
        uint64_t TagBits : 2;
        uint64_t LowIndex : 8;
        uint64_t MidIndex : 9;
        uint64_t HighIndex : 9;
        uint64_t KernelFlag : 36;
    };
    uint64_t handle;
} EXHANDLE_PARTIAL;
} // namespace amd64

static const vm_addr_t KDBG_PSLOADEDMODULELIST = 0x48;
static const vm_addr_t KDBG_PSACTIVEPROCESSHEAD = 0x50;

} // namespace static_offsets

#endif
