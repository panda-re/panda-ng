# The syscall trap on MIPS (o32, n32, n64)

Findings from a prototype of the L0 trap (`syscalls.md`, "Architecture contract") on MIPS, checked
on running guests. The prototype is the patch
`engine/patches/11.1.2/NNNN-WIP-target-mips-syscall-trap.patch` (not in `series`: apply it with
`git am` on top of an `import-series.sh` tree), plus overlay changes in
`engine/src/system/syscall-trap.c`, `engine/src/include/system/syscall-trap.h` and a test consumer,
`engine/src/system/syscall-trap-selftest.c`.

## Setup

- Kernels: linux_builder v4.0.1, Linux 6.13, `vmlinux.{mipsel,mipseb,mips64el,mips64eb}` (SMP
  build). 32-bit kernels are MIPS32 (o32 only); 64-bit kernels are MIPS64r2 with o32 and n32
  compat.
- Machine: `-M malta -m 512M`, default CPU (24Kf) for mipsel/mipseb, `-cpu MIPS64R2-generic` for
  mips64el/mips64eb. Single vCPU (see "Not verified").
- Guest: `engine/tests/guest/mips-sctrap/sct.c`, built with zig 0.15.2 for
  `mipsel-linux-musleabi` / `mips-linux-musleabi` (o32), `mips64el-linux-muslabi64` (n64) and
  `mips64el-linux-muslabin32` (n32), and the big-endian equivalents. Each test makes raw
  syscalls with inline `syscall` and prints what the task saw.
- Consumer: `SYSCALL_TRAP_SELFTEST=<log>` watches every number of the three MIPS ABIs, entry and
  return, logs each event, and performs mutations when `getpid` is called with a magic first
  argument. `MIPS_SC_TRACE=<pid offset>,<THREAD_SIZE>` adds the task-key candidates to each line,
  and the kernel's own task id (`((struct thread_info *)(ksp & ~(THREAD_SIZE - 1)))->task->pid`,
  offsets from the kernel's ISF) as ground truth.
- Run and check: `engine/tests/guest/mips-sctrap/run.py <arch> <out>` boots the guest with every
  ABI binary the kernel supports; `check.py <out>` compares the guest's lines with the log.

Result on all four targets (latest runs):

```
$ python3 check.py out-mipsel      # o32
PASS pairing-task: 4951 returns, 0 on another task than their entry []
PASS one-return: 0 calls with more than one final return []
... 19 per-test checks, all PASS
OVERALL PASS
$ python3 check.py out-mips64el    # o32 + n64 + n32
PASS pairing-task: 14541 returns, 0 on another task than their entry []
... 3 x 19 per-test checks, all PASS
OVERALL PASS
```

`mipseb` (5448 returns) and `mips64eb` (15885 returns) pass the same checks. "pairing-task"
compares, for every reported return, the kernel's task id at the entry and at the return.

## 1. Entry point

**Where.** `generate_exception_end(ctx, EXCP_SYSCALL)` (`translate.c`, microMIPS
`POOL32AXF SYSCALL`, nanoMIPS `P.SYSCALL` and `P16.SYSCALL`) leaves `env->active_tc.PC` at the
syscall instruction with exact state; `mips_cpu_do_interrupt()` delivers it (`case EXCP_SYSCALL`,
cause 8, `set_EPC`). The trap runs at the top of `mips_cpu_do_interrupt()`, before EPC, Cause,
Status.EXL and the hflags change, when:

```c
cs->exception_index == EXCP_SYSCALL && syscall_trap_enabled() &&
(env->hflags & (MIPS_HFLAG_KSU | MIPS_HFLAG_DM | MIPS_HFLAG_ERL | MIPS_HFLAG_BMASK)) == MIPS_HFLAG_UM
```

A skipped call returns from `mips_cpu_do_interrupt()` with `exception_index = EXCP_NONE` and
`PC += 4`. A fault at the entry needs no PC change: the PC is already at the syscall.

**pc and len.** Linux's handler adds 4 to EPC unconditionally (o32 `handle_sys` on a 32-bit
kernel, disassembled from `vmlinux.mipsel`):

```
lw    $9, 0xb4($sp)      # PT_EPC
addiu $9, $9, 0x4
sw    $9, 0xb4($sp)
```

So the reported pc is `PC + 4` and len is 4. Verified: `sct-o32` has `syscall` at `0x22648`,
and the trap logs `pc=2264c`; `sct-n64` has it at `0x1012c50`, logged `pc=1012c54`.

- **Delay slot.** A syscall in a branch delay slot has EPC at the branch; Linux still adds 4 and
  does not support it. The trap ignores it (the `MIPS_HFLAG_BMASK` test).
- **MIPS16e** has no syscall instruction. **microMIPS** `syscall` is 32-bit, and Linux's `+ 4`
  matches. `active_tc.PC` and the PC after eret do not carry the ISA bit, so pc matches either
  way. Not exercised: no microMIPS user binary was run.
- **nanoMIPS** has a 16-bit `syscall`, which needs len 2. There is no upstream nanoMIPS Linux.
  The prototype reports len 4.

**ABI.** On this kernel every MIPS64 task runs with `Status.UX = 1`, whatever its ABI. Logged
Status at the entry, per task:

```
tid 62  (o32 binary): status=0400a4f1
tid 471 (n64 binary): status=0400a4f1 / 2400a4f1
tid 880 (n32 binary): status=0400a4f1 / 2400a4f1
```

So Status cannot tell the ABI. The kernel dispatches by number range, whatever the task. On
`vmlinux.mips64el`, the exception handler `handle_sys` (o32, `scall64-o32.S`) tests
`v0 - 4000 < 467` and otherwise goes to `handle_sysn32` (`v0 - 6000 < 467`), then
`handle_sys64` (`v0 - 5000 < 467`, else ENOSYS). Verified from each binary (test `abi`, getpid
under all three numbers):

```
T o32 abi tid=62  o32=62  n64=62  n32=62       (mips64el)
T n64 abi tid=471 o32=471 n64=471 n32=471
T n32 abi tid=880 o32=880 n64=880 n32=880
T o32 abi tid=62  o32=62  n64=-89 n32=-89      (mipsel: 32-bit kernel, ENOSYS)
```

The trap reports these calls as o32, n64 and n32 (`4020->abi 5`, `5038->abi 12`,
`6038->abi 11`). **Decision: the ABI is the number's range** (4000-4999 o32 = `MIPS`, 5000-5999
`MIPS_N64`, 6000-6999 `MIPS_N32`, anything else `MIPS_N64` on a 64-bit target), and always o32
on a 32-bit target. Task state (`TIF_32BIT_REGS`, `TIF_32BIT_ADDR`) would misreport the calls
the kernel itself runs by number. The reported number is the raw one (4020, not 20), as
`qemu-plugin.h` already documents. The `MIPS_N32` and `MIPS_N64` values are 11 and 12 in the
prototype, after `LOONGARCH64`, following the ABI table in `syscalls.md`.

o32 `syscall(2)` (number 4000, the real number in a0) is reported as 4000 with its raw
registers. Not exercised.

## 2. Arguments

- **o32:** a0-a3 (`$4-$7`, zero-extended 32-bit), then words at `usp + 16`, `usp + 20` for
  arguments 5 and 6. The kernel copies four stack words, `usp + 16 .. usp + 28`
  (`lw $13..$24, 0x10..0x1c($8)` in `handle_sys`), for 7- and 8-word calls such as
  `sync_file_range` with its 64-bit pairs. `SyscallTrapEntry.args[6]` holds the first two;
  decoders that need words 7 and 8 read them at `usp + 24` and `usp + 28`.
- **n32, n64:** a0-a5 (`$4-$9`), full 64-bit registers (n32 passes 64-bit values in one
  register).

Verified on all four targets (test `basic`, getpid with six recognizable arguments, the o32
ones on the stack):

```
T o32 basic tid=62 ret=62 args=11111111,22222222,33333333,44444444,55555555,66666666
E id=167 abi=5 num=4020 a=11111111,22222222,33333333,44444444,55555555,66666666 pc=2264c ... tid=62
R id=167 abi=5 num=4020 ret=62 ...
```

The same for n64 (`pc 1012c54`) and n32 (`pc 2267c`). The stack is read only when the number is
watched (`syscall_trap_wants()`), so an unwatched number costs no guest memory access.

## 3. Return detection

**Where.** Linux returns to user mode with `eret` (by the source, `eretnc` on R5/R6 kernels), after restoring
every user register from `pt_regs`. The end of `restore_partial` on `vmlinux.mipsel`:

```
lw   $2, 0x28($sp)
lw   $sp, 0x94($sp)
eret
```

Both instructions reach `exception_return()` in `target/mips/tcg/system/special_helper.c`. The
hook is there, after `compute_hflags()`:

```c
if (syscall_trap_waiting() &&
    (env->hflags & (MIPS_HFLAG_KSU | MIPS_HFLAG_DM | MIPS_HFLAG_ERL)) == MIPS_HFLAG_UM) {
    mips_sc_return(env);
}
```

`eret` ends the translation block (`DISAS_EXIT`), so a PC or register change made there takes
effect. `deret` returns from EJTAG debug mode (DEPC), which Linux does not use to return to user
mode, so it is not hooked. With no pending call the cost is the `syscall_trap_waiting()` load
and branch, in a helper that already runs on every eret. MIPS I `rfe` kernels are not emulated
by QEMU's MIPS targets.

**Matching** is the core's (task key, user sp, pc). A return elsewhere, such as to a signal
handler, matches nothing. The call's return comes later, when the handler's sigreturn erets
back to it.

## 4. Task key

**No register identifies the task in user mode.** Before `eret`, Linux restores `$28` (kernel
`gp` = thread_info) and `$29` to the user's values, and k0/k1 hold nothing defined. The
candidates, as logged at entries and returns:

| Candidate | Observation | Usable |
|---|---|---|
| `$28`, kernel `sp` | user values at the trap and at eret | no |
| EntryHi ASID | per address space and per CPU, and reallocated: a `wait4` blocked across 400 forks went in with `asid=00000010` and came back with `asid=00000032` (mipsel; n64: `3b` to `5e`) | no |
| Context / XContext | BadVPN2 changes on every TLB miss, and the CPU number is per CPU | no |
| KScratch | not implemented by 24Kf or MIPS64R2-generic (`ks0=0`); where present Linux uses it for its TLB handlers, not for the task | no |
| UserLocal | 0 here (no ULRI); it is the TLS pointer, shared by a vfork child and not set for non-TLS clones | no |
| user sp | the same for a vfork child and its parent: `usp 7fed21e0` for both (mipsel) | not alone |
| **`kernelsp[cpu]`** | the current task's kernel stack top, written by `resume` on every context switch and loaded by the exception handler | **yes** |

**The key is `kernelsp[cpu]`, read from RAM.** Its address is not a register and has no symbol in
the trap. The trap runs the kernel's own exception path to find it. Once per vCPU, at the first
watched syscall, it interprets the instructions from the general exception vector
(`EBase + 0x180`: `except_vec3_generic`, then `exception_handlers[8]`, then `SAVE_SOME`'s
`get_saved_sp`) on a copy of the registers until the load into `$29`. That load's address is
`&kernelsp[cpu]`. mipsel:

```
mfc0  $26, $4          # Context: the CPU number in the high bits
lui   $27, 0x81c5
srl   $26, $26, 0x17
addu  $27, $27, $26
lw    $sp, 0x2380($27) # kernelsp[cpu]
```

On 64-bit kernels the same macro uses XContext (`dmfc0 $26, $20; dsrl32 $26, $26, 0x10`) and
`ld`. The interpreter knows the ~40 opcodes those paths use (ALU, `lui`, shifts, `mfc0` and
`dmfc0` of Status, Cause, Context, XContext, EntryHi, EBase, KScratch, UserLocal, loads,
branches and jumps with delay slots). Any other instruction gives up and the key falls back to
the ASID (or MMID), with a `LOG_UNIMP` message. The learned address equals the `kernelsp`
symbol on all four kernels:

| Kernel | Learned `kspaddr` | `kernelsp` symbol (ISF) |
|---|---|---|
| mipsel | `81c52380` | `0000000081c52380` |
| mipseb | `81c72380` | `0000000081c72380` |
| mips64el | `ffffffff81da4410` | `ffffffff81da4410` |
| mips64eb | `ffffffff81db4410` | `ffffffff81db4410` |

The variable lives in kseg0/ckseg0, so it is read through `address_space_read()` at its physical
address, with no TLB access and no side effects. Its value is a kseg0 address on 32-bit kernels
and an xkphys one (`9000000005a13fe0`) on 64-bit kernels. `k1 = kernelsp[cpu]`,
`k2 = user sp`, `percpu = k1` (Linux finds `current` from the stack: `$28 = sp & ~(THREAD_SIZE - 1)`),
`kmmu = 1 << 0` (kernel mode).

**Proofs** (test, then evidence; each "tid" is the kernel's own task id from the key):

- *Concurrent CLONE_VM threads at one read site:* 4 pthreads block in `read()` through the same
  `syscall` instruction (`pc 2264c`), and the main thread wakes them in reverse order. Each
  return pairs with its own entry:

  ```
  PASS o32 thread 3: tid 66 read(fd 9) guest ret 4; trap entry pc 2264c ksp 8413dfe0 usp 766e6e78, returns [(4, 66)]
  PASS o32 thread 2: tid 65 read(fd 7) guest ret 3; trap entry pc 2264c ksp 8412ffe0 usp 76ee9e78, returns [(3, 65)]
  PASS o32 thread 1: tid 64 read(fd 5) guest ret 2; ... ksp 83ecffe0 ...
  PASS o32 thread 0: tid 63 read(fd 3) guest ret 1; ... ksp 83ed3fe0 ...
  ```

- *vfork* (raw `clone(CLONE_VM | CLONE_VFORK | SIGCHLD, 0)`: the child returns from the
  parent's call on the parent's stack, makes a marker getpid and exits). The child has the
  parent's usp and address space, but another kernel stack. Its return is not taken for the
  parent's, and the parent's return pairs:

  ```
  PASS o32 vfork: parent 62 child 67: trap entry ksp 83ecdfe0 usp 7fed21e0; returns [(67, 62, '83ecdfe0')];
       child's getpid entry tid 67 ksp 83ed3fe0 usp 7fed21e0
  ```

- *clone3 child return* (`CLONE_VM | CLONE_VFORK`, own stack): the same, on all ABIs:

  ```
  PASS n64 clone3: parent 471 child 477: trap entry ksp 9000000005a3ffe0 ...; returns [(477, 471, '9000000005a3ffe0')];
       child's getpid entry tid 477 ksp 9000000005a0ffe0 usp 0000000001040a50
  ```

- *A key that survives a context switch and ASID reallocation:* the `wait4` above returns paired,
  with the same ksp and a different ASID.
- *The same task, the same site, nested:* a SIGALRM handler's `write()` goes through the same
  `syscall` instruction as the `read()` it interrupted, with the same ksp and a lower usp, so it
  is a separate call (k2 = usp).

Over all runs no return was paired with an entry of another task (`pairing-task`: 0 of
40,825 returns).

## 5. Return value and restart

- **Value.** v0, with a3 = 1 when v0 holds a positive errno (`handle_sys`:
  `sltu $8, -1134, $2; sw $8, PT_R7; negu $2`). `get_ret` = `a3 ? -v0 : v0`; `set_ret` writes
  `v0 = -ret, a3 = 1` for -4095..-1, else `v0 = ret` (sign-extended 32-bit for o32) and `a3 = 0`.
  Verified (`skip`, `setret`): a skip returning -13 and a return changed to -2 reach the task as
  v0 = 13 and 2 with a3 set, which the guest's wrapper turns into -13 and -2 (`ret_err=-13`,
  `setret ret=-2`); a skip returning 77 reaches it as 77.
- **Restart.** At the entry `handle_sys` saves the fourth argument in `PT_R26`
  (`sw $7, 0x88($sp)`). On the error path it saves the number in `PT_R0`
  (`lw $9, 0x28($sp); sw $9, 0x20($sp)`). For `-ERESTART*`, the signal code
  (`arch/mips/kernel/signal.c`, from the source, not disassembled) sets `v0 = regs[0]`,
  `a3 = regs[26]` and `EPC -= 4`. The task returns to the syscall
  instruction with the number in v0 and traps again. The core's rule (a return at the trap's
  pc - len, no call pending there) reports it as a return with `restart` set, then a new entry.
  Verified (event ids are from one mipsel run; the excerpt below is from another):

  | Case | Trap events for the `read()` | Guest |
  |---|---|---|
  | SIGALRM handler, `SA_RESTART` | `(1261, [(4003, restart)])`, then `(1264, [(1, 0)])` | `ret=1` |
  | SIGALRM handler, no `SA_RESTART` | `(1272, [(-4, 0)])` | `ret=-4` (EINTR) |
  | SIGSTOP + SIGCONT, no handler | `(1286, [(4003, restart)])`, then `(1294, [(3, 0)])` | `ret=3` |

  The restart's "return value" is v0 = the number (4003, 5000, 6000), as the kernel leaves it.
  With a handler, the restart (or the EINTR) is reported when the handler's `sigreturn` comes
  back to the call:

  ```
  E id=1360 num=4003 a=b,7fe61c30,8,...  pc=2264c        # read
  E id=1361 num=4004 a=c,10250,1,...     pc=2264c        # the handler's write: same site, other usp
  R id=1361 num=4004 ret=1
  E id=1362 num=4119 ...                 pc=7ff8aa50     # sigreturn (vDSO)
  R id=1360 num=4003 ret=4003 restart=1                  # at sigreturn's eret, to pc - 4
  ```

  `ERESTART_RESTARTBLOCK` (`restart_syscall`) was not exercised.

## 6. Fault, inject, rewind, restore

All four are implemented and pass on o32, n32 and n64 (mipsel, mipseb, mips64el, mips64eb):

| Test | What happens | Evidence |
|---|---|---|
| `inject` | getppid injected at a getpid's entry, then the getpid runs | `ret=pid`, `inject_ret = ppid (1)` |
| `injret` | getppid injected at a getpid's return; the task's a0-a2 and return value restored | `ret=pid a0=1a1ec701 a1=b0b0 a2=c0c0`, `inject_ret=1` |
| `injpw` | `pwrite64(fd, "INJECTED", 8, 4096)` injected at the entry: o32 passes the 64-bit offset in stack words 5-6, so the injection writes the user stack, and the rewind puts the task's words back | `file@4096=INJECTED`, stack words after the call `dead0001,dead0002` |
| `injloop` | `injpw` 200 times, after touching 256 pages each time | `n=200 bad=0` |
| `fault` | fault on an untouched anonymous page at the entry; the call resumes | mincore `before=0 after=1` |
| `faultret` | the same at the return | mincore `before=0 after=1` |
| `skip`, `setret` | skip with a value, change the return value | `-13`, `77`, `-2` |

- **FAULT** at the entry raises the user-mode fault with the PC already at the syscall. The
  kernel handles it and erets there, and the syscall traps again (resumed). At a return the PC
  is the user return address, as on the other targets. **MIPS difference:** the TLB is
  software-refilled. The first fault is often a TLB refill that loads an invalid entry and
  returns without making the page present, so a consumer must check again on resume and fault
  again. The test consumer took 2 to 3 rounds (`F id=5326 readable=0`, `readable=0`,
  `readable=1`).
- **INJECT**: v0 = the injected number, a0-a3 (and a4-a5, or o32's stack words 5-6) = its
  arguments.
- **REWIND**: v0 = the call's number, its arguments back (a3 too, which the injected call's
  error flag overwrote), o32 stack words back, `PC -= 4`.
- **RESTORE**: a0-a2 and a4-a5 (or o32's stack words) back. a3 stays the error flag, which
  `set_ret` writes with v0.
- **o32 stack words and the TLB.** At a return, the user stack page may no longer be in the
  software TLB, and QEMU cannot refill it (no hardware walker): `syscall_trap_access()` fails for
  a page that is resident. Before the fallback was added, one full run left the task's words
  as `1000,0` (the injected offset) after an injected `pwrite64`. The prototype now remembers,
  at the entry (when the task has just stored the words and the page is in the TLB), the
  physical address of `usp + 16`, keyed by (task, usp, pc). It writes there when the virtual
  write fails. In `injloop` on mipsel the fallback was taken 13 times out of 200 (`qemu.log`:
  `o32 stack arguments at 7fbcb720 not in the TLB (read and write); RAM 9fafc720`), and all
  200 restores were correct. This RAM write is not logged as a guest mutation for
  record/replay (see open problems).
- **inject_abis**: `MIPS | MIPS_N32 | MIPS_N64`. An injected call may use another ABI's numbers
  only through the number itself (the kernel picks the table by range).

## Proposed `SyscallTrapArch` for MIPS

One arch for all three ABIs. The ABI comes from the number, so `decode` yields it with the
number.

```c
static const SyscallTrapArch mips_syscall_arch = {
    .caps = SYSCALL_ARCH_FAULT | SYSCALL_ARCH_INJECT | SYSCALL_ARCH_REWIND |
            SYSCALL_ARCH_RESTORE,
    /*
     * num = (target_long)v0; abi by range (4000s o32, 5000s n64, 6000s n32; o32 only on a
     * 32-bit target); args: o32 a0-a3 (zero-extended) + words at usp+16, usp+20; n32/n64
     * a0-a5; pc = PC + 4; len = 4; usp = $29.  false in a delay slot.
     */
    .decode = mips_sc_decode,
    /*
     * k1 = kernelsp[cpu] (address learned from the exception vector, read from RAM),
     * k2 = $29, percpu = k1, kmmu = 1 << MMU_KERNEL; fallback k1 = ASID/MMID.
     */
    .key = mips_sc_key,
    .get_ret = mips_sc_get_ret,         /* a3 ? -v0 : v0 */
    .set_ret = mips_sc_set_ret,         /* v0, a3; o32 sign-extends */
    .restarting = mips_sc_restarting,   /* PC == entry pc - 4 (v0 == num) */
    .set_args = mips_sc_set_args,       /* a0-a3, a4-a5 or o32 stack words (RAM fallback) */
    .inject = mips_sc_inject,           /* v0 = num, set_args */
    .rewind = mips_sc_rewind,           /* v0 = num, set_args incl. a3, PC -= 4 */
    .restore = mips_sc_restore,         /* a0-a2, a4-a5 / stack words; a3 via set_ret */
};
```

Points for the contract:

- **Cheap filter before decode.** On o32 the full decode reads guest memory. The contract
  needs the abi and number first (registers only), the bitmap test, then the arguments and the
  key. The prototype adds `syscall_trap_wants(abi, num)` to the core for this.
- **The key costs a RAM read** (one 4- or 8-byte `address_space_read`) at every watched entry,
  and at every eret to user mode while any call is pending. It could be a cached host pointer to
  `kernelsp[cpu]` (it is ordinary RAM, fixed for the kernel's lifetime).
- **`set_ret` needs the ABI** at a plain return (o32 sign extension). The prototype makes the
  core report `rw->abi` with `SYSCALL_TRAP_RET`.
- **o32 stack words** need a target-private word saved with the pending call (the physical
  address of `usp + 16`). The prototype keeps a 64-entry lossy cache in the target instead.

## Verified, and not

Verified on running guests (all four MIPS targets, single vCPU, Linux 6.13): entry at
`EXCP_SYSCALL`, pc = syscall + 4, ABI by number range for all three ABIs and cross-ABI numbers,
o32 stack arguments, eret/eretnc return detection, the `kernelsp[cpu]` key (learned address =
symbol; threads, vfork, clone3, signal nesting, ASID reallocation), a3 error convention,
`-ERESTARTSYS` with and without `SA_RESTART` and with no handler, FAULT/INJECT/REWIND/RESTORE,
skip and return-value changes, big- and little-endian.

Believed, not verified:

- **SMP.** The key is learned per vCPU and read on the current vCPU, and `kernelsp` is per task,
  so migration between entry and return should not matter. These kernels did not bring up a
  second CPU on malta (`-smp 2 -cpu 34Kf` booted with one; with `-cpu 1004Kc` QEMU exited at
  startup), so this is untested.
- **Other kernels.** Only 6.13 was run. The interpreter covers `get_saved_sp` as written since
  Linux 2.6 for 32- and 64-bit kernels; microMIPS kernels (vector in microMIPS) and R6 kernels
  were not tried; non-Linux kernels fall back to the ASID.
- **microMIPS user code** (len 4 and the ISA bit) and **nanoMIPS** (16-bit syscall, len 2):
  not run.
- **o32 indirect `syscall` (4000)** and `ERESTART_RESTARTBLOCK`: not exercised.
- **Cost.** Not measured against stock QEMU.
- **Guest hypercalls (patch 0003)** go through their own instruction in `translate.c` and
  `helper_penguin_guest_hypercall`, which this patch does not touch. No hypercall test was run.

## Open problems

1. **Consumer reads at a return can fail for resident pages** (no hardware TLB refill), not only
   o32 stack words: a `read()` buffer at its return, a string argument. Consumers must fault
   (and fault again after a refill that loaded an invalid entry). A Linux MIPS page-table walker
   in the target would avoid it but needs the kernel's PTE layout.
2. **The o32 RAM fallback is not record/replay-safe.** It bypasses `syscall_trap_access()`'s
   guest-write logging. It needs a physical-address variant of the logged write, or the
   physical address kept with the pending call and written through the logged path.
3. **Kernel stack reuse.** A call that never returns to its site (exit, exit_group, a successful
   execve) stays pending under its task's kernel stack. A later task can get the same stack. If
   it then returns to user mode at the same pc with the same user sp, it would be taken for that
   call's return. The core evicts old calls, and this did not happen in these runs (406
   exit_group calls left pending, 0 mispairings). A task-exit hook could drop them.
4. **The test consumer's magic numbers** trigger on any getpid whose a0 holds the magic (musl's
   `getpid()` leaves a0 as it was), which produced extra injections in the `injloop` run. This
   is harmless to the results but not a pattern for real consumers.
