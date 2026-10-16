# Guest harness: the pagein and syscall-engine sources, under real kernels

These tests boot penguin's own kernels (linux_builder, 6.13 on 11 arches, 4.10
on 7) with a small initramfs, and check the QEMU side of the delta against
what the guest itself sees:

- **pagein**: `qemu_plugin_{read,write,access}_memory_vaddr_fault`, a
  consistent read or write of guest memory that may be paged out, made by
  having the task take the fault;
- **TCG address hooks** (`accel/tcg/hooks.c`, `qemu_plugin_hook_insert*`) and
  `qemu_plugin_register_ptr`;
- **the syscall trap** (`system/syscall-trap.c` and the x86/arm target hooks),
  **the penguin syscall engine** (`system/penguin-syscalls.c`) and **the host
  OSI** (`system/penguin-osi.c`), including syscall injection and the exec
  hook.

The guest program (`pagetest.c`) checks itself against its own view (its
own reads, `/proc`), and the host side logs what QEMU saw. The `.sh`
scripts compare the two and write verdicts and tables.

It is not run in CI: it needs about 20 kernel boots per suite and minutes of
guest time. It is the check for any change to the files above.

## Inputs

`./prepare.sh` fetches and builds everything into `dl/` and `gen/`
(git-ignored). `env.sh` lists each input and its environment override:

| input | default | from |
|---|---|---|
| `QBUILD` | (required) | a QEMU build of this tree: `qemu-system-*`, `libqemu-system-*.so`, `pc-bios` |
| `KERNEL_DIR` | `dl/kernels` | linux_builder `v4.0.1` `kernels-latest.tar.gz` (penguin's pin) |
| `BUSYBOX_DIST` | `dl/busybox-dist` | `nix build github:rehosting/busybox/eeba468aa#dist` |
| `ZCC` | `dl/zig` | `ziglang` 0.16.0 from PyPI (`zig cc -target ...`) |
| `PENGUIN` | `../../../penguin` | a penguin checkout, for the suites that run penguin's own plugins |
| `IGLOO_DRIVER` | `../../../igloo_driver` | an igloo_driver checkout: the portal ops penstub's `hyper.consts` reads |

Host tools: `gcc`, `pkg-config` (glib), `mksquashfs`, `cpio`, `llvm-readelf`
and `llvm-objdump` (address-hook symbols), Python 3 with `cffi` and `dwarffi`.

QEMU is configured the way the harness was developed against:

```sh
mkdir build-rel && cd build-rel
../configure --target-list=x86_64-softmmu,aarch64-softmmu,arm-softmmu,mipsel-softmmu,mips-softmmu,mips64el-softmmu,mips64-softmmu,ppc64-softmmu,riscv64-softmmu,loongarch64-softmmu \
  --enable-plugins --disable-docs --disable-werror --disable-gtk --disable-sdl \
  --disable-opengl --disable-virglrenderer --disable-spice --disable-vnc
ninja
```

## Suites

Every script takes an output directory or `OUT=` (default `out/out-<suite>`)
and exits non-zero if a check failed.

| script | what | where |
|---|---|---|
| `full.sh [phase...]` | pagein on every kernel: reads, writes, hazards (no VMA, EOF, PROT_NONE, read-only), contention, multi-page and batched accesses; `summary.py <out>` tabulates | 18 kernels |
| `kvm.sh` | the same matrix under x86 KVM | x86_64 hosts |
| `hktest.sh` | address hooks: hit counts, mid-block and delay-slot sites, register writes, redirects, faults, aliasing, insert/remove from a callback, conditional hooks, SMP | 11 arches |
| `hkbench.sh` | address-hook costs | 11 arches |
| `sctest.sh func tfunc sctrap` | the syscall plugin (`plugins/syscalls.c`) on its own and on the trap | 11 arches / x86_64, aarch64 |
| `sctest.sh pen` | the `penguin_syscall_*` C ABI under `pyhost.py`: filters, signals, restarts, vfork, execve, threads, faults held across a callback, the reaper, table eviction | x86_64, aarch64 |
| `sctest.sh pyplug` | penguin's own `Syscalls` (backend `trap`) and `mem` on penguin's `qemu_compat`, under `penhost.py` | x86_64, aarch64 |
| `injtest.sh` | syscall injection: ids, ENOSYS, a /proc read through injected calls, signals, EINTR, restarts, a failed execve, refusals, 1000 calls | x86_64 6.13, aarch64 6.13 and 4.10 |
| `ositest.sh` | the host OSI against the guest's /proc (`osiref.py` walks the same structures in Python); `osicdef.py` checks penguin's cdef against `src/include/system/penguin-osi.h` | same |
| `osibench.sh` | what the engine's OSI filters cost per rejected call | x86_64, aarch64 |
| `PENHOST=first run.sh <arch> <ver> user-novma` | injection at the system's first syscall (init's), before init has run | same |
| `PENHOST=takeover run.sh <arch> <ver> user-novma` | pid 1 runs `ours.c` from a memfd, then the real init | same |

x86_64 4.10 is left out of the trap suites: the trap core delivers one event
there, then none (open).
