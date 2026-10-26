Plugin: Hooks
===========

Summary
-------
Calls a function right before the guest executes the instruction at a given virtual address. The
address can be anywhere in a block, not just its start. pandare2's `panda.hook` uses it, and so do
`syscalls2` and `osi_linux`.

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

```
void enable_hooking()
void disable_hooking()
```

Turn the translation-time callback that places hooks on and off. Hooking is on when the plugin
loads.

`hooks_int_fns.h` also declares `add_symbol_hook(struct symbol_hook*)`, which hooked a symbol
(an optional library name and a symbol name) through classic PANDA's `dynamic_symbols` plugin.
panda-ng doesn't ship `dynamic_symbols`, and this plugin doesn't implement `add_symbol_hook`.
So pandare2's `hook_symbol` (which calls `add_symbol_hook`) and `hook_symbol_resolution` (which
calls `dynamic_symbols`) don't work.

Hook callbacks
--------------

Every hook is called as

```
void (*)(CPUState *env, TranslationBlock *tb, struct hook *h);
```

whatever its `type` says: the plugin always calls the `cb` union through this signature (its
`before_tcg_codegen` member; `start_block_exec` and `before_block_exec` have the same one). Write
hook functions with it. `tb` is the block that contains the address.

Changes made to the hook in the callback are kept. A hook disabled in its callback is removed.

struct hook
-----------

| field | meaning |
|---|---|
| `addr` | the virtual address of the instruction to hook |
| `asid` | run only when `hw_proc_id`'s `get_id` equals this; 0 for every address space |
| `type` | a `panda_cb_type`; kept for source compatibility, not used to pick the call |
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
