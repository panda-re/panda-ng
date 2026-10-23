/*
 * panda-qemu: run the PANDA engine as a plain QEMU binary.
 *
 * The engine is built as libpanda-<arch>-softmmu.so, the library pandare2 loads. This launcher
 * dlopens that same library and calls its main(), so `qemu-system-<arch>` and `Panda(arch=...)`
 * run the very same code, and everything the library exports (the PANDA API, for plugins loaded
 * at run time) is visible to what QEMU loads. It is a few kB instead of a second ~25 MB copy of
 * each target linked as an executable.
 *
 * Usage:
 *   qemu-system-<arch> [qemu args...]   (a link to this program; the arch comes from the name)
 *   panda-system-<arch> [qemu args...]  (same)
 *   panda-qemu <arch> [qemu args...]
 *
 * The library is looked up in $PANDA_QEMU_LIBDIR, else in <dir of this program>/../lib. QEMU finds
 * its firmware relative to the running program (/proc/self/exe), at <dir>/../share/qemu, so the
 * install layout is bin/, lib/, share/qemu/ under one prefix.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *strip_prefix(const char *name)
{
    static const char *prefixes[] = { "qemu-system-", "panda-system-" };
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        size_t n = strlen(prefixes[i]);
        if (strncmp(name, prefixes[i], n) == 0 && name[n]) {
            return name + n;
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    char self[PATH_MAX], lib[PATH_MAX + 64];
    const char *base = strrchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
    const char *arch = strip_prefix(base);

    if (!arch) {
        if (argc < 2 || argv[1][0] == '-') {
            fprintf(stderr, "usage: %s <arch> [qemu args...]  (or run it as qemu-system-<arch>)\n",
                    base);
            return 2;
        }
        arch = argv[1];
        argv++;
        argc--;
        /* QEMU names itself after argv[0] in messages. */
        static char name[128];
        snprintf(name, sizeof(name), "qemu-system-%s", arch);
        argv[0] = name;
    }

    const char *libdir = getenv("PANDA_QEMU_LIBDIR");
    if (!libdir || !*libdir) {
        ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (n <= 0) {
            perror("readlink /proc/self/exe");
            return 2;
        }
        self[n] = 0;
        char *dir = dirname(self);
        snprintf(lib, sizeof(lib), "%s/../lib", dir);
        libdir = lib;
    }

    char path[PATH_MAX + 128];
    snprintf(path, sizeof(path), "%s/libpanda-%s-softmmu.so", libdir, arch);
    if (access(path, R_OK) != 0) {
        fprintf(stderr, "%s: no engine for '%s' (%s not found)\n", base, arch, path);
        return 2;
    }
    /* GLOBAL: plugins QEMU loads later (panda_plugin_interface, PANDA plugins) resolve the PANDA
     * API against it. */
    void *engine = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!engine) {
        fprintf(stderr, "%s: %s\n", base, dlerror());
        return 2;
    }
    int (*qemu_main)(int, char **) = (int (*)(int, char **)) dlsym(engine, "main");
    if (!qemu_main) {
        fprintf(stderr, "%s: %s has no main: %s\n", base, path, dlerror());
        return 2;
    }
    return qemu_main(argc, argv);
}
