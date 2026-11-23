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

`hw_proc_id`, which it loads itself: a hook's `asid` is compared with its `get_id`.

APIs and Callbacks
------------------

```
void add_hook(struct hook*)
```

Adds a hook. The plugin copies the struct, so the caller's copy can go away.

Adding the same function at the same address, asid and `type` again is ignored (its `km` and
`context` don't count). A hook added disabled is ignored: nothing could enable the plugin's copy.

```
void enable_hooking()
void disable_hooking()
```

Turn every hook on and off at once, from the next hit. Hooking is on when the plugin loads.

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

`hooks_int_fns.h` also declares `add_symbol_hook(struct symbol_hook*)`, which hooked a symbol
(an optional library name and a symbol name) through classic PANDA's `dynamic_symbols` plugin.
panda-ng doesn't ship `dynamic_symbols`, and this plugin doesn't implement `add_symbol_hook`.
So pandare2's `hook_symbol` (which calls `add_symbol_hook`) and `hook_symbol_resolution` (which
calls `dynamic_symbols`) don't work.

Hook callbacks
--------------

Every hook runs right before the instruction at its address, whatever its `type`. The type picks
the `cb` member, and so the signature, it is called through:

| `type` | called as |
|---|---|
| `PANDA_CB_AFTER_BLOCK_EXEC` | `after_block_exec(cpu, tb, 0, h)`: the exit code is always 0 |
| `PANDA_CB_BEFORE_BLOCK_TRANSLATE` | `before_block_translate(cpu, pc, h)`: `pc` is the hooked address |
| `PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT` | `before_block_exec_invalidate_opt(cpu, tb, h)`: the result is ignored |
| any other (`BEFORE_TCG_CODEGEN`, `START_BLOCK_EXEC`, `BEFORE_BLOCK_EXEC`, `END_BLOCK_EXEC`, `AFTER_BLOCK_TRANSLATE`, `BLOCK_TRANSLATE`) | `void (*)(CPUState *cpu, TranslationBlock *tb, struct hook *h)` |

`tb` is the translated block that contains the hooked instruction. A `BLOCK_TRANSLATE` hook gets
it too, not a `struct qemu_plugin_tb`.

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
| `asid` | run only when `hw_proc_id`'s `get_id` equals this; 0 for every address space |
| `type` | a `panda_cb_type`: which `cb` member is called (above); not when the hook runs |
| `cb` | the function, in the `hooks_panda_cb` union |
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

`panda.hook(addr, enabled=True, kernel=None, asid=None)`: `kernel=True` or `False` sets `km`,
and `asid` filters as above. `python/examples/proc_start.py` hooks each program's entry point.

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
h.type = PANDA_CB_START_BLOCK_EXEC;
h.cb.start_block_exec = my_hook;    // void my_hook(CPUState *, TranslationBlock *, struct hook *)
h.km = MODE_ANY;
h.enabled = true;
add_hook(&h);
```
