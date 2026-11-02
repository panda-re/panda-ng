/* tests/syscall_cover.py's guest program for the return paths (x86_64): calls whose return is not
 * the plain sysret, each case announced by a marker call write(-1, 0, case) (-EBADF), in order:
 *   1 iret:      getpid with TF set (the kernel returns through iret); the SIGTRAP after it
 *                clears TF (SA_SIGINFO handler, rt_sigreturn)
 *   2 restart:   read on an empty pipe, interrupted by SIGALRM with SA_RESTART: the handler writes
 *                a byte to the pipe, the kernel restarts the read, which returns 1
 *   3 eintr:     the same without SA_RESTART: the read returns -EINTR
 *   4 restart_syscall: nanosleep(300 ms); a forked child stops and continues the program 50 ms
 *                in: no handler runs, so the kernel continues the sleep with restart_syscall
 *                (219), which returns 0; wait4 for the child
 *   5 fork:      fork; the child exits (exit_group 5); wait4 returns its pid
 *   6 vfork:     vfork; the child exits (exit_group 6) on the parent's stack, in its address space
 *   7 thread:    clone(CLONE_VM | CLONE_THREAD | ...) on a stack of its own; the thread exits
 *                (exit 7); the parent waits on CLONE_CHILD_CLEARTID's futex
 *   8 clone3:    clone3 as fork; the child exits (exit_group 8); wait4
 *   9 execve -ENOENT: execve("/nonexistent-panda-scpaths")
 *  10 execve:    execve("/bin/true"): no return
 * Exit status (only if execve("/bin/true") fails): 100 + case.
 * Built by tests/programs/build.sh: freestanding, static, no libc. */
typedef unsigned long u64;

#define SYS_read 0
#define SYS_write 1
#define SYS_rt_sigaction 13
#define SYS_rt_sigreturn 15
#define SYS_getpid 39
#define SYS_nanosleep 35
#define SYS_setitimer 38
#define SYS_clone 56
#define SYS_fork 57
#define SYS_vfork 58
#define SYS_execve 59
#define SYS_exit 60
#define SYS_wait4 61
#define SYS_kill 62
#define SYS_futex 202
#define SYS_exit_group 231
#define SYS_pipe2 293
#define SYS_clone3 435

#define SIGTRAP 5
#define SIGALRM 14
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SA_SIGINFO 4
#define SA_RESTORER 0x04000000
#define SA_RESTART 0x10000000
#define EINTR 4

static inline long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long r10 __asm__("r10") = d, r8 __asm__("r8") = e, r9 __asm__("r9") = f;
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8),
                     "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

static void marker(long c) { sc(SYS_write, -1, 0, c, 0, 0, 0); }

static __attribute__((noreturn)) void die(long c)
{
    for (;;) {
        sc(SYS_exit_group, 100 + c, 0, 0, 0, 0, 0);
    }
}

void restorer(void);
__asm__(".text\n.globl restorer\nrestorer:\n mov $15, %eax\n syscall\n hlt\n");

struct ksigaction { void *handler; u64 flags; void (*restorer)(void); u64 mask; };

static void handle(int sig, void *h, u64 flags)
{
    struct ksigaction a = { h, flags | SA_RESTORER, restorer, 0 };
    sc(SYS_rt_sigaction, sig, (long)&a, 0, 8, 0, 0);
}

static int pipefd[2];

static void on_trap(int sig, void *info, void *uc)
{
    ((u64 *)uc)[(40 + 17 * 8) / 8] &= ~0x100ul;     /* uc_mcontext.gregs[REG_EFL]: TF off */
}

static void on_alarm_write(int sig) { sc(SYS_write, pipefd[1], (long)"x", 1, 0, 0, 0); }

static void on_alarm(int sig) {}

static void alarm_in(long usec)
{
    long it[4] = { 0, 0, usec / 1000000, usec % 1000000 };
    sc(SYS_setitimer, 0, (long)it, 0, 0, 0, 0);
}

static volatile int tid_word;
static char thread_stack[16384] __attribute__((aligned(16)));

void _start_c(void)
{
    char buf[8];
    long r;

    marker(1);                                      /* iret */
    handle(SIGTRAP, on_trap, SA_SIGINFO);
    __asm__ volatile("pushfq; orq $0x100, (%%rsp); popfq; syscall"
                     : "=a"(r) : "a"(SYS_getpid) : "rcx", "r11", "memory");
    if (r <= 0) die(1);

    marker(2);                                      /* restart */
    if (sc(SYS_pipe2, (long)pipefd, 0, 0, 0, 0, 0)) die(2);
    handle(SIGALRM, on_alarm_write, SA_RESTART);
    alarm_in(50000);
    if (sc(SYS_read, pipefd[0], (long)buf, 1, 0, 0, 0) != 1) die(2);

    marker(3);                                      /* eintr */
    handle(SIGALRM, on_alarm, 0);
    alarm_in(50000);
    if (sc(SYS_read, pipefd[0], (long)buf, 1, 0, 0, 0) != -EINTR) die(3);

    marker(4);                                      /* restart_syscall */
    {
        long ts[2] = { 0, 300000000 }, child_ts[2] = { 0, 50000000 };
        long parent = sc(SYS_getpid, 0, 0, 0, 0, 0, 0);
        long child = sc(SYS_fork, 0, 0, 0, 0, 0, 0);
        if (child == 0) {
            sc(SYS_nanosleep, (long)child_ts, 0, 0, 0, 0, 0);
            sc(SYS_kill, parent, SIGSTOP, 0, 0, 0, 0);
            sc(SYS_kill, parent, SIGCONT, 0, 0, 0, 0);
            for (;;) sc(SYS_exit_group, 4, 0, 0, 0, 0, 0);
        }
        if (child < 0 || sc(SYS_nanosleep, (long)ts, 0, 0, 0, 0, 0) != 0) die(4);
        if (sc(SYS_wait4, child, 0, 0, 0, 0, 0) != child) die(4);
    }

    marker(5);                                      /* fork */
    r = sc(SYS_fork, 0, 0, 0, 0, 0, 0);
    if (r == 0) {
        for (;;) sc(SYS_exit_group, 5, 0, 0, 0, 0, 0);
    }
    if (r < 0 || sc(SYS_wait4, r, 0, 0, 0, 0, 0) != r) die(5);

    marker(6);                                      /* vfork: the child must not return here */
    __asm__ volatile("syscall; test %%rax, %%rax; jnz 1f; mov $231, %%eax; mov $6, %%edi; syscall; 1:"
                     : "=a"(r) : "a"(SYS_vfork) : "rcx", "r11", "rdi", "memory");
    if (r < 0 || sc(SYS_wait4, r, 0, 0, 0, 0, 0) != r) die(6);

    marker(7);                                      /* thread */
    tid_word = 1;
    {
        register long r10 __asm__("r10") = (long)&tid_word;
        register long r8 __asm__("r8") = 0;
        /* CLONE_VM | FS | FILES | SIGHAND | THREAD | SYSVSEM | CHILD_CLEARTID */
        __asm__ volatile("syscall; test %%rax, %%rax; jnz 1f; mov $60, %%eax; mov $7, %%edi; syscall; 1:"
                         : "=a"(r)
                         : "a"(SYS_clone), "D"(0x00200000 | 0x00040000 | 0x00010000 | 0x800 | 0x400 |
                                               0x200 | 0x100),
                           "S"(thread_stack + sizeof(thread_stack)), "d"(0), "r"(r10), "r"(r8)
                         : "rcx", "r11", "memory");
    }
    if (r < 0) die(7);
    while (tid_word) {
        sc(SYS_futex, (long)&tid_word, 0, tid_word, 0, 0, 0);     /* FUTEX_WAIT */
    }

    marker(8);                                      /* clone3 as fork */
    {
        u64 ca[11] = { 0, 0, 0, 0, SIGCHLD };       /* flags, pidfd, child_tid, parent_tid, exit_signal */
        r = sc(SYS_clone3, (long)ca, sizeof(ca), 0, 0, 0, 0);
        if (r == 0) {
            for (;;) sc(SYS_exit_group, 8, 0, 0, 0, 0, 0);
        }
        if (r < 0 || sc(SYS_wait4, r, 0, 0, 0, 0, 0) != r) die(8);
    }

    marker(9);                                      /* execve -ENOENT */
    {
        char *argv[] = { "true", 0 };
        if (sc(SYS_execve, (long)"/nonexistent-panda-scpaths", (long)argv, 0, 0, 0, 0) != -2) die(9);
        marker(10);                                 /* execve */
        sc(SYS_execve, (long)"/bin/true", (long)argv, 0, 0, 0, 0);
    }
    die(10);
}

__asm__(".text\n.globl _start\n_start:\n xor %ebp, %ebp\n and $-16, %rsp\n call _start_c\n hlt\n");
