# System calls paired in the engine

The engine recognizes system calls from ABIs a consumer declares as data, keeps each call whose
return is wanted, and pairs it with the return to user mode that completes it, keyed by the
calling task. The API is `panda/syscall-pair.h`; the implementation is
`engine/src/panda/src/syscall-pair.c` (declarations, wants, translated code, the general cases)
and `syscall-pair-fast.c` (the functions translated code calls), and the target sites are patch
0102. syscalls2 is its consumer on Linux guests, on every target it supports.

## Ownership

| Layer | Owns | Does not own |
|---|---|---|
| Engine | the trap and return-to-user sites per architecture; the ABI declaration format and its decoding; task keys (reading, caching, revalidation); the pending set and the pairing rules | names, types, tables, the location of any kernel data structure, process semantics |
| Consumer (syscalls2) | the ABI declarations of the OS it knows (Linux: numbers, registers, the calls that never return, execve, fork-like calls); the task key's resolver; names, typed decoding and callbacks | pairing, pending state |

The engine holds no operating-system recipe. What it knows about a kernel (which numbers never
return, where a task's key is kept) comes from the declarations.

## Pay for use

- Nothing is declared: translated code is exactly as without the feature. Every site tests a
  flag at translation time (`tests/syscall_pair_tcg.py` checks that no block calls the engine).
- An ABI is declared: its trap sites load the number's want byte in translated code and call
  the engine only for a wanted number.
- Some number's return is wanted (the return sites are *armed*): the return-to-user sites
  check inline for a pending call of the returning task, and call the engine only when its set
  holds one. Arming and disarming flush translations, so a run that watches entries only pays
  nothing at returns.

## API

A consumer (one per run) registers, declares a task key, then its ABIs, and says which numbers
it wants:

```c
panda_syscall_consumer(self, fn, ctx, data_size);  /* data_size bytes kept with each call */
panda_task_key_declare(self, &key);                /* optional: else the call key */
int abi = panda_syscall_abi_declare(self, &decl);  /* one per trap form */
panda_syscall_want(abi, nr, PANDA_SYSCALL_WANT_ENTRY | PANDA_SYSCALL_WANT_RETURN);
```

`panda_syscall_consumer_release(self)` (with the vCPUs stopped, from the plugin's uninit) drops
the declarations, the wants and every pending call.

### ABI declarations

```c
typedef struct PandaSyscallAbi {
    uint32_t size, id, form, nbits;
    uint8_t nr_reg, nr_width, ret_reg, nargs, arg_reg[6];
    uint8_t ret_skip;                /* the call returns past this many bytes after the trap */
    int64_t err_min;                 /* [err_min, -1] is an error (Linux: -4095) */
    const uint8_t *noreturn;         /* never pending: exit, exit_group, rt_sigreturn */
    const uint8_t *ctx_replace;      /* a return elsewhere ends the call: execve */
    const uint8_t *shared_child;     /* may start a child at the return address: vfork, clone */
    uint8_t err_flag;                /* an error flag besides the value (MIPS $a3, PowerPC CR0.SO) */
    uint8_t arg_off[6];              /* ARG_MEM arguments: bytes above the register */
} PandaSyscallAbi;
```

| Form | Trap | Registers numbered as |
|---|---|---|
| `X86_SYSCALL` | SYSCALL from 64-bit code at CPL 3 | ModRM (0 RAX ... 15 R15) |
| `X86_INT80` | INT 0x80 at CPL 3, any code size | ModRM; 32-bit values |
| `X86_SYSENTER` | SYSENTER at CPL 3 | ModRM; 32-bit values |
| `X86_SYSCALL32` | SYSCALL from 32-bit code in long mode (compat mode) at CPL 3 | ModRM; 32-bit values |
| `A64_SVC` | SVC from AArch64 EL0 | X0-X30 |
| `A32_SVC` | SVC from AArch32 EL0, under an AArch64 EL1 or (arm target) an AArch32 one | R0-R14; 32-bit values |
| `RISCV_ECALL` | ECALL from U-mode (riscv64) | x0-x31 |
| `MIPS_SYSCALL` | SYSCALL in user mode, not in a delay slot | $0-$31; the target's width |
| `PPC_SC` | `sc 0` in problem state from a task of the target's width | r0-r31 |
| `PPC_SC32` | `sc 0` from a 32-bit task on ppc64 (MSR[SF] clear) | r0-r31; 32-bit values |
| `PPC_SCV` | `scv 0` in problem state (ppc64) | r0-r31 |

Number sets are bitmaps of `nbits + 1` bits; every number not below `nbits` (and every negative
one) shares the last bit and the last want byte. `nr_width` 32 takes the number as Linux does,
sign-extended from the low 32 bits. An argument register `PANDA_SYSCALL_ARG_MEM(r)` is the word (of
the form's width) in user memory at register `r` plus `arg_off` bytes, read at the entry (MIPS o32
passes arguments 5 and 6 16 and 20 bytes above `$sp`; on MIPS and 32-bit PowerPC the host reaches
user memory only through the software-filled TLB, so such a word reads as 0 while its page is not in
it).

`err_flag` describes a return that reports an error with a flag: `PANDA_SYSCALL_ERR_REG(r)` (MIPS:
`$a3` set, `$v0` the positive error number) or `PANDA_SYSCALL_ERR_PPC_SO` (PowerPC `sc`: CR0.SO
set, r3 the positive error number; `scv` returns negative values). The engine delivers such a
return negated, so that `err_min` applies.

**Linux's 32-bit vDSO.** A 32-bit task on x86_64 enters through `__kernel_vsyscall`: it pushes
ecx, edx and ebp, then runs `mov %esp, %ebp; sysenter` (Intel) or `mov %ecx, %ebp; syscall`
(AMD), then `int $0x80`. The kernel reads argument 6 where ebp was pushed (at ebp after
sysenter, which loses esp; at esp after syscall) and, after syscall (which loses ecx), argument
2 from ebp. It returns past the `int $0x80` (SYSEXIT, SYSRETL or IRET to a fixed place in the
vDSO), and a restarted call returns to the `int $0x80`, which makes it again. syscalls2
declares these forms with `ret_skip` 2 and argument 6 `PANDA_SYSCALL_ARG_MEM(ebp)` (sysenter) or
`PANDA_SYSCALL_ARG_MEM(esp)` (syscall, argument 2 in ebp), and reports them under its i386
profile.

### Delivery

The consumer's function gets each wanted entry and each paired return:

```c
void fn(void *ctx, CPUState *cpu, PandaSyscallCall *call, int64_t ret, uint32_t flags);

typedef struct PandaSyscallCall {
    uint64_t id;                     /* unique in the run, the same at entry and return */
    int64_t nr;
    uint64_t args[6];
    uint64_t pc;                     /* the trap instruction; the call returns to pc + len */
    uint64_t key;                    /* the task key at the entry */
    uint32_t len;
    uint16_t vcpu;                   /* the entry's */
    uint8_t abi, keytype;
    uint64_t data[];                 /* the consumer's data_size bytes */
} PandaSyscallCall;
```

- **Entry** (`flags` 0): before the trap, with the trapping code's registers; the callback may
  read guest memory and change registers. `call` is the pending record itself (or, for a call
  not kept, the vCPU's delivery slot), so the consumer writes `data[]` there. The engine calls
  it last (a tail call). To decide, after its own callbacks ran, that this call's return is
  wanted after all (or not), the callback calls `panda_syscall_keep(cpu, call, keep)` once it
  has written `data[]`: the engine copies the call into a record (or drops its record) then, and
  the callback does not use `call` after it. A call is kept at the entry if `keytype` is not 0.
- **Return** (`PANDA_SYSCALL_RETURN`): the return has completed, the task is in user mode at its
  destination, and `ret` is the return value. Further flags:

| Flag | Meaning |
|---|---|
| `RESTART` | returned to the trap instruction: the kernel restarts the call (it traps again, as a new call) |
| `ERROR` | `ret` is in the ABI's error range |
| `MOVED` | returned on another vCPU than it entered on |
| `CHILD` | a child's first return (below); the call stays pending for its caller |
| `ENDED` | a `ctx_replace` call ended without returning (a successful execve); `ret` is 0 |

Callbacks run on vCPU threads, concurrently on different vCPUs.

**Routes.** `panda_syscall_route(abi, nr, enter, ret, ctx)` sends one number's entries to `enter`
and its returns to `ret`, with `ctx`, instead of the consumer's function. Each ABI has a table of
`nbits + 1` routes, beside its want bytes, filled with the consumer's function at the
declaration: a delivery calls `route[min(nr, nbits)].fn[flags & RETURN]` (an index and a load
more than one function pointer, and nothing when nothing is delivered). A consumer that
dispatches by number itself (syscalls2: each number to its table slot) routes every number to
the code and data that handle it, and needs no lookup of its own. Routes are set before the
number is wanted (a delivery reads the function and `ctx` apart).

## Task keys

A task key is the same for every system call one task makes and differs between tasks alive at
the same time, read at the user/kernel boundary. No architecture defines a "current task"
register: a key is where the OS keeps a per-task value.

| Form | Key | Linux |
|---|---|---|
| `REG` | a register's value at the boundary | aarch64: `SP_EL1`, the task's kernel stack, banked while it runs at EL0 (for AArch32 tasks too); arm: `SP_svc`, the same for an AArch32 kernel; riscv64: `sscratch`, which holds `current` while the task is in U-mode (0 in the kernel); ppc: `SPRG3`, `&current->thread` |
| `WORD` | a word in guest RAM that the consumer's resolver locates for the current value of a base register | x86_64: the per-CPU word `entry_SYSCALL_64` loads RSP from, at an offset from `KERNEL_GS_BASE`; i386: the TSS's `sp1` (since 4.19; `sp0` before) at TR's base; ppc64: the PACA's `kstack`, at an offset from `SPRG1` that syscalls2 reads from the system call entry (`mr r10,r1; ld r1,PACAKSAVE(r13)`); MIPS: `kernelsp[cpu]`, base `EBase` (constant per CPU), whose address, index register (Context or XContext) and shift syscalls2 reads from the exception entry's `get_saved_sp` |
| call key | (address-space root, user stack pointer): x86 CR3, ARM TTBR0, RISC-V satp, the MIPS ASID, the PowerPC PID | the fallback |

A word is 32 bits on the 32-bit targets with `WORD` keys (i386, mips, mipsel), else 64; it is
read as it is in memory (a key is compared, not interpreted). No architecture has a per-task
register defined by the hardware: each key is where Linux keeps a per-task value, the same from
4.4 (or the port's start) to 6.x. On MIPS the call key is not a usable fallback: Linux gives a process a new ASID when
it flushes its TLB entries, which fork does, so a fork's return is lost.

- **WORD.** The resolver gets the vCPU and the base register's value and returns the word's
  guest physical address. The engine maps it to a host pointer once (it refuses anything but
  RAM) and keeps it per vCPU with the base value it was resolved for. At each boundary it compares
  the base register with that value: equal, one load; different, it resolves again. While the
  resolver cannot locate the word (early boot), the cached value stays unequal (every return
  goes to the C function), calls are kept under the call key, and the resolver runs again after
  4096 boundaries or a new base value. A call made while the word holds 0 is not kept.
  syscalls2's Linux resolver
  decodes the gs-relative load into RSP in the entry code at `LSTAR`, and walks the kernel's page
  tables (under KPTI, the 4 KiB below the user ones).
- **The call key** pairs a call with its return, but a task has several over its life, so it is
  not a task identity.
- Keys are reused once a task exits: a key identifies a live task. `panda_task_key(cpu, &key)`
  gives the current task's key at the boundary and inside the callbacks.

## Pairing rules

Every return to user mode is a candidate: x86 SYSRET, SYSEXIT and protected-mode IRET; AArch64
ERET to EL0 (AArch64 or AArch32); the AArch32 exception returns of an AArch32 kernel (RFE, `MOVS
PC`, `LDM` with the PC and `^`); RISC-V SRET; MIPS ERET and ERETNC; PowerPC RFI, RFID and RFSCV.
That includes the returns that end interrupts and faults, so the check is cheap when nothing is
pending. For the returning task's newest pending call:

| Destination | Result |
|---|---|
| the call's return address (`pc + len`: past the trap and the ABI's `ret_skip` bytes) | its return; popped |
| its restart address (`pc`; with `ret_skip`, the instruction past the trap) | its return, `RESTART`; popped |
| elsewhere, a `ctx_replace` call | `ENDED`; popped |
| elsewhere | a signal handler runs first: the call stays pending, and the handler's own calls stack above it (newest first) until `rt_sigreturn` brings the task back |

Once a call's return has gone elsewhere, a return to its return or restart address pairs it only
with the user stack pointer it entered with: a handler runs on a frame of its own, and a handler
whose system calls use the same instruction (a shared wrapper) returns to the same address; an
interrupt taken just after one of those calls returns would otherwise end the waiting call.

- **Children.** A return of 0 at the return address of a pending `shared_child` call, in the
  caller's address space, by another task (or under the caller's call key) is the child's first
  return: it is delivered `CHILD`, from a copy of the caller's call, and the call stays pending.
  It is delivered once per child: a child that comes back to user mode at that address before it
  moves on (an interrupt or a page fault there) is not a new child. This covers vfork and clone/clone3 with `CLONE_VM` while the caller has not returned yet; a
  fork child (its own address space) is not reported, and two concurrent clones at one address
  in one process may have their children's returns attributed to either call.
- `noreturn` calls are never kept, even when the consumer asks.
- A 32-bit ARM kernel restarts a call that has no handler to run (`-ERESTART*`, or
  `restart_syscall` for `-ERESTART_RESTARTBLOCK`) without returning to user mode: the task sees
  one call, whose return is the restarted call's value, and that is what is delivered. Other
  kernels return to the trap (`RESTART`) and the task traps again.
- These are the rules of POSIX kernels. An OS that calls back into user mode from inside a
  system call and returns into the kernel (Windows user callbacks) needs further rule kinds.

## The pending set

- 2048 sets of 4 records, the set chosen by a multiplicative hash of the key. A record holds the
  key, the `PandaSyscallCall` and the consumer's data.
- Lock-free. Each record has a 64-bit tag: the key's hash, the record's depth (LIFO order among
  one key's records) and a BUSY bit. Each set has an exact count of its records, which translated
  code reads. A record is claimed by a CAS of its tag and published by clearing BUSY. Only the
  key's own task pops it. The races are with another key's entry evicting a record and with a
  child's copy; both take only a record without BUSY, by CAS, and the owner sets BUSY by CAS
  before it reads a record it may pop.
- A kept entry is published before its delivery (so that the delivery is a tail call). Each
  vCPU names the record it last kept, and other vCPUs do not evict it: at most one record per
  vCPU, until that vCPU keeps another.
- **Bounded.** A full set evicts its oldest record (a call left by a killed task); if every
  other record is BUSY or named by another vCPU, the new call is not kept. An evicted call's
  return is not delivered; it is never delivered as another call's. `tests/syscall_truth.py`
  with `SC2_SETS_LOG=0` (one set) forces evictions and checks that.
- **Reset, snapshot load and fast restore** (`panda_analysis_reset`) empty the set and every
  task-key cache: the restored guest may run another kernel.
- At most 256 vCPUs (per-vCPU state is indexed by `cpu_index`).

## Translated code

| Site | Without a declaration | Declared, nothing returning wanted | Some return wanted |
|---|---|---|---|
| x86_64 SYSCALL (64-bit, CPL 3), SYSENTER and compat SYSCALL (CPL 3), A64 SVC (EL0), RISC-V ECALL (U), MIPS SYSCALL (user), PowerPC sc and scv (problem state) | unchanged | the number's want byte; the entry helper if set | same |
| INT 0x80, A32 SVC | unchanged | a flag test in interrupt delivery; the entry function if the number is wanted | same |
| SYSRET, SYSEXIT, protected IRET, ERET, ERETA, AArch32 RFE/MOVS PC/LDM ^ (arm target), SRET, MIPS ERET/ERETNC, RFI, RFID, RFSCV | unchanged | unchanged | after the plain helper: to user mode? the key, its set's count (and the child-watch count), then the pairing helper if not 0 |

The inline return check with a WORD key (x86_64, i386, ppc64, MIPS): the user-mode test, the
vCPU's cached base value against the base register (`KERNEL_GS_BASE`, TR's base, `SPRG1`,
`EBase`; a mismatch goes to a C function that resolves again), the word, a multiplicative hash,
the count load. With a REG key (aarch64 SP_EL1, arm SP_svc, riscv64 sscratch, ppc SPRG3): the
mode test, one load, the hash, the count load. The call key is read inline too. Under an AArch64
kernel a return to an AArch32 task (`A32_SVC` declared) is checked in C. While a child watch can be
set (an ABI has `shared_child` numbers), the check also loads the watch count.

### The functions translated code calls

They are chosen when the site is translated: one entry function per task key form (the
declared one, or the call key) and register layout (the target's standard one: the trap form
whose arguments are the target's argument registers in its word size, e.g. MIPS o32's four, or
any), and one return function per key form (a one-word task key, or the two-word call key). An
ABI of another layout whose arguments are all registers reads them through a table of offsets
built at the declaration. The trap sites taken in C (INT 0x80, A32 SVC) test the number's want
byte first and then enter the same functions (the general layout), with the trap's address. Each handles the common case and passes anything else, before it changes anything, to
a general function:

- entry: the number's entry alone is wanted: the call is written into the vCPU's slot and
  delivered (a tail call). Its return is wanted: the key (WORD: the cached word, if the cached
  base value is current), the hash, way 0 of an empty set claimed by CAS, the record written and
  published, the delivery (a tail call). Anything else (a set that holds calls, a key to
  resolve, a `shared_child` number) goes to the general entry.
- return: the set's only record is in way 0, has the key's tag, is claimed by CAS, is the key's,
  and the task returned to its return address: delivered, then popped. Anything else (other
  records in the set, a restart, a signal handler, an execve, a child) goes to the general
  return.

`syscall-pair-fast.c` is built per target as a library of its own (`panda_hot_ss`, patch 0102's
meson change) with `-fzero-call-used-regs=skip -fomit-frame-pointer`: QEMU and the toolchain
build with `-fzero-call-used-regs=used-gpr` (about 9 instructions on each return, and no tail
calls) and with frame pointers. These functions return without clearing their call-used
registers (the ROP-gadget mitigation is weaker at those returns) and have no frame-pointer frame
(frame-pointer unwinding skips them); the rest of the engine keeps both. The counters of the
common cases (`kept`, `paired`) are counted only in a build with `PANDA_SYSCALL_PAIR_STATS`
defined; `entries` is the vCPUs' call ids, and the rare cases are always counted.

## Coverage

| Target | Declared by syscalls2 (Linux) | Not yet |
|---|---|---|
| x86_64 | x86_64 SYSCALL; i386 INT 0x80 (from 32- and 64-bit code); a 32-bit task's vDSO entries (SYSENTER on Intel, SYSCALL from compat mode on AMD) | x32 numbers are syscalls2's unknown calls; a SYSENTER or compat SYSCALL outside `__kernel_vsyscall` (not followed by `int $0x80`) returns into the vDSO, not past it: its call stays pending until its set evicts it |
| i386 | INT 0x80; SYSENTER through the vDSO (as on x86_64) | as on x86_64 |
| aarch64 | AArch64 SVC; AArch32 SVC (EABI) | — |
| arm | AArch32 SVC (EABI, ARM and Thumb) under an AArch32 kernel | OABI |
| riscv64 | ECALL | — |
| mips, mipsel, mips64, mips64el | SYSCALL: o32, n64, n32 (by number) | a uniprocessor kernel (it reads `kernelsp` without an index: not decoded, the call key is used); SMP is untested (the malta test machines boot one CPU) |
| ppc, ppc64 | sc (64-bit tasks; 32-bit tasks on ppc64), scv | 32-bit PowerPC SMP is untested (the e500 machine boots one CPU) |
| riscv32, loongarch64 | — | syscalls2 has no profile for them |
| Windows and FreeBSD guests | — | syscalls2 tracks their returns with the hooks plugin, keyed by (return address, ASID) |

The engine's pairing and the syscall trap that serves the QEMU plugin syscall API
(`system/syscall-trap.c`) are independent: each has its own consumer and pending calls.

## Testing

- `tests/syscall_truth.py`: the guest's ftrace (`raw_syscalls`) is the reference. On busybox
  test machines (x86_64, i386, aarch64, arm, riscv64, the four MIPS and three PowerPC ones) at 1
  and 4 vCPUs where they boot SMP, workloads of pipelines, fork/exec, tasks blocked and moved
  between vCPUs, threads, signals (EINTR and SA_RESTART) and return paths (TF/IRET, restarts,
  `restart_syscall`, fork, vfork, threads, clone3, execve), a 32-bit program on x86_64 through
  `int $0x80` and its vDSO (with signals that end and restart its calls), on aarch64 also as
  32-bit tasks: syscalls2 reports each call the kernel records, with its arguments, on its vCPU
  and in order, and each call's return is the kernel's. Variants: `--intel` (KPTI; the vDSO
  enters with sysenter, else with syscall), the call key, one pending set.
- `tests/syscalls_smp.py`: syscalls2 with return callbacks on every vCPU at once.
- `tests/syscall_pair_tcg.py`: no translated block calls the engine without a declaration.
- `tests/syscalls.py`: syscalls2's callbacks and arguments on Debian guests, including the vfork
  child's return of clone3.
