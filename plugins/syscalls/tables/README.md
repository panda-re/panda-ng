# syscalls: system call tables

Per profile (an OS and an ABI), every system call's number, name, return type and arguments,
with lookup by number and by name. The plugin core uses them through
[`../syscalls_tables.h`](../syscalls_tables.h).

| File | What |
|---|---|
| `gen_tables.py` | writes `generated/` from the prototype files |
| `linux_extra_prototypes.py` | writes `generated-in/` from a Linux source tree, with syscalls2's `linux_prototypes.py` |
| `regen.sh <linux source>` | both |
| `syscalls_tables.c` | lookup and profile selection |
| `generated/profile_<name>.c` | one profile's calls, by-name index and number index |
| `generated/args.c` | the argument lists of every profile, each distinct list once |
| `generated/profiles.c` | the profile list and the Linux default per ABI |

The generated files are committed. `nix run .#regen-syscalls` regenerates them (with syscalls2's)
from the flake's pinned kernel; `python3 plugins/syscalls/tables/gen_tables.py` alone regenerates
`generated/` after a prototype file changes. The flake check `syscalls-tables` fails when the
committed files differ from what the pinned kernel gives, and `syscalls-tables-test`
(`tests/static/syscalls_tables.py`) checks the compiled tables against the prototype files and
syscalls2.

## Profiles

| ABI | Profile | Calls | Source |
|---|---|---|---|
| `X86_64` | `linux_x64` | 373 | syscalls2 `linux_x64_prototypes.txt` |
| `I386` | `linux_x86` | 446 | syscalls2 `linux_x86_prototypes.txt` |
| `AARCH64` | `linux_arm64` | 326 | syscalls2 `linux_arm64_prototypes.txt` |
| `ARM` | `linux_arm` | 428 | syscalls2 `linux_arm_prototypes.txt` |
| `ARM_OABI` | `linux_arm_oabi` | 440 | `generated-in/linux_arm_oabi_prototypes.txt` |
| `MIPS` (o32) | `linux_mips` | 418 | syscalls2 `linux_mips_prototypes.txt` |
| `MIPS_N64` | `linux_mips64n64` | 366 | syscalls2 `linux_mips64_prototypes.txt`, 5000-5999 |
| `MIPS_N32` | `linux_mips64n32` | 391 | syscalls2 `linux_mips64n32_prototypes.txt` |
| o32, n64, n32 | `linux_mips64` | 1175 | syscalls2 `linux_mips64_prototypes.txt` (syscalls2's mips64 profile, for compat) |
| `PPC` | `linux_ppc` | 430 | syscalls2 `linux_ppc_prototypes.txt` |
| `PPC64` | `linux_ppc64` | 388 | syscalls2 `linux_ppc64_prototypes.txt` |
| `RISCV64` | `linux_riscv64` | 327 | syscalls2 `linux_riscv64_prototypes.txt` |
| `RISCV32` | `linux_riscv32` | 317 | `generated-in/linux_riscv32_prototypes.txt` |
| `LOONGARCH64` | `linux_loongarch64` | 322 | `generated-in/linux_loongarch64_prototypes.txt` |
| `X32` | `linux_x32` | 368 | `generated-in/linux_x32_prototypes.txt` |
| `X86_64` | `freebsd_x64` | 460 | syscalls2 `freebsd_x64_prototypes.txt` |
| `I386`, `X86_64` | `windows_<version>_<x86/x64>` | 238-401 | syscalls2 `windows_*_prototypes.txt`: 2000, xpsp2, xpsp3, 2003sp0 (x86), 2003sp12, vistasp0, vistasp12, 7, 8sp0, 8sp1 |

syscalls2 serves `linux_*` (except the four `generated-in` ones and `linux_mips64n64`),
`freebsd_x64`, `windows_2000_x86`, `windows_xpsp2_x86`, `windows_xpsp3_x86`, `windows_7_x86` and
`windows_7_x64` (`SYSCALLS_PROFILE_COMPAT`). The other Windows prototype files had no syscalls2
profile; their tables are generated the same way and checked only against the prototype files.

Numbers are what user space puts in the number register: with the MIPS ABI offsets (4000, 5000,
6000), with bit 30 (`__X32_SYSCALL_BIT`) on x32, and ARM's private calls from 0x0f0000. An ARM
OABI call has the number EABI gives it (`svc 0x900000 + n` is call n).

### riscv32, loongarch64, x32 and ARM OABI

syscalls2 has no prototype file for these. `linux_extra_prototypes.py` makes them from the same
kernel source as syscalls2's, with syscalls2's `linux_prototypes.py` (its table reader, its
`#if` evaluation of `include/linux/syscalls.h`, its retired-call signatures):

- **riscv32**: `scripts/syscall.tbl`, ABIs `common`, `32` and `arch/riscv/kernel/Makefile.syscalls`'
  `syscall_abis_32` (`riscv`, `memfd_secret`). As in the kernel build, no `time32` rows: riscv32
  has only the 64-bit time calls (`clock_gettime` is 403).
- **loongarch64**: `scripts/syscall.tbl`, ABIs `common` and `64` (its `Makefile.syscalls` adds
  none). Like riscv64 it numbers by the generic table; unlike riscv64 it does not select
  `CONFIG_CLONE_BACKWARDS`, so its `clone` takes `tls` last.
- **x32**: `syscall_64.tbl`, ABIs `common` and `x32`. A compat entry point takes the native
  call's prototype, as syscalls2's tables do for mips n32; `compat_sys_preadv64` and friends,
  which have no native counterpart, take their `include/linux/compat.h` declarations; the
  x32-only `rt_sigreturn` stub takes `sys_rt_sigreturn`'s.
- **ARM OABI**: `arch/arm/tools/syscall.tbl`, ABIs `common` and `oabi`, plus ARM's private calls.
  A row's OABI entry point (its fifth column) gives the prototype when it has a C declaration
  (`sys_oabi_stat64` takes `struct oldabi_stat64 *`); the assembler-only ones (`sys_oabi_pread64`,
  ...) only move a 64-bit argument from an unaligned register pair to an aligned one, and take
  the native prototype. OABI's indirect `syscall` (113, `entry-common.S`) takes the number and
  six arguments.

The no-return and double-return flags come from syscalls2's `context_target.json`: riscv32 and
loongarch64 take `linux:riscv64`'s, x32 `linux:x64`'s, ARM OABI `linux:arm`'s.

Unverified on a guest: every number and argument of these four profiles; riscv32's 64-bit
argument slots (by the RISC-V psABI, a 64-bit scalar takes the next two registers, not an
aligned pair); x32's argument widths (`long` and pointers are 4 bytes in user space and are
passed in 64-bit registers).

## Arguments

Each argument has:

- `name`: syscalls2's (the declared name; `arg<i>` when the prototype gives none; `new`, `data`,
  `int`, `cpu` with a leading `_`), so per-syscall events and compat callbacks have the same
  parameter names.
- `ctype`: the declared type without `__user`, as the prototype file has it.
- `kind`, `width`, `is_signed`: from `ctype` by the type lists in `gen_tables.py`. A type not in
  them fails generation. `char *` is a string unless the argument's name ends in `buf` (as in
  syscalls2), `void *` a buffer, `char **` an array of strings, a pointer to a struct (or a
  Windows `P<X>`) a struct pointer with `type_name`, any other pointer `PTR`.
- `slot`, `nslots`: where the value is in the argument registers and stack. A 64-bit integer on
  a 32-bit ABI takes two slots; on ARM EABI, MIPS o32 and PPC32 the pair starts at an even slot
  (`syscall(2)`), on i386, ARM OABI and riscv32 at the next one. Which slot holds the high half
  follows the guest's endianness.
- `compat_type`, `compat_size`, `compat_type_name` (and `syscalls_compat_ctype()`): what
  syscalls2 has for it: the PPP callback parameter type, and `get_syscall_info()`'s `argt`,
  `argsz`, `argtn`. syscalls2 makes one `SysCall` of the calls in a profile with the same name
  and callback parameters, and reports the first one's details for all of them; these fields do
  the same.

Call flags: `SYSCALLS_NORETURN`, `SYSCALLS_DOUBLE_RETURN` (`context_target.json`),
`SYSCALLS_NO_COMPAT_INFO` (syscalls2's `get_syscall_info()` has no entry for the number) and
`SYSCALLS_NO_COMPAT_CALLBACK` (syscalls2 has no `on_<name>_enter`).

## The sources as they are

The tables take the prototype files as they are. Where they differ from the kernel, or syscalls2
reads them differently:

- `freebsd_x64_prototypes.txt`: line 271 holds two calls (`325 munlockall(void); 326 __getcwd(...)`);
  syscalls2 reads one, 325 `__getcwd`. The tables have both. Five calls return a pointer
  (`17 break`, `71`, `197` and `477 mmap`, `228 shmat`); syscalls2's parser skips them, the tables
  have them with `SYSCALLS_NO_COMPAT_CALLBACK`. Line 224 (`252 AUE_POLL OBSOL openbsd_poll`) is
  not a prototype; 252 is absent. 30 names have more than one signature (`sigaction` 46, 342,
  416; `getfsstat`, `fstat`, `mmap`, ...); syscalls2 has one callback typedef per name.
- mips n32 (`linux_mips64n32` and the 6000-6999 of `linux_mips64`): the 71 rows whose kernel
  entry point is `compat_sys_<x>` have the native `sys_<x>` prototype, with native structure
  types (as do x32's 28). `6332 io_pgetevents` (`compat_sys_io_pgetevents`, `old_timespec32`) has
  the same prototype as `6416` (`__kernel_timespec`).
- `context_target.json`: the no-return lists differ by arch for the same calls (`sys_execve` is
  in `linux:x86` and `linux:arm`, not in `linux:x64` or `linux:mips`; `sys_exit_group` is listed
  twice for x64 and mips; `sys_sigreturn` and `sys_setcontext` are not Linux x86_64 calls).
- syscalls2's types for 64-bit ABIs make `size_t` (and FreeBSD's `id_t`, `key_t`, `semid_t`,
  `dev_t`, riscv64's `uintptr_t`) a `uint32_t`; Windows pointers are `SYSCALL_ARG_U32`/`U64`;
  `char **` is `SYSCALL_ARG_STR_PTR`. The tables' `kind` and `width` are the declared type's; the
  `compat_*` fields keep syscalls2's.
- syscalls2's `get_syscall_info()` for mips n32 has 1 of the 391 calls (6000), and for mips64
  misses the 2 n32 calls whose name and parameters no o32 or n64 call has.
