/* tests/syscall_truth.py's threads and signals workloads, for the kernel's ftrace to record.
 * Argument 1 is the mode:
 *   threads: 6 threads (clone with CLONE_THREAD and CLONE_CHILD_CLEARTID) each make 100 rounds of
 *            getpid, a FUTEX_WAKE on a shared counter and a 0.5 ms nanosleep; the main thread waits
 *            for each on its tid word (FUTEX_WAIT). Then a side thread calls exit_group(3) while
 *            the main thread sleeps in nanosleep: exit status 3.
 *   signals: a 3 ms SIGALRM interval timer. 100 rounds of a 10 ms nanosleep and a read on an empty
 *            pipe, with a handler without SA_RESTART (both end with -EINTR); then 100 reads with
 *            SA_RESTART and a handler that writes a byte to the pipe (each read restarts and
 *            returns 1). Exit status 0, or 10 + the phase that went wrong.
 * Built by tests/programs/build.sh: freestanding, static, no libc (tests/programs/sys.h). */
#include "sys.h"

static __attribute__((noreturn)) void die(long c)
{
    for (;;) {
        sc(NR_exit_group, c, 0, 0, 0, 0, 0);
    }
}

#define NTHREADS 6
static char stacks[NTHREADS + 1][16384] __attribute__((aligned(16)));
static volatile int tids[NTHREADS + 1];
static volatile int counter;

static void worker(void)
{
    long ts[2] = { 0, 500000 };
    for (int r = 0; r < 100; r++) {
        sc(NR_getpid, 0, 0, 0, 0, 0, 0);
        __atomic_add_fetch(&counter, 1, __ATOMIC_SEQ_CST);
        sc(NR_futex, (long)&counter, FUTEX_WAKE, 1, 0, 0, 0);
        sc(NR_nanosleep, (long)ts, 0, 0, 0, 0, 0);
    }
}

static void side(void)
{
    long ts[2] = { 0, 10000000 };
    sc(NR_nanosleep, (long)ts, 0, 0, 0, 0, 0);
    die(3);
}

static void threads(void)
{
    for (int i = 0; i < NTHREADS; i++) {
        tids[i] = 1;
        if (clone_fn(CLONE_THREAD_FLAGS, stacks[i] + sizeof(stacks[i]), &tids[i], worker) < 0) {
            die(11);
        }
    }
    for (int i = 0; i < NTHREADS; i++) {
        int t;
        while ((t = tids[i]) != 0) {
            sc(NR_futex, (long)&tids[i], FUTEX_WAIT, t, 0, 0, 0);
        }
    }
    tids[NTHREADS] = 1;
    clone_fn(CLONE_THREAD_FLAGS, stacks[NTHREADS] + sizeof(stacks[NTHREADS]), &tids[NTHREADS], side);
    for (;;) {
        long ts[2] = { 0, 1000000 };
        sc(NR_nanosleep, (long)ts, 0, 0, 0, 0, 0);
    }
}

static int pipefd[2];

static void on_alarm(int sig) {}

static void on_alarm_write(int sig) { sc(NR_write, pipefd[1], (long)"x", 1, 0, 0, 0); }

static void signals(void)
{
    long ts[2] = { 0, 10000000 };
    char c;

    if (sc(NR_pipe2, (long)pipefd, 0, 0, 0, 0, 0)) {
        die(10);
    }
    handle(SIGALRM, on_alarm, 0);
    alarm_in(3000, 3000);
    for (int r = 0; r < 100; r++) {
        long s = sc(NR_nanosleep, (long)ts, 0, 0, 0, 0, 0);
        if (s != 0 && s != -EINTR) {
            die(11);
        }
        if (sc(NR_read, pipefd[0], (long)&c, 1, 0, 0, 0) != -EINTR) {
            die(12);
        }
    }
    handle(SIGALRM, on_alarm_write, SA_RESTART);
    for (int r = 0; r < 100; r++) {
        if (sc(NR_read, pipefd[0], (long)&c, 1, 0, 0, 0) != 1) {
            die(13);
        }
    }
    alarm_in(0, 0);
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
