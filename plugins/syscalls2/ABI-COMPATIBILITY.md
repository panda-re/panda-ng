# Linux ABI compatibility

syscalls2 traces guests running older kernels as well as the kernel used to generate its
tables. A call becoming `sys_ni_syscall` in a recent kernel is not sufficient reason to remove
its callback. `scripts/linux_prototypes.py` keeps historical signatures for `_sysctl`,
`lookup_dcookie`, `bdflush`, `nfsservctl`, `create_module`, `get_kernel_syms` and `query_module`.
Their numbers and ABI membership still come from the selected kernel table. They are also
recognized when a newer guest invokes them and receives `ENOSYS`.

Historical signatures are documented in Linux's
[v5.8 syscall declarations](https://github.com/torvalds/linux/blob/v5.8/include/linux/syscalls.h)
(`_sysctl`, `bdflush`, `lookup_dcookie`),
[v2.6.39 declarations](https://github.com/torvalds/linux/blob/v2.6.39/include/linux/syscalls.h)
(`nfsservctl`), and the Linux man-pages for
[create_module](https://man7.org/linux/man-pages/man2/create_module.2.html),
[get_kernel_syms](https://man7.org/linux/man-pages/man2/get_kernel_syms.2.html) and
[query_module](https://man7.org/linux/man-pages/man2/query_module.2.html).
Pointer annotations do not change argument extraction: syscalls2 passes guest addresses without
dereferencing these historical structures.

## Cross-check of PR #21's removals

Compared all ten Linux input tables with
[`hrw/syscalls-table` at b019db68a150885e04d99176105525896ad7a9af](https://github.com/hrw/syscalls-table/tree/b019db68a150885e04d99176105525896ad7a9af)
(`data/tables/syscalls-*`, reporting Linux 7.3.0-rc1), and checked disputed entries against
the generating Linux 6.19.6 source. The sources answer different questions: HRW lists exported
UAPI numbers, including some reserved slots, and explicitly excludes some removed calls in
[`data/removed-names.text`](https://github.com/hrw/syscalls-table/blob/b019db68a150885e04d99176105525896ad7a9af/data/removed-names.text).
Neither its presence nor its absence alone proves a call was implemented by an ABI.

Of the original PR's 122 removed number/profile entries, 46 are restored by retaining historical
signatures. MIPS64 contains o32+n64+n32, so the same underlying ABI is counted again in its
separate plugin profiles. The remaining exclusions are below. Additional historical slots
previously absent or named `sys_ni_syscall` now have their real signatures too.

| Profile | Before PR | Original PR | With historical calls | Original removals restored |
|---|---:|---:|---:|---:|
| x64 | 318 | 367 | 373 | 2/2 |
| x86 | 363 | 439 | 446 | 3/3 |
| arm | 356 | 424 | 428 | 3/9 |
| arm64 | 283 | 324 | 326 | 2/2 |
| riscv64 | 285 | 325 | 327 | 2/3 |
| mips (o32) | 393 | 411 | 418 | 7/17 |
| mips64 (o32+n64+n32) | 1091 | 1156 | 1175 | 19/43 |
| mips64n32 | 359 | 385 | 391 | 6/13 |
| ppc | 430 | 423 | 430 | 1/8 |
| ppc64 | 402 | 381 | 388 | 1/22 |

## Entries still excluded

These numbers continue through `on_unknown_sys_*`. A matching UAPI number is not interpreted
using a different architecture's argument signature.

- ARM EABI: 13 `time`, 25 `stime`, 27 `alarm`, 30 `utime`, 102 `socketcall`, 117 `ipc` are
  OABI-only in `arch/arm/tools/syscall.tbl`. HRW's ARM EABI table also omits them. This does not
  add an OABI runtime profile.
- riscv64: 38 `renameat` is behind the generic table's `renameat` ABI tag, which riscv64 does
  not select. HRW also omits it.
- MIPS o32 (including the o32 range in mips64): 4018/4028/4084/4150 are `unused18`, `unused28`,
  `unused84`, `unused150`; 4048 `signal`, 4101 `ioperm`, 4110 `iopl`, 4112 `idle`, 4123
  `modify_ldt` and 4318 `timerfd` are unimplemented slots in the MIPS table. HRW still exports
  numbers for signal/ioperm/iopl/modify_ldt/timerfd; that does not give them the x86 implementation
  or turn timerfd into timerfd_create. The old callbacks for the unused slots supplied invented
  signatures; very old MIPS kernels are not covered by a separate historical ABI profile.
- MIPS n64: 5174–5177, 5193, 5236 and 5277 were all named `sys_ni_syscall(void)`. They are
  reserved/unimplemented slots, not callable signatures. n32 has corresponding slots
  6174–6177, 6193, 6240 and 6281. HRW exports `timerfd` at 5277/6281; the others are absent.
- Both PowerPC profiles: 101 `ioperm`, 110 `iopl`, 113 `vm86`, 123 `modify_ldt`, 201
  `multiplexer`, 453 `map_shadow_stack` have `sys_ni_syscall` as their entry. HRW reports their
  numbers, but they are not the x86 calls with the same names.
- PowerPC32: 363 `switch_endian` is unimplemented; its real implementation is PowerPC64-only.
  HRW exports the number for both.
- PowerPC64: 18 `oldstat`, 22 `umount`, 28 `oldfstat`, 59 `oldolduname`, 67 `sigaction`,
  72 `sigsuspend`, 73 `sigpending`, 76 `getrlimit`, 82 `select`, 84 `oldlstat`, 89 `readdir`,
  109 `olduname`, 119 `sigreturn`, 126 `sigprocmask`, 256 `sys_debug_setcontext` select
  `sys_ni_syscall` in the **64** row. These are not the implemented **32** rows. The appropriate
  modern 64-bit interfaces remain present. HRW exports these slot numbers too.

The pinned kernel stops at 470. HRW's later calls 471/472 do not change the version pin here.
The comparison also found pre-existing, unimplemented UAPI-only slots outside the PR's removals
(x64 205/211/214/215, MIPS o32 4113 and n64 5450); they remain outside the generated prototypes.

`scripts/test_linux_prototypes.py`, run during regeneration and the `syscalls2-tables` flake
check, protects historical numbers/argument counts, ABI exclusions and missing-prototype errors.
Guest tests exercise retired-call arguments and returns on modern kernels as well as newer calls.
