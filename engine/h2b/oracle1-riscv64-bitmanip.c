/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Oracle 1 targets: RISC-V bit manipulation (bitmanip_helper.c). */
#include <stdint.h>

struct target { const char *name; uint64_t (*call)(const uint64_t *); int nargs;
                uint64_t const_mask; uint64_t h1_mask; const int *ops; int nops; int op_arg; };
int check(const struct target *t, long trials);

uint64_t helper_clmul(uint64_t, uint64_t);
uint64_t helper_clmulr(uint64_t, uint64_t);
uint64_t helper_xperm4(uint64_t, uint64_t);
uint64_t helper_xperm8(uint64_t, uint64_t);
uint64_t helper_brev8(uint64_t);
uint64_t helper_unzip(uint64_t);
uint64_t helper_zip(uint64_t);

#define TWO(f) static uint64_t c_##f(const uint64_t *a) { return helper_##f(a[0], a[1]); }
#define ONE(f) static uint64_t c_##f(const uint64_t *a) { return helper_##f(a[0]); }
TWO(clmul) TWO(clmulr) TWO(xperm4) TWO(xperm8) ONE(brev8) ONE(unzip) ONE(zip)

int main(void)
{
    struct target t[] = {
        { "clmul", c_clmul, 2, 0, ~0ull }, { "clmulr", c_clmulr, 2, 0, ~0ull },
        { "xperm4", c_xperm4, 2, 0, ~0ull }, { "xperm8", c_xperm8, 2, 0, ~0ull },
        { "brev8", c_brev8, 1, 0, ~0ull }, { "unzip", c_unzip, 1, 0, ~0ull },
        { "zip", c_zip, 1, 0, ~0ull },
    };
    int bad = 0;
    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        bad |= check(&t[i], 500000);
    }
    return bad;
}
