/* tests/zeroing.py's guest program: DC ZVA on a buffer filled with 0xa5, between hypercalls 1 and 2.
   x26 holds DCZID_EL0 (block size, and whether DC ZVA is permitted at EL0). */
    .globl _start
_start:
    adr x2, labels          /* fault in the label table, and zbuf writable, before hypercall 1 */
    ldrb w9, [x2]
    adr x20, zbuf
    ldrb w9, [x20]
    strb w9, [x20]
    mrs x26, dczid_el0
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
    add x9, x20, #264       /* inside the block at zbuf+256: DC ZVA aligns down */
mz: dc zva, x9
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #2
    adr x2, labels
    mov x3, #0
    adr x4, hc2
hc2:
    .inst 0xd500501f
    mov x0, #0              /* exit(0) */
    mov x8, #93
    svc #0
    .data
labels:                     /* for the test */
    .quad zbuf, m0, mz, hc2
    .balign 2048            /* any DCZID block size up to 2 KiB is aligned */
zbuf:
    .fill 1024, 1, 0xa5
