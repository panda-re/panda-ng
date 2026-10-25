/*
 * The "configurable" machine: a machine described as data. Its memory map,
 * peripherals (by QOM type), interrupt wiring, CPU and entry point are
 * machine properties, set before machine init (on the command line with
 * -M configurable,region.0.base=...,device.0.type=..., or over QMP in
 * preconfig); pandare2's MachineBuilder writes them. The properties' types
 * are QAPI's (qapi/panda-machine.json), so they are parsed and checked by
 * the generated visitors; everything else is checked here, and a bad
 * configuration is an error that names the element, never an assertion.
 *
 * Ideas from Avatar2's configurable machine (memory map, peripherals by QOM
 * type, file-backed regions, entry point); none of its code.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qemu/units.h"
#include "qemu/cutils.h"
#include "qemu/target-info.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qobject/qdict.h"
#include "qobject/qstring.h"
#include "qapi/qapi-visit-panda-machine.h"
#include "qom/qom-qobject.h"
#include "hw/core/boards.h"
#include "hw/core/cpu.h"
#include "hw/core/loader.h"
#include "hw/core/qdev.h"
#include "hw/core/sysbus.h"
#include "hw/core/panda-modules.h"
#include "hw/core/configurable-cpu.h"
#include "hw/arm/machines-qom.h"
#include "hw/riscv/machines-qom.h"
#include "monitor/qdev.h"
#include "system/address-spaces.h"
#include "system/reset.h"
#include "elf.h"

#define TYPE_CONFIGURABLE_MACHINE MACHINE_TYPE_NAME("configurable")
#define CONFIGURABLE_NAME_MAX 128
OBJECT_DECLARE_SIMPLE_TYPE(ConfigurableMachineState, CONFIGURABLE_MACHINE)

struct ConfigurableMachineState {
    MachineState parent;

    PandaMachineRegionList *regions;
    PandaMachineDeviceList *devices;
    bool has_entry;
    uint64_t entry;
    bool has_ram_base;
    uint64_t ram_base;

    CPUState *cpu;
    const ConfigurableCpuOps *cpu_ops;
    MemoryRegion *mrs;          /* one per region, in list order */
    unsigned nregions;
};

/* One mapped range, for the overlap check. */
typedef struct CfgMapEntry {
    uint64_t base, size;
    int32_t priority;
    char *what;
} CfgMapEntry;

static void map_entry_free(gpointer p)
{
    CfgMapEntry *m = p;

    g_free(m->what);
    g_free(m);
}

/* Add [base, base+size) to @map unless it overlaps something at its priority. */
static bool map_claim(GPtrArray *map, uint64_t base, uint64_t size,
                      int32_t priority, char *what, Error **errp)
{
    CfgMapEntry *m;

    if (size == 0) {
        error_setg(errp, "%s: size 0", what);
        g_free(what);
        return false;
    }
    if (base + size - 1 < base) {
        error_setg(errp, "%s: 0x%" PRIx64 "+0x%" PRIx64 " wraps around the "
                   "address space", what, base, size);
        g_free(what);
        return false;
    }
    for (guint i = 0; i < map->len; i++) {
        CfgMapEntry *o = map->pdata[i];

        if (o->priority == priority && base < o->base + o->size &&
            o->base < base + size) {
            error_setg(errp, "%s [0x%" PRIx64 ", 0x%" PRIx64 ") overlaps %s "
                       "[0x%" PRIx64 ", 0x%" PRIx64 ")", what, base,
                       base + size, o->what, o->base, o->base + o->size);
            error_append_hint(errp, "Give one of them a different priority "
                              "if the overlap is intended.\n");
            g_free(what);
            return false;
        }
    }
    m = g_new0(CfgMapEntry, 1);
    m->base = base;
    m->size = size;
    m->priority = priority;
    m->what = what;
    g_ptr_array_add(map, m);
    return true;
}

static uint64_t unimp_read(void *opaque, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "configurable: unimplemented %s: read of %u "
                  "bytes at 0x%" HWADDR_PRIx "\n",
                  memory_region_name(opaque), size, addr);
    return 0;
}

static void unimp_write(void *opaque, hwaddr addr, uint64_t val,
                        unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "configurable: unimplemented %s: write of %u "
                  "bytes (0x%" PRIx64 ") at 0x%" HWADDR_PRIx "\n",
                  memory_region_name(opaque), size, val, addr);
}

static const MemoryRegionOps unimp_ops = {
    .read = unimp_read,
    .write = unimp_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
    .impl.min_access_size = 1,
    .impl.max_access_size = 8,
};

static int region_index(ConfigurableMachineState *cms, const char *name)
{
    int i = 0;

    for (PandaMachineRegionList *l = cms->regions; l; l = l->next, i++) {
        if (g_str_equal(l->value->name, name)) {
            return i;
        }
    }
    return -1;
}

static PandaMachineRegion *region_nth(ConfigurableMachineState *cms, int n)
{
    PandaMachineRegionList *l = cms->regions;

    while (n--) {
        l = l->next;
    }
    return l->value;
}

/* Load @r's file (or the slice of it asked for) as a ROM blob at its base. */
static bool region_load_file(PandaMachineRegion *r, const char *what,
                             Error **errp)
{
    g_autofree char *data = NULL;
    g_autoptr(GError) gerr = NULL;
    gsize len;
    uint64_t off = r->has_file_offset ? r->file_offset : 0;
    uint64_t n;

    if (!g_file_get_contents(r->file, &data, &len, &gerr)) {
        error_setg(errp, "%s: %s", what, gerr->message);
        return false;
    }
    if (off > len) {
        error_setg(errp, "%s: file-offset 0x%" PRIx64 " is past the end of "
                   "'%s' (0x%zx bytes)", what, off, r->file, (size_t)len);
        return false;
    }
    n = r->has_file_size ? r->file_size : len - off;
    if (n > len - off) {
        error_setg(errp, "%s: file-size 0x%" PRIx64 " at offset 0x%" PRIx64
                   " is past the end of '%s' (0x%zx bytes)", what, n, off,
                   r->file, (size_t)len);
        return false;
    }
    if (n > r->size) {
        error_setg(errp, "%s: '%s' gives 0x%" PRIx64 " bytes, more than the "
                   "region (0x%" PRIx64 ")", what, r->file, n, r->size);
        return false;
    }
    /* A ROM blob: written again at every reset, and kept by savevm. */
    rom_add_blob_fixed(r->name, data + off, n, r->base);
    return true;
}

static bool configurable_init_regions(ConfigurableMachineState *cms,
                                      GPtrArray *map, Error **errp)
{
    MemoryRegion *sysmem = get_system_memory();
    PandaMachineRegionList *l;
    unsigned i;

    cms->nregions = 0;
    for (l = cms->regions; l; l = l->next) {
        cms->nregions++;
    }
    cms->mrs = g_new0(MemoryRegion, cms->nregions);

    /* Names first, so an alias may name a region listed after it. */
    for (l = cms->regions, i = 0; l; l = l->next, i++) {
        /* RAMBlock ids ("configurable.<name>") are cut at 256 bytes. */
        if (strlen(l->value->name) > CONFIGURABLE_NAME_MAX) {
            error_setg(errp, "region[%u]: name '%.20s...' is longer than %d "
                       "characters", i, l->value->name, CONFIGURABLE_NAME_MAX);
            return false;
        }
        if (region_index(cms, l->value->name) != i) {
            error_setg(errp, "region[%u]: name '%s' is used twice", i,
                       l->value->name);
            return false;
        }
    }

    for (l = cms->regions, i = 0; l; l = l->next, i++) {
        PandaMachineRegion *r = l->value;
        g_autofree char *what = g_strdup_printf("region[%u] '%s'", i, r->name);
        PandaMachineRegionKind kind = r->has_kind ? r->kind
                                                  : PANDA_MACHINE_REGION_KIND_RAM;
        int32_t prio = r->has_priority ? r->priority : 0;
        g_autofree char *mrname = g_strdup_printf("configurable.%s", r->name);

        if (r->alias_of) {
            int target = region_index(cms, r->alias_of);
            PandaMachineRegion *t;
            uint64_t aoff = r->has_alias_offset ? r->alias_offset : 0;

            if (r->has_kind || r->file) {
                error_setg(errp, "%s: an alias has no kind or file of its own",
                           what);
                return false;
            }
            if (target < 0) {
                error_setg(errp, "%s: alias-of '%s' is not a region", what,
                           r->alias_of);
                return false;
            }
            t = region_nth(cms, target);
            if (t->alias_of) {
                error_setg(errp, "%s: alias-of '%s' is itself an alias", what,
                           r->alias_of);
                return false;
            }
            if (aoff > t->size || r->size > t->size - aoff) {
                error_setg(errp, "%s: 0x%" PRIx64 " bytes at alias-offset "
                           "0x%" PRIx64 " run past the end of '%s' (0x%"
                           PRIx64 " bytes)", what, r->size, aoff, t->name,
                           t->size);
                return false;
            }
            continue;           /* mapped below, once its target exists */
        }
        if (r->has_alias_offset) {
            error_setg(errp, "%s: alias-offset without alias-of", what);
            return false;
        }
        if (r->file && kind == PANDA_MACHINE_REGION_KIND_UNIMP) {
            error_setg(errp, "%s: an unimp region can't have a file", what);
            return false;
        }
        if ((r->has_file_offset || r->has_file_size) && !r->file) {
            error_setg(errp, "%s: file-offset/file-size without a file", what);
            return false;
        }
        if (!map_claim(map, r->base, r->size, prio, g_strdup(what), errp)) {
            return false;
        }
        /*
         * No owner (the machine is not a device): the RAM block is
         * registered for migration globally, under "configurable.<name>"
         * so it can't collide with -m's "ram".
         */
        switch (kind) {
        case PANDA_MACHINE_REGION_KIND_RAM:
            if (!memory_region_init_ram(&cms->mrs[i], NULL, mrname, r->size,
                                        errp)) {
                return false;
            }
            break;
        case PANDA_MACHINE_REGION_KIND_ROM:
            if (!memory_region_init_rom(&cms->mrs[i], NULL, mrname, r->size,
                                        errp)) {
                return false;
            }
            break;
        case PANDA_MACHINE_REGION_KIND_UNIMP:
            memory_region_init_io(&cms->mrs[i], OBJECT(cms), &unimp_ops,
                                  &cms->mrs[i], mrname, r->size);
            break;
        default:
            g_assert_not_reached();
        }
        memory_region_add_subregion_overlap(sysmem, r->base, &cms->mrs[i],
                                            prio);
        if (r->file && !region_load_file(r, what, errp)) {
            return false;
        }
    }

    for (l = cms->regions, i = 0; l; l = l->next, i++) {
        PandaMachineRegion *r = l->value;
        int32_t prio = r->has_priority ? r->priority : 0;

        if (!r->alias_of) {
            continue;
        }
        if (!map_claim(map, r->base, r->size, prio,
                       g_strdup_printf("region[%u] '%s'", i, r->name), errp)) {
            return false;
        }
        g_autofree char *mrname = g_strdup_printf("configurable.%s", r->name);

        memory_region_init_alias(&cms->mrs[i], OBJECT(cms), mrname,
                                 &cms->mrs[region_index(cms, r->alias_of)],
                                 r->has_alias_offset ? r->alias_offset : 0,
                                 r->size);
        memory_region_add_subregion_overlap(sysmem, r->base, &cms->mrs[i],
                                            prio);
    }
    return true;
}

/* Set the device's properties: strings as -device parses them, else typed. */
static bool device_set_props(DeviceState *dev, QObject *props,
                             const char *what, Error **errp)
{
    QDict *d = qobject_to(QDict, props);

    if (!d) {
        error_setg(errp, "%s: prop must be a dictionary of property values",
                   what);
        return false;
    }
    for (const QDictEntry *e = qdict_first(d); e; e = qdict_next(d, e)) {
        QString *s = qobject_to(QString, e->value);
        bool ok = s ? object_property_parse(OBJECT(dev), e->key,
                                            qstring_get_str(s), errp)
                    : object_property_set_qobject(OBJECT(dev), e->key,
                                                  e->value, errp);
        if (!ok) {
            error_prepend(errp, "%s: prop '%s': ", what, e->key);
            return false;
        }
    }
    return true;
}

static int gpio_in_count(DeviceState *dev)
{
    NamedGPIOList *ngl;

    QLIST_FOREACH(ngl, &dev->gpios, node) {
        if (!ngl->name) {
            return ngl->num_in;
        }
    }
    return 0;
}

static bool configurable_init_devices(ConfigurableMachineState *cms,
                                      GPtrArray *map, DeviceState ***devs_out,
                                      Error **errp)
{
    PandaMachineDeviceList *l;
    unsigned i, n = 0;
    DeviceState **devs;

    for (l = cms->devices; l; l = l->next) {
        n++;
    }
    devs = *devs_out = g_new0(DeviceState *, n + 1);

    for (l = cms->devices, i = 0; l; l = l->next, i++) {
        PandaMachineDevice *d = l->value;
        g_autofree char *what = g_strdup_printf("device[%u] (%s)", i, d->type);
        ObjectClass *oc = module_object_class_by_name(d->type);
        DeviceClass *dc;
        DeviceState *dev;
        bool sysbus;
        unsigned j;

        if (!oc) {
            error_setg(errp, "%s: no such QOM type", what);
            error_append_hint(errp, "-device help lists the device types.\n");
            return false;
        }
        dc = (DeviceClass *)object_class_dynamic_cast(oc, TYPE_DEVICE);
        if (!dc || object_class_is_abstract(oc)) {
            error_setg(errp, "%s: not a device type that can be created", what);
            return false;
        }
        sysbus = object_class_dynamic_cast(oc, TYPE_SYS_BUS_DEVICE);
        if (dc->bus_type && !sysbus) {
            error_setg(errp, "%s: sits on a %s bus; the configurable machine "
                       "creates only bus-less and system bus devices", what,
                       dc->bus_type);
            return false;
        }
        if (!sysbus && (d->mmio || d->irq)) {
            error_setg(errp, "%s: mmio and irq need a system bus device",
                       what);
            return false;
        }

        if (d->id && g_str_equal(d->id, "cpu")) {
            error_setg(errp, "%s: id 'cpu' is taken (irq 'cpu:N' names the "
                       "CPU)", what);
            return false;
        }
        dev = qdev_new(d->type);
        if (d->id && !qdev_set_id(dev, g_strdup(d->id), errp)) {
            error_prepend(errp, "%s: ", what);
            object_unref(OBJECT(dev));
            return false;
        }
        if (!d->id) {
            object_property_add_child(machine_get_container("peripheral-anon"),
                                      "device[*]", OBJECT(dev));
        }
        if (d->prop && !device_set_props(dev, d->prop, what, errp)) {
            object_unparent(OBJECT(dev));
            object_unref(OBJECT(dev));
            return false;
        }
        if (!(sysbus ? sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), errp)
                     : qdev_realize_and_unref(dev, NULL, errp))) {
            error_prepend(errp, "%s: ", what);
            return false;
        }
        devs[i] = dev;
        /* Built in or from a module: fast restore only restores vmstate. */
        panda_device_check_vmstate(dev, "savevm and fast restore");

        j = 0;
        for (uint64List *m = d->mmio; m; m = m->next, j++) {
            MemoryRegion *mr;

            if (!sysbus_has_mmio(SYS_BUS_DEVICE(dev), j)) {
                error_setg(errp, "%s: mmio[%u]: the device has %u MMIO "
                           "region(s)", what, j, j);
                return false;
            }
            mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), j);
            if (!map_claim(map, m->value, memory_region_size(mr), 0,
                           g_strdup_printf("%s mmio[%u]", what, j), errp)) {
                return false;
            }
            sysbus_mmio_map(SYS_BUS_DEVICE(dev), j, m->value);
        }
    }
    return true;
}

static DeviceState *irq_target(ConfigurableMachineState *cms,
                               DeviceState **devs, const char *name)
{
    unsigned i = 0;

    if (g_str_equal(name, "cpu")) {
        return DEVICE(cms->cpu);
    }
    for (PandaMachineDeviceList *l = cms->devices; l; l = l->next, i++) {
        if (l->value->id && g_str_equal(l->value->id, name)) {
            return devs[i];
        }
    }
    return NULL;
}

static bool configurable_wire_irqs(ConfigurableMachineState *cms,
                                   DeviceState **devs, Error **errp)
{
    unsigned i = 0;

    for (PandaMachineDeviceList *l = cms->devices; l; l = l->next, i++) {
        unsigned j = 0;

        for (strList *s = l->value->irq; s; s = s->next, j++) {
            g_autofree char *what = g_strdup_printf("device[%u] (%s): irq[%u]",
                                                    i, l->value->type, j);
            const char *colon = strrchr(s->value, ':');
            g_autofree char *name = NULL;
            DeviceState *target;
            uint64_t n;
            int nin;
            bool cpu_irqs;

            if (!*s->value) {
                continue;
            }
            if (!colon || qemu_strtou64(colon + 1, NULL, 0, &n) < 0) {
                error_setg(errp, "%s: '%s' is not TARGET:N", what, s->value);
                return false;
            }
            if (!sysbus_has_irq(SYS_BUS_DEVICE(devs[i]), j)) {
                error_setg(errp, "%s: the device has %u interrupt output(s)",
                           what, j);
                return false;
            }
            name = g_strndup(s->value, colon - s->value);
            target = irq_target(cms, devs, name);
            if (!target) {
                error_setg(errp, "%s: '%s' is neither 'cpu' nor a device id",
                           what, name);
                return false;
            }
            /* MIPS's CPU inputs are env->irq[], not GPIOs. */
            cpu_irqs = target == DEVICE(cms->cpu) && cms->cpu_ops->irq_count;
            nin = cpu_irqs ? cms->cpu_ops->irq_count(cms->cpu)
                           : gpio_in_count(target);
            if (n >= nin) {
                error_setg(errp, "%s: %s has %d interrupt input(s), no input "
                           "%" PRIu64, what, name, nin, n);
                return false;
            }
            sysbus_connect_irq(SYS_BUS_DEVICE(devs[i]), j,
                               cpu_irqs ? cms->cpu_ops->irq_in(cms->cpu, n)
                                        : qdev_get_gpio_in(target, n));
        }
    }
    return true;
}

static void configurable_cpu_reset(void *opaque)
{
    ConfigurableMachineState *cms = opaque;

    cpu_reset(cms->cpu);
    if (cms->cpu_ops->reset) {
        cms->cpu_ops->reset(cms->cpu);
    }
    if (cms->has_entry) {
        cpu_set_pc(cms->cpu, cms->entry);
    }
}

static bool configurable_load_kernel(ConfigurableMachineState *cms,
                                     Error **errp)
{
    MachineState *ms = MACHINE(cms);
    uint64_t entry;
    ssize_t size;

    size = load_elf(ms->kernel_filename, NULL, NULL, NULL, &entry, NULL, NULL,
                    NULL, target_big_endian() ? ELFDATA2MSB : ELFDATA2LSB,
                    cms->cpu_ops->elf_machine(cms->cpu), 0, 0);
    if (size < 0 && size != ELF_LOAD_NOT_ELF) {
        /* An ELF, but not one for this CPU (machine, endianness, size). */
        error_setg(errp, "-kernel %s: %s (this CPU takes ELF machine %d, "
                   "%s-endian)", ms->kernel_filename, load_elf_strerror(size),
                   cms->cpu_ops->elf_machine(cms->cpu),
                   target_big_endian() ? "big" : "little");
        return false;
    }
    if (size < 0) {
        if (!cms->has_ram_base) {
            error_setg(errp, "-kernel %s: not an ELF file, and a raw image "
                       "loads at ram-base, which is not set",
                       ms->kernel_filename);
            return false;
        }
        size = load_image_targphys(ms->kernel_filename, cms->ram_base,
                                   ms->ram_size, errp);
        if (size < 0) {
            return false;
        }
        entry = cms->ram_base;
    }
    if (!cms->has_entry) {
        cms->has_entry = true;
        cms->entry = entry;
    }
    return true;
}

static bool configurable_build(ConfigurableMachineState *cms, Error **errp)
{
    MachineState *ms = MACHINE(cms);
    g_autoptr(GPtrArray) map = g_ptr_array_new_with_free_func(map_entry_free);
    g_autofree DeviceState **devs = NULL;

    cms->cpu_ops = configurable_cpu_ops();
    if (!cms->cpu_ops) {
        error_setg(errp, "no CPU setup for target %s (timers, interrupt "
                   "inputs) yet", target_name());
        error_append_hint(errp, "hw/core/configurable-cpu.c lists the "
                          "targets it supports.\n");
        return false;
    }
    if (!ms->cpu_type) {
        error_setg(errp, "no CPU type: give one with -cpu");
        return false;
    }
    cms->cpu = cms->cpu_ops->create(ms->cpu_type, errp);
    if (!cms->cpu) {
        return false;
    }

    if (cms->has_ram_base) {
        if (!ms->ram) {
            error_setg(errp, "ram-base is set but there is no RAM (-m)");
            return false;
        }
        if (!map_claim(map, cms->ram_base, ms->ram_size, 0,
                       g_strdup("RAM (-m)"), errp)) {
            return false;
        }
        memory_region_add_subregion(get_system_memory(), cms->ram_base,
                                    ms->ram);
    }
    if (!configurable_init_regions(cms, map, errp) ||
        !configurable_init_devices(cms, map, &devs, errp) ||
        !configurable_wire_irqs(cms, devs, errp)) {
        return false;
    }
    if (ms->kernel_filename && !configurable_load_kernel(cms, errp)) {
        return false;
    }
    qemu_register_reset(configurable_cpu_reset, cms);
    return true;
}

static void configurable_init(MachineState *ms)
{
    Error *err = NULL;

    if (!configurable_build(CONFIGURABLE_MACHINE(ms), &err)) {
        error_prepend(&err, "configurable machine: ");
        error_report_err(err);
        exit(EXIT_FAILURE);
    }
}

/* The machine reads its properties once, in init: later sets do nothing. */
static bool configurable_settable(const char *name, Error **errp)
{
    if (phase_check(PHASE_MACHINE_INITIALIZED)) {
        error_setg(errp, "configurable machine: '%s' can't change once the "
                   "machine is built", name);
        return false;
    }
    return true;
}

static void configurable_get_regions(Object *obj, Visitor *v, const char *name,
                                     void *opaque, Error **errp)
{
    visit_type_PandaMachineRegionList(v, name,
                                      &CONFIGURABLE_MACHINE(obj)->regions,
                                      errp);
}

static void configurable_set_regions(Object *obj, Visitor *v, const char *name,
                                     void *opaque, Error **errp)
{
    ConfigurableMachineState *cms = CONFIGURABLE_MACHINE(obj);
    PandaMachineRegionList *list;

    if (!configurable_settable(name, errp) ||
        !visit_type_PandaMachineRegionList(v, name, &list, errp)) {
        return;
    }
    qapi_free_PandaMachineRegionList(cms->regions);
    cms->regions = list;
}

static void configurable_get_devices(Object *obj, Visitor *v, const char *name,
                                     void *opaque, Error **errp)
{
    visit_type_PandaMachineDeviceList(v, name,
                                      &CONFIGURABLE_MACHINE(obj)->devices,
                                      errp);
}

static void configurable_set_devices(Object *obj, Visitor *v, const char *name,
                                     void *opaque, Error **errp)
{
    ConfigurableMachineState *cms = CONFIGURABLE_MACHINE(obj);
    PandaMachineDeviceList *list;

    if (!configurable_settable(name, errp) ||
        !visit_type_PandaMachineDeviceList(v, name, &list, errp)) {
        return;
    }
    qapi_free_PandaMachineDeviceList(cms->devices);
    cms->devices = list;
}

static void configurable_get_addr(Object *obj, Visitor *v, const char *name,
                                  void *opaque, Error **errp)
{
    ConfigurableMachineState *cms = CONFIGURABLE_MACHINE(obj);
    uint64_t val = g_str_equal(name, "entry") ? cms->entry : cms->ram_base;

    visit_type_uint64(v, name, &val, errp);
}

static void configurable_set_addr(Object *obj, Visitor *v, const char *name,
                                  void *opaque, Error **errp)
{
    ConfigurableMachineState *cms = CONFIGURABLE_MACHINE(obj);
    uint64_t val;

    if (!configurable_settable(name, errp) ||
        !visit_type_uint64(v, name, &val, errp)) {
        return;
    }
    if (g_str_equal(name, "entry")) {
        cms->entry = val;
        cms->has_entry = true;
    } else {
        cms->ram_base = val;
        cms->has_ram_base = true;
    }
}

static void configurable_finalize(Object *obj)
{
    ConfigurableMachineState *cms = CONFIGURABLE_MACHINE(obj);

    qapi_free_PandaMachineRegionList(cms->regions);
    qapi_free_PandaMachineDeviceList(cms->devices);
    g_free(cms->mrs);
}

static void configurable_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "machine built from properties (memory map, devices, entry)";
    mc->init = configurable_init;
    mc->max_cpus = 1;
    mc->default_ram_size = 0;
    mc->default_ram_id = "ram";
    mc->no_serial = 0;
    mc->no_parallel = 1;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;

    object_class_property_add(oc, "region", "PandaMachineRegionList",
                              configurable_get_regions,
                              configurable_set_regions, NULL, NULL);
    object_class_property_set_description(oc, "region",
        "memory map: list of {name, base, size, kind=ram|rom|unimp, file, "
        "file-offset, file-size, alias-of, alias-offset, priority}");
    object_class_property_add(oc, "device", "PandaMachineDeviceList",
                              configurable_get_devices,
                              configurable_set_devices, NULL, NULL);
    object_class_property_set_description(oc, "device",
        "peripherals: list of {type, id, mmio: [base...], "
        "irq: ['cpu:N' | 'ID:N' | ''...], prop: {name: value...}}");
    object_class_property_add(oc, "entry", "uint64", configurable_get_addr,
                              configurable_set_addr, NULL, NULL);
    object_class_property_set_description(oc, "entry",
        "PC after every reset (default: -kernel's ELF entry, or the reset "
        "vector)");
    object_class_property_add(oc, "ram-base", "uint64", configurable_get_addr,
                              configurable_set_addr, NULL, NULL);
    object_class_property_set_description(oc, "ram-base",
        "where RAM from -m is mapped (default: not mapped)");
}

static const TypeInfo configurable_machine_info = {
    .name = TYPE_CONFIGURABLE_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(ConfigurableMachineState),
    .instance_finalize = configurable_finalize,
    .class_init = configurable_class_init,
    .interfaces = (const InterfaceInfo[]) {
        { TYPE_TARGET_AARCH64_MACHINE },
        { TYPE_TARGET_ARM_MACHINE },
        { TYPE_TARGET_RISCV32_MACHINE },
        { TYPE_TARGET_RISCV64_MACHINE },
        { }
    },
};

static void configurable_machine_register(void)
{
    type_register_static(&configurable_machine_info);
}

type_init(configurable_machine_register)
