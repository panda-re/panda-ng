# Record and replay

A recording captures everything nondeterministic that reaches the guest (device input, disk
completions, interrupts, timer reads) from one point in a run. Replaying it executes exactly the
same instructions again, as often as you like, with any callbacks and plugins attached. Record
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
