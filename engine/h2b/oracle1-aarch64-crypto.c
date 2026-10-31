/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Oracle 1 for H2b helpers with pointer arguments (aarch64 crypto: gvec helpers whose pointers
 * are vector registers in the CPU state). For these the engine calls the instrumented clone in
 * place of the helper: it does the helper's work and the shadow's. The registers live in a fake
 * CPU state with a shadow beside it. Checks, per trial:
 * - fidelity: the clone leaves the state exactly as the real helper does;
 * - soundness: rerunning the real helper with random tainted input bits flipped changes only
 *   bytes the clone's shadow marks tainted.
 * Aliased registers (vd == vn) included.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct row { const char *name; uint64_t (*fn)(const uint64_t *in); int nargs; unsigned ptrmask; };
extern const struct row taint_h2b_table[];

#define F3(n) void helper_crypto_##n(void *, void *, void *, uint32_t);
#define F2(n) void helper_crypto_##n(void *, void *, uint32_t);
F3(sha1su0) F3(sha1c) F3(sha1p) F3(sha1m) F2(sha1h) F2(sha1su1) F3(sha256h) F3(sha256h2)
F2(sha256su0) F3(sha256su1) F3(sha512h) F3(sha512h2) F2(sha512su0) F3(sha512su1) F3(sm3partw1)
F3(sm3partw2) F3(sm3tt1a) F3(sm3tt1b) F3(sm3tt2a) F3(sm3tt2b) F3(rax1) F3(aese) F3(aesd)
F2(aesmc) F2(aesimc)

/* the reference helpers take the portable path, as the clones do (no host AES instructions) */
unsigned cpuinfo = 0;

struct target { const char *name; void *fn; int nptr; };
#define T3(n) { "crypto_" #n, (void *)helper_crypto_##n, 3 }
#define T2(n) { "crypto_" #n, (void *)helper_crypto_##n, 2 }
static const struct target targets[] = {
    T3(sha1su0), T3(sha1c), T3(sha1p), T3(sha1m), T2(sha1h), T2(sha1su1), T3(sha256h),
    T3(sha256h2), T2(sha256su0), T3(sha256su1), T3(sha512h), T3(sha512h2), T2(sha512su0),
    T3(sha512su1), T3(sm3partw1), T3(sm3partw2), T3(sm3tt1a), T3(sm3tt1b), T3(sm3tt2a),
    T3(sm3tt2b), T3(rax1), T3(aese), T3(aesd), T2(aesmc), T2(aesimc),
};

#define REGS 4
#define STATE (REGS * 32)               /* four 32-byte registers (maxsz up to 32) */
static uint64_t s = 0x9e3779b97f4a7c15ull;
static uint64_t rnd(void)
{
    s ^= s << 13; s ^= s >> 7; s ^= s << 17;
    return s;
}

static void call(const struct target *t, uint8_t *st, const int *reg, uint32_t desc)
{
    if (t->nptr == 3) {
        ((void (*)(void *, void *, void *, uint32_t))t->fn)(st + 32 * reg[0], st + 32 * reg[1],
                                                            st + 32 * reg[2], desc);
    } else {
        ((void (*)(void *, void *, uint32_t))t->fn)(st + 32 * reg[0], st + 32 * reg[1], desc);
    }
}

static int check(const struct target *t, long trials)
{
    const struct row *r = NULL;
    for (const struct row *p = taint_h2b_table; p->name; p++) {
        if (!strcmp(p->name, t->name)) {
            r = p;
        }
    }
    if (!r) {
        printf("%-20s no shadow helper\n", t->name);
        return 1;
    }
    long bad = 0, tainted = 0;
    double bits = 0;
    /* env and shadow env in one block, as the engine lays them out (shadow at a fixed delta) */
    static uint8_t mem[2][STATE] __attribute__((aligned(32)));
    uint8_t *st = mem[0], *sh = mem[1];
    for (long k = 0; k < trials; k++) {
        int reg[3];
        for (int i = 0; i < 3; i++) {
            reg[i] = rnd() % REGS;      /* aliasing allowed */
        }
        uint32_t maxsz = rnd() & 1 ? 32 : 16;
        uint32_t desc = (maxsz / 8 - 1) | (1u << 8) | ((uint32_t)(rnd() & 3) << 10);
        uint8_t in[STATE], in_sh[STATE];
        for (int i = 0; i < STATE; i++) {
            in[i] = rnd();
            in_sh[i] = 0;
        }
        switch (rnd() % 4) {            /* taint: none, one byte, one register, random bits */
        case 0: break;
        case 1: in_sh[rnd() % STATE] = 1 << (rnd() % 8); break;
        case 2: memset(in_sh + 32 * (rnd() % REGS), 0xff, 16); break;
        default: for (int i = 0; i < STATE; i++) in_sh[i] = rnd() & rnd(); break;
        }
        uint64_t args[16] = { 0 };
        int n = t->nptr + 1;
        for (int i = 0; i < t->nptr; i++) {
            args[i] = (uintptr_t)(st + 32 * reg[i]);
        }
        args[t->nptr] = desc;
        args[2 * n] = (uintptr_t)sh - (uintptr_t)st;
        memcpy(st, in, STATE);
        memcpy(sh, in_sh, STATE);
        r->fn(args);
        uint8_t out_sh[STATE], out[STATE], ref[STATE];
        memcpy(out_sh, sh, STATE);
        memcpy(out, st, STATE);
        memcpy(st, in, STATE);
        call(t, st, reg, desc);
        memcpy(ref, st, STATE);
        if (memcmp(out, ref, STATE)) {
            if (bad++ < 3) {
                printf("%-20s CLONE DIFFERS from the helper: regs %d %d %d desc %#x\n", t->name,
                       reg[0], reg[1], reg[2], desc);
            }
            continue;
        }
        int any = 0;
        for (int i = 0; i < STATE; i++) {
            any |= in_sh[i] != 0;
        }
        if (!any) {
            for (int i = 0; i < STATE; i++) {
                if (out_sh[i]) {
                    printf("%-20s shadow appeared from clean inputs\n", t->name);
                    bad++;
                    break;
                }
            }
            continue;
        }
        tainted++;
        for (int i = 0; i < 32 * REGS; i++) {
            bits += __builtin_popcount(out_sh[i]);
        }
        for (int f = 0; f < 16; f++) {
            for (int i = 0; i < STATE; i++) {
                st[i] = in[i] ^ (rnd() & in_sh[i]);
            }
            call(t, st, reg, desc);
            for (int i = 0; i < STATE; i++) {
                uint8_t d = (st[i] ^ out[i]) & ~out_sh[i];
                if (d && bad++ < 3) {
                    printf("%-20s UNSOUND: regs %d %d %d desc %#x byte %d changed %#x\n",
                           t->name, reg[0], reg[1], reg[2], desc, i, d);
                }
            }
        }
    }
    printf("%-20s %ld tainted trials, %ld unsound; tainted state bits after %.1f\n", t->name,
           tainted, bad, tainted ? bits / tainted : 0);
    return bad != 0;
}

int main(void)
{
    int bad = 0;
    for (unsigned i = 0; i < sizeof(targets) / sizeof(targets[0]); i++) {
        bad |= check(&targets[i], 100000);
    }
    return bad;
}
