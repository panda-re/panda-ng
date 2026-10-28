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
| static | `bash static/syscalls_dispatch.sh <output binary>` | a C++ compiler, no engine or guest: generated syscalls2 dispatch and PPP subscriptions (`nix build .#checks.x86_64-linux.syscalls2-dispatch`) |
| smoke | `smoke.py <arch>` | nothing: boots the firmware, loads C plugins (CI runs it on every PR) |
| guest suite | `inventory.py` quick / focused-guest / broad selections | an image (below); `run.sh` materializes every selected test/variant and records requirements or explicit SKIP |
| specialized guest tests | `rr_blocks fastrestore_guards fastrestore_hotplug fastrestore_vsock kpti physmem` | focused or broad inventory tiers; target/host/backend prerequisites are explicit (see below) |
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
tests/run.sh ~/panda-images                      # unchanged legacy default selection
TIER=quick tests/run.sh ~/panda-images/debian-13-x86_64.json
TIER=broad tests/run.sh ~/panda-images
TESTS="rr rrcb" tests/run.sh ~/panda-images/debian-13-x86_64.json
python3 tests/inventory.py focused-guest          # JSON inventory + generated count, no execution
```

`inventory.py` is a small schema-1 requirements inventory, not an engine mode-implementation
matrix. Tiers are cumulative: `quick` selects guest/API sanity; `focused-guest` adds block,
callback, MMIO, syscall, restore and record/replay checks including `rr_blocks` and the
replay-rejection half of restore guards; `broad` adds all other inventoried guest tests.
Counts come from selection. With no explicit `TIER`, unset or empty `TESTS` preserves the exact
original default selection and order. An explicit `TIER` opts into that tier when `TESTS` is unset
or empty; a nonempty `TESTS` selects exactly those names, including aliases or existing unlisted scripts. Unlisted scripts have **unknown**
target/mode requirements, explicitly recorded rather than inferred as supported.

Each cell records intended targets, host, required resources, mode coverage labels, script and
arguments. Known incompatible targets or missing host requirements become `SKIP` with reasons;
missing scripts/fixtures remain `ERROR`, even when a resource would otherwise cause a skip. No
selected cell disappears. The initially declared guest targets are x86_64/aarch64/riscv64;
hotplug and KPTI are x86_64-only. These are test applicability
claims only, not new engine support claims.

Restore variants make conditional coverage separate instead of quietly passing fewer assertions:

| selection | tier | arguments / requirements |
|---|---|---|
| `fastrestore_guards_rr` | focused-guest | `fastrestore_guards.py ... rr`; record/replay rejection, no KVM requirement |
| `fastrestore_guards_kvm` | broad | `... notcg`; matching host/guest architecture, usable `/dev/kvm` |
| `fastrestore_hotplug` | broad | x86_64 ACPI PCI hotplug |
| `fastrestore_vsock` | broad | executable `VHOST_DEVICE_VSOCK` or `vhost-device-vsock` on PATH |
| `fastrestore_vsock_kernel` | broad | `... --kernel`; matching host target, usable `/dev/vhost-vsock` |

Direct `TESTS=fastrestore_guards` still runs that script's original combined default; legacy
partial skips are conservatively `SKIP`. Opening a device is only a prerequisite check, not proof
of working virtualization: errors during the actual test stay failures. Additional kernel/backend
constraints must be evidenced by execution, not hidden behind a blanket pass.

The guest workflow uses focused-guest for PR/manual runs and broad on its nightly schedule, with
manual tier/explicit selections available. It uploads only bounded result/provenance JSON and
logs, excluding private disks, recording data and symbol JSON files.

`run.sh` uses the stdlib-only `run_results.py` supervisor; it needs host `python3`, `git`, and
GNU `cp`, plus the selected `PANDA_PYTHON` (default `panda-python`). It runs images in parallel
(`JOBS`, default all selected images), tests within an image serially. The matrix is persisted
**before** inspecting fixtures or probing pandare2. Image and test ordinal IDs prevent duplicate
basenames or repeated selections from colliding. `$OUT` must not exist, even if empty; the default
is a new temporary directory. Nothing in an existing output directory is removed.

Each attempt copies JSON and qcow2 privately (reflink/sparse when available), rewrites the copied
JSON's file references, and links symbol files. Missing JSON, disks, symbols, scripts, copy failures
and failed interpreter preflight are `ERROR`, not missing cells. Short `/tmp/pt.*` directories
keep UNIX socket paths below their limit. `TIMEOUT` (default 1800 seconds) starts at process launch;
a watchdog sends TERM then KILL after `KILL_GRACE` (default 30 seconds). Only an actual watchdog
expiry is `TIMEOUT`: spontaneous signals are `CRASH`, including SIGKILL, and exits 124/137 without
a watchdog are `FAIL`. Core dumps are disabled. TERM/INT cancellation reconciles running and
not-yet-started cells as `CANCELLED`. Unexpected worker death reconciles unfinished cells as
`ERROR` and kills its recorded test process group. SIGKILL/power loss of the coordinator cannot
run cleanup; the durable plan and unfinished records remain evidence, not a passing run.

`PASS` means exit zero without a skip indication, not proof of full mode coverage. `SKIP` means
exit 77 or a legacy line beginning `SKIP` / containing `: SKIP`, even with exit zero. A partial
subtest skip conservatively marks the whole cell `SKIP`; its log identifies what was excluded.
A nonzero exit or signal still takes precedence over printed skips. New whole-test skips should
exit 77 and print their reason. The runner exits nonzero for FAIL/CRASH/TIMEOUT/ERROR/CANCELLED,
source-fixture changes or failed workers; SKIP is non-failing but never counted as PASS. Strict
`Checks.check/report` XPASS behavior is unchanged.

On non-x86_64 images the runner records `RRCB_NO_ICOUNT=1`: rrcb checks callbacks without comparing
the guest's icount reads, which are not exact there yet. The inventory records this exclusion through the attempt environment; a pass is not evidence
of non-x86 icount equivalence. Mode labels describe test intent, never implementation status.
The matrix size and summary counts are generated from actual selection, not a fixed test count.

### Result schema and retained evidence

All JSON documents carry `schema_version: 1`:

- `plan.json`: ordered cells, selected interpreter, controls, fixture paths/architecture and
  SHA-256 identities of JSON/disks/symbols, including missing-input errors.
- `provenance.json`: source revision, dirty status, tracked diff hash, patch-tree content identity,
  runner/helper identities, host and package-selection overrides. Dirty/untracked status is not a
  complete source archive: preserve that checkout alongside its evidence.
- `results`: readable `STATUS <seconds>s <image-id> <test>` lines, exactly one per planned cell.
  `results.json` contains the same denominator with authoritative first outcomes.
- `<image-id>/<test-id>/result.json`: authoritative **first** outcome; `attempt-001/result.json`
  and subsequent numbered attempts retain argv, cwd, environment overrides, test-source identity,
  timestamps, exit code/signal, watchdog flag, bounded process/stack evidence where `/proc`
  permits, and best-effort observed engine/plugin/Python mappings with resolved paths and hashes.
  Selection overrides and the interpreter's pandare2 import probe are **not** evidence that a
  selected engine/plugin was actually loaded. Mapping sampling can miss short-lived libraries;
  absent observations mean unknown, not a verified selection. `python-probe.log/json` records
  package import provenance separately.
- Each attempt's `test.log` is bounded to a 1 MiB head/tail, with truncation explicit. A JSON
  reproduction command reruns the single selected cell in fresh output using the recorded source,
  interpreter and original fixtures; its preconditions are recorded, not silently reconstructed.
- `completion.json`: worker exits, cancellation/fatal errors, and shared-fixture before/after
  hash differences. Hashing all selected fixtures is deliberate and may add I/O for large disks.

Failure image copies and recordings stay in their attempt directory; successful copies/recordings
are removed unless `KEEP=1`. Short temporary directories are removed after supervised attempts.
Large disks, recordings and core dumps are **not** copied into the bounded diagnostic JSON/log
bundle: CI should upload manifests/JSON/logs, not recursively upload retained `img/` or `rr/`.
No debugger is required; stack evidence is best effort and usually unavailable on restricted hosts.

Retries are opt-in (`RETRIES=1`, default 0) and use independent fresh fixture copies and TMPDIRs.
The first failure remains the cell's verdict and determines the runner exit status even if the
retry passes; passing retries do not erase flakiness. First-failure directories are never replaced.

### Offline runner selftests

```sh
python3 tests/run_selftests.py
# Retain intentional failures and their evidence in a NEW directory:
python3 tests/run_selftests.py --out /tmp/panda-runner-evidence
```

No guest build, pandare2 install or new framework is needed (`unittest` and fake processes only).
The cases cover pass/assertion/strict XPASS, whole and partial skips, signals including spontaneous
SIGKILL, timeout TERM/KILL escalation, numeric exits 124/137, invalid/missing fixtures and scripts,
preflight/setup failure, worker death, pending/running cancellation, duplicate basenames and tests,
output reuse refusal, bounded noisy logs, first-failure retention and fresh-copy retries. Shared
JSON and disk hashes are checked before/after runs. Failed selftests retain their temporary tree;
`--out` retains all evidence, including expected synthetic failures. Selection tests additionally
cover missing backend SKIP without launching, alias arguments, generated tier counts and empty
explicit selection fallback.

CI has an independent lightweight `offline-tools` flake check/job, using only pinned stdlib Python,
Git, Bash and coreutils on Linux (the loaded-library selftest reads `/proc`). It runs all 18 runner
selftests and `tools/docs/gen.py --check`. It deliberately uses the complete source snapshot so
sibling imports, AST source reads, documentation inputs and patch metadata are available. This
check has no engine/guest build dependency and does not replace series application/import-export
verification or runtime guest tests.

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
