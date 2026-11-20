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

Counts: new 76, penguin 25, panda-re/qemu 6, ac88u 2.

| # | subject | origin | license | note |
|---|---|---|---|---|
| 0001 | system: the embedding API's core callbacks and build wiring | penguin | GPL-2.0-or-later AND MIT | qemu_builder main (penguin's series) |
| 0002 | build: emit shared libraries for the embedding API | penguin | GPL-2.0-or-later | qemu_builder main (penguin's series) |
| 0003 | target/mips: route guest hypercalls to the embedding API | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder main (penguin's series) |
| 0004 | target/arm: route guest hypercalls to the embedding API | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder main (penguin's series) |
| 0005 | target/ppc: route guest hypercalls to the embedding API | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder main (penguin's series) |
| 0006 | target/riscv: route guest hypercalls to the embedding API | penguin | GPL-2.0-or-later | qemu_builder main (penguin's series) |
| 0007 | target/loongarch: route guest hypercalls to the embedding API | penguin | GPL-2.0-or-later | qemu_builder main (penguin's series) |
| 0008 | target/i386, hw/i386: route guest hypercalls through an I/O port | penguin | LGPL-2.1-or-later AND MIT | qemu_builder main (penguin's series) |
| 0009 | kvm: expose hypercall and port-I/O exits to the embedding API | penguin | GPL-2.0-or-later | qemu_builder main (penguin's series) |
| 0010 | system: snapshot ABI and a migratable vhost-user-vsock | penguin | GPL-2.0-or-later AND MIT | qemu_builder main (penguin's series) |
| 0011 | system: reset-request and QMP-command callbacks | penguin | LGPL-2.1-or-later AND MIT | qemu_builder main (penguin's series) |
| 0012 | hw/mips/malta: enlarge the prom env slot so the cmdline is not truncated | penguin | MIT | qemu_builder main (penguin's series) |
| 0013 | plugins: qemu_plugin_read_memory_vaddr_fault, page-in reads at a hook | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder workspace/pagein |
| 0014 | plugins, target/arm: a user-access window (DACR) for faulting reads | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder workspace/pagein |
| 0015 | plugins: faulting writes; uaccess fault index; ppc, riscv, loongarch, x86 windows | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder workspace/pagein |
| 0016 | plugins, kvm/x86: faulting accesses and breakpoint callbacks under KVM | penguin | GPL-2.0-or-later | qemu_builder workspace/pagein |
| 0017 | plugins: batched faulting accesses (qemu_plugin_access_memory_vaddr_fault) | penguin | GPL-2.0-or-later | qemu_builder workspace/pagein |
| 0018 | accel/tcg: translation-time address hooks and register pointers | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | qemu_builder workspace/pagein |
| 0019 | system, target/i386, target/arm: the syscall trap | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder workspace/pagein |
| 0020 | plugins: address hooks, register pointers, TLB-first reads, syscall events | penguin | GPL-2.0-or-later | qemu_builder workspace/pagein |
| 0021 | i386: resume a KVM snapshot under TCG and a TCG snapshot under KVM | penguin | GPL-2.0-only AND GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | qemu_builder workspace/panda-next-kvmtcg; Patch-Set: igloo-core |
| 0022 | target/arm/kvm: guest hypercalls under arm64 KVM (SMCCC filter + mailbox doorbell) | penguin | GPL-2.0-or-later | qemu_builder workspace/panda-next-arm64kvm; from Jacques Becker's kvm_aarch64 patch |
| 0023 | target/arm, arm_gic: resume an aarch64 KVM snapshot under TCG and a TCG snapshot under KVM | penguin | GPL-2.0-or-later AND LGPL-2.1-or-later | qemu_builder workspace/panda-next-arm64kvm (UNCERTAIN: kvm/tcg handoff companion of 0021; could be new) |
| 0024 | system/cpu-timers: continue the virtual clock when an icount process loads a non-icount snapshot | new | GPL-2.0-or-later AND MIT | observe's rr spike |
| 0025 | target/i386: keep the guest TSC continuous across a clock-domain change | new | GPL-2.0-or-later | observe's rr spike |
| 0026 | accel/tcg: x-tsc-vclock, the x86 TSC from the virtual clock without icount | new | MIT | observe's rr spike |
| 0027 | system/cpus: run_on_cpu under record/replay: release the replay mutex while the VM is stopped | new | GPL-2.0-or-later AND MIT | observe's rr spike |
| 0028 | qapi: a weak default panda_handle_qmp, so the tools link on macOS | new | LGPL-2.1-or-later | macOS build fixes (panda-next-macos) |
| 0029 | replay: declare replay_mutex_locked() in system/replay.h | new | GPL-2.0-or-later | macOS build fixes (panda-next-macos) |
| 0030 | accel/tcg: forget the plugin callback's return address and flags after a longjmp | new | LGPL-2.1-or-later | review fixes to the penguin page-in/hook patches (UNCERTAIN: could count as penguin) |
| 0031 | plugins, targets: a user-access window owned by the access, not latched per vCPU | new | GPL-2.0-or-later AND LGPL-2.1-or-later | review fixes to the penguin page-in/hook patches (UNCERTAIN: could count as penguin) |
| 0032 | plugins: qemu_plugin_set_pc from a syscall entry callback must not deliver the trap again | new | GPL-2.0-or-later AND LGPL-2.1-or-later | review fixes to the penguin page-in/hook patches (UNCERTAIN: could count as penguin) |
| 0033 | i386/kvm: plugin accesses on KVM: kernel walk, declined breakpoints, full register sync | new | GPL-2.0-or-later | review fixes to the penguin page-in/hook patches (UNCERTAIN: could count as penguin) |
| 0034 | plugins: bind a CF_PCREL block to its virtual address when a plugin read it | new | GPL-2.0-or-later AND LGPL-2.1-or-later | panda-next-pcrel |
| 0035 | accel/tcg: address hooks keep CF_PCREL on outside hooked pages | new | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | panda-next-pcrel |
| 0036 | accel/tcg: rr-ready, launch a VM so that a recording can begin later | new | MIT | panda-next rr (R1-R4) |
| 0037 | replay: write the log header first and flush the log every second | new | GPL-2.0-or-later | panda-next rr (R1-R4) |
| 0038 | replay: x-rr-begin-record and x-rr-begin-replay, recording a running VM | new | GPL-2.0-or-later AND MIT | panda-next rr (R1-R4) |
| 0039 | replay: x-rr-end-record, end a recording and keep running | new | GPL-2.0-or-later | panda-next rr (R1-R4) |
| 0040 | panda: --enable-panda builds the PANDA API and libpanda-<target> | panda-re/qemu | GPL-2.0-or-later | pandang 0001, 0003 |
| 0041 | plugins: add qemu_plugin_tb_size and qemu_plugin_tb_get_insn_by_vaddr | panda-re/qemu | GPL-2.0-or-later | pandang 0002 |
| 0042 | panda: TCG core hooks | panda-re/qemu | LGPL-2.1-or-later | pandang 0004 |
| 0043 | panda: asid_changed on x86, arm and mips | panda-re/qemu | GPL-2.0-or-later AND LGPL-2.1-or-later | pandang 0006, 0009, 0012 |
| 0044 | panda: guest_hypercall on the guest hypercall instruction | new | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | replaces pandang's hypercall patches with penguin's instruction |
| 0045 | panda: panda_disable_tb_chaining turns TB chaining off | new | LGPL-2.1-or-later | UNCERTAIN: re-implements classic PANDA's panda_disable_tb_chaining |
| 0046 | plugins: memory callbacks before the access | panda-re/qemu | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | aking's before/after memory callbacks (wrapup-rebase), reworked |
| 0047 | panda: before_find_fast before each TB lookup | panda-re/qemu | LGPL-2.1-or-later | UNCERTAIN: call site where pandang had it |
| 0048 | panda: asid_changed on riscv satp writes | new | GPL-2.0-or-later |  |
| 0049 | replay: x-rr-begin-record filter ids valid for -nic netdevs | new | GPL-2.0-or-later |  |
| 0050 | replay: no frontend room while a recorded chardev read is pending | new | GPL-2.0-or-later AND MIT |  |
| 0051 | replay: x-rr-begin-record rewinds the log to the start snapshot | new | GPL-2.0-or-later |  |
| 0052 | panda: main_loop_wait callback at each main-loop iteration | new | MIT |  |
| 0053 | panda: after_loadvm callback when load_snapshot succeeds | new | MIT |  |
| 0054 | hw/block/pflash_cfi01: one post_load state handler at a time | new | LGPL-2.1-or-later | UNCERTAIN: same fix as qemu_builder workspace/fastsnap 0015 (penguin) |
| 0055 | hw/core/loader: rom_reset logs the RAM it rewrites | new | GPL-2.0-or-later |  |
| 0056 | target/arm/ptw: log hardware access flag and dirty state updates | new | GPL-2.0-or-later |  |
| 0057 | panda: block callbacks learn of a longjmp out of the block | new | LGPL-2.1-or-later |  |
| 0058 | panda: after_block_exec hook after the PC is synchronized | new | LGPL-2.1-or-later |  |
| 0059 | vhost: don't turn dirty logging on in a device that can't log | new | GPL-2.0-or-later |  |
| 0060 | panda: before_loadvm hook before qemu_loadvm_state loads RAM | new | MIT |  |
| 0061 | accel/tcg: cpu_exec_step_atomic runs in an RCU read-side critical section | new | LGPL-2.1-or-later |  |
| 0062 | target/arm: a snapshot loads across -icount on/off (PMCEID0's INST_RETIRED bit) | new | GPL-2.0-or-later |  |
| 0063 | target/riscv: log hardware A/D page-table updates as dirty | new | GPL-2.0-or-later |  |
| 0064 | plugins: keep the access direction clear of the MemOpIdx in meminfo | new | GPL-2.0-or-later |  |
| 0065 | replay: the replay mutex pairs on its holder, not on replay_mode | new | GPL-2.0-or-later |  |
| 0066 | accel/tcg: rr vCPU loop takes the replay mutex if replay started under it | new | GPL-2.0-or-later AND MIT |  |
| 0067 | replay: the replay vmstate registers once and saves with no log open; a second begin works | new | GPL-2.0-or-later AND MIT |  |
| 0068 | module: load panda modules by path, checked against their target and a panda ABI hash | ac88u | GPL-2.0-only AND GPL-2.0-or-later | UNCERTAIN: from the extboard probe that loaded the ac88u board as a .so |
| 0069 | vl: -M PATH[:NAME] and -module PATH load panda modules | ac88u | GPL-2.0-or-later AND MIT | UNCERTAIN: from the extboard probe that loaded the ac88u board as a .so |
| 0070 | hw/core: build the configurable machine and its QAPI schema | new | GPL-2.0-or-later |  |
| 0071 | replay: a snapshot load judges its virtual timers at the loaded clock | new | GPL-2.0-or-later AND MIT |  |
| 0072 | replay: mark the begin-record start snapshot; refuse loads while recording | new | GPL-2.0-or-later AND MIT |  |
| 0073 | physmem: size a resizeable RAM block's maximum independently of the host page size | new | LGPL-2.1-or-later |  |
| 0074 | block/qcow2: drop the active VM state before a savevm writes it | new | MIT |  |
| 0075 | system/cpu-timers: a snapshot load resets the ticks' monotonic clamp | new | MIT |  |
| 0076 | panda: link a plugin interface per target against its engine | new | GPL-2.0-or-later |  |
| 0077 | target/riscv: a version 11 cpu state still loads | new | GPL-2.0-or-later | QEMU 11.1.2 raised the riscv cpu vmstate minimum to 12 without changing its fields |
| 0078 | hw/block/pflash: log storage writes in the dirty bitmap | new | LGPL-2.1-or-later |  |
| 0079 | migration/vmstate: skip per-field vmdesc bookkeeping when there is no vmdesc | penguin | GPL-2.0-or-later | qemu_builder workspace/fastsnap2 (5170d14) patch 0016; no third-party code (fastsnap's adopted qemu-libafl-bridge files, device-save.c and channel-buffer-writeback.c, are not taken) |
| 0080 | migration/qemu-file: inline the in-buffer case of the byte reader | penguin | MIT | qemu_builder workspace/fastsnap2 (5170d14) patch 0017; no third-party code |
| 0081 | accel/tcg: no PANDA or hook calls in the CPU loop while nothing uses them | new | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | idle-cost fixes: chaining in tcg_cflags, gated loop calls, CF_HOOKS |
| 0082 | plugins: emit block completion at outgoing edges | new | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | normal-completion TCG exits, no deferred PANDA block bookkeeping; gated callbacks as one TCG op (plugin_gate) on x86_64 hosts |
| 0083 | replay: guard guest state against analysis mutations | new | GPL-2.0-or-later | observer-only RR; reject unlogged analysis effects |
| 0084 | replay: coalesce repeated schedules of a pending BH | new | GPL-2.0-or-later | match ordinary BH pending semantics; retain self-reschedules |
| 0085 | system: cancel transient analysis before restore and reset | new | GPL-2.0-or-later AND MIT | abandon host in-flight state independently of load success |
| 0086 | system: choose startup capabilities with -capabilities | new | GPL-2.0-or-later AND MIT | record and taint, none by default; sealed by an arity-aware prepass before option, module and plugin side effects; embedding handshake |
| 0087 | accel/tcg: the record capability launches a recordable machine | new | GPL-2.0-or-later AND MIT | record turns rr-ready on; startup refuses arguments that can't be recorded (accelerator, thread=multi, vCPUs, replay blockers, vhost) |
| 0088 | replay: recording needs the record capability | new | GPL-2.0-or-later | x-rr-begin-record and -icount rr=record check the capability and the replay table |
| 0089 | replay: consult the replay table when replay starts | new | GPL-2.0-or-later | one table of what replay refuses, takes from the log or allows (replay-conflicts.c) |
| 0090 | replay: serialize deferred analysis RAM writes as ordered async events | new | GPL-2.0-or-later | bounded RAM-only effects, producer-free replay, versioned ND transport |
| 0091 | replay: stub analysis register effects without TCG | new | GPL-2.0-or-later | no-TCG linkage for the register effect request |
| 0092 | replay: log analysis guest writes at their instruction | new | GPL-2.0-or-later | synchronous guest-write event (hypercall sites first); hypercall instructions start I/O |
| 0093 | tcg: taint shadow pass hooks | new | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | CF_TAINT pass call site, shadow TLB hooks, device-write clear, reports from memory-writing helpers (MOPS, DC ZVA, cbo.zero), Arm user-bank register helpers, MIPS partial-word stores and ll/lld, CF_TAINT_CHECK (a variant switch leaves the TB, so variants never chain), taint refused without TCG, liveness_pass_2 temps only for the indirect globals a TB uses; gated by the taint capability |
| 0094 | migration/savevm: reset load_section_id at the start of qemu_loadvm_state | new | MIT | a second load matched part/end sections by the previous stream's ids (pseries rr begin with blkreplay); not taint-specific |
| 0095 | target/mips: move the Random register's generator state into CPUMIPSState | new | GPL-2.0-or-later AND LGPL-2.1-or-later | static LCG state made restored guests pick different TLB slots; MIPS rr diverged ~1M insns in; not taint-specific |
| 0096 | target/loongarch: migrate the constant timer | new | GPL-2.0-or-later | the timer's QEMUTimer, and the CPU's halted and interrupt_request (dc->vmsd skips vmstate_cpu_common), were not in vmstate, so a loaded guest idles forever; not taint-specific |
| 0097 | target/ppc: rebuild BookE/40x exception vectors on load | new | GPL-2.0-or-later | fixes ppce500 savevm/loadvm (NIP 0 at the first interrupt); necessary but not sufficient for ppc32 rr, which passes together with 0098; not taint-specific |
| 0098 | hw/intc/openpic: restore the CPUs' external proxy mode on load | new | GPL-2.0-or-later | env->mpic_proxy was set only by a GCR write and not re-derived on load, so a loaded e500 guest in proxy mode got external interrupts never acknowledged (stuck in its handler); with 0097 ppc32 rr passes; not taint-specific |
| 0099 | hw/pci-host/ppce500: rebuild the PCI windows on load | new | GPL-2.0-or-later | the outbound/inbound window aliases were built only on guest register writes, so a -loadvm'd ppce500 guest's PCI BARs were unreachable; not taint-specific |
| 0100 | hw/pci-host/ppce500: migrate the host bridge's config space | new | GPL-2.0-or-later | the host bridge had no vmsd, so BAR0 (CCSR in PCI space, where MSIs reach MSIIR) was unassigned after any load and every MSI was lost (ppc32 virtio-blk hung on its first request); with 0099 tests/busybox.sh powerpc passes; not taint-specific |
| 0101 | monitor: info panda-events lists the PANDA event registry | new | GPL-2.0-or-later | the info command for the event registry (panda/src/events-hmp.c has the handler) |
| 0102 | target: system-call pairing sites | new | GPL-2.0-or-later AND LGPL-2.1-or-later | the trap and return-to-user sites of the engine's declared-ABI system-call pairing (panda/src/syscall-pair.c) |
| 0103 | accel/tcg, target/arm: take SVC, UNDEF and aborts to EL1 without the BQL | new | GPL-2.0-or-later AND LGPL-2.1-or-later | an optional TCGCPUOps.exception_bql_free; on Arm, SVC/UNDEF/aborts from EL0/EL1 to EL1 and the exception returns skip the BQL while every EL change hook is idle (PMU: no counter enabled; GICv3: EL0/EL1 only), so AArch64 guest system calls scale with vCPUs under MTTCG |
| 0104 | accel/tcg: clear a lone CPU_INTERRUPT_EXITTB without the BQL | new | LGPL-2.1-or-later | cpu_handle_interrupt took the BQL only to clear the TB exit request that Arm exception entries and PowerPC rfi set, once per system call once delivery is BQL-free; cleared atomically when it is the only bit, outside record/replay |
| 0105 | target/ppc: take system calls from problem state without the BQL | new | LGPL-2.1-or-later AND MIT | sc/scv from problem state on BookE and Book3S 64-bit through TCGCPUOps.exception_bql_free (0103); ppc_maybe_interrupt and cpu_interrupt_exittb skip the BQL when they have nothing to do or act on the own vCPU, so PowerPC guest system calls scale with vCPUs under MTTCG |
| 0106 | target/mips: update CP0 Cause atomically | new | GPL-2.0-or-later AND LGPL-2.1-or-later AND MIT | CP0 Cause holds both the vCPU's exception bits and the interrupt lines that devices set under the BQL, while mtc0 Cause/Compare and a timer expiry seen by mfc0 Count write it on the vCPU without the BQL; every update becomes an atomic read-modify-write of its own bits, so neither side loses the other's (a lost IP bit is a lost interrupt) |
| 0107 | target/riscv: take ECALL from U-mode and page faults to S-mode without the BQL | new | GPL-2.0-or-later | TCGCPUOps.exception_bql_free (0103) on RISC-V: an ECALL from U-mode, illegal instructions and instruction/load/store page faults taken to S-mode with V=0 are delivered without the BQL (the hart's own state only, as sret writes it); everything else keeps it |
| 0108 | target/loongarch: take SYSCALL and page faults without the BQL | new | GPL-2.0-or-later | TCGCPUOps.exception_bql_free (0103) on LoongArch: SYSCALL, page faults, INE/IPE, FP/LSX/LASX-disabled and FPE are delivered without the BQL (own CSRs only, as ertn writes them; ESTAT's IS bits change only on the vCPU's thread under TCG); interrupts, DBP, BRK, address and bound errors keep it |
| 0109 | target/i386: deliver INT 0x80 from user mode without the BQL | new | LGPL-2.1-or-later | TCGCPUOps.exception_bql_free (0103) on x86: INT 0x80 from CPL 3 in protected mode, outside SVM guest mode and SMM, is delivered without the BQL (own state and guest memory, as the SYSCALL/SYSENTER helpers); every other vector and exception keeps it |
| 0110 | plugins: no marker ops per instruction | new | GPL-2.0-or-later | each instruction's first and last op recorded instead of two plugin_cb markers; translation cost |
| 0111 | plugins: a translation callback can declare where it reads access values | new | GPL-2.0-or-later | qemu_plugin_register_vcpu_tb_trans_cb_flags and QEMU_PLUGIN_TB_TRANS_MEM_VALUE_IN_CB: value stores only for instructions with memory callbacks; opt-in, plugins that do not declare it keep upstream behaviour |
| 0112 | plugins: a translation callback can declare it registers no memory callbacks | new | GPL-2.0-or-later AND MIT | QEMU_PLUGIN_TB_TRANS_NO_MEM_CBS: no plugin memory ops in blocks translated while every translation callback declared it; a memory callback registered on them is ignored with a warning |
