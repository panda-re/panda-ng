/* The test machines' guest agent (tests/testmachine.py): the host puts files in, runs commands and
 * gets files back through hypercalls and guest memory, not the serial console.
 *
 * As PID 1 (the machine's /init) it mounts /dev, /proc, /sys, tracefs and /tmp and installs
 * busybox's applets in /bin; started by hand it does neither. Then it makes hypercalls
 * (MAGIC_AGENT, op, a, b, c) (tests/programs/sys.h, per arch) with the host's answer in the
 * return register:
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
#include "sys.h"

enum { HC_HELLO = 1, HC_POLL = 2, HC_REPLY = 3 };
enum { OP_PUT = 1, OP_RUN = 2, OP_GET = 3 };
#define BUF (4ul << 20)
#define OUT "/tmp/.agent-out"

struct req { u32 op, mode, flags, pathlen; u64 datalen, offset; };

static char buf[BUF] __attribute__((aligned(4096)));
static int last_status;

static void run_wait(const char *const *argv, const char *out)
{
    static const char *const envp[] = { "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "HOME=/", "TERM=dumb",
                                        0 };
    long pid = sys_fork();
    int status = 0;

    if (pid == 0) {
        sc(NR_setsid, 0, 0, 0, 0, 0, 0);
        long in = sys_open("/dev/null", O_RDONLY, 0);
        long fd = out ? sys_open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644)
                      : sys_open("/dev/null", O_WRONLY, 0);
        sc(NR_dup3, in, 0, 0, 0, 0, 0);
        sc(NR_dup3, fd, 1, 0, 0, 0, 0);
        sc(NR_dup3, fd, 2, 0, 0, 0, 0);
        sc(NR_execve, (long)argv[0], (long)argv, (long)envp, 0, 0, 0);
        for (;;) {
            sc(NR_exit, 127, 0, 0, 0, 0, 0);
        }
    }
    /* As PID 1 this also reaps orphans; the command's own status is the one kept. */
    for (;;) {
        long r = sc(NR_wait4, -1, (long)&status, 0, 0, 0, 0);
        if (r == pid || r < 0) {
            break;
        }
    }
    last_status = status;
}

static long put(struct req *r, char *path, char *data)
{
    long fd = sys_open(path, O_WRONLY | O_CREAT | (r->flags & 1 ? O_APPEND : O_TRUNC), r->mode);
    if (fd < 0) {
        return fd;
    }
    for (u64 done = 0; done < r->datalen;) {
        long n = sc(NR_write, fd, (long)(data + done), r->datalen - done, 0, 0, 0);
        if (n <= 0) {
            sc(NR_close, fd, 0, 0, 0, 0, 0);
            return n < 0 ? n : -5;
        }
        done += n;
    }
    sc(NR_close, fd, 0, 0, 0, 0, 0);
    return 0;
}

static long get(const char *path, u64 offset, u64 *len)
{
    long fd = sys_open(path, O_RDONLY, 0);
    *len = 0;
    if (fd < 0) {
        return fd;
    }
    if (offset && sc(NR_lseek, fd, offset, 0, 0, 0, 0) < 0) {
        /* unseekable (a pipe or tracefs file): read and drop up to offset */
        for (u64 skip = offset; skip;) {
            long n = sc(NR_read, fd, (long)buf, skip < BUF ? skip : BUF, 0, 0, 0);
            if (n <= 0) {
                break;
            }
            skip -= n;
        }
    }
    while (*len < BUF) {
        long n = sc(NR_read, fd, (long)(buf + *len), BUF - *len, 0, 0, 0);
        if (n <= 0) {
            break;
        }
        *len += n;
    }
    sc(NR_close, fd, 0, 0, 0, 0, 0);
    return *len;
}

void _start_c(void *sp)
{
    static char path[4096];
    struct req r;
    long ts[2] = { 0, 1000000 };

    (void)sp;
    if (sc(NR_getpid, 0, 0, 0, 0, 0, 0) == 1) {
        sc(NR_mount, (long)"dev", (long)"/dev", (long)"devtmpfs", 0, 0, 0);
        sc(NR_mount, (long)"proc", (long)"/proc", (long)"proc", 0, 0, 0);
        sc(NR_mount, (long)"sys", (long)"/sys", (long)"sysfs", 0, 0, 0);
        sc(NR_mount, (long)"tracefs", (long)"/sys/kernel/tracing", (long)"tracefs", 0, 0, 0);
        sc(NR_mount, (long)"tmp", (long)"/tmp", (long)"tmpfs", 0, 0, 0);
        static const char *const install[] = { "/bin/busybox", "--install", "-s", "/bin", 0 };
        run_wait(install, 0);
    }
    hc_setup();
    for (u64 i = 0; i < BUF; i += 4096) {       /* every page present and writable for the host */
        ((volatile char *)buf)[i] = 0;
    }
    sc(NR_write, 2, (long)"panda agent: ready\n", 19, 0, 0, 0);
    hc(HC_HELLO, (long)buf, BUF, sc(NR_getpid, 0, 0, 0, 0, 0, 0));
    for (;;) {
        long n = hc(HC_POLL, (long)buf, BUF, 0);
        long status;
        u64 len = 0;

        if (n <= 0) {
            int st;
            while (sc(NR_wait4, -1, (long)&st, WNOHANG, 0, 0, 0) > 0) {
            }
            sc(NR_nanosleep, (long)ts, 0, 0, 0, 0, 0);
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
