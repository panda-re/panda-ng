# Record and replay

A recording captures everything nondeterministic that reaches the guest (device input, disk
completions, interrupts, timer reads) from one point in a run. Replaying it executes exactly the
same instructions again, as often as you like, with compatible observe-only callbacks and plugins
attached. Record
cheaply, then run heavy analyses on the replay; or record something rare once and study it
repeatedly.

## Record

Make the VM recordable when you create it, with `rr=True`:

```python
--8<-- "tests/rr.py:panda"
```

That adds `-accel tcg,thread=single,rr-ready=on` (one vCPU thread, and devices set up so a
recording can begin at any point) and puts the disk behind QEMU's blkreplay driver so disk I/O
replays too (x86_64, i386, aarch64, riscv64, mips64 and mips64el). On other targets pandare2
warns that a replay may diverge on disk I/O. Runs that never record don't need `rr=True`.

`record_cmd` reverts to a snapshot (`snap_name`, default `"root"`; `None` to record from where the
guest is), begins recording, types the command, waits for it to finish, and ends the recording.
It raises `RuntimeError` if the command's output says it wasn't found or couldn't run, unless
`ignore_errors=True`. This is `tests/rr.py`'s recording process:

```python
--8<-- "tests/rr.py:record"
```

To record something other than one command, begin and end the recording yourself from a
blocking function. `record` and `end_record` are requests the main loop carries out; `wait_rr`
waits until it has (`tests/rrstop.py`):

```python
--8<-- "tests/rrstop.py:manual"
```

A recording named `dir/name` is two things: the log, `dir/name-rr-nondet.log`, and a snapshot
named `rr-name` saved in the guest's qcow2 image when the recording began. Keep both: a replay
loads that snapshot from the same image.

## Replay

Replay in a fresh process, with the same `Panda` arguments as the recording (`rr=True` included:
a replay needs the recording's command line). `run_replay` loads the start snapshot, runs to the
end of the log and returns. The guest's serial output during the replay is in
`panda.replay_serial`.

```python
--8<-- "tests/rr.py:replay"
```

Register callbacks and load plugins before `run_replay`, as for a live run. They see the same
execution the recording saw: `tests/rrcb.py` checks that block, instruction and memory callbacks,
`asid_changed` and the hypercalls fire at the same instruction counts in the recording and the
replay, and that a replay with no analysis callbacks reaches them at the same counts too.

A callback that should only count what happens during the recording or replay, not the setup
before it, checks `panda.libpanda.rr_on()`:

```python
--8<-- "tests/rr.py:openat"
```

`panda.rr_get_guest_instr_count()` is the instruction count, the replay's clock.

## Analysis access policy

Declare Python callbacks, PPP subscriptions and address hooks with `observe_only=True` for
recording and replay. Hypercall handlers are the exception: they may answer, and the answer
is recorded ([Guest writes from callbacks](#guest-writes-from-callbacks)). Core callbacks also need `rr` in their
compatible `modes` (the default includes it where supported). Entry, registration and
re-enabling reject incompatible callbacks; disabled core callbacks may remain registered.

Ordinary mutation APIs do not log effects. Guest memory, register and PC writes,
faulting accesses and device reads through the guarded analysis APIs are refused during
both recording and replay. Python guest-writing helpers also reject writes from an
observe-only callback during a live run. Instrumentation decisions such as requesting
instruction callbacks or retranslating a block remain available; observer return values
must not replace guest exception, address-space or hypercall results.

These declarations are a trusted-code contract, not a sandbox. Native plugins and Python
code using raw `CPUState` pointers or unguarded native APIs can still write guest state;
such writes are unsupported and can invalidate a recording. The separate experimental effect
API below is not an unsafe override of these guards.

### Experimental ordered RAM and register effects

`panda_analysis_ram_write(address, data, length)` copies a bounded physical RAM write into
the existing ND async queue. It returns **admission**, not completion. Admission requires
RECORD, the startup `rr` capability, main thread with BQL and replay mutex held, and all
vCPUs stopped. It is not callable as a synchronous vCPU callback mutation. Each request is
limited to 64 KiB; at most 64 requests may be pending.

At the existing deterministic scheduler async drain, before the next guest budget, the
engine checks the entire range and records an OK or ACCESS_DENIED result plus the request.
Only writable direct RAM is supported: MMIO, readonly, ROMD and RAM-device ranges are denied
without writing a RAM prefix. Successful writes use the standard address-space write path
for dirty tracking and translated-code invalidation. Replay loads and applies the event
without the original requester, verifies the same preflight result, and refuses effects
without the `rr` capability. Unsaved requests are canceled rather than applied at teardown.

`panda_analysis_reg_write(cpu_index, reg, data, length)` queues a register write with the same
admission, ordering and limits. `reg` is the CPU's gdbstub register number (the numbering
`gdb` and QEMU's own plugin register API use), so no PANDA architecture table is involved;
`length` must equal that register's gdbstub size, at most 256 bytes. The drain checks that
the vCPU and register exist and the size matches, then writes through the target's gdbstub
register setter and records whether the setter accepted it: some refuse (ARM system
registers, aarch64's pointer-authentication keys), and the effect is then ACCESS_DENIED and
changes nothing; the recording goes on. Replay makes the write only where the recording's did,
and stops with a divergence error if the setter refuses one it accepted then.

OK means the setter accepted the write, not that the guest sees the value: QEMU's setters
accept `r8`-`r15` on x86 in 32-bit mode without writing them, and aarch64's `pstate` without
rebuilding the execution state QEMU derives from it. The replay
does exactly the same, so it stays consistent with the recording.

The payload has its own version and the outer ND version is bumped; old logs are not
claimed compatible. Syscall and device effects remain unsupported. There is no
apply-completion callback yet. A write a hypercall handler makes is not queued: it is
recorded at the hypercall (below).

### Guest writes from callbacks

Changing the guest during a recording goes through one API, the guest-write API
(`include/panda/guest-write.h`; `panda.guest` in pandare2; `panda::guest` in Rust). Each write
is logged as part of one event at the instruction where the callback ran, and replay repeats
it there without running the callback. A write made any other way is refused with an error
(`GUEST_WRITE_EPERM`; pandare2's `GuestWriteRefused`), never dropped silently. Without
record/replay the same calls simply write. The design is in
[docs/design/rr-guest-mutation-api.md](../design/rr-guest-mutation-api.md).

| Call | Writes |
|---|---|
| `panda_guest_write_virt`, `panda_guest_write_phys` | guest memory, through the vCPU's page tables or by physical address (RAM only) |
| `panda_guest_write_reg` | a register by gdbstub number (`panda_guest_reg_lookup` maps a gdb XML name such as `x0`, `rip` or `xmm3`), through the target's setter |
| `panda_guest_set_retval` | the hypercall or syscall return register |
| `panda_guest_write_env`, `panda_guest_write_env_path` | bytes in the vCPU's `CPUArchState` at an offset, or through pointer fields of it, given as a path |
| `panda_guest_env_sync` | rebuilds what the target derives from env (arm hflags) and flushes the TLB |

The older entry points are wrappers over these: `panda_virtual_memory_write`,
`panda_physical_memory_write`, `panda_set_retval`, `panda_set_syscall_arg` and
`panda_write_guest_reg`, and pandare2's
`virtual_memory_write`, `physical_memory_write` and `arch.set_reg`/`set_retval`/`set_arg`, whose
register writes go through `panda.guest.env(cpu)`. That view of `CPUArchState` reads through
and turns each assignment into an env write: `env.regs[0] = 1`, `env.cp15.ttbr0_el[1] = x`.
A pointer field gives a view of what it points to, and writes through it are logged as a
path, because host addresses differ between the recording and replay processes:
`env.pmsav7.drbar[3] = x`. The engine checks each path step and range against a layout map
that each build generates from its own debug info (`panda/scripts/guest_layout.py`), so it
holds on that host. It refuses a range that overlaps a host pointer, an unknown pointer, and
repointing. A recording that wrote env replays only on an engine with the same layout: the
layout depends on the host, so a recording made on an x86_64 host may not replay on aarch64.

Where writes are logged so far is guest hypercalls. Each hypercall is one event, answered or
not. This covers the embedding API's consumer (`panda_set_guest_hypercall_callback`), mutating
`guest_hypercall` callbacks and `@panda.hypercall` handlers. During record/replay a pandare2
handler must say which it is: `observe_only=True` or `mutates=True`. Replay does not run
mutators and does not need them registered. It repeats each write with the same call, so the
translation it does (filling the TLB, setting page-table accessed/dirty bits) happens again
too, and returns the recorded answer. A write whose result differs from the recording's stops
the replay with a divergence error. Observe-only hypercall callbacks run in both, before the
writes.

Changing guest state any other way is unsupported. That includes storing into CPU state
through `panda_cpu_env`, cffi's `env_ptr` or `cpu_env()` in C, and target helpers that
rewrite env. Nothing logs such a write, so a replay of that recording diverges, usually
somewhere later and without naming the cause. The API covers every write and costs little
without record/replay too, so use it everywhere: `panda.guest` and its `env(cpu)` view in
Python, `PANDA_GUEST_ENV_SET(cpu, field, value)` and the `panda_guest_*` calls in C, and
`GuestWriter`/`env_write!` in Rust.

Everything else a handler might do to the guest stays refused while recording or replaying:
resets, mapping memory, setting the PC, injecting syscalls or faults, QEMU plugin
memory/register writes, and writes to another vCPU's env. A hypercall's logged writes are
capped at 16 MiB. The hypercall instructions end their translation block under icount (on
every target but x86, whose hypercall is a port write, which already does), so the event sits
at an exact instruction count and env changes take effect from the next block. The KVM
hypercall consumer cannot be recorded. `tests/hypercall_rr.py` records a handler's memory and
register writes and replays them without it, and checks that a raw env write fails the
recording, on x86_64 and aarch64.

## Limits

- TCG only, with one vCPU thread (`thread=single`).
- No snapshot loads while recording: a load isn't in the log, so the replay couldn't repeat it.
  `revert_sync` returns QEMU's "does not allow loading a snapshot while recording" error, a
  `panda_revert` returns -1, and a `savevm` that would replace the recording's start snapshot is
  refused. Fast restore refuses too, while recording and while replaying
  ([Snapshots](snapshots.md)).
- Replay with `run_replay`. QEMU's own `-icount rr=replay,rrfile=...` startup path can't replay
  a recording begun while the guest was running, and stops with an error that says to use
  `x-rr-begin-replay`, which is what `run_replay` does.
- On aarch64 and riscv64, reading the instruction count from a callback during a recording isn't
  reliable yet: the test suite skips that check there (`RRCB_NO_ICOUNT=1`).

The tests are `tests/rr.py`, `rrcb.py`, `rrstop.py` (several recordings in one process),
`rrload.py` (the refused loads) and `rr_blocks.py`. The [features page](../features.md#record-replay)
lists the engine patches record and replay rest on.
