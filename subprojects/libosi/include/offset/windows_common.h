#ifndef _OFFSET_WINDOWS_COMMON_H
#define _OFFSET_WINDOWS_COMMON_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#define _Static_assert static_assert

/*
  These are all structures that should remain constant across all versions of Windows
  that can be used by the generic introspection code. In general, they're structures
  from some standardized very old file formats or common enough that we shouldn't
  expect them to change.

  Presently, they're actually taken from the 32-bit x86 Windows 2000 Server SP 4 Update
  Rollup 1 pdbs

  Don't include this in the version specific files, their header should define matching
  versions for them.
*/

struct __attribute__((__packed__)) _IMAGE_DOS_HEADER { /* len = 0x40 */
    uint16_t e_magic;                                  /* offset = 0, len = 0x2 */
    uint16_t e_cblp;                                   /* offset = 0x2, len = 0x2 */
    uint16_t e_cp;                                     /* offset = 0x4, len = 0x2 */
    uint16_t e_crlc;                                   /* offset = 0x6, len = 0x2 */
    uint16_t e_cparhdr;                                /* offset = 0x8, len = 0x2 */
    uint16_t e_minalloc;                               /* offset = 0xa, len = 0x2 */
    uint16_t e_maxalloc;                               /* offset = 0xc, len = 0x2 */
    uint16_t e_ss;                                     /* offset = 0xe, len = 0x2 */
    uint16_t e_sp;                                     /* offset = 0x10, len = 0x2 */
    uint16_t e_csum;                                   /* offset = 0x12, len = 0x2 */
    uint16_t e_ip;                                     /* offset = 0x14, len = 0x2 */
    uint16_t e_cs;                                     /* offset = 0x16, len = 0x2 */
    uint16_t e_lfarlc;                                 /* offset = 0x18, len = 0x2 */
    uint16_t e_ovno;                                   /* offset = 0x1a, len = 0x2 */
    uint16_t e_res[4];   /* offset = 0x1c, len = 0x8 (4 * 0x2) */
    uint16_t e_oemid;    /* offset = 0x24, len = 0x2 */
    uint16_t e_oeminfo;  /* offset = 0x26, len = 0x2 */
    uint16_t e_res2[10]; /* offset = 0x28, len = 0x14 (10 * 0x2) */
    int32_t e_lfanew;    /* offset = 0x3c, len = 0x4 */
};                       /* End _IMAGE_DOS_HEADER */
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_magic) == 0,
               "offsetof(struct _IMAGE_DOS_HEADER, e_magic) == 0");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_cblp) == 0x2,
               "offsetof(struct _IMAGE_DOS_HEADER, e_cblp) == 0x2");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_cp) == 0x4,
               "offsetof(struct _IMAGE_DOS_HEADER, e_cp) == 0x4");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_crlc) == 0x6,
               "offsetof(struct _IMAGE_DOS_HEADER, e_crlc) == 0x6");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_cparhdr) == 0x8,
               "offsetof(struct _IMAGE_DOS_HEADER, e_cparhdr) == 0x8");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_minalloc) == 0xa,
               "offsetof(struct _IMAGE_DOS_HEADER, e_minalloc) == 0xa");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_maxalloc) == 0xc,
               "offsetof(struct _IMAGE_DOS_HEADER, e_maxalloc) == 0xc");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_ss) == 0xe,
               "offsetof(struct _IMAGE_DOS_HEADER, e_ss) == 0xe");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_sp) == 0x10,
               "offsetof(struct _IMAGE_DOS_HEADER, e_sp) == 0x10");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_csum) == 0x12,
               "offsetof(struct _IMAGE_DOS_HEADER, e_csum) == 0x12");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_ip) == 0x14,
               "offsetof(struct _IMAGE_DOS_HEADER, e_ip) == 0x14");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_cs) == 0x16,
               "offsetof(struct _IMAGE_DOS_HEADER, e_cs) == 0x16");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_lfarlc) == 0x18,
               "offsetof(struct _IMAGE_DOS_HEADER, e_lfarlc) == 0x18");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_ovno) == 0x1a,
               "offsetof(struct _IMAGE_DOS_HEADER, e_ovno) == 0x1a");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_res) == 0x1c,
               "offsetof(struct _IMAGE_DOS_HEADER, e_res) == 0x1c");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_oemid) == 0x24,
               "offsetof(struct _IMAGE_DOS_HEADER, e_oemid) == 0x24");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_oeminfo) == 0x26,
               "offsetof(struct _IMAGE_DOS_HEADER, e_oeminfo) == 0x26");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_res2) == 0x28,
               "offsetof(struct _IMAGE_DOS_HEADER, e_res2) == 0x28");
_Static_assert(offsetof(struct _IMAGE_DOS_HEADER, e_lfanew) == 0x3c,
               "offsetof(struct _IMAGE_DOS_HEADER, e_lfanew) == 0x3c");
_Static_assert(sizeof(struct _IMAGE_DOS_HEADER) == 0x40,
               "sizeof(struct _IMAGE_DOS_HEADER) == 0x40");
/* Symbol "_IMAGE_DOS_HEADER" has no addressable parent for offset check */
struct __attribute__((__packed__)) _IMAGE_FILE_HEADER { /* len = 0x14 */
    uint16_t Machine;                                   /* offset = 0, len = 0x2 */
    uint16_t NumberOfSections;                          /* offset = 0x2, len = 0x2 */
    uint32_t TimeDateStamp;                             /* offset = 0x4, len = 0x4 */
    uint32_t PointerToSymbolTable;                      /* offset = 0x8, len = 0x4 */
    uint32_t NumberOfSymbols;                           /* offset = 0xc, len = 0x4 */
    uint16_t SizeOfOptionalHeader;                      /* offset = 0x10, len = 0x2 */
    uint16_t Characteristics;                           /* offset = 0x12, len = 0x2 */
};                                                      /* End _IMAGE_FILE_HEADER */
_Static_assert(offsetof(struct _IMAGE_FILE_HEADER, Machine) == 0,
               "offsetof(struct _IMAGE_FILE_HEADER, Machine) == 0");
_Static_assert(offsetof(struct _IMAGE_FILE_HEADER, NumberOfSections) == 0x2,
               "offsetof(struct _IMAGE_FILE_HEADER, NumberOfSections) == 0x2");
_Static_assert(offsetof(struct _IMAGE_FILE_HEADER, TimeDateStamp) == 0x4,
               "offsetof(struct _IMAGE_FILE_HEADER, TimeDateStamp) == 0x4");
_Static_assert(offsetof(struct _IMAGE_FILE_HEADER, PointerToSymbolTable) == 0x8,
               "offsetof(struct _IMAGE_FILE_HEADER, PointerToSymbolTable) == 0x8");
_Static_assert(offsetof(struct _IMAGE_FILE_HEADER, NumberOfSymbols) == 0xc,
               "offsetof(struct _IMAGE_FILE_HEADER, NumberOfSymbols) == 0xc");
_Static_assert(offsetof(struct _IMAGE_FILE_HEADER, SizeOfOptionalHeader) == 0x10,
               "offsetof(struct _IMAGE_FILE_HEADER, SizeOfOptionalHeader) == 0x10");
_Static_assert(offsetof(struct _IMAGE_FILE_HEADER, Characteristics) == 0x12,
               "offsetof(struct _IMAGE_FILE_HEADER, Characteristics) == 0x12");
_Static_assert(sizeof(struct _IMAGE_FILE_HEADER) == 0x14,
               "sizeof(struct _IMAGE_FILE_HEADER) == 0x14");
/* Symbol "_IMAGE_FILE_HEADER" has no addressable parent for offset check */
struct __attribute__((__packed__)) _IMAGE_DATA_DIRECTORY { /* len = 0x8 */
    uint32_t VirtualAddress;                               /* offset = 0, len = 0x4 */
    uint32_t Size;                                         /* offset = 0x4, len = 0x4 */
};                                                         /* End _IMAGE_DATA_DIRECTORY */
_Static_assert(offsetof(struct _IMAGE_DATA_DIRECTORY, VirtualAddress) == 0,
               "offsetof(struct _IMAGE_DATA_DIRECTORY, VirtualAddress) == 0");
_Static_assert(offsetof(struct _IMAGE_DATA_DIRECTORY, Size) == 0x4,
               "offsetof(struct _IMAGE_DATA_DIRECTORY, Size) == 0x4");
_Static_assert(sizeof(struct _IMAGE_DATA_DIRECTORY) == 0x8,
               "sizeof(struct _IMAGE_DATA_DIRECTORY) == 0x8");
/* Symbol "_IMAGE_DATA_DIRECTORY" has no addressable parent for offset check */
struct __attribute__((__packed__)) _IMAGE_OPTIONAL_HEADER { /* len = 0xe0 */
    uint16_t Magic;                                         /* offset = 0, len = 0x2 */
    uint8_t MajorLinkerVersion;                             /* offset = 0x2, len = 0x1 */
    uint8_t MinorLinkerVersion;                             /* offset = 0x3, len = 0x1 */
    uint32_t SizeOfCode;                                    /* offset = 0x4, len = 0x4 */
    uint32_t SizeOfInitializedData;                         /* offset = 0x8, len = 0x4 */
    uint32_t SizeOfUninitializedData;                       /* offset = 0xc, len = 0x4 */
    uint32_t AddressOfEntryPoint;                           /* offset = 0x10, len = 0x4 */
    uint32_t BaseOfCode;                                    /* offset = 0x14, len = 0x4 */
    uint32_t BaseOfData;                                    /* offset = 0x18, len = 0x4 */
    uint32_t ImageBase;                                     /* offset = 0x1c, len = 0x4 */
    uint32_t SectionAlignment;                              /* offset = 0x20, len = 0x4 */
    uint32_t FileAlignment;                                 /* offset = 0x24, len = 0x4 */
    uint16_t MajorOperatingSystemVersion;                   /* offset = 0x28, len = 0x2 */
    uint16_t MinorOperatingSystemVersion;                   /* offset = 0x2a, len = 0x2 */
    uint16_t MajorImageVersion;                             /* offset = 0x2c, len = 0x2 */
    uint16_t MinorImageVersion;                             /* offset = 0x2e, len = 0x2 */
    uint16_t MajorSubsystemVersion;                         /* offset = 0x30, len = 0x2 */
    uint16_t MinorSubsystemVersion;                         /* offset = 0x32, len = 0x2 */
    uint32_t Win32VersionValue;                             /* offset = 0x34, len = 0x4 */
    uint32_t SizeOfImage;                                   /* offset = 0x38, len = 0x4 */
    uint32_t SizeOfHeaders;                                 /* offset = 0x3c, len = 0x4 */
    uint32_t CheckSum;                                      /* offset = 0x40, len = 0x4 */
    uint16_t Subsystem;                                     /* offset = 0x44, len = 0x2 */
    uint16_t DllCharacteristics;                            /* offset = 0x46, len = 0x2 */
    uint32_t SizeOfStackReserve;                            /* offset = 0x48, len = 0x4 */
    uint32_t SizeOfStackCommit;                             /* offset = 0x4c, len = 0x4 */
    uint32_t SizeOfHeapReserve;                             /* offset = 0x50, len = 0x4 */
    uint32_t SizeOfHeapCommit;                              /* offset = 0x54, len = 0x4 */
    uint32_t LoaderFlags;                                   /* offset = 0x58, len = 0x4 */
    uint32_t NumberOfRvaAndSizes;                           /* offset = 0x5c, len = 0x4 */
    struct _IMAGE_DATA_DIRECTORY
        DataDirectory[16]; /* offset = 0x60, len = 0x80 (16 * 0x8) */
};                         /* End _IMAGE_OPTIONAL_HEADER */
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, Magic) == 0,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, Magic) == 0");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, MajorLinkerVersion) == 0x2,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, MajorLinkerVersion) == 0x2");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, MinorLinkerVersion) == 0x3,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, MinorLinkerVersion) == 0x3");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfCode) == 0x4,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfCode) == 0x4");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfInitializedData) == 0x8,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfInitializedData) == 0x8");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfUninitializedData) == 0xc,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfUninitializedData) == 0xc");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, AddressOfEntryPoint) == 0x10,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, AddressOfEntryPoint) == 0x10");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, BaseOfCode) == 0x14,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, BaseOfCode) == 0x14");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, BaseOfData) == 0x18,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, BaseOfData) == 0x18");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, ImageBase) == 0x1c,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, ImageBase) == 0x1c");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, SectionAlignment) == 0x20,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, SectionAlignment) == 0x20");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, FileAlignment) == 0x24,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, FileAlignment) == 0x24");
_Static_assert(
    offsetof(struct _IMAGE_OPTIONAL_HEADER, MajorOperatingSystemVersion) == 0x28,
    "offsetof(struct _IMAGE_OPTIONAL_HEADER, MajorOperatingSystemVersion) == 0x28");
_Static_assert(
    offsetof(struct _IMAGE_OPTIONAL_HEADER, MinorOperatingSystemVersion) == 0x2a,
    "offsetof(struct _IMAGE_OPTIONAL_HEADER, MinorOperatingSystemVersion) == 0x2a");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, MajorImageVersion) == 0x2c,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, MajorImageVersion) == 0x2c");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, MinorImageVersion) == 0x2e,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, MinorImageVersion) == 0x2e");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, MajorSubsystemVersion) == 0x30,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, MajorSubsystemVersion) == 0x30");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, MinorSubsystemVersion) == 0x32,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, MinorSubsystemVersion) == 0x32");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, Win32VersionValue) == 0x34,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, Win32VersionValue) == 0x34");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfImage) == 0x38,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfImage) == 0x38");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfHeaders) == 0x3c,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfHeaders) == 0x3c");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, CheckSum) == 0x40,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, CheckSum) == 0x40");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, Subsystem) == 0x44,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, Subsystem) == 0x44");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, DllCharacteristics) == 0x46,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, DllCharacteristics) == 0x46");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfStackReserve) == 0x48,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfStackReserve) == 0x48");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfStackCommit) == 0x4c,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfStackCommit) == 0x4c");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfHeapReserve) == 0x50,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfHeapReserve) == 0x50");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfHeapCommit) == 0x54,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, SizeOfHeapCommit) == 0x54");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, LoaderFlags) == 0x58,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, LoaderFlags) == 0x58");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, NumberOfRvaAndSizes) == 0x5c,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, NumberOfRvaAndSizes) == 0x5c");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER, DataDirectory) == 0x60,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER, DataDirectory) == 0x60");
_Static_assert(sizeof(struct _IMAGE_OPTIONAL_HEADER) == 0xe0,
               "sizeof(struct _IMAGE_OPTIONAL_HEADER) == 0xe0");
/* Symbol "_IMAGE_OPTIONAL_HEADER" has no addressable parent for offset check */
struct __attribute__((__packed__)) _IMAGE_OPTIONAL_HEADER64 { /* len = 0xf0 */
    uint16_t Magic;                                           /* offset = 0, len = 0x2 */
    uint8_t MajorLinkerVersion;           /* offset = 0x2, len = 0x1 */
    uint8_t MinorLinkerVersion;           /* offset = 0x3, len = 0x1 */
    uint32_t SizeOfCode;                  /* offset = 0x4, len = 0x4 */
    uint32_t SizeOfInitializedData;       /* offset = 0x8, len = 0x4 */
    uint32_t SizeOfUninitializedData;     /* offset = 0xc, len = 0x4 */
    uint32_t AddressOfEntryPoint;         /* offset = 0x10, len = 0x4 */
    uint32_t BaseOfCode;                  /* offset = 0x14, len = 0x4 */
    uint64_t ImageBase;                   /* offset = 0x18, len = 0x8 */
    uint32_t SectionAlignment;            /* offset = 0x20, len = 0x4 */
    uint32_t FileAlignment;               /* offset = 0x24, len = 0x4 */
    uint16_t MajorOperatingSystemVersion; /* offset = 0x28, len = 0x2 */
    uint16_t MinorOperatingSystemVersion; /* offset = 0x2a, len = 0x2 */
    uint16_t MajorImageVersion;           /* offset = 0x2c, len = 0x2 */
    uint16_t MinorImageVersion;           /* offset = 0x2e, len = 0x2 */
    uint16_t MajorSubsystemVersion;       /* offset = 0x30, len = 0x2 */
    uint16_t MinorSubsystemVersion;       /* offset = 0x32, len = 0x2 */
    uint32_t Win32VersionValue;           /* offset = 0x34, len = 0x4 */
    uint32_t SizeOfImage;                 /* offset = 0x38, len = 0x4 */
    uint32_t SizeOfHeaders;               /* offset = 0x3c, len = 0x4 */
    uint32_t CheckSum;                    /* offset = 0x40, len = 0x4 */
    uint16_t Subsystem;                   /* offset = 0x44, len = 0x2 */
    uint16_t DllCharacteristics;          /* offset = 0x46, len = 0x2 */
    uint64_t SizeOfStackReserve;          /* offset = 0x48, len = 0x8 */
    uint64_t SizeOfStackCommit;           /* offset = 0x50, len = 0x8 */
    uint64_t SizeOfHeapReserve;           /* offset = 0x58, len = 0x8 */
    uint64_t SizeOfHeapCommit;            /* offset = 0x60, len = 0x8 */
    uint32_t LoaderFlags;                 /* offset = 0x68, len = 0x4 */
    uint32_t NumberOfRvaAndSizes;         /* offset = 0x6c, len = 0x4 */
    struct _IMAGE_DATA_DIRECTORY
        DataDirectory[16]; /* offset = 0x70, len = 0x80 (16 * 0x8) */
};                         /* End _IMAGE_OPTIONAL_HEADER64 */
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, Magic) == 0,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, Magic) == 0");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, MajorLinkerVersion) == 0x2,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, MajorLinkerVersion) == 0x2");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, MinorLinkerVersion) == 0x3,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, MinorLinkerVersion) == 0x3");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfCode) == 0x4,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfCode) == 0x4");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfInitializedData) == 0x8,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfInitializedData) == 0x8");
_Static_assert(
    offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfUninitializedData) == 0xc,
    "offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfUninitializedData) == 0xc");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, AddressOfEntryPoint) == 0x10,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, AddressOfEntryPoint) == 0x10");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, BaseOfCode) == 0x14,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, BaseOfCode) == 0x14");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, ImageBase) == 0x18,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, ImageBase) == 0x18");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, SectionAlignment) == 0x20,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, SectionAlignment) == 0x20");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, FileAlignment) == 0x24,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, FileAlignment) == 0x24");
_Static_assert(
    offsetof(struct _IMAGE_OPTIONAL_HEADER64, MajorOperatingSystemVersion) == 0x28,
    "offsetof(struct _IMAGE_OPTIONAL_HEADER64, MajorOperatingSystemVersion) == 0x28");
_Static_assert(
    offsetof(struct _IMAGE_OPTIONAL_HEADER64, MinorOperatingSystemVersion) == 0x2a,
    "offsetof(struct _IMAGE_OPTIONAL_HEADER64, MinorOperatingSystemVersion) == 0x2a");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, MajorImageVersion) == 0x2c,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, MajorImageVersion) == 0x2c");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, MinorImageVersion) == 0x2e,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, MinorImageVersion) == 0x2e");
_Static_assert(
    offsetof(struct _IMAGE_OPTIONAL_HEADER64, MajorSubsystemVersion) == 0x30,
    "offsetof(struct _IMAGE_OPTIONAL_HEADER64, MajorSubsystemVersion) == 0x30");
_Static_assert(
    offsetof(struct _IMAGE_OPTIONAL_HEADER64, MinorSubsystemVersion) == 0x32,
    "offsetof(struct _IMAGE_OPTIONAL_HEADER64, MinorSubsystemVersion) == 0x32");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, Win32VersionValue) == 0x34,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, Win32VersionValue) == 0x34");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfImage) == 0x38,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfImage) == 0x38");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfHeaders) == 0x3c,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfHeaders) == 0x3c");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, CheckSum) == 0x40,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, CheckSum) == 0x40");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, Subsystem) == 0x44,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, Subsystem) == 0x44");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, DllCharacteristics) == 0x46,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, DllCharacteristics) == 0x46");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfStackReserve) == 0x48,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfStackReserve) == 0x48");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfStackCommit) == 0x50,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfStackCommit) == 0x50");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfHeapReserve) == 0x58,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfHeapReserve) == 0x58");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfHeapCommit) == 0x60,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, SizeOfHeapCommit) == 0x60");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, LoaderFlags) == 0x68,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, LoaderFlags) == 0x68");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, NumberOfRvaAndSizes) == 0x6c,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, NumberOfRvaAndSizes) == 0x6c");
_Static_assert(offsetof(struct _IMAGE_OPTIONAL_HEADER64, DataDirectory) == 0x70,
               "offsetof(struct _IMAGE_OPTIONAL_HEADER64, DataDirectory) == 0x70");
_Static_assert(sizeof(struct _IMAGE_OPTIONAL_HEADER64) == 0xf0,
               "sizeof(struct _IMAGE_OPTIONAL_HEADER64) == 0xf0");
/* Symbol "_IMAGE_OPTIONAL_HEADER64" has no addressable parent for offset check */
struct __attribute__((__packed__)) _IMAGE_NT_HEADERS { /* len = 0xf8 */
    uint32_t Signature;                                /* offset = 0, len = 0x4 */
    struct _IMAGE_FILE_HEADER FileHeader;              /* offset = 0x4, len = 0x14 */
    struct _IMAGE_OPTIONAL_HEADER OptionalHeader;      /* offset = 0x18, len = 0xe0 */
};                                                     /* End _IMAGE_NT_HEADERS */
_Static_assert(offsetof(struct _IMAGE_NT_HEADERS, Signature) == 0,
               "offsetof(struct _IMAGE_NT_HEADERS, Signature) == 0");
_Static_assert(offsetof(struct _IMAGE_NT_HEADERS, FileHeader) == 0x4,
               "offsetof(struct _IMAGE_NT_HEADERS, FileHeader) == 0x4");
_Static_assert(offsetof(struct _IMAGE_NT_HEADERS, OptionalHeader) == 0x18,
               "offsetof(struct _IMAGE_NT_HEADERS, OptionalHeader) == 0x18");
_Static_assert(sizeof(struct _IMAGE_NT_HEADERS) == 0xf8,
               "sizeof(struct _IMAGE_NT_HEADERS) == 0xf8");
/* Symbol "_IMAGE_NT_HEADERS" has no addressable parent for offset check */
struct __attribute__((__packed__)) _IMAGE_NT_HEADERS64 { /* len = 0xf8 */
    uint32_t Signature;                                  /* offset = 0, len = 0x4 */
    struct _IMAGE_FILE_HEADER FileHeader;                /* offset = 0x4, len = 0x14 */
    struct _IMAGE_OPTIONAL_HEADER64 OptionalHeader;      /* offset = 0x18, len = 0xe0 */
};                                                       /* End _IMAGE_NT_HEADERS */
_Static_assert(offsetof(struct _IMAGE_NT_HEADERS64, Signature) == 0,
               "offsetof(struct _IMAGE_NT_HEADERS64, Signature) == 0");
_Static_assert(offsetof(struct _IMAGE_NT_HEADERS64, FileHeader) == 0x4,
               "offsetof(struct _IMAGE_NT_HEADERS64, FileHeader) == 0x4");
_Static_assert(offsetof(struct _IMAGE_NT_HEADERS64, OptionalHeader) == 0x18,
               "offsetof(struct _IMAGE_NT_HEADERS64, OptionalHeader) == 0x18");
_Static_assert(sizeof(struct _IMAGE_NT_HEADERS64) == 0x108,
               "sizeof(struct _IMAGE_NT_HEADERS64) == 0x108");
/* Symbol "_IMAGE_NT_HEADERS64" has no addressable parent for offset check */
struct __attribute__((__packed__)) _IMAGE_SECTION_HEADER { /* len = 0x28 */
    uint8_t Name[8]; /* offset = 0, len = 0x8 (8 * 0x1) */
    char Misc[4]; /* offset = 0x8, len = 0x4 (4 * 0x1) */ /* Fixed __unnamed type */
    uint32_t VirtualAddress;                              /* offset = 0xc, len = 0x4 */
    uint32_t SizeOfRawData;                               /* offset = 0x10, len = 0x4 */
    uint32_t PointerToRawData;                            /* offset = 0x14, len = 0x4 */
    uint32_t PointerToRelocations;                        /* offset = 0x18, len = 0x4 */
    uint32_t PointerToLinenumbers;                        /* offset = 0x1c, len = 0x4 */
    uint16_t NumberOfRelocations;                         /* offset = 0x20, len = 0x2 */
    uint16_t NumberOfLinenumbers;                         /* offset = 0x22, len = 0x2 */
    uint32_t Characteristics;                             /* offset = 0x24, len = 0x4 */
};                                                        /* End _IMAGE_SECTION_HEADER */
_Static_assert(offsetof(struct _IMAGE_SECTION_HEADER, Name) == 0,
               "offsetof(struct _IMAGE_SECTION_HEADER, Name) == 0");
_Static_assert(offsetof(struct _IMAGE_SECTION_HEADER, Misc) == 0x8,
               "offsetof(struct _IMAGE_SECTION_HEADER, Misc) == 0x8");
_Static_assert(offsetof(struct _IMAGE_SECTION_HEADER, VirtualAddress) == 0xc,
               "offsetof(struct _IMAGE_SECTION_HEADER, VirtualAddress) == 0xc");
_Static_assert(offsetof(struct _IMAGE_SECTION_HEADER, SizeOfRawData) == 0x10,
               "offsetof(struct _IMAGE_SECTION_HEADER, SizeOfRawData) == 0x10");
_Static_assert(offsetof(struct _IMAGE_SECTION_HEADER, PointerToRawData) == 0x14,
               "offsetof(struct _IMAGE_SECTION_HEADER, PointerToRawData) == 0x14");
_Static_assert(offsetof(struct _IMAGE_SECTION_HEADER, PointerToRelocations) == 0x18,
               "offsetof(struct _IMAGE_SECTION_HEADER, PointerToRelocations) == 0x18");
_Static_assert(offsetof(struct _IMAGE_SECTION_HEADER, PointerToLinenumbers) == 0x1c,
               "offsetof(struct _IMAGE_SECTION_HEADER, PointerToLinenumbers) == 0x1c");
_Static_assert(offsetof(struct _IMAGE_SECTION_HEADER, NumberOfRelocations) == 0x20,
               "offsetof(struct _IMAGE_SECTION_HEADER, NumberOfRelocations) == 0x20");
_Static_assert(offsetof(struct _IMAGE_SECTION_HEADER, NumberOfLinenumbers) == 0x22,
               "offsetof(struct _IMAGE_SECTION_HEADER, NumberOfLinenumbers) == 0x22");
_Static_assert(offsetof(struct _IMAGE_SECTION_HEADER, Characteristics) == 0x24,
               "offsetof(struct _IMAGE_SECTION_HEADER, Characteristics) == 0x24");
_Static_assert(sizeof(struct _IMAGE_SECTION_HEADER) == 0x28,
               "sizeof(struct _IMAGE_SECTION_HEADER) == 0x28");
/* Symbol "_IMAGE_SECTION_HEADER" has no addressable parent for offset check */
struct __attribute__((__packed__)) _IMAGE_DEBUG_DIRECTORY { /* len = 0x1c */
    uint32_t Characteristics;                               /* offset = 0, len = 0x4 */
    uint32_t TimeDateStamp;                                 /* offset = 0x4, len = 0x4 */
    uint16_t MajorVersion;                                  /* offset = 0x8, len = 0x2 */
    uint16_t MinorVersion;                                  /* offset = 0xa, len = 0x2 */
    uint32_t Type;                                          /* offset = 0xc, len = 0x4 */
    uint32_t SizeOfData;                                    /* offset = 0x10, len = 0x4 */
    uint32_t AddressOfRawData;                              /* offset = 0x14, len = 0x4 */
    uint32_t PointerToRawData;                              /* offset = 0x18, len = 0x4 */
}; /* End _IMAGE_DEBUG_DIRECTORY */
_Static_assert(offsetof(struct _IMAGE_DEBUG_DIRECTORY, Characteristics) == 0,
               "offsetof(struct _IMAGE_DEBUG_DIRECTORY, Characteristics) == 0");
_Static_assert(offsetof(struct _IMAGE_DEBUG_DIRECTORY, TimeDateStamp) == 0x4,
               "offsetof(struct _IMAGE_DEBUG_DIRECTORY, TimeDateStamp) == 0x4");
_Static_assert(offsetof(struct _IMAGE_DEBUG_DIRECTORY, MajorVersion) == 0x8,
               "offsetof(struct _IMAGE_DEBUG_DIRECTORY, MajorVersion) == 0x8");
_Static_assert(offsetof(struct _IMAGE_DEBUG_DIRECTORY, MinorVersion) == 0xa,
               "offsetof(struct _IMAGE_DEBUG_DIRECTORY, MinorVersion) == 0xa");
_Static_assert(offsetof(struct _IMAGE_DEBUG_DIRECTORY, Type) == 0xc,
               "offsetof(struct _IMAGE_DEBUG_DIRECTORY, Type) == 0xc");
_Static_assert(offsetof(struct _IMAGE_DEBUG_DIRECTORY, SizeOfData) == 0x10,
               "offsetof(struct _IMAGE_DEBUG_DIRECTORY, SizeOfData) == 0x10");
_Static_assert(offsetof(struct _IMAGE_DEBUG_DIRECTORY, AddressOfRawData) == 0x14,
               "offsetof(struct _IMAGE_DEBUG_DIRECTORY, AddressOfRawData) == 0x14");
_Static_assert(offsetof(struct _IMAGE_DEBUG_DIRECTORY, PointerToRawData) == 0x18,
               "offsetof(struct _IMAGE_DEBUG_DIRECTORY, PointerToRawData) == 0x18");
_Static_assert(sizeof(struct _IMAGE_DEBUG_DIRECTORY) == 0x1c,
               "sizeof(struct _IMAGE_DEBUG_DIRECTORY) == 0x1c");

#define IMAGE_DIRECTORY_ENTRY_DEBUG 6
#define IMAGE_DEBUG_TYPE_CODEVIEW 2
#define IMAGE_DEBUG_TYPE_MISC 4

struct __attribute__((__packed__)) _IMAGE_EXPORT_DIRECTORY { /* len = 0x0x8 */
    uint32_t Characteristics;                                /* offset = 0, len = 0x4 */
    uint32_t TimeDateStamp;                                  /* offset = 0x4, len = 0x4 */
    uint16_t MajorVersion;                                   /* offset = 0x8, len = 0x2 */
    uint16_t MinorVersion;                                   /* offset = 0xa, len = 0x2 */
    uint32_t Name;                                           /* offset = 0xc, len = 0x4 */
    uint32_t Base;              /* offset = 0x10, len = 0x4 */
    uint32_t NumberOfFunctions; /* offset = 0x14, len = 0x4 */
    uint32_t NumberOfNames;     /* offset = 0x18, len = 0x4 */
    uint32_t AddressOfFunctions;
    uint32_t AddressOfNames;
    uint32_t AddressOfNameOrdinals;

}; /* End _IMAGE_EXPORT_DIRECTORY */
_Static_assert(offsetof(struct _IMAGE_EXPORT_DIRECTORY, Characteristics) == 0,
               "offsetof(struct _IMAGE_EXPORT_DIRECTORY, Characteristics) == 0");
_Static_assert(offsetof(struct _IMAGE_EXPORT_DIRECTORY, TimeDateStamp) == 0x4,
               "offsetof(struct _IMAGE_EXPORT_DIRECTORY, TimeDateStamp) == 0x4");
_Static_assert(offsetof(struct _IMAGE_EXPORT_DIRECTORY, MajorVersion) == 0x8,
               "offsetof(struct _IMAGE_EXPORT_DIRECTORY, MajorVersion) == 0x8");
_Static_assert(offsetof(struct _IMAGE_EXPORT_DIRECTORY, MinorVersion) == 0xa,
               "offsetof(struct _IMAGE_EXPORT_DIRECTORY, MinorVersion) == 0xa");
_Static_assert(offsetof(struct _IMAGE_EXPORT_DIRECTORY, Name) == 0xc,
               "offsetof(struct _IMAGE_EXPORT_DIRECTORY, Name) == 0xc");
_Static_assert(offsetof(struct _IMAGE_EXPORT_DIRECTORY, Base) == 0x10,
               "offsetof(struct _IMAGE_EXPORT_DIRECTORY, Base) == 0x10");
_Static_assert(offsetof(struct _IMAGE_EXPORT_DIRECTORY, NumberOfFunctions) == 0x14,
               "offsetof(struct _IMAGE_EXPORT_DIRECTORY, NumberOfFunctions) == 0x14");
_Static_assert(offsetof(struct _IMAGE_EXPORT_DIRECTORY, NumberOfNames) == 0x18,
               "offsetof(struct _IMAGE_EXPORT_DIRECTORY, NumberOfFunctions) == 0x18");
_Static_assert(offsetof(struct _IMAGE_EXPORT_DIRECTORY, AddressOfFunctions) == 0x1c,
               "offsetof(struct _IMAGE_EXPORT_DIRECTORY, AddressOfFunctions) == 0x1c");
_Static_assert(offsetof(struct _IMAGE_EXPORT_DIRECTORY, AddressOfNames) == 0x20,
               "offsetof(struct _IMAGE_DEBUG_DIRECTORY, AddressOfNames) == 0x20");
_Static_assert(offsetof(struct _IMAGE_EXPORT_DIRECTORY, AddressOfNameOrdinals) == 0x24,
               "offsetof(struct _IMAGE_EXPORT_DIRECTORY, AddressOfNames) == 0x24");
_Static_assert(sizeof(struct _IMAGE_EXPORT_DIRECTORY) == 0x28,
               "sizeof(struct _IMAGE_EXPORT_DIRECTORY) == 0x28");

#define IMAGE_DIRECTORY_ENTRY_TYPE_EXPORT 0
#define IMAGE_DIRECTORY_ENTRY_TYPE_IMPORT 1
#define IMAGE_DIRECTORY_ENTRY_TYPE_RESOURCE 2
#define IMAGE_DIRECTORY_ENTRY_TYPE_EXCEPTION 3
#define IMAGE_DIRECTORY_ENTRY_TYPE_SECURITY 4
#define IMAGE_DIRECTORY_ENTRY_TYPE_BASERELOC 5
#define IMAGE_DIRECTORY_ENTRY_TYPE_DEBUG 6
#define IMAGE_DIRECTORY_ENTRY_TYPE_ARCHITECTURE 7
#define IMAGE_DIRECTORY_ENTRY_TYPE_GLOBALPTR 8
#define IMAGE_DIRECTORY_ENTRY_TYPE_TLS 9
#define IMAGE_DIRECTORY_ENTRY_TYPE_LOAD_CONFIG 10
#define IMAGE_DIRECTORY_ENTRY_TYPE_BOUND_IMPORT 11
#define IMAGE_DIRECTORY_ENTRY_TYPE_IAT 12
#define IMAGE_DIRECTORY_ENTRY_TYPE_DELAY_IMPORT 13
#define IMAGE_DIRECTORY_ENTRY_TYPE_COMDESC 14

#endif
