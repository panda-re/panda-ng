/*
 * pagetest: guest-side driver for the pagein prototype.
 *
 * The host plugin hooks either
 *   - the user-mode "marker" instruction in marker() below (address in the
 *     first argument register, length in the second), or
 *   - the kernel's ksys_write() entry, filtered on fd == HOOK_FD.
 * In both cases it reads [addr, addr+len) from the hook with
 * qemu_plugin_read_memory_vaddr_fault() and logs what it got.  This program
 * prints the ground truth afterwards so the two can be compared.
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
#include <linux/fuse.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sys/eventfd.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define HOOK_FD 77
/* data.bin is laid out in 4 KiB "pages"; MMU pages may be 16 or 64 KiB */
#define FPG 4096
static long PG;

#ifndef MADV_PAGEOUT
#define MADV_PAGEOUT 21
#endif

__attribute__((noinline)) static void marker(const void *p, long n)
{
#if defined(__x86_64__)
    /* nopl 0x45475041(%rax) -- "APGE" */
    __asm__ volatile(
#ifdef PAGEIN_KVM
                     /* KVM has no instruction callbacks: trap here */
                     "int3\n"
#endif
                     ".byte 0x0f,0x1f,0x80,0x41,0x50,0x47,0x45"
                     :: "D"(p), "S"(n) : "memory");
#elif defined(__i386__)
    /* the same nopl, for the compat build */
    __asm__ volatile(".byte 0x0f,0x1f,0x80,0x41,0x50,0x47,0x45"
                     :: "D"(p), "S"(n) : "memory");
#elif defined(__aarch64__)
    register const void *x0 __asm__("x0") = p;
    register long x1 __asm__("x1") = n;
    __asm__ volatile("hint #42" :: "r"(x0), "r"(x1) : "memory");
#elif defined(__arm__)
    register const void *r0 __asm__("r0") = p;
    register long r1 __asm__("r1") = n;
    /* orr r12, r12, r12 is never emitted by compilers here */
    __asm__ volatile("orr r12, r12, r12" :: "r"(r0), "r"(r1) : "r12", "memory");
#elif defined(__mips__)
    register const void *a0 __asm__("$4") = p;
    register long a1 __asm__("$5") = n;
    /* ori $zero, $zero, 0x5041: a nop */
    __asm__ volatile("ori $0, $0, 0x5041" :: "r"(a0), "r"(a1) : "memory");
#elif defined(__powerpc64__)
    register const void *r3 __asm__("r3") = p;
    register long r4 __asm__("r4") = n;
    /* ori r0, r0, 0x5041 (r0 is scratch) */
    __asm__ volatile(".long 0x60005041" :: "r"(r3), "r"(r4) : "r0", "memory");
#elif defined(__riscv)
    register const void *a0 __asm__("a0") = p;
    register long a1 __asm__("a1") = n;
    /* addi x0, x0, 0x541: a HINT (nop), forced 32-bit */
    __asm__ volatile(".4byte 0x54100013" :: "r"(a0), "r"(a1) : "memory");
#elif defined(__loongarch__)
    register const void *a0 __asm__("$a0") = p;
    register long a1 __asm__("$a1") = n;
    /* andi $zero, $zero, 0x541: a nop */
    __asm__ volatile(".word 0x03550400" :: "r"(a0), "r"(a1) : "memory");
#else
#error unsupported arch
#endif
}

static int resident(const void *p)
{
    unsigned char v = 0;
    if (mincore((void *)((uintptr_t)p & ~(uintptr_t)(PG - 1)), PG, &v)) {
        return -1;
    }
    return v & 1;
}

static void dump(const char *tag, const void *p, size_t n)
{
    const unsigned char *b = p;
    printf("%s:", tag);
    for (size_t i = 0; i < n; i++) {
        printf("%02x", b[i]);
    }
    printf("\n");
    fflush(stdout);
}

static void hookfd(void)
{
    int fd = open("/dev/null", O_WRONLY);
    if (fd < 0 || dup2(fd, HOOK_FD) < 0) {
        perror("hookfd");
        exit(1);
    }
    close(fd);
}

static void *map_file(const char *path, size_t len, off_t off)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(path);
        exit(1);
    }
    void *p = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, off);
    if (p == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    close(fd);
    return p;
}

/* Never-touched page of a file mapping, read from a user-mode hook. */
static int t_user_file(const char *path)
{
    char *p = map_file(path, 4 * FPG, 0);
    char *q = p + 2 * FPG + 100;
    printf("PRE resident=%d addr=%p\n", resident(q), q);
    fflush(stdout);
    marker(q, 64);
    printf("POST resident=%d\n", resident(q));
    dump("TRUTH", q, 64);
    return 0;
}

/* Same, but read from the kernel's ksys_write() entry. */
static int t_kern_file(const char *path)
{
    char *p = map_file(path, 4 * FPG, 0);
    char *q = p + 3 * FPG + 200;
    hookfd();
    printf("PRE resident=%d addr=%p\n", resident(q), q);
    fflush(stdout);
    ssize_t r = write(HOOK_FD, q, 64);
    printf("POST write=%zd resident=%d\n", r, resident(q));
    dump("TRUTH", q, 64);
    return 0;
}

static void *unmapped_addr(void)
{
    void *p = mmap(NULL, PG, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    munmap(p, PG);
    return p;
}

/* No VMA at all: a forced fault from user mode is a SIGSEGV. */
static int t_user_novma(void)
{
    char *q = unmapped_addr();
    printf("PRE addr=%p\n", q);
    fflush(stdout);
    marker(q, 64);
    printf("SURVIVED\n");
    return 0;
}

/* No VMA, from the kernel hook: a forced fault there is an oops. */
static int t_kern_novma(void)
{
    char *q = unmapped_addr();
    hookfd();
    printf("PRE addr=%p\n", q);
    fflush(stdout);
    ssize_t r = write(HOOK_FD, q, 64);
    printf("SURVIVED write=%zd errno=%d\n", r, r < 0 ? errno : 0);
    return 0;
}

/* Mapping past EOF of a one-page file: a forced fault is a SIGBUS. */
static int t_user_eof(const char *path)
{
    char *p = map_file(path, 4 * PG, 0);
    char *q = p + 2 * PG;   /* path is < 2 pages long */
    printf("PRE addr=%p\n", q);
    fflush(stdout);
    marker(q, 64);
    printf("SURVIVED\n");
    return 0;
}

static int t_kern_eof(const char *path)
{
    char *p = map_file(path, 4 * PG, 0);
    char *q = p + 2 * PG;
    hookfd();
    printf("PRE addr=%p\n", q);
    fflush(stdout);
    ssize_t r = write(HOOK_FD, q, 64);
    printf("SURVIVED write=%zd errno=%d\n", r, r < 0 ? errno : 0);
    return 0;
}

/* PROT_NONE: VMA exists but is not readable. */
static int t_user_protnone(void)
{
    char *q = mmap(NULL, PG, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    printf("PRE addr=%p\n", q);
    fflush(stdout);
    marker(q, 64);
    printf("SURVIVED\n");
    return 0;
}

/*
 * Kernel hook while a sibling thread hammers the mmap lock for write.  A
 * kernel-mode fault from a PC with no exception-table entry oopses when
 * mmap_read_trylock() fails.
 */
static volatile int stop;
static void *hammer(void *arg)
{
    (void)arg;
    while (!stop) {
        void *p = mmap(NULL, 64 * FPG, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        munmap(p, 64 * FPG);
    }
    return NULL;
}

static int t_kern_contend(const char *path, int iters, int nham)
{
    pthread_t th[8];
    nham = nham < 1 ? 1 : nham > 8 ? 8 : nham;
    hookfd();
    for (int i = 0; i < nham; i++) {
        pthread_create(&th[i], NULL, hammer, NULL);
    }
    int ok = 0;
    for (int i = 0; i < iters; i++) {
        char *p = map_file(path, 4 * FPG, 0);
        char *q = p + (i % 4) * FPG + 8;
        if (write(HOOK_FD, q, 32) == 32) {
            ok++;
        }
        munmap(p, 4 * FPG);
        /* evict so the next iteration really faults */
        posix_fadvise(open(path, O_RDONLY), 0, 0, POSIX_FADV_DONTNEED);
    }
    stop = 1;
    for (int i = 0; i < nham; i++) {
        pthread_join(th[i], NULL);
    }
    printf("CONTEND ok=%d/%d\n", ok, iters);
    return 0;
}

static long vmstat(const char *key)
{
    char k[64];
    long v;
    FILE *f = fopen("/proc/vmstat", "r");
    while (f && fscanf(f, "%63s %ld", k, &v) == 2) {
        if (!strcmp(k, key)) {
            fclose(f);
            return v;
        }
    }
    if (f) {
        fclose(f);
    }
    return -1;
}

static int wfile(const char *path, const char *v)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        return -1;
    }
    ssize_t r = write(fd, v, strlen(v));
    close(fd);
    return r < 0 ? -1 : 0;
}

static long rfile_long(const char *path)
{
    char b[64] = "";
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    ssize_t r = read(fd, b, sizeof(b) - 1);
    close(fd);
    return r > 0 ? atol(b) : -1;
}

/*
 * Push q's page out to swap.  MADV_PAGEOUT plus memory.reclaim (5.19+);
 * otherwise squeeze this task's cgroup v2 memory.max below its usage.
 */
static void swap_out(unsigned char *p, size_t n, unsigned char *q)
{
    for (int i = 0; i < 20 && resident(q); i++) {
        if (madvise(p, n, MADV_PAGEOUT)) {
            break;          /* < 5.4 */
        }
        usleep(100000);
    }
    /* drop the clean swap-cache copy so the read must come from the device */
    for (int i = 0; i < 8 && resident(q); i++) {
        int fd = open("/sys/fs/cgroup/memory.reclaim", O_WRONLY);
        if (fd < 0) {
            break;
        }
        ssize_t w = write(fd, "4M", 2);
        printf("reclaim %d -> %zd resident=%d\n", i, w, resident(q));
        close(fd);
    }
    if (resident(q)) {
        char b[32];
        for (int i = 0; i < 8 && resident(q); i++) {
            long cur = rfile_long("/sys/fs/cgroup/pt/memory.current");
            long max = cur > 2 * (long)n ? cur - (long)n : cur / 2;
            snprintf(b, sizeof(b), "%ld", max < 16384 ? 16384 : max);
            int r = wfile("/sys/fs/cgroup/pt/memory.max", b);
            printf("squeeze %d cur=%ld max=%s -> %d resident=%d\n", i, cur,
                   b, r, resident(q));
        }
        wfile("/sys/fs/cgroup/pt/memory.max", "max");
    }
}

/*
 * Join a fresh cgroup before allocating: cgroup v2 never moves existing
 * charges, so swap_out() can only squeeze pages charged here.
 */
static void cg_enter(void)
{
    char b[32];
    mkdir("/sys/fs/cgroup/pt", 0755);
    wfile("/sys/fs/cgroup/cgroup.subtree_control", "+memory");
    snprintf(b, sizeof(b), "%d", getpid());
    wfile("/sys/fs/cgroup/pt/cgroup.procs", b);
    /* a previous test may have died before lifting its squeeze */
    wfile("/sys/fs/cgroup/pt/memory.max", "max");
}

/* Anonymous page pushed out to swap, then read (or written). */
static int t_swap(int kern, int wr)
{
    size_t n = 64 * FPG;
    cg_enter();
    unsigned char *p = mmap(NULL, n, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)(i * 7 + (i >> 12));
    }
    unsigned char *q = p + 17 * FPG + 300;
    swap_out(p, n, q);
    long in0 = vmstat("pswpin");
    printf("PRE resident=%d addr=%p\n", resident(q), q);
    fflush(stdout);
    if (kern) {
        hookfd();
        write(HOOK_FD, q, 64);
    } else {
        marker(q, 64);
    }
    long in1 = vmstat("pswpin");
    printf("POST resident=%d pswpin_delta=%ld\n", resident(q), in1 - in0);
    dump("TRUTH", q, 64);
    if (wr) {
        unsigned char orig[64];
        for (int i = 0; i < 64; i++) {
            size_t o = (q - p) + i;
            orig[i] = (unsigned char)(o * 7 + (o >> 12));
        }
        dump("ORIG", orig, 64);
    }
    return 0;
}

/*
 * Write tests.  The plugin writes a fixed pattern at the hook; the guest
 * prints what its mapping holds afterwards (TRUTH), what it held before
 * (ORIG) and what another view of the same memory holds (OTHER): the file
 * through read(2), or a fresh zero-filled anonymous page.  A correct
 * write changes TRUTH and never OTHER.
 */
static void hook_at(int kern, void *q, long n)
{
    if (kern) {
        hookfd();
        ssize_t r = write(HOOK_FD, q, n);
        printf("POST write=%zd errno=%d\n", r, r < 0 ? errno : 0);
    } else {
        marker(q, n);
    }
}

static void pread_dump(const char *tag, const char *path, off_t off)
{
    unsigned char b[64];
    int fd = open(path, O_RDONLY);
    if (fd < 0 || pread(fd, b, 64, off) != 64) {
        perror("pread");
        return;
    }
    close(fd);
    dump(tag, b, 64);
}

/* Never-touched page of a private writable file mapping: copy-on-write. */
static int t_wfile(const char *path, int kern)
{
    int fd = open(path, O_RDONLY);
    char *p = mmap(NULL, 4 * FPG, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);
    char *q = p + FPG + 500;
    pread_dump("ORIG", path, FPG + 500);
    printf("PRE resident=%d addr=%p\n", resident(q), q);
    fflush(stdout);
    hook_at(kern, q, 64);
    printf("SURVIVED resident=%d\n", resident(q));
    dump("TRUTH", q, 64);
    pread_dump("OTHER", path, FPG + 500);
    return 0;
}

/* Anonymous page that was only read, so it maps the shared zero page. */
static int t_wzero(int kern)
{
    unsigned char *p = mmap(NULL, 4 * FPG, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    unsigned char *q = p + 2 * FPG + 64;
    unsigned char z[64] = { 0 };
    (void)*(volatile unsigned char *)q;
    dump("ORIG", z, 64);
    printf("PRE resident=%d addr=%p\n", resident(q), q);
    fflush(stdout);
    hook_at(kern, q, 64);
    printf("SURVIVED resident=%d\n", resident(q));
    dump("TRUTH", q, 64);
    /* a fresh read-only-touched anonymous page: the zero page again */
    unsigned char *o = mmap(NULL, 4 * FPG, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    dump("OTHER", o + 2 * FPG + 64, 64);
    return 0;
}

/* Read-only file mapping, already resident: the write must be refused. */
static int t_wro(const char *path, int kern)
{
    int fd = open(path, O_RDONLY);
    char *p = mmap(NULL, 4 * FPG, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    char *q = p + FPG + 700;
    (void)*(volatile char *)q;
    pread_dump("ORIG", path, FPG + 700);
    printf("PRE resident=%d addr=%p\n", resident(q), q);
    fflush(stdout);
    hook_at(kern, q, 64);
    printf("SURVIVED resident=%d\n", resident(q));
    dump("TRUTH", q, 64);
    pread_dump("OTHER", path, FPG + 700);
    return 0;
}

/*
 * Accesses spanning pages, several accesses per hook, and hooks on two
 * vCPUs at once.  Each checked range is reported on one line:
 *   AT addr=<a> len=<n> TRUTH:<hex, first 256 bytes> TRUTHFNV:<fnv1a>
 *      [ORIG:.. ORIGFNV:..] [OTHER:.. OTHERFNV:..]
 * span.bin is 64 blocks of 4 KiB, every 16 bytes naming block and offset.
 */
static uint64_t fnv1a(const unsigned char *p, size_t n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 0x100000001b3ull;
    }
    return h;
}

static void hexfnv(const char *tag, const void *p, size_t n)
{
    const unsigned char *b = p;
    printf(" %s:", tag);
    for (size_t i = 0; i < n && i < 256; i++) {
        printf("%02x", b[i]);
    }
    printf(" %sFNV:%016llx", tag, (unsigned long long)fnv1a(b, n));
}

static void at(const void *q, size_t n, const void *orig, const void *other)
{
    printf("AT addr=%p len=%zu", q, n);
    hexfnv("TRUTH", q, n);
    if (orig) {
        hexfnv("ORIG", orig, n);
    }
    if (other) {
        hexfnv("OTHER", other, n);
    }
    printf("\n");
    fflush(stdout);
}

static int resident_pages(const void *q, size_t n)
{
    int c = 0;
    for (uintptr_t a = (uintptr_t)q & ~(uintptr_t)(PG - 1);
         a < (uintptr_t)q + n; a += PG) {
        c += resident((void *)a) == 1;
    }
    return c;
}

static void *pread_buf(const char *path, off_t off, size_t n)
{
    unsigned char *b = malloc(n);
    int fd = open(path, O_RDONLY);
    if (fd < 0 || pread(fd, b, n, off) != (ssize_t)n) {
        perror("pread");
        exit(1);
    }
    close(fd);
    return b;
}

/* [q, q+len) covers npages never-touched pages, starting 32 bytes short of
 * the first boundary */
static size_t span_len(int npages)
{
    return npages < 2 ? 64 : (size_t)(npages - 2) * PG + 64;
}

/*
 * Read across npages swapped-out anonymous pages: swap-in maps one page per
 * fault (file pages would come in together, by readahead and fault-around).
 */
static int t_span(int kern, int npages)
{
    size_t n = span_len(npages), sz = 64 * FPG;
    cg_enter();
    unsigned char *p = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for (size_t i = 0; i < sz; i++) {
        p[i] = (unsigned char)(i * 7 + (i >> 12));
    }
    unsigned char *q = p + (sz / 2 / PG) * PG - 32;
    swap_out(p, sz, q);
    for (int i = 0; i < 8 && resident_pages(q, n); i++) {
        swap_out(p, sz, q + n - 1);
    }
    printf("PRE resident=%d/%d addr=%p len=%zu\n", resident_pages(q, n),
           npages, q, n);
    fflush(stdout);
    hook_at(kern, q, n);
    printf("SURVIVED resident=%d/%d\n", resident_pages(q, n), npages);
    at(q, n, NULL, NULL);
    return 0;
}

/*
 * Write across npages never-touched pages of a private file mapping (each
 * one copy-on-write).  With ro_last the last page is read-only: the guest
 * refuses that page, so nothing may be written to the others either.
 */
static int t_wspan(int kern, int npages, int ro_last, const char *path)
{
    int fd = open(path, O_RDONLY);
    char *p = mmap(NULL, (npages + 1) * PG, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE, fd, 0);
    close(fd);
    char *q = p + PG - 32;
    size_t n = span_len(npages);
    if (ro_last) {
        mprotect(p + (npages - 1) * PG, PG, PROT_READ);
    }
    void *orig = pread_buf(path, PG - 32, n);
    printf("PRE resident=%d/%d addr=%p len=%zu\n", resident_pages(q, n),
           npages, q, n);
    fflush(stdout);
    hook_at(kern, q, n);
    printf("SURVIVED resident=%d/%d\n", resident_pages(q, n), npages);
    at(q, n, orig, pread_buf(path, PG - 32, n));
    return 0;
}

/*
 * Several dependent accesses in one hook (plugin op=multi).  The hook gets
 * the address of a descriptor on a swapped-out page, reads it, reads the
 * range it points to (a never-touched file page), then writes w1 (a page
 * backed by the shared zero page) and w2 (a never-touched private file
 * page, copy-on-write): four faults in one hook.
 *   variant 1: ptr is a PROT_NONE page (the guest refuses the second read)
 *   variant 2: w2 is a read-only mapping (the guest refuses that write)
 */
struct desc {
    uint64_t ptr, len, w1, w2;
};

static int t_multi(int kern, int variant, const char *path)
{
    size_t n = 64 * FPG;
    cg_enter();
    unsigned char *a = mmap(NULL, n, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for (size_t i = 0; i < n; i++) {
        a[i] = (unsigned char)(i * 7 + (i >> 12));
    }
    struct desc *d = (struct desc *)(a + 17 * FPG + 256);
    char *s = map_file(path, 4 * PG, 0);
    char *ptr = s + 2 * PG + 40;
    if (variant == 1) {
        ptr = mmap(NULL, PG, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    unsigned char *z = mmap(NULL, 4 * FPG, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    unsigned char *w1 = z + 2 * FPG + 64;
    (void)*(volatile unsigned char *)w1;
    int fd = open(path, O_RDONLY);
    char *f = mmap(NULL, 4 * PG, PROT_READ | (variant == 2 ? 0 : PROT_WRITE),
                   MAP_PRIVATE, fd, 0);
    close(fd);
    char *w2 = f + PG + 500;
    if (variant == 2) {
        (void)*(volatile char *)w2;
    }
    *d = (struct desc) { (uintptr_t)ptr, 64, (uintptr_t)w1, (uintptr_t)w2 };
    struct desc copy = *d;
    unsigned char zero[64] = { 0 };
    void *orig2 = pread_buf(path, PG + 500, 64);

    swap_out(a, n, (unsigned char *)d);
    printf("PRE resident desc=%d ptr=%d w2=%d addr=%p\n", resident(d),
           variant == 1 ? 0 : resident(ptr), resident(w2), (void *)d);
    fflush(stdout);
    hook_at(kern, d, sizeof(*d));
    printf("SURVIVED\n");
    at(d, sizeof(*d), &copy, NULL);
    if (variant != 1) {
        at(ptr, 64, NULL, NULL);
    } else {
        printf("NOACCESS addr=%p\n", (void *)ptr);    /* must be refused */
    }
    unsigned char *o = mmap(NULL, 4 * FPG, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    at(w1, 64, zero, o + 2 * FPG + 64);
    at(w2, 64, orig2, pread_buf(path, PG + 500, 64));
    return 0;
}

/*
 * Two threads, one per CPU, hooking at the same time: every third access
 * of each is to its own PROT_NONE page (the guest refuses it), the others
 * to never-touched file pages.  Each fault outstanding on one vCPU must be
 * accounted to its own task.
 */
struct par {
    int t, kern, iters, done;
    const char *path;
    char *bad;
};
static pthread_barrier_t par_bar;

static void *par_thread(void *arg)
{
    struct par *a = arg;
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(a->t, &cs);
    sched_setaffinity(0, sizeof(cs), &cs);
    for (int i = 0; i < a->iters; i++) {
        char *p = map_file(a->path, 64 * FPG, 0);
        char *q = i % 3 == 2 ? a->bad
                             : p + ((2 * i + a->t) % 64) * FPG + 16 * a->t + 8;
        pthread_barrier_wait(&par_bar);
        if (a->kern) {
            write(HOOK_FD, q, 48);
        } else {
            marker(q, 48);
        }
        munmap(p, 64 * FPG);
        pthread_barrier_wait(&par_bar);
        if (a->t == 0) {
            int fd = open(a->path, O_RDONLY);
            posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
            close(fd);
        }
        a->done++;
    }
    return NULL;
}

static int t_par(int kern, int iters, const char *path)
{
    struct par a[2];
    pthread_t th[2];
    if (kern) {
        hookfd();
    }
    pthread_barrier_init(&par_bar, NULL, 2);
    for (int t = 0; t < 2; t++) {
        a[t] = (struct par) { t, kern, iters, 0, path,
                              mmap(NULL, PG, PROT_NONE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) };
        printf("PAR bad t=%d addr=%p\n", t, a[t].bad);
    }
    printf("PAR cpus=%ld\n", sysconf(_SC_NPROCESSORS_ONLN));
    fflush(stdout);
    for (int t = 0; t < 2; t++) {
        pthread_create(&th[t], NULL, par_thread, &a[t]);
    }
    for (int t = 0; t < 2; t++) {
        pthread_join(th[t], NULL);
    }
    printf("PAR done t0=%d t1=%d of %d\n", a[0].done, a[1].done, iters);
    return 0;
}

/*
 * Cost of one hook on a resident page: n markers (user) or n write()s to
 * the hook fd (kernel), against n write()s to another fd (kernel hook
 * breakpoint taken, nothing read).
 */
static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

static int t_bench(int kern, long n)
{
    static char buf[64] = "resident";
    int other = open("/dev/null", O_WRONLY);
    double t0, t1, t2;

    hookfd();
    t0 = now_ns();
    for (long i = 0; i < n; i++) {
        if (kern) {
            write(HOOK_FD, buf, 8);
        } else {
            marker(buf, 8);
        }
    }
    t1 = now_ns();
    for (long i = 0; i < n; i++) {
        write(other, buf, 8);
    }
    t2 = now_ns();
    printf("BENCH %s n=%ld hooked_ns=%.0f plain_write_ns=%.0f\n",
           kern ? "kern" : "user", n, (t1 - t0) / n, (t2 - t1) / n);
    return 0;
}

#if defined(__x86_64__)
#include <sys/io.h>
/* Floor for any hook that exits to QEMU: n port writes QEMU ignores */
static int t_io_bench(long n)
{
    double t0;

    if (iopl(3)) {
        perror("iopl");
        return 1;
    }
    t0 = now_ns();
    for (long i = 0; i < n; i++) {
        outb(0, 0x80);
    }
    printf("BENCH io n=%ld exit_ns=%.0f\n", n, (now_ns() - t0) / n);
    return 0;
}
#endif


/*
 * Address hooks (plugin/hooktest.c hooks these by symbol address).  Called
 * through volatile pointers so the compiler keeps every call.
 */
__attribute__((noinline)) long hk_target(long x) { return x * 3 + 1; }
__attribute__((noinline)) long hk_target2(long x) { return x * 5 + 2; }
__attribute__((noinline)) void hk_phase(long n)
{
    __asm__ volatile("" :: "r"(n) : "memory");
}
__attribute__((noinline)) long hk_regs(long a) { return a + 1; }
__attribute__((noinline)) long hk_redirect(long a) { return a ^ a ^ 1; }
__attribute__((noinline)) long hk_other(long a) { return a ^ a ^ 2; }
__attribute__((noinline)) void hk_fault(const void *p)
{
    __asm__ volatile("" :: "r"(p) : "memory");
}
static long (*volatile p_target)(long) = hk_target;
static long (*volatile p_target2)(long) = hk_target2;
static void (*volatile p_phase)(long) = hk_phase;
static long (*volatile p_regs)(long) = hk_regs;
static long (*volatile p_redirect)(long) = hk_redirect;
static void (*volatile p_fault)(const void *) = hk_fault;

static void calls(long (*f)(long), long k, long mul, long add)
{
    for (long i = 0; i < k; i++) {
        if (f(i) != i * mul + add) {
            printf("HK bad result i=%ld\n", i);
        }
    }
}

/* Map the page holding @fn a second time, from the executable file. */
static void *alias_of(void *fn)
{
    uintptr_t a = (uintptr_t)fn, lo, hi, page = a & ~(uintptr_t)(PG - 1);
    unsigned long long off;
    char line[512], perm[8];
    FILE *m = fopen("/proc/self/maps", "r");
    void *r = NULL;

    while (m && fgets(line, sizeof(line), m)) {
        if (sscanf(line, "%lx-%lx %7s %llx", &lo, &hi, perm, &off) == 4 &&
            a >= lo && a < hi) {
            int fd = open("/proc/self/exe", O_RDONLY);
            char *q = mmap(NULL, PG, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd,
                           off + (page - lo));
            close(fd);
            if (q != MAP_FAILED) {
                r = q + (a - page);
            }
            break;
        }
    }
    if (m) {
        fclose(m);
    }
    return r;
}

static int t_hk_count(long k)
{
    p_phase(0);
    calls(p_target, k, 3, 1);
    calls(p_target2, 2 * k, 5, 2);
    p_phase(1);
    printf("HK expect window=0 target=%ld target2=%ld\n", k, 2 * k);
    printf("HK regs got=%ld want=1006\n", p_regs(5));
    printf("HK redir got=%ld want=2\n", p_redirect(7));

    int fd = open("/mnt/span.bin", O_RDONLY);
    char *base = mmap(NULL, 64 * FPG, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (base == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    const char *q = base + 5 * FPG + 0x40;
    int res = resident_pages(q, 1);
    p_fault(q);
    printf("HK fault resident_before=%d want=%.32s\n", res, q);
    return 0;
}

static int t_hk_alias(long k)
{
    long (*at)(long) = alias_of((void *)hk_target);
    long (*at2)(long) = alias_of((void *)hk_target2);

    if (!at || !at2) {
        printf("HK alias mapping failed\n");
        return 1;
    }
    printf("HK alias target=%p at=%p target2=%p at2=%p\n", (void *)hk_target,
           (void *)at, (void *)hk_target2, (void *)at2);
    p_phase(0);
    calls(at, k, 3, 1);         /* alias first: translated there */
    p_phase(1);
    calls(p_target, k, 3, 1);   /* hooked address */
    p_phase(2);
    calls(p_target2, k, 5, 2);  /* hooked address first */
    p_phase(3);
    calls(at2, k, 5, 2);        /* then its alias */
    p_phase(4);
    printf("HK expect window=0 target=0 target2=0\n");
    printf("HK expect window=1 target=%ld target2=0\n", k);
    printf("HK expect window=2 target=0 target2=%ld\n", k);
    printf("HK expect window=3 target=0 target2=0\n");
    return 0;
}

static int t_hk_dyn(long k)
{
    for (long w = 0; w < 4; w++) {
        p_phase(w);
        calls(p_target, k, 3, 1);
    }
    p_phase(4);
    printf("HK expect window=0 target=0 target2=0\n");
    printf("HK expect window=1 target=%ld target2=0\n", k);
    printf("HK expect window=2 target=0 target2=0\n");
    printf("HK expect window=3 target=%ld target2=0\n", k);
    return 0;
}

/* A kernel-address hook (the plugin's ktarget=, getppid's handler). */
static int t_hk_kern(long k)
{
    p_phase(0);
    for (long i = 0; i < k; i++) {
        syscall(SYS_getppid);
    }
    p_phase(1);
    printf("HK expect window=0 ktarget=%ld\n", k);
    return 0;
}

/* Two processes, one per CPU, hitting the same hooks at once. */
static int t_hk_smp(long k)
{
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    pid_t pid[2];

    printf("HK smp cpus=%ld\n", ncpu);
    p_phase(0);
    for (int c = 0; c < 2; c++) {
        pid[c] = fork();
        if (pid[c] == 0) {
            cpu_set_t set;
            CPU_ZERO(&set);
            CPU_SET(c % (ncpu > 0 ? ncpu : 1), &set);
            sched_setaffinity(0, sizeof(set), &set);
            calls(p_target, k, 3, 1);
            calls(p_target2, k, 5, 2);
            _exit(0);
        }
    }
    for (int c = 0; c < 2; c++) {
        waitpid(pid[c], NULL, 0);
    }
    p_phase(1);
    printf("HK expect window=0 target=%ld target2=%ld\n", 2 * k, 2 * k);
    return 0;
}

/*
 * Syscall hooks (plugin/syscalls.c with guest/pk/sc-rules.txt): a fixed
 * sequence whose expected events are printed as "SC expect" lines.
 */
static int t_sc_func(void)
{
    char path[] = "/data.sqsh";
    int fd, fd2, p[2], st;
    long r;
    pid_t pid = getpid(), child;

    printf("SC expect self pid=%d\n", pid);

    /* openat: the path resident at entry, then in an untouched file mapping */
    fd = openat(AT_FDCWD, path, O_RDONLY);
    printf("SC expect open late=0 fd=%d\n", fd);
    int pf = open("/sc_path.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (write(pf, path, sizeof(path)) != sizeof(path)) {
        printf("SC write failed\n");
    }
    char *mp = mmap(NULL, PG, PROT_READ, MAP_PRIVATE, pf, 0);
    close(pf);
    fd2 = openat(AT_FDCWD, mp, O_RDONLY);
    printf("SC expect open late=1 fd=%d\n", fd2);
    close(fd2);

    /* ioctl: 0x1234 three times (arg 0xf5), 0x5678 twice (arg 0x05), arg -1 once */
    for (int i = 0; i < 3; i++) {
        syscall(SYS_ioctl, fd, 0x1234, 0xf5);
    }
    for (int i = 0; i < 2; i++) {
        syscall(SYS_ioctl, fd, 0x5678, 0x05);
    }
    syscall(SYS_ioctl, fd, 0x5678, -1L);
    printf("SC expect ioctl eq=3 range=3 mask=3 neg=1\n");

    /* a read that blocks until a child writes (pipe2: mips pipe() fails here) */
    if (syscall(SYS_pipe2, p, 0) != 0) {
        printf("SC expect blockread ret=pipe-failed pid=%d\n", pid);
    } else {
        child = fork();
        if (child == 0) {
            usleep(200000);
            if (write(p[1], "hello", 5) != 5) {
                _exit(1);
            }
            _exit(0);
        }
        char buf[777];
        r = read(p[0], buf, 777);
        waitpid(child, &st, 0);
        printf("SC expect blockread ret=%ld pid=%d\n", r, pid);
        close(p[0]);
        close(p[1]);
    }

    /* fork: the child's first return, and the parent's */
    child = fork();
    if (child == 0) {
        _exit(0);
    }
    waitpid(child, &st, 0);
    printf("SC expect fork child=%d parent=%d\n", child, pid);

    /* actions */
    r = syscall(SYS_getpgid, 4242);
    printf("SC expect override got=%ld want=777\n", r);
    r = syscall(SYS_getpgid, 0);
    printf("SC expect notoverridden got=%ld want=%d\n", r, getpgid(0));
    r = syscall(SYS_dup3, fd, 100, 0);
    printf("SC expect setarg got=%ld want=101\n", r);
    r = syscall(SYS_getpgid, 4343);
    printf("SC expect setret got=%ld want=55\n", r);

    /* a count limited to this process */
    for (int i = 0; i < 10; i++) {
        syscall(SYS_getpid);
    }
    printf("SC expect getpid n=11\n");       /* and the getpid() above */

    /* return filters: one failing getsid, one succeeding */
    syscall(SYS_getsid, 4444);
    syscall(SYS_getsid, 0);
    printf("SC expect getsid err=1 ok=1\n");

    /* exit_group never returns */
    child = fork();
    if (child == 0) {
        syscall(SYS_exit_group, 0);
    }
    waitpid(child, &st, 0);
    printf("SC expect exit child=%d\n", child);
    close(fd);
    return 0;
}

/*
 * Syscall events at the trap and the return to user mode (plugin/syscalls.c
 * mode=trap, guest/pk/sc-trap-rules.txt): the cases where a call's return
 * is not simply the next return to user mode.
 */
static volatile int sc_sigs;

static void sc_on_alarm(int sig)
{
    /* syscalls inside the handler, while the interrupted read is pending */
    syscall(SYS_getpgid, 5151);
    syscall(SYS_getpgid, 5151);
    sc_sigs++;
}

static void sc_alarm(int restart, long ms)
{
    struct sigaction sa = { .sa_handler = sc_on_alarm };
    struct itimerval it = { .it_value = { .tv_sec = ms / 1000,
                                          .tv_usec = ms % 1000 * 1000 } };

    sa.sa_flags = restart ? SA_RESTART : 0;
    sigaction(SIGALRM, &sa, NULL);
    setitimer(ITIMER_REAL, &it, NULL);
}

static int sc_thr_fd[2];
static long sc_thr_tid[2], sc_thr_ret[2];

static void *sc_thr(void *arg)
{
    long i = (long)arg;
    char buf[780];

    sc_thr_tid[i] = syscall(SYS_gettid);
    sc_thr_ret[i] = read(sc_thr_fd[i], buf, 780);
    return NULL;
}

static int t_sc_trap(void)
{
    pid_t pid = getpid(), child;
    int p[2], q[2], st;
    char buf[800];
    long r;

    printf("SC expect self pid=%d\n", pid);

    /* a blocked read interrupted by a signal, without SA_RESTART */
    if (syscall(SYS_pipe2, p, 0) == 0) {
        sc_alarm(0, 150);
        r = syscall(SYS_read, p[0], buf, 778);
        printf("SC expect eintr ret=%ld pid=%d sigs=%d\n", r < 0 ? -errno : r,
               pid, sc_sigs);
        printf("SC expect inhandler n=2 ret=-3\n");

        /* with SA_RESTART: the kernel restarts it; a child writes later */
        sc_alarm(1, 150);
        child = fork();
        if (child == 0) {
            usleep(400000);
            if (write(p[1], "hello", 5) != 5) {
                _exit(1);
            }
            _exit(0);
        }
        r = syscall(SYS_read, p[0], buf, 779);
        waitpid(child, &st, 0);
        printf("SC expect restart ret=%ld pid=%d sigs=%d\n", r, pid, sc_sigs);
        close(p[0]);
        close(p[1]);
    }

    /* vfork: the child runs on the parent's stack while the parent waits */
    child = vfork();
    if (child == 0) {
        syscall(SYS_getpgid, 5252);
        _exit(0);
    }
    waitpid(child, &st, 0);
    printf("SC expect vfork child=%d parent=%d\n", child, pid);

    /* execve: a failure returns; a success does not */
    {
        char *argv[] = { "true", NULL }, *envp[] = { NULL };
        r = syscall(SYS_execve, "/nonexistent", argv, envp);
        printf("SC expect exec fail=%ld\n", r < 0 ? -errno : r);
        child = fork();
        if (child == 0) {
            execve("/bin/busybox", argv, envp);
            _exit(1);
        }
        waitpid(child, &st, 0);
        printf("SC expect execok status=%d\n", WEXITSTATUS(st));
    }

    /* two threads blocked in read at once: each return goes to its thread */
    if (syscall(SYS_pipe2, p, 0) == 0 && syscall(SYS_pipe2, q, 0) == 0) {
        pthread_t t[2];
        sc_thr_fd[0] = p[0];
        sc_thr_fd[1] = q[0];
        for (long i = 0; i < 2; i++) {
            pthread_create(&t[i], NULL, sc_thr, (void *)i);
        }
        usleep(200000);
        if (write(q[1], "defg", 4) != 4 || write(p[1], "abc", 3) != 3) {
            printf("SC thr write failed\n");
        }
        for (int i = 0; i < 2; i++) {
            pthread_join(t[i], NULL);
        }
        printf("SC expect thr t0=%ld:%ld t1=%ld:%ld\n", sc_thr_tid[0],
               sc_thr_ret[0], sc_thr_tid[1], sc_thr_ret[1]);
    }
    return 0;
}

/* n successful execs: each leaves a call pending in trap mode (eviction) */
static int t_sc_execs(long n)
{
    char *argv[] = { "true", NULL }, *envp[] = { NULL };
    int st;

    for (long i = 0; i < n; i++) {
        pid_t c = fork();
        if (c == 0) {
            /* a different stack depth each time: distinct pending calls */
            volatile char *pad = __builtin_alloca(16 + (i % 4096) * 16);
            pad[0] = 0;
            execve("/bin/busybox", argv, envp);
            _exit(1);
        }
        waitpid(c, &st, 0);
    }
    printf("SC expect execs n=%ld\n", n);
    return 0;
}

/*
 * Penguin syscall hooks (system/penguin-syscalls.c, guest/pk/pyhost.py):
 * string filters, unregistering from a callback, the analysis scope, a
 * pid filter registered at run time, skip, and chained values.
 */
static int t_sc_pen(void)
{
    pid_t pid = getpid(), child;
    int st, fd;
    long r;

    printf("SC expect self pid=%d\n", pid);

    /* string filters: starts with "/proc/", contains "self", ends "status" */
    fd = syscall(SYS_openat, AT_FDCWD, "/proc/self/status", O_RDONLY);
    printf("SC expect strings fd=%d\n", fd);

    /* a hook that unregisters itself at its third event */
    for (int i = 0; i < 10; i++) {
        syscall(SYS_getppid);
    }
    printf("SC expect unregister n=3\n");

    /* scope: a child in a new UTS namespace, the parent in the initial one */
    child = fork();
    if (child == 0) {
        if (unshare(CLONE_NEWUTS) != 0) {
            _exit(2);
        }
        for (int i = 0; i < 5; i++) {
            syscall(SYS_sched_yield);
        }
        _exit(0);
    }
    waitpid(child, &st, 0);
    for (int i = 0; i < 3; i++) {
        syscall(SYS_sched_yield);
    }
    printf("SC expect scope in=5 out=3 status=%d\n", WEXITSTATUS(st));

    /* a pid filter, registered by the hook on getpgid(6363) */
    syscall(SYS_getpgid, 6363);
    child = fork();
    if (child == 0) {
        for (int i = 0; i < 2; i++) {
            syscall(SYS_getsid, 7171);
        }
        _exit(0);
    }
    waitpid(child, &st, 0);
    for (int i = 0; i < 4; i++) {
        syscall(SYS_getsid, 7171);
    }
    printf("SC expect pidfilter n=4\n");

    /* the first skip ends the entry; the return hooks see its value */
    r = syscall(SYS_getpgid, 6161);
    printf("SC expect skip got=%ld want=616\n", r);

    /* return values and arguments chain from hook to hook */
    r = syscall(SYS_getpgid, 6262);
    printf("SC expect retchain got=%ld want=2\n", r);
    r = syscall(SYS_dup3, fd, 200, 0);
    printf("SC expect argchain got=%ld want=202\n", r);
    return 0;
}

/* page @i of @path, mapped privately and not touched */
static char *sc_map_page(const char *path, int i)
{
    int fd = open(path, O_RDONLY);
    char *p = mmap(NULL, PG, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
                   (off_t)i * PG);
    close(fd);
    return p == MAP_FAILED ? NULL : p;
}

/*
 * Penguin hooks (pyhost.py "fault") on memory the task has not touched:
 * pages of a file mapped but not faulted in yet, each its own mapping (no
 * fault-around into the next), and an address with no mapping at all.
 */
static int t_sc_fault(void)
{
    static const char *text[] = {
        "/proc/self/status", "hello-from-page1", "cow-me-original",
        "page3-at-return", "page4-at-return",
    };
    const char *path = "/sc_fault.bin";
    char *buf = calloc(5, PG), *m[5], *bad, got[64] = "", file[64] = "";
    int fd, nul, p[2];
    long r;

    printf("SC expect self pid=%d\n", getpid());
    for (int i = 0; i < 5; i++) {
        strcpy(buf + i * PG, text[i]);
    }
    fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || write(fd, buf, 5 * PG) != 5 * PG) {
        printf("SC error: cannot write %s\n", path);
        return 1;
    }
    close(fd);
    for (int i = 0; i < 5; i++) {
        m[i] = sc_map_page(path, i);
    }
    bad = mmap(NULL, PG, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    munmap(bad, PG);

    /* a string filter at the entry; its hook skips the call */
    r = syscall(SYS_openat, AT_FDCWD, m[0], O_RDONLY);
    printf("SC expect fstr got=%ld want=7777\n", r);
    /* the same filter on an address with no mapping: no match, no signal */
    errno = 0;
    r = syscall(SYS_openat, AT_FDCWD, bad, O_RDONLY);
    printf("SC expect fbad got=%ld/%d want=-1/%d\n", r, errno, EFAULT);
    /* a callback reads the buffer, at the entry */
    nul = open("/dev/null", O_WRONLY);
    r = write(nul, m[1], 1717);
    printf("SC expect fread got=%ld want=1717\n", r);
    /* a callback writes it: a private copy, the file keeps its bytes */
    if (pipe(p) == 0) {
        r = write(p[1], m[2], 1818);
        if (read(p[0], got, sizeof(got) - 1) < 0) {
            got[0] = 0;
        }
        got[sizeof(got) - 1] = 0;
    }
    fd = open(path, O_RDONLY);
    if (pread(fd, file, sizeof(file) - 1, 2 * PG) < 0) {
        file[0] = 0;
    }
    close(fd);
    printf("SC expect fwrite pipe=%s mem=%s file=%s\n", got, m[2], file);
    /* a return hook reads two pages; then one with no mapping */
    syscall(SYS_setpgid, (long)m[3], (long)m[4]);
    syscall(SYS_setpgid, (long)bad, (long)m[3]);
    printf("SC expect fret n=2\n");
    unlink(path);
    free(buf);
    return 0;
}

/*
 * A FUSE file whose pages never arrive: the server answers what it takes
 * to open /fu/f (one page) and never a READ, so a fault on its mapping
 * waits, killably, for good.  Returns the server's pid.
 */
static pid_t sc_fuse_hang(void)
{
    char opts[128];
    int fd = open("/dev/fuse", O_RDWR);
    pid_t pid;

    mkdir("/fu", 0755);
    snprintf(opts, sizeof(opts), "fd=%d,rootmode=40000,user_id=0,group_id=0", fd);
    if (fd < 0 || mount("sc", "/fu", "fuse", 0, opts)) {
        return -1;
    }
    pid = fork();
    if (pid) {
        close(fd);
        return pid;
    }
    for (;;) {
        static char in[1 << 17];
        struct fuse_in_header *h = (void *)in;
        struct { struct fuse_out_header h; char b[256]; } o;
        struct fuse_attr *a = NULL;
        size_t n = 0;
        ssize_t r = read(fd, in, sizeof(in));

        if (r < (ssize_t)sizeof(*h)) {
            if (r < 0 && errno == EINTR) {
                continue;
            }
            _exit(0);
        }
        memset(&o, 0, sizeof(o));
        o.h.unique = h->unique;
        switch (h->opcode) {
        case FUSE_INIT: {
            struct fuse_init_out *io = (void *)o.b;
            io->major = FUSE_KERNEL_VERSION;
            io->minor = FUSE_KERNEL_MINOR_VERSION;
            io->max_write = 4096;
            /* readahead in the background: the fault waits on the page
             * lock, killably (a synchronous READ the server has taken is
             * waited for whatever happens) */
            struct fuse_init_in *ii = (void *)(in + sizeof(*h));
            io->flags = FUSE_ASYNC_READ & ii->flags;
            io->max_readahead = ii->max_readahead;  /* 0: read synchronously */
            n = sizeof(*io);
            break;
        }
        case FUSE_LOOKUP: {
            struct fuse_entry_out *e = (void *)o.b;
            if (strcmp(in + sizeof(*h), "f")) {
                o.h.error = -ENOENT;
                break;
            }
            e->nodeid = 2;
            a = &e->attr;
            n = sizeof(*e);
            break;
        }
        case FUSE_GETATTR: {
            struct fuse_attr_out *ao = (void *)o.b;
            a = &ao->attr;
            n = sizeof(*ao);
            break;
        }
        case FUSE_OPEN:
            n = sizeof(struct fuse_open_out);
            break;
        case FUSE_READ: case FUSE_FORGET: case FUSE_BATCH_FORGET:
        case FUSE_INTERRUPT:
            continue;           /* no answer */
        case FUSE_FLUSH: case FUSE_RELEASE: case FUSE_RELEASEDIR:
            break;
        default:
            o.h.error = -ENOSYS;
        }
        if (a) {
            a->ino = h->nodeid == 1 && h->opcode == FUSE_GETATTR ? 1 : 2;
            a->mode = a->ino == 1 ? 040755 : 0100644;
            a->size = a->ino == 1 ? 0 : 4096;
            a->nlink = 1;
            a->blksize = 4096;
        }
        o.h.len = sizeof(o.h) + (o.h.error ? 0 : n);
        if (write(fd, &o, o.h.len) < 0 && errno == ENODEV) {
            _exit(0);
        }
    }
}

/* waitpid, saying where the child is if it does not go */
static void sc_reap_wait(pid_t pid, int *st)
{
    for (int i = 0; i < 40; i++) {
        if (waitpid(pid, st, WNOHANG) == pid) {
            return;
        }
        usleep(50000);
    }
    for (const char *f = "wchan\0stat\0stack\0"; *f; f += strlen(f) + 1) {
        char path[64], buf[2048];
        int fd, n;
        snprintf(path, sizeof(path), "/proc/%d/%s", pid, f);
        fd = open(path, O_RDONLY);
        n = fd < 0 ? 0 : read(fd, buf, sizeof(buf) - 1);
        buf[n > 0 ? n : 0] = 0;
        printf("SC stuck pid=%d %s: %s\n", pid, f, buf);
        if (fd >= 0) {
            close(fd);
        }
    }
    fflush(stdout);
    waitpid(pid, st, 0);
}

/*
 * Penguin hooks (pyhost.py "reap") on tasks killed with a call in the
 * engine: one blocked in the fault its callback needs (a FUSE page that
 * never arrives), one waiting for its return (a pipe nobody writes).  The
 * reaper (release_task) drops both calls; getpgid(9191/9192) are markers
 * at which the host logs its counts.
 */
static int t_sc_reap(void)
{
    pid_t srv, c1, c2;
    int p[2], st1 = 0, st2 = 0;

    printf("SC expect self pid=%d\n", getpid());
    srv = sc_fuse_hang();
    if (srv < 0 || pipe(p)) {
        printf("SC error: fuse %d or pipe: %s\n", srv, strerror(errno));
        return 1;
    }
    c1 = fork();
    if (c1 == 0) {
        int fd = open("/fu/f", O_RDONLY), nul = open("/dev/null", O_WRONLY);
        char *m = fd < 0 ? MAP_FAILED : mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, fd, 0);

        if (m == MAP_FAILED) {
            _exit(3);
        }
        /* the callback reads m: the fault waits for the server forever */
        if (write(nul, m, 1919) < 0) {
            _exit(4);
        }
        _exit(5);
    }
    c2 = fork();
    if (c2 == 0) {
        static char b[1920];
        close(p[1]);
        if (read(p[0], b, 1920) < 0) {
            _exit(6);
        }
        _exit(7);
    }
    usleep(300000);
    syscall(SYS_getpgid, 9191);
    kill(c1, SIGKILL);
    kill(c2, SIGKILL);
    sc_reap_wait(c1, &st1);
    sc_reap_wait(c2, &st2);
    syscall(SYS_getpgid, 9192);
    printf("SC expect reap c1=%d c2=%d\n",
           WIFSIGNALED(st1) ? WTERMSIG(st1) : -WEXITSTATUS(st1),
           WIFSIGNALED(st2) ? WTERMSIG(st2) : -WEXITSTATUS(st2));
    umount2("/fu", MNT_DETACH);
    kill(srv, SIGKILL);
    waitpid(srv, NULL, 0);
    close(p[0]);
    close(p[1]);
    return 0;
}

static double sc_time(long nr, long a0, long n)
{
    double t0 = now_ns();
    for (long i = 0; i < n; i++) {
        syscall(nr, a0);
    }
    return (now_ns() - t0) / n;
}

static int write_str(const char *path, const char *str, int append);
#define TRACING "/sys/kernel/tracing"

/*
 * The in-guest equivalent of syscall hooks: ftrace syscall tracepoints,
 * for getppid only (syscalls/sys_{enter,exit}_getppid) and for every
 * syscall (raw_syscalls/sys_{enter,exit}), recording into the ring buffer.
 */
static int t_sc_tbench(long n)
{
    static const char *const evs[2][2] = {
        { TRACING "/events/syscalls/sys_enter_getppid/enable",
          TRACING "/events/syscalls/sys_exit_getppid/enable" },
        { TRACING "/events/raw_syscalls/sys_enter/enable",
          TRACING "/events/raw_syscalls/sys_exit/enable" },
    };
    static const char *const what[] = { "getppid", "all" };

    mount("none", TRACING, "tracefs", 0, NULL);
    write_str(TRACING "/buffer_size_kb", "64", 0);
    sc_time(SYS_getppid, 0, n / 10);
    for (int w = 0; w < 2; w++) {
        int ok = 1;
        for (int e = 0; e < 2; e++) {
            ok &= !write_str(evs[w][e], "1", 0);
        }
        if (!ok) {
            printf("BENCH sct what=%s unavailable\n", what[w]);
            continue;
        }
        double a = sc_time(SYS_getppid, 0, n);
        double b = sc_time(SYS_getpgid, 0, n);
        double c = sc_time(SYS_getsid, 0, n);
        for (int e = 0; e < 2; e++) {
            write_str(evs[w][e], "0", 0);
        }
        printf("BENCH sct what=%s n=%ld getppid_ns=%.1f getpgid_ns=%.1f "
               "getsid_ns=%.1f\n", what[w], n, a, b, c);
    }
    return 0;
}

/* Per-syscall cost: a hooked syscall, a filtered one, one never hooked. */
static int t_sc_bench(long n)
{
    sc_time(SYS_getppid, 0, n / 10);    /* warm up */
    sc_time(SYS_getpgid, 0, n / 10);
    sc_time(SYS_getsid, 0, n / 10);
    double a = sc_time(SYS_getppid, 0, n);
    double b = sc_time(SYS_getpgid, 0, n);
    double c = sc_time(SYS_getsid, 0, n);
    printf("BENCH sc n=%ld getppid_ns=%.1f getpgid_ns=%.1f getsid_ns=%.1f\n",
           n, a, b, c);
    return 0;
}

static double hk_time(long (*f)(long), long n)
{
    double t0 = now_ns();
    for (long i = 0; i < n; i++) {
        f(i);
    }
    return (now_ns() - t0) / n;
}

/* a hooked call that a filter may reject: write(2) of one byte to /dev/null */
static int t_sc_fdbench(long n)
{
    int fd = open("/dev/null", O_WRONLY);
    char c = 'x';
    double t0, w, g;

    for (long i = 0; i < n / 10; i++) {
        write(fd, &c, 1);
    }
    sc_time(SYS_getppid, 0, n / 10);
    t0 = now_ns();
    for (long i = 0; i < n; i++) {
        write(fd, &c, 1);
    }
    w = (now_ns() - t0) / n;
    g = sc_time(SYS_getppid, 0, n);
    printf("BENCH fd n=%ld write_ns=%.1f getppid_ns=%.1f\n", n, w, g);
    return 0;
}

static int t_hk_bench(long n)
{
    calls(p_target, 1000, 3, 1);        /* translate */
    calls(p_target2, 1000, 5, 2);
    p_phase(0);
    double a = hk_time(p_target, n), b = hk_time(p_target2, n);
    p_phase(2);                          /* dyn=on: removes target */
    double c = hk_time(p_target, n);
    p_phase(5);
    printf("BENCH hk n=%ld hooked_ns=%.1f unhooked_ns=%.1f "
           "after_remove_ns=%.1f\n", n, a, b, c);
    return 0;
}


static void hk_warm(void)
{
    /* translate a broad slice of busybox and the kernel (bigger TB cache) */
    if (system("find / -xdev >/dev/null 2>&1; gzip -c /pagetest | gunzip | "
               "md5sum >/dev/null; sha256sum /pagetest >/dev/null; "
               "ls -lR /proc/self /sys/kernel >/dev/null 2>&1; "
               "awk 'BEGIN{for(i=0;i<1000;i++)s+=i}' ; "
               "sort -r /proc/kallsyms | head -1 >/dev/null; "
               "tar cf - /bin 2>/dev/null | wc -c >/dev/null") < 0) {
        perror("system");
    }
}

/*
 * Cost of inserting/removing a hook from a callback, as the guest sees it
 * (the callback, the commit and the vCPU's exit from translated code).
 */
static int t_hk_ins(long k, int warm)
{
    double t[6];

    if (warm) {
        hk_warm();
    }
    calls(p_target, k, 3, 1);
    p_phase(8);                         /* translate the phase call */
    t[0] = now_ns(); p_phase(9);        /* no action: baseline */
    t[1] = now_ns(); p_phase(1);        /* insert at a new address */
    t[2] = now_ns(); calls(p_target, k, 3, 1);
    t[3] = now_ns(); p_phase(2);        /* remove */
    t[4] = now_ns(); p_phase(3);        /* insert where a hook was */
    t[5] = now_ns(); p_phase(4);
    printf("BENCH ins warm=%d phase_ns=%.0f insert_new_ns=%.0f "
           "remove_ns=%.0f reinsert_ns=%.0f\n", warm, t[1] - t[0],
           t[2] - t[1], t[4] - t[3], t[5] - t[4]);
    return 0;
}

static int write_str(const char *path, const char *str, int append)
{
    int fd = open(path, O_WRONLY | (append ? O_APPEND : O_TRUNC));
    int r = fd < 0 ? -1 : (int)write(fd, str, strlen(str));
    if (r < 0) {
        printf("HK write %s: %s\n", path, strerror(errno));
    }
    if (fd >= 0) {
        close(fd);
    }
    return r < 0 ? -1 : 0;
}

static void print_file(const char *tag, const char *path)
{
    char line[256];
    FILE *f = fopen(path, "r");
    while (f && fgets(line, sizeof(line), f)) {
        printf("%s %s", tag, line);
    }
    if (f) {
        fclose(f);
    }
}

#define TRACING "/sys/kernel/tracing"

/* The in-guest equivalent: a uprobe trace event on hk_target. */
static int t_hk_ubench(long n)
{
    uintptr_t a = (uintptr_t)hk_target, lo, hi;
    unsigned long long off = 0;
    char line[512], perm[8], ev[128];
    FILE *m = fopen("/proc/self/maps", "r");

    mount("none", TRACING, "tracefs", 0, NULL);
    while (m && fgets(line, sizeof(line), m)) {
        if (sscanf(line, "%lx-%lx %7s %llx", &lo, &hi, perm, &off) == 4 &&
            a >= lo && a < hi) {
            off += a - lo;
            break;
        }
    }
    fclose(m);
    calls(p_target, 1000, 3, 1);
    double base = hk_time(p_target, n);
    snprintf(ev, sizeof(ev), "p:hk/hkt /pagetest:0x%llx\n", off);
    if (write_str(TRACING "/uprobe_events", ev, 1) ||
        write_str(TRACING "/events/hk/hkt/enable", "1", 0)) {
        return 1;
    }
    double hooked = hk_time(p_target, n);
    write_str(TRACING "/events/hk/hkt/enable", "0", 0);
    print_file("HK uprobe_profile", TRACING "/uprobe_profile");
    write_str(TRACING "/uprobe_events", "-:hk/hkt\n", 1);
    printf("BENCH uprobe n=%ld base_ns=%.1f hooked_ns=%.1f\n", n, base, hooked);
    return 0;
}

static double getppid_time(long n)
{
    double t0 = now_ns();
    for (long i = 0; i < n; i++) {
        syscall(SYS_getppid);
    }
    return (now_ns() - t0) / n;
}

/* A kernel probe: getppid(2) with a kprobe (kprobe=1) or without. */
static int t_hk_kbench(long n, int kprobe)
{
    char line[256], sym[128] = "", ev[160];
    FILE *k;
    double base, hooked = 0;

    getppid_time(1000);
    p_phase(0);
    base = getppid_time(n);
    p_phase(1);
    if (!kprobe) {
        printf("BENCH kprobe kprobe=0 n=%ld base_ns=%.1f\n", n, base);
        return 0;
    }
    mount("none", TRACING, "tracefs", 0, NULL);
    /* the syscall entry the kernel calls (same names as guest/pk/syms.py) */
    static const char *const names[] = {
        "__x64_sys_getppid", "__arm64_sys_getppid", "__riscv_sys_getppid",
        "sys_getppid", "SyS_getppid",
    };
    int best = 99;
    k = fopen("/proc/kallsyms", "r");
    while (k && fgets(line, sizeof(line), k)) {
        char t, name[128];
        unsigned long long addr;
        if (sscanf(line, "%llx %c %127s", &addr, &t, name) != 3) {
            continue;
        }
        for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); i++) {
            if (i < best && !strcmp(name, names[i])) {
                best = i;
                strcpy(sym, name);
            }
        }
    }
    if (k) {
        fclose(k);
    }
    snprintf(ev, sizeof(ev), "p:hkk/getppid %s\n", sym);
    printf("HK kprobe sym=%s\n", sym);
    if (!sym[0] || write_str(TRACING "/kprobe_events", ev, 1) ||
        write_str(TRACING "/events/hkk/getppid/enable", "1", 0)) {
        return 1;
    }
    hooked = getppid_time(n);
    write_str(TRACING "/events/hkk/getppid/enable", "0", 0);
    print_file("HK kprobe_profile", TRACING "/kprobe_profile");
    write_str(TRACING "/kprobe_events", "-:hkk/getppid\n", 1);
    printf("BENCH kprobe kprobe=1 sym=%s n=%ld base_ns=%.1f hooked_ns=%.1f\n",
           sym, n, base, hooked);
    return 0;
}

/* exec /piework n times; with ASLR on it lands at a new address each time */
static int t_hk_exec(long n, int aslr)
{
    char v[4];
    double first = 0, rest = 0;

    snprintf(v, sizeof(v), "%d", aslr);
    write_str("/proc/sys/kernel/randomize_va_space", v, 0);
    for (long i = 0; i < n; i++) {
        int st;
        double t0 = now_ns();
        pid_t pid = fork();
        if (pid == 0) {
            execl("/piework", "piework", (char *)NULL);
            _exit(127);
        }
        waitpid(pid, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st)) {
            printf("HK piework failed status=0x%x\n", st);
            return 1;
        }
        double dt = now_ns() - t0;
        if (i == 0) {
            first = dt;
        } else {
            rest += dt;
        }
    }
    printf("BENCH exec aslr=%d n=%ld first_us=%.0f rest_avg_us=%.0f\n", aslr, n,
           first / 1e3, rest / (n - 1) / 1e3);
    return 0;
}

/*
 * osi: the process state a host-side OSI reads, with the kernel's own view of
 * it (/proc/self) as "OSI truth" lines.  Re-execs itself as osi-run with
 * known arguments and environment, opens files of each kind (regular, pipe,
 * socket, anon inode, device, deleted), maps a file at an offset, forks a
 * named child, then marks getpgid(9301) (enter) and getpgid(9302) (return).
 * /proc is read with read(2) into static buffers: no allocation, so no new
 * mappings, between the truth and the marks.
 */
static char osi_buf[65536];

static long osi_slurp(const char *path)
{
    int fd = open(path, O_RDONLY);
    long n = 0, r;

    if (fd < 0) {
        return -1;
    }
    while (n < (long)sizeof(osi_buf) - 1 &&
           (r = read(fd, osi_buf + n, sizeof(osi_buf) - 1 - n)) > 0) {
        n += r;
    }
    close(fd);
    osi_buf[n] = 0;
    return n;
}

static void osi_list(const char *key, const char *path)
{
    long n = osi_slurp(path);

    printf("OSI truth %s ", key);
    for (long i = 0; i < n; i++) {
        putchar(osi_buf[i] ? osi_buf[i] : (i + 1 < n ? '|' : '\n'));
    }
    if (n <= 0 || osi_buf[n - 1]) {
        putchar('\n');
    }
}

static void osi_link(const char *key, const char *path)
{
    char t[512];
    ssize_t n = readlink(path, t, sizeof(t) - 1);

    t[n < 0 ? 0 : n] = 0;
    printf("OSI truth %s %s\n", key, t);
}

/*
 * Syscall injection (penhost.py "inject"): at getpgid(94xx) entries the
 * host makes this task run calls before its own, and answers through the
 * skipped call's return value and buffers.  Self-checking: SC check lines.
 */
static volatile sig_atomic_t inj_usr1, inj_alrm;
static volatile int inj_pipe_w = -1;

static void inj_on_usr1(int s)
{
    (void)s;
    syscall(SYS_getppid);       /* a call of the handler's own, meanwhile */
    inj_usr1++;
}

static void inj_on_alrm(int s)
{
    char c = 'x';

    (void)s;
    inj_alrm++;
    if (inj_pipe_w >= 0 && write(inj_pipe_w, &c, 1) < 0) {
        inj_alrm += 100;
    }
}

static void inj_timer(long ms)
{
    struct itimerval it = { { 0, 0 }, { ms / 1000, (ms % 1000) * 1000 } };

    setitimer(ITIMER_REAL, &it, NULL);
}

#define INJ_CHECK(name, cond, ...) do {                                 \
        int ok_ = (cond);                                               \
        bad += !ok_;                                                    \
        printf("SC check %s %s ", name, ok_ ? "ok" : "FAIL");           \
        printf(__VA_ARGS__);                                            \
        printf("\n");                                                   \
    } while (0)

static int t_sc_inject(void)
{
    pid_t pid = getpid();
    int bad = 0;
    long r;

#if defined(__x86_64__) || defined(__aarch64__)
    struct sigaction sa;
    struct { int64_t fd, n, closed, unmapped; char data[256]; } res;
    char own[256] = "";
    int p[2], fd, lowest;
    char c = 0;

    /* getpid and gettid, run before getpgid(9401) */
    r = syscall(SYS_getpgid, 9401);
    INJ_CHECK("ids", r == pid, "got=%ld want=%d", r, pid);

    /* a number the kernel has no call for; a failed execve comes back,
       clone is refused */
    errno = 0;
    r = syscall(SYS_getpgid, 9402);
    INJ_CHECK("enosys", r == -1 && errno == ENOSYS, "got=%ld errno=%d", r, errno);

    /* /proc/self/stat through the task's own calls: mmap, openat, read,
       close, munmap; the fd is the lowest free one, and is free again */
    lowest = dup(0);
    close(lowest);
    memset(&res, 0, sizeof(res));
    r = syscall(SYS_write, 999, &res, 9403);
    fd = open("/proc/self/stat", O_RDONLY);
    if (fd < 0 || read(fd, own, sizeof(own) - 1) < 0) {
        own[0] = 0;
    }
    close(fd);
    {
        char *e1 = strstr(res.data, ") "), *e2 = strstr(own, ") ");
        INJ_CHECK("proc", r > 0 && r == res.n && e1 && e2 &&
                  e1 - res.data == e2 - own && !memcmp(res.data, own, e1 - res.data),
                  "r=%ld got=%.40s own=%.40s", r, res.data, own);
    }
    INJ_CHECK("fds", res.fd == lowest && fd == lowest && res.closed == 0 &&
              res.unmapped == 0, "fd=%ld lowest=%d again=%d close=%ld munmap=%ld",
              (long)res.fd, lowest, fd, (long)res.closed, (long)res.unmapped);

    /* the injected kill's signal: its handler runs (and makes a call)
       before the injected call is back */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = inj_on_usr1;
    sigaction(SIGUSR1, &sa, NULL);
    r = syscall(SYS_getpgid, 9404);
    INJ_CHECK("signal", r == 0 && inj_usr1 == 1, "got=%ld handled=%d", r, (int)inj_usr1);

    /* an injected ppoll blocks until SIGALRM: -EINTR */
    sa.sa_handler = inj_on_alrm;
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);
    inj_timer(100);
    errno = 0;
    r = syscall(SYS_getpgid, 9405);
    INJ_CHECK("eintr", r == -1 && errno == EINTR && inj_alrm == 1,
              "got=%ld errno=%d alarms=%d", r, errno, (int)inj_alrm);

    /* an injected read on an empty pipe; the SA_RESTART handler writes a
       byte, and the kernel restarts the read, which gets it */
    if (pipe(p)) {
        return 1;
    }
    inj_pipe_w = p[1];
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);
    inj_timer(100);
    r = syscall(SYS_getpgid, 9406, p[0], &c);
    INJ_CHECK("restart", r == 1 && c == 'x' && inj_alrm == 2,
              "got=%ld byte=%d alarms=%d", r, c, (int)inj_alrm);
    inj_pipe_w = -1;
    close(p[0]);
    close(p[1]);

    /* at a return: the injected call's value, as the callback returns it */
    r = syscall(SYS_getpgid, 9407);
    INJ_CHECK("atreturn", r == pid, "got=%ld pid=%d", r, pid);

    /* ... and the task's registers but the result are its own again */
    {
        unsigned long a1 = 0x1111, a2 = 0x2222, a3 = 0x3333, a4 = 0x4444, a5 = 0x5555;
        long rv = 9410, nr = SYS_getpgid;
#if defined(__aarch64__)
        register long x0 __asm__("x0") = rv;
        register unsigned long x1 __asm__("x1") = a1, x2 __asm__("x2") = a2,
            x3 __asm__("x3") = a3, x4 __asm__("x4") = a4, x5 __asm__("x5") = a5;
        register long x8 __asm__("x8") = nr;
        __asm__ volatile("svc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4),
                         "+r"(x5), "+r"(x8) : : "memory");
        rv = x0; a1 = x1; a2 = x2; a3 = x3; a4 = x4; a5 = x5; nr = x8;
#else
        /* rdi, rsi, rdx, r10, r8, r9; rax the number, then the result */
        register unsigned long r10 __asm__("r10") = a3, r8 __asm__("r8") = a4,
            r9 __asm__("r9") = a5;
        unsigned long rdi = 9410, rax = nr;
        __asm__ volatile("syscall" : "+a"(rax), "+D"(rdi), "+S"(a1), "+d"(a2), "+r"(r10),
                         "+r"(r8), "+r"(r9) : : "rcx", "r11", "memory");
        rv = rax; a3 = r10; a4 = r8; a5 = r9;
        a1 = rdi == 9410 ? a1 : 0;      /* the first argument too */
#endif
        INJ_CHECK("atreturn-regs", rv == pid && a1 == 0x1111 && a2 == 0x2222 &&
                  a3 == 0x3333 && a4 == 0x4444 && a5 == 0x5555 && nr == SYS_getpgid,
                  "ret=%ld pid=%d a1=%lx a2=%lx a3=%lx a4=%lx a5=%lx nr=%ld",
                  rv, pid, a1, a2, a3, a4, a5, nr);
    }

    /* many in one callback */
    r = syscall(SYS_getpgid, 9408);
    INJ_CHECK("many", r == pid, "got=%ld want=%d", r, pid);
#else
#if defined(__arm__)
    /* an AArch32 task on an AArch64 kernel: EABI calls are injected */
    r = syscall(SYS_getpgid, 9409);
    INJ_CHECK("compat", r == pid, "got=%ld pid=%d", r, pid);
#else
    /* an i386 task's call: refused */
    r = syscall(SYS_getpgid, 9409);
    INJ_CHECK("compat", r == 1, "got=%ld pid=%d", r, pid);
#endif
#endif
    return bad != 0;
}

/*
 * hc-write: send_hypercall's shape (x86_64: outl to port 0x88, number in
 * eax, a buffer in rdi): the host writes into a zeroed stack buffer from
 * the hypercall, with the guest's permissions (penhost hcwrite).
 */
static int t_hc_write(void)
{
#if defined(__x86_64__)
    char out[0x1000] = {0};
    register unsigned long rdi __asm__("rdi") = (unsigned long)out;
    unsigned long rax = 0xb335a535;

    if (syscall(SYS_iopl, 3)) {
        printf("HC iopl failed\n");
        return 1;
    }
    __asm__ volatile("outl %%eax, $0x88" : "+a"(rax) : "r"(rdi) : "memory");
    printf("HC out=%p got=%.40s\n", (void *)out, out);
    return strncmp(out, "written", 7) != 0;
#else
    return 0;
#endif
}

static int t_osi(char **argv)
{
    char exe[256];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    char *av[] = { "pagetest", "osi-run", "alpha", "beta gamma", NULL };
    char *ev[] = { "OSIENV=1", "FOO=bar baz", NULL };

    (void)argv;
    if (n < 0) {
        return 1;
    }
    exe[n] = 0;
    execve(exe, av, ev);
    return 1;
}

static int t_osi_run(void)
{
    int fd, p[2], go[2], st, fds[64], nfd = 0;
    pid_t child;
    char path[64], *m;
    DIR *d;
    struct dirent *e;

    if (chdir("/mnt") != 0 || pipe(p) != 0 || pipe(go) != 0) {
        return 1;
    }
    fd = open("/mnt/data.bin", O_RDONLY);
    socket(AF_UNIX, SOCK_STREAM, 0);
    eventfd(0, 0);
    open("/dev/null", O_RDWR);
    close(creat("/osi-del", 0600));
    open("/osi-del", O_RDONLY);
    unlink("/osi-del");
    dup2(fd, 40);                       /* a gap in the table */
    m = mmap(NULL, 2 * PG, PROT_READ, MAP_PRIVATE, fd, PG);
    mmap(NULL, 3 * PG, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED) {
        return 1;
    }
    child = fork();
    if (child == 0) {
        prctl(PR_SET_NAME, "osichild");
        read(go[0], path, 1);
        _exit(0);
    }
    usleep(100000);

    printf("OSI truth pid %d\n", getpid());
    printf("OSI truth ppid %d\n", getppid());
    printf("OSI truth ids %d %d %d %d\n", getuid(), getgid(), geteuid(), getegid());
    printf("OSI truth child %d\n", child);
    osi_slurp("/proc/self/comm");
    osi_buf[strcspn(osi_buf, "\n")] = 0;
    printf("OSI truth comm %s\n", osi_buf);
    snprintf(path, sizeof(path), "/proc/%d/comm", child);
    osi_slurp(path);
    osi_buf[strcspn(osi_buf, "\n")] = 0;
    printf("OSI truth childcomm %s\n", osi_buf);
    osi_list("args", "/proc/self/cmdline");
    osi_list("env", "/proc/self/environ");
    osi_link("cwd", "/proc/self/cwd");
    osi_link("exe", "/proc/self/exe");
    /* the fd table, without the directory's own fd */
    d = opendir("/proc/self/fd");
    while (d && (e = readdir(d)) && nfd < 64) {
        if (e->d_name[0] != '.' && atoi(e->d_name) != dirfd(d)) {
            fds[nfd++] = atoi(e->d_name);
        }
    }
    if (d) {
        closedir(d);
    }
    for (int i = 0; i < nfd; i++) {
        char key[32];
        snprintf(key, sizeof(key), "fd %d", fds[i]);
        snprintf(path, sizeof(path), "/proc/self/fd/%d", fds[i]);
        osi_link(key, path);
    }
    /* the mappings: start end perms offset name */
    osi_slurp("/proc/self/maps");
    for (char *l = osi_buf, *nl; *l; l = nl + 1) {
        unsigned long lo, hi, off;
        char perms[8];
        int k = 0;
        nl = strchr(l, '\n');
        if (!nl) {
            break;
        }
        *nl = 0;
        if (sscanf(l, "%lx-%lx %7s %lx %*s %*s %n", &lo, &hi, perms, &off, &k) >= 4) {
            char *name = k ? l + k : "";
            while (*name == ' ') {
                name++;
            }
            printf("OSI truth map %lx %lx %s %lx %s\n", lo, hi, perms, off, name);
        }
    }
    /* /proc/self/stat: starttime (22), startcode..startstack (26-28),
       start_data..env_end (45-51), after the comm's closing paren */
    osi_slurp("/proc/self/stat");
    {
        char *s = strrchr(osi_buf, ')') + 2;
        unsigned long long f[64] = { 0 };
        int i = 3;
        for (char *t = strtok(s, " "); t && i < 64; t = strtok(NULL, " ")) {
            f[i++] = strtoull(t, NULL, 10);
        }
        printf("OSI truth start %llu\n", f[22]);
        printf("OSI truth mm %llx %llx %llx %llx %llx %llx %llx %llx %llx %llx\n",
               f[26], f[27], f[28], f[45], f[46], f[47], f[48], f[49], f[50], f[51]);
    }
    /* the processes: every pid in /proc */
    d = opendir("/proc");
    printf("OSI truth procs");
    while (d && (e = readdir(d))) {
        if (e->d_name[0] >= '0' && e->d_name[0] <= '9') {
            printf(" %s", e->d_name);
        }
    }
    printf("\n");
    if (d) {
        closedir(d);
    }
    syscall(SYS_getpgid, 9301);
    syscall(SYS_getpgid, 9302);
    /* for the fd-path and exe filters: 5 writes to /dev/null, 3 to a pipe
       (and the release below: 4), a mark */
    fd = open("/dev/null", O_WRONLY);
    for (int i = 0; i < 5; i++) {
        write(fd, "x", 1);
    }
    for (int i = 0; i < 3; i++) {
        write(p[1], "y", 1);
    }
    syscall(SYS_getpgid, 9303);
    write(go[1], "x", 1);
    waitpid(child, &st, 0);
    printf("OSI done child=%d\n", WEXITSTATUS(st));
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: pagetest <test> [file]\n");
        return 2;
    }
    const char *t = argv[1], *f = argc > 2 ? argv[2] : "/mnt/data.bin";
    setvbuf(stdout, NULL, _IOLBF, 0);
    PG = sysconf(_SC_PAGESIZE);
    printf("TEST %s\n", t);
    marker(NULL, 0);    /* the plugin logs a test boundary */
    if (!strcmp(t, "user-file")) return t_user_file(f);
    if (!strcmp(t, "kern-file")) return t_kern_file(f);
    if (!strcmp(t, "user-novma")) return t_user_novma();
    if (!strcmp(t, "kern-novma")) return t_kern_novma();
    if (!strcmp(t, "user-eof")) return t_user_eof(f);
    if (!strcmp(t, "kern-eof")) return t_kern_eof(f);
    if (!strcmp(t, "user-protnone")) return t_user_protnone();
    if (!strcmp(t, "kern-contend")) return t_kern_contend(f, argc > 3 ? atoi(argv[3]) : 200, argc > 4 ? atoi(argv[4]) : 1);
#if defined(__x86_64__)
    if (!strcmp(t, "io-bench")) return t_io_bench(argc > 2 ? atol(argv[2]) : 10000);
#endif
    if (!strcmp(t, "user-bench")) return t_bench(0, argc > 2 ? atol(argv[2]) : 10000);
    if (!strcmp(t, "kern-bench")) return t_bench(1, argc > 2 ? atol(argv[2]) : 10000);
    if (!strcmp(t, "user-swap")) return t_swap(0, 0);
    if (!strcmp(t, "kern-swap")) return t_swap(1, 0);
    if (!strcmp(t, "user-wswap")) return t_swap(0, 1);
    if (!strcmp(t, "kern-wswap")) return t_swap(1, 1);
    if (!strcmp(t, "user-wfile")) return t_wfile(f, 0);
    if (!strcmp(t, "kern-wfile")) return t_wfile(f, 1);
    if (!strcmp(t, "user-wzero")) return t_wzero(0);
    if (!strcmp(t, "kern-wzero")) return t_wzero(1);
#define NARG(d) (argc > 2 ? atoi(argv[2]) : (d))
    if (!strcmp(t, "user-span")) return t_span(0, NARG(3));
    if (!strcmp(t, "kern-span")) return t_span(1, NARG(3));
    if (!strcmp(t, "user-wspan")) return t_wspan(0, NARG(3), 0, "/mnt/span.bin");
    if (!strcmp(t, "kern-wspan")) return t_wspan(1, NARG(3), 0, "/mnt/span.bin");
    if (!strcmp(t, "user-wspanro")) return t_wspan(0, NARG(3), 1, "/mnt/span.bin");
    if (!strcmp(t, "kern-wspanro")) return t_wspan(1, NARG(3), 1, "/mnt/span.bin");
    if (!strcmp(t, "user-multi")) return t_multi(0, NARG(0), "/mnt/span.bin");
    if (!strcmp(t, "kern-multi")) return t_multi(1, NARG(0), "/mnt/span.bin");
    if (!strcmp(t, "user-par")) return t_par(0, NARG(60), "/mnt/span.bin");
    if (!strcmp(t, "kern-par")) return t_par(1, NARG(60), "/mnt/span.bin");
    if (!strcmp(t, "hk-count")) return t_hk_count(NARG(100));
    if (!strcmp(t, "hk-alias")) return t_hk_alias(NARG(100));
    if (!strcmp(t, "hk-dyn")) return t_hk_dyn(NARG(100));
    if (!strcmp(t, "hk-kern")) return t_hk_kern(NARG(1000));
    if (!strcmp(t, "hk-smp")) return t_hk_smp(NARG(20000));
    if (!strcmp(t, "sc-func")) return t_sc_func();
    if (!strcmp(t, "sc-trap")) return t_sc_trap();
    if (!strcmp(t, "sc-execs")) return t_sc_execs(NARG(3500));
    if (!strcmp(t, "sc-pen")) return t_sc_pen();
    if (!strcmp(t, "osi")) return t_osi(argv);
    if (!strcmp(t, "osi-run")) return t_osi_run();
    if (!strcmp(t, "sc-fault")) return t_sc_fault();
    if (!strcmp(t, "sc-reap")) return t_sc_reap();
    if (!strcmp(t, "sc-inject")) return t_sc_inject();
    if (!strcmp(t, "hc-write")) return t_hc_write();
    if (!strcmp(t, "sc-bench")) return t_sc_bench(NARG(200000));
    if (!strcmp(t, "sc-fdbench")) return t_sc_fdbench(NARG(200000));
    if (!strcmp(t, "sc-tbench")) return t_sc_tbench(NARG(200000));
    if (!strcmp(t, "hk-bench")) return t_hk_bench(NARG(1000000));
    if (!strcmp(t, "hk-ins")) return t_hk_ins(NARG(100), argc > 3 ? atoi(argv[3]) : 0);
    if (!strcmp(t, "hk-ubench")) return t_hk_ubench(NARG(20000));
    if (!strcmp(t, "hk-kbench")) return t_hk_kbench(NARG(100000), argc > 3 ? atoi(argv[3]) : 0);
    if (!strcmp(t, "hk-exec")) return t_hk_exec(NARG(50), argc > 3 ? atoi(argv[3]) : 0);
    if (!strcmp(t, "user-wro")) return t_wro(f, 0);
    if (!strcmp(t, "kern-wro")) return t_wro(f, 1);
    fprintf(stderr, "unknown test %s\n", t);
    return 2;
}
