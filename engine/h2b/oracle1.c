/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Oracle 1 for H2b shadow helpers: random inputs and random input shadows; flipping any tainted
 * input bits must leave every output bit the shadow calls clean unchanged (soundness). Also
 * reports the mean number of tainted output bits against H1's (precision).
 *
 * Built per helper set by run-oracle1.sh: links the original pure helpers with their shadows.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct row { const char *name; uint64_t (*fn)(const uint64_t *in); int nargs; unsigned ptrmask; };
extern const struct row taint_h2b_table[];

typedef uint64_t (*orig_fn)(const uint64_t *a);
struct target { const char *name; orig_fn call; int nargs; uint64_t const_mask; uint64_t h1_mask;
                const int *ops; int nops; int op_arg; };

static uint64_t s = 0x9e3779b97f4a7c15ull;
static uint64_t rnd(void)
{
    s ^= s << 13; s ^= s >> 7; s ^= s << 17;
    return s;
}

/* values that exercise flags: edges, small, signs, random */
static uint64_t val(void)
{
    static const uint64_t edge[] = { 0, 1, 2, 0x7f, 0x80, 0xff, 0x7fff, 0x8000, 0xffff,
        0x7fffffff, 0x80000000, 0xffffffff, 0x7fffffffffffffffull, 0x8000000000000000ull, ~0ull };
    switch (rnd() % 4) {
    case 0: return edge[rnd() % (sizeof(edge) / sizeof(edge[0]))];
    case 1: return rnd() & 0xff;
    case 2: return (uint64_t)-(int64_t)(rnd() & 0xff);
    default: return rnd();
    }
}

static uint64_t mask(void)
{
    switch (rnd() % 5) {
    case 0: return 0;
    case 1: return ~0ull;
    case 2: return 1ull << (rnd() % 64);
    case 3: return 0xffull << (8 * (rnd() % 8));
    default: return rnd();
    }
}

int check(const struct target *t, long trials)
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
    double bits = 0, h1bits = 0;
    for (long k = 0; k < trials; k++) {
        uint64_t in[16] = { 0 };
        int n = t->nargs;
        for (int i = 0; i < n; i++) {
            in[i] = val();
            in[n + i] = mask();
        }
        if (t->ops) {           /* the op is a translation-time constant: clean */
            in[t->op_arg] = t->ops[rnd() % t->nops];
            in[n + t->op_arg] = 0;
        }
        uint64_t out = t->call(in), sh = r->fn(in);
        uint64_t any = 0;
        for (int i = 0; i < n; i++) {
            any |= in[n + i];
        }
        if (!any) {
            if (sh) {
                printf("%-20s nonzero shadow %#llx with clean inputs\n", t->name,
                       (unsigned long long)sh);
                bad++;
            }
            continue;
        }
        tainted++;
        bits += __builtin_popcountll(sh & t->h1_mask);
        h1bits += __builtin_popcountll(t->h1_mask);
        for (int f = 0; f < 16; f++) {
            uint64_t x[16];
            memcpy(x, in, sizeof(x));
            for (int i = 0; i < n; i++) {
                x[i] ^= rnd() & in[n + i];
            }
            uint64_t diff = (t->call(x) ^ out) & ~sh;
            if (diff) {
                if (bad++ < 5) {
                    printf("%-20s UNSOUND: args", t->name);
                    for (int i = 0; i < n; i++) {
                        printf(" %#llx/%#llx", (unsigned long long)in[i],
                               (unsigned long long)in[n + i]);
                    }
                    printf(" out %#llx shadow %#llx changed %#llx\n", (unsigned long long)out,
                           (unsigned long long)sh, (unsigned long long)diff);
                }
            }
        }
    }
    printf("%-20s %ld tainted trials, %ld unsound; tainted result bits %.2f (H1 %.2f)\n",
           t->name, tainted, bad, tainted ? bits / tainted : 0, tainted ? h1bits / tainted : 0);
    return bad != 0;
}
