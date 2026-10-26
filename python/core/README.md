# pandare2

pandare2 is the Python interface to panda-ng, PANDA on QEMU 11.1. A script creates a `Panda`
(a virtual machine), registers callbacks on the guest's execution, loads C plugins, drives the
guest through its serial console, and records and replays executions.

Source, issues and the full documentation: https://github.com/panda-re/panda-ng

## Install

The wheel, `pandare2-<version>-py3-none-manylinux_2_38_<arch>.whl` (x86_64 and aarch64), is
attached to each panda-ng release. It needs glibc 2.38 or newer (Debian 13, Ubuntu 24.04,
Fedora 39, RHEL 10 and later) and carries everything else: the engine
(`libpanda-<arch>-softmmu.so`), the plugins, QEMU's firmware, and `qemu-system-<arch>` for each
target.

```sh
python3 -m venv venv
venv/bin/pip install pandare2-*-manylinux_2_38_$(uname -m).whl
```

With nix, in a checkout of the repository, `nix build .#wheel` builds the wheel and
`nix build .#python` builds a Python with pandare2; the flake's overlay also adds `pandare2` to
every nixpkgs Python package set. The repository's README has both in detail.

## A first script

This boots the x86_64 firmware with no guest disk, loads the `syscalls2` plugin, and stops after
20000 basic blocks (from the repository's `tests/smoke.py`):

```python
from pandare2 import Panda

panda = Panda(arch="x86_64", mem="256M", os_version="linux-64-generic",
              extra_args=["-display", "none", "-nodefaults"])
panda.load_plugin("syscalls2")
blocks = 0

@panda.cb_before_block_exec
def before_block(cpu, tb):
    global blocks
    blocks += 1
    if blocks == 20000:
        panda.end_analysis()

panda.run()
print(f"{blocks} blocks")
```

## A guest

Analysis of a real guest needs an image with a snapshot. `pandare2.mkimage` builds one from a
distribution's cloud image, with the guest kernel's symbols for the OS introspection plugins. Build
images with the pandare2 that will run them: the snapshot is taken by that engine. From the wheel,
it needs `gdb` and `dwarf2json` on `PATH`; the nix build brings its own.

```sh
venv/bin/python -m pandare2.mkimage debian-13-x86_64 -o ~/panda-images
```

```python
import os
from pandare2 import Panda

panda = Panda(generic=os.path.expanduser("~/panda-images/debian-13-x86_64.json"))

@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    print(panda.run_serial_cmd("uname -a"))
    panda.end_analysis()

panda.run()
```

## More

In the repository:

- `tests/`: each test is a working script with a docstring that says what it does; `tests/README.md`
  says how to build the images and run them.
- `python/examples/`: short example scripts.
- `plugins/<name>/README.md`: the C plugins and their arguments (`panda.load_plugin(name, args)`).
