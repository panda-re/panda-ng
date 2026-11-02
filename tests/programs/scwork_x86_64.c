/* tests/syscall_truth.py's threads and signals workloads (x86_64), for the kernel's ftrace to
 * record. Argument 1 is the mode:
 *   threads: 6 threads (clone with CLONE_THREAD and CLONE_CHILD_CLEARTID) each make 100 rounds of
 *            getpid, a FUTEX_WAKE on a shared counter and a 0.5 ms nanosleep; the main thread waits
 *            for each on its tid word (FUTEX_WAIT). Then a side thread calls exit_group(3) while
 *            the main thread sleeps in nanosleep: exit status 3.
 *   signals: a 3 ms SIGALRM interval timer. 100 rounds of a 10 ms nanosleep and a read on an empty
 *            pipe, with a handler without SA_RESTART (both end with -EINTR); then 100 reads with
 *            SA_RESTART and a handler that writes a byte to the pipe (each read restarts and
 *            returns 1). Exit status 0, or 10 + the phase that went wrong.
 * Built by tests/programs/build.sh: freestanding, static, no libc. */
typedef unsigned long u64;

#define SYS_read 0
#define SYS_write 1
#define SYS_rt_sigaction 13
#define SYS_nanosleep 35
#define SYS_setitimer 38
#define SYS_getpid 39
#define SYS_exit_group 231
#define SYS_futex 202
#define SYS_pipe2 293

#define SIGALRM 14
#define SA_RESTORER 0x04000000
#define SA_RESTART 0x10000000
#define EINTR 4
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

static inline long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long r10 __asm__("r10") = d, r8 __asm__("r8") = e, r9 __asm__("r9") = f;
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8),
                     "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

/* thread_spawn(stack_top, ctid, fn, arg): clone(CLONE_VM | FS | FILES | SIGHAND | THREAD | SYSVSEM
 * | CHILD_CLEARTID); the thread runs fn(arg) and exits. Returns the tid. */
long thread_spawn(void *stack, volatile int *ctid, void (*fn)(long), long arg);
__asm__(".text\n.globl thread_spawn\nthread_spawn:\n"
        " push %r12\n push %r13\n mov %rdx, %r12\n mov %rcx, %r13\n"
        " mov %rsi, %r10\n mov %rdi, %rsi\n mov $0x250f00, %edi\n xor %edx, %edx\n xor %r8d, %r8d\n"
        " mov $56, %eax\n syscall\n test %rax, %rax\n jnz 1f\n"
        " mov %r13, %rdi\n call *%r12\n mov $60, %eax\n xor %edi, %edi\n syscall\n hlt\n"
        "1:\n pop %r13\n pop %r12\n ret\n");

void restorer(void);
__asm__(".text\n.globl restorer\nrestorer:\n mov $15, %eax\n syscall\n hlt\n");

struct ksigaction { void *handler; u64 flags; void (*restorer)(void); u64 mask; };

static void handle(int sig, void *h, u64 flags)
{
    struct ksigaction a = { h, flags | SA_RESTORER, restorer, 0 };
    sc(SYS_rt_sigaction, sig, (long)&a, 0, 8, 0, 0);
}

static __attribute__((noreturn)) void die(long c)
{
    for (;;) {
        sc(SYS_exit_group, c, 0, 0, 0, 0, 0);
    }
}

#define NTHREADS 6
static char stacks[NTHREADS + 1][16384] __attribute__((aligned(16)));
static volatile int tids[NTHREADS + 1];
static volatile int counter;

static void worker(long i)
{
    long ts[2] = { 0, 500000 };
    for (int r = 0; r < 100; r++) {
        sc(SYS_getpid, 0, 0, 0, 0, 0, 0);
        __atomic_add_fetch(&counter, 1, __ATOMIC_SEQ_CST);
        sc(SYS_futex, (long)&counter, FUTEX_WAKE, 1, 0, 0, 0);
        sc(SYS_nanosleep, (long)ts, 0, 0, 0, 0, 0);
    }
}

static void side(long i)
{
    long ts[2] = { 0, 10000000 };
    sc(SYS_nanosleep, (long)ts, 0, 0, 0, 0, 0);
    die(3);
}

static void threads(void)
{
    for (int i = 0; i < NTHREADS; i++) {
        tids[i] = 1;
        if (thread_spawn(stacks[i] + sizeof(stacks[i]), &tids[i], worker, i) < 0) {
            die(11);
        }
    }
    for (int i = 0; i < NTHREADS; i++) {
        int t;
        while ((t = tids[i]) != 0) {
            sc(SYS_futex, (long)&tids[i], FUTEX_WAIT, t, 0, 0, 0);
        }
    }
    tids[NTHREADS] = 1;
    thread_spawn(stacks[NTHREADS] + sizeof(stacks[NTHREADS]), &tids[NTHREADS], side, 0);
    for (;;) {
        long ts[2] = { 0, 1000000 };
        sc(SYS_nanosleep, (long)ts, 0, 0, 0, 0, 0);
    }
}

static int pipefd[2];

static void on_alarm(int sig) {}

static void on_alarm_write(int sig) { sc(SYS_write, pipefd[1], (long)"x", 1, 0, 0, 0); }

static void signals(void)
{
    long it[4] = { 0, 3000, 0, 3000 }, off[4] = { 0 }, ts[2] = { 0, 10000000 };
    char c;

    if (sc(SYS_pipe2, (long)pipefd, 0, 0, 0, 0, 0)) {
        die(10);
    }
    handle(SIGALRM, on_alarm, 0);
    sc(SYS_setitimer, 0, (long)it, 0, 0, 0, 0);
    for (int r = 0; r < 100; r++) {
        long s = sc(SYS_nanosleep, (long)ts, 0, 0, 0, 0, 0);
        if (s != 0 && s != -EINTR) {
            die(11);
        }
        if (sc(SYS_read, pipefd[0], (long)&c, 1, 0, 0, 0) != -EINTR) {
            die(12);
        }
    }
    handle(SIGALRM, on_alarm_write, SA_RESTART);
    for (int r = 0; r < 100; r++) {
        if (sc(SYS_read, pipefd[0], (long)&c, 1, 0, 0, 0) != 1) {
            die(13);
        }
    }
    sc(SYS_setitimer, 0, (long)off, 0, 0, 0, 0);
    die(0);
}

void _start_c(long *sp)
{
    char *mode = sp[0] > 1 ? (char *)sp[2] : "";

    if (mode[0] == 't') {
        threads();
    } else if (mode[0] == 's') {
        signals();
    }
    die(2);
}

__asm__(".text\n.globl _start\n_start:\n xor %ebp, %ebp\n mov %rsp, %rdi\n and $-16, %rsp\n"
        " call _start_c\n hlt\n");
