/* The freestanding guest programs' system layer (agent.c, scpaths.c, scwork.c): system calls,
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

#else
#error "tests/programs/sys.h: no system layer for this arch"
#endif

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_TRUNC 01000
#define O_APPEND 02000
#define AT_FDCWD (-100)
#define WNOHANG 1
#define SIGTRAP 5
#define SIGUSR1 10
#define SIGALRM 14
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SA_SIGINFO 4
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

/* the kernel's struct sigaction for rt_sigaction (sigset_t: 8 bytes, of unsigned longs) */
struct ksigaction { void *handler; unsigned long flags; void (*restorer)(void); unsigned long mask[2]; };

void sc_restorer(void);
__asm__(".text\n.globl sc_restorer\nsc_restorer:\n" SIGRETURN_ASM);

static inline void handle(int sig, void *h, unsigned long flags)
{
    /* SA_SIGINFO: an rt frame everywhere (arm sets up a non-rt one without it, which only
     * sigreturn undoes) */
    struct ksigaction a = { h, flags | SA_RESTORER | SA_SIGINFO, sc_restorer, 0 };
    sc(NR_rt_sigaction, sig, (long)&a, 0, 8, 0, 0);
}

static inline void alarm_in(long usec, long interval)
{
    long it[4] = { interval / 1000000, interval % 1000000, usec / 1000000, usec % 1000000 };
    sc(NR_setitimer, 0, (long)it, 0, 0, 0, 0);
}

#endif
