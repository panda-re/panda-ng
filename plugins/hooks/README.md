Plugin: Hooks
===========

Summary
-------
Calls a function right before the guest executes the instruction at a given virtual address. The
address can be anywhere in a block, not just its start. pandare2's `panda.hook` uses it, and so do
`syscalls2` and `osi_linux`.

It is built on the engine's address hooks (`panda/address-hook.h`, over `accel/tcg/hooks.c`): one
engine hook per hooked address, whose callback filters the hooks added there by asid and
kernel/user mode, in C, and calls them. Code on pages without hooks is not instrumented and keeps
its shared (CF_PCREL) translations. TCG only: under KVM hooks are accepted and never run.

Arguments
---------

None.

Dependencies
------------

None. A hook's `asid` is compared with `panda_current_asid()` on every arch, the value
pandare2's `panda.current_asid(cpu)` returns. On MIPS that is the EntryHi ASID, which the kernel
recycles, so a hook with an asid can also run in a later process that gets the same ASID; filter
further in the callback if that matters. (Classic PANDA compared with `hw_proc_id`'s `get_id`,
the task pointer on MIPS: pandare2's asids never matched there, and loading hw_proc_id put a
callback on every block.)

APIs and Callbacks
------------------

```
void add_hook(struct hook*)
```

Adds a hook. The plugin copies the struct, so the caller's copy can go away, and changes to it
after `add_hook` returns do nothing: to disable a hook, set `enabled = false` on the `struct hook`
its callback gets (the plugin's copy), from the callback or from a pointer kept from it.

Adding the same hook at the same address again is ignored while the first is enabled: the same
function, `asid`, `km` and `context`. The same function with another `context` or `km`
is another hook. A hook added disabled is ignored: nothing could enable the plugin's copy.

```
void enable_hooking()
void disable_hooking()
```

Turn every hook on and off at once, from the next hit. Hooking is on when the plugin loads. With
several vCPUs, a hook whose callback was already running on another vCPU when
`disable_hooking()` returned may still finish; none starts after.

Unloading the plugin removes its engine hooks while the vCPUs are paused, before the library is
closed: no hook runs once the unload has happened. An unload asked for from a vCPU's callback
(a hook, a hypercall) happens a little later, in the main loop, so hooks can still run until then
(in practice the main loop pauses the vCPUs within the same phase of a guest program:
`tests/hooks.py unload-hot`).

Events
------

`hooks.hit` (`hooks_events.h`), in the PANDA event registry (`panda/events.h`): emitted after a
hook's function ran, once per hook per hit, with `cpu`, `tb`, `pc` and `hook`. Observers only,
in analysis, fuzz and record/replay. It is emitted only while something is subscribed.

```C
#include "hooks/hooks_events.h"

static void on_hit(void *ctx, void *args, void *data)
{
    struct panda_hooks_hit_args *a = args;
    printf("hook at 0x%" PRIx64 "\n", a->pc);
}

panda_subscribe(self, panda_hooks_hit_event(), on_hit, NULL, PANDA_MODE_ANALYSIS,
                PANDA_SUB_OBSERVE);
```

Classic PANDA's `add_symbol_hook(struct symbol_hook*)` (a symbol through its `dynamic_symbols`
plugin) is not provided: panda-ng doesn't ship `dynamic_symbols`, and the declaration is gone
from `hooks_int_fns.h`, so a caller fails to build or to resolve the symbol instead of linking
against a function that never existed.

Hook callbacks
--------------

Every hook runs right before the instruction at its address and is called as

```C
void fn(CPUState *cpu, TranslationBlock *tb, struct hook *h);  // hook_func_t
```

`tb` is the translated block that contains the hooked instruction, `h` the plugin's copy of the
hook. There are no callback types: classic PANDA's `type` field and `cb` union are gone (every
type ran before the instruction anyway, so `after_block_exec`, `before_block_translate` and the
others did not do what their names said). A caller that set `h.type` or a `cb.<member>` fails to
build until it sets `h.cb` instead.

The asid and mode are compared when the instruction runs. At one address, hooks with an asid run
before those with asid 0, each in the order they were added. General-purpose registers are
current in the CPU state and may be written; the program counter is not (it holds what the block
left there).

Changes made to the hook in the callback are kept: there is one copy, shared by every vCPU. A
hook disabled in its callback, or through a pointer kept from one, is removed at its next hit.

struct hook
-----------

| field | meaning |
|---|---|
| `addr` | the virtual address of the instruction to hook |
| `asid` | run only when `panda_current_asid()` equals this; 0 for every address space |
| `cb` | the function (`hook_func_t`, above) |
| `km` | `MODE_ANY`, `MODE_KERNEL_ONLY` or `MODE_USER_ONLY` |
| `enabled` | false disables the hook, which is then removed |
| `context` | free for the caller |

Example
-------

From Python (`tests/hookalias.py`):

```python
@panda.hook(pc, kernel=False)
def h(cpu, tb, hook):
    print(f"hit 0x{hook.addr:x}")
    hook.enabled = False    # once
```

`panda.hook(addr, enabled=True, kernel=None, asid=None, observe_only=False)`: `kernel=True` or
`False` sets `km`, and `asid` filters as above. It no longer takes `cb_type`.
`python/examples/proc_start.py` hooks each program's entry point.

From C, as `osi_linux` does:

```C
void *hooks = panda_get_plugin_by_name("hooks");
if (hooks == NULL) {
    panda_require("hooks");
    hooks = panda_get_plugin_by_name("hooks");
}
void (*add_hook)(struct hook *) = (void (*)(struct hook *))dlsym(hooks, "add_hook");

struct hook h = {0};
h.addr = addr;
h.asid = 0;
h.cb = my_hook;                     // void my_hook(CPUState *, TranslationBlock *, struct hook *)
h.km = MODE_ANY;
h.enabled = true;
add_hook(&h);
```
