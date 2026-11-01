# Guest mutation API for record/replay — design

Status: reviewed 2026-10-31 ("good. build it."). Phase 1 is built and guest-tested (2026-11-01): every flake check passes, and the record/replay guest tests pass on x86_64, aarch64 and riscv64. See [Phase 1 as built](#phase-1-as-built).

## Summary

During a recording, changing the guest is allowed only through an explicit API. The engine logs each write as a synchronous replay event, placed at the instruction where the callback ran. Replay does not run mutating callbacks; it applies the logged writes at the same point. A write made any other way is refused with an error. It is never silently dropped and never skipped.

Review is asked for on:
1. the API surface (C, Python, Rust);
2. the event and its placement rules;
3. the callback sites that can and cannot host writes, which narrows "any callback" (see [Where writes may happen](#where-writes-may-happen));
4. the delivery plan.

## Goals and non-goals

Goals:
- One API for guest writes during record/replay:
  - memory: virtual and physical RAM;
  - registers: core, FP/vector, SVE, and system registers where the target can write them;
  - any field of a vCPU's `CPUArchState` (the `env_ptr` structure) by offset, including nested structs and arrays;
  - the hypercall return value.
- Structured writes are built on a few primitive ops. Python and Rust write a guest struct through a guest pointer, or an env field by name; the log stores only bytes at a location.
- Each write is logged at its instruction. Replay reproduces the guest state exactly, without the plugin that made the write.
- Writes that bypass the API are refused or, where they can't be intercepted, detected and fail the recording.
- Penguin and PANDA plugins use the same API.

Non-goals:
- **Old replay logs.** No compatibility; 1.0 and this work are a declared break.
- **KVM.** Record/replay is TCG only.
- **Penguin MMIO reads.** Refused during record/replay for now; a later event records their answers ([Refusals](#refusals)).
- **Syscall-trap control effects** (skip, inject, fault), reset and map_memory. These stay refused. If they are wanted later, each becomes its own op kind in the same event.
- **Device MMIO writes from analysis.** Refused, as 0091 already does.

## Current state

There are four mechanisms in the stack today. They overlap, and none is the general answer.

| Mechanism | Where | What it does | Limit |
|---|---|---|---|
| Guard | #33 (0084) | Refuses guest writes and mutating callbacks during record/replay. | Everything that mutates is refused. |
| Deferred effects | #34/#35 (0091, 0092) | `panda_analysis_ram_write` and `panda_analysis_reg_write` queue a write from the main thread, with vCPUs stopped. The write is applied and logged at the next async checkpoint. | Can't be used from a vCPU callback. The write lands later, not at the callback's instruction. |
| Hypercall event | Slice H (0093, allocated 0095) | One `EVENT_HYPERCALL` per hypercall. The handler's writes through `panda_mem`, `penguin_write_guest_reg` and `panda_set_retval` are logged. Replay skips the handler and repeats the writes. | Hypercalls only. Writes made directly into CPU state are caught by diffing the 'g'-packet registers, which is implicit logging. |
| Skip mutators | #39 (held) | Replay doesn't run mutating callbacks. | Nothing is logged, so the replay diverges. This contradicts never-skip. |

The proposal generalises slice H: its scope, event and replay path become the API's core. Slice H's register diff is dropped: writes outside the API are unsupported, not diffed.

## API surface

### C core (`include/panda/guest-write.h`)

```c
typedef enum {
    GUEST_WRITE_OK = 0,
    GUEST_WRITE_EPERM,    /* no mutation scope: observer, replay, wrong thread or site */
    GUEST_WRITE_EINVAL,   /* bad register number or length, bad vCPU */
    GUEST_WRITE_EFAULT,   /* address doesn't translate, or isn't plain RAM */
    GUEST_WRITE_EACCES,   /* the target refused the register write (logged as denied) */
    GUEST_WRITE_E2BIG,    /* the event would exceed its cap (16 MiB, as slice H) */
} GuestWriteResult;

GuestWriteResult panda_guest_write_virt(CPUState *cpu, vaddr addr, const void *buf, size_t len);
GuestWriteResult panda_guest_write_phys(hwaddr addr, const void *buf, size_t len);
GuestWriteResult panda_guest_write_reg(CPUState *cpu, int regnum, const void *buf, size_t len);
GuestWriteResult panda_guest_set_retval(CPUState *cpu, uint64_t value);   /* hypercall/syscall answer */

/* Raw bytes into the vCPU's CPUArchState at a byte offset (offsetof(CPUArchState, ...)).
 * Refused (EINVAL) if the range leaves the struct or overlaps a host-pointer field. */
GuestWriteResult panda_guest_write_env(CPUState *cpu, size_t offset, const void *buf, size_t len);
/* Write through host pointers reachable from env. Starting at env, for each derefs[i]:
 * p = *(void **)(p + derefs[i]) (NULL: EFAULT); then write len bytes at p + offset.
 * panda_guest_write_env is the n == 0 case. Every step must be a pointer field of the
 * type reached so far, and the final range must fit the pointee (layout map, below). */
GuestWriteResult panda_guest_write_env_path(CPUState *cpu, const uint32_t *derefs, size_t n,
                                            size_t offset, const void *buf, size_t len);
/* Optional: recompute what QEMU derives from env (arm hflags, TLB flush, ...) after raw writes. */
void panda_guest_env_sync(CPUState *cpu);

/* Register discovery, so callers needn't hardcode gdbstub numbers. */
int panda_guest_reg_lookup(CPUState *cpu, const char *name, size_t *len);  /* -1 if unknown */
```

- **Register names.** Registers are named by their gdbstub register number, as 0092 and Penguin's `penguin_write_guest_reg` already do. The gdbstub XML features cover core, FP/vector and SVE registers on every target, so no per-arch table is needed. `panda_guest_reg_lookup` maps a feature name such as `x0`, `xmm3` or `z7` to its number and size.
- **System registers.** Not every target can write them through gdbstub: ARM system registers and aarch64 pointer-authentication keys refuse today, and x86 MSRs beyond the gdb set are missing. Phase 4 adds an engine-side writer for each target's listed system registers. Until then, those writes return `EACCES` and are logged as denied, as 0092 does.
- **Env writes: two levels.** There are two ways to write a register.
  - `write_reg` goes through QEMU's setter, so derived state stays consistent.
  - `write_env` copies bytes, as `penguin_cpu_env` users and pandare2's `cpu.env_ptr` do today. QEMU's derived state (arm hflags, x86 lazy flags, softfloat status) is not updated unless the caller runs `panda_guest_env_sync`.
  
  Determinism doesn't depend on that consistency, because replay copies the same bytes at the same point. What makes it safe:
  - **TLB flush and TB exit.** After any env op the engine flushes the vCPU's TLB and leaves the TB, in record and replay alike. Stale cached translations then can't differ between the two runs, which may have different plugins.
  - **No host pointers.** A host pointer value (a field like `env->...->` or `cpu->opaque`) differs between record and replay processes because of ASLR. Ranges overlapping pointer fields are refused. The pointer-field map for each target is generated from the bindgen layout the Rust bindings already produce.
  - **Layout hash.** The recording header stores a hash of each target's `CPUArchState` layout. Replay refuses a recording made with a different layout.
- **Pointers.** There are two kinds, handled differently.
  - **Guest pointers** are read freely, since reads are deterministic. A struct reached through one, such as a task_struct, is written with `write_virt` at the resolved address.
  - **Pointers to pointers in guest memory** work the same way: each pointer is read, and the final address is the one written and logged. Guest memory is identical in record and replay at that instruction, so the logged address is valid in both.
  - **Host pointers inside env, and pointers to pointers** (for example ARM's PMSA region arrays, `env->pmsav7.drbar`, and the per-bank arrays of pointers in `env->pmsav8`) are written by **path**, not by address. A host address means nothing in the replay process, but the path from env to the field does.
    - The log stores the path: the vCPU, the offset of each pointer to follow, then the final offset and the bytes.
    - Recording follows the path, writes and logs it. Replay follows the same path in its own process, at the same instruction, and writes there.
    - Following a path at the same guest point in both runs reaches the same logical object, even if QEMU reallocated it in between: replay's QEMU state matches the recording's there.
    - **Validation.** Each step must land on a pointer field of the type reached so far, and the final write must fit inside the pointee. The generated layout map records each pointer field's pointee type. Runtime-sized arrays (`drbar` holds `pmsav7_dregion` entries) take their length from a short hand-written table of length fields; a pointer field in neither place can't be written.
    - **Repointing.** Changing a pointer's value, so that it points somewhere else, stays refused. There's no way to express the new target in a form valid in both processes.
    - **Roots beyond env.** A path can start anywhere that both processes can name the same way. The call becomes `panda_guest_write_path(root, derefs, n, offset, buf, len)`, and env is one kind of root:

      | Root | Identity in the log | Resolved with |
      |---|---|---|
      | vCPU env / CPUState | cpu_index | `qemu_get_cpu` |
      | Device or other QOM object | canonical QOM path (`/machine/peripheral/foo`, `/machine/unattached/device[3]`) | `object_resolve_path` |
      | RAM block not mapped at a guest address (VRAM, device buffers) | RAMBlock idstr + offset | `qemu_ram_block_by_name` |
      | Plugin-owned object the guest can see | name registered at plugin init (`panda_guest_root_register("penguin.x", ptr, type)`) | registry lookup; replay must register the same name |

      - **Thread safety.** Non-CPU roots need the BQL held around the write, because device code runs on other threads. The engine takes it in both modes.
      - **Recording.** The roots a recording used go in its header. Replay refuses to start if one doesn't resolve.
      - **Layout source (decided 2026-10-31).** The layout map is bindgen/cffi. Anything nameable there is writable, derived state included. vmstate was considered and rejected: it would exclude fields that aren't migrated.
        - **Device roots carry two names.** The log records the QOM path, the QOM type name and the C struct name. Resolving checks `object_dynamic_cast(obj, qom_type)` and that the instance size covers the struct. A path that resolves to an object of another type is an error, not a reinterpretation.
        - **Bindgen covers every device, generated.** Coverage isn't driven by Penguin; the API is for any analysis, so device types aren't added one by one.
          - **Which headers:** for each target, the build takes the headers of every device compiled into it.
          - **QOM type to C struct:** scraped from QEMU's own declarations, `OBJECT_DECLARE_TYPE`, `OBJECT_DECLARE_SIMPLE_TYPE` and `DECLARE_INSTANCE_CHECKER`. For example, `OBJECT_DECLARE_SIMPLE_TYPE(PL011State, PL011)` gives `TYPE_PL011` to `PL011State`.
          - **Packaging:** the result is a separate device-layout module, so the main bindings don't grow.
          - **Check:** a startup check compares each generated struct's size with its QOM type's `instance_size`. A mismatch disables that type for writes.
          - **Limit:** a device whose state struct is defined only in its `.c` file isn't nameable. Many QEMU devices do this. Those are out of reach unless the struct is moved to a header, which would be an engine patch per device.
        - **Array lengths.** Bindgen doesn't know runtime array lengths, so the hand-written length table stays. Each entry names the field that holds the length, for example `pmsav7.drbar: pmsav7_dregion`.
        - **Device derived state.** Raw writes to a device can leave its derived state stale, such as timers or IRQ lines. Replay reproduces that exactly, so determinism holds. Whether the device behaves as the writer intended is the writer's concern, as with `env`. There is no generic device sync.
      - **First roots:** vCPU env and device state (QOM path). RAM-block and plugin-registered roots wait until there's a user.
- **No rr.** With no recording, each call writes immediately and logs nothing. The cost is one branch.
- **Existing entry points become wrappers.** `panda_virtual_memory_write`, `panda_physical_memory_write`, `panda_set_retval`, `panda_set_syscall_arg` and `penguin_write_guest_reg` call the API, so existing callers are logged without changes. Their return codes are unchanged; the detailed result is available from the new calls.

### Python (pandare2)

- `panda.virtual_memory_write`, `panda.physical_memory_write`, `panda.arch.set_reg`, `panda.arch.set_retval` and `panda.arch.set_arg` are routed through the API.
- A refusal raises `GuestWriteError`, with subclasses per result code. Today some of these fail silently.
- **Registration.** During record/replay, every callback must declare `observe_only=True` or `mutates=True`; a registration with neither is refused. This is what makes mutation during a recording "a very clear thing". Outside record/replay, the current default is kept.
- **New spellings.** `panda.guest.write_virt`, `panda.guest.write_phys` and `panda.guest.set_reg(cpu, name_or_num, value)` are added for code that wants the explicit names.
- **Env proxy.** `panda.guest.env(cpu)` is a proxy over the cffi `CPUArchState`. Reads pass through. Each assignment (`env.regs[0] = 1`, `env.cp15.ttbr0_el[1] = x`, `env.vfp.zregs[3].d[0] = y`) resolves its offset with `ffi.offsetof` and calls `panda_guest_write_env`. A pointer-typed field gives back another proxy, not a raw cffi pointer, and that proxy carries the path. `env.pmsav7.drbar[3] = x` becomes path [offsetof(pmsav7.drbar)], offset 12. A pointer to pointers adds a step: `env.pmsav8.rbar[M_REG_S][2] = x` becomes path [offsetof(pmsav8.rbar[1])], offset 8. Assigning to a pointer field itself raises. `cpu.env_ptr` itself becomes read-only during record/replay, so the old spelling fails loudly instead of slipping past the log.
- **Guest structs.** `panda.guest.struct(cpu, 'task_struct', addr)` resolves fields through the ISF (or a cffi type). Pointer fields read as typed views (`.mm.pgd`), and every field assignment is a `write_virt`.

### Rust (panda-bindings and the safe crate)

- The autogen bindings pick up the C calls when they are regenerated.
- The safe layer adds a `GuestWriter` handle, passed only to callbacks registered as mutating. Its methods return `Result<(), GuestWriteError>`. An observer never receives a writer, so the type system enforces at compile time what C enforces at run time.
- Env fields are written with `env_write!(writer, cpu, regs[0], value)`. The macro computes `offset_of!` against the bindgen `CPUArchState` and checks the value's type. `=>` marks a pointer to follow: `env_write!(writer, cpu, pmsav7.drbar => [3], value)`, or `pmsav8.rbar[1] => [2]`.

## Event and log format

A mutating callback invocation that writes something produces one `EVENT_GUEST_WRITE`. This replaces slice H's `EVENT_HYPERCALL`, which becomes the hypercall site's case.

```
EVENT_GUEST_WRITE
  u16  site          callback type (hypercall, before_block_exec, syscall_enter, ...)
  u32  cpu_index
  u8   flags         hypercall sites: handled
  u64  retval        hypercall sites only
  u32  op_count
  ops[]:
    u8 kind          VIRT | PHYS | REG | ENV
    u8 result        OK | EFAULT | EACCES   (the write's own result while recording)
    u64 addr / i32 regnum / ENV: u8 n, u32 derefs[n], u32 offset
    u32 len, bytes
```

Placement and ordering:
- **Where it sits.** The event is saved at the instruction count where its site fires. It uses the synchronous pattern QEMU uses for clock and random-number reads: recording calls `replay_save_instructions()` then `replay_put_event()`; replay calls `replay_account_executed_instructions()` then `replay_next_event_is()`.
- **One event per site invocation.** All mutating callbacks at one site share one event. Within a site, observers run first, then mutators, in registration order, in both record and replay.
- **Empty invocations log nothing.** An invocation with no writes saves no event, apart from hypercall sites, which always save one so their answer is replayed. Replay checks for an event at every invocation of a site that the recording listed as mutating. Where nothing was saved, the next event is something else and the check is cheap.
- **Sites are listed in the header.** The recording header lists the site types that had mutating callbacks at any point in the recording. Replay instruments exactly those sites, whether or not a plugin is loaded, so its check points exist.
- **The TB ends after any write.** If the event has any op, the vCPU leaves the TB after the site, in record and in replay. A register write can change the PC or hflags, and a memory write can hit the current TB's code. Ending the TB in both modes keeps the effect deterministic. Writes are rare, so the cost is small.

## Record and replay semantics

| Step | Record | Replay |
|---|---|---|
| Site fires | Observers run, then mutators inside a mutation scope. | Observers run. Mutators are not called. |
| Writes | Each API call checks the scope, makes the write and appends an op with its result. | Each logged op is applied with the same call, so translation side effects happen again too (TLB fills, page-table accessed/dirty bits). |
| End of site | Saves the event if it has ops. Exits the TB if it has ops. | Any op whose result differs from the logged one is a fatal divergence error. Exits the TB if there were ops. |
| Answer (hypercall) | handled and retval logged. | Returned from the log. |

- **Mutators registered at replay.** They are allowed, but not called. Slice H's test covers this: "replay-mutator: no handler ran".
- **API calls during replay.** Refused with `EPERM`. Only the engine's own apply path writes.
- **Deferred effects from #34/#35.** These stay as the main-thread path, under clearer names (`panda_guest_write_*_request`). They are applied at an async checkpoint. Checkpoints and synchronous events are both placed by instruction count in one log, so their order is fixed without extra work. A synchronous call made off the vCPU thread returns `EPERM`; it is not converted silently. Open question 2 covers whether to merge the two.

### Where writes may happen

"Any callback" holds for every callback that runs at a deterministic point in guest execution. Two kinds of callback don't, and can't host synchronous writes:

| Site kind | Examples | Rule | Phase |
|---|---|---|---|
| Hypercall | `guest_hypercall`, Penguin hypercalls, `@panda.hypercall` | Synchronous event (slice H) | 1 |
| Block boundary | `before_block_exec`, `start_block_exec`, `after_block_exec` | Synchronous event | 2 |
| Syscall trap | Penguin syscall enter/return hooks, syscalls2 | Synchronous event | 2 |
| Instruction | `insn_exec`, hooks plugin at an address | Synchronous event. The hook's instruction starts I/O, which ends the TB under icount, as slice H does for hypercalls. | 3 |
| Mid-instruction | memory read/write callbacks, `before_handle_exception` | Refused permanently (`EPERM`). The instruction can fault and restart, so the point is not well-defined. | — |
| Translation time | `before_block_translate`, `insn_translate` | Refused permanently. When translation happens depends on the TB cache, not on the guest. | — |
| Main loop or device | `main_loop_wait`, `after_machine_init`, unassigned I/O | Deferred request only (#34/#35 path) | 1 |

## Refusals

Intercepted writes, which the API refuses with an error:
- every memory write path in `panda_mem.c` and `syscall-trap.c`, plus Penguin's `penguin_syscall_write_memory`;
- every register setter in `panda_arch.c`, plus `penguin_write_guest_reg`;
- `set_pc`, which is refused until phase 3 adds a PC op. Its semantics are fixed by the TB-exit rule above.

Writes that can't be intercepted are unsupported, not detected (decided 2026-10-31; see [Phase 1 as built](#phase-1-as-built)):
- **Raw `CPUArchState` writes outside the API.** These come from `penguin_cpu_env`, pandare2's cffi `env` fields, C plugins writing `env` directly, or target helpers that rewrite env. Nothing logs them, so a replay of that recording diverges. The API covers every write, with or without record/replay, so such a write is the caller's mistake.
- **Host pointers into guest RAM** (`panda_map_memory`, `qemu_map_ram_ptr`). These stay refused during record/replay, as in 0084.

Penguin MMIO reads:
- A Penguin MMIO region's read callback is answered by the plugin, which makes it a nondeterministic input.
- During record/replay, registering such a region, or having one already registered, refuses the recording with a clear error.
- A later slice records the answers as an event, the same pattern as hypercalls.

## Migration

Penguin:
- **Memory and register writes.** `penguin_syscall_write_memory` and `penguin_write_guest_reg` keep their signatures and route through the API. They already log in slice H.
- **`penguin_cpu_env` writes.** Penguin's register writes through `penguin_cpu_env` move to `penguin_write_guest_reg`, which adds `penguin_reg_lookup` by name.
- **Syscall hooks** that skip a syscall or rewrite its return use the syscall-trap site and the API. Skip, inject and fault stay refused under record/replay.
- **Registration.** Penguin's consumer registers as mutating explicitly.

PANDA plugins:
- **Hooks and wrappers.** Plugins that already write through `panda_*` wrappers are logged with no change. They only need to declare themselves mutating for record/replay (hooks, hypercaller's mutator bridge).
- **Direct `env` writers** in our own code move to the API (pandare2 extras in phase 1, Penguin in phase 2).

PANDA vs Penguin hypercall differences, resolved toward Penguin where rehosting depends on it:

Penguin's guest library (`guest-utils/native/hypercall.h`) uses each target's Linux syscall registers:

| Target | Number register | Return register |
|---|---|---|
| x86_64 / i386 | rax / eax | rax / eax |
| aarch64 | x8 | x0 |
| arm32 (and AArch32 on aarch64) | r7 | r0 |
| mips | v0 | v0 |
| ppc | r0 | r3 |
| riscv64, loongarch64 | a7 | a0 |

Checked 2026-11-01, question 5: hypercaller already matches. It reads the magic with `panda_get_syscall_arg(cpu, 0)`, whose index 0 is the syscall-number register in every target's table (`panda_arch.c`), plus r7 for AArch32 on aarch64. Since 0045, the guest's hypercall is Penguin's instruction on every target, and the helper returns Penguin's return register. The earlier table here, which listed syscall argument 0 as a0, was wrong; no register move is needed.

The check did find one bug, on the return side. On aarch64 in AArch64 state, `panda_guest_set_retval`, and so C's `panda_set_retval` and Rust's `GuestWriter::set_retval`, wrote `env->regs[0]`, the AArch32 view, not x0; `panda_get_retval` read it. A guest never saw the value. pandare2's `arch.set_retval` writes X0 by name, which is why `hypercall_rr` passed. Both now use x0 in AArch64 state, and `hypercall_rr` sets the return through both paths on alternate calls.

## Delivery plan

Each patch takes a number from manager-bc. Nothing is pushed without approval.

1. **API core and hypercall site** (folds in slice H; its 0095 allocation carries over):
   - `guest-write.h` and the API;
   - `EVENT_GUEST_WRITE` replaces `EVENT_HYPERCALL`, with the header site list;
   - slice H's logged diff is dropped;
   - pandare2 `GuestWriteError` and `mutates=`;
   - Rust bindings and `GuestWriter`;
   - deferred requests renamed.
   
   Tests: slice H's tests, plus a static test that `gdb_num_g_regs > 0` on every target. A fake count is what hid the slice H bug.
2. **Block-boundary and syscall-trap sites; Penguin migration** (`penguin_cpu_env` writers, MMIO-read refusal). Guest tests on x86_64, aarch64 and riscv64.
3. **Instruction sites** (`insn_exec`, hooks) **and PC writes.** #39's `rr_mutators` test is rewritten here against the API, with icount on to avoid the host-clock livelock found on aarch64.
4. **System-register writers, per target.**

Path writes land with the phases:
- **Phase 1:** env roots, including paths through pointers. Also the generated layout map and the length table.
- **Phase 2:** device roots, with the generated device-layout bindings for every device compiled into each target.
5. **Penguin MMIO read answers as an event.**

Independent of the plan:
- `penguin` capability: dropped from #32 locally. #33–#35 are rebased onto it; push script follows for approval.
- #39: stays held. Close it when phase 1 lands; its hypercaller bridge is already in slice H.
- riscv hypercall row: reproduce before phase 1, because it decides whether phase 1's guest tests can include riscv64.

## Phase 1 as built

Slice H (hypercall answers) is folded in, and the whole is engine patch 0093. Where it differs from the design above:

- **`panda_guest_env_sync` is logged.** It is a `SYNC` op, so replay repeats it.
- **The layout hash travels in each event, not in a recording header.** Replay compares it when an event has env ops. The header site list waits for phase 2: the hypercall site always saves an event, so phase 1 has nothing to list.
- **No guard.** The first build compared env (and the arrays it owns) at the end of each recorded site with what the logged writes explain, and stopped the recording on any other change. It was removed: writing guest state outside the API is documented as unsupported (guest-write.h, the record/replay guide, penguin.h), and the API is cheap enough to use everywhere (`PANDA_GUEST_ENV_SET` in C, `panda.guest.env(cpu)` in Python, `env_write!` in Rust), so a raw write is the caller's mistake, not something every recording pays to check. pandare2's bundled extras (`ioctlFaker`, `fileFaker`) moved off `env_ptr` writes. Penguin's `penguin_cpu_env` writes move in phase 2.
- **A malformed env path (`EINVAL`) is not logged.** It changes nothing and fails the same way in replay.
- **Paths may follow up to four pointers** (`GUEST_WRITE_PATH_MAX`). The generated map lists every pointer reachable within four hops whose pointee bindgen knows (135 KB of C for all targets). A `void *` and bindgen's opaque types are never followed.
- **Rust:** a `GuestWriter` is bound to one vCPU, so `env_write!(w, regs[0], v)` takes no cpu argument. Indices go at the end of a field path (`pmsav8.rbar[1] => [2]`, not `a[1].b`). It uses the regenerated bindings for the C calls.
- **pandare2:** `cpu.env_ptr` stays a raw cffi pointer for reads; writing through it is unsupported. `set_pc` stays refused. ppc's `MSR` goes through the target's setter (`panda.guest.set_reg`), not `ppc_store_msr`.
- **The open questions** below were taken as recommended, except 1 (no guard, above); deferred requests keep their own names (renaming is left to the PR that touches #34/#35); an unspecified `@panda.hypercall` is refused during record/replay; the refused callback kinds stand; and private `.c` device structs are moved on request (phase 2). Question 5: hypercaller already reads Penguin's registers (see the table above).

## Open questions

1. ~~**Register raw-write guard.**~~ Decided: no guard; raw writes are unsupported.
2. **Deferred requests.** Keep them as separately named calls (`*_request`), or merge them into the same calls with behaviour chosen by thread? I recommend separate names: one call should have one timing.
3. **Unspecified registrations.** Refused during record/replay unless they declare `observe_only` or `mutates`? I recommend yes. The alternative keeps 0084's "legacy means mutating", which would let unlabelled plugins write during recordings.
4. **Mid-instruction and translation-time callbacks.** These are refused permanently. Is that acceptable as the boundary of "any callback"?
5. ~~**Hypercaller's magic register**~~ Decided (2026-11-01): hypercaller uses Penguin's number and return registers on every target. On checking, it already did; the aarch64 return-register bug the check found is fixed.
6. **Devices whose state struct is private to a `.c` file.** Leave them unreachable, or move their structs to headers as people need them? I recommend moving them on request, each as a small engine patch.
