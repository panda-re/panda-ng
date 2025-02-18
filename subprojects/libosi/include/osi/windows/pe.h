#pragma once

#include "osi/windows/wintrospection.h"
#include <iohal/memory/virtual_memory.h>
#include <offset/windows_common.h>

// Descriptions from: https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format

// In general we are going to hide the implementation if the
// structure isn't a constant size across all the versions of
// Windows
struct mem_pe;

struct mem_pe* init_mem_pe(struct WindowsProcessOSI* process_osi, uint64_t image_base,
                           bool force);

// IMAGE_FILE_HEADER
/**
 * The number that identifies the type of target machine.
 *
 * Common values:
 *   IMAGE_FILE_MACHINE_I386  0x014c
 *   IMAGE_FILE_MACHINE_AMD64 0x8664
 */
uint16_t mem_pe_get_machine(struct mem_pe* mp);
/**
 * The number of sections.
 *
 * This indicates the size of the section table, which immediately follows the headers.
 */
uint16_t mem_pe_get_numberofsections(struct mem_pe* mp);

/**
 * The low 32 bits of the number of seconds since 00:00 January 1, 1970
 * (a C run-time time_t value), that indicates when the file was created.
 */
uint32_t mem_pe_get_timedatestamp(struct mem_pe* mp);

/**
 * The file offset of the COFF symbol table, or zero if no COFF symbol
 * table is present. This value should be zero for an image because COFF
 * debugging information is deprecated.
 */
uint32_t mem_pe_get_pointertosymboltable(struct mem_pe* mp);

/**
 * The number of entries in the symbol table. This data can be used to
 * locate the string table, which immediately follows the symbol table.
 * This value should be zero for an image because COFF debugging
 * information is deprecated.
 */
uint32_t mem_pe_get_numberofsymbols(struct mem_pe* mp);

/**
 * The size of the optional header, which is required for executable files
 * but not for object files. This value should be zero for an object file.
 */
uint32_t mem_pe_get_sizeofoptionalheader(struct mem_pe* mp);

/**
 * The flags that indicate the attributes of the file.
 */
uint32_t mem_pe_get_characteristics(struct mem_pe* mp);

/** Check if this is a 32bit PE using the Machine value */
bool mem_pe_is_i386(struct mem_pe* mp);
/** Check if this is a 64bit PE using the Machine value */
bool mem_pe_is_amd64(struct mem_pe* mp);

// IMAGE_OPTIONAL_HEADER

/**
 * The unsigned integer that identifies the state of the image file.
 * The most common number is 0x10B, which identifies it as a normal
 * executable file. 0x107 identifies it as a ROM image, and 0x20B
 * identifies it as a PE32+ executable.
 */
uint16_t mem_pe_get_magic(struct mem_pe* mp);

/** The linker major version number. */
uint8_t mem_pe_get_majorlinkerversion(struct mem_pe* mp);

/** The linker minor version number. */
uint8_t mem_pe_get_minorlinkerversion(struct mem_pe* mp);

/**
 * The size of the code (text) section, or the sum of all code sections
 * if there are multiple sections.
 */
uint32_t mem_pe_get_sizeofcode(struct mem_pe* mp);

/**
 * The size of the initialized data section, or the sum of all
 * such sections if there are multiple data sections.
 */
uint32_t mem_pe_get_sizeofinitializeddata(struct mem_pe* mp);

/**
 * The size of the uninitialized data section (BSS), or the sum
 * of all such sections if there are multiple BSS sections.
 */
uint32_t mem_pe_get_sizeofuninitializeddata(struct mem_pe* mp);

/**
 * The address of the entry point relative to the image base when the
 * executable file is loaded into memory. For program images, this is
 * the starting address. For device drivers, this is the address of the
 * initialization function. An entry point is optional for DLLs. When no
 * entry point is present, this field must be zero.
 */
uint32_t mem_pe_get_addressofentrypoint_rva(struct mem_pe* mp);

/** mem_pe_get_addressofentrypoint_rva adjusted by the image base */
uint64_t mem_pe_get_addressofentrypoint_va(struct mem_pe* mp);

/**
 * The address that is relative to the image base of the beginning-of-code
 * section when it is loaded into memory.
 */
uint64_t mem_pe_get_baseofcode_rva(struct mem_pe* mp);

/** mem_pe_get_baseofcode_rva adjusted by the image base */
uint64_t mem_pe_get_baseofcode_va(struct mem_pe* mp);

/**
 * The preferred address of the first byte of image when
 * loaded into memory; must be a multiple of 64 K. The default
 * for DLLs is 0x10000000. The default for Windows CE EXEs is
 * 0x00010000. The default for Windows NT, Windows 2000, Windows
 * XP, Windows 95, Windows 98, and Windows Me is 0x00400000.
 */
uint64_t mem_pe_get_imagebase(struct mem_pe* mp);

/**
 * The alignment (in bytes) of sections when they are loaded into
 * memory. It must be greater than or equal to FileAlignment. The
 * default is the page size for the architecture.
 */
uint32_t mem_pe_get_sectionalignment(struct mem_pe* mp);

/**
 * The alignment factor (in bytes) that is used to align the raw
 * data of sections in the image file. The value should be a power
 * of 2 between 512 and 64 K, inclusive. The default is 512. If the
 * SectionAlignment is less than the architecture's page size, then
 * FileAlignment must match SectionAlignment.
 */
uint32_t mem_pe_get_filealignment(struct mem_pe* mp);

/**
 * The major version number of the required operating system.
 */
uint16_t mem_pe_get_majoroperatingsystemversion(struct mem_pe* mp);

/**
 * The minor version number of the required operating system.
 */
uint16_t mem_pe_get_minoroperatingsystemversion(struct mem_pe* mp);

/**
 * The minor version number of the image.
 */
uint16_t mem_pe_get_majorimageversion(struct mem_pe* mp);

/**
 * The minor version number of the image.
 */
uint16_t mem_pe_get_minorimageversion(struct mem_pe* mp);

/**
 * The major version number of the subsystem.
 */
uint16_t mem_pe_get_majorsubsystemversion(struct mem_pe* mp);

/**
 * The minor version number of the subsystem.
 */
uint16_t mem_pe_get_minorsubsystemversion(struct mem_pe* mp);

/** Reserved, must be zero */
uint32_t mem_pe_get_win32versionvalue(struct mem_pe* mp);

/**
 * The size (in bytes) of the image, including all headers, as the image
 * is loaded in memory. It must be a multiple of SectionAlignment.
 */
uint32_t mem_pe_get_sizeofimage(struct mem_pe* mp);

/**
 * The combined size of an MS-DOS stub, PE header, and section headers
 * rounded up to a multiple of FileAlignment.
 */
uint32_t mem_pe_get_sizeofheaders(struct mem_pe* mp);

/**
 * The image file checksum. The algorithm for computing the checksum
 * is incorporated into IMAGHELP.DLL. The following are checked for
 * validation at load time: all drivers, any DLL loaded at boot time,
 * and any DLL that is loaded into a critical Windows process.
 */
uint32_t mem_pe_get_checksum(struct mem_pe* mp);

/**
 * The subsystem that is required to run this image
 *
 * Common values:
 *    IMAGE_SUBSYSTEM_NATIVE      1  // device drivers and native windows processes
 *    IMAGE_SUBSYSTEM_WINDOWS_GUI 2
 *    IMAGE_SUBSYSTEM_WIDNOWS_CUI 3
 */
uint16_t mem_pe_get_subsystem(struct mem_pe* mp);

/**
 * IMAGE_DLLCHARACTERISTICS_*
 */
uint16_t mem_pe_get_dllcharacteristics(struct mem_pe* mp);

/** Reserved, must be zero. */
uint32_t mem_pe_get_loaderflags(struct mem_pe* mp);

/**
 * The number of data-directory entries in the remainder of the optional header.
 * Each describes a location and size.
 */
uint32_t mem_pe_get_numberofrvaandsizes(struct mem_pe* mp);

/**
 * Read in a section header, returning true on success */
bool mem_pe_load_section_header(struct mem_pe* mp, struct _IMAGE_SECTION_HEADER* sechead,
                                uint16_t idx);

/**
 * Get the best available GUID
 */
std::string mem_pe_get_guid(struct mem_pe* mp);
std::string mem_pe_get_tds_guid(struct mem_pe* mp);
std::string mem_pe_get_pdb_name(struct mem_pe* mp);

/** Parse the export table, returning true if successful
 *
 * Will still return true if there aren't any exports
 */
bool parse_exports(struct mem_pe* mp);

/** Parse the debug table, looking for CODEVIEW entries
 *
 * Will still return true if there isn't any debug data
 */

bool parse_debug(struct mem_pe* mp);

std::string mem_pe_export_table_get_name(struct mem_pe* mp);
uint32_t mem_pe_export_table_get_base(struct mem_pe* mp);
uint32_t mem_pe_export_table_get_numberoffunctions(struct mem_pe* mp);

uint64_t mem_pe_export_table_get_rva_by_table_idx(struct mem_pe* mp, uint32_t idx);
bool mem_pe_export_table_get_name_by_table_idx(struct mem_pe* mp, char* buffer,
                                               size_t* blen, uint32_t table_idx);
uint32_t mem_pe_export_table_get_table_idx_by_ordinal(struct mem_pe* mp,
                                                      uint32_t ordinal);

void free_mem_pe(struct mem_pe* pe);
