# Tests

Each test is a script run with pandare2's Python; its docstring says what it checks and how to
run it. Build that Python once:

```sh
nix build .#python -o result-python      # python3 with pandare2: the engine, the plugins
export PANDA_PYTHON=$PWD/result-python/bin/python3
```

| kind | tests | needs |
|---|---|---|
| static | `static/refs.py [arch...]` | nothing, no guest: every engine and plugin function, C type and CPUArchState field pandare2 names exists, on every arch (`nix build .#checks.x86_64-linux.pandare2-refs`) |
| static | `static/layout.py <engine>/include/panda-ng [arch...]` | nothing, no guest: pandare2's cffi lays out CPUState, CPUArchState and the engine's structs as gcc does, on every arch (`nix build .#checks.x86_64-linux.layout`) |
| static | `static/capabilities.py`, `static/analysis_policy.py` | Python (and C compiler for native policy), no engine/guest: capability normalization, zero default and startup freeze |
| static | `static/vanilla_inventory.py` | Python only: every patch digest/classification and overlay covered; not a vanilla parity test |
| static | `static/lifecycle.py` | Python, C compiler and GLib, no engine or guest: transient cancellation/reset-hook lifetime and fast-restore notification (`nix build .#checks.x86_64-linux.lifecycle`) |
| static | `static/replay_bh.py <patched replay/replay-events.c>` | Python and a C compiler, no engine or guest: pending BH coalescing and reentrant event drains (`nix build .#checks.x86_64-linux.replay-bh`) |
| static | `static/replay_native.py` | Python and a C compiler, no engine or guest: extracted native mutation guards and Penguin continuations (`nix build .#checks.x86_64-linux.replay-native`) |
| static | `static/replay_policy.py` | Python only, no engine or guest: RR admission and observe-only wrapper policy (`nix build .#checks.x86_64-linux.replay-policy`) |
| static | `bash static/syscalls_dispatch.sh <output binary>` | a C++ compiler, no engine or guest: generated syscalls2 dispatch and PPP subscriptions (`nix build .#checks.x86_64-linux.syscalls2-dispatch`) |
| smoke | `smoke.py <arch>` | nothing: boots the firmware, loads C plugins (CI runs it on every PR) |
| guest suite | `guest api osi_isf blocks callbacks calls mem mmio late reset teardown asidstory tbpc hookalias rr rrcb rrstop rrload fuzz fastrestore fastcov syscalls` | an image (below); `run.sh` runs them |
| more guest tests | `kpti physmem rr_blocks fastrestore_guards fastrestore_hotplug fastrestore_vsock` | an image; not in `run.sh`'s default list (x86_64-only, or need extra host tools: see each docstring) |
| reset/restore lifecycle | `lifecycle.py <image>` | real public owner reset-hook ordering, ordinary/fast restore notifications, rejection, unregister/re-register and hardware reset; `RRCB_HANDOFF=1` only for fixtures with handoff snapshot extensions; pending/held/injected slot and safe teardown cases are host-only in `static/lifecycle.py` and `static/penguin_detach.py` |
| replay safety | `replay_guard.py <image> [workdir]` | record/replay observer and mutation refusal; `RRCB_HANDOFF=1` explicitly admits fixtures saved with handoff snapshot extensions |
| benchmark | `bench_cb.py <image>` | an image, and `PANDA_PLUGIN_DIR` from `nix build .#test-plugins` (the `tests/plugins/cbcount` C baseline): ns per callback in C, bare cffi and `@panda.cb_*` |
| constructor | `construct.py [case...]` | nothing, no guest: the argv each `Panda(...)` form builds, which keyword wins over an image, and the errors (`nix build .#checks.x86_64-linux.pandare2-construct`) |
| several Pandas | `multi.py [case...]`, `multi.py guests <x86_64 json> <aarch64 json>` | nothing (the guests case: two images): Pandas in one process, each case in a child process (`nix build .#checks.x86_64-linux.pandare2-multi`) |
| bare metal | `blocks_bare.py mips\|ppc`, `machine.py <arch>...`, `machine_sw.py <arch>...`, `machine_builder.py` | nothing |
| boards | `board.py <at91sam9260.so>`, `machine_raw.sh <engine bin> [arch...]`, `engine/tests/modules/` | see "Boards" |

## Images

The guest tests take an image description written by `pandare2.mkimage`: a distribution's cloud
image with a `root` snapshot at a logged-in serial shell, plus the guest kernel's symbols (Volatility
ISF and osi_linux kernelinfo). Build them with the PANDA that will run them: the snapshot is taken
by that PANDA, with the arguments pandare2 restores it with.

```sh
$PANDA_PYTHON -m pandare2.mkimage debian-13-x86_64 debian-13-aarch64 debian-13-riscv64 \
    ubuntu-24.04-x86_64 ubuntu-24.04-aarch64 -o ~/panda-images
```

Each writes `<name>.qcow2`, `<name>.json`, `<name>.isf.json.xz` and `<name>.kernelinfo.conf`.
Downloads (cloud images, kernel debug packages) are cached in `~/.cache/panda-images`; once a
kernel's vmlinux is extracted there, its debug package (1-1.8 GB) can be deleted. Building is one
boot under TCG plus symbol extraction: on a 2026 x86_64 server, all five in parallel, 5-6 min
each for debian-13, 11 min for ubuntu x86_64 and 21 min for ubuntu aarch64, downloads included.
`debian-13-ppc64` is also a recipe; the suite isn't run on it yet.

## Running the suite

```sh
tests/run.sh ~/panda-images                      # every image in the directory, the default tests
TESTS="rr rrcb" tests/run.sh ~/panda-images/debian-13-x86_64.json
```

`run.sh` runs the images in parallel (`JOBS`) and the tests on each image one after another, each
on its own copy of the image, so the shared images are never written. Results go to
`$OUT/results`, one line per run, with each run's log in `$OUT/<image>/<test>.log`; a failing run
keeps its image copy and work dir (`KEEP=1` keeps all of them). `TIMEOUT` (default 1800 s) bounds
each test. On non-x86_64 images it sets `RRCB_NO_ICOUNT=1`: rrcb then checks the callbacks without
comparing the guest's icount reads, which are not exact there yet.

The full default run (19 tests on 5 images, all at once) took 19 minutes on that server.

## Performance tools

These tools are independent of the guest suite runner. Offline protocol selftests need only
stdlib Python and Linux `taskset` (util-linux); no engine or guest fixture is needed:

```sh
python3 -I tests/perf/test_perf.py
nix build .#checks.x86_64-linux.perf-tools
```

The independent `perf-tools` flake check uses pinned Python, Bash, coreutils, util-linux and a
C compiler. It runs the protocol selftests, shell syntax checks and the real `idlework.c` CPU
workload once on the host for correctness, including invalid repetition-count rejection.
This does not boot an engine or establish guest correctness or performance.

`tests/perf/run.py` documents its schema-1 configuration and artifact requirements. `--check`
validates configuration offline; `--execute CONFIG.json NEW_OUT` is only for an explicitly
reserved quiet window, with already prepared inputs and no concurrent builds. It compares the
installed shared-engine launcher with matching vanilla QEMU and identical shared A/A aliases,
rotates sample order, preserves every repeat and requires correctness/exit markers before
samples. No icount or writable guest drives are allowed. Artifacts/configuration/logs are
hashed, and shared-engine mappings must actually be observed in `/proc`. Reanalyze retained
evidence with `python3 -I tests/perf/analyze.py NEW_OUT`; contamination invalidates a campaign.
There is no automatic performance verdict or mandatory shared-host timing threshold.

`perf/idlework.c` checks sort ordering/permutation, its VM against a scalar reference and a CRC
known vector. `perf/mkinitrd.sh STATIC_BUSYBOX STATIC_IDLEWORK NEW_OUT.gz` packages separately
prepared, static target-compatible inputs using cpio and gzip; it refuses existing output.
Keep exact compiler/configuration records. The minimal read-only initrd accepts `pt=iw-cpu:N`.
Host correctness alone is not integrated boot evidence.

`bench_cb.py` is callback attribution, not an end-to-end acceptance gate. It needs pandare2,
`qemu-img`, a standalone snapshot-bearing qcow2 JSON fixture and the C baseline plugin above.
It privately copies the fixture, retains all positive-count repeats and summarizes medians
rather than the fastest repeat. Bound the whole process externally; serial commands are
bounded individually. Missing C phases are explicitly skipped, not a pass. Restore/activation
is outside dispatch samples. `engine/scripts/offcost-gate.sh` is retained only as a historical
series-vs-series diagnostic: it does not measure the shipped shared engine or establish
vanilla parity.

## Boards

- `machine.py`, `machine_sw.py`: the engine's built-in `configurable` machine from
  `pandare2.MachineBuilder`; no image or external board needed.
- `machine_raw.sh <dir with qemu-system-*> [arch...]`: the same machine without PANDA, on the
  launcher (`nix build .#qemu`, then `result/bin`).
- `board.py <at91sam9260.so>`: an external board loaded with `-M path:name`. The board comes from
  rehosting/qemu_builder's at91sam9260-board branch, built against the engine's SDK
  (`nix build .#plugin-sdk`, then its `panda-board-cc`, or the flake's `lib.<system>.mkBoard`).
- `engine/tests/modules/run.sh <dir with qemu-system-aarch64> <sdk>` and `configurable.sh`: panda
  modules, the SDK's ABI checks and the configurable machine on an aarch64 engine. They assemble
  aarch64 guest code with `as`/`ld`, so run them on an aarch64 host or with aarch64 binutils
  first on `PATH`. `nix build .#board-example` builds `ext-devices.c` as a module.
