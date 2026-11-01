/* tests/hypercall_rr.py's guest program; see answers_x86_64.S. The answer comes back in x0. */
    .set N, 16
    .globl _start
_start:
    adr x9, buf
    str xzr, [x9]           /* fault buf's page in, writable, before the handler writes it */
    mov x19, #0             /* checksum */
    mov x20, #0             /* i */
loop:
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #1
    adr x2, buf
    mov x3, x20
    adr x4, hc1
hc1:
    .inst 0xd500501f        /* the hypercall instruction */
    add x19, x19, x0
    adr x9, buf
    ldr x10, [x9]
    add x19, x19, x10
    add x20, x20, #1
    cmp x20, #N
    b.ne loop
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #2
    mov x2, x19
    mov x3, #0
    adr x4, hc2
hc2:
    .inst 0xd500501f
    mov x0, #0              /* exit(0) */
    mov x8, #93
    svc #0
    .data
    .balign 8
buf:
    .quad 0
