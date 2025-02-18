#include <string.h>

#include <offset/i_t.h>
#include <offset/offset.h>

#include "osi/windows/wintrospection.h"
#include "windows_handles.h"
#include "windows_static_offsets.h"

#define TABLE_MASK ~0x7
#define HANDLE_ENTRY_SIZE_64 0x10
#define HANDLE_ENTRY_SIZE_32 0x08

// the resolve functions in this file are based off of ReactOS
// https://doxygen.reactos.org/d0/d72/ex_8h.html#a1fa10d89ce5eb73bd55ed2cd2001d38a

uint64_t resolve_base_x64(struct WindowsProcessOSI *posi, uint64_t value, uint32_t level, uint64_t base)
{
    static_offsets::amd64::EXHANDLE_PARTIAL handle_obj;
    handle_obj.handle = value;
    handle_obj.TagBits = 0;

    switch (level) {
    case 2: {
        base = base + (sizeof(uint64_t) * handle_obj.HighIndex);
        auto obj = osi::i_t(posi->vmem, posi->tlib, base, "UNKNOWN");
        base = obj.get64();
        // fall through
    }
    case 1: {
        base = base + (sizeof(uint64_t) * handle_obj.MidIndex);
        auto obj = osi::i_t(posi->vmem, posi->tlib, base, "UNKNOWN");
        base = obj.get64();
        // fall through
    }
    case 0:
        base = base + (handle_obj.LowIndex * HANDLE_ENTRY_SIZE_64);
        break;
    default:
        return 0;
    }
    return base;
}

uint32_t resolve_base_x86(struct WindowsProcessOSI *posi, uint32_t value, uint32_t level, uint32_t base)
{
    static_offsets::i386::EXHANDLE_PARTIAL handle_obj;
    handle_obj.handle = value;

    switch (level) {
    case 2: {
        base = base + (sizeof(uint32_t) * handle_obj.HighIndex);
        auto obj = osi::i_t(posi->vmem, posi->tlib, base, "UNKNOWN");
        base = obj.get32();
        // fall through
    }
    case 1: {
        base = base + (sizeof(uint32_t) * handle_obj.MidIndex);
        auto obj = osi::i_t(posi->vmem, posi->tlib, base, "UNKNOWN");
        base = obj.get32();
        // fall through
    }
    case 0:
        base = base + (handle_obj.LowIndex * HANDLE_ENTRY_SIZE_32);
        break;
    default:
        return 0;
    }
    return base;
}

osi::i_t resolve_handle_table_entry(struct WindowsProcessOSI* posi, uint64_t handle,
                                    bool x64)
{
    osi::i_t obj(posi->vmem, posi->tlib, posi->eprocess_address, "_EPROCESS");

    auto table = obj("ObjectTable");
    auto code = table["TableCode"].getu();

    uint32_t table_level = code & 0x3;
    uint64_t table_base = code & (~0x3);

    uint64_t entry = 0;
    if (x64) {
        entry = resolve_base_x64(posi, handle, table_level, table_base);
    } else {
        if (handle > 0xffffffff) {
            fprintf(stderr, "Cannot look up 64 bit value as 32 bit handle\n");
            return osi::i_t();
        }

        entry = resolve_base_x86(posi, static_cast<uint32_t>(handle), table_level,
                                 static_cast<uint32_t>(table_base));
    }

    uint64_t header = obj.set_address(entry).getu() & TABLE_MASK;
    return obj.set_address(header).set_type("_OBJECT_HEADER");
}

osi::i_t resolve_handle_table_entry_win2000(struct WindowsProcessOSI* posi,
                                            uint32_t handle)
{
    osi::i_t obj(posi->vmem, posi->tlib, posi->eprocess_address, "_EPROCESS");

    auto table = obj("ObjectTable");
    uint32_t table_base = table["Layer1"].get32();

    // TODO: verify that all entries are actually level 3? This was added
    // as panda-re/panda does this for windows 2k

    uint64_t entry = resolve_base_x86(posi, handle, 2, table_base);
    uint64_t header = obj.set_address(entry).getu();

    // The lock flag must be set to get a valid ptr
    header |= 0x80000000;
    header &= TABLE_MASK;

    return obj.set_address(header).set_type("_OBJECT_HEADER");
}
