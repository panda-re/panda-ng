/* tests/mem.py's guest program; see mem_x86_64.S. Loaded values in x21-x24, the exit code in x25. */
    .globl _start
_start:
    adr x2, labels          /* fault in the label table, and buf writable, before hypercall 1 */
    ldrb w9, [x2]
    adr x20, buf
    ldrb w9, [x20]
    strb w9, [x20]
    adr x10, src16
    ldrb w9, [x10]
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #1
    mov x3, #0
    adr x4, hc1
hc1:
    .inst 0xd500501f        /* the hypercall instruction */
    b m0                    /* end the TB: what follows is translated with the callbacks on */
m0:
    adr x20, buf
    adr x10, src16
    mov w9, #0x11
mw1: strb w9, [x20]
    mov w9, #0x2233
mw2: strh w9, [x20, #2]
    movz w9, #0x6677
    movk w9, #0x4455, lsl #16
mw3: str w9, [x20, #4]
    movz x9, #0xeeff
    movk x9, #0xccdd, lsl #16
    movk x9, #0xaabb, lsl #32
    movk x9, #0x8899, lsl #48
mw4: str x9, [x20, #8]
mr5: ldr q0, [x10]
mw5: str q0, [x20, #16]
mr1: ldrb w21, [x20]
mr2: ldrh w22, [x20, #2]
mr3: ldr w23, [x20, #4]
mr4: ldr x24, [x20, #8]
    mov x25, #0
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #2
    adr x2, labels
    mov x3, #0
    adr x4, hc2
hc2:
    .inst 0xd500501f
    mov x1, #3
    adr x4, hc3
hc3:
    .inst 0xd500501f
    adrp x11, fresh
mpf: str w1, [x11]           /* first touch of this page: a page fault, then the store again */
    mov x1, #4
    adr x4, hc4
hc4:
    .inst 0xd500501f
    mov x0, x25             /* exit(x25) */
    mov x8, #93
    svc #0
    .data
labels:                     /* for the test */
    .quad buf, src16, mw1, mw2, mw3, mw4, mr5, mw5, mr1, mr2, mr3, mr4, mpf, fresh, m0, hc2
    .balign 16
src16:
    .ascii "0123456789abcdef"
buf:
    .skip 64
    .bss
    .balign 4096
    .skip 4096              /* a page between .data's tail and the one touched */
fresh:
    .skip 4096
