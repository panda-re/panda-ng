# Machines and modules

Most analyses run on one of QEMU's machines (`-M virt`, `-M pc`, ...) with a guest image. When
the code you want to run expects other hardware, such as bare-metal code or a firmware image for
a board QEMU doesn't have, describe the machine yourself:

- `pandare2.MachineBuilder` lays out a machine in Python (CPU, memory map, devices, interrupt
  wiring, entry point) for the engine's built-in `configurable` machine;
- a **panda module** is a shared library with a board or devices of your own, compiled against
  the engine's SDK and loaded with `-M PATH:NAME` or `-module PATH`.

## MachineBuilder

`MachineBuilder(arch)` collects the machine, and `Panda(arch=..., machine=builder)` runs it.
This is `tests/machine.py`: a CPU, 16 MB of RAM, a ROM holding a file, an unimplemented
peripheral, a UART with its interrupt wired to the CPU, and an ELF to run.

```python
--8<-- "tests/machine.py:builder"
```

It lowers to `-M configurable,...` keyval arguments plus `-cpu` and `-kernel`, which the test
prints. The builder's methods:

| method | adds |
|---|---|
| `cpu(type)` | the CPU type, e.g. `"cortex-a53"`, `"rv64"` |
| `ram(base, size)` | RAM at `base` (a multiple of 8 KiB); without it there is no `-m` RAM |
| `region(name, base, size, kind, file=...)` | a region of kind `"ram"`, `"rom"` or `"unimp"` (reads 0, ignores writes), optionally loaded from a file |
| `rom`, `rom_file`, `ram_file`, `unimp` | shorthands for `region` |
| `alias(name, base, size, of)` | another region's contents at a second address |
| `device(type, mmio, id=, irq=[...], **props)` | a device by QOM type at its MMIO address(es); `irq` entries are `"cpu:N"`, `"<id>:N"` or `None`; other keywords are device properties |
| `entry(addr)`, `kernel(path)` | where the CPU starts; an ELF's entry is used by default |
| `board(path, name)`, `module(path)` | an external board, or a module to load (below) |

The builder raises `ValueError` before the engine starts for what it can check (overlapping or
duplicate regions, unknown ids, alias targets); the engine rejects the rest (unknown QOM types,
MMIO and IRQ counts out of range) with an error naming the element. `tests/machine.py` runs on
aarch64, arm, riscv64, mips, mipsel and ppc; `tests/machine_builder.py` checks the builder
without running the engine.

### Setting up memory from Python

The `configurable` machine also works with no memory map at all: give it a CPU, then map memory,
write code and set the pc from an `after_machine_init` callback, before the CPU runs.
`tests/machine_sw.py` does this:

```python
--8<-- "tests/machine_sw.py:panda"
```

```python
--8<-- "tests/machine_sw.py:setup"
```

`map_memory(name, size, address)` adds RAM to the guest's physical address space.

## Panda modules

A panda module is a shared library built with the engine's SDK. It can register machine types
(a board) or devices.

- `-M PATH:NAME` uses machine type `NAME` from the module at `PATH` (`-M PATH` alone when it has
  one). From pandare2: `MachineBuilder(arch).board(path, name)`.
- `-module PATH` loads a module before the machine is created, so its devices can be used with
  `-device` or in a `MachineBuilder`. From pandare2: `Panda(modules=[path])` or
  `MachineBuilder.module(path)`.

`tests/board.py` runs an external ARM board this way and checks the usual callbacks on it.

The engine checks a module before it loads it, and refuses with an error that says what to do:

- it must be built for the engine's target (an arm module can't load into `qemu-system-aarch64`);
- it must be built against the same engine ABI. `-module help` prints the engine's ABI hash.
  `PANDA_MODULE_ABI=ignore` loads a mismatched module anyway, with a warning (never one for
  another target);
- its QOM type names must not clash with the engine's or another module's.

The engine also warns about devices without vmstate, whose state snapshots can't save: those
from modules, and built-in ones the configurable machine creates.

### Building a module

The SDK is the engine's `sdk` output, with `bin/panda-board-cc`, which compiles and links a
module for one target. It compiles with the compiler and host headers the engine was built with,
so on nix run it in the engine's shell (`nix develop .#engine`). `engine/tests/modules/run.sh`
builds its example devices and runs one under a bare-metal guest program like this (the `as` and
`ld` lines need aarch64 binutils: an aarch64 host, or a cross binutils first on `PATH`):

```sh
nix build .#engine.sdk -o sdk
nix build .#qemu -o qemu
M=engine/tests/modules
nix develop .#engine -c sdk/bin/panda-board-cc aarch64-softmmu -o ext-devices.so $M/ext-devices.c
as $M/counter.S -o counter.o && ld -Ttext=0x40080000 -e _start counter.o -o counter.elf
qemu/bin/qemu-system-aarch64 -display none -monitor none -semihosting \
    -module ext-devices.so -M virt -cpu cortex-a53 -m 64M -device ext-counter,addr=0xb000000 \
    -kernel counter.elf -serial stdio
```

The guest prints `ID=434e5452 COUNT=00000003 SELF=4d4f4453`: it read the device's ID register and
its count of three writes.

In a flake, `mkBoard` in `lib.<system>` does the same, and checks that the result loads. The
flake builds `engine/tests/modules/ext-devices.c` (two example devices) this way as
`packages.<system>.board-example`:

```nix
--8<-- "flake.nix:board-example"
```

`mkBoard` takes `name`, `target` (e.g. `"arm-softmmu"`), `src` (a directory of `*.c`), and
optionally `engine` and `extraCflags`; the module is `lib/panda-boards/<name>.so`.

`engine/tests/modules/run.sh` exercises all of this on an aarch64 engine (`-device` from a
module, the ABI and target checks, type clashes, the vmstate warning), and `configurable.sh`
checks the configurable machine's error messages and building it over QMP with `-preconfig`.
The [tests README](https://github.com/panda-re/panda-ng/blob/main/tests/README.md#boards) says
how to run them.
