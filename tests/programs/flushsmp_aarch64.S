/* tests/flush_smp.py's guest program; see flushsmp_x86_64.S. The test stops the loop with x25. */
    .globl _start
_start:
    mov x25, #0
    mov x20, #0
loop:
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #1
    adr x2, after
    adr x3, mark
    adr x4, hc
hc:
    .inst 0xd500501f        /* the hypercall instruction */
    b after                 /* end the TB: `after` is the next block */
after:
    add x20, x20, #1
mark:
    cbz x25, loop
    mov x0, #0              /* exit(0) */
    mov x8, #93
    svc #0
