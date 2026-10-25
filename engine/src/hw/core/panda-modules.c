/*
 * Panda modules: external boards and devices loaded as shared objects
 * (-M PATH[:NAME], -module PATH). The loading and the ABI check are
 * util/module.c's module_load_path(); this file keeps which QOM types came
 * from which module, resolves -M PATH[:NAME], and checks module devices
 * for vmstate. All of it is startup or snapshot time, and none of it runs
 * unless a module is named.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "qemu/target-info.h"
#include "qom/object.h"
#include "hw/core/qdev.h"
#include "hw/core/boards.h"
#include "hw/core/panda-modules.h"

/* typename -> path of the module that registered it */
static GHashTable *module_types;
/* realpath -> GPtrArray of its type names */
static GHashTable *modules;

/* What a module's type_init()s registered, from the QOM type registration hook. */
typedef struct ModuleTypes {
    GPtrArray *types;           /* new type names (QOM's strings) */
    GPtrArray *clashes;         /* names that already existed (copies) */
} ModuleTypes;

static void module_type_registered(const char *name, bool duplicate,
                                   void *opaque)
{
    ModuleTypes *mt = opaque;

    if (duplicate) {
        g_ptr_array_add(mt->clashes, g_strdup(name));
    } else {
        g_ptr_array_add(mt->types, (gpointer)name);
    }
}

GPtrArray *panda_module_load(const char *path, Error **errp)
{
    ERRP_GUARD();
    g_autofree char *real = realpath(path, NULL);
    g_autoptr(GPtrArray) clashes = g_ptr_array_new_with_free_func(g_free);
    ModuleTypes mt;
    GPtrArray *types;
    bool ok;

    if (phase_check(PHASE_MACHINE_CREATED)) {
        error_setg(errp, "%s: modules load only before the machine is "
                   "created", path);
        return NULL;
    }
    if (!real) {
        error_setg_errno(errp, errno, "%s", path);
        return NULL;
    }
    if (!modules) {
        modules = g_hash_table_new(g_str_hash, g_str_equal);
        module_types = g_hash_table_new(g_str_hash, g_str_equal);
    }
    types = g_hash_table_lookup(modules, real);
    if (types) {
        return g_ptr_array_ref(types);
    }

    /*
     * The hook records the types the module's inits register, and turns a
     * name that already exists (built in, or from another module) into a
     * record here instead of QOM's abort.
     */
    types = g_ptr_array_new();
    mt.types = types;
    mt.clashes = clashes;
    type_register_set_hook(module_type_registered, &mt);
    ok = module_load_path(real, target_name(), errp);
    type_register_set_hook(NULL, NULL);
    if (!ok) {
        g_ptr_array_unref(types);
        return NULL;
    }
    for (guint i = 0; i < types->len; i++) {
        g_hash_table_insert(module_types, types->pdata[i], g_strdup(path));
    }
    g_hash_table_insert(modules, g_steal_pointer(&real), types);
    if (clashes->len) {
        /*
         * Its other types are registered and its code stays mapped (QOM
         * points into it), so this is fatal to the run; but an error that
         * names the clash, not an abort.
         */
        const char *name = clashes->pdata[0];
        const char *origin = panda_module_type_origin(name);

        if (origin && g_str_equal(origin, path)) {
            origin = "the module itself";
        }
        error_setg(errp, "%s registers QOM type '%s', already registered by "
                   "%s", path, name, origin ? origin : "the engine (built in)");
        if (clashes->len > 1) {
            error_append_hint(errp, "%u more type name(s) clash.\n",
                              clashes->len - 1);
        }
        error_append_hint(errp, "Rename the module's type (a prefix of its "
                          "own, e.g. '<vendor>.<board>-<device>').\n");
        return NULL;
    }
    return g_ptr_array_ref(types);
}

const char *panda_module_type_origin(const char *typename)
{
    return module_types ? g_hash_table_lookup(module_types, typename) : NULL;
}

char *panda_module_machine_spec(const char *spec, Error **errp)
{
    g_autofree char *path = g_strdup(spec);
    g_autoptr(GPtrArray) types = NULL;
    g_autoptr(GString) names = g_string_new("");
    char *want = strrchr(path, ':');
    const char *name = NULL;
    int n = 0;

    if (!strchr(spec, '/') && !g_str_has_suffix(spec, CONFIG_HOST_DSOSUF) &&
        !(want && strstr(spec, CONFIG_HOST_DSOSUF ":"))) {
        return NULL;
    }
    if (want && !strchr(want, '/')) {
        *want++ = '\0';
    } else {
        want = NULL;
    }
    types = panda_module_load(path, errp);
    if (!types) {
        return NULL;
    }
    for (guint i = 0; i < types->len; i++) {
        ObjectClass *oc = object_class_by_name(types->pdata[i]);
        MachineClass *mc;

        /* A machine type of this target (it implements the target's interface). */
        if (!object_class_dynamic_cast(oc, TYPE_MACHINE) ||
            !object_class_dynamic_cast(oc, target_machine_typename()) ||
            object_class_is_abstract(oc)) {
            continue;
        }
        mc = MACHINE_CLASS(oc);
        g_string_append_printf(names, " %s", mc->name);
        if (!want || g_str_equal(want, mc->name)) {
            name = mc->name;
            n++;
        }
    }
    if (n != 1) {
        error_setg(errp, "%s: %s; it registered:%s", path,
                   want ? "no such machine type" : "pick one with PATH:NAME",
                   names->len ? names->str : " no machine types");
        return NULL;
    }
    return g_strdup(name);
}

/* Types already warned about (one warning per type and run). */
static GHashTable *vmstate_warned;

static void warn_no_vmstate(DeviceState *dev, const char *origin,
                            const char *what)
{
    const char *type = object_get_typename(OBJECT(dev));

    if (DEVICE_GET_CLASS(dev)->vmsd) {
        return;
    }
    if (!vmstate_warned) {
        vmstate_warned = g_hash_table_new(g_str_hash, g_str_equal);
    }
    if (g_hash_table_add(vmstate_warned, (gpointer)type)) {
        g_autofree char *path = object_get_canonical_path(OBJECT(dev));

        warn_report("device %s (%s, %s) has no vmstate: %s will not save "
                    "or restore it", path ? path : "(unattached)", type,
                    origin, what);
    }
}

static int check_device_vmstate(Object *obj, void *opaque)
{
    DeviceState *dev = (DeviceState *)object_dynamic_cast(obj, TYPE_DEVICE);
    g_autofree char *from = NULL;
    const char *origin;

    if (!dev || !dev->realized) {
        return 0;
    }
    origin = panda_module_type_origin(object_get_typename(obj));
    if (origin) {
        from = g_strdup_printf("from %s", origin);
        warn_no_vmstate(dev, from, opaque);
    }
    return 0;
}

void panda_modules_check_vmstate(const char *what)
{
    if (!module_types || !g_hash_table_size(module_types)) {
        return;
    }
    object_child_foreach_recursive(object_get_root(), check_device_vmstate,
                                   (void *)what);
}

void panda_device_check_vmstate(DeviceState *dev, const char *what)
{
    const char *origin = panda_module_type_origin(object_get_typename(
                                                      OBJECT(dev)));
    g_autofree char *from = origin ? g_strdup_printf("from %s", origin)
                                   : NULL;

    warn_no_vmstate(dev, from ? from : "built in", what);
}
