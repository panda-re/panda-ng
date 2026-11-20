/* tests/osi_isf_facts.py's guest program: a process with facts to check, and lifecycle events.
 * Argument 1 is the mode:
 *   hold:    opens /tmp/osifacts.dat at offset 1234, a pipe, a TCP socket listening on
 *            127.0.0.1:5555 and one connected to it, a UDP socket bound to 127.0.0.1:5556 and a
 *            unix stream socket listening on /tmp/osifacts.sock; starts 3 threads; sets its ids
 *            (setresgid 11 12 13, setresuid 21 22 23, which clears its capabilities); writes
 *            "ready <pid>" and sleeps.
 *   cparent: clone(CLONE_PARENT): the child (its parent is ours) sleeps 0.3 s and exits 5; we
 *            write "child <tid>" and exit 0.
 *   threads: two threads; the first exits alone (exit 4), then the second ends the process
 *            (exit_group 3).
 *   vfork:   clone(CLONE_VM | CLONE_VFORK) on a stack of its own; the child execs argument 2 (with
 *            argument 3 as its argument 1); we write "child <tid>" and exit 0.
 * Built by tests/programs/build.sh: freestanding, static, no libc (tests/programs/sys.h). */
#include "sys.h"

/* Calls only this program makes. */
#if defined(__x86_64__)
#define NR_socket 41
#define NR_connect 42
#define NR_bind 49
#define NR_listen 50
#define NR_setresuid 117
#define NR_setresgid 119
#elif defined(__aarch64__)
#define NR_socket 198
#define NR_bind 200
#define NR_listen 201
#define NR_connect 203
#define NR_setresuid 147
#define NR_setresgid 149
#elif defined(__arm__)
#define NR_socket 281
#define NR_bind 282
#define NR_connect 283
#define NR_listen 284
#define NR_setresuid 208    /* setresuid32 */
#define NR_setresgid 210
#elif defined(__mips__) && _MIPS_SIM == _ABI64
#define NR_socket 5040
#define NR_connect 5041
#define NR_bind 5048
#define NR_listen 5049
#define NR_setresuid 5115
#define NR_setresgid 5117
#elif defined(__mips__) && _MIPS_SIM == _ABIO32
#define NR_socket 4183
#define NR_connect 4170
#define NR_bind 4169
#define NR_listen 4174
#define NR_setresuid 4185
#define NR_setresgid 4190
#else
#error "osifacts.c: no socket call numbers for this arch"
#endif
#if defined(__mips__)
#define SOCK_STREAM 2
#define SOCK_DGRAM 1
#else
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#endif
#define AF_UNIX 1
#define AF_INET 2
#define CLONE_PARENT 0x8000

static __attribute__((noreturn)) void die(long c)
{
    for (;;) {
        sc(NR_exit_group, c, 0, 0, 0, 0, 0);
    }
}

static void out(const char *s) { sc(NR_write, 1, (long)s, str_len(s), 0, 0, 0); }

static void out_num(const char *what, long n)
{
    char b[32];
    int i = sizeof(b);
    b[--i] = 0;
    b[--i] = '\n';
    do {
        b[--i] = '0' + n % 10;
        n /= 10;
    } while (n);
    b[--i] = ' ';
    out(what);
    out(b + i);
}

static void nap(long ms)
{
    long ts[2] = { ms / 1000, (ms % 1000) * 1000000 };
    sc(NR_nanosleep, (long)ts, 0, 0, 0, 0, 0);
}

static void sleep_forever(void)
{
    for (;;) {
        nap(1000);
    }
}

struct sin {
    unsigned short family, port;
    unsigned char addr[4], zero[8];
};

static long inet(int type, int port, int doconnect)
{
    /* 127.0.0.1:port, the port in network order */
    struct sin a = { AF_INET, 0, { 127, 0, 0, 1 }, { 0 } };
    unsigned char *p = (unsigned char *)&a.port;
    long s = sc(NR_socket, AF_INET, type, 0, 0, 0, 0);

    p[0] = port >> 8;
    p[1] = port & 0xff;
    if (s < 0) {
        die(20);
    }
    if (doconnect) {
        if (sc(NR_connect, s, (long)&a, sizeof(a), 0, 0, 0)) {
            die(21);
        }
    } else if (sc(NR_bind, s, (long)&a, sizeof(a), 0, 0, 0)) {
        die(22);
    }
    return s;
}

static void hold(void)
{
    static char stacks[3][16384] __attribute__((aligned(16)));
    static volatile int tids[3];
    struct { unsigned short family; char path[108]; } un = { AF_UNIX, "/tmp/osifacts.sock" };
    long f = sys_open("/tmp/osifacts.dat", O_RDWR | O_CREAT, 0644), s;
    int pfd[2];

    if (f < 0 || sc(NR_lseek, f, 1234, 0, 0, 0, 0) != 1234) {
        die(10);
    }
    if (sc(NR_pipe2, (long)pfd, 0, 0, 0, 0, 0)) {
        die(11);
    }
    s = inet(SOCK_STREAM, 5555, 0);
    if (sc(NR_listen, s, 4, 0, 0, 0, 0)) {
        die(12);
    }
    inet(SOCK_STREAM, 5555, 1);
    inet(SOCK_DGRAM, 5556, 0);
    s = sc(NR_socket, AF_UNIX, SOCK_STREAM, 0, 0, 0, 0);
    if (s < 0 || sc(NR_bind, s, (long)&un, 2 + str_len(un.path) + 1, 0, 0, 0) ||
        sc(NR_listen, s, 4, 0, 0, 0, 0)) {
        die(13);
    }
    for (int i = 0; i < 3; i++) {
        tids[i] = 1;
        if (clone_fn(CLONE_THREAD_FLAGS, stacks[i] + sizeof(stacks[i]), &tids[i], sleep_forever) < 0) {
            die(14);
        }
    }
    if (sc(NR_setresgid, 11, 12, 13, 0, 0, 0) || sc(NR_setresuid, 21, 22, 23, 0, 0, 0)) {
        die(15);
    }
    out_num("ready", sc(NR_getpid, 0, 0, 0, 0, 0, 0));
    sleep_forever();
}

static void child_exit5(void)
{
    nap(300);
    die(5);
}

static void cparent(void)
{
    static char stack[16384] __attribute__((aligned(16)));
    long t = clone_fn(CLONE_PARENT | SIGCHLD, stack + sizeof(stack), 0, child_exit5);

    if (t < 0) {
        die(30);
    }
    out_num("child", t);
    die(0);
}

static void thread_exit4(void)
{
    sc(NR_exit, 4, 0, 0, 0, 0, 0);
}

static void thread_group3(void)
{
    nap(100);
    die(3);
}

static void threads(void)
{
    static char stacks[2][16384] __attribute__((aligned(16)));
    static volatile int tids[2];

    tids[0] = tids[1] = 1;
    if (clone_fn(CLONE_THREAD_FLAGS, stacks[0] + sizeof(stacks[0]), &tids[0], thread_exit4) < 0) {
        die(40);
    }
    nap(50);
    if (clone_fn(CLONE_THREAD_FLAGS, stacks[1] + sizeof(stacks[1]), &tids[1], thread_group3) < 0) {
        die(41);
    }
    sleep_forever();
}

static char *exec_argv[3];

static void exec_child(void)
{
    static char *envp[] = { "OSIFACTS=vfork", 0 };

    sc(NR_execve, (long)exec_argv[0], (long)exec_argv, (long)envp, 0, 0, 0);
    sc(NR_exit, 127, 0, 0, 0, 0, 0);
}

static void vfork_exec(char *prog, char *arg)
{
    static char stack[16384] __attribute__((aligned(16)));
    long t;

    exec_argv[0] = prog;
    exec_argv[1] = arg;
    t = clone_fn(CLONE_VM | CLONE_VFORK | SIGCHLD, stack + sizeof(stack), 0, exec_child);
    if (t < 0) {
        die(50);
    }
    out_num("child", t);
    die(0);
}

void _start_c(long *sp)
{
    char *mode = sp[0] > 1 ? (char *)sp[2] : "";

    if (mode[0] == 'h') {
        hold();
    } else if (mode[0] == 'c') {
        cparent();
    } else if (mode[0] == 't') {
        threads();
    } else if (mode[0] == 'v' && sp[0] > 3) {
        vfork_exec((char *)sp[3], (char *)sp[4]);
    }
    die(2);
}
