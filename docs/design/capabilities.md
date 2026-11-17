# Startup capabilities

A startup capability is something a run must choose before the machine exists, because it changes
the run's contract or its layout in a way that can't be undone mid-run. Anything that is always
available, or that the engine can decide from the running system, is not a capability and has no
gate.

This replaces the six bits of 0086/0087 (`analysis`, `fuzz`, `rr`, `guest-hypercall`, `handoff`,
`taint`).

## The model

| Capability | Chosen | What it means |
|---|---|---|
| **record** | startup, off by default | The machine is built to be recordable. A recording starts and stops on request at any point, as often as needed in one run. |
| **taint** | startup | Shadow state is reserved before guest memory exists. |

## record

Recording needs a deterministic machine: icount and single-threaded TCG, one vCPU, and devices
whose inputs can be logged. The `record` capability builds the machine that way from launch
(today's `-accel tcg,rr-ready=on`). Startup refuses arguments that can't be recorded, naming the
capability and the argument: another accelerator, `thread=multi`, more than one vCPU,
`-snapshot`, `-rtc base=localtime`, vhost backends.

With the capability, a recording can begin at any point of the run and end at any point, and the
run can record again afterwards: begin, end, begin, end, in one process. Each recording starts
from a snapshot taken when it begins and replays on its own. Between recordings the run continues
normally; snapshots, loads and analysis work as in any run. Without the capability, a request to
record is refused.

## Replay is a mode, not a capability

Replay is not requested at startup: a run is in replay when it loads a recording, and the engine
knows when that is. Replay paths check that a recording is loaded. The recording decides the
machine, so the arguments that would change it are refused. Replay excludes:

| Feature | In replay |
|---|---|
| record | refused |
| guest-visible mutation outside the log (register and memory writes, syscall mutation, fast restore) | refused; logged effects are replayed |
| hypercall answers | come from the log; live handlers observe only |
| live devices and network input | come from the log |
| taint | allowed |
| analysis (observe-only callbacks and plugins) | allowed |

The checks live in one table that replay consults when it starts and when a feature is requested,
instead of separate refusals in each feature.

## taint

Taint reserves its shadow state before the machine is realized, so it stays a startup capability.
It does not imply record or replay.

## Gating rules

- A capability gates only behaviour that depends on the startup choice. Code that can tell what
  to do from the running system (a loaded snapshot's mode, whether a recording is loaded, whether
  a callback is registered) checks that instead.
- A run with no capabilities still has everything else: callbacks, plugins, hooks, coverage, fast
  snapshot/restore, guest hypercalls and cross-mode snapshot loading.
- Each capability lists what it refuses, and the refusal names the capability and the argument.

## Interfaces

- Command line: `-capabilities record,taint` (comma-separated; none by default).
- C: `panda_configure_capabilities(PANDA_CAP_RECORD | ...)` before the machine is created.
- Python: `Panda(..., capabilities=["record", "taint"])`.

The old names (`analysis`, `rr`, `guest-hypercall`, `handoff`, `fuzz`) are removed, not mapped.

## Changes to the series

- 0086: the bit set becomes `record` and `taint`. The `analysis`, `guest-hypercall`, `handoff`
  and `fuzz` bits and their gates are removed.
- 0087: the handoff and cross-icount adaptations run whenever the loaded snapshot's mode differs
  from the machine's.
- `replay/replay-effect.c` and the other `rr` gates: `record` for recording-only paths; replay
  paths check that a recording is loaded.
- Fast restore checks its limits when used: refused while recording or in replay, and refused,
  naming the device, on a machine with devices that write guest RAM without marking it dirty.
- pandare2: `rr=True` becomes `capabilities=["record"]`; replay needs no capability argument.

## Not covered here

- Taint during a live run (not only in replay).
- Recording a run launched without `record` (under KVM or MTTCG) by switching the machine to a
  recordable mode when a recording is requested. Thread mode and some device settings (virtio
  ioeventfd) are fixed when devices are created, so this needs more than a snapshot reload.
