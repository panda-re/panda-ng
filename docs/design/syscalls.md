# Syscalls: engine enter/return events and the `syscalls` plugin

The engine reports system calls as two events, `syscall.enter` and `syscall.return`, and does
nothing else with them. Names, argument types, decoding, per-syscall callbacks, syscalls2
compatibility and Penguin's hook model live in the `syscalls` plugin. With nothing subscribed,
the syscall path costs one flag load per trap.

## Layers

| Layer | Where | Does | Does not |
|---|---|---|---|
| L0 trap | `system/syscall-trap.c` + one target hook set per arch | detects entry, reads number and arguments per ABI, keys the task, pairs a call with its return, normalizes the return value, filters by a per-ABI bitmap, and holds, faults, injects or rewinds a call | names, types, policy |
| L1 events | `panda/src/syscall-events.c` | declares `syscall.enter` and `syscall.return` on the event registry, keeps per-subscription watches, orders observers and mutators, and logs mutations under record/replay | names, types |
| L2 plugin | `plugins/syscalls` | tables, lookup, decoders, per-syscall events, syscalls2-compatible PPP, Penguin's hook model | — |

syscalls2 remains in tree, unchanged, until `syscalls` serves every profile it serves. Loading
`syscalls2` then loads `syscalls`.

## Cost rules

- **L0.** With no watch armed, entry costs one load and a predicted branch, and a return costs
  nothing. A number nobody watches costs one bitmap test inside the helper. A call's return is
  tracked only when a subscriber asked for that call's return.
- **L1.** The trap has no L1 consumer while no subscription exists. Subscribing and unsubscribing
  recompute the trap's bitmap as the OR of the subscriptions' watches.
- **L2.** Loading `syscalls` subscribes to nothing. Each registration (per-syscall, PPP, Penguin
  hook) adds exactly the numbers it needs; `on_all_*` adds all of them. L2 holds one L1
  subscription per kind (observer, mutator) and fans out internally. Arguments are decoded only
  when a callback asks for them.

| Configuration | Bound, per arch |
|---|---|
| Engine, nothing subscribed | within noise of stock QEMU on a syscall-heavy workload |
| `syscalls` loaded, nothing registered | within noise of the line above |
| One callback on one number, for the other numbers | within noise of the first line |
| Watched entry, C observer | trap floor + 75 ns |
| Watched entry and return, C observer | trap floor + 250 ns |
| compat `on_all_sys_enter2` + `on_all_sys_return2` | at least 4x faster than syscalls2 |

## L0: the trap

### Architecture contract

Each target provides one `SyscallTrapArch` and calls `syscall_trap_enter()` from its syscall
exception path and `syscall_trap_return()` from its return-to-user path while a call is pending.

```c
typedef struct SyscallTrapEntry {
    int abi;
    int64_t num;
    uint64_t args[6];
    uint64_t pc;            /* past the trap instruction */
    unsigned len;           /* trap instruction length */
    uint64_t usp;           /* user stack pointer (MIPS o32: args 5-8 are there) */
} SyscallTrapEntry;

typedef struct SyscallTrapKey {
    uint64_t k1, k2;        /* same at entry and at that call's return; differs between tasks */
    uint64_t percpu;
    uint32_t kmmu;          /* kernel MMU indexes, for reads from callbacks */
} SyscallTrapKey;

typedef struct SyscallTrapArch {
    unsigned caps;          /* SYSCALL_ARCH_FAULT | _INJECT | _REWIND | _RESTORE */
    bool (*decode)(CPUState *cs, SyscallTrapEntry *e);
    void (*key)(CPUState *cs, SyscallTrapKey *k);
    int64_t (*get_ret)(CPUState *cs, int abi);             /* -errno on error */
    void (*set_ret)(CPUState *cs, int abi, int64_t ret);
    bool (*restarting)(CPUState *cs, int abi);
    void (*set_args)(CPUState *cs, int abi, const uint64_t args[6]);
    void (*inject)(CPUState *cs, int abi, int64_t num, const uint64_t args[6]);
    void (*rewind)(CPUState *cs, const SyscallTrapRewind *rw);
    void (*restore)(CPUState *cs, const SyscallTrapRewind *rw, int64_t ret);
} SyscallTrapArch;
```

- **Return values are normalized.** On MIPS (`a3` set) and on PPC `sc` (`cr0.SO` set) an error is
  a positive errno plus a flag; `get_ret` returns `-errno`, and `set_ret` writes both the value
  and the flag. PPC `scv` and every other ABI already use `-errno`.
- **Capabilities.** An arch without `_INJECT` still reports calls. L1 refuses inject on it, with
  a reason naming the arch.
- **KVM.** The trap is TCG-only. Under KVM no syscall events are emitted, and subscribing is
  refused with that reason.
- **vDSO.** Calls the kernel serves in user mode through the vDSO never trap and are never
  reported.

### ABIs

The ABI values are those of `enum qemu_plugin_syscall_abi`, which is public plugin API. New ABIs
are appended and existing values are never renumbered.

| Value | ABI | Targets |
|---|---|---|
| `X86_64` | x86_64 `syscall` | x86_64 |
| `I386` | `int 0x80`, `sysenter` | i386, x86_64 (compat) |
| `AARCH64` | `svc` | aarch64 |
| `ARM` | EABI `svc 0` | arm, aarch64 (compat) |
| `ARM_OABI` | `svc #nr` | arm |
| `MIPS` | o32 | mips, mipsel, mips64, mips64el (compat) |
| `PPC64` | `sc`/`scv`, 64-bit | ppc64 |
| `PPC` | `sc`, 32-bit | ppc, ppc64 (compat) |
| `RISCV64` | `ecall` | riscv64 |
| `RISCV32` | `ecall` | riscv32 |
| `LOONGARCH64` | `syscall` | loongarch64 |
| `MIPS_N32` | n32 | mips64, mips64el |
| `MIPS_N64` | n64 | mips64, mips64el |
| `X32` | x32 (`syscall` with bit 30 set) | x86_64 |

### Rules every arch follows

- **The task key** identifies the task at entry and at that call's return, whatever mode the
  task runs in. A 32-bit call from a 64-bit task uses the same width of key as a native call.
- **vfork.** A child that returns from the parent's call is not reported as the parent's return.
- **No call is assumed not to return.** `rt_sigreturn`, `exit` and `exit_group` can return under
  seccomp or ptrace; their returns are paired like any other.
- **A skipped call's return** reports the value the mutator set; the number register is not
  reread.
- **Restarts** (`-ERESTART*`) are reported as a return with `restart` set, and the restarted call
  as a new entry.

## L1: `syscall.enter` and `syscall.return`

Both events are declared on the event registry (see `event-model.md`) with instances, so a
subscriber's per-call data spans entry and return.

```c
typedef struct PandaSyscall {
    uint32_t size;
    int32_t abi;
    int64_t num;
    uint64_t args[6];
    uint64_t pc;
    uint64_t call_id;       /* unique per call; the same at entry and return */
    int64_t ret;            /* return */
    uint32_t flags;         /* PANDA_SC_RESTART, _RESUMED, _INJECTED */
} PandaSyscall;

bool panda_syscall_watch(panda_sub sub, int abi, int64_t num, unsigned what); /* num -1: all */
bool panda_syscall_unwatch(panda_sub sub, int abi, int64_t num, unsigned what);
bool panda_syscall_want_return(PandaSyscall *sc);   /* from an entry callback */

/* mutators */
bool panda_syscall_skip(PandaSyscall *sc, int64_t ret);
bool panda_syscall_set_arg(PandaSyscall *sc, unsigned i, uint64_t value);
bool panda_syscall_set_ret(PandaSyscall *sc, int64_t ret);
bool panda_syscall_fault(PandaSyscall *sc, uint64_t addr, bool write);
bool panda_syscall_inject(PandaSyscall *sc, int64_t num, const uint64_t args[6]);
```

- **Order.** `PANDA_SUB_OBSERVE_ORIGINAL` observers first, then mutators by priority, then
  observers, which see the final values.
- **Holds.** A mutator that faults or injects holds the call. When the call comes back, the
  holder runs again first, then the subscribers after it.
- **Record/replay.** Every mutator call is one logged guest mutation (`rr-guest-mutation-api.md`).
  On replay the mutator does not run and its logged effects are applied at the same point. A
  mutator call that cannot be logged is refused with a reason; none is silently skipped.
- **Detach.** Unsubscribing a mutator that holds calls releases them as if it had returned
  without changes.
- **Other consumers.** The QEMU-plugin syscall bridge (`plugins/syscall-system.c`) is one more
  trap consumer, ordered as an observer.

### Multi-consumer trap and L1, as built

Overlay only, with no engine patch and no target change.

**The trap** (`system/syscall-trap.h`) takes any number of consumers:
`syscall_trap_consumer_add(ops, opaque, order, priority)`, with order classes
`SYSCALL_TRAP_OBSERVE_ORIGINAL`, `_MUTATE` and `_OBSERVE`, then priority, then age.
- **Watches.** Each consumer has its own watch bitmaps. The trap's bitmap is their OR, kept per
  entry on each watch change and recomputed in full when a consumer goes. `syscall_trap_on` is
  set while any entry is set, or while a call is held.
- **Per-call state.** A call's own fields (arguments, return value, skip, fault, inject) are
  shared, so each consumer sees what the ones before it changed. `want_return`, `keep`, `task`,
  `data`, `resumed` and `injected` belong to each consumer. They live in a table with one
  column per consumer beside the pending-call slots. A dispatch copies a slot's row into
  per-thread scratch, runs the consumers without the lock, and stores back what is still
  needed. The first 16 consumers need no allocation.
- **Holds.** A held call resumes at its holder (`resumed`), then reaches the consumers after it.
  The consumers before it keep their state from the first pass.
- **Drops** reach each consumer with data on a call exactly once, outside the lock.
  `forget_task` matches any consumer's task.
- **Removal** (`syscall_trap_consumer_remove`) drops the consumer's data on every call. A call
  it held for a fault resumes at the consumers after it. Removal is refused, with no change, while
  a call it holds is running an injected call. Removing the last consumer resets the core as the
  old `clear_consumer` did.
- **`SyscallTrap.id`** is a call id, the same at entry and return.
- **`SYSCALL_TRAP_OPS_SKIPPED_RETURN`.** A consumer with this flag gets the return of a skipped
  call, right after its entry. Penguin and the plugin bridge don't set it, so they behave as
  before.
- **`syscall_trap_set_arch(word)` / `syscall_trap_arch()`** carry a target word from a call's
  entry to its return (PPC `sc`/`scv`, MIPS o32 stack slot).
- **A dispatch abandoned by longjmp.** `qemu_plugin_set_pc` from an entry callback leaves the
  dispatch by longjmp. The next dispatch on that thread drops what the consumers before the one
  that left had on the call.
- **Penguin and the plugin bridge** are two consumers and can be active together. Penguin is
  `_MUTATE` at priority 0. The bridge is `_OBSERVE` at `INT_MAX`, last, because of the longjmp
  above. It is still allowed to change the call.

**L1** (`panda/src/syscall-events.c`, `include/panda/syscall-events.h`):
- **Events.** `syscall.enter` and `syscall.return` are declared at startup with
  `PANDA_SYSCALL_SIGNATURE`, `MUTABLE` and `INSTANCES`, for analysis, fuzz and rr modes (not KVM).
- **Dispatch.** The registry tells L1 when an event's active subscriptions change
  (`panda_event_set_watcher`, in `events-internal.h`). L1 keeps one RCU snapshot and one trap
  consumer per kind (early observers, mutators, observers). A kind's consumer exists only while
  it has an active subscription.
- **Watches.** A subscription is called only for the numbers it watches. A `syscall.enter`
  subscription may watch returns too, and is then called at both, with the same instance data
  and `PANDA_SC_AT_RETURN` set. A `syscall.return` subscription watches returns only.
- **Call records.** A call keeps the snapshot it started with, and its instance data, until it
  returns. The records come from a per-thread pool.
- **Refusals.** Mutator calls are refused with a reason (`panda_syscall_refusal()`) from an
  observer, outside the call's own callback, in the wrong phase, under KVM, while the analysis
  write policy forbids writes, and during record/replay.
- **Record/replay.** Syscall mutations are not logged. The events are declared without
  `RR_MUTATE`, so a mutator subscription is refused under record/replay. It is re-admitted after
  replay ends.
- **Exit calls.** `exit` and `exit_group`, per ABI number in L1, arm no return unless the
  subscription watches with `PANDA_SC_NORETURN_TOO`.
- **Skipped calls.** A skipped call's return reaches the subscriptions that want it, with
  `PANDA_SC_SKIPPED` and the value the mutator set.
- **Before vCPUs exist.** Plugins subscribe from `init_plugin`, before `qemu_init`. QEMU's RCU
  thread can't run callbacks then, so until the machine has vCPUs, the registry, L1 and the trap
  free what they replace at once (`panda_events_rcu_ready`).

Tests: `tests/static/syscall_trap.py` (flake check `syscall-trap`) runs the trap core against a
Python model, plus directed trap and L1 cases, under ASan and UBSan. `tests/static/penguin_detach.py`
checks Penguin's detach on the real core. `tests/syscall_events.py` runs C plugins on a busybox
x86_64 guest.

## L2: the `syscalls` plugin

- **core.** Tables per ABI, generated from syscalls2's `generated-in/*_prototypes.txt`: name,
  argument count, argument types and names. `syscalls_lookup(abi, num)` and
  `syscalls_by_name(abi, name)`. Typed decoders and string/buffer reads; a read that hits a
  non-present page holds the call and faults the page in.
- **events.** Per-syscall events named `<abi>.<name>.enter` and `<abi>.<name>.return`, with typed
  fields. Subscribing adds a watch on that number alone.
- **compat.** Generated PPP with syscalls2's names and C signatures (`on_sys_<name>_enter/return`,
  `on_all_sys_enter/return`, `on_all_sys_enter2/return2`, `on_unknown_sys_enter/return`), and
  `get_syscall_info`, `get_syscall_meta`, `get_syscall_retval`, `syscall_info_t`,
  `syscall_ctx_t`. A consumer moves by changing the plugin name in `PPP_REG_CB` and including
  `syscalls/syscalls2_compat.h`.
- **penguin.** igloo's hook model: hooks by name, on all or on unknown calls; pid, comm, scope,
  argument, return-value and exe filters, including string and fd-path matches; the
  read/read_str/write/syscall command protocol with fault-in holds; the guard, reaper and exec
  kernel hooks; stats and detach. Its C API is `syscalls_penguin_*`, with the hook, layout and
  stats structs of the engine's former `penguin-syscalls.h`.

Later table sources: the guest kernel's own BTF or ISF types and its `sys_call_table`.

## Tests

| Test | Proves |
|---|---|
| host model | L0 pairing, keys, restart, vfork, fault/inject/rewind, without a guest |
| cover | every number of every ABI on the arch enters and returns once, with its arguments and return |
| truth | trap events equal the guest kernel's `raw_syscalls` tracepoints |
| consumers | L1 observers, mutators and the plugin bridge together: order and isolation |
| rr | a recording with mutators replays with identical events |
| tables | generated tables equal the prototypes; names and argument counts equal tracefs |
| conformance | syscalls2 and `syscalls` compat produce the same callbacks on one replay |
| penguin | Penguin's hook tests against the plugin |
| budget | the cost table, against stock QEMU, per arch |
