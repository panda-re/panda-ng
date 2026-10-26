# Getting started

This page gets you a Python with pandare2, builds a guest image, and runs a first analysis on it:
which process opens a file, what it has mapped, and what else is running.

## Get pandare2

pandare2 is the Python package that drives the engine. Every way of getting it bundles the
engine (QEMU with PANDA) and the C plugins.

With nix, run a script with the flake's Python:

```sh
nix run github:panda-re/panda-ng#python -- script.py
```

From a checkout, build that Python once and keep it, as the tests do:

```sh
nix build .#python -o result-python
export PANDA_PYTHON=$PWD/result-python/bin/python3
```

Without nix, install the wheel into a venv. It needs glibc 2.38 or newer (Debian 13, Ubuntu
24.04 and later) and nothing else from the host. `nix build .#wheel` builds it.

```sh
python3 -m venv venv
venv/bin/pip install pandare2-*-manylinux_2_38_$(uname -m).whl
```

The [README](https://github.com/panda-re/panda-ng/blob/main/README.md) covers the wheel, the
Debian package and using the flake from your own flake.

## Build a guest image

Analyses that need a running Linux guest use an image built by `pandare2.mkimage`: a
distribution's cloud image with a snapshot named `root` taken at a logged-in shell on the serial
console, and the guest kernel's symbols.

```sh
$PANDA_PYTHON -m pandare2.mkimage debian-13-x86_64 -o ~/panda-images
```

It writes `debian-13-x86_64.qcow2`, `debian-13-x86_64.json` (what `Panda` reads), and the
kernel's symbols as `debian-13-x86_64.isf.json.xz` (Volatility ISF) and
`debian-13-x86_64.kernelinfo.conf` (for osi_linux). The other recipes are `debian-13-aarch64`,
`debian-13-riscv64`, `ubuntu-24.04-x86_64` and `ubuntu-24.04-aarch64`; `debian-13-ppc64` is a
recipe too, but the test suite isn't run on it yet.

Building takes one boot under TCG plus symbol extraction: 5-6 minutes for a debian-13 image on a
2026 x86_64 server, 11-21 minutes for Ubuntu. Downloads are cached in `~/.cache/panda-images`.

Build images with the PANDA that will run them. The `root` snapshot is taken by that engine, with
the arguments pandare2 restores it with; another build may not load it.

## A first analysis

This is `tests/osi_isf.py`, which the test suite runs on every image. It starts the guest, runs
`cat /etc/os-release` on its console, and when `cat` opens the file, asks the kernel's data
structures who is running.

First it creates the `Panda` object from the image description and loads two plugins: osi, the
introspection interface, and osi_isf, which implements it from the kernel's ISF symbols.

```python
--8<-- "tests/osi_isf.py:setup"
```

Then the callbacks. `@panda.ppp("syscalls2", "on_sys_openat_enter")` runs on every `openat`
system call, with its decoded arguments (it loads syscalls2 if it isn't loaded). The callback runs
while the guest is stopped inside the system call, so it can read guest memory (`read_str`) and
ask OSI for the current process, its mappings, the process list and the kernel modules.

```python
--8<-- "tests/osi_isf.py:callbacks"
```

Last, the function that drives the guest. `@panda.queue_blocking` runs it in its own thread once
the guest starts. It reverts to the `root` snapshot, types commands on the serial console and
waits for their output, then ends the analysis. `panda.run()` runs the guest until then.

```python
--8<-- "tests/osi_isf.py:drive"
```

Run it on the image:

```sh
$PANDA_PYTHON tests/osi_isf.py ~/panda-images/debian-13-x86_64.json
```

It prints the process (`cat`, with the PID the shell printed), its mappings and the kernel
modules, then a PASS or FAIL line for each check.

## Running the test suite

`tests/run.sh` runs the guest tests on one or more images, in parallel, each test on its own copy
of the image (the tests revert and write snapshots, so the shared images are never written):

```sh
tests/run.sh ~/panda-images                      # every image in the directory, the default tests
TESTS="osi_isf rr" tests/run.sh ~/panda-images/debian-13-x86_64.json
```

Results go to `$OUT/results`, one line per run, and each run's log to `$OUT/<image>/<test>.log`.
The [tests README](https://github.com/panda-re/panda-ng/blob/main/tests/README.md) lists every
test and what it needs.

## Next

- [The Python API](python.md): the `Panda` object, its threads, callbacks, memory and registers.
- [Plugins](plugins.md): the C plugins, and building your own.
- [Record and replay](record-replay.md), [snapshots and fast restore](snapshots.md).
- [Features](../features.md): what works, on which accelerators, and where it is tested.
