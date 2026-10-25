/*
 * Test panda module: two devices, built with the board SDK (panda-board-cc).
 *  - ext-counter: bus-less and user-creatable, so -device ext-counter,addr=A maps it.
 *    Reads at 0: ID 0x434e5452 ("CNTR"); writes at 4 count; reads at 8: the count;
 *    reads at 0xc: qemu_get_thread_id() (below). Its count is in vmstate.
 *  - ext-nostate: a sysbus device with no vmstate, for the guard that warns about it.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/core/qdev.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/sysbus.h"
#include "migration/vmstate.h"
#include "system/address-spaces.h"

/*
 * Same name as an engine function (util/oslib-posix.c), which the executable exports
 * (--export-dynamic). Built with the SDK (-fvisibility=hidden, -Bsymbolic) the module's own
 * calls bind to this copy, and the guest reads 0x4d4f4453 ("MODS") at 0xc; without them,
 * ELF symbol lookup finds the executable's first and the guest reads a host thread id.
 */
int qemu_get_thread_id(void)
{
    return 0x4d4f4453;
}

#define TYPE_EXT_COUNTER "ext-counter"
OBJECT_DECLARE_SIMPLE_TYPE(ExtCounterState, EXT_COUNTER)

struct ExtCounterState {
    DeviceState parent_obj;
    MemoryRegion mmio;
    uint64_t addr;
    uint32_t count;
};

static uint64_t ext_counter_read(void *opaque, hwaddr off, unsigned size)
{
    ExtCounterState *s = opaque;

    switch (off) {
    case 0x0: return 0x434e5452;
    case 0x8: return s->count;
    case 0xc: return qemu_get_thread_id();
    default:  return 0;
    }
}

static void ext_counter_write(void *opaque, hwaddr off, uint64_t val,
                              unsigned size)
{
    ExtCounterState *s = opaque;

    if (off == 0x4) {
        s->count++;
    }
}

static const MemoryRegionOps ext_counter_ops = {
    .read = ext_counter_read,
    .write = ext_counter_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

static void ext_counter_realize(DeviceState *dev, Error **errp)
{
    ExtCounterState *s = EXT_COUNTER(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &ext_counter_ops, s,
                          "ext-counter", 0x100);
    memory_region_add_subregion(get_system_memory(), s->addr, &s->mmio);
}

static const Property ext_counter_props[] = {
    DEFINE_PROP_UINT64("addr", ExtCounterState, addr, 0),
};

static const VMStateDescription vmstate_ext_counter = {
    .name = "ext-counter",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(count, ExtCounterState),
        VMSTATE_END_OF_LIST()
    },
};

static void ext_counter_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = ext_counter_realize;
    dc->vmsd = &vmstate_ext_counter;
    dc->user_creatable = true;
    device_class_set_props(dc, ext_counter_props);
}

static const TypeInfo ext_counter_info = {
    .name = TYPE_EXT_COUNTER,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(ExtCounterState),
    .class_init = ext_counter_class_init,
};

#define TYPE_EXT_NOSTATE "ext-nostate"
OBJECT_DECLARE_SIMPLE_TYPE(ExtNostateState, EXT_NOSTATE)

struct ExtNostateState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    uint32_t reg;
};

static uint64_t ext_nostate_read(void *opaque, hwaddr off, unsigned size)
{
    return ((ExtNostateState *)opaque)->reg;
}

static void ext_nostate_write(void *opaque, hwaddr off, uint64_t val,
                              unsigned size)
{
    ((ExtNostateState *)opaque)->reg = val;
}

static const MemoryRegionOps ext_nostate_ops = {
    .read = ext_nostate_read,
    .write = ext_nostate_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void ext_nostate_init(Object *obj)
{
    ExtNostateState *s = EXT_NOSTATE(obj);

    memory_region_init_io(&s->mmio, obj, &ext_nostate_ops, s, "ext-nostate",
                          0x100);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static const TypeInfo ext_nostate_info = {
    .name = TYPE_EXT_NOSTATE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ExtNostateState),
    .instance_init = ext_nostate_init,
};

static void ext_devices_register(void)
{
    type_register_static(&ext_counter_info);
    type_register_static(&ext_nostate_info);
}

type_init(ext_devices_register)
