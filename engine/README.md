# engine/: the PANDA engine

The engine is upstream QEMU (a pinned release tarball) plus a patch series plus an overlay of
source files we wrote. `nix build .#engine` builds it with `--enable-panda`; everything else in
panda-ng (the generated headers, plugins, pandare2, the wheel, the .deb, `qemu-system-<arch>`)
is built on top of it. It used to live in `rehosting/qemu_builder` (branches
`workspace/panda-next-pandang`, then `workspace/panda-next-integ`), which panda-ng's flake took as
an input. It moved here in October 2026 so that panda-ng is self-contained: the flake has no
qemu-builder input, and engine work (patches, the overlay, the engine's tests) is done here, in
panda-ng branches and PRs. qemu_builder stays penguin's.

## Layout

```
engine/
  base.json             the upstream release: tag, tarball URL, hash, and patchedTree (the git tree
                        hash the series must produce)
  patches/<version>/    the series (one .patch per topic) and its `series` file (quilt order)
  src/                  the overlay: files we authored (panda/, include/panda/, accel/tcg/hooks.c,
                        system/penguin*.c, contrib/plugins/panda_plugin_interface.c, ...), copied
                        over the patched tree, never patched
  nix/source.nix        tarball + series + src/ -> the patched tree (`nix build .#engine-src`)
  scripts/              verify-series.sh (THE gate), import-series.sh, export-series.sh,
                        probe-versions.sh, offcost-gate.sh, check-delta-present.sh, lib-extract.sh
  tests/guest/          the engine-level guest harness (address hooks, page-in, syscall trap/engine,
                        OSI, rr, off-cost); not run in CI except through the off-cost gate
  tests/modules/        panda modules and the configurable machine on an aarch64 engine build
                        (run.sh BUILD SDK, configurable.sh BUILD; ext-devices.c is also the
                        flake's `board-example`)
```

The configure line, targets and install layout are in the flake (`mkEngine` in `flake.nix`), not
here. The engine's tests on real guests are panda-ng's `tests/*.py` (guest, api, blocks, rr,
fastrestore, ...).

### The invariant

**No patch in the series references a file we created.** Upstream files are changed by patches;
new files go in `src/` and are copied in after the series is applied. A patch that edits a file
an earlier patch created is what made the old chronological fork unrebaseable.

## Working on the engine

All of this runs on Linux (the colima VM is fine). macOS is case-insensitive: QEMU's tree has
paths that differ only in case, and git's `core.ignorecase` changes the tree hash. The scripts set
`core.ignorecase=false`, but keep dev trees on a case-sensitive disk.

```sh
cd engine
url=$(jq -r '.[].url' base.json); curl -LO "$url"       # qemu-11.1.0.tar.xz
./scripts/import-series.sh 11.1.0 qemu-11.1.0.tar.xz /tmp/qemu-dev
```

`/tmp/qemu-dev` is a git tree: the pristine tarball (tagged `base-11.1.0`), one commit per patch,
and a last commit `VENDORED: src/` with the overlay. Build it like the flake does
(`nix develop .#engine`, then `../configure --enable-plugins --enable-panda --target-list=... &&
ninja`), or point a test at it.

**Changing the series.** Commit on top of the dev tree (or `git rebase -i base-11.1.0` to amend a
patch in place; keep each patch topical, not chronological). Then:

```sh
git commit ...                                         # in panda-ng first: export rewrites patches/
./scripts/export-series.sh 11.1.0 /tmp/qemu-dev        # every commit above base-11.1.0 except VENDORED
./scripts/verify-series.sh 11.1.0 qemu-11.1.0.tar.xz --record   # re-records base.json patchedTree
```

Export rules (from the qemu_builder workflow):

- **`src/` is edited here, directly** (`engine/src/...`), not exported. Commits whose subject
  contains `VENDORED:` are skipped by the export. If you edit overlay files in the dev tree, copy
  them back into `engine/src/` by hand and keep that commit `VENDORED:`.
- A new file needed by the engine goes in `src/` (the invariant). A patch only edits upstream files.
- Patch `From:` lines follow the commit author: commit in the dev tree as yourself, with the
  identity you want in the series (the panda-next series uses `Luke Craig <lacraig3@gmail.com>`).
- The export is deterministic (`--no-numbered --zero-commit --full-index`): exporting an unchanged
  dev tree gives no diff. CI checks this (the round trip in `.github/workflows/engine.yml`), so
  never hand-edit a .patch file in a way the export would not produce.
- `export-series.sh` deletes and rewrites `patches/<version>/`: it refuses to run on a dirty
  panda-ng tree, so commit first.
- Renumbering happens only through the export (patches are numbered in dev-tree order). Every
  export or base change needs `verify-series.sh --record`, and the new `patchedTree` is committed
  with the patches.
- Patches that touch a hot path must cost ~nothing when unused: the off-cost gate below.
<!-- REUSE-IgnoreStart -->
- Each patch's message ends with `SPDX-License-Identifier:` (the licenses of the files it changes)
  and `Origin:` (upstream qemu, panda-re/qemu, penguin, ac88u, new) trailers; give a new patch both
  and add it to `patches/<version>/ORIGINS.md`.
<!-- REUSE-IgnoreEnd -->

## Gates

| gate | what | where |
|---|---|---|
| `scripts/verify-series.sh <ver> <tarball>` | tarball sha256 = base.json; every patch applies with **strict** context (`git am`, no 3-way); the patched tree hashes to `patchedTree` | CI (`engine.yml`, every PR touching engine/ or the flake) |
| round trip | `import-series.sh` then `export-series.sh` reproduces `patches/` byte for byte | CI (`engine.yml`) |
| `nix build .#engine-src`, `.#engine-x86_64` + `scripts/check-delta-present.sh` | the series + overlay build; every library carries the core, hypercall and syscall-engine symbols | CI (`engine.yml`) |
| `scripts/offcost-gate.sh` | stock vs this series, same configure line, guest workloads on x86_64 and aarch64: within `TOL` (5%) | manual (`engine-offcost.yml`, a quiet host) |
| `scripts/probe-versions.sh <ver> <qemu.git> <refs>` | per patch: ok / drift (3-way) / moved / conflict on other upstream refs | weekly (`engine-canary.yml`, strict-only) |
| panda-ng's `tests/*.py` on real guests | guest, api, blocks, rr, fastrestore, ... on debian-13 x86_64, aarch64, riscv64 and ubuntu-24.04 x86_64, aarch64 | by hand in the VM |
| `tests/modules/run.sh`, `configurable.sh`; `nix build .#board-example` | panda modules, the SDK, the configurable machine | by hand in the VM |

## Rebasing onto a new QEMU release (next: 11.1.2)

One live version at a time: `base.json` has one entry and `patches/` one directory.

1. Preview: `./scripts/probe-versions.sh 11.1.0 <full qemu.git clone> v11.1.2` (a full clone has
   the pre-image blobs, so drift and conflict are told apart).
2. Dev tree on the old base, then move it:
   ```sh
   ./scripts/import-series.sh 11.1.0 qemu-11.1.0.tar.xz /tmp/rebase
   cd /tmp/rebase && git branch series
   git checkout -q --orphan up && git rm -rqf . && tar xf qemu-11.1.2.tar.xz --strip-components=1
   git add -A && git commit -qm "pristine qemu-11.1.2" && git tag base-11.1.2
   git checkout -q series && git rebase --onto base-11.1.2 base-11.1.0   # resolve conflicts per patch
   ```
   Set `core.ignorecase=false` in that repo if you are not on Linux.
3. Build and test the dev tree (configure as in `mkEngine`), fix the overlay in `engine/src/` for
   upstream API changes.
4. In panda-ng: replace base.json's entry with `"11.1.2": {tag, url, hash (SRI, `nix hash
   convert`/`nix-prefetch-url`), sha256}` (no patchedTree yet), `git mv patches/11.1.0
   patches/11.1.2`, commit, then `./scripts/export-series.sh 11.1.2 /tmp/rebase` and
   `./scripts/verify-series.sh 11.1.2 qemu-11.1.2.tar.xz --record`. Commit.
5. `nix build .#python`, the guest tests on the three debian-13 images, the off-cost gate.

Known for 11.1.2 (checked 2026-10-23: all patches then applied with strict context):

- upstream 827802a ("Use TLB_FORCE_SLOW not TLB_MMIO for system plugins"): `probe_access_internal`
  returns the raw flags with `phost` NULL, so the page-in paths (0013/0014/0015/0017) that test
  only `flags & TLB_MMIO` and then memcpy through `host` must treat `!host` as I/O.
- upstream 14375dd ("Fix TLB_MMIO check in tlb_plugin_lookup()"): `qemu_plugin_hwaddr_is_io`
  starts returning true, so the PANDA bridge's after-access memory callbacks stop firing for MMIO.
  Decide whether `virt_mem_after_*` should still fire there (classic PANDA did) and test it.

## What came from qemu_builder, and what did not

Imported (qemu_builder `integ` 3198bab, the last import): `base.json`, the whole series
(`patches/11.1.0/`, 73 patches, renumbered 0001-0073 by the export), `src/` (minus the three penguin packaging scripts below),
`scripts/` and `tests/`. Not imported, because they build or check penguin's artifact
(`penguin-qemu.tar.gz`: `libqemu-system-*.so` + CFFI bindings for penguin's Python), which panda-ng
does not build:

| qemu_builder path | what it is | why not here |
|---|---|---|
| `build.sh`, `configs/default.json` | penguin-qemu's build: TCG-only `build-system` + KVM build, 14 penguin arch names, the --enable-* feature set (vnc, capstone, libiscsi, libnfs, usb, rdma, curl, ...) | panda-ng configures in `mkEngine` (its own target list and flags); neither file is read by the engine build |
| `nix/qemu.nix`, `flake.nix` outputs `penguin-qemu`, `dist`, `declaredNixDeps` | penguin-qemu package and release tarball | penguin's artifact; panda-ng's packages are `engine`, `wheel`, `deb`, ... |
| `src/scripts/penguin-cffi-gen.py`, `penguin-env-cffi-gen.py`, `penguin-qemu-package.py` (+ a committed `__pycache__/*.pyc`) | CFFI binding and env-module generators and the tarball packager | only build.sh runs them; pandare2 makes its bindings from libpanda-ng's headers |
| `scripts/check-config-contract.sh` | nixDeps of configs/default.json are in penguin-qemu's closure | checks a file that is not here |
| `.github/workflows/build.yml`, `publish.yml` | build/publish penguin-qemu.tar.gz on rehosting's Arc runners | replaced by `engine.yml`; panda-ng publishes only through `release.yaml` |
| `README.md` | qemu_builder's | this file |

`series.yml`, `offcost.yml` and `upstream-canary.yml` were ported to `engine.yml`,
`engine-offcost.yml` and `engine-canary.yml` (GitHub-hosted runners, paths under engine/).

The series itself is imported whole. Several patches exist for penguin (IGLOO) rather than PANDA,
but later patches are built on them (0002's shared-library link is how libpanda is linked; 0045,
PANDA's `guest_hypercall`, sits on the hypercall routing of 0001 and 0003-0008), so removing any
means re-porting the series and re-testing. Which ones PANDA uses:

| patches | for | used by PANDA / pandare2 |
|---|---|---|
| 0001, 0003-0008 | guest hypercalls (penguin core, per-arch routing) | yes: `guest_hypercall` (0045), the hypercaller plugin |
| 0002 | shared libraries, `b_staticpic` | yes: libpanda-<target>.so is linked the same way. Its own `libqemu-system-<penguin name>.so` and `libqemu-kvm-*.so` are extra links of the same objects (the flake symlinks them to libpanda) |
| 0009, 0023 | KVM hypercall and port-I/O exits (x86, arm64 SMCCC) | no (PANDA is TCG); penguin under KVM |
| 0010 | penguin snapshot ABI, migratable vhost-user-vsock | no; IGLOO's vsock |
| 0011, 0029 | penguin reset-request and QMP-command callbacks | no |
| 0012 | mips malta: larger prom env for the kernel command line | any malta guest with a long cmdline |
| 0013-0017, 0032, 0034 | plugin page-in/faulting memory access | the `pagein` lane's explicit page-in path for PANDA's virtual memory API |
| 0018, 0020, 0036 | translation-time address hooks, register pointers | not by src/panda today (penguin's pyhost, tests/guest); 0036 is also part of the CF_PCREL work |
| 0019, 0021, 0033 | syscall trap, penguin syscall engine + host-side OSI, set_pc from a syscall entry | no (PANDA has syscalls2/osi plugins) |
| 0022, 0024 | resume a KVM snapshot under TCG (x86, aarch64 + GIC) | not today; would let a KVM-booted snapshot be analysed under TCG |
| 0025-0028, 0030, 0037-0040, 0050-0052 | record/replay (icount clock continuity, run_on_cpu under rr, rr-ready, begin/end record) | yes: pandare2 rr |
| 0031, 0035 | plugin callback return address; CF_PCREL binding | yes |
| 0041-0049, 0053, 0054, 0058, 0059 | the PANDA spine and callbacks (0059: after_block_exec once the PC is synchronized, panda_tb_pc) | yes |
| 0055-0057, 0060 | dirty logging gaps found by fast restore; vhost devices that can't log | yes: `panda_fast_restore` |
| 0061 | before_loadvm hook | yes |
| 0062 | cpu_exec_step_atomic under RCU | yes (robustness) |
| 0063, 0064 | aarch64 snapshot loads across icount on/off (PMCEID0); riscv64 hardware A/D updates logged as dirty | yes: rr, fast restore |
| 0065 | plugin memory access direction kept out of the MemOp | yes: memory callbacks |
| 0066-0068, 0072, 0073 | rr: replay mutex ownership, rr vCPU loop, replay vmstate registration, snapshot loads judge timers at the loaded clock, loads refused while recording | yes: pandare2 rr |
| 0069-0071 | panda modules (`-module`, `-M path:name`), the configurable machine and its QAPI | yes: `MachineBuilder`, `mkBoard` |

Numbering: qemu_builder's lanes numbered patches by block (0080, 0110, 0120, ...); the first
round trip here renumbered them in series order (0110->0060, 0111->0061, 0080-0082->0062-0064,
0090->0065, 0120-0122->0066-0068, 0130-0132->0069-0071, 0140->0072, 0141->0073). Older notes
use the qemu_builder numbers.

The "no" rows are candidates to drop later; nothing has been dropped (Luke decides).

## Provenance

The cutover is done: engine/ is the source of truth and panda-ng's flake builds it directly.
The last qemu_builder commit imported was 3198bab (qemu_builder `integ`, 73 patches, patchedTree
0647db2c); the import script and its provenance record are gone. A later penguin change in
qemu_builder (to the IGLOO patches, say) is ported here as a patch through the dev tree.
