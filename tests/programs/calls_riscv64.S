/* tests/calls.py's guest program; see calls_x86_64.S. Non-leaf functions keep ra on the stack.
 * No compressed instructions. */
    .option norvc
    .set N, 20
    .set D, 4
    .globl _start
_start:
    lla a2, labels          /* fault the label table's page in before hypercall 1 reads it */
    lbu t0, 0(a2)
    li a0, 0x50616e64
    mv a7, a0
    li a1, 1
    li a3, 0
    lla a4, hc1
hc1:
    .4byte 0x00004013       /* xori x0, x0, 0: the hypercall instruction */
    j setup
setup:
    li s1, 0                /* i */
    j loop
loop:
    mv a0, s1
    call f
ret_main:
    addi s1, s1, 1
    li t0, N
    bne s1, t0, loop
done:
    li a0, D
    call r
ret_r0:
    j fin
f:
    addi sp, sp, -16
hook_mid:
    sd ra, 8(sp)
    andi t1, a0, 1
    beqz t1, f_even
f_odd:
    call g
f_even:
    call h
f_ret:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret
g:
    addi sp, sp, -16
    sd ra, 8(sp)
    call h
g_ret:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret
h:
    ret
r:
    beqz a0, r_base
r_rec:
    addi sp, sp, -16
    sd ra, 8(sp)
    addi a0, a0, -1
    call r
r_ret:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret
r_base:
    ret
fin:
    li a0, 0x50616e64
    mv a7, a0
    li a1, 2
    lla a2, labels
    li a3, 0
    lla a4, hc2
hc2:
    .4byte 0x00004013
    li a0, 0                /* exit(0) */
    li a7, 93
    ecall
    .data
labels:                     /* for the test: N, D, then the labels' addresses */
    .quad N, D, setup, loop, ret_main, done, ret_r0, f, hook_mid, f_odd, f_even, f_ret, g, g_ret, h, r, r_rec, r_ret, r_base, fin
