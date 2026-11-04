/*
 * Lookup in the generated system call tables (generated/), and profile selection.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <string.h>

#include "../syscalls_tables.h"

extern const SyscallProfile *const syscalls_profile_list[];
extern const size_t syscalls_profile_count;
extern const SyscallProfile *const syscalls_linux_default[SYSCALLS_ABI_COUNT];

static const SyscallProfile *selected[SYSCALLS_ABI_COUNT];
static bool selected_init;

static void init_selection(void)
{
    if (!selected_init) {
        memcpy(selected, syscalls_linux_default, sizeof(selected));
        selected_init = true;
    }
}

const SyscallProfile *const *syscalls_profiles(size_t *n)
{
    *n = syscalls_profile_count;
    return syscalls_profile_list;
}

const SyscallProfile *syscalls_profile_by_name(const char *name)
{
    for (size_t i = 0; i < syscalls_profile_count; i++) {
        if (!strcmp(syscalls_profile_list[i]->name, name)) {
            return syscalls_profile_list[i];
        }
    }
    return NULL;
}

/* Windows service packs that share a prototype file */
static const char *const windows_aliases[][2] = {
    { "7sp0", "7" }, { "7sp1", "7" },
    { "2003sp1", "2003sp12" }, { "2003sp2", "2003sp12" },
    { "vistasp1", "vistasp12" }, { "vistasp2", "vistasp12" },
};

static bool version_matches(const SyscallProfile *p, const char *version)
{
    if (p->os != SYSCALLS_OS_WINDOWS) {
        return true;
    }
    if (!version) {
        return false;
    }
    if (!strcmp(p->version, version)) {
        return true;
    }
    for (size_t i = 0; i < sizeof(windows_aliases) / sizeof(windows_aliases[0]); i++) {
        if (!strcmp(windows_aliases[i][0], version) && !strcmp(windows_aliases[i][1], p->version)) {
            return true;
        }
    }
    return false;
}

const SyscallProfile *syscalls_profile_find(int os, const char *version, int abi)
{
    for (size_t i = 0; i < syscalls_profile_count; i++) {
        const SyscallProfile *p = syscalls_profile_list[i];
        if (p->os == os && p->abi == abi && version_matches(p, version)) {
            return p;
        }
    }
    return NULL;
}

const SyscallInfo *syscalls_profile_lookup(const SyscallProfile *p, int64_t num)
{
    if (!p) {
        return NULL;
    }
    if (num >= p->dense_lo && num - p->dense_lo < (int64_t)p->dense_n) {
        unsigned i = p->dense[num - p->dense_lo];
        return i ? &p->calls[i - 1] : NULL;
    }
    size_t lo = 0, hi = p->ncalls;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (p->calls[mid].num < num) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo < p->ncalls && p->calls[lo].num == num ? &p->calls[lo] : NULL;
}

const SyscallInfo *syscalls_profile_lookup_name(const SyscallProfile *p, const char *name)
{
    if (!p || !name) {
        return NULL;
    }
    size_t lo = 0, hi = p->ncalls;
    while (lo < hi) {   /* the first of that name: by_name is by name, then number */
        size_t mid = lo + (hi - lo) / 2;
        if (strcmp(p->calls[p->by_name[mid]].name, name) < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < p->ncalls && !strcmp(p->calls[p->by_name[lo]].name, name)) {
        return &p->calls[p->by_name[lo]];
    }
    return NULL;
}

const SyscallProfile *syscalls_profile(int abi)
{
    if (abi < 0 || abi >= SYSCALLS_ABI_COUNT) {
        return NULL;
    }
    init_selection();
    return selected[abi];
}

void syscalls_set_profile(int abi, const SyscallProfile *p)
{
    if (abi < 0 || abi >= SYSCALLS_ABI_COUNT) {
        return;
    }
    init_selection();
    selected[abi] = p;
}

int syscalls_select(int os, const char *version)
{
    const SyscallProfile *found[SYSCALLS_ABI_COUNT];
    int n = 0;

    for (int abi = 0; abi < SYSCALLS_ABI_COUNT; abi++) {
        found[abi] = os == SYSCALLS_OS_LINUX ? syscalls_linux_default[abi]
                                             : syscalls_profile_find(os, version, abi);
        n += found[abi] != NULL;
    }
    if (n) {
        memcpy(selected, found, sizeof(selected));
        selected_init = true;
    }
    return n;
}

int syscalls_select_os_name(const char *os_name)
{
    static const struct { const char *prefix; int os; } families[] = {
        { "linux-", SYSCALLS_OS_LINUX },
        { "windows-", SYSCALLS_OS_WINDOWS },
        { "freebsd-", SYSCALLS_OS_FREEBSD },
    };

    if (!os_name) {
        return 0;
    }
    for (size_t i = 0; i < sizeof(families) / sizeof(families[0]); i++) {
        size_t len = strlen(families[i].prefix);
        if (strncmp(os_name, families[i].prefix, len)) {
            continue;
        }
        /* <family>-<bits>-<variant>[-...]: Windows' variant is its version */
        const char *variant = strchr(os_name + len, '-');
        char version[32] = "";
        if (variant) {
            size_t vlen = strcspn(variant + 1, "-");
            if (vlen < sizeof(version)) {
                memcpy(version, variant + 1, vlen);
                version[vlen] = '\0';
            }
        }
        return syscalls_select(families[i].os, version);
    }
    return 0;
}

const SyscallInfo *syscalls_lookup(int abi, int64_t num)
{
    return syscalls_profile_lookup(syscalls_profile(abi), num);
}

const SyscallInfo *syscalls_by_name(int abi, const char *name)
{
    return syscalls_profile_lookup_name(syscalls_profile(abi), name);
}
