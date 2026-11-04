/* tests/syscall_cover.py's guest program for the return paths: calls whose return is not the plain
 * return to the next instruction of the same task. Each case is announced by a marker call
 * write(-1, 0, case) (-EBADF), in order:
 *   1 iret:      (x86_64 only; elsewhere the marker alone) getpid with TF set (the kernel returns
 *                through iret); the SIGTRAP after it clears TF (SA_SIGINFO handler, rt_sigreturn)
 *   2 restart:   read on an empty pipe, interrupted by SIGALRM with SA_RESTART: the handler writes
 *                a byte to the pipe, the kernel restarts the read, which returns 1
 *   3 eintr:     the same without SA_RESTART: the read returns -EINTR
 *   4 restart_syscall: nanosleep(300 ms); a forked child stops and continues the program 50 ms
 *                in: no handler runs, so the kernel continues the sleep with restart_syscall,
 *                which returns 0; wait4 for the child
 *   5 fork:      fork; the child exits (exit_group 5); wait4 returns its pid
 *   6 vfork:     vfork (aarch64: clone(CLONE_VM | CLONE_VFORK)); the child returns from the same
 *                call at the same pc on the parent's stack, in its address space, and exits
 *                (exit_group 6) without touching memory
 *   7 thread:    clone(CLONE_VM | CLONE_THREAD | ...) on a stack of its own; the thread exits
 *                (exit 7); the parent waits on CLONE_CHILD_CLEARTID's futex
 *   8 clone3:    clone3 as fork; the child exits (exit_group 8); wait4
 *   9 clone3 vfork: clone3(CLONE_VM | CLONE_VFORK) with no stack: as 6, the child returns from
 *                the parent's call on the parent's stack (exit_group 9)
 *  10 same-site read: a thread (CLONE_VM, own stack) and the main thread block in read at one
 *                instruction (site_read), the thread on pipe A, the main thread on pipe B; a
 *                forked writer writes 2 bytes to B, then 1 byte to A: the main thread's read
 *                returns 2, then the thread's returns 1
 *  11 execve -ENOENT: execve("/nonexistent-panda-scpaths")
 *  12 execve:    execve("/bin/true"): no return
 * Exit status (only if execve("/bin/true") fails): 100 + case.
 * Built by tests/programs/build.sh: freestanding, static, no libc (tests/programs/sys.h). */
#include "sys.h"

static void marker(long c) { sc(NR_write, -1, 0, c, 0, 0, 0); }

static __attribute__((noreturn)) void die(long c)
{
    for (;;) {
        sc(NR_exit_group, 100 + c, 0, 0, 0, 0, 0);
    }
}

static int pipefd[2];

#if defined(__x86_64__)
static void on_trap(int sig, void *info, void *uc)
{
    ((u64 *)uc)[(40 + 17 * 8) / 8] &= ~0x100ul;     /* uc_mcontext.gregs[REG_EFL]: TF off */
}

static long getpid_tf(void)
{
    long r;
    __asm__ volatile("pushfq; orq $0x100, (%%rsp); popfq; syscall"
                     : "=a"(r) : "a"(NR_getpid) : "rcx", "r11", "memory");
    return r;
}

/* vfork; the child exit_group(6)s on the spot */
static long vfork_exit(void)
{
    long r;
    __asm__ volatile("syscall; test %%rax, %%rax; jnz 1f; mov $231, %%eax; mov $6, %%edi; syscall; 1:"
                     : "=a"(r) : "a"(NR_vfork) : "rcx", "r11", "rdi", "memory");
    return r;
}

/* clone3(args, size); the child exit_group(9)s on the spot */
static long clone3_exit(void *args, long size)
{
    long r;
    __asm__ volatile("syscall; test %%rax, %%rax; jnz 1f; mov $231, %%eax; mov $9, %%edi; syscall; 1:"
                     : "=a"(r) : "a"(NR_clone3), "D"(args), "S"(size) : "rcx", "r11", "memory");
    return r;
}
#elif defined(__aarch64__)
static long vfork_exit(void)
{
    register long x0 __asm__("x0") = CLONE_VM | CLONE_VFORK | SIGCHLD;
    __asm__ volatile("mov x8, #220; mov x1, #0; mov x2, #0; mov x3, #0; mov x4, #0; svc #0;"
                     "cbnz x0, 1f; mov x0, #6; mov x8, #94; svc #0; 1:"
                     : "+r"(x0) : : "x1", "x2", "x3", "x4", "x8", "memory");
    return x0;
}

static long clone3_exit(void *args, long size)
{
    register long x0 __asm__("x0") = (long)args;
    register long x1 __asm__("x1") = size;
    __asm__ volatile("mov x8, #435; svc #0; cbnz x0, 1f; mov x0, #9; mov x8, #94; svc #0; 1:"
                     : "+r"(x0) : "r"(x1) : "x8", "memory");
    return x0;
}
#endif

static void on_alarm_write(int sig) { sc(NR_write, pipefd[1], (long)"x", 1, 0, 0, 0); }

static void on_alarm(int sig) {}

/* The one read instruction both of case 10's threads block in. */
__attribute__((noinline)) static long site_read(long fd, char *buf, long n)
{
    return sc(NR_read, fd, (long)buf, n, 0, 0, 0);
}

static volatile int tid_word;
static char thread_stack[16384] __attribute__((aligned(16)));
static int pipe_a[2], pipe_b[2];
static volatile long thread_ret;
static char thread_buf[8];

static void thread_exit7(void)
{
    sc(NR_exit, 7, 0, 0, 0, 0, 0);
}

static void thread_read(void)
{
    thread_ret = site_read(pipe_a[0], thread_buf, sizeof(thread_buf));
}

static void wait_thread(void)
{
    while (tid_word) {
        sc(NR_futex, (long)&tid_word, FUTEX_WAIT, tid_word, 0, 0, 0);
    }
}

void _start_c(void *sp)
{
    char buf[8];
    long r;

    marker(1);                                      /* iret */
#if defined(__x86_64__)
    handle(SIGTRAP, on_trap, SA_SIGINFO);
    if (getpid_tf() <= 0) die(1);
#endif

    marker(2);                                      /* restart */
    if (sc(NR_pipe2, (long)pipefd, 0, 0, 0, 0, 0)) die(2);
    handle(SIGALRM, on_alarm_write, SA_RESTART);
    alarm_in(50000, 0);
    if (sc(NR_read, pipefd[0], (long)buf, 1, 0, 0, 0) != 1) die(2);

    marker(3);                                      /* eintr */
    handle(SIGALRM, on_alarm, 0);
    alarm_in(50000, 0);
    if (sc(NR_read, pipefd[0], (long)buf, 1, 0, 0, 0) != -EINTR) die(3);

    marker(4);                                      /* restart_syscall */
    {
        long ts[2] = { 0, 300000000 }, child_ts[2] = { 0, 50000000 };
        long parent = sc(NR_getpid, 0, 0, 0, 0, 0, 0);
        long child = sys_fork();
        if (child == 0) {
            sc(NR_nanosleep, (long)child_ts, 0, 0, 0, 0, 0);
            sc(NR_kill, parent, SIGSTOP, 0, 0, 0, 0);
            sc(NR_kill, parent, SIGCONT, 0, 0, 0, 0);
            for (;;) sc(NR_exit_group, 4, 0, 0, 0, 0, 0);
        }
        if (child < 0 || sc(NR_nanosleep, (long)ts, 0, 0, 0, 0, 0) != 0) die(4);
        if (sc(NR_wait4, child, 0, 0, 0, 0, 0) != child) die(4);
    }

    marker(5);                                      /* fork */
    r = sys_fork();
    if (r == 0) {
        for (;;) sc(NR_exit_group, 5, 0, 0, 0, 0, 0);
    }
    if (r < 0 || sc(NR_wait4, r, 0, 0, 0, 0, 0) != r) die(5);

    marker(6);                                      /* vfork: the child must not return here */
    r = vfork_exit();
    if (r < 0 || sc(NR_wait4, r, 0, 0, 0, 0, 0) != r) die(6);

    marker(7);                                      /* thread */
    tid_word = 1;
    r = clone_fn(CLONE_THREAD_FLAGS, thread_stack + sizeof(thread_stack), &tid_word, thread_exit7);
    if (r < 0) die(7);
    wait_thread();

    marker(8);                                      /* clone3 as fork */
    {
        u64 ca[11] = { 0, 0, 0, 0, SIGCHLD };       /* flags, pidfd, child_tid, parent_tid, exit_signal */
        r = sc(NR_clone3, (long)ca, sizeof(ca), 0, 0, 0, 0);
        if (r == 0) {
            for (;;) sc(NR_exit_group, 8, 0, 0, 0, 0, 0);
        }
        if (r < 0 || sc(NR_wait4, r, 0, 0, 0, 0, 0) != r) die(8);
    }

    marker(9);                                      /* clone3 vfork, on the parent's stack */
    {
        static u64 ca[11] = { CLONE_VM | CLONE_VFORK, 0, 0, 0, SIGCHLD };
        r = clone3_exit(ca, sizeof(ca));
        if (r < 0 || sc(NR_wait4, r, 0, 0, 0, 0, 0) != r) die(9);
    }

    marker(10);                                     /* same-site read in two threads */
    {
        long ts[2] = { 0, 100000000 }, writer;
        if (sc(NR_pipe2, (long)pipe_a, 0, 0, 0, 0, 0) || sc(NR_pipe2, (long)pipe_b, 0, 0, 0, 0, 0))
            die(10);
        tid_word = 1;
        thread_ret = 0;
        if (clone_fn(CLONE_THREAD_FLAGS, thread_stack + sizeof(thread_stack), &tid_word,
                     thread_read) < 0) die(10);
        writer = sys_fork();
        if (writer == 0) {
            sc(NR_nanosleep, (long)ts, 0, 0, 0, 0, 0);
            sc(NR_write, pipe_b[1], (long)"xx", 2, 0, 0, 0);
            sc(NR_nanosleep, (long)ts, 0, 0, 0, 0, 0);
            sc(NR_write, pipe_a[1], (long)"y", 1, 0, 0, 0);
            for (;;) sc(NR_exit_group, 10, 0, 0, 0, 0, 0);
        }
        if (writer < 0 || site_read(pipe_b[0], buf, sizeof(buf)) != 2) die(10);
        wait_thread();
        if (thread_ret != 1 || sc(NR_wait4, writer, 0, 0, 0, 0, 0) != writer) die(10);
    }

    marker(11);                                     /* execve -ENOENT */
    {
        char *argv[] = { "true", 0 };
        if (sc(NR_execve, (long)"/nonexistent-panda-scpaths", (long)argv, 0, 0, 0, 0) != -2) die(11);
        marker(12);                                 /* execve */
        sc(NR_execve, (long)"/bin/true", (long)argv, 0, 0, 0, 0);
    }
    die(12);
}
