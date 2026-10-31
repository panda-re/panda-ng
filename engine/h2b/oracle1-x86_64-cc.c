/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Oracle 1 targets: x86 lazy flags (cc_helper.c). */
#include <stdint.h>

struct target { const char *name; uint64_t (*call)(const uint64_t *); int nargs;
                uint64_t const_mask; uint64_t h1_mask; const int *ops; int nops; int op_arg; };
int check(const struct target *t, long trials);

uint64_t helper_cc_compute_all(uint64_t dst, uint64_t src1, uint64_t src2, int op);
uint64_t helper_cc_compute_c(uint64_t dst, uint64_t src1, uint64_t src2, int op);

static uint64_t all(const uint64_t *a) { return helper_cc_compute_all(a[0], a[1], a[2], a[3]); }
static uint64_t c(const uint64_t *a) { return helper_cc_compute_c(a[0], a[1], a[2], a[3]); }

static const int all_ops[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
    19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 55, 59 };
static const int c_ops[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 59 };

int main(void)
{
    struct target t[] = {
        { "cc_compute_all", all, 4, 0, 0x8d5, all_ops, sizeof(all_ops) / sizeof(int), 3 },
        { "cc_compute_c", c, 4, 0, 1, c_ops, sizeof(c_ops) / sizeof(int), 3 },
    };
    int bad = 0;
    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        bad |= check(&t[i], 2000000);
    }
    return bad;
}
