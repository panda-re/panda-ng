/*
 * PandaModuleInfo: what a panda module (an external board or device shared
 * object) was built for. Every module carries one, as the symbol
 * panda_module_info (qemu/panda-module.h defines it); module_load_path()
 * reads it before running any of the module's code.
 *
 * Plain C types only: the header is -include'd before qemu/osdep.h.
 *
 * Versioned: fields are only ever appended. A new field bumps
 * PANDA_MODULE_INFO_VERSION, and a reader uses only the fields that the
 * module's version and size cover, so adding one (per-kind interface
 * versions, say) needs no flag day for modules already built.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef QEMU_PANDA_MODULE_INFO_H
#define QEMU_PANDA_MODULE_INFO_H

#define PANDA_MODULE_INFO_MAGIC   0x494d4450u     /* "PDMI" */
#define PANDA_MODULE_INFO_VERSION 1

typedef struct PandaModuleInfo {
    /* version 1 */
    unsigned int magic;         /* PANDA_MODULE_INFO_MAGIC */
    unsigned int version;       /* PANDA_MODULE_INFO_VERSION at build time */
    unsigned int size;          /* sizeof(PandaModuleInfo) at build time */
    char abi[68];               /* PANDA_ABI_HASH: 64 hex digits */
    char target[32];            /* TARGET_NAME: "aarch64", "mipsel", ... */
    char qemu_version[32];      /* QEMU_VERSION of the engine the SDK is from */
} PandaModuleInfo;

#endif
