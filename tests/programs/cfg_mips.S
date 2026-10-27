/* tests/machine.py's bare-metal program for the configurable machine (mips and mipsel, 24Kf,
 * kernel mode with Status.ERL set from reset, so kuseg is unmapped: virtual = physical).
 * Assembled twice: --defsym BE=1 -EB for mips, --defsym BE=0 -EL for mipsel.
 * Reads the CP0 timer (Count twice, then writes Compare): before the configurable machine set
 * up MIPS's CP0 timer this was a NULL dereference in the host. Prints "CFG-OK <arch> TIMER-OK"
 * when Count moved. Hypercall n passes (MAGIC, n, a, b, c) in a0..a3, t0 (v0 = MAGIC). Before
 * hypercall 1 it sets HI = 0x48494849 and LO = 0x4c4f4c4f. */
    .set mips32r2
    .set noat
    .equ ROM, 0x1fc00000
    .equ UART, 0x1f000000
    .equ UNIMP, 0x1f010000
    .equ MAGIC, 0x50616e64
    .text
    .globl _start
_start:
    la $sp, stack_top
    li $t0, ROM
    lw $s5, 0($t0)              /* the file-backed ROM's first word */
    mfc0 $s0, $9                /* Count */
    li $t1, 100000
1:  addiu $t1, $t1, -1
    bnez $t1, 1b
    mfc0 $s1, $9
    addiu $t2, $s1, 0x1000
    mtc0 $t2, $11               /* Compare: arms the timer (Status.IE is off) */
    li $t0, UART
    la $t1, hello
    bne $s0, $s1, 2f
    la $t1, stuck
2:  lbu $t2, 0($t1)
    beqz $t2, 3f
    sb $t2, 0($t0)              /* THR */
    addiu $t1, $t1, 1
    b 2b
3:  li $t1, 0x48494849         /* HI and LO: tests/machine.py reads them at hypercall 1 */
    mthi $t1
    li $t1, 0x4c4f4c4f
    mtlo $t1
    li $v0, MAGIC
    move $a0, $v0
    li $a1, 1
    la $a2, marker
    move $a3, $s5
    move $t0, $sp
hc1:
    movz $0, $0, $0             /* the hypercall instruction */
.if BE
    li $t1, 0x53544f52          /* "STOR" in memory */
.else
    li $t1, 0x524f5453
.endif
    la $t2, marker
    sw $t1, 16($t2)
    lw $t3, 16($t2)
    li $t0, UNIMP
    sw $t1, 4($t0)
    lw $t4, 8($t0)
    li $v0, MAGIC
    move $a0, $v0
    li $a1, 2
    la $a2, marker
    move $a3, $t4
    move $t0, $t3
hc2:
    movz $0, $0, $0
    li $t0, UART
    li $t1, 0x02                /* IER: THR empty interrupt, wired to cpu:2 (IP2) */
    sb $t1, 1($t0)
    mfc0 $a3, $13               /* Cause: bit 10 (IP2) pending */
    li $v0, MAGIC
    move $a0, $v0
    li $a1, 3
    li $a2, 0
hc3:
    movz $0, $0, $0
4:  b 4b
    .data
hello:
.if BE
    .asciz "CFG-OK mips TIMER-OK\n"
.else
    .asciz "CFG-OK mipsel TIMER-OK\n"
.endif
stuck:
    .asciz "CFG-OK TIMER-STUCK\n"
    .balign 16
marker:
    .ascii "AAAAAAAABBBBBBBBCCCCCCCCtailtail"
    .bss
    .balign 16
    .space 0x1000
stack_top:
