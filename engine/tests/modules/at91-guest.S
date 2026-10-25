/* at91sam9260 bare-metal check: DBGU chip id, PIT ticks through the AIC (polled, IRQs masked),
 * five ticks, then a semihosting exit. Same program for the in-tree board and the board .so. */
    .equ DBGU, 0xFFFFF200
    .equ AIC, 0xFFFFF000
    .equ PIT, 0xFFFFFD30
    .globl _start
    .arm
_start:
    ldr sp, =stack_top
    adr r0, hello
    bl puts
    ldr r4, =DBGU
    ldr r0, [r4, #0x40]          /* CIDR */
    bl puthex
    adr r0, nl
    bl puts
    ldr r5, =PIT
    ldr r1, =0x030fffff          /* PITEN | PITIEN | PIV */
    str r1, [r5, #0]
    ldr r6, =AIC
    mov r1, #2                   /* IECR: source 1, the system interrupt */
    str r1, [r6, #0x120]
    mov r7, #0
1:  ldr r1, [r6, #0x10c]         /* IPR */
    tst r1, #2
    beq 1b
    ldr r1, [r5, #8]             /* PIVR: acknowledge */
    add r7, r7, #1
    mov r0, #'T'
    bl putc
    add r0, r7, #'0'
    bl putc
    cmp r7, #5
    bne 1b
    adr r0, done
    bl puts
    mov r0, #0x18                /* semihosting SYS_EXIT */
    ldr r1, =0x20026             /* ADP_Stopped_ApplicationExit */
    svc 0x123456
2:  b 2b

putc:
    ldr r2, =DBGU
3:  ldr r3, [r2, #0x14]          /* SR: TXRDY */
    tst r3, #2
    beq 3b
    str r0, [r2, #0x1c]          /* THR */
    bx lr
puts:
    push {r4, lr}
    mov r4, r0
4:  ldrb r0, [r4], #1
    cmp r0, #0
    popeq {r4, pc}
    bl putc
    b 4b
puthex:
    push {r4, r5, lr}
    mov r4, r0
    mov r5, #28
5:  mov r0, r4, lsr r5
    and r0, r0, #15
    cmp r0, #10
    addlt r0, r0, #'0'
    addge r0, r0, #('a' - 10)
    bl putc
    subs r5, r5, #4
    bpl 5b
    pop {r4, r5, pc}
    .ltorg
hello: .asciz "AT91 bare-metal: cidr="
nl:    .asciz "\n"
done:  .asciz "\nDONE\n"
    .balign 4
    .bss
    .space 0x1000
stack_top:
