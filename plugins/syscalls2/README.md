Plugin: syscalls2
===========

Summary
-------

The `syscalls2` plugin provides callbacks that allow notification whenever system calls occur in the guest, and can provide the parameters for each system call as long as the guest operating system (OS) is one of those supported by `syscalls2`.

This is accomplished by automatically generating a bunch of code based on an initial prototypes file. For full details, have a look at `scripts/syscall_parser.py` and one of the prototypes files, such as `generated-in/linux_x86_prototypes.txt`.

For adding support for a new OS or updating the existing ones, see `MAINTENANCE.md`.

The profile is selected from the guest OS name and the target. The OS name is `Panda(os_version=...)`, or the `os` of an image description passed as `Panda(generic=...)` (images written by `pandare2.mkimage` have one); its format is in `plugins/osi/README.md`. Profiles:

* Linux: x86, x86_64, arm, aarch64, mips, mipsel, mips64, mips64el, ppc, ppc64, riscv64. Not riscv32 or loongarch64: there the plugin refuses to load.
* Windows (x86 and x86_64 only): `windows-32-xpsp2`, `windows-32-xpsp3`, `windows-32-2000`, `windows-32-7sp[01]`, `windows-64-7sp[01]`. No panda-ng test covers them.
* FreeBSD: x86_64 only. No panda-ng test covers it.

Arguments
---------

* `load-info`: boolean, defaults to `false`. Enables loading of system call information for the selected OS profile. This allows more generic processing of system call events, without having to implement individual hooks.
* `abi`: string. Accepted but not used: on mips64 the ABI (o32, n64 or n32) of each system call is taken from its number (4000-4999, 5000-5999, 6000-6999).
* `task_key`: boolean, defaults to `true`. Linux only: pair returns by task (below), or with `false` by the call key (address space and user stack pointer; for tests; on MIPS it loses fork's return, whose ASID changes).

From Python:

```python
panda.load_plugin("syscalls2", {"load-info": True})
```

How returns are paired
----------------------

**Linux guests (every target).** syscalls2 declares the guest's system-call ABIs to the engine ([docs/design/syscall-pairing.md](../../docs/design/syscall-pairing.md)) and the task key, and the engine recognizes these calls, keeps each one whose return a callback wants (with its `syscall_ctx_t`), and pairs it with its return by task. So concurrent calls of threads of one process each get their own return, a call returning on another vCPU is paired, and no hook is added per call. The enter callbacks run before the trap; the return callbacks once the task is back in user mode at the return address (`pc`), as before.

| Target | ABIs declared | Task key |
|---|---|---|
| x86_64 | `syscall`; a 32-bit task's `int 0x80`, and through the vDSO `sysenter` or `syscall` from compat mode (reported under the i386 profile) | the per-CPU top-of-stack word at an offset from `KERNEL_GS_BASE`, decoded from the entry code at `LSTAR` |
| i386 | `int 0x80`; `sysenter` through the vDSO | the TSS's `sp1` (4.19 and later; `sp0` before), at TR's base |
| aarch64 | AArch64 `svc`; AArch32 `svc` (EABI) | `SP_EL1` |
| arm | `svc` (EABI, ARM and Thumb) | `SP_svc` (r13 of SVC mode) |
| riscv64 | `ecall` | `sscratch` (`current` while in U-mode) |
| mips, mipsel, mips64, mips64el | `syscall`: o32 (4000-), n64 (5000-), n32 (6000-) by number; errors flagged in `$a3` | `kernelsp[cpu]`, its address, register and shift decoded from the exception entry code |
| ppc | `sc` (errors flagged in CR0.SO) | `SPRG3` (`&current->thread`) |
| ppc64 | `sc` from 64-bit and from 32-bit tasks, `scv` | the PACA's `kstack`, at an offset from `SPRG1` decoded from the system call entry |
- A call the kernel restarts (the task traps again) has no return callback; its new entry gets one.
- An `execve` that succeeds has no return callback.
- A signal handler that runs before a call returns doesn't end it: the call returns when the handler's `rt_sigreturn` brings the task back.
- A vfork or clone child's first return (0) in its caller's address space is delivered as a return of the caller's call, before the caller's own (the tables' "double return" calls, and any `shared_child` call).
- A 32-bit ARM kernel restarts a call that has no handler to run inside the kernel: the call's return is the restarted call's value (on other kernels the task traps again: a new entry).
- MIPS o32 arguments 5 and 6 are on the user stack; the host reads user memory on MIPS only through the TLB, so they read as 0 while their page is not in it (as before).

**Windows and FreeBSD guests (x86 and x86_64).** A hook (the `hooks` plugin) at each call's return address, keyed by the return address and the process's ASID: threads of one process making the same call at the same address share the key.

Dependencies
------------

`hw_proc_id`, and `hooks` where returns are tracked with hooks (Windows and FreeBSD guests); it loads them itself.

APIs and Callbacks
------------------

### Callbacks
The `syscalls2` plugin defines one callback for each system call in each operating system (far too many to list here). The prototypes for each one are in `generated/syscalls_ext_typedefs_<arch>.h` (`x86`, `x64`, `arm`, `arm64`, `mips`, `mips64`, `mips64n32`, `ppc`, `ppc64`, `riscv64`).

Each callback is named `on_${SYSCALLNAME}_enter` for calls and `on_${SYSCALLNAME}_return` for returns. The parameters are the CPU state pointer, program counter, and then the arguments to the system call.

In addition to the OS-specific system calls, there are six callbacks defined that apply to all OSes:

Name: **on_unknown_sys_enter**

Signature:

```C
typedef void (*on_unknown_sys_enter_t)(CPUState *env, target_ulong pc, target_ulong callno)
```

Description: Called when an unknown system call (i.e., one that does not have a callback already defined for it) is invoked in the guest. The system call number will be available in the `callno` parameter.

Name: **on_unknown_sys_return**

Signature:

```C
typedef void (*on_unknown_sys_return_t)(CPUState *env, target_ulong pc, target_ulong callno)
```

Description: Called when an unknown system call (i.e., one that does not have a callback already defined for it) returns in the guest. The system call number will be available in the `callno` parameter.

Name: **on_all_sys_enter**

Signature:

```C
typedef void (*on_all_sys_enter_t)(CPUState *env, target_ulong pc, target_ulong callno)
```

Description: Called for every system call invoked in the guest. The call number is available in the `callno` parameter. This callback does minimal processing on the side of `syscalls2` plugin.

Name: **on_all_sys_return**

Signature:

```C
typedef void (*on_all_sys_return_t)(CPUState *env, target_ulong pc, target_ulong callno)
```

Description: Called whenever any system call returns in the guest. The call number is available in the `callno` parameter. This callback does minimal processing on the side of `syscalls2` plugin.

Name: **on_all_sys_enter2**

Signature:

```C
typedef void (*on_all_sys_enter2_t)(CPUState *cpu, target_ulong pc, const syscall_info_t *call, const syscall_ctx_t *rp)
```

Description: Called for every system call invoked in the guest. The `call` parameter is used to provide information about the system call. The `rp` parameter is used to provide information about the context of the system call (asid, argument values etc). This means that some additional processing is required on the side of the `syscalls2` plugin. You need to have the `load-info` flag enabled for `syscalls2` to use this variant of the callback.

Name: **on_all_sys_return2**

Signature:

```C
typedef void (*on_all_sys_return2_t)(CPUState *cpu, target_ulong pc, const syscall_info_t *call, const syscall_ctx_t *rp)
```

Description: Called whenever any system call returns in the guest. The `call` parameter is used to provide information about the system call. The `rp` parameter is used to provide information about the context of the system call (asid, argument values etc). This means that some additional processing is required on the side of the `syscalls2` plugin. You need to have the `load-info` flag enabled for `syscalls2` to use this variant of the callback.


### API calls
Finally the plugin provides four API calls:

Name: **get_syscall_retval**

Signature:

```C
target_long get_syscall_retval(CPUState *cpu)
```

Description: Retrieves the return value of a system call, abstracting away architecture-specific details. The call must be made in the appropriate context, so that the return value is still available (e.g. in a `on_all_sys_return` callback).

Name: **get_syscall_info**

Signature:

```C
const syscall_info_t *get_syscall_info(uint32_t callno)
```

Description: Returns a pointer to a `syscall_info_t` struct containing information about the specified system call. Available only when the `load-info` flag of the plugin has been turned on.

Name: **get_syscall_meta**

Signature:

```C
const syscall_meta_t *get_syscall_meta(void)
```

Description: Returns a pointer to a `syscall_meta_t` struct containing meta-information about the system calls of the guest operating system. Available only when the `load-info` flag of the plugin has been turned on.

Name: **syscalls2_current_call**

Signature:

```C
bool syscalls2_current_call(CPUState *cpu, uint64_t *id, uint32_t *flags)
```

Description: Inside an enter or return callback on a guest whose returns the engine pairs (Linux): the call's id, the same at its entry and its return, and the engine's delivery flags (`panda/syscall-pair.h`: `PANDA_SYSCALL_RETURN` at a return, `PANDA_SYSCALL_CHILD` at a child's first return). Returns false elsewhere.



Example
-------

From Python, `panda.ppp` registers a function on one of these callbacks (and loads `syscalls2` if it isn't loaded). The arguments after `cpu` and `pc` are the system call's. From `tests/guest.py`:

```python
@panda.ppp("syscalls2", "on_sys_openat_enter")
def openat(cpu, pc, dfd, filename, flags, *mode):
    try:
        print(panda.read_str(cpu, filename))
    except ValueError:
        pass    # the name isn't paged in
```

`python/examples/example_syscalls.py` is a complete script.

A C plugin registers with `PPP_REG_CB`:

```C
#include "panda.h"
#include "syscalls2/syscalls_ext_typedefs.h"

static void all_sys_enter(CPUState *cpu, target_ulong pc, target_ulong callno) {
    printf("system call %lu at 0x%lx\n", (unsigned long)callno, (unsigned long)pc);
}

bool init_plugin(void *self) {
    panda_require("syscalls2");
    PPP_REG_CB("syscalls2", on_all_sys_enter, all_sys_enter);
    return true;
}

void uninit_plugin(void *self) { }
```

`proc_start_linux` and `osi_linux` are the shipped plugins that use `syscalls2`.
