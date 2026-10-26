# The Python API

pandare2 runs the engine inside your Python process. This page explains how a pandare2 program
is put together: the `Panda` object, where your code runs, driving the guest, reading its memory
and registers, and callbacks. The [Python API reference](../reference/python.md) has every method.

## The Panda object

A `Panda` object owns one engine instance, and a process can have only one. Programs that need
several runs, such as a recording and its replays, start a child process for each
(`tests/rr.py` does).

Most programs start from an image built by `pandare2.mkimage` ([Getting started](getting-started.md)):

```python
--8<-- "tests/osi_isf.py:setup"
```

`generic=` takes the image's `.json` and sets the architecture, memory size, disk, serial-console
prompt and machine arguments from it. Without an image, name the architecture and pass the
machine yourself, as `tests/smoke.py` does on the [overview](../index.md) page:
`Panda(arch=..., mem=..., extra_args=[...])`.

The arguments you will use most:

| argument | what it does |
|---|---|
| `generic` | an image `.json` from `pandare2.mkimage` |
| `arch`, `mem` | the target and RAM size when there is no image |
| `extra_args` | more engine arguments, a list or a string (`["-icount", "shift=6,sleep=off"]`) |
| `rr` | make the VM recordable ([Record and replay](record-replay.md)); pass it to replays too |
| `machine`, `modules` | a `MachineBuilder` machine or panda modules ([Machines](machines.md)) |
| `plugin_path` | where C plugins are looked for (also `$PANDA_PLUGIN_DIR`) |
| `catch_exceptions` | default `True`: an exception in a callback ends the run and `run()` raises it |

`panda.arch_name` is the target, `panda.arch` has the register helpers, and `panda.libpanda` is
the engine's C API through cffi (`panda.ffi`), for what has no Python wrapper yet.

## run() and where your code runs

`panda.run()` starts the guest and returns when the analysis ends: you call `end_analysis()`, the
guest shuts down, or a replay reaches its end. On the way out it unloads the plugins and deletes
the callbacks.

While it runs, your code runs in one of three places:

| | runs on | the guest | use it for |
|---|---|---|---|
| callbacks (`@panda.cb_*`, `@panda.ppp`, `@panda.hypercall`, `@panda.hook`) | the vCPU thread | stopped until you return | reading and writing guest state, turning callbacks on and off |
| blocking functions (`@panda.queue_blocking`) | a thread of their own | running | driving the guest: `run_serial_cmd`, `revert_sync`, `run_monitor_cmd`, `record_cmd` |
| main-loop functions (`panda.queue_main_loop_wait_fn(fn, args)`) | QEMU's main loop | between vCPU runs | snapshot loads and saves, fast restore, anything that needs the VM stopped |

Blocking functions wait for the guest, so never call one from a callback: the guest can't run
until the callback returns. A callback that needs a blocking step queues it instead, as the fuzz
loop in [Snapshots](snapshots.md) does with `vm_stop`, `queue_main_loop_wait_fn` and
`panda_cont`.

Blocking functions run in the order they were queued. This one, from `tests/hookalias.py`, sets
up two guest processes, reads their state over the console, registers a hook at an address it
learned from the guest, and lets the processes run into it:

```python
--8<-- "tests/hookalias.py:drive"
```

`end_analysis()` stops the run from any thread. `stop_run()` stops it without unloading plugins
first.

## Driving the guest

These need a guest with a shell on its serial console and a prompt pandare2 knows, which the
mkimage images have. Call them from a blocking function.

- `run_serial_cmd(cmd, timeout=None)` types a command, waits for the next prompt, and returns
  what the command printed. The default timeout is 30 seconds.
- `revert_sync(name)` loads a snapshot (`loadvm`) and returns once it has loaded. It raises
  `ValueError` if the image has no such snapshot, and returns QEMU's message on other errors.
- `run_monitor_cmd(cmd)` runs a QEMU monitor command (`savevm`, `info snapshots`, ...) and
  returns its output.

## Memory and registers

Inside a callback the vCPU is stopped, so guest memory and registers are consistent. This is
`tests/api.py`, in its hypercall callback, reading and writing a buffer its guest program passed
in (`marker`), and a kernel symbol's address (`banner_addr`):

```python
--8<-- "tests/api.py:memory"
```

- `virtual_memory_read(cpu, addr, length)` and `virtual_memory_write(cpu, addr, data)` go through
  the guest's page tables; `fmt="int"` returns an integer instead of bytes. They raise
  `ValueError` when the address isn't mapped.
- `physical_memory_read(addr, length)` and `physical_memory_write(addr, data)` take guest
  physical addresses; `virt_to_phys(cpu, addr)` translates one.
- `read_str(cpu, addr)` reads a C string.

Registers are in `panda.arch`: `get_reg(cpu, name)` and `set_reg(cpu, name, value)`, `get_pc`
and `set_pc`, and `get_arg(cpu, n, convention=...)`, which reads the n-th argument of a call
(`"syscall"` for system calls and hypercalls). `cpu_env(cpu)` is the whole CPU state, for
registers without a name. This bare-metal test sets the pc before the CPU starts, and reads a
hypercall's argument:

```python
--8<-- "tests/machine_sw.py:setup"
```

```python
--8<-- "tests/machine_sw.py:hypercall"
```

In a callback in the middle of a block, `get_pc` returns the block's start, not the current
instruction's address. `enable_precise_pc()` doesn't change that yet (`tests/api.py` lists it
as a known gap).

## Callbacks

Every PANDA callback has a decorator, `@panda.cb_<name>`; the
[callback reference](../reference/callbacks.md) lists them, with their arguments and whether
each fires in this engine. A decorator registers the function and returns it unchanged. Callbacks need TCG.

An exception in a callback can't propagate into the engine: with `catch_exceptions` (the default)
pandare2 keeps it, ends the analysis and `run()` raises it; until then the callbacks return
without calling your functions.

A callback has a name (the function's, or `name=`) and can start disabled. `insn_exec` runs only
for instructions whose `insn_translate` returned true, so this pair, from `tests/blocks.py`,
instruments only the test program's code:

```python
--8<-- "tests/blocks.py:insn"
```

`panda.enable_callback("it")` and `panda.disable_callback("it")` turn one on and off by name, from
any thread, including from inside another callback. `delete_callback(name)` removes it.

A decorator is a function, so callbacks can also be registered while the guest runs. Here
(`tests/late.py`) a hypercall from the guest registers instruction and memory callbacks on code
that was already translated, and deletes them at the next hypercall; they still see that code
run:

```python
--8<-- "tests/late.py:late"
```

Some callbacks return a value that changes what the guest does (`insn_translate` returns whether
to instrument, `before_handle_exception` the exception to raise); the reference gives each
signature.

The other kinds of callback:

- `@panda.ppp(plugin, name)` registers with a C plugin's callback, such as syscalls2's
  `on_sys_openat_enter` or osi's `on_task_change`. See [Plugins](plugins.md).
- `@panda.hypercall(magic)` runs when the guest executes the hypercall instruction with that
  magic number as its first system-call argument (through the hypercaller plugin); read the rest
  with `panda.arch.get_arg(cpu, n, convention="syscall")`.
- `@panda.hook(addr, kernel=False)` runs when the guest starts a block at `addr` (through the
  hooks plugin).

Memory callbacks (`virt_mem_after_read` and the rest) work once registered. A registered memory
callback instruments every load or store even while it is disabled, so register them when you
need them, as `tests/late.py` does. `enable_memcb()` and `disable_memcb()` are accepted but have
no effect in this engine.

## PyPlugin

A `PyPlugin` is a class that groups an analysis's callbacks and state so it can be loaded into
any `Panda`: subclass it, register callbacks in `__init__(self, panda)`, read arguments with
`self.get_arg(name)`, and load it with `panda.pyplugins.load(MyPlugin, {"arg": "value"})`. Methods
marked `@PyPlugin.ppp_export` can be called by other PyPlugins through `self.ppp.<Class>`.

No test in this repository loads a PyPlugin yet, so there is no example here; see the
[reference](../reference/python.md#pandare2.pyplugin.PyPlugin).
