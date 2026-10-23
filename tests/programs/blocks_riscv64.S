/* tests/blocks.py's guest program; see blocks_x86_64.S. No compressed instructions. */
    .option norvc
    .set N, 1000
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
    j setup                 /* end the TB: what follows runs with the callbacks on */
setup:                      /* block "setup" */
    li t0, N
    li t1, 0
    j loop                  /* end the block, so "loop" starts every iteration */
loop:                       /* block "loop": N times */
    add t1, t1, t0
    andi t2, t0, 1
    beqz t2, even
odd:                        /* block "odd": N/2 times */
    addi t1, t1, 3
even:                       /* block "even": N/2 times */
    addi t0, t0, -1
    bnez t0, loop
fault:                      /* block "fault": its third instruction faults (first write to the page) */
    lla t3, page
    li t4, 7
fault_store:
    sd t4, 0(t3)
    addi t4, t4, 1
    sd t4, 8(t3)
    j sys
sys:                        /* block "sys": ends in a system call (getpid) */
    li a7, 172
    ecall
    li a0, 0x50616e64       /* hypercall 2 */
    mv a7, a0
    li a1, 2
    li a2, 0
    li a3, 0
    lla a4, hc2
hc2:
    .4byte 0x00004013
    li a0, 0                /* exit(0) */
    li a7, 93
    ecall
    .data
labels:                     /* for the test: the blocks' addresses */
    .quad setup, loop, odd, even, fault, fault_store, sys
    .bss
    .balign 4096
    .skip 4096              /* a whole page between .data's tail and the one written */
page:
    .skip 4096
