# Syscall callbacks from the trap — design

Status: open questions answered, successor plugin added (2026-11-01); tests 1-3 written and passing on main's trap (2 and 3 with GAPs), on snapshotted test machines. Phase 1, first half done (2026-11-02): the trap's consumer list and two of the three trap fixes (the third withdrawn: see test 2) (`engine/src`, and the x86 patch: 0094 in this tree, 0100 in manager-bc's allocation); test 1 extended and passing; guest tests 2-3 pass on it with no GAPs (test 2: 47 checks; test 3: 32 checks, on 1 AMD and 4 Intel vCPUs, 3 of 3 runs); the guest suite's `syscalls` and `callbacks` and `replay_guard.py` (101 checks) pass. Next: 0101 (the PANDA callbacks).

## Summary

The engine's syscall trap becomes a PANDA callback source. Two new callback types,
`PANDA_CB_SYSCALL_ENTER` and `PANDA_CB_SYSCALL_RETURN`, are registered with
`panda_register_callback_modes` like any other new-style callback:

- **Observers** (`observe_only = true`) see each call as the kernel really runs it, once at
  entry and once at return. Any number may be registered. They work during record and replay.
- **Modifiers** (`observe_only = false`) may skip a call, change its arguments or return value,
  inject a call, or ask for a fault. They run before the observers. As for every other modifier
  callback, they are refused while recording or replaying.

The trap supports several consumers at once, so Penguin and PANDA plugins can run together. A new
`syscalls` plugin replaces syscalls2. It holds per-ABI tables generated from the kernel, a
subscription by name, and lazy argument decoders, so there is no per-syscall generated code and
no per-block instruction decoding. Return tracking needs no hooks.

Review is asked on:
1. the callback signatures and the `PandaSyscall` struct;
2. the ordering rules for observers and modifiers;
3. the per-plugin filter;
4. engine-owned per-call data, and how skipped calls are reported;
5. the phases.

## Why: measured cost

The benchmark ran on an x86_64 Debian 13 guest against stock QEMU 11.1.2. Each value is the
median of 6 runs, each starting from a fresh `loadvm`. All callbacks are C, on every syscall.

| | `dd`, 401k syscalls | 300× `/bin/true` | per syscall |
|---|---|---|---|
| stock QEMU | 0.84 s | 3.21 s | |
| engine, no plugins | 0.90 s | 3.15 s | |
| syscalls2, entry and return | 5.98 s | 5.07 s | ≈ 12.7 µs |
| trap, entry | 0.93 s | 3.20 s | ≈ 75 ns |
| trap, entry and return | 1.00 s | 3.27 s | ≈ 250 ns |

syscalls2's cost comes from its return tracking. On every call it adds a hooks-plugin hook at the
return address. Each `add_hook` reaches `tcg_hooks_mark_pc`, which always queues an exclusive
commit and invalidates the blocks there, even when the address is already hooked. That is one
stop-the-world section, one invalidation and one retranslation per syscall: 403,919 invalidations
for 403k calls. With no subscriber at all, loading syscalls2 still costs about 12% on the `dd`
workload, because it decodes every translated block to find syscall instructions.

## Current state

- `system/syscall-trap.c` (patch 0019) is called by the targets at syscall entry and at every
  return to user mode while a call is pending. This happens only on i386/x86_64 and arm/aarch64.
  It pairs a return with its call by task key, user stack pointer and return pc. It holds calls
  across a fault or an injected call, and it strips changes when `replay_analysis_write_allowed()`
  is false.
- **One consumer at a time** (`syscall_trap_set_consumer`). Today's consumers:
  - Penguin's syscall engine (`system/penguin-syscalls.c`), which changes calls and uses `task`,
    `data`, `keep` and `resumed`;
  - the QEMU plugin bridge (`plugins/syscall-system.c`), which exposes skip, args, ret, keep and a
    cookie.
- syscalls2 does not use the trap. It decodes the last instruction of every translated block and
  adds an instruction callback on syscall instructions. For returns it adds a hook through the
  hooks plugin, keyed by (return address, ASID).

## API

### Callback types and struct (`include/panda/syscall.h`, new)

```c
typedef struct PandaSyscall {
    uint32_t size;          /* sizeof(PandaSyscall) at the engine's build: fields are only added */
    int abi;                /* SYSCALL_ABI_*, as in qemu-plugin.h */
    int64_t num;
    uint64_t args[6];
    uint64_t pc;            /* the address after the trap instruction */
    uint32_t insn_len;      /* so pc - insn_len is the trap instruction */
    uint64_t id;            /* unique for the call's life, the same at entry and return */
    int64_t ret;            /* return: the value; entry: the value of a skip */
    bool restart;           /* return: the kernel is restarting the call */
    bool skipped;           /* return: no kernel call ran; ret is the skip's value */
    bool want_return;       /* entry, any registration: report this call's return to me */
    void *call_data;        /* this plugin's per-call bytes (panda_syscall_call_data), or NULL */
    /* modifiers only; an observer's writes to these are ignored */
    bool skip;
    bool fault;
    bool fault_write;
    uint64_t fault_addr;
    bool inject;
    int64_t inject_num;
    uint64_t inject_args[6];
    bool resumed;           /* set by the engine: a held call delivered again */
    bool injected;
    int64_t inject_ret;
} PandaSyscall;

/* panda_cb members */
void (*syscall_enter)(CPUState *cpu, PandaSyscall *sc);
void (*syscall_return)(CPUState *cpu, PandaSyscall *sc);
```

`panda_callback_supported_modes()` returns `ANALYSIS | FUZZ | RR` for both types on targets whose
code calls the trap, and 0 on the others. Registration then fails on mips/ppc/riscv/loongarch,
and syscalls2 falls back to its current path there. Neither type supports KVM.

### Filter (per plugin)

```c
/* flags: PANDA_SYSCALL_ENTER | PANDA_SYSCALL_RETURN; num -1: all; 0 flags: none */
bool panda_syscall_watch(void *plugin, int abi, int64_t num, unsigned flags);
```

- A plugin that registers a syscall callback and never calls `panda_syscall_watch` watches every
  number on every ABI of the target, for the types it registered.
- The trap's watch table is the union of every plugin's filter. A call no plugin watches costs
  what it costs today: one load at the trap and nothing at the return.
- Before calling a plugin's callbacks, the engine checks that plugin's own filter. Storage is
  2 bits per number per ABI, about 4 KiB per plugin on x86_64.

### Ordering

At **entry**, for each call:
1. Modifiers run in registration order. Each one sees the changes made by those before it.
2. If a modifier sets `fault` or `inject`, the delivery stops there and the call is held for it
   (its arguments and return changes, if any, are not applied). When the task comes back
   (`resumed`), that modifier gets the call again, then the consumers after it get it for the
   first time; those before it are not called again, and their changes are kept. A later
   modifier may hold it again the same way. If the holding modifier is removed meanwhile, the
   call goes straight on to the consumers after it. (As implemented in 0100: no "first wins"
   race arises, because the delivery is sequential.)
3. Observers run on the final pass only, in registration order. They get a copy with the final
   arguments, so each call the kernel really runs is observed exactly once.
4. `want_return` is the OR over all registrations: an observer may ask to track a call's return.
   That is tracking, not a change to the guest, so it is allowed during record and replay.

At **return**: the modifiers run first (they may change `ret`, or `fault`/`inject` as now), then
the observers with the final value.

A **skipped** call reaches no kernel, so no return comes from the target. The engine delivers the
return callbacks right after the entry, with `skipped = true` and `ret` set to the skip's value.
Observers therefore see every entry paired with a return, unless the call never returns: `exit`,
a successful `execve`, or a call the trap discards (below).

### Per-call data (engine-owned)

```c
/* Once per plugin, before its first syscall callback runs. 0: none (the default). */
bool panda_syscall_call_data(void *plugin, size_t size);
```

A plugin that saves something at entry for use at the return declares its size once. For every
call that plugin's callbacks see, `sc->call_data` points at that many bytes:
- zeroed at the entry;
- the same bytes at the return;
- freed by the engine when the call ends, whichever way it ends.

A pending call can be discarded without a return: its task is forgotten
(`syscall_trap_forget_task`), a snapshot is restored or the machine is reset
(`syscall_trap_cancel_all`), the pending table is full, or a stale entry is evicted. In each case
the engine frees the bytes with the call. There is no drop event: the plugin owns no per-call
memory, so it has nothing to free and nothing can leak. Reviewed 2026-11-01: this rather than a
drop flag.

The bytes are allocated only for calls that want a return. syscalls2 declares
`sizeof(syscall_ctx_t)` (about 170 bytes on x86_64).

### Penguin and the QEMU plugin bridge

Both stay as they are on the outside. Inside, `syscall_trap_set_consumer` is replaced by
`syscall_trap_add_consumer(ops, opaque, observe_only)` and `syscall_trap_remove_consumer`. The list has no fixed cap: an RCU list, with each pending slot's per-consumer state in a small array grown when a consumer is added. The PANDA
callback dispatcher is one consumer, Penguin another, the QEMU plugin bridge a third. Penguin and
the bridge register as modifiers. The ordering rules above apply across consumers, with the
PANDA modifiers placed among them in registration order.

Each pending slot keeps its consumer-private fields (`task`, `data`, `keep`) **per consumer**:

- A return is delivered to each consumer whose part of the slot is still pending.
- `keep` set by one consumer keeps only that consumer's part.
- The slot is freed when no part is pending.

This per-consumer state is the hardest part of the change (see [Risks](#risks)).

### Names

The new public API uses PANDA names: `PandaSyscall`, `panda_syscall_watch`,
`PANDA_CB_SYSCALL_*`, `include/panda/syscall.h`. `penguin_syscall_*` keeps its names. The trap's
internal `syscall_trap_*` is not exported in the SDK.

## The syscalls plugin (successor to syscalls2)

Reviewed 2026-11-01: the engine callbacks are the primary API. There is still a plugin with
syscalls2's purpose: names and decoded arguments, for C, Rust and Python users. Arguments are
described by tables and decoded lazily by helpers.

The split:
- **Engine (transport)**: the two callback types, the per-plugin filter by number, per-call
  data. It has raw numbers and register arguments only. It holds no names and no types.
- **Plugin `syscalls` (knowledge)**:
  - Per-ABI tables generated from the kernel sources: name, number, argument count, and for each
    argument its name and kind (`int`, `fd`, `pointer`, `string`, `buffer` with the index of its
    length argument, `struct` with a layout name, `flags`). Today's tables are stale: x86_64
    lacks 42 calls, including `clone3`, which Debian 13's libc uses. Generating from the kernel
    fixes that, and CI compares them against a kernel release.
  - Lookup: `syscalls_lookup(abi, "openat")` → number; `syscalls_describe(abi, num)` → the
    table entry.
  - A subscription API that wraps the engine callbacks, so a plugin can say
    `syscalls_watch(self, "openat", ENTER | RETURN)`. It resolves the name on every ABI the
    target has (x86_64 and i386 on x86_64) and calls `panda_syscall_watch`. The callbacks are
    still the plugin's own `PANDA_CB_SYSCALL_*` registrations, observer or modifier.
  - Lazy decoders, run only when called: `syscalls_arg_string(cpu, sc, i)`,
    `syscalls_arg_buffer(cpu, sc, i)` (its length from the table),
    `syscalls_arg_struct(cpu, sc, i)` (through the ISF layout when there is one), and
    `syscalls_format(cpu, sc)` for logs. At return, the entry's arguments are in the caller's
    own call data, saved there at entry.
  - Python: `@panda.syscall("openat", ret=True, observe_only=True)`. Arguments are decoded on
    access, by name: `call.args.pathname`, `call.ret`.
- **No per-syscall generated code** and no PPP callback names. A syscall nobody watches is never
  decoded; a watched one is decoded only as far as its callback reads.

In-tree users move to it: `proc_start_linux` (execve and auxv), `osi_linux`, pandare2's
`procname=` filter, and the Python tests that use `ppp("syscalls2", ...)`.

**Open**: syscalls2 itself, once its in-tree users have moved. Either it becomes a small
deprecated shim that maps its PPP names onto `syscalls`, for out-of-tree users, or it is
removed. That decision is not needed until phase 3.

## syscalls2 on the trap (interim, superseded by the plugin above)

On x86 and arm, when `panda_register_callback_modes(PANDA_CB_SYSCALL_ENTER, …, observe_only=true)`
succeeds:

- No `PANDA_CB_BLOCK_TRANSLATE` registration and no block decoding. It no longer needs the hooks
  plugin. It still needs `hw_proc_id`, for the ASID in its context.
- The entry callback picks the profile from `sc->abi`: on x86_64, `I386` uses the x86 profile
  (`int 0x80` or the vDSO); on aarch64, `ARM` uses the arm profile. It then calls the generated
  `enter_switch(cpu, profile, sc->pc - sc->insn_len, -1)` unchanged. On arm OABI it passes the
  call number from the instruction (`abi == SYSCALL_ABI_ARM_OABI`).
- The generated switch template's return block (`hooks_add_hook` + `running_syscalls.emplace`)
  becomes a call to the backend's `syscalls2_track_return(cpu, &ctx)`:
  - on the trap, it sets `sc->want_return` and copies `ctx` into `sc->call_data`;
  - on the old path, it adds the hook as now.
- The return callback runs `return_switch(cpu, sc->pc, sc->call_data)` as `hook_syscall_return`
  does. `double_return` stays as it is. syscalls2 no longer keeps `running_syscalls` on this path.
- A plugin argument `backend=trap|legacy` keeps the old path available on x86 and arm for
  comparison. The default is `trap`.

**Known difference.** The trap matches a return to its task. A `fork`/`clone` child returns in a
different task, so the trap does not report that return; syscalls2 doesn't today either (its
lookup key holds the parent's ASID). The conformance test below checks this rather than assuming
it. Reporting the child's return is a separate feature.

## Record and replay

- Observers of both types are `PANDA_MODE_RR`-compatible and run during record and replay.
- Modifiers are refused there by `callback_mode_valid`, as for every other modifier type. The
  trap's own stripping stays in place as a second check.
- This matches the guest-mutation design's non-goal: syscall-trap control effects stay refused
  during record and replay.

## Tests

The goal is that every system call the guest makes is reported, exactly once on entry and (when
asked) exactly once on return, with the right ABI, number, arguments, pc and return value. This
must hold on every entry instruction and return path the trap hooks, and while several consumers
change the call. The guest kernel is the reference: the tests check our events against what the
kernel recorded, not against our own decoder.

Entry and return paths the trap hooks today (patch 0019). Each must be exercised:

| arch | entry | ABI reported | return |
|---|---|---|---|
| x86_64 | `syscall` in 64-bit mode (`helper_syscall`) | x86_64 (x32 numbers too: bit 30 set) | `sysret`; `iret` when the kernel can't sysret (TF/RF set, changed rcx/r11, signal delivery, `execve`, `rt_sigreturn`) |
| x86_64 | `int 0x80` from a 64-bit task | i386 (32-bit table, truncated registers) | `iret` |
| x86_64 | `int 0x80` from a 32-bit task | i386 | `iret` |
| x86_64 | `sysenter` from the 32-bit vDSO (arg 6 on the user stack, return past the `int 0x80`) | i386 | `sysexit`/`iret` |
| x86_64 | `syscall` from a 32-bit task through the vDSO (AMD vendor CPUs; arg 2 in ebp) | i386 | `sysret` (32-bit) |
| aarch64 | `svc #0` from AArch64 EL0 | aarch64 | `eret` |
| aarch64 | `svc #0` from an AArch32 EL0 task, A32 and T32 (`len` 4 and 2) | arm | `eret` |
| arm (32-bit kernel) | `svc #0` (EABI); `swi 0x900000+n` (OABI) | arm, arm_oabi | exception return |

The other architectures (mips, ppc, riscv) have no trap. On them the tests check that
registering a syscall callback fails cleanly (the supported modes are 0), and that the `syscalls`
plugin says so rather than going quiet.

### 1. Host-only: the consumer list and slot state machine (`tests/static/syscall_consumers.py`)

No engine and no guest. This follows `static/penguin_detach.py` and `static/lifecycle.py`: compile
the real `syscall-trap.c` slot and consumer code against stubs, and drive it from a script.

- **Model check.** A small Python reference model of the slot states (pending, held enter/return,
  inject, inject-ret) and of per-consumer state (task, data, keep, want_return). The test runs
  seeded random sequences against the C code and the model, compares the delivered events and
  the slot table after every step, and shrinks any failure to a short trace. Events in a
  sequence: enter, return, restart, fault and resume, inject, skip, `forget_task`, `cancel_all`,
  slot eviction, consumer add/remove mid-call, and `want_return` set by an observer only.
  Defaults: 1–6 consumers (more than the old cap of 4), 10 000 sequences in CI, and a long run
  behind an environment variable.
- **Directed cases** (each a named test with the expected event list):
  - two modifiers: the first to fault owns the hold, and the resumed call goes back to it only;
  - observers get one delivery, on the final pass, and see the modifiers' final values;
  - writes to an observer's copy have no effect;
  - inject at entry and at return while an observer watches;
  - `-ERESTARTSYS` restart with two consumers;
  - `keep` set by one consumer only;
  - a skipped call gives an immediate return with `skipped` set, to every consumer that asked
    for a return;
  - a consumer removed between enter and return gets no return, and its data is freed;
  - an added consumer never sees a return for a call it didn't see enter.
- **Call data accounting.** Every allocation from `panda_syscall_call_data` is freed exactly once
  on every way out: the return, `forget_task`, `cancel_all`, eviction (stale or full table),
  consumer removal, and unload. The stub allocator counts, and the build uses ASan and UBSan.
- **Capacity.** 4096 pending calls plus one evicts, and the evicted call's return never pairs
  with a new call at the same (k1, k2, pc).

**Status (2026-11-02): extended for the consumer list (0100); PASS.** One to six consumers per
sequence (modifiers and observers, some with skipped-call returns, some without a return
callback), added and removed mid-sequence, each with its own decision on every event; every
delivery is compared, in order, with the consumer it went to. 23 directed cases (the 12 below,
updated, plus the multi-consumer ones listed above and the vfork and hold-argument cases), capacity, and 10 000 random sequences (1.58 million trap calls, 4 m 55 s). 17 bugs
planted in the new code (holder delivered twice, observer changes applied, a removed consumer's
parts kept, keep ignored, stack parts stored, the watch union overwritten, the vfork fix
undone, a removal that refuses nothing, and others): all 17 caught, each shrunk
to 4-7 commands. `static/lifecycle.py` and `static/penguin_detach.py` moved to the new API;
the second now checks that Penguin's detach leaves another consumer's calls, injection and
watches as they were.

Behaviour changes found while writing it: a return held for a fault comes back with the value
it was held with (so an earlier modifier's change survives the hold; the register is the same
in a real guest); and a removed consumer's drops run outside both of the trap's locks (an early
draft held the list lock, so a drop that called back in would deadlock).

**Earlier status (2026-11-01): written for main's single-consumer trap (patch 0019), the baseline; PASS.**
The C side is the real slot table and enter/return paths, compiled with ASan and UBSan and driven
through a pipe; the model is Python, from `syscall-trap.h`'s contract. They agree on 12 directed
cases, the capacity case (4097 calls: peak 3072 pending, 2046 evicted) and 10 000 random sequences
(1.37 million trap calls, 1 m 45 s). The model also checks that a call's data only comes back with
that call, and that all of it is released. Six bugs planted in the C code: five caught and shrunk
to 5-6 commands; the sixth (no release on the injected-return path when the consumer has no
return callback) is unreachable with one consumer, and becomes reachable with the list. With
0100 the model gains the consumer list (observers, modifiers, per-consumer data and
`want_return`) and the multi-consumer directed cases above.

### 2. Guest: every number through every entry (`tests/syscall_cover.py`)

A static guest program per arch (`tests/programs/sccover_<arch>.S`, built by `build.sh`, plus
i386 and AArch32 builds) installs a seccomp filter that returns `SECCOMP_RET_ERRNO` for every
call except the few it needs (`exit_group`, `write` to a result fd). Then, for each entry
instruction the arch has, it makes every number from 0 to the table's maximum plus 64. This
covers out-of-range numbers, the x32 range with bit 30 set, and -1. Each call uses distinct
recognizable values in all six argument registers, and in the arguments that live elsewhere
(i386 fast-path argument 6 on the stack). The kernel runs none of these calls, so none can hurt
the guest, and each returns a known `-errno`.

Checks, per call:

- enter exactly once, with the right ABI, number, all six arguments, pc and `insn_len`;
- return exactly once, with the same `id`, `ret == -errno`, and the pc after the instruction;
- the number of events equals the number of calls the program made, with nothing extra from the
  program's pc range.

The program then runs again with no seccomp filter, on a short list of calls whose return path
is forced:

- TF set before `syscall` (the iret return, with a SIGTRAP handler);
- `rt_sigreturn`;
- a successful `execve` (enter, no return to the old image);
- `exit` and `exit_group` (enter, never a return);
- `vfork`, `clone`, `clone3` and `fork` (the parent's return; the child's return is documented as
  not reported, and the test checks it isn't);
- a blocking `read` interrupted by a signal, with `SA_RESTART` (restarted: two enters, one return
  at the end) and without it (`-EINTR`);
- `nanosleep` interrupted and then continued through `restart_syscall`.

**Status (2026-11-01): written for x86_64, run against main's trap (patch 0019).**
`tests/syscall_cover.py`, `tests/programs/sccover_{x86_64,i386}.S`, `scpaths_x86_64.c`, and the
`sctrace` test plugin (`tests/plugins/sctrace`). The plugin records trap events as the trap's one
consumer; once 0101 lands it moves to the callbacks, and the test stays the same. The test makes
47 checks over 3,018 sweep calls plus the return-path cases, and takes 6 s; it passed 4 of 4 runs.
Covered: x86_64 `syscall` (0..599, x32, out of range), `int 0x80` from 64-bit and 32-bit tasks,
the 32-bit vDSO's `syscall` (AMD vendor), iret with TF set, restart, `-EINTR`, `restart_syscall`
after a stop, fork, vfork, a thread, clone3, and execve both failing and succeeding.

What it found in main's trap (GAPs in the test until fixed; the int 0x80 and vfork fixes are in 0100, and the test passes on it with no GAPs; the sigreturn fix is withdrawn):
- **`int 0x80` from a 64-bit task never gets its return.** `x86_sc_i386` keys the pending call
  with `uint32_t sp` (the 32-bit esp), and the return looks it up with the 64-bit rsp. All 600
  returns are lost, and each call is dropped when the next one arrives. Fix: key on the full
  register and use the 32-bit value only to read argument 6.
- **vfork reports the child's return as the parent's.** The child runs in the parent's address
  space on the parent's stack, so (k1, k2, pc) match. The parent's vfork appears to return 0, and
  its real return (the pid) is lost. Fix: for calls that vfork (vfork, clone/clone3 with
  `CLONE_VFORK`), the engine keeps the call pending across a 0 return (the trap's `keep`, applied
  by the engine). The parent is suspended until the child execs or exits, so the child's return
  always comes first.
- **`rt_sigreturn` holds a slot until the next one at the same address drops it.** It never
  returns, so this wastes slots and gives observers a drop. Fix: the engine knows the calls that
  don't return when they succeed (`rt_sigreturn`, `exit`, `exit_group`, and `execve`/`execveat`
  only when they succeed) and doesn't report a return for `rt_sigreturn`, `exit` and
  `exit_group`.
  **Withdrawn (2026-11-02):** these calls do return when the kernel refuses them: test 2's sweep,
  under a seccomp filter that answers every call with an error, got `-1234` back from each of
  them, and a tracer can do the same. Their returns must be reported, so they stay pending like
  any call; a real `rt_sigreturn`'s slot is reclaimed by eviction, or when a call at its key
  replaces it.
- **The header contradicts the code on `drop()`.** The code calls it for any data still set after
  the return, and Penguin relies on that; the header says it is called "without its return". This
  is moot with engine-owned call data, but the 0100 header must be right.

Not covered yet: `sysenter` (the kernel patches the vDSO for the CPU vendor at boot, so it needs
an image booted with an Intel-vendor CPU), the i386 return paths, `-smp 4`, and aarch64 (no image
built here yet).

### 3. Guest: the kernel as reference (`tests/syscall_truth.py`)

The guest's ftrace `raw_syscalls:sys_enter`/`sys_exit` records every call with its CPU, pid,
number, arguments and return. The test enables it, runs workloads, and compares the kernel's
records with the trap's events, CPU by CPU and in order. Both are serial per vCPU, so the order
must match exactly. The trace's reader runs after the workload, and its own calls are cut out by
pid.

Workloads:

- busybox and coreutils pipelines;
- a fork/exec storm;
- a multithreaded program (futex, `clone` with `CLONE_THREAD`, `exit_group` from a side thread);
- signals that interrupt blocking calls;
- a 32-bit program;
- `int 0x80` from a 64-bit task;
- (aarch64) an AArch32 program.

Each workload runs with `-smp 1` and `-smp 4`; with 4 vCPUs, tasks migrate between an enter and
its return.

Allowed differences: ftrace reports some calls the trap does not see at an instruction, and those
are listed explicitly. Two known cases are `restart_syscall` written by the kernel on signal
return, and kernel-internal calls on the `do_execve` path (kernel threads have no user entry).
Anything not on that list fails the test, with the first ten mismatches printed with their
context.

**Status (2026-11-01): written for x86_64, run against main's trap (patch 0019), on the test
machines below.** The workloads that exist: the 64-bit and 32-bit programs `sctruth_x86_64` and
`sctruth_i386` (200 rounds of real calls through `syscall`, `int 0x80` and the vDSO's entry, then
fork and wait), the return-path program from test 2, busybox pipelines, a fork/exec storm,
`scwork threads` and `scwork signals`. The 1-vCPU AMD machine runs the 64-bit `syscall` path; the
4-vCPU Intel machine also runs `sysenter` through the vDSO. Results:

- PASS on both, 32 checks, 4 s (1 vCPU) and 6 s (4 vCPUs, 3 of 3 runs). With 4 vCPUs the work is
  spread over all of them, and 54 returns came back on a different vCPU from their enter; all
  matched.
- The trap's enters match the kernel's exactly in every workload.
- GAPs (main's trap bugs, also found by test 2): the returns of `int 0x80` calls from a 64-bit
  task, and the vfork child's return, in each workload that vforks (busybox's `xargs`, and the
  return-path program).

What the first draft above got wrong about the kernel, now in the checker:

- `rt_sigreturn`'s `sys_exit` reports number -1, and an `execve` of a 32-bit program exits under
  the i386 number (11). The return check pairs an enter with the next event of the same pid,
  whatever its number.
- A thread killed by another thread's `exit_group` has a kernel exit and no user return. It is
  allowed only as that pid's last event.
- A call the kernel ends with -ERESTART*: the trap's return has `restart` set, carries -EINTR (what
  the task sees), or is missing (the task was killed). Exit, exit_group, a successful execve and
  the sigreturns have no return.

### 4. Guest: consumers together (`tests/syscall_consumers.py`)

The image's own fixture with real plugins: Penguin's syscall rules (a modifier, through
`penguin-syscalls.c`), the QEMU plugin bridge (a TCG plugin that skips one call), a C PANDA
modifier, and two PANDA observers (one C, one Python), loaded in different orders.

Checks:

- the observers see what the modifiers did:
  - a skipped `mkdir` has `skipped` set, and the directory doesn't exist;
  - a rewritten `openat` path is the file that gets opened;
  - a rewritten return value is the one the program sees;
- one injected `getpid` before a call: its own enter and return go only to its injector, and the
  observers see the task's call once;
- a Penguin fault hold on an unmapped buffer (the guest pages it in, then the call resumes) while
  an observer watches: one enter, after the resume, and one return;
- an observer that sets `want_return` gets returns that no modifier asked for;
- per-call data: one plugin's data never reaches another plugin;
- the data is freed when a task is killed with SIGKILL while blocked in a call, and on a snapshot
  revert taken while calls are held (allocation and free counters, read through a test-only
  query);
- slot pressure: 5000 threads blocked in `futex` (beyond the 4096 slots) get one return each, or
  are reported as evicted. A return is never paired with the wrong call; the test checks `id` and
  the arguments.
- a plugin unregisters (and another is unloaded) while calls are in flight: there are no later
  callbacks to it, and nothing crashes.

### 5. Record and replay (`tests/syscall_rr.py`)

The workloads from test 3 are recorded with observers attached. In replay, the observers' event
logs must be byte-identical to the logs from the recording. Modifier registration fails during
both record and replay, and Penguin rules are stripped as now. Setting `want_return` in replay
works.

### 6. The `syscalls` plugin (phase 2)

- **Tables against the guest kernel** (`tests/syscalls_tables.py`):
  - For each ABI the kernel has, read the `sys_call_table` (and the compat tables) from guest
    memory, and resolve each entry to its symbol through the ISF.
  - Every implemented entry must have our name and number. Ours must not name a number the
    kernel maps to `sys_ni_syscall`, unless it is listed as retired.
  - Argument kinds are checked against the kernel's syscall metadata (`__syscall_meta__*`, read
    the way `engine/tests/guest/sysprof.py` reads it): count, pointer or not, and `const char
    __user *` as a string.
  - This runs on every image, so a new kernel that adds calls fails here first.
- **Generator staleness** in CI: the generator is run again on the pinned kernel sources, and
  the output is diffed against the tables in the tree.
- **Decoders**: the existing `tests/syscalls.py` fixture (`clone3`, `rseq`, `close_range`,
  `faccessat2`, `fchmodat2`) moves to `@panda.syscall`. `syscalls_arg_string`,
  `syscalls_arg_buffer` (length from the paired argument), `syscalls_arg_struct` (through the
  ISF) and `syscalls_format` must give the program's known values. Faulting pointers come back
  as errors, not garbage.
- **Conformance with syscalls2** while it still exists: one fixture, with both plugins loaded.
  They must report the same (name, number, arguments, return) for every call both know.
  syscalls2 being wrong is reported, not hidden: its table gaps from the summary are listed
  explicitly.

### Test machines (`tests/testmachine.py`)

Tests 2-5 need a guest that takes a program and runs it, and should not wait for a boot.
`testmachine.Machine(image, smp=1, intel=False, full=False)` gives one with a snapshot. The
snapshot is built once, cached next to the image (`machines/<arch>-<tiny|full>-smp<N>[-intel]-<key>/`),
and rebuilt when its inputs change. Two kinds:

- **tiny** (default): the image's distribution kernel (from the distribution's archive, checked
  against its SHA256) with an initramfs that holds static busybox and the agent as `/init`. No
  disk. The snapshot takes 7-9 s to build and is 94 MB.
- **full**: the image's own distribution on an overlay of its disk, cold-booted (the image's
  `root` snapshot is in the backing file, which `loadvm` can't use) with the agent started as
  root. About 90 s to build, about 300 MB.

The host talks to the agent (`tests/programs/agent_x86_64.c`) through hypercalls, not the
serial console: `out 0x88` with a magic number; the agent polls for requests, and the host writes
each request into the agent's 4 MiB buffer through the logged guest-write API. Requests are put
(a file), run (`sh -c`, with exit status and output) and get (a file). `Machine.revert` loads the
snapshot and waits for the agent. Each `Machine.panda()` copies the snapshot, so several runs
don't share a file lock.

Measured (x86_64 Debian 13 kernel, under TCG):

| machine | build | revert | put 9 MiB | get |
|---|---|---|---|---|
| tiny, 1 AMD vCPU | 9 s | 0.45 s | 0.23 s | 0.04 s |
| tiny, 4 Intel vCPUs | 9 s | 0.54 s | | |
| full, 1 AMD vCPU | 90 s | 1.78 s | 0.20 s | 0.04 s |
| full, 4 Intel vCPUs | 80 s | 1.61 s | 0.16 s | 0.04 s |

The first truth run sent ftrace's output over the serial console. It took more than 25 minutes
and was stopped. On a tiny machine through the agent, the whole test takes 4-6 s.

`testmachine.py <image json> [--smp N] [--intel] [--full]` builds the machine and runs a smoke test
(put, run, get, round trip). Not yet: an aarch64 agent (and a busybox cross build), and moving
`syscall_cover.py` onto the machines.

### Where they run

| test | needs | CI |
|---|---|---|
| `static/syscall_consumers.py` | C compiler, ASan | every PR (`nix build .#checks.x86_64-linux.syscall-consumers`) |
| `syscall_cover.py`, `syscall_truth.py`, `syscall_consumers.py`, `syscall_rr.py`, `syscalls_tables.py` | an image | `run.sh` default list; x86_64 and aarch64 images (AArch32 and i386 programs ride along) |
| trap benchmark (`bench.py` variants, from the measurements above) | x86_64 image, quiet cores | by hand; the results go in the PR |

Images: only debian-13-x86_64 is built here so far. aarch64 needs `pandare2.mkimage
debian-13-aarch64` (about 6 minutes, run in a quiet window). The arm 32-bit kernel and OABI rows
have no image yet. Those rows stay untested until there is one, and the table above says so.

## Phases

1. **Engine: multi-consumer trap and the callback types.**
   - Consumer list and per-consumer slot state in `syscall-trap.c`.
   - `PANDA_CB_SYSCALL_ENTER`/`RETURN`, `panda_syscall_watch`, `panda_syscall_call_data`,
     `include/panda/syscall.h`.
   - Penguin and the QEMU plugin bridge moved onto the list.
   - Tests (see Tests): written first, against the current single-consumer trap where they
     apply, so they show the baseline. Then 1 (host-only model check), 2 (cover), 3 (truth),
     4 (consumers together) and 5 (record/replay). The existing Penguin syscall guest tests
     must also pass unchanged. The trap benchmark is rerun through the callback path.
   - One or two engine patches; numbers from manager-bc.
2. **The `syscalls` plugin**: generated tables with a CI staleness check, lookup, subscription
   and lazy decoders (C), and `@panda.syscall` (Python). Then migrate the in-tree users, and
   decide syscalls2's shim or removal. Tests: section 6 (tables against the guest kernel,
   staleness, decoders, conformance).
   - Until that lands, syscalls2 can optionally go on the trap as an interim step (below), if
     its users need the speed sooner.
   - The steps listed under this item were the plan for that interim step:
   - Backend split, template change, regenerated switches.
   - Conformance test: the same guest fixture under `backend=trap` and `backend=legacy` gives the
     same sequence of (number, arguments, return value) for every PPP callback type syscalls2
     users rely on (`on_all_sys_*`, the specific `on_sys_*_enter/return`, `on_unknown_*`).
   - Benchmark against the baseline above. Expected: `dd` from 5.98 s to about 1.0–1.1 s with a
     return subscriber, and syscalls2's idle 12% gone.
3. **`tcg_hooks_mark_pc` deduplication** (separate small engine patch).
   - Don't commit an address that already has a site whose blocks were invalidated once.
   - This helps the old path on the other architectures and every other repeated `add_hook`.
4. **pandare2**: `@panda.syscall_enter` / `@panda.syscall_return` decorators with `observe_only`,
   on the generic wrapper. Later, the hooks plugin on engine hooks.

## Risks

- **Per-consumer slot state** interacts with fault holds, injected calls (`SC_INJECT`,
  `SC_INJECT_RET`) and restarts. Only one modifier may own a hold or an injection, so holds stay
  single-owner. But the observers' and the other consumers' parts of the slot must survive the
  rewind and be delivered once, on the final pass.
  - This needs targeted tests: a fault hold and an injection while an observer watches the same
    call, a restart (`-ERESTARTSYS`) with two consumers, and `keep` by one consumer only.
- **Slot capacity**: 4096 pending calls are shared by all consumers, as now.
- **ABI**: adding two `panda_cb_type` values moves `PANDA_CB_LAST`. The cffi and Rust bindings
  are generated, so they follow. Native plugins built against an older header must be rebuilt;
  1.0 is a declared break.

## Open questions

None.

Resolved:
- Observers may set `want_return` (tracking only). Reviewed 2026-11-01: yes.
- No fixed cap on trap consumers. Reviewed 2026-11-01: a cap of 4 is not enough.
- No drop event: engine-owned per-call data instead. Reviewed 2026-11-01.
