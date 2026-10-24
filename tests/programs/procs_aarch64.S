/* tests/asidstory.py's guest program; see procs_x86_64.S. */
    .set M, 20
    .globl _start
_start:
    mov x8, #172            /* getpid */
    svc #0
    mov x20, x0
    mov x19, #0             /* i */
1:
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #5
    mov x2, x20
    mov x3, x19
    adr x4, hc
hc:
    .inst 0xd500501f        /* the hypercall instruction */
    adr x0, ts              /* nanosleep(&ts, 0) */
    mov x1, #0
    mov x8, #101
    svc #0
    add x19, x19, #1
    cmp x19, #M
    b.ne 1b
    mov x0, #0              /* exit(0) */
    mov x8, #93
    svc #0
    .data
ts: .quad 0, 10000000       /* 10 ms */
