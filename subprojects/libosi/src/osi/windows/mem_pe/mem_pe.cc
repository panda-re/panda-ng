#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
#include <string>

#include "offset/windows_common.h"
#include "osi/windows/pe.h"
#include "osi/windows/wintrospection.h"

struct mem_pe {
    struct WindowsProcessOSI* process_osi;
    uint64_t image_base;
    struct _IMAGE_DOS_HEADER dos_header;
    struct _IMAGE_NT_HEADERS nt_header;
    bool is32bit;
    union {
        _IMAGE_OPTIONAL_HEADER h32;
        _IMAGE_OPTIONAL_HEADER64 h64;
    } optional_header;
    bool parsed_exports;
    bool has_exports;
    bool parsed_debug;
    bool has_codeview_debug_table;
    struct _IMAGE_EXPORT_DIRECTORY export_table;
    struct _IMAGE_DEBUG_DIRECTORY debug_table;
};

struct mem_pe* init_mem_pe(struct WindowsProcessOSI* process_osi, uint64_t image_base,
                           bool force)
{
    auto mempe = (struct mem_pe*)std::calloc(1, sizeof(struct mem_pe));
    mempe->process_osi = process_osi;
    mempe->image_base = image_base;
    auto status = process_osi->vmem->read(image_base, &mempe->dos_header,
                                          sizeof(struct _IMAGE_DOS_HEADER));
    if (!TRANSLATE_SUCCEEDED(status)) {
        fprintf(stderr, "DOS Header is paged out %" PRIx64 "\n", image_base);
        std::free(mempe);
        return nullptr;
    }
    // Check this is a valid DOS header, ignoring mismatches if the
    // force flag is set
    if (!force && (mempe->dos_header.e_magic != 0x5a4d)) {
        fprintf(stderr, "_IMAGE_DOS_HEADER.e_magic = %x\n", mempe->dos_header.e_magic);
        std::free(mempe);
        return nullptr;
    }

    auto nt_header_address = image_base + mempe->dos_header.e_lfanew;
    status = process_osi->vmem->read(nt_header_address, &mempe->nt_header,
                                     sizeof(struct _IMAGE_NT_HEADERS));
    if (!TRANSLATE_SUCCEEDED(status)) {
        fprintf(stderr, "Failed to read nt header at %" PRIx64 "\n", nt_header_address);
        std::free(mempe);
        return nullptr;
    }

    if (!force && (mempe->nt_header.Signature != 0x4550)) {
        fprintf(stderr, "_IMAGE_NT_HEADERS.Signature = %x\n", mempe->nt_header.Signature);
        std::free(mempe);
        return nullptr;
    }

    mempe->is32bit = mem_pe_is_i386(mempe);
    auto optional_header =
        nt_header_address + sizeof(uint32_t) + sizeof(struct _IMAGE_FILE_HEADER);
    if (mempe->is32bit) {
        status = process_osi->vmem->read(optional_header, &mempe->optional_header.h32,
                                         sizeof(struct _IMAGE_OPTIONAL_HEADER));
    } else {
        status = process_osi->vmem->read(optional_header, &mempe->optional_header.h64,
                                         sizeof(struct _IMAGE_OPTIONAL_HEADER64));
    }
    if (!TRANSLATE_SUCCEEDED(status)) {
        fprintf(stderr, "Failed to read optional header at %" PRIx64 "\n", optional_header);
        std::free(mempe);
        return nullptr;
    }

    return mempe;
}

void free_mem_pe(struct mem_pe* pe)
{
    if (pe) {
        std::free(pe);
    }
}

uint16_t mem_pe_get_machine(struct mem_pe* mp)
{
    return mp->nt_header.FileHeader.Machine;
}

uint16_t mem_pe_get_numberofsections(struct mem_pe* mp)
{
    return mp->nt_header.FileHeader.NumberOfSections;
}

uint32_t mem_pe_get_timedatestamp(struct mem_pe* mp)
{
    return mp->nt_header.FileHeader.TimeDateStamp;
}

uint32_t mem_pe_get_pointertosymboltable(struct mem_pe* mp)
{
    return mp->nt_header.FileHeader.PointerToSymbolTable;
}

uint32_t mem_pe_get_numberofsymbols(struct mem_pe* mp)
{
    return mp->nt_header.FileHeader.NumberOfSymbols;
}

uint32_t mem_pe_get_sizeofoptionalheader(struct mem_pe* mp)
{
    return mp->nt_header.FileHeader.SizeOfOptionalHeader;
}

uint32_t mem_pe_get_characteristics(struct mem_pe* mp)
{
    return mp->nt_header.FileHeader.Characteristics;
}

bool mem_pe_is_i386(struct mem_pe* mp) { return mem_pe_get_machine(mp) == 0x014c; }

bool mem_pe_is_amd64(struct mem_pe* mp) { return mem_pe_get_machine(mp) == 0x8664; }

uint16_t mem_pe_get_magic(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.Magic;
    } else {
        return mp->optional_header.h64.Magic;
    }
}

uint8_t mem_pe_get_majorlinkerversion(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.MajorLinkerVersion;
    } else {
        return mp->optional_header.h64.MajorLinkerVersion;
    }
}

uint8_t mem_pe_get_minorlinkerversion(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.MinorLinkerVersion;
    } else {
        return mp->optional_header.h64.MinorLinkerVersion;
    }
}

uint32_t mem_pe_get_sizeofcode(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.SizeOfCode;
    } else {
        return mp->optional_header.h64.SizeOfCode;
    }
}

uint32_t mem_pe_get_sizeofinitializeddata(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.SizeOfInitializedData;
    } else {
        return mp->optional_header.h64.SizeOfInitializedData;
    }
}

uint32_t mem_pe_get_sizeofuninitializeddata(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.SizeOfUninitializedData;
    } else {
        return mp->optional_header.h64.SizeOfUninitializedData;
    }
}

uint32_t mem_pe_get_addressofentrypoint_rva(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.AddressOfEntryPoint;
    } else {
        return mp->optional_header.h64.AddressOfEntryPoint;
    }
}

uint64_t mem_pe_get_addressofentrypoint_va(struct mem_pe* mp)
{
    return mp->image_base + mem_pe_get_addressofentrypoint_rva(mp);
}

uint64_t mem_pe_get_baseofcode_rva(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.BaseOfCode;
    } else {
        return mp->optional_header.h64.BaseOfCode;
    }
}

uint64_t mem_pe_get_baseofcode_va(struct mem_pe* mp)
{
    return mp->image_base + mem_pe_get_baseofcode_rva(mp);
}

uint64_t mem_pe_get_imagebase(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.ImageBase;
    } else {
        return mp->optional_header.h64.ImageBase;
    }
}

uint32_t mem_pe_get_sectionalignment(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.SectionAlignment;
    } else {
        return mp->optional_header.h64.SectionAlignment;
    }
}

uint32_t mem_pe_get_filealignment(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.FileAlignment;
    } else {
        return mp->optional_header.h64.FileAlignment;
    }
}

uint16_t mem_pe_get_majoroperatingsystemversion(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.MajorOperatingSystemVersion;
    } else {
        return mp->optional_header.h64.MajorOperatingSystemVersion;
    }
}

uint16_t mem_pe_get_minoroperatingsystemversion(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.MinorOperatingSystemVersion;
    } else {
        return mp->optional_header.h64.MinorOperatingSystemVersion;
    }
}

uint16_t mem_pe_get_majorimageversion(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.MajorImageVersion;
    } else {
        return mp->optional_header.h64.MajorImageVersion;
    }
}

uint16_t mem_pe_get_minorimageversion(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.MinorImageVersion;
    } else {
        return mp->optional_header.h64.MinorImageVersion;
    }
}

uint16_t mem_pe_get_majorsubsystemversion(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.MajorSubsystemVersion;
    } else {
        return mp->optional_header.h64.MajorSubsystemVersion;
    }
}

uint16_t mem_pe_get_minorsubsystemversion(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.MinorSubsystemVersion;
    } else {
        return mp->optional_header.h64.MinorSubsystemVersion;
    }
}

uint32_t mem_pe_get_win32versionvalue(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.Win32VersionValue;
    } else {
        return mp->optional_header.h64.Win32VersionValue;
    }
}

uint32_t mem_pe_get_sizeofimage(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.SizeOfImage;
    } else {
        return mp->optional_header.h64.SizeOfImage;
    }
}

uint32_t mem_pe_get_sizeofheaders(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.SizeOfHeaders;
    } else {
        return mp->optional_header.h64.SizeOfHeaders;
    }
}

uint32_t mem_pe_get_checksum(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.CheckSum;
    } else {
        return mp->optional_header.h64.CheckSum;
    }
}

uint16_t mem_pe_get_subsystem(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.Subsystem;
    } else {
        return mp->optional_header.h64.Subsystem;
    }
}

uint16_t mem_pe_get_dllcharacteristics(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.DllCharacteristics;
    } else {
        return mp->optional_header.h64.DllCharacteristics;
    }
}

uint32_t mem_pe_get_loaderflags(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.LoaderFlags;
    } else {
        return mp->optional_header.h64.LoaderFlags;
    }
}

uint32_t mem_pe_get_numberofrvaandsizes(struct mem_pe* mp)
{
    if (mp->is32bit) {
        return mp->optional_header.h32.NumberOfRvaAndSizes;
    } else {
        return mp->optional_header.h64.NumberOfRvaAndSizes;
    }
}

bool mem_pe_load_section_header(struct mem_pe* mp, struct _IMAGE_SECTION_HEADER* sechead,
                                uint16_t idx)
{

    auto max_n = mem_pe_get_numberofsections(mp);
    if (idx >= max_n) {
        // Slows things down a bit but simplifies client interface
        std::memset(sechead, 0, sizeof(_IMAGE_SECTION_HEADER));
        return false;
    }

    auto nt_header_address = mp->image_base + mp->dos_header.e_lfanew;

    auto section_table_address = nt_header_address + sizeof(uint32_t) +
                                 sizeof(struct _IMAGE_FILE_HEADER) +
                                 mem_pe_get_sizeofoptionalheader(mp);
    auto section_addr =
        section_table_address + sizeof(struct _IMAGE_SECTION_HEADER) * idx;

    auto status = mp->process_osi->vmem->read(section_addr, sechead,
                                              sizeof(struct _IMAGE_SECTION_HEADER));
    if (!TRANSLATE_SUCCEEDED(status)) {
        fprintf(stderr, "Section header isn't readable %" PRIx64 "\n", section_table_address);
        // Slows things down a bit but simplifies client interface
        std::memset(sechead, 0, sizeof(_IMAGE_SECTION_HEADER));
        return false;
    }

    return true;
}

bool parse_exports(struct mem_pe* mp)
{
    // Do this at most once
    if (mp->parsed_exports) {
        return true;
    }

    uint64_t export_table_addr = 0;
    uint32_t export_table_size = 0;
    if (mp->is32bit) {
        export_table_addr =
            mp->optional_header.h32.DataDirectory[IMAGE_DIRECTORY_ENTRY_TYPE_EXPORT]
                .VirtualAddress;
        export_table_size =
            mp->optional_header.h32.DataDirectory[IMAGE_DIRECTORY_ENTRY_TYPE_EXPORT].Size;
    } else {
        export_table_addr =
            mp->optional_header.h64.DataDirectory[IMAGE_DIRECTORY_ENTRY_TYPE_EXPORT]
                .VirtualAddress;
        export_table_size =
            mp->optional_header.h64.DataDirectory[IMAGE_DIRECTORY_ENTRY_TYPE_EXPORT].Size;
    }
    export_table_addr += mp->image_base;

    mp->has_exports = (export_table_size != 0);

    if (export_table_size == 0) {
        mp->parsed_exports = true;
        return true;
    }

    auto status = mp->process_osi->vmem->read(export_table_addr, &(mp->export_table),
                                              sizeof(struct _IMAGE_EXPORT_DIRECTORY));
    if (!TRANSLATE_SUCCEEDED(status)) {
        fprintf(stderr, "ExportTable isn't readable %" PRIx64 "\n", export_table_addr);
        std::memset(&(mp->export_table), 0, sizeof(_IMAGE_EXPORT_DIRECTORY));
        mp->parsed_exports = false;
        return false;
    }

    mp->parsed_exports = true;
    return true;
}

std::string mem_pe_export_table_get_name(struct mem_pe* mp)
{
    if (!(mp->parsed_exports)) {
        return "<attemped_read_before_parsing>";
    }
    if (!(mp->has_exports)) {
        return "";
    }

    uint64_t name_addr = mp->image_base + mp->export_table.Name;
    const uint64_t max_name_size = 256; // Set some limit in case the memory is corrupted
    char buffer[max_name_size] = {0};

    for (uint64_t idx = 0; idx < max_name_size - 1; ++idx) {
        auto status = mp->process_osi->vmem->read(name_addr + idx, buffer + idx, 1);
        if (!TRANSLATE_SUCCEEDED(status)) {
            buffer[idx] = '\0';
            break;
        }
        if (buffer[idx] == '\0') {
            break;
        }
    }

    return std::string(buffer);
}

uint32_t mem_pe_export_table_get_base(struct mem_pe* mp)
{
    if ((!mp->parsed_exports)) {
        return 0;
    }
    return mp->export_table.Base;
}

uint32_t mem_pe_export_table_get_numberoffunctions(struct mem_pe* mp)
{
    if (!(mp->parsed_exports)) {
        fprintf(stderr, "Tried to read exports before parsing them!\n");
        return 0;
    }

    if (!(mp->has_exports)) {
        return 0;
    }

    return mp->export_table.NumberOfFunctions;
}

uint64_t mem_pe_export_table_get_rva_by_table_idx(struct mem_pe* mp, uint32_t idx)
{
    if (!mp->parsed_exports) {
        fprintf(stderr, "Tried to read exports before parsing them!\n");
        return 0;
    }
    if (!(mp->has_exports)) {
        return 0;
    }

    auto table_entry_addr = mp->image_base + mp->export_table.AddressOfFunctions;
    if (idx >= mp->export_table.NumberOfFunctions) {
        fprintf(stderr, "Attempted to read a table entry that does not exist %u!\n", idx);
        return 0;
    }
    table_entry_addr += sizeof(uint32_t) * idx; // table is filled with RVA

    uint32_t retval = 0;
    auto status =
        mp->process_osi->vmem->read(table_entry_addr, &retval, sizeof(uint32_t));
    if (!TRANSLATE_SUCCEEDED(status)) {
        fprintf(stderr, "Failed to read export rva %" PRIx64 "!\n", table_entry_addr);
        return 0;
    }
    return retval;
}

bool namehelper_by_name_idx(struct mem_pe* mp, char* buffer, size_t* blen,
                            uint32_t name_idx)
{
    uint32_t rva = 0;
    auto name_addr =
        mp->image_base + mp->export_table.AddressOfNames + name_idx * sizeof(uint32_t);
    auto status = mp->process_osi->vmem->read(name_addr, &rva, sizeof(uint32_t));
    if (!TRANSLATE_SUCCEEDED(status)) {
        fprintf(stderr, "Failed to read name rva: %" PRIx64 "!\n", name_addr);
        return false;
    }

    uint64_t read_addr = rva + mp->image_base;
    for (uint64_t idx = 0; idx < *blen - 1; ++idx) {
        auto status = mp->process_osi->vmem->read(read_addr + idx, buffer + idx, 1);
        if (!TRANSLATE_SUCCEEDED(status)) {
            buffer[idx] = '\0';
            break;
        }
        if (buffer[idx] == '\0') {
            return true;
        }
    }
    buffer[*blen - 1] = '\0';
    return false;
}

// From infosecinstitue.com
//
// NumberOfFunctions
// AddressOfFunctions ->    [rva0  | rva1 | rva2 | rva3 | rva4]
//                            ^        ---------------------^
// NumberOfNames              |        |
// AddressOfNames ->        [name0 | name1 ]
//                            ^        ^
// NumberOfNames              |        |
// AddressOfNameOrdinals -> [ 0    |   4   ]
bool mem_pe_export_table_get_name_by_table_idx(struct mem_pe* mp, char* buffer,
                                               size_t* blen, uint32_t table_idx)
{
    if (!mp->parsed_exports) {
        fprintf(stderr, "Tried to read exports before parsing them!\n");
        return false;
    }
    if (!(mp->has_exports)) {
        return false;
    }

    auto base_addr = mp->image_base + mp->export_table.AddressOfNameOrdinals;
    for (uint64_t idx = 0; idx < mp->export_table.NumberOfNames; ++idx) {
        auto addr = base_addr + idx * sizeof(uint16_t);
        uint16_t candidate = 0;
        auto status = mp->process_osi->vmem->read(addr, &candidate, 4);
        if (!TRANSLATE_SUCCEEDED(status)) {
            if (buffer) {
                buffer[0] = '\0';
            }
            *blen = 0;
            return false;
        }
        if (candidate == table_idx) {
            return namehelper_by_name_idx(mp, buffer, blen, idx);
        }
    }

    if (buffer != nullptr) {
        buffer[0] = '\0';
    }
    *blen = 0;
    return true;
}

uint32_t mem_pe_export_table_get_table_idx_by_ordinal(struct mem_pe* mp, uint32_t ordinal)
{
    if (!mp->parsed_exports) {
        fprintf(stderr, "Tried to read exports before parsing them!\n");
        return 0;
    }
    if (!(mp->has_exports)) {
        return 0;
    }

    if (ordinal < mp->export_table.Base) {
        fprintf(stderr, "WARNING: Underflow detected in export ordinal parsing\n");
        return 0;
    }

    return ordinal - mp->export_table.Base;
}

bool parse_debug(struct mem_pe* mp)
{
    // Do this at most once
    if (mp->parsed_debug) {
        return true;
    }

    uint64_t debug_table_addr = 0;
    uint32_t debug_table_size = 0;
    if (mp->is32bit) {
        debug_table_addr =
            mp->optional_header.h32.DataDirectory[IMAGE_DIRECTORY_ENTRY_TYPE_DEBUG]
                .VirtualAddress;
        debug_table_size =
            mp->optional_header.h32.DataDirectory[IMAGE_DIRECTORY_ENTRY_TYPE_DEBUG].Size;
    } else {
        debug_table_addr =
            mp->optional_header.h64.DataDirectory[IMAGE_DIRECTORY_ENTRY_TYPE_DEBUG]
                .VirtualAddress;
        debug_table_size =
            mp->optional_header.h64.DataDirectory[IMAGE_DIRECTORY_ENTRY_TYPE_DEBUG].Size;
    }
    debug_table_addr += mp->image_base;

    mp->has_codeview_debug_table = (debug_table_size != 0);

    if (debug_table_size == 0) {
        mp->parsed_debug = true;
        return true;
    }

    bool was_failure = false;
    uint64_t debug_entries = debug_table_size / sizeof(_IMAGE_DEBUG_DIRECTORY);
    for (uint32_t ix = 0; ix < debug_entries; ++ix) {

        auto status = process_vmem_read(
            mp->process_osi,
            debug_table_addr + ix * sizeof(struct _IMAGE_DEBUG_DIRECTORY),
            &(mp->debug_table), sizeof(struct _IMAGE_DEBUG_DIRECTORY));
        if (!TRANSLATE_SUCCEEDED(status)) {
            fprintf(stderr, "DebugTable isn't readable %" PRIx64 "\n",
                    debug_table_addr + ix * sizeof(struct _IMAGE_DEBUG_DIRECTORY));
            std::memset(&(mp->debug_table), 0, sizeof(_IMAGE_DEBUG_DIRECTORY));
            was_failure = true;
            continue;
        }
        auto& dbg = mp->debug_table;
        if (dbg.Type == IMAGE_DEBUG_TYPE_MISC) {
            fprintf(stderr, "Found a valid but unhandled debug info type!\n");
        } else if (dbg.Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
            mp->has_codeview_debug_table = true;
            break;
        }
    }
    mp->parsed_debug = !was_failure; // done only if all of the directories parsed okay
    if (!mp->has_codeview_debug_table) {
        return mp->parsed_debug; // okay if we parsed but don't have anything
    }

    return true;
}

static inline std::string read_null_term_string(struct WindowsProcessOSI* process_osi,
                                                uint64_t addr)
{
    std::string result = "";
    char ch = '\0';
    uint32_t max_length = 2048;

    while (max_length &&
           TRANSLATE_SUCCEEDED(process_vmem_read(process_osi, addr, &ch, 1))) {
        if (ch == '\0') {
            break;
        }
        result += ch;
        max_length -= 1;
        addr += 1;
    }
    return result;
}

struct GUIDHolder {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
};

static inline std::string mem_pe_get_cv_guid(struct mem_pe* mp)
{
    std::string result = "";
    if (!mp->has_codeview_debug_table) {
        return result;
    }
    if (mp->debug_table.AddressOfRawData == 0) {
        fprintf(stderr, "Debug information is not mapped in\n");
        return result;
    }
    result.resize(64);

    uint32_t signature = 0;
    uint64_t dbg_info = mp->image_base + mp->debug_table.AddressOfRawData;
    auto status = process_vmem_read(mp->process_osi, dbg_info, &signature, 4);
    if (!TRANSLATE_SUCCEEDED(status)) {
        fprintf(stderr, "Failed to parse CV_HEADER at %" PRIx64 "\n", dbg_info);
        return "";
    }

    if (signature == 0x53445352) { // RSDS
        struct GUIDHolder guid = {0};
        status = process_vmem_read(mp->process_osi, dbg_info + 4, &guid, 16);
        if (!TRANSLATE_SUCCEEDED(status)) {
            fprintf(stderr, "Failed to parse CV_INFO_PDB70 at %" PRIx64 "\n", dbg_info + 4);
            return "";
        }

        // DEBUG: age is not part of the GUID, but leaving for debugging
        // uint32_t age = 0;
        // status = process_vmem_read(mp->process_osi, dbg_info + 0x14, &age, 4);
        // if (!TRANSLATE_SUCCEEDED(status)) {
        //    fprintf(stderr, "Failed to parse CV_INFO_PDB70 at %" PRIx64 "\n", dbg_info + 0x14);
        //    return "";
        //}

        snprintf(&(result[0]), 64, "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
                 guid.data1, guid.data2, guid.data3, guid.data4[0], guid.data4[1],
                 guid.data4[2], guid.data4[3], guid.data4[4], guid.data4[5],
                 guid.data4[6], guid.data4[7]);
        return result;

    } else if (signature == 0x3031424e) { // NB10
        uint32_t sigts = 0;
        status = process_vmem_read(mp->process_osi, dbg_info + 8, &sigts, 4);
        if (!TRANSLATE_SUCCEEDED(status)) {
            fprintf(stderr, "Failed to parse CV_INFO_PDB20 at %" PRIx64 "\n", dbg_info + 8);
            return "";
        }

        // DEBUG: age is not part of the GUID, but leaving for debugging
        // uint32_t age = 0;
        // status = process_vmem_read(mp->process_osi, dbg_info + 0xC, &age, 4);
        // if (!TRANSLATE_SUCCEEDED(status)) {
        //    fprintf(stderr, "Failed to parse CV_INFO_PDB20 at %" PRIx64 "\n", dbg_info + 0xC);
        //    return "";
        //}

        snprintf(&(result[0]), 16, "%08X", sigts);
        return result;

    } else {
        fprintf(stderr, "Unsupported PDB info type: %x\n", signature);
    }

    return result;
}

std::string mem_pe_get_tds_guid(struct mem_pe* mp)
{
    // This relies on c++11 guarantees about .c_str
    std::string result = "";
    result.resize(16);
    snprintf(&(result[0]), 32, "%08X%08X", mem_pe_get_timedatestamp(mp),
             mem_pe_get_sizeofimage(mp));
    return result;
}

std::string mem_pe_get_guid(struct mem_pe* mp)
{
    if (!mp->parsed_debug) {
        // We don't care if this fails. We fall back to TDS anyway
        (void)parse_debug(mp);
    }

    if (mp->has_codeview_debug_table) {
        return mem_pe_get_cv_guid(mp);
    } else {
        return mem_pe_get_tds_guid(mp);
    }
}

std::string mem_pe_get_pdb_name(struct mem_pe* mp)
{
    if (!mp->parsed_debug) {
        // We don't care if this fails, looks the same either way
        (void)parse_debug(mp);
    }

    if (!mp->has_codeview_debug_table) {
        return "";
    }

    if (mp->debug_table.AddressOfRawData == 0) {
        fprintf(stderr, "Debug information is not mapped in\n");
        return "";
    }

    uint32_t signature = 0;
    uint64_t dbg_info = mp->image_base + mp->debug_table.AddressOfRawData;

    auto status = process_vmem_read(mp->process_osi, dbg_info, &signature, 4);
    if (!TRANSLATE_SUCCEEDED(status)) {
        fprintf(stderr, "Failed to parse CV_HEADER at %" PRIx64 " in PDB name\n", dbg_info);
        return "";
    }

    if (signature == 0x53445352) { // RSDS
        return read_null_term_string(mp->process_osi, dbg_info + 0x18);
    } else if (signature == 0x3031424e) { // NB10
        return read_null_term_string(mp->process_osi, dbg_info + 0x10);
    }
    fprintf(stderr, "Unsupported CV type: %" PRIx32 "\n", signature);
    return "";
}
