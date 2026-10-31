# Where each patch comes from, and its license

<!-- REUSE-IgnoreStart -->
Every patch in this series carries two trailers before its `---`:

- `SPDX-License-Identifier:` the licenses of the QEMU 11.1.2 files the patch changes (from
  each file's header; a file without one is GPL-2.0-or-later under QEMU's `LICENSE`, and an
  LGPL "version 2 or later" header is read as LGPL-2.1-or-later). A change to a file is under
  that file's license. `REUSE.toml` at the top of the repository repeats these.
<!-- REUSE-IgnoreEnd -->
- `Origin:` where the change was first written:
  - `upstream qemu <commit>`: a backport (none in this series);
  - `panda-re/panda`: ported from classic PANDA (none in this series; classic PANDA's code is
    in `engine/src/`, not in patches);
  - `panda-re/qemu`: from panda-re/qemu, panda-ng's earlier QEMU fork (the `pandang` series);
  - `penguin`: from rehosting/qemu_builder, penguin's QEMU series;
  - `ac88u`: from the ac88u external-board work;
  - `new`: written for panda-ng.

The trailers are part of each commit message in an `import-series.sh` tree, so
`export-series.sh` keeps them. Add both to a new patch, and update this table when the series
changes (the export does not rewrite this file). "Uncertain" in a note marks a judgement call.

Counts: new 52, penguin 26, panda-re/qemu 6, ac88u 2.

| # | subject | origin | license | note |
|---|---|---|---|---|
| 0001 | penguin: core callbacks and build wiring | penguin | GPL-2.0-or-later AND MIT | qemu_builder main (penguin's series) |
| 0002 | build: emit shared libraries for the Penguin embedding API | penguin | GPL-2.0-or-later | qemu_builder main (penguin's series) |
| 0003 | target/mips: route guest hypercalls into Penguin | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder main (penguin's series) |
| 0004 | target/arm: route guest hypercalls into Penguin | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder main (penguin's series) |
| 0005 | target/ppc: route guest hypercalls into Penguin | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder main (penguin's series) |
| 0006 | target/riscv: route guest hypercalls into Penguin | penguin | GPL-2.0-or-later | qemu_builder main (penguin's series) |
| 0007 | target/loongarch: route guest hypercalls into Penguin | penguin | GPL-2.0-or-later | qemu_builder main (penguin's series) |
| 0008 | target/i386, hw/i386: route guest hypercalls through an I/O port | penguin | LGPL-2.1-or-later AND MIT | qemu_builder main (penguin's series) |
| 0009 | kvm: expose hypercall and port-I/O exits to Penguin | penguin | GPL-2.0-or-later | qemu_builder main (penguin's series) |
| 0010 | penguin: snapshot ABI and a migratable vhost-user-vsock | penguin | GPL-2.0-or-later AND MIT | qemu_builder main (penguin's series) |
| 0011 | penguin: reset-request and QMP-command callbacks | penguin | LGPL-2.1-or-later AND MIT | qemu_builder main (penguin's series) |
| 0012 | hw/mips/malta: enlarge the prom env slot so the cmdline is not truncated | penguin | MIT | qemu_builder main (penguin's series) |
| 0013 | plugins: qemu_plugin_read_memory_vaddr_fault, page-in reads at a hook | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder workspace/pagein |
| 0014 | plugins, target/arm: a user-access window (DACR) for faulting reads | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder workspace/pagein |
| 0015 | plugins: faulting writes; uaccess fault index; ppc, riscv, loongarch, x86 windows | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder workspace/pagein |
| 0016 | plugins, kvm/x86: faulting accesses and breakpoint callbacks under KVM | penguin | GPL-2.0-or-later | qemu_builder workspace/pagein |
| 0017 | plugins: batched faulting accesses (qemu_plugin_access_memory_vaddr_fault) | penguin | GPL-2.0-or-later | qemu_builder workspace/pagein |
| 0018 | accel/tcg: translation-time address hooks and register pointers | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | qemu_builder workspace/pagein |
| 0019 | system, target/i386, target/arm: the syscall trap | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder workspace/pagein |
| 0020 | plugins: address hooks, register pointers, TLB-first reads, syscall events | penguin | GPL-2.0-or-later | qemu_builder workspace/pagein |
| 0021 | system: build the penguin syscall engine and host-side OSI | penguin | GPL-2.0-or-later | qemu_builder workspace/pagein |
| 0022 | i386: resume a KVM snapshot under TCG and a TCG snapshot under KVM | penguin | GPL-2.0-only AND GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | qemu_builder workspace/panda-next-kvmtcg; Patch-Set: igloo-core |
| 0023 | target/arm/kvm: penguin hypercalls under arm64 KVM (SMCCC filter + mailbox doorbell) | penguin | GPL-2.0-or-later | qemu_builder workspace/panda-next-arm64kvm; from Jacques Becker's kvm_aarch64 patch |
| 0024 | target/arm, arm_gic: resume an aarch64 KVM snapshot under TCG and a TCG snapshot under KVM | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder workspace/panda-next-arm64kvm (UNCERTAIN: kvm/tcg handoff companion of 0022; could be new) |
| 0025 | system/cpu-timers: continue the virtual clock when an icount process loads a non-icount snapshot | new | GPL-2.0-or-later AND MIT | observe's rr spike |
| 0026 | target/i386: keep the guest TSC continuous across a clock-domain change | new | GPL-2.0-or-later | observe's rr spike |
| 0027 | accel/tcg: x-tsc-vclock, the x86 TSC from the virtual clock without icount | new | MIT | observe's rr spike |
| 0028 | system/cpus: run_on_cpu under record/replay: release the replay mutex while the VM is stopped | new | GPL-2.0-or-later AND MIT | observe's rr spike |
| 0029 | qapi: a weak default penguin_handle_qmp, so the tools link on macOS | new | LGPL-2.1-or-later | macOS build fixes (panda-next-macos) |
| 0030 | replay: declare replay_mutex_locked() in system/replay.h | new | GPL-2.0-or-later | macOS build fixes (panda-next-macos) |
| 0031 | accel/tcg: forget the plugin callback's return address and flags after a longjmp | new | LGPL-2.1-or-later | review fixes to the penguin page-in/hook patches (UNCERTAIN: could count as penguin) |
| 0032 | plugins, targets: a user-access window owned by the access, not latched per vCPU | new | GPL-2.0-or-later AND LGPL-2.1-or-later | review fixes to the penguin page-in/hook patches (UNCERTAIN: could count as penguin) |
| 0033 | plugins: qemu_plugin_set_pc from a syscall entry callback must not deliver the trap again | new | GPL-2.0-or-later AND LGPL-2.1-or-later | review fixes to the penguin page-in/hook patches (UNCERTAIN: could count as penguin) |
| 0034 | i386/kvm: plugin accesses on KVM: kernel walk, declined breakpoints, full register sync | new | GPL-2.0-or-later | review fixes to the penguin page-in/hook patches (UNCERTAIN: could count as penguin) |
| 0035 | plugins: bind a CF_PCREL block to its virtual address when a plugin read it | new | GPL-2.0-or-later AND LGPL-2.1-or-later | panda-next-pcrel |
| 0036 | accel/tcg: address hooks keep CF_PCREL on outside hooked pages | new | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | panda-next-pcrel |
| 0037 | accel/tcg: rr-ready, launch a VM so that a recording can begin later | new | MIT | panda-next rr (R1-R4) |
| 0038 | replay: write the log header first and flush the log every second | new | GPL-2.0-or-later | panda-next rr (R1-R4) |
| 0039 | replay: x-rr-begin-record and x-rr-begin-replay, recording a running VM | new | GPL-2.0-or-later AND MIT | panda-next rr (R1-R4) |
| 0040 | replay: x-rr-end-record, end a recording and keep running | new | GPL-2.0-or-later | panda-next rr (R1-R4) |
| 0041 | panda: --enable-panda builds the PANDA API and libpanda-<target> | panda-re/qemu | GPL-2.0-or-later | pandang 0001, 0003 |
| 0042 | plugins: add qemu_plugin_tb_size and qemu_plugin_tb_get_insn_by_vaddr | panda-re/qemu | GPL-2.0-or-later | pandang 0002 |
| 0043 | panda: TCG core hooks | panda-re/qemu | LGPL-2.1-or-later | pandang 0004 |
| 0044 | panda: asid_changed on x86, arm and mips | panda-re/qemu | GPL-2.0-or-later AND LGPL-2.1-or-later | pandang 0006, 0009, 0012 |
| 0045 | panda: guest_hypercall on penguin's hypercall instruction | new | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | replaces pandang's hypercall patches with penguin's instruction |
| 0046 | panda: panda_disable_tb_chaining turns TB chaining off | new | LGPL-2.1-or-later | UNCERTAIN: re-implements classic PANDA's panda_disable_tb_chaining |
| 0047 | plugins: memory callbacks before the access | panda-re/qemu | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | aking's before/after memory callbacks (wrapup-rebase), reworked |
| 0048 | panda: before_find_fast before each TB lookup | panda-re/qemu | LGPL-2.1-or-later | UNCERTAIN: call site where pandang had it |
| 0049 | panda: asid_changed on riscv satp writes | new | GPL-2.0-or-later |  |
| 0050 | replay: x-rr-begin-record filter ids valid for -nic netdevs | new | GPL-2.0-or-later |  |
| 0051 | replay: no frontend room while a recorded chardev read is pending | new | GPL-2.0-or-later AND MIT |  |
| 0052 | replay: x-rr-begin-record rewinds the log to the start snapshot | new | GPL-2.0-or-later |  |
| 0053 | panda: main_loop_wait callback at each main-loop iteration | new | MIT |  |
| 0054 | panda: after_loadvm callback when load_snapshot succeeds | new | MIT |  |
| 0055 | hw/block/pflash_cfi01: one post_load state handler at a time | new | LGPL-2.1-or-later | UNCERTAIN: same fix as qemu_builder workspace/fastsnap 0015 (penguin) |
| 0056 | hw/core/loader: rom_reset logs the RAM it rewrites | new | GPL-2.0-or-later |  |
| 0057 | target/arm/ptw: log hardware access flag and dirty state updates | new | GPL-2.0-or-later |  |
| 0058 | panda: block callbacks learn of a longjmp out of the block | new | LGPL-2.1-or-later |  |
| 0059 | panda: after_block_exec hook after the PC is synchronized | new | LGPL-2.1-or-later |  |
| 0060 | vhost: don't turn dirty logging on in a device that can't log | new | GPL-2.0-or-later |  |
| 0061 | panda: before_loadvm hook before qemu_loadvm_state loads RAM | new | MIT |  |
| 0062 | accel/tcg: cpu_exec_step_atomic runs in an RCU read-side critical section | new | LGPL-2.1-or-later |  |
| 0063 | target/arm: a snapshot loads across -icount on/off (PMCEID0's INST_RETIRED bit) | new | GPL-2.0-or-later |  |
| 0064 | target/riscv: log hardware A/D page-table updates as dirty | new | GPL-2.0-or-later |  |
| 0065 | plugins: keep the access direction clear of the MemOpIdx in meminfo | new | GPL-2.0-or-later |  |
| 0066 | replay: the replay mutex pairs on its holder, not on replay_mode | new | GPL-2.0-or-later |  |
| 0067 | accel/tcg: rr vCPU loop takes the replay mutex if replay started under it | new | GPL-2.0-or-later AND MIT |  |
| 0068 | replay: the replay vmstate registers once and saves with no log open; a second begin works | new | GPL-2.0-or-later AND MIT |  |
| 0069 | module: load panda modules by path, checked against their target and a panda ABI hash | ac88u | GPL-2.0-only AND GPL-2.0-or-later | UNCERTAIN: from the extboard probe that loaded the ac88u board as a .so |
| 0070 | vl: -M PATH[:NAME] and -module PATH load panda modules | ac88u | GPL-2.0-or-later AND MIT | UNCERTAIN: from the extboard probe that loaded the ac88u board as a .so |
| 0071 | hw/core: build the configurable machine and its QAPI schema | new | GPL-2.0-or-later |  |
| 0072 | replay: a snapshot load judges its virtual timers at the loaded clock | new | GPL-2.0-or-later AND MIT |  |
| 0073 | replay: mark the begin-record start snapshot; refuse loads while recording | new | GPL-2.0-or-later AND MIT |  |
| 0074 | physmem: size a resizeable RAM block's maximum independently of the host page size | new | LGPL-2.1-or-later |  |
| 0075 | block/qcow2: drop the active VM state before a savevm writes it | new | MIT |  |
| 0076 | system/cpu-timers: a snapshot load resets the ticks' monotonic clamp | new | MIT |  |
| 0077 | panda: link a plugin interface per target against its engine | new | GPL-2.0-or-later |  |
| 0078 | target/riscv: a version 11 cpu state still loads | new | GPL-2.0-or-later | QEMU 11.1.2 raised the riscv cpu vmstate minimum to 12 without changing its fields |
| 0079 | hw/block/pflash: log storage writes in the dirty bitmap | new | LGPL-2.1-or-later |  |
| 0080 | migration/vmstate: skip per-field vmdesc bookkeeping when there is no vmdesc | penguin | GPL-2.0-or-later | qemu_builder workspace/fastsnap2 (5170d14) patch 0016; no third-party code (fastsnap's adopted qemu-libafl-bridge files, device-save.c and channel-buffer-writeback.c, are not taken) |
| 0081 | migration/qemu-file: inline the in-buffer case of the byte reader | penguin | MIT | qemu_builder workspace/fastsnap2 (5170d14) patch 0017; no third-party code |
| 0082 | accel/tcg: no PANDA or hook calls in the CPU loop while nothing uses them | new | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | idle-cost fixes: chaining in tcg_cflags, gated loop calls, CF_HOOKS |
| 0083 | plugins: emit block completion at outgoing edges | new | GPL-2.0-or-later AND LGPL-2.1-or-later | normal-completion TCG exits, no deferred PANDA block bookkeeping |
| 0084 | replay: guard guest state against analysis mutations | new | GPL-2.0-or-later | observer-only RR; reject unlogged analysis effects |
| 0085 | replay: coalesce repeated schedules of a pending BH | new | GPL-2.0-or-later | match ordinary BH pending semantics; retain self-reschedules |
| 0086 | system: cancel transient analysis before restore and reset | new | GPL-2.0-or-later AND MIT | abandon host in-flight state independently of load success |
