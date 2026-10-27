# KVM

Under KVM the guest runs on the host CPU at native speed, so it boots and reaches an interesting
state in seconds rather than minutes. The analysis features that instrument guest code need TCG.
A common pattern is to get the guest where you want it under KVM, snapshot it, and analyse from
there under TCG.

KVM support is **experimental**: no CI runner has KVM yet, and what is described here was
checked by hand.

## Running under KVM

KVM runs guests of the host's architecture, x86_64 or aarch64, and needs access to `/dev/kvm`.
From pandare2, pass `accel="kvm"`. The constructor checks that KVM can run the guest (an x86_64 or
aarch64 guest on a host of the same arch, with `/dev/kvm` readable and writable) and raises
`ValueError` saying what is missing; `rr=True` or an `-accel` in `extra_args` next to it raises
too. (`extra_args=["-accel", "kvm"]`, the way to ask before `accel=`, still works, with a
`DeprecationWarning`.) This is `tests/fastrestore_guards.py` checking that fast restore refuses
under KVM:

```python
--8<-- "tests/fastrestore_guards.py:kvm"
```

What works under KVM:

- the guest, at native speed;
- guest hypercalls and port I/O reaching the engine's own hypercall handlers (penguin's, for tools
  built on the engine; not PANDA's `guest_hypercall`), QEMU snapshots (`savevm`, `loadvm`) and
  the monitor (`run_monitor_cmd`);
- for tools built on the engine: hardware breakpoints (`qemu_plugin_hw_breakpoint_insert`,
  `qemu_plugin_register_vcpu_bp_cb`) and plugin memory accesses that page in what the guest
  hasn't touched (x86_64), see the [features page](../features.md#kvm).

What needs TCG:

- PANDA callbacks (block, instruction, memory and the rest): all but `main_loop_wait`,
  `after_machine_init` and `after_loadvm`. On a `Panda(accel="kvm")` their decorators,
  `hook`, `hypercall` and `hypersyscall` raise `NotImplementedError`. PANDA's
  `guest_hypercall` (which `@panda.hypercall` goes through) is among them: under KVM the
  engine hands a hypercall only to penguin's consumers;
- [record and replay](record-replay.md): `record` and `run_replay` raise `NotImplementedError`;
- [fast restore](snapshots.md#fast-restore): `panda_fast_snap` and `panda_fast_restore` return -1
  under KVM.

## Moving a guest between KVM and TCG

!!! warning "Experimental"
    No test in this repository covers this yet. It was verified by hand, in both directions,
    with a trial harness that isn't in the repository.

Save the running VM under one accelerator and load it under the other, with QEMU's
`savevm`/`loadvm` (the snapshot is stored in the qcow2 image) or by migrating to a file
(`migrate file:PATH` in the monitor, `-incoming file:PATH` on the loading side). The direction
doesn't matter: KVM to TCG to analyse, or TCG to KVM to run on fast.

The two sides must describe the same machine:

| | KVM side | TCG side |
|---|---|---|
| accelerator | `accel="kvm"` (`-accel kvm`) | `accel="tcg"`, the default |
| CPU, x86_64 | `-cpu host` | the default `-cpu` |
| CPU, aarch64 | `-cpu host` | the host's CPU as a named model, e.g. on a Pi 5 `-cpu cortex-a76,cntfrq=54000000` |
| machine, aarch64 | `-M virt,gic-version=2` | `-M virt,gic-version=2` |
| devices | the same `-nic` (e.g. `-nic none`), disks and memory | the same |

- The CPU model must match. `-cpu max` on the TCG side loads, but the guest panics after the load.
- On aarch64, `cntfrq=` gives the TCG CPU the KVM host's counter frequency (54 MHz on a Pi 5).
  Patch 0024's description also asks for `kvm-steal-time=off` on the KVM side
  (`-cpu host,kvm-steal-time=off`).
- The device set must match. An engine built with user networking adds a slirp NIC section to
  the saved state, so pass the same `-nic` on both sides, e.g. `-nic none`.
- Moving between hosts with different page sizes, such as a Pi 5 (16K pages) and an x86_64
  host (4K pages), needs engine patch 0074, which is in this series.

The engine patches this rests on are 0022 (x86), 0024 (aarch64 CPU and GIC) and 0074 (page
sizes); see the [features page](../features.md#kvm-tcg) and the
[patch list](../reference/patches.md).
