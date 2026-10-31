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

Declare Python callbacks, PPP subscriptions, address hooks and hypercalls with
`observe_only=True` for recording and replay. Core callbacks also need `rr` in their
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

### Experimental ordered RAM effects

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

The payload has its own version and the outer ND version is bumped; old logs are not
claimed compatible. Register, syscall and device effects remain unsupported. Standalone
implementation tests pass; integrated builds, producer-free guest replay and observer-order
conformance are still pending. There is no apply-completion callback yet.

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
