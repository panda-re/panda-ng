# One event model for callbacks, PPP and syscalls — plan

Status: decisions taken 2026-11-02 (see Decisions); E1 not started. This file revives the 10-30 proposal "One event model for callbacks, PPP and
syscalls2" (Claude Doc, https://claude.ai/artifact/U7NzSbrahmvzZzUxx6Jgkh). It checks that proposal
against what has landed since and against the syscall-callbacks plan (`syscall-callbacks.md`).
Facts about today's surfaces come from `notes/callbacks-inventory.md` on the `cbinventory` branch.
The proposal's content (UUID identity, signatures, diagnostics) is restated only where it changes.

## Why this has to come first

The next planned step, 0101, adds `PANDA_CB_SYSCALL_ENTER`/`RETURN` as two more `panda_cb_type`
values, with per-plugin `panda_syscall_watch` and `panda_syscall_call_data`. That is the surface
the event model replaces:
- the enum grows;
- `PANDA_CB_LAST` moves;
- two more members join the frozen `panda_cb` union;
- per-plugin state is keyed by plugin pointer rather than by subscription.

If 0101 goes in first, the syscall events are the first thing the registry has to shim. Built on
the registry, they are its first native events instead. The `syscalls` plugin, the PPP retirement,
the hooks plugin and osi would all be built against whichever surface exists when they start.

## What changed since 10-30

| 10-30 proposal assumed | Now |
|---|---|
| Builds on 0084–0092 (parked) | 0084–0093 are on main: modes, observe-only, capabilities, effects, and 0093's logged guest writes (`EVENT_GUEST_WRITE`) |
| The trap has one consumer | 0100 (PR #55): any number of consumers. Modifiers run in the order added, then observers. Holds and resume, per-consumer parts, and skipped calls reported at return. vfork and int 0x80 fixed |
| syscalls2 rebased onto the trap, with a table decoder and the generated `on_<name>_enter` kept as a shim | Decided 11-01: a new `syscalls` plugin, made of kernel-generated tables, lookup and lazy decoders, with no PPP names. syscalls2 becomes a shim or is removed (not decided) |
| "Under record or replay only observers are admitted" | rr policy (10-31): mutation through an explicit logged API, never silently skipped. Hypercall sites are phase 1 (0093); the syscall trap is phase 2 |
| Return matching needs an identity provider (§6) | The trap pairs by task key, stack pointer and return pc, with no kernel symbols, and passes tests 2 and 3 on 1 and 4 vCPUs. Fork-child returns are still not reported. That is the only identity gap left, and it is the identity provider's job |

## The plan

The 10-30 design is kept: one registry, UUIDv5 identity over (namespace, name, hashed
signature), a `size`-prefixed argument struct, subscription states with reasons, admission at
subscribe time, RCU dispatch, and per-descriptor KVM availability. The changes are below.

### 1. Subscriptions carry the per-consumer state

0101 put the watch filter and the call data per plugin. They move to the subscription handle:

```c
panda_sub sub = panda_subscribe(self, PANDA_EV_SYSCALL_ENTER, on_enter, ctx,
                                PANDA_MODE_ANALYSIS, PANDA_SUB_OBSERVE);
panda_sub_set_data_size(sub, sizeof(struct my_call));        /* generic: per-event instance data */
panda_syscall_watch(sub, SYSCALL_ABI_X86_64, __NR_openat);   /* event-specific filter */
```

- **Per-event instance data** is generic. An event whose instances span two emits (syscall entry
  and return, and later block start and end) declares that it has instances. A subscriber then
  gets zeroed bytes at the first emit, the same bytes at the second, and the engine frees them.
  This is today's engine-owned call data, made general.
- **Filters stay event-specific.** The publisher exports the filter calls, as the syscall
  source exports `panda_syscall_watch(sub, …)`. A general filter language is not part of this.
  The 10-30 performance rule stays: filter in C before dispatch.

### 2. Ordering: observers and mutators

There are two rules on record:
- **10-30 (you):** "generally observers should run prior to the mutators", with mutators seeing
  a possibly modified world.
- **0100 as built:** modifiers run first, in queue order, and observers run on the final pass with
  the final arguments. Each call the kernel really runs is then observed exactly once.

For an event whose mutators can cancel or rewrite what happens (syscall skip, fault/retry,
inject), an observer that runs first sees calls that never happen, or happen with other
arguments. For an event that is only a notification, the order makes no difference. **Proposal:**
- observers run after mutators by default;
- an observer may subscribe with `PANDA_SUB_OBSERVE_ORIGINAL` to run before the mutators and see
  the source's values;
- mutators keep subscription order, with an optional integer priority;
- a mutator sees `modified` and the original values.

This also answers the 10-30 open question about an observer phase after the mutators.

Decided 2026-11-02: after by default, with before as an opt-in (this is what 0100 already does).

### 3. Mutation under record/replay follows the rr policy

Admission no longer says "observers only under rr". It says that a mutator subscribes in rr only
to events whose source logs mutations (`desc.modes` includes `RR_MUTATE`). Today only the
hypercall site does (0093). The syscall trap is the rr policy's phase 2. Until then, syscall
mutators are refused in rr with a reason, as 0100 does now.

### 4. PPP: events, and a separate service lookup

The inventory shows that 18 of osi's 19 PPP callbacks are not events. They are how the osi API
reaches its backend, returning results through out-parameters, and with two backends loaded the
last one wins. Making these "events" would carry that misuse forward. The plan splits them:
- **Events**, for things that happen: `osi.task_change`, `proc_start.auxv`, `hooks.hit`,
  `syscall.enter/return`.
- **Services**, for "one provider answers": `panda_service_provide(ns, "osi.current_process", sig, fn)` /
  `panda_service_find(...)`. They use the same UUID and signature machinery and the same
  diagnostics. Exactly one provider is allowed, and a second one is a reported conflict. The
  `_int_fns.h`/apigen path is the existing direct API, and services can replace it later. Not
  required for phase 1.

PPP macros become a shim: `PPP_REG_CB(plugin, name, fn)` subscribes by name in the PANDA
namespace. Plugins that keep the old macros build unchanged. Their callbacks have no observe or
mutate declaration, so they are admitted as legacy, outside rr.

### 5. Core callbacks become declared events, behind the existing API

`panda_register_callback*` and `MAKE_CALLBACK` stay. Each `panda_cb_type` is declared once as a
PANDA-namespace event, and the `PandaCallbackEntry` lists become that event's subscriber list.
**The hot path must not change.** Dispatch for a core type stays "load list head, return if
empty". The registry only adds the descriptor, admission and diagnostics around it. The dead
types (20 with no call site) are not declared, and registering one is refused with a reason
instead of `assert(false)`. The duplicates (the eight memory types, the four block types) are
merged later, as new events with shims. They are not part of phase 1.

## Phases (engine patch numbers from manager-bc; only 0101 is reserved)

| | What | Tests and gates |
|---|---|---|
| **E1** | Registry core: `panda/events.h`, `events.c`, with declare, find, subscribe, emit, states and reasons, UUIDv5 (GLib SHA-1), signature parse, admission (modes, capability, KVM, observe/mutate, rr-mutate), RCU, per-instance data. Core types declared and the `panda_register_callback*` shim. `info panda-events`. | Host-only model test like test 1: binding, pending, mismatch, refusal, unload and RCU under ASan/UBSan. `tests/callbacks.py` unchanged. Offcost gate and `cbench` show no change on block, mem and syscall paths. UUID vectors identical across C, Python and Rust |
| **E2** | Syscall events on E1, replacing 0101: `syscall.enter`/`syscall.return` with `PandaSyscall` as the argument struct (it already has `size`). The PANDA dispatcher is one trap consumer, and `panda_syscall_watch(sub, …)` is its filter. The ordering from section 2. | Tests 4 (consumers together) and 5 (rr). Trap bench through the event path stays within noise of 0100's ~75/250 ns |
| **E3** | pandare2: `panda.events()`, `@panda.event(...)`, cffi struct built from the signature string, and `@panda.syscall(...)`. `ppp` and `cb_*` stay as shims | `pybench`; existing Python tests unchanged |
| **E4** | The `syscalls` plugin: tables, lookup, lazy decoders, and test 6. PPP shim; proc_start_linux, osi_linux and pandare2's `procname=` move to events. syscalls2 becomes a shim or is removed | Conformance against syscalls2 on the 10 callbacks used in tree |
| **E5** | osi services (section 4); hooks plugin on `tcg_hook_insert` publishing `hooks.hit` | hooks overhead from the 11-01 baseline |
| **E6** | Merge the memory and block duplicates, then remove the legacy macros and the frozen layout at a major version | |

Hard parts:
- **E1's shim must be free on the hot path.** The risk is an extra indirection per block or per
  memory access. The cost will be measured, not assumed.
- **The frozen `panda_cb_list` layout** and the Rust bindings must keep building through E1–E5.
- **E3 builds cffi types at run time** from signature strings, while today's are compiled cdefs.
  Signatures also need to say what a `ptr` points to before Python can decode one. The grammar is
  still open from 10-30.
- **Ordering with holds.** 0100's resume rule ("the holder again, then the consumers after it")
  has to hold in the generic dispatcher, or stay syscall-specific inside the trap consumer. The
  plan is the latter: the trap stays the source of truth for holds, and the event layer only
  delivers.

## Still open (carried from 10-30, plus new)

- Signature grammar for `ptr` targets, bools and fixed arrays.
- Renames: old UUIDs as aliases, or a breaking change.
- Strictness default, and whether Penguin turns it on.
- syscalls2: a shim or removal (unchanged from 11-01).

## E1 as built (2026-11-02, uncommitted)

Overlay only, with no engine patch:
- **New files:** `include/panda/events.h` (the API, installed with the engine), `events-internal.h`, `admission.h`, `panda/src/events.c` and `events-core.c`.
- **Changed files:** `callbacks.c` and `rr_control.c`.

What it does:
- **Registry.** UUIDv5, signature parsing with layout checks, placeholders from `find`, and the PENDING, MISMATCH (field-level diff), REFUSED and DISABLED states with their reasons. It also has the dispatch order from section 2, mutator priority, open/close instances with per-subscription data, and RCU snapshots. Unload drops the plugin's subscriptions before `dlclose`, and its events become undeclared. Listings come as text and as JSON.
- **One admission policy.** `panda_admit` (in `admission.h`) is the policy. `callback_mode_valid` now calls it, so core callbacks and events refuse for the same reasons. Refusal messages name the type and the rule. The rr start check covers registry subscriptions, and admission is re-evaluated after every rr transition.
- **Core types are declared as `cb.<name>` events.** Their `panda_cbs` lists are listed as their subscribers. `MAKE_CALLBACK` and the list layout are unchanged.

Narrower than the plan, or different:
- **Core events can only be listed in E1.** A `panda_subscribe` to one is REFUSED with a reason, because a core event has no argument struct yet. Core signatures leave out return values and spell bool as `u8`. They are provisional until their structs land.
- **Subscriber signature.** It is `fn(ctx, args, data)`, not 10-30's `fn(ctx, args)`, so that per-instance data reaches the subscriber.
- **`info panda-events` isn't done.** No `hmp_panda_*` command is wired into the monitor today, so it needs an `hmp-commands-info.hx` patch (and a number). `panda_events_describe(json)` is there for it and for `panda.events()` in E3.
- **Strictness option and the after-startup PENDING report:** not done.

Tests:
- `tests/static/events_registry.py` compiles the real `events.c` against a Python model, under ASan/UBSan, and is the flake check `events-registry`. It has directed cases plus 3000 random sequences, and seven bugs planted on purpose were all caught.
- `capability_admission.py` and `lifecycle.py` are updated, and `docs/vanilla-gates.toml` covers the new files.

Still to run in a build window: the engine build, `tests/callbacks.py`, offcost and cbench.

## Decisions (2026-11-02)

- **Ordering:** observers run after mutators by default; `PANDA_SUB_OBSERVE_ORIGINAL` opts an
  observer in to running before them.
- **Sequencing:** the registry comes first. E1 then E2 replace 0101's `PANDA_CB_SYSCALL_*` enum
  types.
- **osi:** services are split from events and built in E5, with the osi rework.

