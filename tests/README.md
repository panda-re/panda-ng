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
| static | `static/events_registry.py` | Python, C compiler and GLib, no engine or guest: the event registry (`events.c`) against a model under ASan/UBSan: binding, pending, mismatch, refusal, dispatch order, instance data, unload, RCU, UUIDv5 vectors (`nix build .#checks.x86_64-linux.events-registry`) |
| static | `static/lifecycle.py` | Python, C compiler and GLib, no engine or guest: transient cancellation/reset-hook lifetime and fast-restore notification (`nix build .#checks.x86_64-linux.lifecycle`) |
| static | `static/replay_bh.py <patched replay/replay-events.c>` | Python and a C compiler, no engine or guest: pending BH coalescing and reentrant event drains (`nix build .#checks.x86_64-linux.replay-bh`) |
| static | `static/replay_native.py` | Python and a C compiler, no engine or guest: extracted native mutation guards and Penguin continuations (`nix build .#checks.x86_64-linux.replay-native`) |
| static | `static/guest_layout.py <engine>/include/panda/guest-layout` | Python, no guest: the guest-write layout map the engine build generated from its debug info agrees with the committed bindings (`bindings_layout.py`) on every arch (`nix build .#checks.x86_64-linux.guest-layout`) |
| static | `static/guest_view.py` | Python with cffi, no engine or guest: pandare2's env view turns field assignments into the engine's env writes, through pointers by path (`nix build .#checks.x86_64-linux.guest-view`) |
| static | `static/replay_policy.py` | Python only, no engine or guest: RR admission and observe-only wrapper policy (`nix build .#checks.x86_64-linux.replay-policy`) |
| static | `bash static/syscalls_dispatch.sh <output binary>` | a C++ compiler, no engine or guest: generated syscalls2 dispatch and PPP subscriptions (`nix build .#checks.x86_64-linux.syscalls2-dispatch`) |
| smoke | `smoke.py <arch>` | nothing: boots the firmware, loads C plugins (CI runs it on every PR) |
| guest suite | `guest api osi_isf blocks block_activation callbacks calls mem mmio late reset teardown asidstory tbpc hookalias rr rrcb rrstop rrload fuzz fastrestore fastcov syscalls taint taint_mmio taint_errors` | an image (below); `run.sh` runs them |
| every arch | `busybox.sh [arch...]` | nix and network: builds `mkbusybox.py` images (a linux_builder kernel, a busybox initramfs, an ext2 disk) for 12 arches and runs `taint taint_mmio taint_errors` on them through `run.sh`, about 3 minutes. CI runs it on PRs that touch taint or record/replay, and nightly |
| more guest tests | `kpti physmem rr_blocks fastrestore_guards fastrestore_hotplug fastrestore_vsock` | an image; not in `run.sh`'s default list (x86_64-only, or need extra host tools: see each docstring) |
| test machines | `testmachine.py <busybox json> [--smp N] [--intel]` | a `mkbusybox.py` image: builds and checks its test machine (below) |
| syscall trap | `syscall_cover.py <busybox json> [--intel]`, `syscall_truth.py <busybox json> [--smp N] [--intel]` | a `mkbusybox.py` image (x86_64, aarch64, armel) and `PANDA_PLUGIN_DIR` from `nix build .#test-plugins` (the `tests/plugins/sctrace` observer): every number through every entry instruction and the hard return paths (cover); the trap's events against the guest kernel's `raw_syscalls` ftrace (truth). 2-10 s each on a test machine; `TESTS="testmachine syscall_cover syscall_truth" tests/busybox.sh x86_64 aarch64 armel` |
| reset/restore lifecycle | `lifecycle.py <image>` | real public owner reset-hook ordering, ordinary/fast restore notifications, rejection, unregister/re-register and hardware reset; `RRCB_HANDOFF=1` only for fixtures with handoff snapshot extensions; pending/held/injected slot and safe teardown cases are host-only in `static/lifecycle.py` and `static/penguin_detach.py` |
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

Machines exist for x86_64, aarch64 and arm (busybox-armel). The syscall tests run on them:

```sh
nix build .#python -o result-python && nix build .#test-plugins -o result-plugins
export PANDA_PYTHON=$PWD/result-python/bin/python3 PANDA_PLUGIN_DIR=$PWD/result-plugins/lib/panda/plugins
DL=~/.cache/panda-busybox      # busybox.sh's downloads (it fetches them on its first run)
$PANDA_PYTHON tests/mkbusybox.py x86_64 aarch64 armel -o ~/panda-busybox --kernels $DL/kernels --busybox $DL/busybox
$PANDA_PYTHON tests/syscall_cover.py ~/panda-busybox/busybox-x86_64.json
$PANDA_PYTHON tests/syscall_truth.py ~/panda-busybox/busybox-aarch64.json --smp 4
```

They observe the trap through `tests/sctrace.py`, a QEMU plugin on the plugin syscall API
(`qemu_plugin_register_syscall_event_cb`); swapping that module for another observer moves them
to it. Known trap bugs are listed in each test as gaps (`testlib.Checks`): GAP while they fail,
XPASS (a failure) once fixed.

### Adding an arch

The machine and both tests are parametrised by arch; an arch supplies:

1. **An image**: `mkbusybox.py <arch>` (all 12 of its arches have one).
2. **The system layer** in `programs/sys.h`, under the compiler's arch macro:
   - `NR_<name>` for the calls the programs make (riscv64 and loongarch64 use the asm-generic
     numbers aarch64 has; mips o32 numbers start at 4000);
   - `sc(n, a, b, c, d, e, f)`: a system call returning `-errno` on error (mips: the error is a
     positive value with `a3` set, and o32 arguments 5 and 6 go on the user stack; ppc `sc`: the
     error is positive with `cr0.SO` set; both must be turned into `-errno` here);
   - `hc(op, a, b, c)`: penguin's hypercall instruction with the magic in the number register and
     in argument 0, op, a, b and c in arguments 1-4, the host's answer in the return register.
     The instructions the engine decodes (engine patches 0003-0008): x86 `out %eax, $0x88`
     (after `ioperm`), aarch64 `.inst 0xd500501f`, arm `mcr p7, 0, r0, c0, c0, 0` (number in
     r7), riscv `xori x0, x0, 0` (number in a7, a0-a4), loongarch `cpucfg $zero, $zero` (number
     in a7, a0-a4), ppc `xori r10, r10, 0` (number in r0, r3-r7), mips `movz $0, $0, $0`
     (number in v0, but only a0-a3: the agent's REPLY passes its status in argument 4, so on mips
     the status has to move into the reply buffer);
   - `hc_setup()`, `clone_fn(flags, stack, ctid, fn)` (mind the arch's clone argument order:
     x86_64 takes the child tid before tls, arm and arm64 after it: the kernel's
     `CONFIG_CLONE_BACKWARDS`), `SIGRETURN_ASM` (end it with a
     trapping instruction, so no handler starts just past the call) and `_start`;
   - in `programs/scpaths.c`, `vfork_exit` and `clone3_exit` (the child exits without touching
     memory).
3. **The build**: the arch in `programs/build.sh`'s lists for `agent`, `scpaths` and `scwork` (its
   nixpkgs `pkgsCross` name and flags), then `sh tests/programs/build.sh agent scpaths scwork`.
4. **The machine**: its pandare2 arch in `testmachine.AGENT_ARCH`.
5. **The observer**: the target in the flake's `sctrace` package (`targets`).
6. **The tests**: in `syscall_cover.py`, a `PROGRAMS` entry and the scpaths numbers in `NR` (and,
   for the every-number sweep, a `programs/sccover_<arch>.S` with the arch's `AUDIT_ARCH_*` in
   its seccomp filter, its trap instruction in `sites()` and its calls in `expected()`); in
   `syscall_truth.py`, a `WORKLOADS` entry and the ABI's rows in `NO_EXIT`, `SIGRETURN`,
   `EXECVE`, `VFORK`, `CLONE`, `ABI_GROUP` and `mask_of`. A 32-bit ABI on a 64-bit kernel (i386
   on x86_64, arm on aarch64) runs the 32-bit build on the 64-bit machine.
7. **The engine**: the trap's target hooks for the arch (`system/syscall-trap.c`'s
   `syscall_trap_enter` and `syscall_trap_return`). The engine has them for x86 and arm only so
   far; on another arch the machine works but the trap reports nothing.

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
