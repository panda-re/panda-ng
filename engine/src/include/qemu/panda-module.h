/*
 * panda-module.h: included (-include) into every file of a panda module,
 * i.e. an external board or device shared object built with the SDK.
 * It stamps the module with a PandaModuleInfo: the panda ABI hash of the
 * headers it compiled against and the target it was compiled for, which
 * module_load_path() compares with the engine's.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef QEMU_PANDA_MODULE_H
#define QEMU_PANDA_MODULE_H

#include "config-host.h"
#ifdef CONFIG_TARGET
#include CONFIG_TARGET
#endif
#include "panda-abi.h"
#include "qemu/panda-module-info.h"

#ifndef BUILD_DSO
#error "a panda module is built with -DBUILD_DSO (use the SDK's cflags)"
#endif
#ifndef TARGET_NAME
#error "a panda module is built for one target (use the SDK's cflags.<target>)"
#endif

/* Weak: every translation unit of the module defines it; one copy stays. */
__attribute__((weak, visibility("default"), used))
const PandaModuleInfo panda_module_info = {
    .magic = PANDA_MODULE_INFO_MAGIC,
    .version = PANDA_MODULE_INFO_VERSION,
    .size = sizeof(PandaModuleInfo),
    .abi = PANDA_ABI_HASH,
    .target = TARGET_NAME,
    .qemu_version = QEMU_VERSION,
};

#endif
