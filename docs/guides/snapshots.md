# Snapshots and fast restore

There are two ways to put a guest back to an earlier state. QEMU snapshots save the whole VM
(RAM, devices and disk) into the qcow2 image, and last. Fast restore keeps RAM and device state
in host memory and copies back only the pages written since, for loops that restore thousands
of times, such as fuzzing.

## QEMU snapshots

`revert_sync(name)` loads a snapshot from the image; every analysis on an mkimage image starts
with `revert_sync("root")`. Save one with `panda.snap(name)` or the monitor's `savevm`, and
delete one with `delvm`. From `tests/api.py`:

```python
--8<-- "tests/api.py:snapshots"
```

`panda.snap(name)` stops the VM, saves, and continues. Called from a blocking function it returns
once the snapshot is taken; called from a callback it only queues the save for the main loop,
which is how the fuzz loop below takes its snapshot at a system call. `@panda.cb_after_loadvm`
runs after every load, before guest code runs.

Snapshots are internal to the qcow2 file, so they travel with it, including between hosts with
different page sizes (an arm64 host with 16K pages and an x86_64 host with 4K pages). To keep the
image unchanged, work on a copy, as `tests/run.sh` does.

A snapshot only loads into the same machine: the same RAM size, devices and CPU model. pandare2
restores an mkimage image with the arguments its `.json` gives, which is why images should be
built with the engine that runs them.

`Panda.delvm(name)` stops the VM and doesn't start it again; use the monitor's `delvm`, as above.

## Fast restore

`panda_fast_snap()` copies RAM and device state into host memory and turns on dirty-page
tracking. `panda_fast_restore()` puts them back, copying only the pages written since the
snapshot or the last restore, and leaves the VM stopped. There is no pandare2 wrapper yet: call
them through `panda.libpanda`, from the main loop with the VM stopped.

`tests/fuzz.py` is a fuzzing loop. When `cat` enters `openat` for `/etc/os-release`, it takes the
snapshot: stop the VM, queue the snapshot and a continue on the main loop. The `--fast` option
chooses fast restore over a qcow2 snapshot.

```python
--8<-- "tests/fuzz.py:snap"
```

Each iteration stops the VM, restores, writes the next input over the path in `cat`'s memory, and
continues. Two more syscalls2 callbacks (a `read` on fd 3, a `write` on fd 2) see whether `cat`
found the file, record the outcome and call `next_input` again.

```python
--8<-- "tests/fuzz.py:loop"
```

Run it with fast restore:

```sh
$PANDA_PYTHON tests/fuzz.py ~/panda-images/debian-13-x86_64.json 100 --fast
```

It reports iterations per second, and the counters from `panda_fast_info()`. `tests/run.sh` runs
it without `--fast` (qcow2 snapshots); `tests/fastrestore.py`, also in the default suite, checks
fast restore itself.

The other calls: `panda_fast_delete()` frees the snapshot and stops dirty tracking;
`panda_fast_verify()` compares all RAM with the snapshot and returns the number of pages that
differ (0 right after a restore; a test aid, it reads all RAM); `panda_fast_info()` returns
counters as `key=value` text, such as `restores`, `full_restores`, `last_pages` and `last_mode`.
`engine/src/include/panda/panda_api.h` documents each.

### Limits

- TCG only, and not while recording or replaying: both calls return -1 there (under
  `accel="kvm"` too).
- The disk is not restored. A guest that writes to disk between restores sees those writes.
- After anything that rewrites RAM without dirty tracking (a `loadvm`) or reads the dirty bitmap
  (a `savevm` or a migration), the next restore copies all RAM, still correctly.
- With a vhost device that can't log dirty pages (a vhost-user backend without a shared log), each
  restore compares every page instead: still exact, but it reads all RAM.
- Writes through host pointers from `panda_map_virt_to_host` aren't tracked.
- If RAM blocks change after the snapshot (a device with its own RAM, such as a NIC's option
  ROM, is plugged or unplugged), restore refuses (-1); take a new snapshot.

The tests are `tests/fastrestore.py`, `fastrestore_guards.py` (the refusals),
`fastrestore_hotplug.py`, `fastrestore_vsock.py` and `fuzz.py`.
