/* tests/fastcov.py's program: hypercall 1 (MAGIC, 1, &input), then 64 rounds over input's bits,
 * each taking one of two paths by the bit, then hypercall 2 and exit(0). The host snapshots at
 * hypercall 1, writes input after each restore, and ends each lap at hypercall 2. */
    .globl _start
_start:
    adr x9, input
    str xzr, [x9]           /* fault input's page in (a private copy) for the host to write */
    movz x0, #0x6e64        /* hypercall 1 */
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #1
    adr x2, input
    mov x3, #0
    adr x4, hc1
hc1:
    .inst 0xd500501f
    b 1f                    /* end the block: the VM stops here, before input is read */
1:
    adr x9, input
    ldr x10, [x9]
    mov x11, #0
    mov x12, #0
round:
    lsr x13, x10, x11
    tbz x13, #0, zero
    add x12, x12, #3        /* bit set */
    b next
zero:
    eor x12, x12, #6        /* bit clear */
next:
    add x11, x11, #1
    cmp x11, #64
    b.ne round
    movz x0, #0x6e64        /* hypercall 2 */
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #2
    mov x2, x12
    mov x3, #0
    adr x4, hc2
hc2:
    .inst 0xd500501f
    mov x0, #0              /* exit(0) */
    mov x8, #93
    svc #0
    .data
    .balign 8
input:
    .quad 0
