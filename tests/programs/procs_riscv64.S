/* tests/asidstory.py's guest program; see procs_x86_64.S. No compressed instructions. */
    .option norvc
    .option norelax         /* no gp-relative lla (gp is never set) */
    .set M, 20
    .globl _start
_start:
    li a7, 172              /* getpid */
    ecall
    mv s2, a0
    li a0, 4                /* write(4, &buf, 1): ready (barrier: see procs_x86_64.S) */
    lla a1, buf
    li a2, 1
    li a7, 64
    ecall
    li a0, 3                /* read(3, &buf, 1): wait for the release */
    lla a1, buf
    li a2, 1
    li a7, 63
    ecall
    li t0, 1
    beq a0, t0, 2f
    li a0, 2                /* exit(2) */
    li a7, 93
    ecall
2:
    li s1, 0                /* i */
1:
    li a0, 0x50616e64
    mv a7, a0
    li a1, 5
    mv a2, s2
    mv a3, s1
    lla a4, hc
hc:
    .4byte 0x00004013       /* xori x0, x0, 0: the hypercall instruction */
    lla a0, ts              /* nanosleep(&ts, 0) */
    li a1, 0
    li a7, 101
    ecall
    addi s1, s1, 1
    li t0, M
    bne s1, t0, 1b
    li a0, 0                /* exit(0) */
    li a7, 93
    ecall
    .data
ts: .quad 0, 10000000       /* 10 ms */
buf: .byte 0
