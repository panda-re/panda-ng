/* CPU subset promoted from idlecost/notes/idlecost/idlework.c.
 * CLOCK_MONOTONIC follows host time under TCG; never run with icount.
 * Checks sort ordering/permutation, VM against a scalar reference, CRC test
 * vector, and child-independent deterministic outputs before reporting success.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define NSORT 65536
static int32_t arr[NSORT];
static uint32_t rng = 12345;
static uint32_t lcg(void)
{
    rng = rng * 1103515245u + 12345u;
    return rng >> 1;
}
static double now_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) { perror("clock_gettime"); exit(1); }
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}
static void qs(int32_t *a, long lo, long hi)
{
    while (lo < hi) {
        int32_t p = a[(lo + hi) / 2];
        long i = lo, j = hi;
        while (i <= j) {
            while (a[i] < p) i++;
            while (a[j] > p) j--;
            if (i <= j) {
                int32_t t = a[i]; a[i] = a[j]; a[j] = t;
                i++; j--;
            }
        }
        if (j - lo < hi - i) { qs(a, lo, j); lo = i; }
        else { qs(a, i, hi); hi = j; }
    }
}
enum { PUSH, MUL, XOR, DUP, SWAP, DROP, JNZ, DEC, SHR, AND, OVER, HALT };
static const uint32_t prog[] = {
    PUSH, 0, PUSH, 200000,
    SWAP, OVER, PUSH, 2654435761u & 0x7fffffff, MUL, XOR,
    DUP, PUSH, 13, SHR, XOR, PUSH, 0x7fffffff, AND,
    SWAP, DEC, DUP, JNZ, 4, DROP, HALT,
};
/* Unsigned arithmetic avoids the historical signed-overflow undefined behavior. */
__attribute__((noinline)) static uint64_t run_vm(void)
{
    uint64_t st[64];
    int sp = 0;
    long pc = 0;
    for (;;) {
        switch (prog[pc++]) {
        case PUSH: st[sp++] = prog[pc++]; break;
        case MUL: sp--; st[sp - 1] *= st[sp]; break;
        case XOR: sp--; st[sp - 1] ^= st[sp]; break;
        case AND: sp--; st[sp - 1] &= st[sp]; break;
        case SHR: sp--; st[sp - 1] >>= st[sp]; break;
        case DUP: st[sp] = st[sp - 1]; sp++; break;
        case OVER: st[sp] = st[sp - 2]; sp++; break;
        case SWAP: { uint64_t t = st[sp - 1]; st[sp - 1] = st[sp - 2]; st[sp - 2] = t; } break;
        case DROP: sp--; break;
        case DEC: st[sp - 1]--; break;
        case JNZ: sp--; if (st[sp]) pc = prog[pc]; else pc++; break;
        case HALT: return st[sp - 1];
        default: exit(1);
        }
    }
}
static uint32_t crc32_bitwise(const uint8_t *p, size_t n)
{
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & -(c & 1));
    }
    return ~c;
}
static int cpu(int reps)
{
    double ts = 0, tv = 0, tc = 0;
    uint64_t sum = 0, reference = 0;
    static uint8_t buf[262144];
    for (uint64_t n = 200000; n; n--) {
        reference ^= n * (2654435761u & 0x7fffffff);
        reference ^= reference >> 13;
        reference &= 0x7fffffff;
    }
    if (crc32_bitwise((const uint8_t *)"123456789", 9) != 0xcbf43926u) return 1;
    for (int r = 0; r < reps; r++) {
        uint64_t before = 0, after = 0;
        for (long i = 0; i < NSORT; i++) { arr[i] = (int32_t)lcg(); before += arr[i]; }
        double t0 = now_ns();
        qs(arr, 0, NSORT - 1);
        double t1 = now_ns();
        uint64_t vm = run_vm();
        double t2 = now_ns();
        if (vm != reference) return 1;
        for (long i = 0; i < NSORT; i++) {
            after += arr[i];
            if (i && arr[i - 1] > arr[i]) return 1;
        }
        if (before != after) return 1;
        for (size_t i = 0; i < sizeof(buf); i++) buf[i] = (uint8_t)lcg();
        double t3 = now_ns();
        uint32_t crc = crc32_bitwise(buf, sizeof(buf));
        double t4 = now_ns();
        sum += arr[NSORT / 3] + vm + crc;
        ts += t1 - t0; tv += t2 - t1; tc += t4 - t3;
    }
    puts("CHECK idlework cpu PASS");
    printf("BENCH iw cpu reps=%d sort_ms=%.6f vm_ms=%.6f crc_ms=%.6f sum=%llu\n",
           reps, ts / 1e6, tv / 1e6, tc / 1e6, (unsigned long long)sum);
    return 0;
}
int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    long reps = 6;
    if (argc < 2 || argc > 3 || strcmp(argv[1], "cpu")) return 2;
    if (argc == 3) {
        char *end;
        errno = 0;
        reps = strtol(argv[2], &end, 10);
        if (errno || !*argv[2] || *end || reps < 1 || reps > 100) return 2;
    }
    return cpu((int)reps);
}
