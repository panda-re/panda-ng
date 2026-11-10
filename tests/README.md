# Tests

Each test is a script run with pandare2's Python; its docstring says what it checks and how to
run it. `tests/run` runs them by area, tier and arch from the metadata each declares (below,
"Running tests"). Build that Python once:

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
| static | `static/events_registry.py` | Python, C compiler and GLib, no engine or guest: the event registry (`events.c`) against a model under ASan/UBSan: binding, pending, mismatch, refusal, dispatch order, instance data, unload, RCU, UUIDv5 vectors (`nix build .#checks.x86_64-linux.events-registry`) |
| static | `static/lifecycle.py` | Python, C compiler and GLib, no engine or guest: transient cancellation/reset-hook lifetime and fast-restore notification (`nix build .#checks.x86_64-linux.lifecycle`) |
| static | `static/replay_bh.py <patched replay/replay-events.c>` | Python and a C compiler, no engine or guest: pending BH coalescing and reentrant event drains (`nix build .#checks.x86_64-linux.replay-bh`) |
| static | `static/replay_native.py` | Python and a C compiler, no engine or guest: extracted native mutation guards (`nix build .#checks.x86_64-linux.replay-native`) |
| static | `static/guest_layout.py <engine>/include/panda/guest-layout` | Python, no guest: the guest-write layout map the engine build generated from its debug info agrees with the committed bindings (`bindings_layout.py`) on every arch (`nix build .#checks.x86_64-linux.guest-layout`) |
| static | `static/guest_view.py` | Python with cffi, no engine or guest: pandare2's env view turns field assignments into the engine's env writes, through pointers by path (`nix build .#checks.x86_64-linux.guest-view`) |
| static | `static/replay_policy.py` | Python only, no engine or guest: RR admission and observe-only wrapper policy (`nix build .#checks.x86_64-linux.replay-policy`) |
| static | `bash static/syscalls_dispatch.sh <output binary>` | a C++ compiler, no engine or guest: generated syscalls2 dispatch and PPP subscriptions (`nix build .#checks.x86_64-linux.syscalls2-dispatch`) |
| smoke | `smoke.py <arch>` | nothing: boots the firmware, loads C plugins (CI runs it on every PR) |
| guest suite | `guest api osi_isf blocks block_activation callbacks calls mem mmio late reset teardown asidstory tbpc hookalias rr rrcb rrstop rrload fuzz fastrestore fastcov syscalls taint taint_mmio taint_errors` | an image (below); `run.sh` runs them |
| every arch | `busybox.sh [arch...]` | nix and network: builds `mkbusybox.py` images (a linux_builder kernel, a busybox initramfs, an ext2 disk) for 12 arches and runs `taint taint_mmio taint_errors` on them through `run.sh`, about 3 minutes. CI runs it on PRs that touch taint or record/replay, and nightly |
| more guest tests | `kpti physmem rr_blocks fastrestore_guards fastrestore_hotplug fastrestore_vsock` | an image; not in `run.sh`'s default list (x86_64-only, or need extra host tools: see each docstring) |
| test machines | `testmachine.py <busybox json> [--smp N] [--intel]` | a `mkbusybox.py` image (x86_64, aarch64, armel): builds and checks its test machine (below) |
| reset/restore lifecycle | `lifecycle.py <image>` | real public owner reset-hook ordering, ordinary/fast restore notifications, rejection, unregister/re-register and hardware reset; `RRCB_HANDOFF=1` only for fixtures with handoff snapshot extensions; pending/held/injected slot and safe teardown cases are host-only in `static/lifecycle.py` |
| replay safety | `replay_guard.py <image> [workdir]` | record/replay observer and mutation refusal; `RRCB_HANDOFF=1` explicitly admits fixtures saved with handoff snapshot extensions |
| recorded RAM effects | `replay_effect.py <x86_64 image> [workdir]` | deferred overlapping RAM writes, then two fresh producer-free replays |
| guest writes | `hypercall_rr.py <x86_64, aarch64 or riscv64 image> [workdir]` | a `mutates=True` hypercall handler's memory and return-register writes (through the guest-write API) recorded, then replayed without the handler, with it registered (it must not run) and again |
| malformed RAM effects | `replay_effect_invalid.py <x86_64 image> <recording prefix> [workdir]` | copies a passing `replay_effect.py` log; rejects unknown version, oversized length and truncated payload; retains original recording |
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

## Test machines

`testmachine.py` turns a `mkbusybox.py` image into a machine for tests that run guest programs:
the image's kernel and machine arguments with an initramfs of the image's busybox and the guest
agent (`programs/agent.c`) as `/init`, snapshotted at the agent's first poll (one boot, 3-6 s) and
cached next to the image (`machines/`, or `$TESTMACHINE_CACHE`). A test copies the snapshot,
reverts to it (0.3-0.6 s) and moves files and commands through the agent's hypercalls and guest
memory (`Agent.put`, `run`, `get`): no boot and no serial console. The agent mounts `/dev`,
`/proc`, `/sys`, tracefs and `/tmp`.

Machines exist for x86_64, aarch64 and arm (busybox-armel). An arch needs:

1. **An image**: `mkbusybox.py <arch>` (all 12 of its arches have one).
2. **The system layer** in `programs/sys.h`, under the compiler's arch macro: `NR_<name>` for the
   calls the agent makes, `sc(n, a, b, c, d, e, f)` (a system call returning `-errno` on error),
   `hc(op, a, b, c)` (penguin's hypercall instruction with the magic in the number register and in
   argument 0, op, a, b and c in arguments 1-4, the host's answer in the return register; the
   instructions the engine decodes are in engine patches 0003-0008), `hc_setup()`,
   `clone_fn(flags, stack, ctid, fn)`, `SIGRETURN_ASM` and `_start`.
3. **The build**: the arch in `programs/build.sh`'s list for `agent` (its nixpkgs `pkgsCross`
   name and flags), then `sh tests/programs/build.sh agent`.
4. **The machine**: its pandare2 arch in `testmachine.AGENT_ARCH`.

## Running tests

`tests/run` collects one item per test, arch and variant from each test file's `PANDA_TEST`
metadata, selects them and runs each in its own process, through pytest (`tests/pandatest/`):

```sh
tests/run --tier static                     # the static tier, in parallel
tests/run smoke --arch x86_64
tests/run taint --arch x86_64,aarch64       # the taint tests on busybox guests
tests/run testmachine --build-fixtures      # builds the missing machines first
tests/run --list --explain taint            # what would run, why, the commands, what is skipped
```

Terms are areas (`taint`), test names (`smoke`, `static/refs`) or item id globs
(`'testmachine[x86_64,*smp=4*]'`: `*` and `?` match, brackets are literal); none selects
everything. `--tier` and `--arch` take comma-separated lists. `--variants default` runs each
variant at its default value only; `--variants all` (the default) runs every combination. Other
options go to pytest (`-x`, `-k`).

Arches are named by the engine target they run on (`libpanda-<target>-softmmu.so`), in item ids,
metadata and `--arch`; `--arch` also takes `mkbusybox.py`'s names:

| arch | mkbusybox | | arch | mkbusybox |
|---|---|---|---|---|
| `x86_64` | `x86_64` | | `ppc` | `powerpc` |
| `aarch64` | `aarch64` | | `ppc64` | `powerpc64` |
| `arm` | `armel` | | `ppc64le` | `powerpc64le` (on the `ppc64` target) |
| `mips` | `mipseb` | | `riscv64` | `riscv64` |
| `mipsel` | `mipsel` | | `loongarch64` | `loongarch64` |
| `mips64` | `mips64eb` | | `i386`, `riscv32` | none |
| `mips64el` | `mips64el` | | | |

**The engine** is a `tools/dev/dev.sh` build: `--build <dir>` (default `<repo>/.dev`, or
`$PANDA_DEV_DIR`). `PANDA_PYTHON`, `PANDA_LIB`, `PANDA_PLUGIN_DIR`, `PANDA_ENGINE_SRC` (the patched
QEMU tree) and `PANDA_ENGINE_PREFIX` (its installed headers) override it, e.g. for a
`nix build .#python` result. pytest runs from `$PANDATEST_PYTHON`, else `$PANDA_PYTHON` or the
build's Python if it has pytest (the dev shell's Python, and so `.dev/bin/python3`, has it), else
`python3` on `PATH`; the tests themselves run with the
engine's Python. An item whose engine, plugin, host tool or fixture is missing is skipped, with
the reason.

**Running.** Static and harness items run at once (`--jobs`, default 8). Guest items run one at
a time, each after a resource guard: at least 45 GB free on `/home` (`PANDATEST_MIN_FREE_GB`),
8 GB `MemAvailable` (`PANDATEST_MIN_MEM_GB`), no build holding the build directory's lock or
`$PANDATEST_BUILD_LOCK`, and no `ninja` or `meson` process. While the guard fails, the item waits,
re-checking every 30 s, for up to `$PANDATEST_GUARD_WAIT` seconds (default 1800); if it still
fails, the item is skipped with the reason, never passed (`--no-wait`: skipped at once). Every item runs pinned with `taskset -c` to `--cpus`
(default `$PANDATEST_CPUS`, or `64-79` on a host with at least 80 CPUs) and under `timeout`. Each
gets a work directory and a private `TMPDIR`; busybox and cloud images are copied into it, as
`run.sh` does.

**Results** go to `--out` (default `$TMPDIR/panda-tests-<time>`): `junit.xml`, `results.json`
(per item: its id, outcome, reason, duration, log, the fixture's name and identity, the item's
hash, and the engine build's git revision and whether its tree had uncommitted changes),
`logs/<item>.log`, and the work directories of the items that failed (`--keep`: all of them).

**Expected failures** (`xfail`) run, and report XFAIL while they fail; an entry that passes
reports XPASS, which fails the run, so stale entries come out.

### Metadata

A test file declares a literal dict (read with `ast`: collecting imports nothing):

```python
PANDA_TEST = dict(
    area="syscalls",                    # what it covers: selected by name
    tier="guest",                       # static | harness | guest | stress | bench
    arches=["x86_64", "aarch64"],       # one item per arch (none: one item)
    fixture="testmachine",              # none | busybox | testmachine[-smpN] | cloud-<distro>
    plugins=["syscalls"],               # checked in the plugin directory ("test:<name>": built by the test)
    variants=dict(smp=[1, 4], intel=[False, True]),   # one item per combination (--variants all)
    xfail=[dict(when=dict(arch="aarch64", smp=4), reason="...", issue="#NN")],
    always=False,                       # run on every change
    args=["{fixture}"],                 # the argv after the script (this is the default with a fixture)
    env={"SCTRACE_OBSERVER": "syscalls"},
    needs=["cc", "glib"],               # cc, c++, glib, pandare2, all-targets, py:<module>, a program
    timeout=1800,                       # seconds (default: 1800 guest, 900 static and harness)
    nix_check="layout",                 # the flake check that runs this file
)
```

`args` and `env` may name `{fixture}` (the image json the test takes), `{arch}`, `{panda_arch}`,
`{work}` (the item's work directory), `{repo}`, `{engine_src}`, `{engine_prefix}` and
`{plugin_dir}`. A shell script puts the dict in a comment (`# PANDA_TEST = dict(...`), and a
module that is not a test says `PANDA_TEST = None`. Files without metadata are listed as "no
metadata: not run by tests/run".

The variants are `smp` (`--smp N`, and the machine `testmachine-<arch>-smpN`), `intel` (x86_64
only: `--intel`, `testmachine-<arch>-intel`), `form` (`SCOBSERVE_LEAN`: `call`, `lean`, `entry`)
and `backend` (`PANDA_SYSCALLS_BACKEND`); `pandatest/meta.py` defines them, each with its default
value (`smp` 1, `intel` False, `form` `call`, `backend` `trap`, the syscalls plugin's own default).
A test can give a variant as `dict(values=[...], default=..., arches=[...])`: its own default, and
the only arches it applies to.
An item's id is its test, arch and variants: `testmachine[x86_64,intel=1,smp=4]`.

`static/flake_checks.py` (the flake check `flake-checks`) checks that every flake check that runs
a test script is the `nix_check` that script's metadata names, and the other way round.

### Fixtures

| name | what | where |
|---|---|---|
| `busybox-<arch>` | a `mkbusybox.py` image | `$PANDATEST_BUSYBOX`, else `<cache>/busybox/<recipe>/`: `busybox-<mkbusybox arch>.json` |
| `testmachine-<arch>[-smpN][-intel]` | a test machine on `busybox-<arch>` | `$TESTMACHINE_CACHE`, else `<cache>/machines` |
| `cloud-<distro>-<arch>` | a `pandare2.mkimage` image | `$PANDATEST_IMAGES`, else `<cache>/images/<distro>-<arch>.json` |

`<cache>` is `$PANDATEST_CACHE`, else `~/.cache/panda-tests`. A missing fixture skips its items
unless `--build-fixtures` is given; then it is built before the first guest item that needs it
(a busybox image needs `tests/busybox.sh`'s downloads, in `$PANDA_BUSYBOX_DL` or
`~/.cache/panda-busybox`). Each fixture has an identity: for an image, a hash of its json and the
size and modification time of the files it names; for a machine, its directory's name, whose key
hashes everything it is built from.

## Running the suite with run.sh

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
