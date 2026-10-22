/* tests/api.py's guest program. Hypercall n passes (MAGIC, n, &marker, sp, pc of the hypercall);
 * between hypercalls 1 and 2 it stores "STOREDOK" at marker+16 and loads it, then it writes marker to stdout. */
    .globl _start
_start:
    adr x10, marker         /* fault the marker's page in, writable, before hypercall 1 */
    ldrb w9, [x10]
    strb w9, [x10]
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #1
    adr x2, marker
    mov x3, sp
    adr x4, hc1
hc1:
    .inst 0xd500501f        /* the hypercall instruction */
    b 1f                    /* end the TB: what follows is translated with the callbacks on */
1:
    movz x9, #0x5453        /* "STOREDOK" */
    movk x9, #0x524f, lsl #16
    movk x9, #0x4445, lsl #32
    movk x9, #0x4b4f, lsl #48
    adr x10, marker
    str x9, [x10, #16]
    ldr x11, [x10, #16]     /* and a load of it */
    mov x30, #0             /* the bl below, which ends this TB, fills x30: */
    bl 2f                   /* whether end_block_exec sees it says when that runs */
2:
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #2
    adr x2, marker
    mov x3, sp
    adr x4, hc2
hc2:
    .inst 0xd500501f
    mov x0, #1              /* write(1, marker, 32) */
    adr x1, marker
    mov x2, #32
    mov x8, #64
    svc #0
    mov x0, #0              /* exit(0) */
    mov x8, #93
    svc #0
    .data
marker:
    .ascii "AAAAAAAABBBBBBBBCCCCCCCCtailtail"
