/* The test machines' guest agent (tests/testmachine.py): the host puts files in, runs commands and
 * gets files back through hypercalls and guest memory, not the serial console.
 *
 * As PID 1 (the tiny machines' /init) it mounts /dev, /proc, /sys, tracefs and /tmp and installs
 * busybox's applets in /bin; started as root on a full distribution it does neither. Then it
 * makes hypercalls (MAGIC_AGENT, op, a, b, c), `out 0x88` with the arguments in the system call
 * registers and the host's answer in rax:
 *   HELLO(buf, size, pid)    once, when ready
 *   POLL(buf, size)          the host writes a request to buf and answers its length, or 0 (the
 *                            agent sleeps 1 ms and asks again, reaping orphans)
 *   REPLY(buf, len, status)  the request's result: len bytes in buf, and a status
 * A request is struct req, then the path (or command), then the data. Ops:
 *   PUT:  write data to path with mode (append if flags & 1, else truncate); status 0 or -errno
 *   RUN:  /bin/sh -c command, stdin /dev/null, stdout and stderr to OUT; status the wait status;
 *         the reply carries the first size bytes of OUT (GET reads the rest)
 *   GET:  up to size bytes of path from offset; status the count or -errno
 * Built by tests/programs/build.sh: freestanding, static, no libc. */
typedef unsigned long u64;
typedef unsigned int u32;

#define MAGIC_AGENT 0x50416774
enum { HC_HELLO = 1, HC_POLL = 2, HC_REPLY = 3 };
enum { OP_PUT = 1, OP_RUN = 2, OP_GET = 3 };
#define BUF (4ul << 20)
#define OUT "/tmp/.agent-out"

struct req { u32 op, mode, flags, pathlen; u64 datalen, offset; };

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_pread64 17
#define SYS_ioperm 173
#define SYS_nanosleep 35
#define SYS_getpid 39
#define SYS_dup2 33
#define SYS_fork 57
#define SYS_execve 59
#define SYS_exit 60
#define SYS_wait4 61
#define SYS_mkdir 83
#define SYS_setsid 112
#define SYS_mount 165

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_TRUNC 01000
#define O_APPEND 02000
#define WNOHANG 1

static char buf[BUF] __attribute__((aligned(4096)));
static int last_status;

static inline long sc(long n, long a, long b, long c, long d, long e, long f)
{
    register long r10 __asm__("r10") = d, r8 __asm__("r8") = e, r9 __asm__("r9") = f;
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8),
                     "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

static inline long hc(long op, long a, long b, long c)
{
    register long r10 __asm__("r10") = b, r8 __asm__("r8") = c;
    long ret = MAGIC_AGENT;
    __asm__ volatile("out %%eax, $0x88" : "+a"(ret) : "D"(MAGIC_AGENT), "S"(op), "d"(a), "r"(r10),
                     "r"(r8) : "memory");
    return ret;
}

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

static void run_wait(const char *const *argv, const char *out)
{
    static const char *const envp[] = { "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "HOME=/", "TERM=dumb",
                                        0 };
    long pid = sc(SYS_fork, 0, 0, 0, 0, 0, 0);
    int status = 0;

    if (pid == 0) {
        sc(SYS_setsid, 0, 0, 0, 0, 0, 0);
        long in = sc(SYS_open, (long)"/dev/null", O_RDONLY, 0, 0, 0, 0);
        long fd = out ? sc(SYS_open, (long)out, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0)
                      : sc(SYS_open, (long)"/dev/null", O_WRONLY, 0, 0, 0, 0);
        sc(SYS_dup2, in, 0, 0, 0, 0, 0);
        sc(SYS_dup2, fd, 1, 0, 0, 0, 0);
        sc(SYS_dup2, fd, 2, 0, 0, 0, 0);
        sc(SYS_execve, (long)argv[0], (long)argv, (long)envp, 0, 0, 0);
        for (;;) {
            sc(SYS_exit, 127, 0, 0, 0, 0, 0);
        }
    }
    /* As PID 1 this also reaps orphans; the command's own status is the one kept. */
    for (;;) {
        long r = sc(SYS_wait4, -1, (long)&status, 0, 0, 0, 0);
        if (r == pid || r < 0) {
            break;
        }
    }
    last_status = status;
}

static long put(struct req *r, char *path, char *data)
{
    long fd = sc(SYS_open, (long)path, O_WRONLY | O_CREAT | (r->flags & 1 ? O_APPEND : O_TRUNC),
                 r->mode, 0, 0, 0);
    if (fd < 0) {
        return fd;
    }
    for (u64 done = 0; done < r->datalen;) {
        long n = sc(SYS_write, fd, (long)(data + done), r->datalen - done, 0, 0, 0);
        if (n <= 0) {
            sc(SYS_close, fd, 0, 0, 0, 0, 0);
            return n < 0 ? n : -5;
        }
        done += n;
    }
    sc(SYS_close, fd, 0, 0, 0, 0, 0);
    return 0;
}

static long get(const char *path, u64 offset, u64 *len)
{
    long fd = sc(SYS_open, (long)path, O_RDONLY, 0, 0, 0, 0);
    *len = 0;
    if (fd < 0) {
        return fd;
    }
    while (*len < BUF) {
        long n = sc(SYS_pread64, fd, (long)(buf + *len), BUF - *len, offset + *len, 0, 0);
        if (n <= 0) {
            break;
        }
        *len += n;
    }
    sc(SYS_close, fd, 0, 0, 0, 0, 0);
    return *len;
}

void _start_c(void)
{
    static char path[4096];
    struct req r;
    long ts[2] = { 0, 1000000 };

    if (sc(SYS_getpid, 0, 0, 0, 0, 0, 0) == 1) {
        sc(SYS_mount, (long)"dev", (long)"/dev", (long)"devtmpfs", 0, 0, 0);
        sc(SYS_mount, (long)"proc", (long)"/proc", (long)"proc", 0, 0, 0);
        sc(SYS_mount, (long)"sys", (long)"/sys", (long)"sysfs", 0, 0, 0);
        sc(SYS_mount, (long)"tracefs", (long)"/sys/kernel/tracing", (long)"tracefs", 0, 0, 0);
        sc(SYS_mount, (long)"tmp", (long)"/tmp", (long)"tmpfs", 0, 0, 0);
        static const char *const install[] = { "/bin/busybox", "--install", "-s", "/bin", 0 };
        run_wait(install, 0);
    }
    sc(SYS_ioperm, 0x88, 4, 1, 0, 0, 0);
    for (u64 i = 0; i < BUF; i += 4096) {       /* every page present and writable for the host */
        ((volatile char *)buf)[i] = 0;
    }
    sc(SYS_write, 2, (long)"panda agent: ready\n", 19, 0, 0, 0);
    hc(HC_HELLO, (long)buf, BUF, sc(SYS_getpid, 0, 0, 0, 0, 0, 0));
    for (;;) {
        long n = hc(HC_POLL, (long)buf, BUF, 0);
        long status;
        u64 len = 0;

        if (n <= 0) {
            int st;
            while (sc(SYS_wait4, -1, (long)&st, WNOHANG, 0, 0, 0) > 0) {
            }
            sc(SYS_nanosleep, (long)ts, 0, 0, 0, 0, 0);
            continue;
        }
        memcpy(&r, buf, sizeof(r));
        if (r.pathlen >= sizeof(path)) {
            hc(HC_REPLY, (long)buf, 0, -36);    /* ENAMETOOLONG */
            continue;
        }
        memcpy(path, buf + sizeof(r), r.pathlen);
        path[r.pathlen] = 0;
        switch (r.op) {
        case OP_PUT:
            status = put(&r, path, buf + sizeof(r) + r.pathlen);
            break;
        case OP_RUN: {
            const char *const argv[] = { "/bin/sh", "-c", path, 0 };
            run_wait(argv, OUT);
            status = last_status;
            get(OUT, 0, &len);
            break;
        }
        case OP_GET:
            status = get(path, r.offset, &len);
            break;
        default:
            status = -22;
        }
        hc(HC_REPLY, (long)buf, len, status);
    }
}

__asm__(".text\n.globl _start\n_start:\n xor %ebp, %ebp\n and $-16, %rsp\n call _start_c\n hlt\n");
