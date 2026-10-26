# Plugins

PANDA plugins are shared libraries that register callbacks with the engine and can offer an API
and callbacks of their own to other plugins and to Python. Use one when an analysis is too hot
for Python (a callback on every block or instruction), or to reuse what panda-ng ships: system
call decoding, OS introspection, hooks and hypercalls.

## The shipped plugins

Every build ships these, for every target (`plugins/config.panda`):

| plugin | what it does |
|---|---|
| [syscalls2](https://github.com/panda-re/panda-ng/blob/main/plugins/syscalls2/README.md) | a callback on every system call's entry and return, with decoded arguments |
| [osi](https://github.com/panda-re/panda-ng/blob/main/plugins/osi/README.md) | the OS introspection interface: current process and thread, process list, mappings, modules |
| [osi_linux](https://github.com/panda-re/panda-ng/blob/main/plugins/osi_linux/README.md) | osi for Linux, from a kernelinfo file (`kconf_file`) |
| osi_isf | osi for Linux, from a Volatility ISF file (`isf`) |
| [proc_start_linux](https://github.com/panda-re/panda-ng/blob/main/plugins/proc_start_linux/README.md) | a callback with each new process's auxiliary vector (argv, environment, entry point) |
| [hooks](https://github.com/panda-re/panda-ng/blob/main/plugins/hooks/README.md) | a callback when execution reaches an address (`@panda.hook`) |
| [hypercaller](https://github.com/panda-re/panda-ng/blob/main/plugins/hypercaller/README.md) | dispatches guest hypercalls by magic number (`@panda.hypercall`) |
| [hypersyscalls](https://github.com/panda-re/panda-ng/blob/main/plugins/hypersyscalls/README.md) | system calls reported by an instrumented guest kernel through hypercalls |
| [hw_proc_id](https://github.com/panda-re/panda-ng/blob/main/plugins/hw_proc_id/README.md) | one process identifier on every architecture |
| cskeleton, cppskeleton, rust_skeleton | starting points for a C, C++ or Rust plugin |

The mkimage images carry the files osi_linux and osi_isf need.

## Loading a plugin

`panda.load_plugin(name, args)` loads a plugin by name, with its arguments as a dictionary of
strings. Load plugins before `panda.run()`, or from a callback or blocking function while the
guest runs. `tests/guest.py` loads three:

```python
--8<-- "tests/guest.py:plugins"
```

Each plugin's README lists its arguments; booleans are `"true"` or `"false"`. The engine has no
`-panda` command-line option as classic PANDA had: load PANDA plugins from Python, or from
another plugin.

pandare2 looks for plugins in `plugin_path=` if given, then `$PANDA_PLUGIN_DIR`, then the ones
bundled with it. `unload_plugin(name)` unloads one; `run()` unloads them all when it returns.

## Using a plugin from Python

`panda.plugins[name]` is the plugin's C API, loading the plugin first if it isn't loaded.
`@panda.ppp(plugin, callback)` registers a Python function with one of the plugin's callbacks,
and loads the plugin too. From `tests/guest.py`, a syscalls2 callback that reads the file name
`openat` was given, and a proc_start_linux callback that asks osi which process started:

```python
--8<-- "tests/guest.py:ppp"
```

The callback's arguments are the plugin's: syscalls2's are the CPU, the pc, then the system
call's arguments. syscalls2 picks its system call tables from the OS name pandare2 gives the
engine (the image's `os`, or `os_version=`). It has Linux tables for i386, x86_64, arm, aarch64,
mips, mips64, ppc, ppc64 and riscv64 (`plugins/syscalls2/generated-in/`).

## Writing a C plugin

A plugin is a C or C++ file with `init_plugin`, which registers its callbacks, and
`uninit_plugin`. Start from `plugins/cskeleton`. This one, from `examples/consumer`, counts
blocks and offers the count as a function:

```c
--8<-- "examples/consumer/plugin/blockcount.c"
```

`buildPandaPlugin` in the flake's `lib.<system>` compiles every `*.c` in a directory against the
engine's headers, for each target in `targets` (default x86_64, aarch64 and riscv64), into
`lib/panda/plugins/libpanda-<pname>_<arch>-softmmu.so`, the name `load_plugin` looks for. Its
other arguments are `buildInputs` and `extraCFlags`. From the consumer flake:

```nix
--8<-- "examples/consumer/flake.nix:plugin"
```

`pluginDirWith [ ... ]` makes a plugin directory holding panda-ng's plugins and yours, for
`$PANDA_PLUGIN_DIR`:

```nix
--8<-- "examples/consumer/flake.nix:script"
```

The script loads the plugin like any other, and calls its function through cffi on the same
library:

```python
--8<-- "examples/consumer/count.py:load"
```

A plugin built this way works with the engine it was built against. It also works with the
wheel's engine while both come from the same engine source revision.

To add a plugin to panda-ng itself, put it in `plugins/<name>/` with a `meson.build` (copy
cskeleton's) and add its name to `plugins/config.panda`. A plugin that offers callbacks to
others declares them in `<name>_int.h`; the build generates the `<name>_ext.h` that users
include.

## Rust plugins

`rust/panda-bindings` is the crate `panda`: bindgen output of `include/panda.h` for every
target, one Cargo feature per target. `plugins/rust_skeleton` is a plugin that uses it: an
`init_plugin` that registers a block callback. It is built for every target with the other
plugins and ships with them, so `panda.load_plugin("rust_skeleton")` finds it.

This is experimental. No test loads a Rust plugin yet; CI only checks that the bindings compile
for every target feature.
