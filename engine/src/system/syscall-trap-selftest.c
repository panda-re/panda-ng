/*
 * A test consumer of the syscall trap (prototype; MIPS ABIs only).
 *
 *   SYSCALL_TRAP_SELFTEST=<log file>
 *
 * Watches every number of every MIPS ABI, entry and return, and writes one
 * line per event.  getpid with a magic first argument asks for a mutation:
 *
 *   0x1a1ec700  inject getppid at the entry, then run getpid
 *   0x1a1ec701  inject getppid at the return
 *   0x1a1ec702  inject pwrite64(a1, a2, a3, offset 4096) at the entry
 *   0xfa017000  fault on a1 (read) at the entry
 *   0xfa017001  fault on a1 (read) at the return
 *   0x5c1f0000  skip, returning a1
 *   0x5e7e0000  set the return value to a1
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/cpu.h"
#include "system/syscall-trap.h"
#include "qemu/target-info.h"

static FILE *st_out;
static uint64_t st_seq;

static int64_t st_nr(int abi, int o32, int n64, int n32)
{
    return abi == SYSCALL_ABI_MIPS ? o32 : abi == SYSCALL_ABI_MIPS_N64 ? n64 :
           abi == SYSCALL_ABI_MIPS_N32 ? n32 : -1;
}

static void st_keys(CPUState *cpu, char *buf, size_t n)
{
    buf[0] = 0;
    if (syscall_trap_debug_keys) {
        syscall_trap_debug_keys(cpu, buf, n);
    }
}

/*
 * Back after a fault: fault again while the page is still not readable (a
 * MIPS TLB refill can come back with the page still invalid).
 */
static bool st_refault(CPUState *cpu, SyscallTrap *sc)
{
    uint8_t b;
    vaddr bad;
    bool ok = syscall_trap_access(cpu, sc->args[1], &b, 1, false, &bad);

    fprintf(st_out, "F id=%" PRIu64 " readable=%d\n", sc->task, ok);
    if (!ok) {
        sc->fault = true;
        sc->fault_addr = sc->args[1];
        sc->fault_write = false;
    }
    return !ok;
}

static void st_enter(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    char k[512];
    bool marker = sc->num == st_nr(sc->abi, 4020, 5038, 6038);
    uint32_t m = sc->args[0];

    if (!sc->resumed) {
        sc->task = ++st_seq;
    }
    st_keys(cpu, k, sizeof(k));
    fprintf(st_out, "E id=%" PRIu64 " abi=%d num=%" PRId64 " a=%" PRIx64 ",%"
            PRIx64 ",%" PRIx64 ",%" PRIx64 ",%" PRIx64 ",%" PRIx64 " pc=%" PRIx64
            " resumed=%d injected=%d inject_ret=%" PRId64 " %s\n",
            sc->task, sc->abi, sc->num, sc->args[0], sc->args[1], sc->args[2],
            sc->args[3], sc->args[4], sc->args[5], sc->pc, sc->resumed,
            sc->injected, sc->injected ? sc->inject_ret : 0, k);
    if (marker && sc->resumed && m == 0xfa017000 && st_refault(cpu, sc)) {
        return;
    }
    if (!marker || sc->resumed) {
        return;
    }
    memset(sc->inject_args, 0, sizeof(sc->inject_args));
    switch (m) {
    case 0x1a1ec700:
        sc->inject = true;
        sc->inject_num = st_nr(sc->abi, 4064, 5108, 6108);
        break;
    case 0x1a1ec702:
        sc->inject = true;
        sc->inject_num = st_nr(sc->abi, 4201, 5017, 6017);
        sc->inject_args[0] = sc->args[1];
        sc->inject_args[1] = sc->args[2];
        sc->inject_args[2] = sc->args[3];
        if (sc->abi == SYSCALL_ABI_MIPS) {
            /* o32: a 64-bit argument takes an aligned pair: a3 pad, then the stack */
            sc->inject_args[target_big_endian() ? 5 : 4] = 4096;
        } else {
            sc->inject_args[3] = 4096;
        }
        break;
    case 0xfa017000:
        sc->fault = true;
        sc->fault_addr = sc->args[1];
        sc->fault_write = false;
        break;
    case 0x5c1f0000:
        sc->skip = true;
        sc->ret = (int32_t)sc->args[1];
        break;
    }
}

static void st_ret(CPUState *cpu, SyscallTrap *sc, void *opaque)
{
    char k[512];
    bool marker = sc->num == st_nr(sc->abi, 4020, 5038, 6038);
    uint32_t m = sc->args[0];

    st_keys(cpu, k, sizeof(k));
    fprintf(st_out, "R id=%" PRIu64 " abi=%d num=%" PRId64 " ret=%" PRId64
            " restart=%d resumed=%d injected=%d inject_ret=%" PRId64 " %s\n",
            sc->task, sc->abi, sc->num, sc->ret, sc->restart, sc->resumed,
            sc->injected, sc->injected ? sc->inject_ret : 0, k);
    if (marker && sc->resumed && m == 0xfa017001 && st_refault(cpu, sc)) {
        return;
    }
    if (!marker || sc->resumed || sc->restart) {
        return;
    }
    switch (m) {
    case 0x1a1ec701:
        sc->inject = true;
        sc->inject_num = st_nr(sc->abi, 4064, 5108, 6108);
        memset(sc->inject_args, 0, sizeof(sc->inject_args));
        break;
    case 0xfa017001:
        sc->fault = true;
        sc->fault_addr = sc->args[1];
        sc->fault_write = false;
        break;
    case 0x5e7e0000:
        sc->ret = (int32_t)sc->args[1];
        break;
    }
}

static const SyscallTrapOps st_ops = {
    .enter = st_enter,
    .ret = st_ret,
};

static void __attribute__((constructor)) st_init(void)
{
    const char *f = getenv("SYSCALL_TRAP_SELFTEST");

    if (!f || !*f) {
        return;
    }
    st_out = fopen(f, "w");
    if (!st_out) {
        return;
    }
    setvbuf(st_out, NULL, _IOLBF, 0);
    syscall_trap_set_consumer(&st_ops, NULL);
    syscall_trap_watch(SYSCALL_ABI_MIPS, -1, SYSCALL_TRAP_ENTER | SYSCALL_TRAP_RETURN);
    syscall_trap_watch(SYSCALL_ABI_MIPS_N32, -1, SYSCALL_TRAP_ENTER | SYSCALL_TRAP_RETURN);
    syscall_trap_watch(SYSCALL_ABI_MIPS_N64, -1, SYSCALL_TRAP_ENTER | SYSCALL_TRAP_RETURN);
}
