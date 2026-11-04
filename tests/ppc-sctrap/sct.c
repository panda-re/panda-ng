/*
 * Guest side of the PowerPC syscall-trap prototype test (freestanding, no
 * libc): ppc32, ppc64 and ppc64le Linux.  Every call goes through one sc
 * (or one scv) instruction, and the calls the host checks carry MAGIC in
 * the sixth argument (r8).  Prints what it did as "SCT ..." lines; the
 * host consumer (target/ppc/sc-trap-dbg.c) prints what the trap saw.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
typedef unsigned long ul;
typedef long sl;

#define MAGIC 0x5ca11edul
#define NR_exit 1
#define NR_fork 2
#define NR_read 3
#define NR_write 4
#define NR_open 5
#define NR_close 6
#define NR_getpid 20
#define NR_kill 37
#define NR_pipe 42
#define NR_getppid 64
#define NR_mmap 90
#define NR_setitimer 104
#define NR_wait4 114
#define NR_clone 120
#define NR_sched_yield 158
#define NR_nanosleep 162
#define NR_rt_sigaction 173
#define NR_vfork 189
#define NR_gettid 207
#define NR_exit_group 234
#define NR_clone3 435

#define SIGALRM 14
#define SIGCONT 18
#define SIGSTOP 19
#define SA_RESTART 0x10000000ul
#define EINTR 4

static int use_scv __attribute__((unused));
static volatile ul *shared;     /* [0]: tagged calls made, all tasks */

__attribute__((noinline)) static sl sys_sc(ul n, ul a, ul b, ul c, ul d,
                                           ul e, ul f)
{
    register ul r0 __asm__("r0") = n;
    register ul r3 __asm__("r3") = a;
    register ul r4 __asm__("r4") = b;
    register ul r5 __asm__("r5") = c;
    register ul r6 __asm__("r6") = d;
    register ul r7 __asm__("r7") = e;
    register ul r8 __asm__("r8") = f;

    __asm__ volatile("sc\n\tbns+ 1f\n\tneg %1,%1\n1:"
                     : "+r"(r0), "+r"(r3), "+r"(r4), "+r"(r5), "+r"(r6),
                       "+r"(r7), "+r"(r8)
                     :
                     : "memory", "cr0", "r9", "r10", "r11", "r12", "ctr",
                       "xer");
    return r3;
}

#ifdef __powerpc64__
__attribute__((noinline)) static sl sys_scv(ul n, ul a, ul b, ul c, ul d,
                                            ul e, ul f)
{
    register ul r0 __asm__("r0") = n;
    register ul r3 __asm__("r3") = a;
    register ul r4 __asm__("r4") = b;
    register ul r5 __asm__("r5") = c;
    register ul r6 __asm__("r6") = d;
    register ul r7 __asm__("r7") = e;
    register ul r8 __asm__("r8") = f;

    __asm__ volatile(".long 0x44000001"
                     : "+r"(r0), "+r"(r3), "+r"(r4), "+r"(r5), "+r"(r6),
                       "+r"(r7), "+r"(r8)
                     :
                     : "memory", "cr0", "cr1", "cr5", "cr6", "cr7", "r9",
                       "r10", "r11", "r12", "ctr", "lr", "xer");
    return r3;
}
#endif

static sl sys(ul n, ul a, ul b, ul c, ul d, ul e, ul f)
{
#ifdef __powerpc64__
    if (use_scv) {
        return sys_scv(n, a, b, c, d, e, f);
    }
#endif
    return sys_sc(n, a, b, c, d, e, f);
}

#define S0(n) sys(n, 0, 0, 0, 0, 0, 0)
#define S1(n, a) sys(n, (ul)(a), 0, 0, 0, 0, 0)
#define S2(n, a, b) sys(n, (ul)(a), (ul)(b), 0, 0, 0, 0)
#define S3(n, a, b, c) sys(n, (ul)(a), (ul)(b), (ul)(c), 0, 0, 0)
#define S4(n, a, b, c, d) sys(n, (ul)(a), (ul)(b), (ul)(c), (ul)(d), 0, 0)
/* a tagged call, counted */
static sl T(ul n, ul a, ul b, ul c, ul d, ul e)
{
    sl r = sys(n, a, b, c, d, e, MAGIC);

    __atomic_fetch_add(&shared[0], 1, __ATOMIC_SEQ_CST);
    return r;
}

static ul slen(const char *s)
{
    ul n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

static void puts_(const char *s)
{
    sys_sc(NR_write, 1, (ul)s, slen(s), 0, 0, 0);
}

static void putn(sl v)
{
    char b[24], *p = b + 23;
    int neg = v < 0;
    ul u = neg ? -(ul)v : (ul)v;

    *p = 0;
    do {
        *--p = '0' + u % 10;
        u /= 10;
    } while (u);
    if (neg) {
        *--p = '-';
    }
    puts_(p);
}

static void say(const char *what, sl v)
{
    puts_("SCT ");
    puts_(what);
    puts_(" ");
    putn(v);
    puts_("\n");
}

static void sexit(int c)
{
    for (;;) {
        sys_sc(NR_exit, c, 0, 0, 0, 0, 0);
    }
}

static void reap(void)
{
    while (S4(NR_wait4, -1l, 0, 0x40000000, 0) > 0) {  /* __WALL */
    }
}

/* ---- tests ---- */

static void t_basic(void)
{
    sl tid = S0(NR_gettid);
    int bad = 0;

    for (int i = 0; i < 1000; i++) {
        bad += T(NR_gettid, tid, 0, 0, 0, 0) != tid;
    }
    for (int i = 0; i < 10; i++) {
        bad += T(NR_read, -1ul, 0, 0, 0, 0xbad) != -9;   /* EBADF */
    }
    say("basic.bad", bad);
}

static int pfd[8][2];

static void reader(int i, int rounds)
{
    char buf[16];
    sl tid = S0(NR_gettid);

    for (int r = 0; r < rounds; r++) {
        T(NR_read, pfd[i][0], (ul)buf, i + 1, 0, 0);
        T(NR_gettid, tid, 0, 0, 0, 0);
    }
}

static void feed(int n, int rounds)
{
    static const char x[16] = "0123456789abcdef";

    for (int r = 0; r < rounds; r++) {
        for (int i = 0; i < n; i++) {
            S3(NR_write, pfd[i][1], x, i + 1);
            S0(NR_sched_yield);
        }
    }
}

static void t_procs(void)
{
    const int n = 4, rounds = 200;

    for (int i = 0; i < n; i++) {
        S1(NR_pipe, pfd[i]);
    }
    for (int i = 0; i < n; i++) {
        if (S0(NR_fork) == 0) {
            reader(i, rounds);
            sexit(0);
        }
    }
    feed(n, rounds);
    reap();
    say("procs.done", n);
}

static volatile int thr_done;
static char stacks[5][16384] __attribute__((aligned(16)));

static void thread_main(long i)
{
    reader(i, 200);
    __atomic_fetch_add(&thr_done, 1, __ATOMIC_SEQ_CST);
}

/*
 * clone/clone3 a CLONE_VM thread that runs fn(arg) on @stack and exits.
 * The child's first instructions after the sc run on the new stack: fn and
 * arg are in r30/r31, which the child inherits.
 */
static sl spawn(int use_clone3, void *stack_top, void (*fn)(long), long arg)
{
    const ul flags = 0x100 | 0x200 | 0x400 | 0x800 | 0x10000 | 0x40000;
    /* VM FS FILES SIGHAND THREAD SYSVSEM */
    static unsigned long long cargs[11];
    register ul r0 __asm__("r0");
    register ul r3 __asm__("r3");
    register ul r4 __asm__("r4");
    register ul r5 __asm__("r5") = 0;
    register ul r6 __asm__("r6") = 0;
    register ul r7 __asm__("r7") = 0;
    register ul r8 __asm__("r8") = MAGIC;
    register ul r30 __asm__("r30") = (ul)fn;
    register ul r31 __asm__("r31") = arg;
    ul sp = ((ul)stack_top - 256) & ~15ul;

    ((ul *)sp)[0] = 0;
    if (use_clone3) {
        for (int i = 0; i < 11; i++) {
            cargs[i] = 0;
        }
        cargs[0] = flags;
        cargs[5] = (ul)stack_top - 16384;   /* stack, stack_size */
        cargs[6] = 16384 - 256;
        r0 = NR_clone3;
        r3 = (ul)cargs;
        r4 = sizeof(cargs);
    } else {
        r0 = NR_clone;
        r3 = flags;
        r4 = sp;
    }
    __atomic_fetch_add(&shared[0], 1, __ATOMIC_SEQ_CST);
    __asm__ volatile(
        "sc\n\t"
        "bso- 1f\n\t"
        "cmpwi %1,0\n\t"
        "bne 1f\n\t"
        /* child: on its stack */
#ifdef __powerpc64__
        "stdu 1,-128(1)\n\t"
        "mr 12,30\n\t"
#else
        "stwu 1,-64(1)\n\t"
#endif
        "mtctr 30\n\t"
        "mr 3,31\n\t"
        "bctrl\n\t"
        "li 0,1\n\t"
        "li 3,0\n\t"
        "sc\n\t"
        "1:"
        : "+r"(r0), "+r"(r3), "+r"(r4), "+r"(r5), "+r"(r6), "+r"(r7),
          "+r"(r8), "+r"(r30), "+r"(r31)
        :
        : "memory", "cr0", "r9", "r10", "r11", "r12", "ctr", "lr", "xer");
    (void)sp;
    return r3;
}

static void t_threads(void)
{
    const int n = 4, rounds = 200;
    sl t[5];

    for (int i = 0; i < n; i++) {
        S1(NR_pipe, pfd[i]);
    }
    thr_done = 0;
    for (int i = 0; i < n; i++) {
        /* thread 3 by clone3 (its stack is given as base + size) */
        t[i] = spawn(i == 3, stacks[i] + sizeof(stacks[i]), thread_main, i);
        if (t[i] <= 0) {
            say("threads.spawn-failed", t[i]);
        }
    }
    feed(n, rounds);
    while (thr_done < n) {
        S0(NR_sched_yield);
    }
    say("threads.done", n);
}

/*
 * vfork: the child runs on the parent's stack until it exits, so it runs
 * in this asm only: a tagged gettid with its own tid in r3, then exit.
 */
#define VFORK_ASM(insn)                                                     \
    __asm__ volatile(insn "\n\t"                                            \
                     "bso- 1f\n\t"                                          \
                     "cmpwi 3,0\n\t"                                        \
                     "bne 1f\n\t"                                           \
                     "li 8,0\n\tli 0,207\n\t" insn "\n\t"                  \
                     "li 0,207\n\t"                                         \
                     "lis 8,0x5ca\n\t"                                      \
                     "ori 8,8,0x11ed\n\t" insn "\n\t"                       \
                     "li 0,1\n\t"                                           \
                     "li 3,0\n\t" insn "\n\t"                               \
                     "1:"                                                   \
                     : "+r"(r0), "+r"(r3), "+r"(r8)                         \
                     :                                                      \
                     : "memory", "cr0", "cr1", "cr5", "cr6", "cr7", "r4",   \
                       "r5", "r6", "r7", "r9", "r10", "r11", "r12", "ctr",  \
                       "lr", "xer")

static sl vfork_tagged(void)
{
    register ul r0 __asm__("r0") = NR_vfork;
    register ul r3 __asm__("r3") = 0;
    register ul r8 __asm__("r8") = MAGIC;

#ifdef __powerpc64__
    if (use_scv) {
        VFORK_ASM(".long 0x44000001");
        return r3;          /* scv: -errno */
    }
#endif
    VFORK_ASM("sc");
    return r3;              /* an error is never 0 or a pid here */
}

static void t_vfork(void)
{
    int kids = 0;

    for (int i = 0; i < 50; i++) {
        sl p = vfork_tagged();

        __atomic_fetch_add(&shared[0], 2, __ATOMIC_SEQ_CST);  /* + child's */
        kids += p > 0;
        reap();
    }
    for (int i = 0; i < 20; i++) {
        sl p = sys(NR_fork, 0, 0, 0, 0, 0, MAGIC);
        if (p > 0) {
            __atomic_fetch_add(&shared[0], 1, __ATOMIC_SEQ_CST);
        }
        if (p == 0) {
            sl tid = S0(NR_gettid);
            T(NR_gettid, tid, 0, 0, 0, 0);
            sexit(0);
        }
        kids += p > 0;
        reap();
    }
    say("vfork+fork.parents-saw-child", kids);
}

/* signals: the kernel's struct sigaction (handler, flags, restorer, mask) */
static int sigfd[2];
static volatile int handled;

static void on_alarm(int sig)
{
    (void)sig;
    handled++;
    if (handled > 1) {
        S3(NR_write, sigfd[1], "x", 1);       /* the restarted read gets it */
    }
}

static void set_alarm(ul flags)
{
    ul sa[5] = { (ul)on_alarm, flags, 0, 0, 0 };
    ul it[4] = { 0, 0, 0, 100000 };           /* 100 ms, once */

    S4(NR_rt_sigaction, SIGALRM, sa, 0, 8);
    S3(NR_setitimer, 0, it, 0);
}

static void t_signal(void)
{
    char c;
    sl r;

    S1(NR_pipe, sigfd);
    handled = 0;
    set_alarm(0);
    r = T(NR_read, sigfd[0], (ul)&c, 1, 0, 0x1e);
    say("signal.norestart.read", r);           /* -EINTR */
    set_alarm(SA_RESTART);
    r = T(NR_read, sigfd[0], (ul)&c, 1, 0, 0x1e);
    say("signal.restart.read", r);             /* 1, after a restart */
    say("signal.handled", handled);

    /* no handler: SIGSTOP + SIGCONT from a child while we sleep / read */
    sl me = S0(NR_getpid);
    if (S0(NR_fork) == 0) {
        ul ts[2] = { 0, 100000000 };
        S2(NR_nanosleep, ts, 0);
        S2(NR_kill, me, SIGSTOP);
        S2(NR_nanosleep, ts, 0);
        S2(NR_kill, me, SIGCONT);
        S2(NR_nanosleep, ts, 0);
        S3(NR_write, sigfd[1], "y", 1);
        sexit(0);
    }
    r = T(NR_read, sigfd[0], (ul)&c, 1, 0, 0x1e);
    say("signal.stopcont.read", r);
    reap();
    if (S0(NR_fork) == 0) {
        ul ts[2] = { 0, 100000000 };
        S2(NR_nanosleep, ts, 0);
        S2(NR_kill, me, SIGSTOP);
        S2(NR_nanosleep, ts, 0);
        S2(NR_kill, me, SIGCONT);
        sexit(0);
    }
    {
        ul ts[2] = { 0, 400000000 }, rem[2];
        r = T(NR_nanosleep, (ul)ts, (ul)rem, 0, 0, 0x1e);
        say("signal.stopcont.nanosleep", r);  /* 0, via restart_syscall */
    }
    reap();
}

static void t_mutate(void)
{
    sl tid = S0(NR_gettid);
    sl ppid = S0(NR_getppid);
    sl r;
    char *page;
    sl fd;

    r = T(NR_getppid, 0, 0, 0, 0, 0x1a1);
    say("mutate.inject-enter.getppid==tid", r == tid);
    r = T(NR_getppid, 0, 0, 0, 0, 0x1a2);
    say("mutate.inject-return.getppid==tid+1e6", r == tid + 1000000);
    r = S0(NR_getppid);
    say("mutate.plain.getppid==ppid", r == ppid);
    page = (char *)sys_sc(NR_mmap, 0, 65536, 3, 0x22, -1ul, 0);
    fd = S2(NR_open, "/dev/null", 1);
    r = T(NR_write, fd, (ul)(page + 32768), 1, 0, 0xfa1);
    say("mutate.fault-enter.write", r);
    S1(NR_close, fd);
}

static void run(const char *tag)
{
    shared[0] = 0;
    puts_("SCT begin ");
    puts_(tag);
    puts_("\n");
    t_basic();
    t_procs();
    t_threads();
    t_vfork();
    t_signal();
    t_mutate();
    say("tagged-calls", shared[0]);
}

__attribute__((unused)) static int has_scv(ul *auxv)
{
    for (; auxv[0]; auxv += 2) {
        if (auxv[0] == 26) {                    /* AT_HWCAP2 */
            return (auxv[1] & 0x00100000) != 0; /* PPC_FEATURE2_SCV */
        }
    }
    return 0;
}

void cmain(ul *sp)
{
    ul argc = sp[0];
    char **argv = (char **)(sp + 1);
    char **envp = argv + argc + 1;
    ul *auxv;

    while (*envp) {
        envp++;
    }
    auxv = (ul *)(envp + 1);
    shared = (volatile ul *)sys_sc(NR_mmap, 0, 4096, 3, 0x21, -1ul, 0);
#ifdef __powerpc64__
    run("ppc64-sc");
    if (has_scv(auxv)) {
        use_scv = 1;
        run("ppc64-scv");
        use_scv = 0;
    } else {
        say("no-scv", 0);
    }
#else
    (void)auxv;
    run("ppc32-sc");
#endif
    sys_sc(NR_exit_group, 0, 0, 0, 0, 0, 0);
}

__asm__(
    ".text\n"
    ".globl _start\n"
    "_start:\n"
#ifdef __powerpc64__
    "  bl 1f\n"
    "1: mflr 12\n"
    "  addis 2,12,(.TOC.-1b)@ha\n"
    "  addi 2,2,(.TOC.-1b)@l\n"
    "  mr 3,1\n"
    "  clrrdi 1,1,4\n"
    "  li 0,0\n"
    "  stdu 0,-128(1)\n"
    "  bl cmain\n"
    "  nop\n"
#else
    "  mr 3,1\n"
    "  clrrwi 1,1,4\n"
    "  li 0,0\n"
    "  stwu 0,-64(1)\n"
    "  bl cmain\n"
#endif
    "  trap\n");
