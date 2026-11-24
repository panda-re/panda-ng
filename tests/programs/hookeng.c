/* tests/hookeng.py's guest program: code at known addresses for engine address hooks.
 *   labels:     writes "<name> <hex address>" for each label, one per line;
 *   late <n>:   n rounds of blk(), with the round (1..n) in `iter` before each;
 *   hot <n>:    n rounds of hot();
 *   ins <n>:    n rounds of ins();
 * blk is one block of three adjacent instructions a0 a1 a2 that jumps to b0 (another block, its
 * chained successor on the same page), which jumps to e0, the first instruction on the next page;
 * on MIPS the first jump's delay slot is ds0. hot is c0 c1. ins0 is an instruction
 * longer than 2 bytes (inside: ins0 + 2). On arm, th0 th1 are Thumb instructions, called from ARM
 * code in late's rounds too.
 * Built by tests/programs/build.sh: freestanding, static, no libc (tests/programs/sys.h). */
#include "sys.h"

volatile unsigned long iter;

void blk(void);
void hot(void);
void ins(void);
extern char a0[], a1[], a2[], b0[], b1[], e0[], c0[], c1[], ins0[];

#if defined(__x86_64__)
__asm__(".text\n.globl blk, a0, a1, a2, b0, b1, e0, hot, c0, c1, ins, ins0\n"
        "blk:\na0: add $1, %rax\na1: add $2, %rax\na2: add $3, %rax\n jmp b0\n .skip 32, 0xcc\n"
        "b0: add $5, %rax\nb1: add $6, %rax\n jmp e0\n .balign 4096, 0xcc\ne0: add $7, %rax\n ret\n"
        "hot:\nc0: add $1, %rax\nc1: add $2, %rax\n ret\n"
        "ins:\nins0: movabs $0x1122334455667788, %rax\n ret\n");
#elif defined(__aarch64__)
__asm__(".text\n.globl blk, a0, a1, a2, b0, b1, e0, hot, c0, c1, ins, ins0\n"
        "blk:\na0: add x9, x9, #1\na1: add x9, x9, #2\na2: add x9, x9, #3\n b b0\n .skip 32\n"
        "b0: add x9, x9, #5\nb1: add x9, x9, #6\n b e0\n .balign 4096\ne0: add x9, x9, #7\n ret\n"
        "hot:\nc0: add x9, x9, #1\nc1: add x9, x9, #2\n ret\n"
        "ins:\nins0: movz x9, #0x1234, lsl #16\n ret\n");
#elif defined(__mips__)
extern char ds0[];
__asm__(".text\n.set push\n.set noreorder\n.globl blk, a0, a1, a2, ds0, b0, b1, e0, hot, c0, c1, ins, ins0\n"
        "blk:\na0: addiu $8, $8, 1\na1: addiu $8, $8, 2\na2: addiu $8, $8, 3\n j b0\n"
        "ds0: addiu $9, $9, 1\n .skip 32\n"
        "b0: addiu $8, $8, 5\nb1: j e0\n addiu $8, $8, 6\n .balign 4096\ne0: jr $31\n addiu $8, $8, 7\n"
        "hot:\nc0: addiu $8, $8, 1\nc1: jr $31\n addiu $8, $8, 2\n"
        "ins:\nins0: lui $8, 0x1234\n jr $31\n nop\n.set pop\n");
#elif defined(__powerpc64__)
__asm__(".text\n.globl blk, a0, a1, a2, b0, b1, e0, hot, c0, c1, ins, ins0\n"
        "blk:\na0: addi 9, 9, 1\na1: addi 9, 9, 2\na2: addi 9, 9, 3\n b b0\n .skip 32\n"
        "b0: addi 9, 9, 5\nb1: addi 9, 9, 6\n b e0\n .balign 4096\ne0: addi 9, 9, 7\n blr\n"
        "hot:\nc0: addi 9, 9, 1\nc1: addi 9, 9, 2\n blr\n"
        "ins:\nins0: lis 9, 0x1234\n blr\n");
#elif defined(__arm__)
extern char th0[], th1[];
void th(void);
__asm__(".syntax unified\n.text\n.arm\n.globl blk, a0, a1, a2, b0, b1, e0, hot, c0, c1, ins, ins0\n"
        "blk:\na0: add r0, r0, #1\na1: add r0, r0, #2\na2: add r0, r0, #3\n b b0\n .skip 32\n"
        "b0: add r0, r0, #5\nb1: add r0, r0, #6\n b e0\n .balign 4096\ne0: add r0, r0, #7\n bx lr\n"
        "hot:\nc0: add r0, r0, #1\nc1: add r0, r0, #2\n bx lr\n"
        "ins:\nins0: movw r0, #0x1234\n bx lr\n"
        ".thumb\n.globl th, th0, th1\n.thumb_func\nth:\nth0: adds r0, r0, #1\nth1: adds r0, r0, #2\n"
        " bx lr\n.arm\n");
#else
#error "hookeng.c: no code for this arch"
#endif

static void out(const char *s)
{
    unsigned long n = 0;

    while (s[n]) {
        n++;
    }
    sc(NR_write, 1, (long)s, n, 0, 0, 0);
}

static void label(const char *name, const void *p)
{
    char buf[40], *q = buf;
    unsigned long v = (unsigned long)p;

    while (*name) {
        *q++ = *name++;
    }
    *q++ = ' ';
    for (int i = (int)sizeof(v) * 8 - 4; i >= 0; i -= 4) {
        *q++ = "0123456789abcdef"[(v >> i) & 15];
    }
    *q++ = '\n';
    *q = 0;
    out(buf);
}

static int eq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++, b++;
    }
    return *a == *b;
}

static unsigned long num(const char *s)
{
    unsigned long v = 0;

    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (unsigned long)(*s++ - '0');
    }
    return v;
}

void _start_c(long *sp)
{
    long argc = sp[0];
    char **argv = (char **)(sp + 1);
    unsigned long n = argc > 2 ? num(argv[2]) : 0;

    if (argc > 1 && eq(argv[1], "labels")) {
        label("a0", a0); label("a1", a1); label("a2", a2); label("b0", b0); label("b1", b1); label("e0", e0);
        label("c0", c0); label("c1", c1); label("ins0", ins0); label("iter", (const void *)&iter);
#if defined(__mips__)
        label("ds0", ds0);
#endif
#if defined(__arm__)
        label("th0", th0); label("th1", th1);
#endif
    } else if (argc > 1 && eq(argv[1], "late")) {
        for (unsigned long i = 1; i <= n; i++) {
            iter = i;
            blk();
#if defined(__arm__)
            th();
#endif
        }
    } else if (argc > 1 && eq(argv[1], "hot")) {
        for (unsigned long i = 0; i < n; i++) {
            hot();
        }
    } else if (argc > 1 && eq(argv[1], "ins")) {
        for (unsigned long i = 0; i < n; i++) {
            ins();
        }
    }
    sc(NR_exit_group, 0, 0, 0, 0, 0, 0);
}
