/* tests/fastcov.py's program; see fastcov_aarch64.S. No compressed instructions. */
    .option norvc
    .option norelax
    .globl _start
_start:
    lla t0, input
    sd zero, 0(t0)          /* fault input's page in (a private copy) for the host to write */
    li a0, 0x50616e64       /* hypercall 1 */
    mv a7, a0
    li a1, 1
    lla a2, input
    li a3, 0
    lla a4, hc1
hc1:
    .4byte 0x00004013
    j 1f                    /* end the block: the VM stops here, before input is read */
1:
    lla t0, input
    ld s1, 0(t0)
    li s2, 0
    li s3, 0
    li s4, 64
round:
    srl t1, s1, s2
    andi t1, t1, 1
    beqz t1, zero
    addi s3, s3, 3          /* bit set */
    j next
zero:
    xori s3, s3, 5          /* bit clear */
next:
    addi s2, s2, 1
    bne s2, s4, round
    li a0, 0x50616e64       /* hypercall 2 */
    mv a7, a0
    li a1, 2
    mv a2, s3
    li a3, 0
    lla a4, hc2
hc2:
    .4byte 0x00004013
    li a0, 0                /* exit(0) */
    li a7, 93
    ecall
    .data
    .balign 8
input:
    .quad 0
