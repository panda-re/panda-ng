# One event model for callbacks, PPP and syscalls

PANDA has three ways for code to react to what the guest does: core callbacks
(`panda_register_callback`, a fixed `panda_cb_type` enum and the frozen `panda_cb` union),
plugin-to-plugin callbacks (PPP), and syscalls2's generated per-syscall callbacks. Each has its
own registration, its own failure modes and its own rules for record/replay. This document
describes one registry that all three are built on.

## The model

- **Events.** An event is declared by its publisher with a namespace, a name and a signature
  (a typed field list for its argument struct). Its identity is a UUIDv5 over the namespace, the
  name and the hashed signature. The argument struct starts with a `size` field, so fields can be
  appended without breaking older subscribers.
- **Subscriptions.** A subscriber names an event and gets a handle. The handle is in one of these
  states, each with a reason that names the cause:
  - BOUND: the publisher declared the event with the same signature;
  - PENDING: no publisher has declared it yet (it binds when one does);
  - MISMATCH: declared with another signature (the reason is a field-level diff);
  - REFUSED: admission refused it (the reason names the rule);
  - DISABLED: turned off by the subscriber.
- **Admission** is decided at subscribe time, and again when the run's mode changes (for example
  entering record or replay): modes, capability, KVM availability (per event descriptor), and
  whether a mutator may run under record/replay.
- **Dispatch** reads an RCU-published snapshot of each event's subscribers. Unloading a plugin
  drops its subscriptions before `dlclose`.

### Per-subscription state

Filters and per-call data belong to the subscription, not to the plugin:

```c
panda_sub sub = panda_subscribe(self, PANDA_EV_SYSCALL_ENTER, on_enter, ctx,
                                PANDA_MODE_ANALYSIS, PANDA_SUB_OBSERVE);
panda_sub_set_data_size(sub, sizeof(struct my_call));        /* generic: per-instance data */
panda_syscall_watch(sub, SYSCALL_ABI_X86_64, __NR_openat);   /* event-specific filter */
```

- **Instance data** is generic. An event whose instances span two emits (a syscall's entry and
  return, a block's start and end) is declared with instances. Each subscriber gets zeroed bytes
  at the first emit and the same bytes at the second; the engine frees them.
- **Filters are event-specific.** The publisher exports its filter calls, as the syscall source
  exports `panda_syscall_watch(sub, …)`. Filtering happens in C, before dispatch.

### Ordering: observers and mutators

For an event whose mutators can cancel or rewrite what happens (skip a syscall, retry a fault,
inject), an observer that ran first would see calls that never happen, or happen with other
arguments. So:
- observers run after mutators by default, and see the final values;
- an observer may subscribe with `PANDA_SUB_OBSERVE_ORIGINAL` to run before the mutators and see
  the source's values;
- mutators run by priority, then in subscription order;
- a mutator sees both the modified and the original values.

This is the order the syscall trap already uses for its consumers.

### Mutation under record/replay

A mutator subscribes under record or replay only to events whose source logs its mutations
(`desc.modes` includes `RR_MUTATE`). Today that is the hypercall site. Syscall mutators are
refused under record/replay, with a reason, until the syscall trap logs its mutations.

### PPP: events and services

Most of osi's PPP callbacks are not events: they are how the osi API reaches its backend,
returning results through out-parameters, and with two backends loaded the last one wins. The
registry separates the two uses:
- **Events**, for things that happen: `osi.task_change`, `proc_start.auxv`, `hooks.hit`,
  `syscall.enter`, `syscall.return`.
- **Services**, for "one provider answers": `panda_service_provide(ns, "osi.current_process",
  sig, fn)` and `panda_service_find(...)`. They use the same UUID and signature machinery and
  the same diagnostics. Exactly one provider is allowed; a second one is a reported conflict.

The PPP macros become a shim: `PPP_REG_CB(plugin, name, fn)` subscribes by name in the PANDA
namespace, and plugins that use the macros build unchanged.

### Core callbacks

`panda_register_callback*` and `MAKE_CALLBACK` stay. Each `panda_cb_type` is declared as a
PANDA-namespace event (`cb.<name>`), and its `PandaCallbackEntry` list is that event's subscriber
list. The hot path does not change: dispatch for a core type is still "load the list head, return
if empty". The registry adds the descriptor, admission and diagnostics around it. Types with no
call site are not declared, and registering one is refused with a reason instead of
`assert(false)`.

## Phases

| | What | Tests |
|---|---|---|
| **E1** | Registry core: `panda/events.h` and `events.c` with declare, find, subscribe, emit, states and reasons, UUIDv5, signature parsing, admission, RCU and instance data. Core types declared as `cb.*` events behind `panda_register_callback*`. | Host model test under ASan/UBSan; `tests/callbacks.py` unchanged; no change in block, memory or syscall path cost; UUID vectors identical across C, Python and Rust |
| **E1b** | Core callbacks get argument structs, so `cb.*` events can be subscribed to through the registry | `tests/callbacks.py` through the registry; no hot-path cost |
| **E2** | `syscall.enter` and `syscall.return`, with `PandaSyscall` as the argument struct. The PANDA dispatcher is a syscall trap consumer, and `panda_syscall_watch(sub, …)` is its filter | Consumers together; record/replay; the trap's per-call cost through the event path |
| **E3** | pandare2: `panda.events()`, `@panda.event(...)`, cffi structs built from signatures, and `@panda.syscall(...)`. `ppp` and `cb_*` stay as shims | pybench; existing Python tests unchanged |
| **E4** | The `syscalls` plugin, built on the syscall events: kernel-generated tables, lookup, decoding, and per-syscall events. syscalls2 becomes a deprecated shim; proc_start_linux, osi_linux and pandare2's `procname=` move to events | Conformance against syscalls2 on the callbacks used in tree |
| **E5** | osi services; the hooks plugin on the engine's address hooks, publishing `hooks.hit`; the PPP shim | hooks overhead |
| **E6** | Merge the duplicated memory and block callback types, then remove the legacy macros and the frozen layout at a major version | |

Hard parts:
- **The core-callback shim must be free on the hot path.** The risk is an extra indirection per
  block or per memory access; it is measured, not assumed.
- **The frozen `panda_cb_list` layout** and the Rust bindings keep building through E1–E5.
- **pandare2 builds cffi types at run time** from signature strings, where today's are compiled
  cdefs. A signature has to say what a pointer points to before Python can decode it.
- **Ordering with holds.** The syscall trap's resume rule ("the holder again, then the consumers
  after it") stays inside the trap consumer; the event layer only delivers.

## E1 as built

Overlay only, with no engine patch:
- **New files:** `include/panda/events.h` (the API, installed with the engine),
  `events-internal.h`, `admission.h`, `panda/src/events.c` and `events-core.c`.
- **Changed files:** `callbacks.c` and `rr_control.c`.

What it does:
- **Registry.** UUIDv5, signature parsing with layout checks, placeholders from `find`, the
  subscription states with their reasons, the dispatch order above, mutator priority, open/close
  instances with per-subscription data, and RCU snapshots. Unload drops the plugin's
  subscriptions before `dlclose`, and its events become undeclared. Listings come as text and as
  JSON (`panda_events_describe`).
- **One admission policy.** `panda_admit` (in `admission.h`) is the policy, and
  `callback_mode_valid` calls it, so core callbacks and events are refused for the same reasons.
  Refusal messages name the type and the rule. The rr start check covers registry subscriptions,
  and admission is re-evaluated after every rr transition.
- **Core types are declared as `cb.<name>` events**, with their `panda_cbs` lists as their
  subscribers. `MAKE_CALLBACK` and the list layout are unchanged.
- **Subscriber signature:** `fn(ctx, args, data)`, so that per-instance data reaches the
  subscriber.

Limits in E1:
- **Core events can be listed but not subscribed to.** A `panda_subscribe` to one is refused with
  a reason, because a core event has no argument struct yet (E1b). Core signatures leave out
  return values and spell bool as `u8`, and are provisional until their structs land.
- **No monitor command.** `panda_events_describe` is the listing an `info panda-events` command
  and `panda.events()` are built on.
- **No startup report** of subscriptions still unbound after startup.

Tests: `tests/static/events_registry.py` compiles the real `events.c` against a Python model under
ASan/UBSan (flake check `events-registry`), with directed cases and random sequences.
`capability_admission.py` and `lifecycle.py` are updated, and `docs/vanilla-gates.toml` covers the
new files.

## E1 finish as built

- **`info panda-events`.** The handler is `panda/src/events-hmp.c`, which prints
  `panda_events_describe(false)`. Engine patch 0102 adds the command to `hmp-commands-info.hx`
  under `CONFIG_PANDA`.
  `panda.events()` reads the JSON form.
- **Unbound subscriptions fail startup.** Once startup is over, every subscription still
  PENDING or MISMATCH is reported in one block, one line per subscription, giving its event,
  signature, owner, kind, state and reason. The block names the way out. It runs once per run.
  - `PANDA_EVENTS_STRICT=0` (or `warn`), or `panda_events_set_strict(false)` before startup,
    turns the block into a warning, and the run goes on. The call wins over the variable. An
    unrecognized value warns and means fatal.
  - `PANDA_SUB_OPTIONAL`, combined with the kind flag, marks a subscription that uses a
    publisher only if it is loaded. Optional subscriptions are listed but never fail the
    check, and the listing shows their kind as `<kind>+optional`. pandare2 has
    `@panda.event(..., optional=True)`.
  - **Command line** (`qemu-system-<arch>` and `panda-qemu`, with no `panda_init`): a
    machine-init-done notifier, added by `events.c` when the library loads, runs the check
    after `-plugin` and the panda modules have loaded. A failure is logged with `error_report`,
    then `exit(1)`. The notifier returns at once unless analysis is provisioned, so runs without
    it are untouched. If nothing has used the registry, the check returns without setting it
    up.
  - **pandare2:** `panda_init` defers the notifier (`panda_events_defer_startup_check()`
    before `qemu_init`) and calls `panda_events_reevaluate()` after `qemu_init`, because a
    subscription made before QEMU chose KVM was admitted with KVM off. It runs the check after
    `after_machine_init`, so plugins loaded there count, and returns 1 on failure without
    exiting. pandare2 then raises `EventsUnboundError` with the report
    (`panda_events_startup_report()`). A MISMATCH from `@panda.event` raises at subscribe time.
  - A subscription that becomes PENDING after startup (its publisher unloads) is logged when
    that happens. Only the startup check fails a run.
- **Owner names.** `panda_events_set_owner_name(owner, name)` names an owner that is not a
  plugin, such as pandare2's subscriptions, in listings and reasons. A registered name wins over
  the plugin's, and an unload forgets it.
- **UUID vectors.** `tests/static/event-uuid-vectors.tsv` lists namespace, name, signature,
  hashed part and UUID. `events_registry.py` checks the vectors against `events.c` and Python's
  `uuid5`, and `rust/panda-bindings/tests/event_uuid_vectors.rs` derives them in Rust.
- **Signature grammar:** `bool`, `ptr<T>` and `int[N]`, described in
  `event-signature-grammar.md`.
