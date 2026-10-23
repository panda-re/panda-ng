/* tests/blocks.py's guest program; see blocks_x86_64.S. */
    .set N, 1000
    .globl _start
_start:
    adr x2, labels          /* fault the label table's page in before hypercall 1 reads it */
    ldrb w9, [x2]
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #1
    mov x3, #0
    adr x4, hc1
hc1:
    .inst 0xd500501f        /* the hypercall instruction */
    b setup                 /* end the TB: what follows runs with the callbacks on */
setup:                      /* block "setup" */
    mov x9, #N
    mov x10, #0
    b loop                  /* end the block, so "loop" starts every iteration */
loop:                       /* block "loop": N times */
    add x10, x10, x9
    tbz x9, #0, even
odd:                        /* block "odd": N/2 times */
    add x10, x10, #3
even:                       /* block "even": N/2 times */
    subs x9, x9, #1
    b.ne loop
fault:                      /* block "fault": its third instruction faults (first write to the page) */
    adrp x11, page
    mov x12, #7
fault_store:
    str x12, [x11]
    add x12, x12, #1
    str x12, [x11, #8]
    b sys
sys:                        /* block "sys": ends in a system call (getpid) */
    mov x8, #172
    svc #0
    movz x0, #0x6e64        /* hypercall 2 */
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #2
    mov x2, #0
    mov x3, #0
    adr x4, hc2
hc2:
    .inst 0xd500501f
    mov x0, #0              /* exit(0) */
    mov x8, #93
    svc #0
    .data
labels:                     /* for the test: the blocks' addresses */
    .quad setup, loop, odd, even, fault, fault_store, sys
    .bss
    .balign 4096
    .skip 4096              /* a whole page between .data's tail and the one written */
page:
    .skip 4096
