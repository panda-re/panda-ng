/*
 * The syscalls plugin's system call tables: per profile (an OS and an ABI), each call's number,
 * name and argument types, looked up by number or by name.
 *
 * The tables are generated (tables/gen_tables.py) from syscalls2's prototype files and the ones
 * in tables/generated-in; tables/README.md says what each profile comes from.
 *
 * Numbers are the values user space passes in the number register: with the ABI's offset on
 * MIPS (o32 4000, n64 5000, n32 6000), with __X32_SYSCALL_BIT (0x40000000) on x32, and ARM's
 * private calls from 0x0f0000. ARM OABI numbers are the ones EABI uses (svc 0x900000 + n: n).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef SYSCALLS_TABLES_H
#define SYSCALLS_TABLES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ABIs: the values of enum qemu_plugin_syscall_abi (system/syscall-trap.h's SYSCALL_ABI_*);
 * new ones are appended, existing ones are never renumbered.
 */
enum {
    SYSCALLS_ABI_X86_64,
    SYSCALLS_ABI_I386,
    SYSCALLS_ABI_AARCH64,
    SYSCALLS_ABI_ARM,
    SYSCALLS_ABI_ARM_OABI,
    SYSCALLS_ABI_MIPS,          /* o32 */
    SYSCALLS_ABI_PPC64,
    SYSCALLS_ABI_PPC,
    SYSCALLS_ABI_RISCV64,
    SYSCALLS_ABI_RISCV32,
    SYSCALLS_ABI_LOONGARCH64,
    SYSCALLS_ABI_MIPS_N32,
    SYSCALLS_ABI_MIPS_N64,
    SYSCALLS_ABI_X32,
    SYSCALLS_ABI_COUNT
};

enum {
    SYSCALLS_OS_LINUX,
    SYSCALLS_OS_FREEBSD,
    SYSCALLS_OS_WINDOWS,
};

/* What an argument is. */
typedef enum {
    SYSCALLS_ARG_INT,           /* signed integer of `width` bytes */
    SYSCALLS_ARG_UINT,          /* unsigned integer of `width` bytes */
    SYSCALLS_ARG_LONG,          /* signed, register-sized (long, off_t on Linux) */
    SYSCALLS_ARG_ULONG,         /* unsigned, register-sized (size_t, HANDLE, ...) */
    SYSCALLS_ARG_PTR,           /* pointer to a scalar, a pointer or a function */
    SYSCALLS_ARG_STR,           /* pointer to a NUL-terminated string */
    SYSCALLS_ARG_WSTR,          /* pointer to a NUL-terminated UTF-16 string (Windows PWSTR) */
    SYSCALLS_ARG_BUF,           /* pointer to bytes (void *, a char * named *buf) */
    SYSCALLS_ARG_STRUCT_PTR,    /* pointer to a structure: `type_name` */
    SYSCALLS_ARG_STR_ARRAY,     /* pointer to a NULL-terminated array of strings (argv) */
} SyscallArgKind;

/* syscalls2's syscall_argtype_t (syscalls2_info.h), the same values */
enum {
    SYSCALLS_COMPAT_U64 = 0x00,
    SYSCALLS_COMPAT_U32 = 0x01,
    SYSCALLS_COMPAT_U16 = 0x02,
    SYSCALLS_COMPAT_S64 = 0x10,
    SYSCALLS_COMPAT_S32 = 0x11,
    SYSCALLS_COMPAT_S16 = 0x12,
    SYSCALLS_COMPAT_BUF_PTR = 0x20,
    SYSCALLS_COMPAT_STRUCT_PTR = 0x21,
    SYSCALLS_COMPAT_STR_PTR = 0x22,
};

typedef struct SyscallArg {
    const char *name;           /* syscalls2's: declared, "arg<i>" if none, reserved words '_'-prefixed */
    const char *ctype;          /* the declared type, without __user: "const char *" */
    const char *type_name;      /* STRUCT_PTR: the structure ("stat", "OBJECT_ATTRIBUTES"); else NULL */
    const char *compat_type_name; /* syscalls2's argtn: the structure, or "n/a" */
    uint8_t kind;               /* SyscallArgKind */
    uint8_t width;              /* bytes in the guest ABI (a pointer: its size) */
    uint8_t is_signed;
    uint8_t slot;               /* first argument slot (register, then stack) */
    uint8_t nslots;             /* 2: a 64-bit integer on a 32-bit ABI, in slot and slot + 1 */
    uint8_t compat_type;        /* syscalls2's syscall_argtype_t (SYSCALLS_COMPAT_*) */
    uint8_t compat_size;        /* syscalls2's argsz: the size of its callback parameter */
} SyscallArg;

/* syscalls2's callback parameter type for an argument: "uint32_t", "int64_t", ... */
static inline const char *syscalls_compat_ctype(const SyscallArg *a)
{
    bool s = a->compat_type == SYSCALLS_COMPAT_S64 || a->compat_type == SYSCALLS_COMPAT_S32 ||
             a->compat_type == SYSCALLS_COMPAT_S16;
    switch (a->compat_size) {
    case 8: return s ? "int64_t" : "uint64_t";
    case 4: return s ? "int32_t" : "uint32_t";
    case 2: return s ? "int16_t" : "uint16_t";
    }
    return NULL;
}

/* SyscallInfo.flags */
#define SYSCALLS_NORETURN           0x1 /* syscalls2's panda_noreturn (context_target.json) */
#define SYSCALLS_DOUBLE_RETURN      0x2 /* syscalls2's panda_doublereturn */
/* syscalls2's get_syscall_info() for this profile has no entry for the number */
#define SYSCALLS_NO_COMPAT_INFO     0x4
/* syscalls2's profile has no on_<name>_enter (always, where syscalls2 lacks the profile) */
#define SYSCALLS_NO_COMPAT_CALLBACK 0x8

typedef struct SyscallInfo {
    int64_t num;
    const char *name;           /* sys_read, NtReadFile, mmap: the callback name syscalls2 uses */
    const char *rettype;
    int8_t abi;                 /* the call's ABI (differs within a SYSCALLS_PROFILE_MULTI_ABI one) */
    uint8_t nargs;
    uint8_t nslots;             /* argument slots used: nargs, plus one per 64-bit pair and gap */
    uint8_t flags;
    const SyscallArg *args;
} SyscallInfo;

/* SyscallProfile.flags */
#define SYSCALLS_PROFILE_COMPAT     0x1 /* syscalls2 has this profile */
#define SYSCALLS_PROFILE_MULTI_ABI  0x2 /* several ABIs by number (linux_mips64: o32, n64, n32) */

typedef struct SyscallProfile {
    const char *name;           /* "linux_x64", "windows_7_x86", "freebsd_x64", ... */
    int os;                     /* SYSCALLS_OS_* */
    int abi;                    /* SYSCALLS_ABI_*, -1 for a multi-ABI profile */
    const char *version;        /* Windows: "7", "xpsp3", ... (the prototype file's); else NULL */
    const char *source;         /* the prototype file, from the repository root */
    unsigned flags;
    uint32_t ncalls;
    const SyscallInfo *calls;   /* by number */
    const uint16_t *by_name;    /* indexes of calls, by name (strcmp), then number */
    int64_t dense_lo;           /* dense[n - dense_lo]: index + 1 of call n, 0 for none */
    uint32_t dense_n;
    const uint16_t *dense;
    uint32_t max_args;
    /* syscalls2's syscall_meta_t for this profile; zeros if syscalls2 doesn't have it */
    uint32_t compat_max;
    uint32_t compat_max_generic;
    uint32_t compat_max_args;
} SyscallProfile;

/* every profile */
const SyscallProfile *const *syscalls_profiles(size_t *n);
const SyscallProfile *syscalls_profile_by_name(const char *name);
/*
 * The profile for an OS (SYSCALLS_OS_*), a version and an ABI. Linux and FreeBSD ignore the
 * version. Windows takes the prototype files' versions ("2000", "xpsp2", "xpsp3", "2003sp0",
 * "2003sp12", "vistasp0", "vistasp12", "7", "8sp0", "8sp1") and their service packs
 * ("7sp1", "vistasp2", "2003sp1", ...).
 */
const SyscallProfile *syscalls_profile_find(int os, const char *version, int abi);

/* A call of one profile. */
const SyscallInfo *syscalls_profile_lookup(const SyscallProfile *p, int64_t num);
/* The lowest-numbered call of that name. */
const SyscallInfo *syscalls_profile_lookup_name(const SyscallProfile *p, const char *name);

/*
 * The profile each ABI uses. Linux's are selected until a select call; select before the first
 * lookup on another thread (the selection is not synchronized).
 */
const SyscallProfile *syscalls_profile(int abi);
void syscalls_set_profile(int abi, const SyscallProfile *p);
/* Selects an OS's profiles for every ABI it has (others: none). Returns how many. */
int syscalls_select(int os, const char *version);
/*
 * Selects from an OS name as PANDA gives it (plugins/osi/README.md): "linux-64-...",
 * "windows-32-7sp1", "freebsd-64-...". Returns how many ABIs got a profile; 0 when the name is
 * not understood (the selection is then unchanged).
 */
int syscalls_select_os_name(const char *os_name);

const SyscallInfo *syscalls_lookup(int abi, int64_t num);
const SyscallInfo *syscalls_by_name(int abi, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SYSCALLS_TABLES_H */
