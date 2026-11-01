/* tests/hypercall_rr.py's guest program; see answers_x86_64.S. The answer comes back in a0.
 * No compressed instructions. */
    .option norvc
    .set N, 16
    .globl _start
_start:
    lla t1, buf
    sd zero, 0(t1)          /* fault buf's page in, writable, before the handler writes it */
    li s1, 0                /* checksum */
    li s2, 0                /* i */
loop:
    li a0, 0x50616e64
    mv a7, a0
    li a1, 1
    lla a2, buf
    mv a3, s2
    lla a4, hc1
hc1:
    .4byte 0x00004013       /* xori x0, x0, 0: the hypercall instruction */
    add s1, s1, a0
    lla t1, buf
    ld t2, 0(t1)
    add s1, s1, t2
    addi s2, s2, 1
    li t3, N
    bne s2, t3, loop
    li a0, 0x50616e64
    mv a7, a0
    li a1, 2
    mv a2, s1
    li a3, 0
    lla a4, hc2
hc2:
    .4byte 0x00004013
    li a0, 0                /* exit(0) */
    li a7, 93
    ecall
    .data
    .balign 8
buf:
    .quad 0
