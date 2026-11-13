/* tests/flush_smp.py's guest program; see flushsmp_x86_64.S. The test stops the loop with s6. No
 * compressed instructions. */
    .option norvc
    .globl _start
_start:
    li s6, 0
    li s1, 0
loop:
    li a0, 0x50616e64
    mv a7, a0
    li a1, 1
    lla a2, after
    lla a3, mark
    lla a4, hc
hc:
    .4byte 0x00004013       /* xori x0, x0, 0: the hypercall instruction */
    j after                 /* end the TB: `after` is the next block */
after:
    addi s1, s1, 1
mark:
    beqz s6, loop
    li a0, 0                /* exit(0) */
    li a7, 93
    ecall
