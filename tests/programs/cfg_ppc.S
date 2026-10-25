/* tests/machine.py's bare-metal program for the configurable machine (ppc, 750, supervisor
 * state, MMU off: virtual = physical). Reads the timebase twice and writes the decrementer:
 * before the configurable machine set up PPC's timebase (env->tb_env) this was a NULL
 * dereference in the host. Prints "CFG-OK ppc TIMER-OK" when the timebase moved.
 * Hypercall n passes (MAGIC, n, a, b, c) in r3..r7 (r0 = MAGIC); the instruction is
 * xori r10,r10,0. MSR[EE] stays off: the UART's interrupt stays pending (machine.py reads it). */
    .equ ROM, 0x1fc00000
    .equ UART, 0x10000000
    .equ UNIMP, 0x10010000
    .equ MAGIC, 0x50616e64
    .text
    .globl _start
_start:
    lis 1, stack_top@ha
    addi 1, 1, stack_top@l
    lis 9, ROM@h
    lwz 25, 0(9)                /* the file-backed ROM's first word */
    mftb 20                     /* timebase */
    lis 9, 1
    mtctr 9
1:  bdnz 1b
    mftb 21
    lis 9, 0x7fff
    mtdec 9                     /* decrementer */
    lis 9, UART@h
    lis 10, hello@ha
    addi 10, 10, hello@l
    cmpw 20, 21
    bne 2f
    lis 10, stuck@ha
    addi 10, 10, stuck@l
2:  lbz 11, 0(10)
    cmpwi 11, 0
    beq 3f
    stb 11, 0(9)                /* THR */
    addi 10, 10, 1
    b 2b
3:  lis 3, MAGIC@h
    ori 3, 3, MAGIC@l
    mr 0, 3
    li 4, 1
    lis 5, marker@ha
    addi 5, 5, marker@l
    mr 6, 25
    mr 7, 1
hc1:
    xori 10, 10, 0              /* the hypercall instruction */
    lis 11, 0x5354
    ori 11, 11, 0x4f52          /* "STOR" */
    lis 12, marker@ha
    addi 12, 12, marker@l
    stw 11, 16(12)
    lwz 13, 16(12)
    lis 9, UNIMP@h
    stw 11, 4(9)
    lwz 14, 8(9)
    lis 3, MAGIC@h
    ori 3, 3, MAGIC@l
    mr 0, 3
    li 4, 2
    lis 5, marker@ha
    addi 5, 5, marker@l
    mr 6, 14
    mr 7, 13
hc2:
    xori 10, 10, 0
    lis 9, UART@h
    li 11, 0x02                 /* IER: THR empty interrupt, wired to cpu:5 (the 6xx INT pin) */
    stb 11, 1(9)
    lis 3, MAGIC@h
    ori 3, 3, MAGIC@l
    mr 0, 3
    li 4, 3
    li 5, 0
    li 6, 0
hc3:
    xori 10, 10, 0
4:  b 4b
    .data
hello:
    .asciz "CFG-OK ppc TIMER-OK\n"
stuck:
    .asciz "CFG-OK TIMER-STUCK\n"
    .balign 16
marker:
    .ascii "AAAAAAAABBBBBBBBCCCCCCCCtailtail"
    .bss
    .balign 16
    .space 0x1000
stack_top:
