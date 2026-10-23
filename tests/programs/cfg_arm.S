/* tests/machine.py's bare-metal program for the configurable machine (arm, cortex-a15, SVC).
 * Hypercall n passes (MAGIC, n, a, b, c) in r0..r4 (r7 = MAGIC). */
    .syntax unified
    .arm
    .equ ROM, 0x00000000
    .equ UART, 0x09000000
    .equ UNIMP, 0x0a000000
    .equ MAGIC, 0x50616e64
    .globl _start
_start:
    ldr sp, =stack_top
    ldr r10, =ROM
    ldr r5, [r10]               /* the file-backed ROM's first 4 bytes */
    ldr r10, =UART
    ldr r9, =0x301
    str r9, [r10, #0x30]
    adr r11, hello
1:  ldrb r9, [r11], #1
    cmp r9, #0
    beq 2f
    str r9, [r10]
    b 1b
2:  ldr r0, =MAGIC
    mov r7, r0
    mov r1, #1
    adr r2, marker
    mov r3, r5
    mov r4, sp
hc1:
    mcr p7, 0, r0, c0, c0, 0    /* the hypercall instruction */
    b 3f
3:  ldr r9, =0x524f5453         /* "STOR" */
    adr r12, marker
    str r9, [r12, #16]
    ldr r8, [r12, #16]
    ldr r10, =UNIMP
    str r9, [r10, #4]
    ldr r6, [r10, #8]
    ldr r0, =MAGIC
    mov r7, r0
    mov r1, #2
    adr r2, marker
    mov r3, r6
    mov r4, r8
hc2:
    mcr p7, 0, r0, c0, c0, 0
    ldr r10, =UART
    mov r9, #0x20
    str r9, [r10, #0x38]
    mov r9, #'!'
    str r9, [r10]
    isb
    mrc p15, 0, r3, c12, c1, 0  /* ISR: bit 7, an IRQ is pending at the CPU */
    ldr r0, =MAGIC
    mov r7, r0
    mov r1, #3
    mov r2, #0
hc3:
    mcr p7, 0, r0, c0, c0, 0
4:  b 4b
    .ltorg
hello:
    .asciz "CFG-OK arm\n"
    .balign 4
marker:
    .ascii "AAAAAAAABBBBBBBBCCCCCCCCtailtail"
    .bss
    .balign 16
    .space 0x1000
stack_top:
