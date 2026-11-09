# Guest harness: the pagein and syscall-trap sources, under real kernels

These tests boot penguin's own kernels (linux_builder, 6.13 on 11 arches, 4.10
on 7) with a small initramfs, and check the QEMU side of the delta against
what the guest itself sees:

- **pagein**: `qemu_plugin_{read,write,access}_memory_vaddr_fault`, a
  consistent read or write of guest memory that may be paged out, made by
  having the task take the fault;
- **TCG address hooks** (`accel/tcg/hooks.c`, `qemu_plugin_hook_insert*`) and
  `qemu_plugin_register_ptr`;
- **the syscall trap** (`system/syscall-trap.c` and the x86/arm target hooks),
  through the syscall plugin (`plugins/syscalls.c`).

The guest program (`pagetest.c`) checks itself against its own view (its
own reads, `/proc`), and the host side logs what QEMU saw. The `.sh`
scripts compare the two and write verdicts and tables.

It is not run in CI, except the off-cost gate (`offcost.sh`, by hand from the
Off-cost workflow): the suites need about 20 kernel boots each and minutes of
guest time. It is the check for any change to the files above.

## Inputs

`./prepare.sh` fetches and builds everything into `dl/` and `gen/`
(git-ignored). `env.sh` lists each input and its environment override:

| input | default | from |
|---|---|---|
| `QBUILD` | (required) | the build directory of an engine tree (below): `qemu-system-*`, `libqemu-system-*.so`, `pc-bios` |
| `KERNEL_DIR` | `dl/kernels` | linux_builder `v4.0.1` `kernels-latest.tar.gz` (penguin's pin) |
| `BUSYBOX_DIST` | `dl/busybox-dist` | `nix build github:rehosting/busybox/eeba468aa#dist` |
| `ZCC` | `dl/zig` | `ziglang` 0.16.0 from PyPI (`zig cc -target ...`) |

Host tools: `gcc`, `pkg-config` (glib), `mksquashfs`, `cpio`, `llvm-readelf`
and `llvm-objdump` (address-hook symbols), Python 3.

`QBUILD` is a build directory, not an installed engine: the harness wants `qemu-system-<arch>`,
`libqemu-system-<arch>.so` and `pc-bios/` side by side, and the tree's `include/plugins` one level
up (`QSRC`). The flake's `engine` output does have `lib/libqemu-system-<arch>.so` (links to
`libpanda-<target>-softmmu.so`), but in the installed layout (`bin/`, `lib/`, `share/qemu/`), so
it can't be `QBUILD`. Make an engine tree with `engine/scripts/import-series.sh`
(`engine/README.md`, "Working on the engine") and, in it, configure QEMU the way the harness was
developed against:

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
| `hktest.sh` | address hooks: hit counts, mid-block and delay-slot sites, register writes, redirects, faults, aliasing, insert/remove from a callback, conditional hooks, SMP, a hook inserted into a hot loop on a page that ran CF_PCREL (`late`) | 11 arches |
| `hkbench.sh` | address-hook costs | 11 arches |
| `sctest.sh func tfunc sctrap` | the syscall plugin (`plugins/syscalls.c`) on its own and on the trap | 11 arches / x86_64, aarch64 |
| `setpctest.sh` | `qemu_plugin_set_pc()` from a syscall entry callback resumes the task, not the trap again (`plugins/scsetpc.c`) | x86_64, aarch64, armel 6.13 |
| `offcost.sh STOCK SERIES` | the off-cost gate: the series with nothing loaded against stock (the series at `STOCK_REF`, default `origin/main`; same tarball, same configure line; `scripts/offcost-gate.sh` builds both), and an rr-ready launch against a plain one; wall time per workload (boot and translation, syscalls, virtio swap), fails above `TOL` (5%) / `RRTOL` (10%) | x86_64, aarch64 6.13 |

x86_64 4.10 is left out of the trap suites: the trap core delivers one event
there, then none (open).
