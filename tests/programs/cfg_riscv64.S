/* tests/machine.py's bare-metal program for the configurable machine (riscv64, rv64, M-mode).
 * Hypercall n passes (MAGIC, n, a, b, c) in a0..a4 (a7 = MAGIC). */
    .option norvc
    .equ ROM, 0x20000000
    .equ UART, 0x10000000
    .equ UNIMP, 0x10010000
    .equ MAGIC, 0x50616e64
    .globl _start
_start:
    la sp, stack_top
    li t0, ROM
    ld t5, 0(t0)                /* the file-backed ROM's first 8 bytes */
    li t0, UART
    la t1, hello
1:  lbu t2, 0(t1)
    beqz t2, 2f
    sb t2, 0(t0)                /* THR */
    addi t1, t1, 1
    j 1b
2:  li a0, MAGIC
    mv a7, a0
    li a1, 1
    la a2, marker
    mv a3, t5
    mv a4, sp
hc1:
    .4byte 0x00004013           /* the hypercall instruction */
    j 3f
3:  li t1, 0x4b4f4445524f5453
    la t0, marker
    sd t1, 16(t0)
    ld t3, 16(t0)
    li t0, UNIMP
    sw t1, 4(t0)
    lw t4, 8(t0)
    li a0, MAGIC
    mv a7, a0
    li a1, 2
    la a2, marker
    mv a3, t4
    mv a4, t3
hc2:
    .4byte 0x00004013
    li t0, UART
    li t1, 0x02                 /* IER: THR empty interrupt */
    sb t1, 1(t0)
    csrr a3, mip                /* bit 11 (MEIP): the UART's line, wired to the CPU */
    li a0, MAGIC
    mv a7, a0
    li a1, 3
    li a2, 0
hc3:
    .4byte 0x00004013
4:  j 4b
hello:
    .asciz "CFG-OK riscv64\n"
    .data
    .balign 16
marker:
    .ascii "AAAAAAAABBBBBBBBCCCCCCCCtailtail"
    .bss
    .balign 16
    .space 0x1000
stack_top:
