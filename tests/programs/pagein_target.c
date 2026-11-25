/*
 * The guest program of tests/pagein.py: memory a page-in request can and can't reach, and a
 * register check while requests are served around it.
 *
 * usage: pagein_target <dir> <seconds> <mode: spin|sleep>
 *
 * Maps at fixed addresses, BASE + n * STEP (64-bit: 0x7e0000000000 + n * 0x10000000; MIPS64
 * 0x5000000000; 32-bit 0x50000000 + n * 0x01000000), and tells the test BASE and STEP:
 *   0x7e0000000000 TAG    1 page, present
 *   0x7e0010000000 FILE   64 pages of <dir>/pt.<pid>, dropped from the page cache: a read does I/O
 *   0x7e0020000000 ANON   64 pages never touched (zero)
 *   0x7e0030000000 WR     64 pages never touched: the test writes a pattern, we check it at exit
 *   0x7e0040000000 HOLE   no mapping
 *   0x7e0050000000 TRUNC  a file mapping truncated to 0 (touching it is SIGBUS)
 *   0x7e0060000000 NONE   PROT_NONE
 *   0x7e0070000000 RO     a read-only private file mapping (a write must be refused)
 *   0x7e0080000000 NONE2  written, then mprotect(PROT_NONE): a non-present PTE that isn't empty
 *   0x7e0090000000 SW     written, then MADV_PAGEOUT: swap entries when swap is on
 *   0x7e00a0000000 GUARDR anonymous, with MADV_GUARD_INSTALL guard pages (Linux 6.13+)
 *   0x7e00b0000000 GROW   a MAP_GROWSDOWN region; the test asks for the page below it
 * Then hypercall READY(TAG) and, for <seconds>: spin (a user-mode loop that keeps two copies of
 * a recurrence in registers and counts mismatches) or sleep (in nanosleep), touching nothing
 * above. Then it checks FILE (pattern), ANON (zero), WR (the test's pattern, if the test wrote it),
 * and reports DONE(mismatches, bad bytes) by hypercall, prints it, exits 0.
 * Faults: SIGSEGV/SIGBUS handlers print and exit 99/98 (the test fails on either).
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/time.h>

#define PG 4096UL
#define NP 64
/* the layout: BASE + n * STEP; the READY hypercall tells the test BASE and STEP */
#if __SIZEOF_LONG__ == 4
#define BASE 0x50000000UL           /* below every 32-bit kernel's split (MIPS: 2 GiB) */
#define STEP 0x01000000UL
#elif defined(__mips__)
#define BASE 0x5000000000UL         /* MIPS64 user segments may have only 40 bits */
#define STEP 0x10000000UL
#else
#define BASE 0x7e0000000000UL
#define STEP 0x10000000UL
#endif
#define TAG    (BASE + 0 * STEP)
#define FILEA  (BASE + 1 * STEP)
#define ANON   (BASE + 2 * STEP)
#define WR     (BASE + 3 * STEP)
#define TRUNC  (BASE + 5 * STEP)
#define NONE   (BASE + 6 * STEP)
#define RO     (BASE + 7 * STEP)
#define NONE2  (BASE + 8 * STEP)
#define SW     (BASE + 9 * STEP)
#define GUARDR (BASE + 10 * STEP)
#define GROW   (BASE + 11 * STEP)
#define MAGIC 0x46525431L      /* "FRT1" */

static inline uint64_t pat(uint64_t va) { return va ^ 0xa5a5a5a5a5a5a5a5ULL; }

#if defined(__x86_64__)
static long hc(long op, long a, long b)
{
    register long r10 __asm__("r10") = b;
    long ret = MAGIC;
    __asm__ volatile("out %%eax, $0x88" : "+a"(ret) : "D"(MAGIC), "S"(op), "d"(a), "r"(r10)
                     : "memory");
    return ret;
}
#include <sys/io.h>
static void hc_setup(void) { ioperm(0x88, 4, 1); }
#elif defined(__aarch64__)
static long hc(long op, long a, long b)
{
    register long x8 __asm__("x8") = MAGIC, x0 __asm__("x0") = MAGIC;
    register long x1 __asm__("x1") = op, x2 __asm__("x2") = a, x3 __asm__("x3") = b;
    __asm__ volatile(".inst 0xd500501f" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3)
                     : "memory");
    return x0;
}
static void hc_setup(void) {}
#elif defined(__riscv)
static long hc(long op, long a, long b)
{
    register long a7 __asm__("a7") = MAGIC, a0 __asm__("a0") = MAGIC;
    register long a1 __asm__("a1") = op, a2 __asm__("a2") = a, a3 __asm__("a3") = b;
    __asm__ volatile(".4byte 0x00004013" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2), "r"(a3)
                     : "memory");
    return a0;
}
static void hc_setup(void) {}
#elif defined(__arm__)
/* penguin's arm hypercall: mcr p7, 0, r0, c0, c0, 0 (the number in r7) */
static long hc(long op, long a, long b)
{
    register long r7 __asm__("r7") = MAGIC, r0 __asm__("r0") = MAGIC;
    register long r1 __asm__("r1") = op, r2 __asm__("r2") = a, r3 __asm__("r3") = b;
    __asm__ volatile("mcr p7, 0, r0, c0, c0, 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2), "r"(r3)
                     : "memory");
    return r0;
}
static void hc_setup(void) {}
#elif defined(__mips__)
/* penguin's mips hypercall: movz $0, $0, $0 (the number in $v0) */
static long hc(long op, long a, long b)
{
    register long v0 __asm__("$2") = MAGIC, a0 __asm__("$4") = MAGIC;
    register long a1 __asm__("$5") = op, a2 __asm__("$6") = a, a3 __asm__("$7") = b;
    __asm__ volatile(".word 0x0000000a" : "+r"(v0) : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                     : "memory");
    return v0;
}
static void hc_setup(void) {}
#elif defined(__powerpc__)
/* penguin's ppc hypercall: xori 10, 10, 0 (the number in r0) */
static long hc(long op, long a, long b)
{
    register long r0 __asm__("r0") = MAGIC, r3 __asm__("r3") = MAGIC;
    register long r4 __asm__("r4") = op, r5 __asm__("r5") = a, r6 __asm__("r6") = b;
    __asm__ volatile(".long 0x694a0000" : "+r"(r3) : "r"(r0), "r"(r4), "r"(r5), "r"(r6)
                     : "memory");
    return r3;
}
static void hc_setup(void) {}
#endif

static void onsig(int s)
{
    printf("PT SIGNAL %d\n", s);
    fflush(stdout);
    _exit(s == SIGBUS ? 98 : 99);
}

/* sig mode: a timer signal (every 10 ms of user CPU time) whose handler longjmps back to the top
 * of the spin loop, so that
 * threads come back from the page-in's system call somewhere else than where it was made */
static sigjmp_buf jb;
static volatile long jumps;
static void onalrm(int s)
{
    (void)s;
    siglongjmp(jb, 1);
}

static long now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "/tmp";
    int secs = argc > 2 ? atoi(argv[2]) : 5;
    int spin = argc > 3 && !strcmp(argv[3], "spin");
    int sig = argc > 4 && !strcmp(argv[4], "sig");
    char path[256], tpath[256];

    signal(SIGSEGV, onsig);
    signal(SIGBUS, onsig);
    hc_setup();
    snprintf(path, sizeof path, "%s/pt.%d", dir, getpid());
    snprintf(tpath, sizeof tpath, "%s/ptt.%d", dir, getpid());
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    for (uint64_t p = 0; p < NP; p++) {
        uint64_t buf[PG / 8];
        for (unsigned i = 0; i < PG / 8; i++) buf[i] = pat(FILEA + p * PG + i * 8);
        write(fd, buf, PG);
    }
    fsync(fd);
    uint64_t *tag = mmap((void *)TAG, PG, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    uint64_t *file = mmap((void *)FILEA, NP * PG, PROT_READ, MAP_SHARED | MAP_FIXED, fd, 0);
    uint64_t *anon = mmap((void *)ANON, NP * PG, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    uint64_t *wr = mmap((void *)WR, NP * PG, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    mmap((void *)NONE, NP * PG, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    mmap((void *)RO, NP * PG, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, 0);
    int tfd = open(tpath, O_RDWR | O_CREAT | O_TRUNC, 0644);
    ftruncate(tfd, NP * PG);
    mmap((void *)TRUNC, NP * PG, PROT_READ, MAP_SHARED | MAP_FIXED, tfd, 0);
    ftruncate(tfd, 0);
    uint64_t *n2 = mmap((void *)NONE2, NP * PG, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    for (unsigned i = 0; i < NP * PG / 8; i++) n2[i] = pat(NONE2 + i * 8);
    mprotect(n2, NP * PG, PROT_NONE);
    uint64_t *sw = mmap((void *)SW, NP * PG, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    for (unsigned i = 0; i < NP * PG / 8; i++) sw[i] = pat(SW + i * 8);
    madvise(sw, NP * PG, 21 /* MADV_PAGEOUT */);
    mmap((void *)GUARDR, NP * PG, PROT_READ | PROT_WRITE,
         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    tag[3] = madvise((void *)GUARDR, NP * PG, 102 /* MADV_GUARD_INSTALL */) == 0;
    mmap((void *)GROW, NP * PG, PROT_READ | PROT_WRITE,
         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_GROWSDOWN, -1, 0);
    madvise(file, NP * PG, MADV_DONTNEED);
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    tag[0] = 0x5441475441475441ULL;
    tag[1] = getpid();

    hc(1, TAG, STEP);                   /* READY: the layout */

    volatile long end = now_ms() + secs * 1000L, mism = 0, iters = 0;
    if (spin && sig) {
        /* user CPU time, not wall time: a slow guest isn't buried in signals */
        struct itimerval it = { { 0, 10000 }, { 0, 10000 } };
        signal(SIGVTALRM, onalrm);
        setitimer(ITIMER_VIRTUAL, &it, NULL);
        if (sigsetjmp(jb, 1)) {
            jumps++;
        }
    }
    if (spin) {
        while (now_ms() < end) {
            /* two copies of one recurrence, kept in registers across the loop */
            register uint64_t x = 0x1234567 + iters, y = 0x1234567 + iters;
            for (int i = 0; i < 100000; i++) {
                x = x * 6364136223846793005ULL + 1442695040888963407ULL;
                y = y * 6364136223846793005ULL + 1442695040888963407ULL;
                __asm__ volatile("" : "+r"(x), "+r"(y));
                if (x != y) {
                    mism++;
                    y = x;
                }
            }
            iters++;
        }
    } else {
        while (now_ms() < end) {
            struct timespec ts = { 0, 2000000 };
            nanosleep(&ts, NULL);
            iters++;
        }
    }
    if (sig) {             /* before anything below can be jumped out of */
        struct itimerval off = { { 0, 0 }, { 0, 0 } };
        setitimer(ITIMER_VIRTUAL, &off, NULL);
    }
    long bad = 0, wrote = tag[2];       /* the test sets tag[2] when it wrote WR */
    for (unsigned i = 0; i < NP * PG / 8; i++) {
        if (file[i] != pat(FILEA + i * 8)) bad++;
        if (anon[i] != 0) bad++;
        if (wrote && wr[i] != pat(WR + i * 8)) bad++;
        if (sw[i] != pat(SW + i * 8)) bad++;
    }
    unsigned char vec;
    long grew = mincore((void *)(GROW - PG), PG, &vec) == 0;   /* did the stack VMA expand? */
    printf("PT DONE jumps %ld iters %ld mismatches %ld bad %ld wrote %ld grew %ld\n", jumps, iters,
           mism, bad, wrote,
           grew);
    fflush(stdout);
    hc(2, mism, bad);                   /* DONE */
    unlink(path);
    unlink(tpath);
    return 0;
}
