Plugin: proc_start_linux
===========

Summary
-------

This plugin uses `syscalls2` to provide the auxiliary vector in Linux systems as a PPP callback.
It watches `execve` and `execveat`, and reads the new program's arguments, environment and
auxiliary vector from its stack when it first runs. It doesn't load on ppc, ppc64, riscv32 or
loongarch64.

Arguments
---------

None.

Dependencies
------------
`syscalls2`, which it loads itself.

APIs and Callbacks
------------------

We provide the following PPP callback:

```
void (*on_rec_auxv_t)(CPUState *env, TranslationBlock *tb, struct auxv_values *av);
```

The structure (`proc_start_linux.h`) currently provides many elements of the auxiliary vector that you are quite unlikely to need.

There is no guarantee a kernel sets each value every time. The `struct auxv_values` is set to 0 before it is filled. A value provided should be suspect if it is a zero.

Python Example
-------

From `tests/guest.py`; `python/examples/proc_start.py` is a complete script.

```python
@panda.ppp("proc_start_linux", "on_rec_auxv")
def auxv(cpu, tb, av):
    argv = [panda.ffi.string(av.argv[i]).decode() for i in range(av.argc)]
    print(panda.ffi.string(av.execfn).decode(), argv, hex(av.entry))
```
