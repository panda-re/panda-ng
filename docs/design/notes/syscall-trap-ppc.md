# Syscall trap on PowerPC (ppc, ppc64): prototype findings

Status: prototype. The target hooks are in
`engine/patches/11.1.2/NNNN-WIP-target-ppc-syscall-trap.patch` (not in `series`). The core change
they need (a per-call variant, below) is in `engine/src/system/syscall-trap.c` and
`engine/src/include/system/syscall-trap.h`. The guest test is `tests/ppc-sctrap/`.

## Setup

- Engine: QEMU 11.1.2 plus the series plus the WIP patch, dev tree from `import-series.sh`,
  `--target-list=ppc-softmmu,ppc64-softmmu`, TCG.
- Guests: linux_builder 6.13 kernels from `tests/busybox.sh`'s cache, booted with
  `tests/mkbusybox.py`'s machine arguments:
  - `powerpc`: ppce500, e500mc, 32-bit BookE kernel, Debian ppc32 busybox, `-smp 1`
  - `powerpc64`: pseries, POWER9, big-endian
  - `powerpc64le`: pseries, POWER9, little-endian
  
  The ppc64 guests ran with `-smp 2`. On ppce500, `-smp 2` hangs at boot after
  `qemu_e500_setup_arch()`, with or without the trap, so the ppc32 kernel ran on one CPU.
- Guest program: `tests/ppc-sctrap/sct.c`, freestanding and static, built with clang for ppc32,
  ppc64 ELFv2 BE and ppc64le (`build.sh`). Every call goes through one `sc` instruction, or one
  `scv 0` in the scv pass. Calls the host checks carry `0x5ca11ed` in r8 (argument 6). It runs:
  - basic: 1000 `gettid`, then 10 `read(-1)` (EBADF)
  - 4 forked processes and 4 CLONE_VM threads (3 by `clone`, 1 by `clone3`), each doing 200
    rounds of a blocking `read` plus a `gettid`. Each task reads a different byte count, so the
    return value says which task's call it was.
  - 50 vforks, whose children run on the parent's stack, and 20 forks
  - signals: SIGALRM without and with SA_RESTART during a blocking `read`, and SIGSTOP/SIGCONT
    with no handler during `read` and during `nanosleep`
  - mutations: inject at entry, inject at return, and fault at entry
  
  On ppc64 BE the 32-bit binary also runs as a compat task.
- Host consumer: `target/ppc/sc-trap-dbg.c`, enabled by `SCTRAP_LOG=<file>`. It watches every
  number of both PPC ABIs for entry and return. For each pair it compares the kernel's current
  pid at entry and at return. The pid comes from the task-key registers plus ISF offsets (used
  for this check only, not by the trap): ppc32 `SPRG3 - offsetof(task_struct, thread)`, ppc64
  `paca->__current`. It also checks the return values of the tagged calls.
- Run: `python3 tests/ppc-sctrap/run.py --qemu-dir <build> --bins <dir> --arch <arch> --smp N`.

Result summaries (consumer output):

```
powerpc (ppc32 BookE, -smp 1)
SUMMARY enter ppc32/sc=8432 ... return ppc32/sc=8337 ... restart=3
SUMMARY magic enter=4365 return=4363 (restart 3) checks ok=4354 bad=0 pid-pairing ok=8331 bad=7 (exit/exit_group 7) unknown=0
SUMMARY inject ok=2 bad=0 fault req=2 resumed-ok=1 bad=0 trap pending=78 full=0 stale=16 faults=2

powerpc64 (BE, -smp 2: sct64 with sc, sct64 with scv, sct32 compat)
SUMMARY enter ppc32/sc=8002 ppc64/sc=9685 ppc64/scv=7899 return ppc32/sc=7918 ppc64/sc=9541 ppc64/scv=7848 restart=9
SUMMARY magic enter=13149 return=13089 (restart 9) checks ok=13062 bad=0 pid-pairing ok=25309 bad=1 (exit/exit_group 1) unknown=0
SUMMARY inject ok=6 bad=0 fault req=3 resumed-ok=3 bad=0 trap pending=241 full=0 stale=35 faults=3
SUMMARY ppc64 PACAKSAVE learnt=2320

powerpc64le (-smp 2: sc, scv)
SUMMARY enter ppc32/sc=0 ppc64/sc=9605 ppc64/scv=7880 return ppc32/sc=0 ppc64/sc=9466 ppc64/scv=7829 restart=6
SUMMARY magic enter=8784 return=8726 (restart 6) checks ok=8708 bad=0 pid-pairing ok=17297 bad=0 (exit/exit_group 0) unknown=0
SUMMARY inject ok=4 bad=0 fault req=2 resumed-ok=2 bad=0 ...
SUMMARY ppc64 PACAKSAVE learnt=2320
```

The guest prints the same results on every arch and in every pass, and they match what is
expected:

```
SCT basic.bad 0
SCT procs.done 4
SCT threads.done 4
SCT vfork+fork.parents-saw-child 70
SCT signal.norestart.read -4
SCT signal.restart.read 1
SCT signal.handled 2
SCT signal.stopcont.read 1
SCT signal.stopcont.nanosleep 0
SCT mutate.inject-enter.getppid==tid 1
SCT mutate.inject-return.getppid==tid+1e6 1
SCT mutate.plain.getppid==ppid 1
SCT mutate.fault-enter.write 1
SCT tagged-calls 4361
```

Accounting for the tagged calls on ppc32: the program made 4361. The trap saw 4365 entries:
4361, plus 3 re-entries after restarts, plus one `sigreturn` that ran with r8 still holding the
tag. 4363 of those returned, 3 of them as restarts. Two entries never returned: the
`sigreturn`, and the injected-then-skipped `getppid` (the core reports no return for a skipped
call). Pairing entries with returns in the log (`num`, k1, k2) leaves only calls that never
return: `exit` (thread exits and vfork-child exits that still had the tag in r8),
`rt_sigreturn`/`sigreturn`, and the skipped `getppid`.

An earlier ppc64 BE run had 50 `bad` checks. They were an artefact of the test: in the scv pass,
the vfork child's untagged `gettid` still had the tag in r8, which scv left in place where sc had
cleared it. The test now clears r8 there, and the run above shows `bad=0`. The pid mismatches
(7, 1 and 0; the earlier BE run had 4) are open problem 1 below.

For every tagged `gettid` return on all three arches, the return value equals the pid derived
from the key registers at that return (ppc32: 2670 of 2670, ppc64: 8060 of 8060, ppc64le: 5340
of 5340; these counts are from the first runs).

## 1. Entry points

**Verified.** `sc` raises `POWERPC_EXCP_SYSCALL` with LEV in `env->error_code`
(`gen_sc`: LEV bit 0 only). `scv` raises `POWERPC_EXCP_SYSCALL_VECTORED`; it exists on ppc64
only (`helper_scv`, and only when FSCR[SCV] is set: the pseries POWER9 guest's kernel enables it
and advertises it in `AT_HWCAP2` `PPC_FEATURE2_SCV`). Both reach `powerpc_excp()` with NIP still
at the instruction; every per-model handler (`powerpc_excp_{40x,6xx,7xx,74xx,booke,books}`)
then does `env->nip += 4`.

The hook goes at the top of `powerpc_excp()`, before the per-model dispatch. One hook covers
every exception model:

```c
if ((excp == POWERPC_EXCP_SYSCALL || excp == POWERPC_EXCP_SYSCALL_VECTORED) &&
    syscall_trap_enabled() && FIELD_EX64(env->msr, MSR, PR) && ppc_sc_enter(cpu, excp)) {
    powerpc_reset_excp_state(cpu);      /* skipped: no exception */
    return;
}
```

- Only LEV 0 is reported. `sc 1` is a hypercall (from the kernel, so PR=0 excludes it already),
  and `scv 1..127` is not a Linux syscall.
- `pc` is NIP + 4 and `len` is 4, for both `sc` and `scv`. `scv` returns to LR, which the
  instruction set to NIP + 4.
- Skip: NIP += 4, set the result (below), and do not deliver the exception.
- Patch 0005's hypercall (`xori r10,r10,0` in translation) is a different path and is unaffected.
  The pseries guests' `sc 1` hcalls pass through untouched (PR=0), and every guest booted with the
  trap watching all numbers.

## 2. ABIs

**Verified** for ppc64 native (BE and LE), ppc32 compat on the ppc64 BE kernel, and native ppc32
on a 32-bit BookE kernel.

- ABI: `SYSCALL_ABI_PPC64` when `msr_is_64bit(env, env->msr)` (MSR[SF], or MSR[CM] on BookE),
  otherwise `SYSCALL_ABI_PPC`. A compat task on a ppc64 kernel has SF=0.
- Number: r0. Arguments: r3–r8. A 32-bit task's values are truncated to 32 bits.
- The ppc64le kernel has no 32-bit userland, so compat was tested on BE only.

```
E cpu1 abi=7 v=0 num=207 ... k1=0xc0000000074b7e50 k2=0xffca3780 pid=244     (sct32 on the ppc64 BE kernel)
E cpu0 abi=6 v=0 num=207 ... k1=0xc00000000571fe50 k2=0x7fffd7789090 pid=83  (sct64)
E cpu0 abi=7 v=0 num=3 a0=0xffffffff ... k1=0xc5c6d5c0 k2=0xbfe9ca20 pid=83   (ppc32 kernel)
```

The key has the same width for a compat call as for a native one: k1 is the 64-bit
`paca->kstack` in both cases.

## 3. Return detection

**Verified.** Every return-from-interrupt form goes through `do_rfi()` in
`target/ppc/tcg-excp_helper.c`: `rfi`, `rfid`, `hrfid`, `rfscv`, and `rfci`/`rfdi`/`rfmci`/40x
`rfci`. The hook sits at its end, after the new MSR and NIP are in place:

```c
if (unlikely(syscall_trap_waiting()) && FIELD_EX64(env->msr, MSR, PR)) {
    ppc_sc_return(env);
}
```

- Cost when no call is pending: one load and one branch inside a helper that already exists.
  `rfi` is always a helper call that ends the TB. The entry check, `syscall_trap_enabled()`,
  runs only on the syscall exception path.
- A coarse wall-clock comparison (ppc64le, whole boot plus test, about 20 s, two runs each)
  showed no difference above noise between three builds: no consumer, consumer installed with
  nothing watched, and everything watched. This was not a cost measurement; the budget test is
  still to do.
- The return goes to user mode through `rfscv` (LR, CTR) for plain `scv` returns, and through
  `rfid` (SRR0/1) for `sc` and for `scv` returns that need a full register restore (restart,
  signal delivery). Both go through `do_rfi()`; restarts of `scv` calls were seen there
  (§5).
- A state-exact fault from the return path works: `do_rfi()` has set NIP and MSR, so
  `syscall_trap_fault(cs, 0)` raises the DSI as if from user mode. The inject-at-return test
  exercised the REWIND path.
- Not covered: a kernel that returns to user mode with `mtmsr`/`mtmsrd` instead of an rfi form.
  Linux does not do this.

## 4. Task key

**Verified for Linux on Book3S-64 (pseries, POWER9, both endians) and 32-bit BookE (e500mc).
Not tested on book3s32 (6xx/7xx/74xx), on powernv (hypervisor-mode kernel), or on 64-bit BookE.**

What the registers hold while a task is in user mode:

| Kernel | SPRG0 | SPRG1 | SPRG2 | SPRG3 |
|---|---|---|---|---|
| ppc64 Book3S (pseries) | 0 | PACA (per CPU) | PACA / scratch | vDSO CPU number (USPRG3) |
| ppc32 BookE | scratch | scratch | scratch | `&current->thread` (SPRN_SPRG_THREAD) |

```
ppc64: sprg0-3=0,0xc000000003d30000,0xc000000003d30000,0      (cpu0)
       sprg0-3=0,0xc00000001ffeec80,0xc00000001ffeec80,0x1    (cpu1)
ppc32: sprg0-3=0,0,0,0xc5c6d5c0   k1=0xc5c6d5c0 pid=83
```

- **ppc32:** k1 = SPRG3 = `&current->thread`. The kernel's `_switch` writes it, so it is per
  task. Subtracting `offsetof(task_struct, thread)` (1984) gives the task whose `pid` equals the
  `gettid` result in all 2670 cases. On book3s32, Linux keeps the physical address of the thread
  struct there, which would still be per task; not tested.
- **ppc64:** no register is per task while a task is in user mode. r13 is the user's thread
  pointer then, and the PACA in r13 only after `GET_PACA`; SPRG1 is the PACA, which is per CPU.
  k1 is therefore `paca->kstack` (PACAKSAVE), the current task's kernel stack top, read
  physically at `PACA & 0x0fffffffffffffff` in the kernel's endianness. `_switch` writes it at
  every context switch, so it stays fixed for a task across migrations. This needs the field's
  offset. The prototype learns it without symbols: after a syscall entry from user mode, the
  first privileged `ld r1,D(r13)` the kernel executes is `ld r1,PACAKSAVE(r13)` (both
  `system_call_common` and `system_call_vectored_common` begin `mr r10,r1; ld r1,PACAKSAVE(r13)`).
  The translator emits a helper for privileged `ld r1,D(r13)` only while the offset is unknown.
  The learnt offset, 2320, equals `paca_struct.kstack` in the kernel's ISF.
  - The PACA comes from SPRG1 for a guest kernel and HSPRG0 for a hypervisor kernel (MSR[HV]).
    HSPRG0 is not tested.
  - The first call after boot (or after QEMU starts) is keyed with k1=0, before the offset is
    learnt, so it is not paired.
  - The offset is host state, learnt once per process. A different kernel loaded later in the
    same process (`loadvm` of another guest) would need it reset; the prototype does not do this.
- **k2:** the user r1. Every k1 above already separates tasks; k2 separates a signal handler's
  calls from the call it interrupted, as on the other arches.
- **Concurrency and vfork (verified):** On ppc64, logged pairs that entered on one vCPU and returned on the other: 5 (BE) and 7 (LE).
  The same `(pc, user sp)` was used by more than one k1 9 times (BE) and 7 times (LE), so only k1 told those tasks
  apart: fork children inherit the parent's sp, and vfork children run on it. The vfork child
  has a different k1 with the same pc region and the same sp. The parent's return is paired with
  ret = the child's pid, and the child's return from vfork pairs with nothing:

  ```
  E cpu0 abi=7 v=0 num=189 ...   pc=0x10010b4c k1=0xc5c6d5c0 k2=0xbfe9ca20 pid=83   (parent vfork)
  E cpu0 abi=7 v=0 num=207 a0=0x5c pc=0x10010b70 k1=0xc70f2ec0 k2=0xbfe9ca20 pid=92  (child, parent's stack)
  R cpu0 abi=7 v=0 num=207 ret=92 ...              k1=0xc70f2ec0 k2=0xbfe9ca20 pid=92
  R cpu0 abi=7 v=0 num=189 ret=92 pc=0x10010b4c k1=0xc5c6d5c0 k2=0xbfe9ca20 pid=83   (parent's return)
  ```

  The clone and clone3 parents' returns were all > 0 (consumer check), and the CLONE_VM threads'
  reads, all through the same `sc`, each returned that thread's own byte count (`bad=0`).
- **Open problem 1: key reuse after a task dies.** k1 is an address, and the kernel reuses
  task_struct and stack memory. An `exit` entry stays pending forever. When a new task gets the
  same task_struct or stack and returns to user mode at the same pc and sp, it is paired as the
  dead task's `exit` return: ppc32 7 cases, ppc64 BE 4 and 1 in two runs, ppc64le 0.

  ```
  PID-MISMATCH num=1 enter-pid=146 return-pid=156 ret=0
  ```

  In the test every call goes through one `sc` instruction, so a fork child's first return
  lands exactly at the `exit` site. With per-call-site `sc` (most of libc) this needs a shared
  syscall wrapper, but `syscall(2)` is one. The same holds for x86 (CR3) and arm (the SVC sp).
  Options: drop a task's pending calls when its kernel stack is reused (no symbol-free signal
  for that found yet); add a per-incarnation value such as pid or start time to the key (needs
  offsets, from OSI/BTF); or report `exit`/`exit_group` returns only when they come back to
  their own pc with the task's own key and the expected sp **and** the consumer opted in.
  Undecided.

## 5. Return value and restarts

**Verified.**

- `sc`: an error is a positive errno in r3 with cr0.SO set (`crf[0] & CRF_SO`); success clears
  SO (the kernel clears it on entry). `get_ret` = SO ? -r3 : r3. `set_ret` writes r3 and SO.
- `scv`: r3 holds -errno; CR is not touched (volatile in the scv ABI), so SO must not be read.
- The return path cannot tell `sc` from `scv`: both can return through `rfid`. The variant has to
  be kept with the pending call. The prototype adds `CPUState.syscall_variant` (0 sc, 1 scv),
  stored in `SyscallTrap.variant`, and `syscall_trap_variant(pc, k1, k2)` to look it up before
  `syscall_trap_return()`. That costs a second locked lookup per pending return; a core-called
  `get_ret(cs, abi, variant)` would remove it.
- EBADF: tagged `read(-1)` returned -9 at the trap for `sc` and `scv` (`checks bad=0`).
- Signal without SA_RESTART (the kernel's `-ERESTARTSYS` turned into EINTR): reported as -4 for
  sc, scv and compat:

  ```
  R cpu0 abi=6 v=0 num=3 ret=-4 a0=0x13 pc=0x10010570 ...
  R cpu0 abi=6 v=1 num=3 ret=-4 a0=0x25 pc=0x10011ecc ...
  R cpu1 abi=7 v=0 num=3 ret=-4 a0=0x13 pc=0x100115b8 ...
  ```

- Restart (SA_RESTART handler, or SIGSTOP/SIGCONT with no handler). The kernel
  (`check_syscall_restart`) sets r3 = orig_gpr3 and NIP -= 4, so the task comes back to the `sc`
  or `scv` itself, and the core reports it as a restart (`pc == entry pc - len`). This happened
  directly in the no-handler case, and through `sigreturn` after the handler in the SA_RESTART
  case. **The restart code is not visible in user registers:** for `-ERESTARTSYS`,
  `-ERESTARTNOINTR` and `-ERESTARTNOHAND`, r3 is the call's first argument again (fd 19 here).
  Only `-ERESTART_RESTARTBLOCK` survives: r3 keeps 516 and r0 becomes `__NR_restart_syscall`
  (0), and the re-entry is reported as number 0:

  ```
  R ... v=0 num=3   ret=-19  a0=0x13 ... RESTART            (sc: r3=orig_gpr3=19, SO still set)
  R ... v=1 num=3   ret=37   a0=0x25 ... RESTART            (scv: r3=orig_gpr3=37)
  R ... v=0 num=162 ret=-516 ... RESTART                    (nanosleep, RESTARTBLOCK)
  E ... v=0 num=0   a0=0x204 ...                            (restart_syscall)
  E ... v=1 num=0   a0=0xfffffffffffffdfc ...               (scv: r3 = -516)
  ```

  On PPC, then, a restart is reported with `restart` set and no reliable restart code, except
  RESTARTBLOCK. The proposal: `get_ret` returns `-ERESTART_RESTARTBLOCK` when r0 == 0 at a
  restart, and the design states that the other codes cannot be told apart on this arch. The
  kernel's `raw_syscalls:sys_exit` tracepoint does see -512; the truth test has to allow for
  this difference on PPC.

## 6. Fault, inject, rewind, restore

**Verified (prototype)** on all three guests, for `sc` and `scv` and for compat tasks:

- **INJECT at entry, then REWIND:** at a tagged `getppid` the consumer injects `gettid`. The task
  comes back to the trap with its registers restored (r0 = 64, r3–r8), the consumer gets
  `injected` with `inject_ret` = the task's tid, and then skips the call with that value. The
  guest saw `getppid() == gettid()`. Rewind is r0/r3–r8 back and NIP -= 4; for `scv` that is the
  `scv` again (LR is clobbered, as the scv ABI allows).
- **INJECT at return, then RESTORE:** `gettid` injected at `getppid`'s return. The task gets r0
  and r4–r8 back plus the new r3 (set to inject_ret + 1000000, normalized), and the guest saw
  tid + 1000000.
- **FAULT at entry:** a `write` from an untouched anonymous page. On Book3S one fault was enough
  (`fault req=1, resumed readable=1` per pass). **On BookE it took two:** the first DSI is the
  page fault, and the kernel does not preload the TLB entry, so the next access is another
  software TLB miss, and `syscall_trap_read()`, which on BookE can only look up the TLB, fails
  again. A consumer, or L2's fault-in loop, has to retry until the read succeeds (bounded).
  The trap does not need to change.
- caps: `SYSCALL_ARCH_FAULT | _INJECT | _REWIND | _RESTORE` for both ABIs on both targets.
  `syscall_inject_abis` = PPC64 | PPC on ppc64, PPC on ppc32.
- Record/replay with these mutations was not tested.

## Proposed `SyscallTrapArch` for ppc and ppc64

The contract needs one addition: a per-call **variant**. It is set by `decode`, kept by the core
with the pending call, and passed back to the value hooks. PPC needs it for sc vs scv; MIPS
o32/n32/n64 and x86 int80/sysenter might use it too.

```c
typedef struct SyscallTrapEntry {
    int abi;
    unsigned variant;       /* new: kept with the call; PPC 0 sc, 1 scv */
    int64_t num;
    uint64_t args[6];
    uint64_t pc;
    unsigned len;
    uint64_t usp;
} SyscallTrapEntry;

/* get_ret/set_ret/restarting take (cs, abi, variant) */

static bool ppc_decode(CPUState *cs, SyscallTrapEntry *e)
{
    /* called from powerpc_excp() for SYSCALL / SYSCALL_VECTORED with MSR[PR] */
    CPUPPCState *env = cpu_env(cs);
    bool is64 = msr_is_64bit(env, env->msr);

    if (env->error_code != 0) {
        return false;                        /* LEV != 0 */
    }
    e->abi = is64 ? SYSCALL_ABI_PPC64 : SYSCALL_ABI_PPC;
    e->variant = cs->exception_index == POWERPC_EXCP_SYSCALL_VECTORED;
    e->num = is64 ? env->gpr[0] : (uint32_t)env->gpr[0];
    for (int i = 0; i < 6; i++) {
        e->args[i] = is64 ? env->gpr[3 + i] : (uint32_t)env->gpr[3 + i];
    }
    e->pc = env->nip + 4;                    /* NIP is still at the sc/scv */
    e->len = 4;
    e->usp = env->gpr[1];
    return true;
}

static void ppc_key(CPUState *cs, SyscallTrapKey *k)
{
    /* Book3S-64: k1 = paca->kstack (offset learnt from ld r1,D(r13)), percpu = PACA
     * (SPRG1, HSPRG0 under MSR[HV]); ppc32 / BookE: k1 = percpu = SPRG3;
     * k2 = user r1; kmmu = BookE (1 << 1), Book3S (1 << 1) | (1 << 5). */
}

static int64_t ppc_get_ret(CPUState *cs, int abi, unsigned variant)
{
    /* sc: cr0.SO ? -r3 : r3; scv: r3; 32-bit ABI: sign-extended from 32 bits */
}

static void ppc_set_ret(CPUState *cs, int abi, unsigned variant, int64_t ret)
{
    /* sc: -4095..-1 -> r3 = -ret, SO = 1, else r3 = ret, SO = 0; scv: r3 = ret */
}

static bool ppc_restarting(CPUState *cs, int abi, unsigned variant)
{
    /* NIP == entry pc - 4, decided by the core; r0 == 0 marks RESTARTBLOCK */
}

static void ppc_set_args(CPUState *cs, int abi, const uint64_t a[6]);   /* r3-r8 */
static void ppc_inject(CPUState *cs, int abi, int64_t num, const uint64_t a[6]); /* r0, r3-r8 */
static void ppc_rewind(CPUState *cs, const SyscallTrapRewind *rw);  /* r0, r3-r8; NIP -= len */
static void ppc_restore(CPUState *cs, const SyscallTrapRewind *rw, int64_t ret);
                                         /* r0, r4-r8, then set_ret(ret) */

static const SyscallTrapArch ppc_syscall_arch = {
    .caps = SYSCALL_ARCH_FAULT | SYSCALL_ARCH_INJECT | SYSCALL_ARCH_REWIND |
            SYSCALL_ARCH_RESTORE,
    .decode = ppc_decode, .key = ppc_key, .get_ret = ppc_get_ret,
    .set_ret = ppc_set_ret, .restarting = ppc_restarting, .set_args = ppc_set_args,
    .inject = ppc_inject, .rewind = ppc_rewind, .restore = ppc_restore,
};
```

Hook sites: entry at the top of `powerpc_excp()`; return at the end of `do_rfi()`, gated on
`syscall_trap_waiting()` and the new MSR[PR]. The Book3S-64 key also needs a translation hook
for the PACAKSAVE offset (`do_ldst_D` for a privileged `ld r1,D(r13)`).

## Verified and not verified

Verified on running guests: everything marked **Verified** above, on Linux 6.13 under pseries
POWER9 (BE and LE, 2 vCPUs) and ppce500 e500mc (1 vCPU).

Not verified:
- book3s32 kernels (mac99, g3beige); powernv and hypervisor-mode kernels (HSPRG0); 64-bit BookE
  (e5500/e6500), for which the prototype falls back to SPRG3, probably wrongly (Linux keeps the
  PACA in an SPRG there too)
- ppc32 SMP: ppce500 with `-smp 2` does not boot in this setup, with or without the trap
- record/replay and snapshots with pending or held calls; resetting the learnt PACAKSAVE when a
  different kernel is loaded
- KVM: the trap is TCG-only, and the hooks are compiled only with CONFIG_TCG
- performance against the cost table
- glibc/musl userland: the test is freestanding; busybox's own calls were traced and paired
  (`pid-pairing ok` covers every call, tagged or not)

## Open problems

1. Key reuse after a task dies (§4): `exit`/`exit_group` entries alias with a later task's return
   at the same pc and sp. This affects every arch whose key is an address.
2. The restart code is not recoverable on PPC except `ERESTART_RESTARTBLOCK` (§5).
3. The contract needs a per-call variant (sc/scv) passed to `get_ret`/`set_ret` (§5).
4. On BookE, fault-in needs up to two rounds (§6).
5. ppc64 calls made before PACAKSAVE is learnt are not paired (the first call after start).
6. A skipped call reports no return from the core; the design says it should report one. This
   is not PPC-specific.
