/* tests/calls.py's guest program; see calls_x86_64.S. Non-leaf functions keep lr on the stack. */
    .set N, 20
    .set D, 4
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
    b setup
setup:
    mov x19, #0             /* i */
    b loop
loop:
    mov x0, x19
    bl f
ret_main:
    add x19, x19, #1
    cmp x19, #N
    b.ne loop
done:
    mov x0, #D
    bl r
ret_r0:
    b fin
f:
    stp x29, x30, [sp, #-16]!
hook_mid:
    tbz x0, #0, f_even
f_odd:
    bl g
f_even:
    bl h
f_ret:
    ldp x29, x30, [sp], #16
    ret
g:
    stp x29, x30, [sp, #-16]!
    bl h
g_ret:
    ldp x29, x30, [sp], #16
    ret
h:
    ret
r:
    cbz x0, r_base
r_rec:
    stp x29, x30, [sp, #-16]!
    sub x0, x0, #1
    bl r
r_ret:
    ldp x29, x30, [sp], #16
    ret
r_base:
    ret
fin:
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
labels:                     /* for the test: N, D, then the labels' addresses */
    .quad N, D, setup, loop, ret_main, done, ret_r0, f, hook_mid, f_odd, f_even, f_ret, g, g_ret, h, r, r_rec, r_ret, r_base, fin
