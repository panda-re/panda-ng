/* The freestanding guest programs' system layer (agent.c and the other <name>.c programs): system calls,
 * syscall numbers, the PANDA hypercall, thread spawn and the entry point, per arch. No libc.
 *
 * An arch provides here:
 *   sc(n, a..f)      a system call with six arguments, the result in the return register
 *   hc(op, a, b, c)  the PANDA hypercall MAGIC_AGENT: the number register and argument 0 hold the
 *                    magic, arguments 1-4 op, a, b, c (pandare2's "syscall" convention 2-5); the
 *                    host's answer comes back in the return register
 *   hc_setup()       whatever the task needs before its first hypercall (x86: ioperm)
 *   clone_fn(flags, stack_top, ctid, fn)
 *                    clone(flags) onto stack_top; the child calls fn() then exit(0); the parent
 *                    gets the child's tid
 *   NR_<name>        the numbers of the calls the programs make (asm-generic names; NR_vfork is
 *                    only on arches that have it)
 *   sc() returns -errno on an error on every arch (MIPS and PowerPC report errors with a flag)
 *   _start           aligns the stack and calls _start_c(sp) with the initial stack pointer
 * Built by tests/programs/build.sh with -nostdlib -ffreestanding. */
#ifndef PANDA_TEST_SYS_H
#define PANDA_TEST_SYS_H

typedef unsigned long long u64;
typedef unsigned int u32;

#define MAGIC_AGENT 0x50416774

#if defined(__x86_64__)
#define NR_read 0
#define NR_write 1
#define NR_close 3
#define NR_lseek 8
#define NR_mmap 9
#define NR_munmap 11
#define NR_rt_sigaction 13
#define NR_rt_sigreturn 15
#define NR_nanosleep 35
#define NR_setitimer 38
#define NR_getpid 39
#define NR_clone 56
#define NR_fork 57
#define NR_vfork 58
#define NR_execve 59
#define NR_exit 60
#define NR_wait4 61
#define NR_kill 62
#define NR_getppid 110
#define NR_setsid 112
#define NR_mount 165
#define NR_ioperm 173
#define NR_futex 202
#define NR_restart_syscall 219
#define NR_exit_group 231
#define NR_openat 257
#define NR_mkdirat 258
#define NR_dup3 292
#define NR_pipe2 293
#define NR_clone3 435

static inline long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long r10 __asm__("r10") = d, r8 __asm__("r8") = e, r9 __asm__("r9") = f;
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8),
                     "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

/* penguin's x86 hypercall: a write to I/O port 0x88 */
static inline long hc(long op, long a, long b, long c)
{
    register long r10 __asm__("r10") = b, r8 __asm__("r8") = c;
    long ret = MAGIC_AGENT;
    __asm__ volatile("out %%eax, $0x88" : "+a"(ret) : "D"(MAGIC_AGENT), "S"(op), "d"(a), "r"(r10),
                     "r"(r8) : "memory");
    return ret;
}

static inline void hc_setup(void) { sc(NR_ioperm, 0x88, 4, 1, 0, 0, 0); }

/* x86_64 clone: flags, stack, parent_tid, child_tid, tls */
long clone_fn(unsigned long flags, void *stack, volatile int *ctid, void (*fn)(void));
__asm__(".text\n.globl clone_fn\nclone_fn:\n"
        " mov %rcx, %r9\n mov %rdx, %r10\n xor %edx, %edx\n xor %r8d, %r8d\n"
        " mov $56, %eax\n syscall\n test %rax, %rax\n jnz 1f\n"
        " call *%r9\n mov $60, %eax\n xor %edi, %edi\n syscall\n hlt\n"
        "1: ret\n");

#define SIGRETURN_ASM " mov $15, %eax\n syscall\n hlt\n"
__asm__(".text\n.globl _start\n_start:\n xor %ebp, %ebp\n mov %rsp, %rdi\n and $-16, %rsp\n"
        " call _start_c\n hlt\n");

#elif defined(__aarch64__)
#define NR_dup3 24
#define NR_mkdirat 34
#define NR_mount 40
#define NR_openat 56
#define NR_close 57
#define NR_pipe2 59
#define NR_lseek 62
#define NR_read 63
#define NR_write 64
#define NR_exit 93
#define NR_exit_group 94
#define NR_futex 98
#define NR_nanosleep 101
#define NR_setitimer 103
#define NR_restart_syscall 128
#define NR_kill 129
#define NR_rt_sigaction 134
#define NR_rt_sigreturn 139
#define NR_setsid 157
#define NR_getpid 172
#define NR_getppid 173
#define NR_munmap 215
#define NR_clone 220
#define NR_execve 221
#define NR_mmap 222
#define NR_wait4 260
#define NR_clone3 435

static inline long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long x8 __asm__("x8") = n, x0 __asm__("x0") = a, x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c, x3 __asm__("x3") = d, x4 __asm__("x4") = e;
    register long x5 __asm__("x5") = f;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
                     : "memory");
    return x0;
}

/* penguin's arm64 hypercall instruction (an unallocated MSR immediate encoding) */
static inline long hc(long op, long a, long b, long c)
{
    register long x8 __asm__("x8") = MAGIC_AGENT, x0 __asm__("x0") = MAGIC_AGENT;
    register long x1 __asm__("x1") = op, x2 __asm__("x2") = a, x3 __asm__("x3") = b;
    register long x4 __asm__("x4") = c;
    __asm__ volatile(".inst 0xd500501f" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4)
                     : "memory");
    return x0;
}

static inline void hc_setup(void) {}

/* arm64 clone: flags, stack, parent_tid, tls, child_tid */
long clone_fn(unsigned long flags, void *stack, volatile int *ctid, void (*fn)(void));
__asm__(".text\n.globl clone_fn\nclone_fn:\n"
        " mov x9, x3\n mov x4, x2\n mov x2, #0\n mov x3, #0\n mov x8, #220\n svc #0\n"
        " cbnz x0, 1f\n blr x9\n mov x0, #0\n mov x8, #93\n svc #0\n"
        "1: ret\n");

/* brk: nothing (a signal handler) may start just past the svc, where the trap would take the
 * kernel's return to it as rt_sigreturn's, which stays pending */
#define SIGRETURN_ASM " mov x8, #139\n svc #0\n brk #0\n"
__asm__(".text\n.globl _start\n_start:\n mov x29, #0\n mov x0, sp\n and x1, x0, #-16\n mov sp, x1\n"
        " bl _start_c\n b .\n");

#elif defined(__arm__)
/* EABI, ARM state (build.sh: -marm, so r7 is free for the number) */
#define NR_restart_syscall 0
#define NR_exit 1
#define NR_read 3
#define NR_write 4
#define NR_close 6
#define NR_execve 11
#define NR_lseek 19
#define NR_getpid 20
#define NR_mount 21
#define NR_kill 37
#define NR_getppid 64
#define NR_setsid 66
#define NR_munmap 91
#define NR_setitimer 104
#define NR_wait4 114
#define NR_clone 120
#define NR_nanosleep 162
#define NR_rt_sigreturn 173
#define NR_rt_sigaction 174
#define NR_vfork 190
#define NR_futex 240
#define NR_exit_group 248
#define NR_openat 322
#define NR_mkdirat 323
#define NR_dup3 358
#define NR_pipe2 359
#define NR_clone3 435

static inline long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long r7 __asm__("r7") = n, r0 __asm__("r0") = a, r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c, r3 __asm__("r3") = d, r4 __asm__("r4") = e;
    register long r5 __asm__("r5") = f;
    __asm__ volatile("svc #0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
                     : "memory");
    return r0;
}

/* penguin's arm hypercall instruction: mcr p7, 0, r0, c0, c0, 0 (the number in r7) */
static inline long hc(long op, long a, long b, long c)
{
    register long r7 __asm__("r7") = MAGIC_AGENT, r0 __asm__("r0") = MAGIC_AGENT;
    register long r1 __asm__("r1") = op, r2 __asm__("r2") = a, r3 __asm__("r3") = b;
    register long r4 __asm__("r4") = c;
    __asm__ volatile("mcr p7, 0, r0, c0, c0, 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2), "r"(r3),
                     "r"(r4) : "memory");
    return r0;
}

static inline void hc_setup(void) {}

/* arm clone: flags, stack, parent_tid, tls, child_tid */
long clone_fn(unsigned long flags, void *stack, volatile int *ctid, void (*fn)(void));
__asm__(".text\n.arm\n.globl clone_fn\nclone_fn:\n"
        " push {r4, r7}\n mov ip, r3\n mov r4, r2\n mov r2, #0\n mov r3, #0\n mov r7, #120\n"
        " svc #0\n cmp r0, #0\n bne 1f\n blx ip\n mov r0, #0\n mov r7, #1\n svc #0\n"
        "1: pop {r4, r7}\n bx lr\n");

#define SIGRETURN_ASM " mov r7, #173\n svc #0\n udf #0\n"
__asm__(".text\n.arm\n.globl _start\n_start:\n mov fp, #0\n mov r0, sp\n bic r1, r0, #7\n"
        " mov sp, r1\n bl _start_c\n b .\n");

#elif defined(__i386__)
/* a 32-bit kernel (the i386 target): int $0x80 */
#define NR_restart_syscall 0
#define NR_exit 1
#define NR_read 3
#define NR_write 4
#define NR_close 6
#define NR_execve 11
#define NR_lseek 19
#define NR_getpid 20
#define NR_mount 21
#define NR_kill 37
#define NR_getppid 64
#define NR_setsid 66
#define NR_munmap 91
#define NR_ioperm 101
#define NR_setitimer 104
#define NR_wait4 114
#define NR_clone 120
#define NR_nanosleep 162
#define NR_rt_sigreturn 173
#define NR_rt_sigaction 174
#define NR_vfork 190
#define NR_futex 240
#define NR_exit_group 252
#define NR_openat 295
#define NR_mkdirat 296
#define NR_dup3 330
#define NR_pipe2 331
#define NR_clone3 435

/* argument 6 goes in ebp, which the compiler may not hand out: through memory */
static inline long sc(long n, long a, long b, long c, long d, long e, long f)
{
    long nf[2] = { n, f }, ret;
    __asm__ volatile("push %%ebp\n mov 4(%%eax), %%ebp\n mov (%%eax), %%eax\n int $0x80\n"
                     " pop %%ebp"
                     : "=a"(ret) : "a"(nf), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e) : "memory");
    return ret;
}

/* penguin's x86 hypercall: a write to I/O port 0x88 (pandare2's "syscall" convention: eax, ebx,
 * ecx, edx, esi, edi) */
static inline long hc(long op, long a, long b, long c)
{
    long ret = MAGIC_AGENT;
    __asm__ volatile("out %%eax, $0x88" : "+a"(ret) : "b"(MAGIC_AGENT), "c"(op), "d"(a), "S"(b),
                     "D"(c) : "memory");
    return ret;
}

static inline void hc_setup(void) { sc(NR_ioperm, 0x88, 4, 1, 0, 0, 0); }

/* i386 clone: flags, stack, parent_tid, tls, child_tid; fn reaches the child on its stack */
long clone_fn(unsigned long flags, void *stack, volatile int *ctid, void (*fn)(void));
__asm__(".text\n.globl clone_fn\nclone_fn:\n"
        " push %ebx\n push %esi\n push %edi\n"
        " mov 16(%esp), %ebx\n mov 20(%esp), %ecx\n mov 24(%esp), %edi\n mov 28(%esp), %eax\n"
        " mov %eax, -4(%ecx)\n sub $4, %ecx\n xor %edx, %edx\n xor %esi, %esi\n"
        " mov $120, %eax\n int $0x80\n test %eax, %eax\n jnz 1f\n"
        " pop %eax\n and $-16, %esp\n call *%eax\n mov $1, %eax\n xor %ebx, %ebx\n int $0x80\n hlt\n"
        "1: pop %edi\n pop %esi\n pop %ebx\n ret\n");

#define SIGRETURN_ASM " mov $173, %eax\n int $0x80\n hlt\n"
__asm__(".text\n.globl _start\n_start:\n xor %ebp, %ebp\n mov %esp, %eax\n and $-16, %esp\n"
        " sub $12, %esp\n push %eax\n call _start_c\n hlt\n");

#elif defined(__riscv) && __riscv_xlen == 64
#define NR_dup3 24
#define NR_mkdirat 34
#define NR_mount 40
#define NR_openat 56
#define NR_close 57
#define NR_pipe2 59
#define NR_lseek 62
#define NR_read 63
#define NR_write 64
#define NR_exit 93
#define NR_exit_group 94
#define NR_futex 98
#define NR_nanosleep 101
#define NR_setitimer 103
#define NR_restart_syscall 128
#define NR_kill 129
#define NR_rt_sigaction 134
#define NR_rt_sigreturn 139
#define NR_setsid 157
#define NR_getpid 172
#define NR_getppid 173
#define NR_munmap 215
#define NR_clone 220
#define NR_execve 221
#define NR_mmap 222
#define NR_wait4 260
#define NR_clone3 435

static inline long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long a7 __asm__("a7") = n, a0 __asm__("a0") = a, a1 __asm__("a1") = b;
    register long a2 __asm__("a2") = c, a3 __asm__("a3") = d, a4 __asm__("a4") = e;
    register long a5 __asm__("a5") = f;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5)
                     : "memory");
    return a0;
}

/* penguin's riscv hypercall instruction: xori x0, x0, 0 (the number in a7) */
static inline long hc(long op, long a, long b, long c)
{
    register long a7 __asm__("a7") = MAGIC_AGENT, a0 __asm__("a0") = MAGIC_AGENT;
    register long a1 __asm__("a1") = op, a2 __asm__("a2") = a, a3 __asm__("a3") = b;
    register long a4 __asm__("a4") = c;
    __asm__ volatile(".4byte 0x00004013" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2), "r"(a3),
                     "r"(a4) : "memory");
    return a0;
}

static inline void hc_setup(void) {}

/* riscv clone: flags, stack, parent_tid, tls, child_tid */
long clone_fn(unsigned long flags, void *stack, volatile int *ctid, void (*fn)(void));
__asm__(".text\n.globl clone_fn\nclone_fn:\n"
        " mv t1, a3\n mv a4, a2\n li a2, 0\n li a3, 0\n li a7, 220\n ecall\n"
        " bnez a0, 1f\n jalr t1\n li a0, 0\n li a7, 93\n ecall\n"
        "1: ret\n");

#define SIGRETURN_ASM " li a7, 139\n ecall\n ebreak\n"
__asm__(".text\n.globl _start\n_start:\n li s0, 0\n mv a0, sp\n andi sp, sp, -16\n"
        " call _start_c\n1: j 1b\n");

#elif defined(__mips__)
/* o32 (mips, mipsel) or n64 (mips64, mips64el): an error sets $a3, with the positive error
 * number in $v0; o32 passes arguments 5 and 6 on the stack (16 and 20 bytes above $sp) */
#if _MIPS_SIM == _ABIO32
#define NR_BASE 4000
#define NR_read 4003
#define NR_write 4004
#define NR_close 4006
#define NR_execve 4011
#define NR_lseek 4019
#define NR_getpid 4020
#define NR_mount 4021
#define NR_kill 4037
#define NR_getppid 4064
#define NR_setsid 4066
#define NR_munmap 4091
#define NR_setitimer 4104
#define NR_wait4 4114
#define NR_clone 4120
#define NR_nanosleep 4166
#define NR_rt_sigreturn 4193
#define NR_rt_sigaction 4194
#define NR_futex 4238
#define NR_exit_group 4246
#define NR_restart_syscall 4253
#define NR_openat 4288
#define NR_mkdirat 4289
#define NR_dup3 4327
#define NR_pipe2 4328
#define NR_exit 4001
#define NR_clone3 4435
#else
#define NR_read 5000
#define NR_write 5001
#define NR_close 5003
#define NR_lseek 5008
#define NR_munmap 5011
#define NR_rt_sigaction 5013
#define NR_nanosleep 5034
#define NR_setitimer 5036
#define NR_getpid 5038
#define NR_clone 5055
#define NR_execve 5057
#define NR_exit 5058
#define NR_wait4 5059
#define NR_kill 5060
#define NR_getppid 5108
#define NR_setsid 5110
#define NR_mount 5160
#define NR_futex 5194
#define NR_exit_group 5205
#define NR_rt_sigreturn 5211
#define NR_restart_syscall 5213
#define NR_openat 5247
#define NR_mkdirat 5248
#define NR_dup3 5286
#define NR_pipe2 5287
#define NR_clone3 5435
#endif

#if _MIPS_SIM == _ABIO32
static inline long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long v0 __asm__("$2") = n, a0 __asm__("$4") = a, a1 __asm__("$5") = b;
    register long a2 __asm__("$6") = c, a3 __asm__("$7") = d;
    __asm__ volatile(".set push\n.set noreorder\n addiu $sp, $sp, -32\n sw %5, 16($sp)\n"
                     " sw %6, 20($sp)\n syscall\n addiu $sp, $sp, 32\n.set pop"
                     : "+r"(v0), "+r"(a3) : "r"(a0), "r"(a1), "r"(a2), "r"(e), "r"(f)
                     : "$1", "$3", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24",
                       "$25", "hi", "lo", "memory");
    return a3 ? -v0 : v0;
}

/* penguin's mips hypercall instruction: movz $0, $0, $0 (the number in $v0; o32: argument 5,
 * c, on the stack as for a system call) */
static inline long hc(long op, long a, long b, long c)
{
    register long v0 __asm__("$2") = MAGIC_AGENT, a0 __asm__("$4") = MAGIC_AGENT;
    register long a1 __asm__("$5") = op, a2 __asm__("$6") = a, a3 __asm__("$7") = b;
    __asm__ volatile(".set push\n.set noreorder\n addiu $sp, $sp, -32\n sw %5, 16($sp)\n"
                     " .word 0x0000000a\n addiu $sp, $sp, 32\n.set pop"
                     : "+r"(v0) : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(c) : "memory");
    return v0;
}

/* o32 clone: flags, stack, parent_tid, tls, child_tid (on the stack) */
long clone_fn(unsigned long flags, void *stack, volatile int *ctid, void (*fn)(void));
__asm__(".text\n.set push\n.set noreorder\n.globl clone_fn\nclone_fn:\n"
        " addiu $sp, $sp, -32\n sw $6, 16($sp)\n move $25, $7\n move $6, $0\n move $7, $0\n"
        " li $2, 4120\n syscall\n bnez $7, 2f\n nop\n bnez $2, 1f\n nop\n"
        " addiu $sp, $sp, -32\n jalr $25\n nop\n move $4, $0\n li $2, 4001\n syscall\n"
        "2: subu $2, $0, $2\n1: jr $31\n addiu $sp, $sp, 32\n.set pop\n");

#define SIGRETURN_ASM " li $2, 4193\n syscall\n break\n"
__asm__(".text\n.set push\n.set noreorder\n.globl _start\n_start:\n move $4, $sp\n li $8, -16\n"
        " and $sp, $sp, $8\n addiu $sp, $sp, -32\n lui $28, %hi(_gp)\n addiu $28, $28, %lo(_gp)\n"
        " jal _start_c\n nop\n1: b 1b\n nop\n.set pop\n");
#else
static inline long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long v0 __asm__("$2") = n, a0 __asm__("$4") = a, a1 __asm__("$5") = b;
    register long a2 __asm__("$6") = c, a3 __asm__("$7") = d, a4 __asm__("$8") = e;
    register long a5 __asm__("$9") = f;
    __asm__ volatile("syscall" : "+r"(v0), "+r"(a3)
                     : "r"(a0), "r"(a1), "r"(a2), "r"(a4), "r"(a5)
                     : "$1", "$3", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25",
                       "hi", "lo", "memory");
    return a3 ? -v0 : v0;
}

static inline long hc(long op, long a, long b, long c)
{
    register long v0 __asm__("$2") = MAGIC_AGENT, a0 __asm__("$4") = MAGIC_AGENT;
    register long a1 __asm__("$5") = op, a2 __asm__("$6") = a, a3 __asm__("$7") = b;
    register long a4 __asm__("$8") = c;
    __asm__ volatile(".word 0x0000000a" : "+r"(v0) : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4)
                     : "memory");
    return v0;
}

/* n64 clone: flags, stack, parent_tid, tls, child_tid */
long clone_fn(unsigned long flags, void *stack, volatile int *ctid, void (*fn)(void));
__asm__(".text\n.set push\n.set noreorder\n.globl clone_fn\nclone_fn:\n"
        " move $25, $7\n move $8, $6\n move $6, $0\n move $7, $0\n li $2, 5055\n syscall\n"
        " bnez $7, 2f\n nop\n bnez $2, 1f\n nop\n"
        " daddiu $sp, $sp, -32\n jalr $25\n nop\n move $4, $0\n li $2, 5058\n syscall\n"
        "2: dsubu $2, $0, $2\n1: jr $31\n nop\n.set pop\n");

#define SIGRETURN_ASM " li $2, 5211\n syscall\n break\n"
__asm__(".text\n.set push\n.set noreorder\n.globl _start\n_start:\n move $4, $sp\n li $8, -16\n"
        " and $sp, $sp, $8\n daddiu $sp, $sp, -32\n lui $28, %highest(_gp)\n"
        " daddiu $28, $28, %higher(_gp)\n dsll $28, $28, 16\n daddiu $28, $28, %hi(_gp)\n"
        " dsll $28, $28, 16\n daddiu $28, $28, %lo(_gp)\n jal _start_c\n nop\n1: b 1b\n nop\n"
        ".set pop\n");
#endif

static inline void hc_setup(void) {}

#elif defined(__powerpc__)
/* ppc, ppc64 (ELFv2): an error sets CR0.SO, with the positive error number in r3 */
#define NR_restart_syscall 0
#define NR_exit 1
#define NR_read 3
#define NR_write 4
#define NR_close 6
#define NR_execve 11
#define NR_lseek 19
#define NR_getpid 20
#define NR_mount 21
#define NR_kill 37
#define NR_getppid 64
#define NR_setsid 66
#define NR_munmap 91
#define NR_setitimer 104
#define NR_wait4 114
#define NR_clone 120
#define NR_nanosleep 162
#define NR_rt_sigreturn 172
#define NR_rt_sigaction 173
#define NR_vfork 189
#define NR_futex 221
#define NR_exit_group 234
#define NR_openat 286
#define NR_mkdirat 287
#define NR_dup3 316
#define NR_pipe2 317
#define NR_clone3 435

static inline long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long r0 __asm__("r0") = n, r3 __asm__("r3") = a, r4 __asm__("r4") = b;
    register long r5 __asm__("r5") = c, r6 __asm__("r6") = d, r7 __asm__("r7") = e;
    register long r8 __asm__("r8") = f;
    __asm__ volatile("sc\n bns+ 1f\n neg %1, %1\n1:"
                     : "+r"(r0), "+r"(r3), "+r"(r4), "+r"(r5), "+r"(r6), "+r"(r7), "+r"(r8)
                     : : "r9", "r10", "r11", "r12", "cr0", "ctr", "memory");
    return r3;
}

/* penguin's ppc hypercall instruction: xori 10, 10, 0 (the number in r0) */
static inline long hc(long op, long a, long b, long c)
{
    register long r0 __asm__("r0") = MAGIC_AGENT, r3 __asm__("r3") = MAGIC_AGENT;
    register long r4 __asm__("r4") = op, r5 __asm__("r5") = a, r6 __asm__("r6") = b;
    register long r7 __asm__("r7") = c;
    __asm__ volatile(".long 0x694a0000" : "+r"(r3) : "r"(r0), "r"(r4), "r"(r5), "r"(r6), "r"(r7)
                     : "memory");
    return r3;
}

static inline void hc_setup(void) {}

/* ppc clone: flags, stack, parent_tid, tls, child_tid; the child calls fn on a frame of its own.
 * fn goes to the child on its stack (the kernel's entry does not keep the volatile registers). */
long clone_fn(unsigned long flags, void *stack, volatile int *ctid, void (*fn)(void));
#ifdef __powerpc64__
__asm__(".text\n.globl clone_fn\nclone_fn:\n"
        " std 6, -8(4)\n mr 7, 5\n li 5, 0\n li 6, 0\n li 0, 120\n sc\n bns+ 1f\n neg 3, 3\n"
        " blr\n1: cmpdi 3, 0\n bnelr\n ld 12, -8(1)\n li 0, 0\n stdu 0, -128(1)\n mtctr 12\n"
        " bctrl\n li 3, 0\n li 0, 1\n sc\n");
#define SIGRETURN_ASM " li 0, 172\n sc\n trap\n"
__asm__(".text\n.globl _start\n_start:\n mr 3, 1\n clrrdi 1, 1, 4\n li 0, 0\n stdu 0, -128(1)\n"
        " lis 2, .TOC.@highest\n ori 2, 2, .TOC.@higher\n rldicr 2, 2, 32, 31\n"
        " oris 2, 2, .TOC.@h\n ori 2, 2, .TOC.@l\n bl _start_c\n nop\n1: b 1b\n");
#else
__asm__(".text\n.globl clone_fn\nclone_fn:\n"
        " stw 6, -4(4)\n mr 7, 5\n li 5, 0\n li 6, 0\n li 0, 120\n sc\n bns+ 1f\n neg 3, 3\n"
        " blr\n1: cmpwi 3, 0\n bnelr\n lwz 12, -4(1)\n li 0, 0\n stwu 0, -64(1)\n mtctr 12\n"
        " bctrl\n li 3, 0\n li 0, 1\n sc\n");
#define SIGRETURN_ASM " li 0, 172\n sc\n trap\n"
__asm__(".text\n.globl _start\n_start:\n mr 3, 1\n clrrwi 1, 1, 4\n li 0, 0\n stwu 0, -64(1)\n"
        " bl _start_c\n1: b 1b\n");
#endif

#else
#error "tests/programs/sys.h: no system layer for this arch"
#endif

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define AT_FDCWD (-100)
#define WNOHANG 1
#define SIGTRAP 5
#define SIGALRM 14
#if defined(__mips__)
#define O_APPEND 0x0008
#define O_CREAT 0x0100
#define O_TRUNC 0x0200
#define SIGUSR1 16
#define SIGCHLD 18
#define SIGSTOP 23
#define SIGCONT 25
#define SA_SIGINFO 8
#else
#define O_CREAT 0100
#define O_TRUNC 01000
#define O_APPEND 02000
#define SIGUSR1 10
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SA_SIGINFO 4
#endif
#define SA_RESTORER 0x04000000
#define SA_RESTART 0x10000000
#define EINTR 4
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define CLONE_VM 0x100
#define CLONE_FS 0x200
#define CLONE_FILES 0x400
#define CLONE_SIGHAND 0x800
#define CLONE_VFORK 0x4000
#define CLONE_THREAD 0x10000
#define CLONE_SYSVSEM 0x40000
#define CLONE_CHILD_CLEARTID 0x200000
#define CLONE_THREAD_FLAGS (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | \
                            CLONE_SYSVSEM | CLONE_CHILD_CLEARTID)

static inline long sys_open(const char *path, long flags, long mode)
{
    return sc(NR_openat, AT_FDCWD, (long)path, flags, mode, 0, 0);
}

/* fork, as clone(SIGCHLD) */
static inline long sys_fork(void) { return sc(NR_clone, SIGCHLD, 0, 0, 0, 0, 0); }

/* gcc may emit calls to these even when freestanding (and must not turn them into calls to
 * themselves). */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memset(void *d, int c, unsigned long n)
{
    volatile char *p = d;
    while (n--) {
        *p++ = c;
    }
    return d;
}

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memcpy(void *d, const void *s, unsigned long n)
{
    char *p = d;
    const char *q = s;
    while (n--) {
        *p++ = *q++;
    }
    return d;
}

static inline unsigned long str_len(const char *s)
{
    unsigned long n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

/* The kernel's struct sigaction for rt_sigaction. KSA_RESTORER: it has sa_restorer, which the
 * kernel returns through (x86, arm); riscv, MIPS and PowerPC return from a handler through the
 * vDSO (riscv and MIPS have no sa_restorer field; PowerPC ignores it). */
#if defined(__mips__)
#define KSA_RESTORER 0
#define KSIGSET 16
struct ksigaction { unsigned int flags; void *handler; unsigned long mask[16 / sizeof(long)]; };
#define KSIGACTION(h, f, r) { (f), (h), { 0 } }
#elif defined(__riscv)
#define KSA_RESTORER 0
#define KSIGSET 8
struct ksigaction { void *handler; unsigned long flags; unsigned long mask[8 / sizeof(long)]; };
#define KSIGACTION(h, f, r) { (h), (f), { 0 } }
#elif defined(__powerpc__)
#define KSA_RESTORER 0
#define KSIGSET 8
struct ksigaction { void *handler; unsigned long flags; void (*restorer)(void); unsigned long mask[2]; };
#define KSIGACTION(h, f, r) { (h), (f) | SA_RESTORER, (r), { 0 } }
#else
#define KSA_RESTORER 1
#define KSIGSET 8
struct ksigaction { void *handler; unsigned long flags; void (*restorer)(void); unsigned long mask[2]; };
#define KSIGACTION(h, f, r) { (h), (f) | SA_RESTORER, (r), { 0 } }
#endif

void sc_restorer(void);
__asm__(".text\n.globl sc_restorer\nsc_restorer:\n" SIGRETURN_ASM);

static inline void handle(int sig, void *h, unsigned long flags)
{
    /* SA_SIGINFO: an rt frame everywhere (arm sets up a non-rt one without it, which only
     * sigreturn undoes) */
    struct ksigaction a = KSIGACTION(h, flags | SA_SIGINFO, sc_restorer);
    sc(NR_rt_sigaction, sig, (long)&a, 0, KSIGSET, 0, 0);
}

static inline void alarm_in(long usec, long interval)
{
    long it[4] = { interval / 1000000, interval % 1000000, usec / 1000000, usec % 1000000 };
    sc(NR_setitimer, 0, (long)it, 0, 0, 0, 0);
}

#endif
