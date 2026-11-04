/*
 * Guest side of the MIPS syscall-trap prototype checks (o32, n32, n64).
 * Each test prints what the task itself saw; check.py compares it
 * with the trap's log (SYSCALL_TRAP_SELFTEST).
 *
 *   zig cc -target mipsel-linux-musleabi      -static -O1 -pthread sct.c   (o32)
 *   zig cc -target mips64el-linux-muslabi64   -static -O1 -pthread sct.c   (n64)
 *   zig cc -target mips64el-linux-muslabin32  -static -O1 -pthread sct.c   (n32)
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

/* the register width */
#if _MIPS_SIM == _ABIO32
typedef long R;
#define L "l"
#else
typedef long long R;
#define L "ll"
#endif

#if _MIPS_SIM == _ABIO32
#define ABI "o32"
/* a 6-argument call; e and f go to sp+16 and sp+20 and are read back */
__attribute__((noinline)) static R sc6(R n, R a, R b, R c, R d, R *e, R *f)
{
    register long v0 __asm__("$2") = n;
    register long a0 __asm__("$4") = a, a1 __asm__("$5") = b,
                  a2 __asm__("$6") = c, a3 __asm__("$7") = d;
    long ee = *e, ff = *f;

    __asm__ volatile(
        "addiu $sp, $sp, -32\n\t"
        "sw %[e], 16($sp)\n\t"
        "sw %[f], 20($sp)\n\t"
        "syscall\n\t"
        "lw %[e], 16($sp)\n\t"
        "lw %[f], 20($sp)\n\t"
        "addiu $sp, $sp, 32\n\t"
        : "+r"(v0), "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3), [e] "+r"(ee),
          [f] "+r"(ff)
        :
        : "$1", "$3", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15",
          "$24", "$25", "hi", "lo", "memory");
    *e = ee;
    *f = ff;
    return a3 ? -v0 : v0;
}

/* the same, returning a0-a2 as they are after the call */
__attribute__((noinline)) static R sc3r(R n, R *a, R *b, R *c)
{
    register long v0 __asm__("$2") = n;
    register long a0 __asm__("$4") = *a, a1 __asm__("$5") = *b,
                  a2 __asm__("$6") = *c, a3 __asm__("$7") = 0;

    __asm__ volatile("syscall"
                     : "+r"(v0), "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3)
                     :
                     : "$1", "$3", "$8", "$9", "$10", "$11", "$12", "$13",
                       "$14", "$15", "$24", "$25", "hi", "lo", "memory");
    *a = a0;
    *b = a1;
    *c = a2;
    return a3 ? -v0 : v0;
}
#else
#if _MIPS_SIM == _ABI64
#define ABI "n64"
#else
#define ABI "n32"
#endif
__attribute__((noinline)) static R sc6(R n, R a, R b, R c, R d, R *e, R *f)
{
    register R v0 __asm__("$2") = n;
    register R a0 __asm__("$4") = a, a1 __asm__("$5") = b, a2 __asm__("$6") = c,
               a3 __asm__("$7") = d, a4 __asm__("$8") = *e, a5 __asm__("$9") = *f;

    __asm__ volatile("syscall"
                     : "+r"(v0), "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3),
                       "+r"(a4), "+r"(a5)
                     :
                     : "$1", "$3", "$10", "$11", "$12", "$13", "$14", "$15",
                       "$24", "$25", "hi", "lo", "memory");
    *e = a4;
    *f = a5;
    return a3 ? -v0 : v0;
}

__attribute__((noinline)) static R sc3r(R n, R *a, R *b, R *c)
{
    register R v0 __asm__("$2") = n;
    register R a0 __asm__("$4") = *a, a1 __asm__("$5") = *b,
               a2 __asm__("$6") = *c, a3 __asm__("$7") = 0;

    __asm__ volatile("syscall"
                     : "+r"(v0), "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3)
                     :
                     : "$1", "$3", "$8", "$9", "$10", "$11", "$12", "$13",
                       "$14", "$15", "$24", "$25", "hi", "lo", "memory");
    *a = a0;
    *b = a1;
    *c = a2;
    return a3 ? -v0 : v0;
}
#endif

static R sc(R n, R a, R b, R c)
{
    R e = 0, f = 0;
    return sc6(n, a, b, c, 0, &e, &f);
}

static R gettid_(void)
{
    return sc(SYS_gettid, 0, 0, 0);
}

#define P(...) do { printf("T " ABI " " __VA_ARGS__); fflush(stdout); } while (0)

/* --- basic: arguments 1-6 --- */
static void t_basic(void)
{
    R e = 0x55555555, f = 0x66666666;
    R r = sc6(SYS_getpid, 0x11111111, 0x22222222, 0x33333333, 0x44444444, &e, &f);
    P("basic tid=%" L "d ret=%" L "d args=11111111,22222222,33333333,44444444,55555555,66666666\n",
      gettid_(), r);
}

/* --- abi: the number picks the ABI --- */
static void t_abi(void)
{
    P("abi tid=%" L "d o32=%" L "d n64=%" L "d n32=%" L "d\n", gettid_(), sc(4020, 0, 0, 0),
      sc(5038, 0, 0, 0), sc(6038, 0, 0, 0));
}

/* --- count: N calls, all paired --- */
static void t_count(void)
{
    int n = 1000;
    R t = gettid_();
    for (int i = 0; i < n; i++) {
        sc(SYS_getppid, 0xc0de, i, 0);
    }
    P("count tid=%" L "d n=%d\n", t, n);
}

/* --- threads: CLONE_VM tasks blocked at one read site, woken in reverse --- */
#define NT 4
static int tp[NT][2];
__attribute__((noinline)) static R rd(int fd, void *buf, int n)
{
    return sc(SYS_read, fd, (R)(uintptr_t)buf, n);
}
static void *thr(void *arg)
{
    int i = (int)(intptr_t)arg;
    char buf[64];
    R t = gettid_();
    R r = rd(tp[i][0], buf, sizeof(buf));
    P("thread i=%d tid=%" L "d fd=%d ret=%" L "d\n", i, t, tp[i][0], r);
    return NULL;
}
static void t_threads(void)
{
    pthread_t th[NT];
    char buf[64];
    memset(buf, 'x', sizeof(buf));
    for (int i = 0; i < NT; i++) {
        pipe(tp[i]);
        pthread_create(&th[i], NULL, thr, (void *)(intptr_t)i);
    }
    usleep(300000);         /* all of them in read */
    for (int i = NT - 1; i >= 0; i--) {
        write(tp[i][1], buf, i + 1);
        usleep(50000);
    }
    for (int i = 0; i < NT; i++) {
        pthread_join(th[i], NULL);
    }
    P("threads done\n");
}

/* --- vfork: the child returns from the parent's clone, on the parent's stack --- */
static void t_vfork(void)
{
    R pid;
    R t = gettid_();
#if _MIPS_SIM == _ABIO32
    register long v0 __asm__("$2") = SYS_clone;
    register long a0 __asm__("$4") = CLONE_VM | CLONE_VFORK | SIGCHLD,
                  a1 __asm__("$5") = 0, a2 __asm__("$6") = 0, a3 __asm__("$7") = 0;
    __asm__ volatile(
        "addiu $sp, $sp, -32\n\t"
        "sw $0, 16($sp)\n\t"
        "syscall\n\t"
        "bnez $7, 1f\n\t"
        "nop\n\t"
        "bnez $2, 1f\n\t"
        "nop\n\t"
        "li $2, %[getpid]\n\t"      /* the child, on the parent's stack */
        "li $4, 0xc41d\n\t"
        "syscall\n\t"
        "li $2, %[exit]\n\t"
        "li $4, 7\n\t"
        "syscall\n\t"
        "1: addiu $sp, $sp, 32\n\t"
        : "+r"(v0), "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3)
        : [getpid] "i"(SYS_getpid), [exit] "i"(SYS_exit)
        : "$1", "$3", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15",
          "$24", "$25", "hi", "lo", "memory");
    pid = a3 ? -(R)v0 : (R)v0;
#else
    register R v0 __asm__("$2") = SYS_clone;
    register R a0 __asm__("$4") = CLONE_VM | CLONE_VFORK | SIGCHLD,
               a1 __asm__("$5") = 0, a2 __asm__("$6") = 0, a3 __asm__("$7") = 0,
               a4 __asm__("$8") = 0;
    __asm__ volatile(
        "syscall\n\t"
        "bnez $7, 1f\n\t"
        "nop\n\t"
        "bnez $2, 1f\n\t"
        "nop\n\t"
        "li $2, %[getpid]\n\t"
        "li $4, 0xc41d\n\t"
        "syscall\n\t"
        "li $2, %[exit]\n\t"
        "li $4, 7\n\t"
        "syscall\n\t"
        "1:\n\t"
        : "+r"(v0), "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3), "+r"(a4)
        : [getpid] "i"(SYS_getpid), [exit] "i"(SYS_exit)
        : "$1", "$3", "$9", "$10", "$11", "$12", "$13", "$14", "$15",
          "$24", "$25", "hi", "lo", "memory");
    pid = a3 ? -v0 : v0;
#endif
    int st = 0;
    waitpid(pid, &st, 0);
    P("vfork tid=%" L "d child=%" L "d status=%d\n", t, pid, WEXITSTATUS(st));
}

/* --- clone3: a CLONE_VM|CLONE_VFORK child on its own stack --- */
struct cl_args {
    uint64_t flags, pidfd, child_tid, parent_tid, exit_signal, stack, stack_size,
             tls, set_tid, set_tid_size, cgroup;
};
static void t_clone3(void)
{
    static struct cl_args ca;
    static char stk[16384] __attribute__((aligned(16)));
    R pid, t = gettid_();

    memset(&ca, 0, sizeof(ca));
    ca.flags = CLONE_VM | CLONE_VFORK;
    ca.exit_signal = SIGCHLD;
    ca.stack = (uintptr_t)stk;
    ca.stack_size = sizeof(stk);
#if _MIPS_SIM == _ABIO32
    register long v0 __asm__("$2") = SYS_clone3;
    register long a0 __asm__("$4") = (long)&ca, a1 __asm__("$5") = sizeof(ca),
                  a3 __asm__("$7") = 0;
    __asm__ volatile(
        "syscall\n\t"
        "bnez $7, 1f\n\t"
        "nop\n\t"
        "bnez $2, 1f\n\t"
        "nop\n\t"
        "li $2, %[getpid]\n\t"
        "li $4, 0xc103\n\t"
        "syscall\n\t"
        "li $2, %[exit]\n\t"
        "li $4, 9\n\t"
        "syscall\n\t"
        "1:\n\t"
        : "+r"(v0), "+r"(a0), "+r"(a1), "+r"(a3)
        : [getpid] "i"(SYS_getpid), [exit] "i"(SYS_exit)
        : "$1", "$3", "$6", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15",
          "$24", "$25", "hi", "lo", "memory");
    pid = a3 ? -(R)v0 : (R)v0;
#else
    register R v0 __asm__("$2") = SYS_clone3;
    register R a0 __asm__("$4") = (R)(uintptr_t)&ca, a1 __asm__("$5") = sizeof(ca),
               a3 __asm__("$7") = 0;
    __asm__ volatile(
        "syscall\n\t"
        "bnez $7, 1f\n\t"
        "nop\n\t"
        "bnez $2, 1f\n\t"
        "nop\n\t"
        "li $2, %[getpid]\n\t"
        "li $4, 0xc103\n\t"
        "syscall\n\t"
        "li $2, %[exit]\n\t"
        "li $4, 9\n\t"
        "syscall\n\t"
        "1:\n\t"
        : "+r"(v0), "+r"(a0), "+r"(a1), "+r"(a3)
        : [getpid] "i"(SYS_getpid), [exit] "i"(SYS_exit)
        : "$1", "$3", "$6", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15",
          "$24", "$25", "hi", "lo", "memory");
    pid = a3 ? -v0 : v0;
#endif
    int st = 0;
    waitpid(pid, &st, 0);
    P("clone3 tid=%" L "d child=%" L "d status=%d\n", t, pid, WEXITSTATUS(st));
}

/* --- signals: a read interrupted by a handler, with and without SA_RESTART --- */
static int sp_[2];
static void on_alrm(int s)
{
    (void)s;
    sc(SYS_write, sp_[1], (R)(uintptr_t)"!", 1);
}
static void t_sig(int restart)
{
    struct sigaction sa;
    char b[8];
    R t = gettid_();

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_alrm;
    sa.sa_flags = restart ? SA_RESTART : 0;
    sigaction(SIGALRM, &sa, NULL);
    pipe(sp_);
    alarm(1);
    R r = rd(sp_[0], b, sizeof(b));
    P("sig restart=%d tid=%" L "d fd=%d ret=%" L "d\n", restart, t, sp_[0], r);
    close(sp_[0]);
    close(sp_[1]);
}

/* --- stopcont: SIGSTOP/SIGCONT on a blocked read: no handler, transparent restart --- */
static void t_stopcont(void)
{
    int p[2];
    char b[8];
    pipe(p);
    pid_t c = fork();
    if (c == 0) {
        R t = gettid_();
        R r = rd(p[0], b, sizeof(b));
        P("stopcont child tid=%" L "d fd=%d ret=%" L "d\n", t, p[0], r);
        _exit(0);
    }
    usleep(200000);
    kill(c, SIGSTOP);
    usleep(100000);
    kill(c, SIGCONT);
    usleep(100000);
    write(p[1], "abc", 3);
    waitpid(c, NULL, 0);
}

/* --- asid: a call blocked while more than 256 address spaces come and go --- */
static void t_asid(void)
{
    R t = gettid_();
    pid_t c = fork();
    if (c == 0) {
        for (int i = 0; i < 400; i++) {
            pid_t g = fork();
            if (g == 0) {
                _exit(0);
            }
            waitpid(g, NULL, 0);
        }
        _exit(3);
    }
    int st = 0;
    R r = sc(SYS_wait4, c, (R)(uintptr_t)&st, 0);
    P("asid tid=%" L "d child=%d wait4=%" L "d status=%d\n", t, c, r, WEXITSTATUS(st));
}

/* --- mutations through the self-test consumer --- */
static void t_mut(void)
{
    R pid = getpid(), ppid = getppid(), t = gettid_();
    R a, b, c, r;

    r = sc(SYS_getpid, 0x1a1ec700, 0, 0);
    P("inject tid=%" L "d ret=%" L "d pid=%" L "d ppid=%" L "d\n", t, r, pid, ppid);

    a = 0x1a1ec701; b = 0xb0b0; c = 0xc0c0;
    r = sc3r(SYS_getpid, &a, &b, &c);
    P("injret tid=%" L "d ret=%" L "d pid=%" L "d ppid=%" L "d a0=%" L "x a1=%" L "x a2=%" L "x\n", t, r, pid,
      ppid, a, b, c);

    int fd = open("/tmp/inj", O_RDWR | O_CREAT | O_TRUNC, 0644);
    static char z[8192];
    write(fd, z, sizeof(z));
    R e = 0xdead0001, f = 0xdead0002;
    r = sc6(SYS_getpid, 0x1a1ec702, fd, (R)(uintptr_t)"INJECTED", 8, &e, &f);
    char got[9] = {0};
    pread(fd, got, 8, 4096);
    close(fd);
    P("injpw tid=%" L "d ret=%" L "d pid=%" L "d file@4096=%s e=%" L "x f=%" L "x\n", t, r, pid, got,
      e, f);

    long pg = sysconf(_SC_PAGESIZE);
    unsigned char vec = 0;
    /* anonymous pages: mincore says whether the task's page table has them */
    char *fm = mmap(NULL, pg, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mincore(fm, pg, &vec);
    int before = vec & 1;
    r = sc(SYS_getpid, 0xfa017000, (R)(uintptr_t)fm, 0);
    mincore(fm, pg, &vec);
    P("fault tid=%" L "d ret=%" L "d pid=%" L "d resident_before=%d after=%d\n", t, r, pid, before,
      vec & 1);
    char *fm2 = mmap(NULL, pg, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mincore(fm2, pg, &vec);
    before = vec & 1;
    r = sc(SYS_getpid, 0xfa017001, (R)(uintptr_t)fm2, 0);
    mincore(fm2, pg, &vec);
    P("faultret tid=%" L "d ret=%" L "d pid=%" L "d resident_before=%d after=%d\n", t, r, pid,
      before, vec & 1);

    P("skip tid=%" L "d ret_err=%" L "d ret_ok=%" L "d\n", t, sc(SYS_getpid, 0x5c1f0000, -13, 0),
      sc(SYS_getpid, 0x5c1f0000, 77, 0));
    P("setret tid=%" L "d ret=%" L "d\n", t, sc(SYS_getpid, 0x5e7e0000, -2, 0));
}

/* --- injloop: inject with o32 stack arguments, many times, under TLB pressure --- */
static void t_injloop(void)
{
    int fd = open("/tmp/inj2", O_RDWR | O_CREAT | O_TRUNC, 0644);
    static char z[8192], big[1 << 20];
    int bad = 0, n = 200;

    write(fd, z, sizeof(z));
    for (int i = 0; i < n; i++) {
        R e = 0xdead0000 + i, f = 0xbeef0000 + i;
        /* touch many pages so the TLB loses the stack's entry */
        for (int j = 0; j < (int)sizeof(big); j += 4096) {
            big[j] = i;
        }
        R r = sc6(SYS_getpid, 0x1a1ec702, fd, (R)(uintptr_t)"INJECTED", 8, &e, &f);
        if (r != getpid() || e != (R)(0xdead0000 + i) || f != (R)(0xbeef0000 + i)) {
            bad++;
        }
    }
    close(fd);
    P("injloop n=%d bad=%d\n", n, bad);
}

int main(int argc, char **argv)
{
    const char *w = argc > 1 ? argv[1] : "all";
    int all = !strcmp(w, "all");

#define RUN(name, call) if (all || !strcmp(w, name)) { call; }
    RUN("basic", t_basic());
    RUN("abi", t_abi());
    RUN("count", t_count());
    RUN("threads", t_threads());
    RUN("vfork", t_vfork());
    RUN("clone3", t_clone3());
    RUN("sig", (t_sig(1), t_sig(0)));
    RUN("stopcont", t_stopcont());
    RUN("asid", t_asid());
    RUN("mut", t_mut());
    RUN("injloop", t_injloop());
    P("end\n");
    return 0;
}
