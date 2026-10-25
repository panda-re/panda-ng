/*
 * Test panda module whose type name clashes with a built-in one ("pl011"): loading it is
 * an error naming both, not QOM's abort (tests/modules/run.sh).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "hw/core/qdev.h"

static const TypeInfo ext_clash_info = {
    .name = "pl011",
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(DeviceState),
};

static void ext_clash_register(void)
{
    type_register_static(&ext_clash_info);
}

type_init(ext_clash_register)
