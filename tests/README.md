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
| smoke | `smoke.py <arch>` | nothing: boots the firmware, loads C plugins (CI runs it on every PR) |
| guest suite | `guest api osi_isf blocks callbacks calls mem late reset teardown asidstory tbpc hookalias rr rrcb rrstop rrload fuzz fastrestore` | an image (below); `run.sh` runs them |
| more guest tests | `kpti physmem rr_blocks fastrestore_guards fastrestore_hotplug fastrestore_vsock` | an image; not in `run.sh`'s default list (x86_64-only, or need extra host tools: see each docstring) |
| benchmark | `bench_cb.py <image>` | an image, and `PANDA_PLUGIN_DIR` from `nix build .#test-plugins` (the `tests/plugins/cbcount` C baseline): ns per callback in C, bare cffi and `@panda.cb_*` |
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
