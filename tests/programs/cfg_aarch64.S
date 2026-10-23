/* tests/machine.py's bare-metal program for the configurable machine (aarch64, cortex-a53, EL3).
 * Hypercall n passes (MAGIC, n, a, b, c) in x0..x4 (x8 = MAGIC). Layout: see tests/machine.py LAYOUT. */
    .equ ROM, 0x00000000
    .equ UART, 0x09000000
    .equ UNIMP, 0x0a000000
    .equ MAGIC, 0x50616e64
    .globl _start
_start:
    ldr x9, =stack_top
    mov sp, x9
    ldr x10, =ROM
    ldr x5, [x10]               /* the file-backed ROM's first 8 bytes */
    ldr x10, =UART
    mov w9, #0x301              /* UARTCR: UARTEN | TXE | RXE */
    str w9, [x10, #0x30]
    adr x11, hello
1:  ldrb w9, [x11], #1
    cbz w9, 2f
    str w9, [x10]               /* UARTDR */
    b 1b
2:  ldr x0, =MAGIC
    mov x8, x0
    mov x1, #1
    adr x2, marker
    mov x3, x5
    mov x4, sp
hc1:
    .inst 0xd500501f            /* the hypercall instruction */
    b 3f                        /* end the TB: what follows is translated with the callbacks on */
3:  ldr x9, =0x4b4f4445524f5453  /* "STOREDOK" */
    adr x12, marker
    str x9, [x12, #16]
    ldr x13, [x12, #16]
    ldr x10, =UNIMP
    str w9, [x10, #4]           /* an unimplemented peripheral: write ignored, read 0 */
    ldr w14, [x10, #8]
    ldr x0, =MAGIC
    mov x8, x0
    mov x1, #2
    adr x2, marker
    mov x3, x14
    mov x4, x13
hc2:
    .inst 0xd500501f
    ldr x10, =UART
    mov w9, #0x20               /* UARTIMSC: TX interrupt */
    str w9, [x10, #0x38]
    mov w9, #'!'
    str w9, [x10]
    isb
    mrs x3, isr_el1             /* bit 7: an IRQ is pending at the CPU */
    ldr x0, =MAGIC
    mov x8, x0
    mov x1, #3
    mov x2, #0
hc3:
    .inst 0xd500501f
4:  b 4b
    .ltorg
hello:
    .asciz "CFG-OK aarch64\n"
    .data
    .balign 16
marker:
    .ascii "AAAAAAAABBBBBBBBCCCCCCCCtailtail"
    .bss
    .balign 16
    .space 0x1000
stack_top:
