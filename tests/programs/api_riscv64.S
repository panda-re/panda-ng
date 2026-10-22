/* tests/api.py's guest program. Hypercall n passes (MAGIC, n, &marker, sp, pc of the hypercall);
 * between hypercalls 1 and 2 it stores "STOREDOK" at marker+16 and loads it, then it writes marker to stdout.
 * No compressed instructions, so the offsets tests/api.py relies on match aarch64's. */
    .option norvc
    .globl _start
_start:
    lla t0, marker          /* fault the marker's page in, writable, before hypercall 1 */
    lbu t1, 0(t0)
    sb t1, 0(t0)
    li a0, 0x50616e64
    mv a7, a0
    li a1, 1
    lla a2, marker
    mv a3, sp
    lla a4, hc1
hc1:
    .4byte 0x00004013       /* xori x0, x0, 0: the hypercall instruction */
    j 1f                    /* end the TB: what follows is translated with the callbacks on */
1:
    li t1, 0x4b4f4445524f5453   /* "STOREDOK" */
    lla t0, marker
    sd t1, 16(t0)
    ld t2, 16(t0)           /* and a load of it */
    li ra, 0                /* the jal below, which ends this TB, fills ra: */
    jal ra, 2f              /* whether end_block_exec sees it says when that runs */
2:
    li a0, 0x50616e64
    mv a7, a0
    li a1, 2
    lla a2, marker
    mv a3, sp
    lla a4, hc2
hc2:
    .4byte 0x00004013
    li a0, 1                /* write(1, marker, 32) */
    lla a1, marker
    li a2, 32
    li a7, 64
    ecall
    li a0, 0                /* exit(0) */
    li a7, 93
    ecall
    .data
marker:
    .ascii "AAAAAAAABBBBBBBBCCCCCCCCtailtail"
