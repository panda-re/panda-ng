/* tests/syscall_cover.py's guest program (aarch64): every system call number through the entry
 * instruction a 64-bit task has, with the kernel refusing them all.
 *
 * It installs a seccomp filter that returns -1234 (SECCOMP_RET_ERRNO) for every call except
 * write and exit_group with 0x5afe5afe in argument 0's high 32 bits (the kernel truncates both
 * to int), then makes, numbering the calls seq = 0, 1, ...:
 *   svc #0,     nr 0..599                    (the table, and past its end)
 *   svc #0x5a,  nr 0..599                    (Linux ignores the immediate)
 *   svc #0,     nr from the specials table   (-1, 2^32, int overflow, ...)
 * Arguments: argument i of call seq is base[i] + seq, in all 64 bits of x0-x5.
 * Each call's x0 after it goes to rets[seq]; the program writes rets[0..seq) to stdout with
 * write(0x5afe5afe00000001, rets, 8 * seq) and exits with exit_group(0x5afe5afe00000000).
 * Exit status 1: seccomp couldn't be installed. */
    .text
    .globl _start

    .macro args64
    ldp x0, x1, [x21]
    add x0, x0, x19
    add x1, x1, x19
    ldp x2, x3, [x21, #16]
    add x2, x2, x19
    add x3, x3, x19
    ldp x4, x5, [x21, #32]
    add x4, x4, x19
    add x5, x5, x19
    .endm

    .macro marker reg, low
    movz \reg, #\low
    movk \reg, #0x5afe, lsl #32
    movk \reg, #0x5afe, lsl #48
    .endm

_start:
    mov x8, #167            /* prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) */
    mov x0, #38
    mov x1, #1
    mov x2, #0
    mov x3, #0
    mov x4, #0
    svc #0
    cbnz x0, fail
    mov x8, #277            /* seccomp(SECCOMP_SET_MODE_FILTER, 0, &fprog) */
    mov x0, #1
    mov x1, #0
    adrp x2, fprog
    add x2, x2, :lo12:fprog
    svc #0
    cbnz x0, fail

    mov x19, #0             /* seq */
    adrp x20, rets
    add x20, x20, :lo12:rets
    adrp x21, base
    add x21, x21, :lo12:base

    mov x22, #0             /* svc #0, 0..599 */
1:  args64
    mov x8, x22
    svc #0
    str x0, [x20, x19, lsl #3]
    add x19, x19, #1
    add x22, x22, #1
    cmp x22, #600
    b.lo 1b

    mov x22, #0             /* svc #0x5a, 0..599 */
1:  args64
    mov x8, x22
    svc #0x5a
    str x0, [x20, x19, lsl #3]
    add x19, x19, #1
    add x22, x22, #1
    cmp x22, #600
    b.lo 1b

    adrp x23, specials      /* svc #0, specials */
    add x23, x23, :lo12:specials
    mov x22, #0
1:  args64
    ldr x8, [x23, x22, lsl #3]
    svc #0
    str x0, [x20, x19, lsl #3]
    add x19, x19, #1
    add x22, x22, #1
    cmp x22, #(specials_end - specials) / 8
    b.lo 1b

    mov x8, #64             /* write(1, rets, 8 * seq) */
    marker x0, 1
    mov x1, x20
    lsl x2, x19, #3
    svc #0
    marker x0, 0
    b out
fail:
    marker x0, 1
out:
    mov x8, #94             /* exit_group */
    svc #0
    b .

    .data
    .balign 8
base:
    .quad 0xa0a1000011100000, 0xa2a3000022200000, 0xa4a5000033300000
    .quad 0xa6a7000044400000, 0xa8a9000055500000, 0xaaab000066600000
specials:
    .quad -1, 0x100000000, 0x7fffffff, 0x80000000, 0xffffffff, 0x8000000000000000
specials_end:
    .balign 8
    /* struct sock_fprog { unsigned short len; struct sock_filter *filter; } */
fprog:
    .short (filter_end - filter) / 8
    .short 0, 0, 0
    .quad filter
    /* struct sock_filter { u16 code; u8 jt, jf; u32 k; } on struct seccomp_data:
     * nr at 0, arch at 4, args[0]'s high half at 20 */
filter:
    .short 0x20; .byte 0, 0; .long 4                /* 0: ld arch */
    .short 0x15; .byte 0, 5; .long 0xc00000b7       /* 1: jeq AUDIT_ARCH_AARCH64, else 7 */
    .short 0x20; .byte 0, 0; .long 20               /* 2: ld args[0] >> 32 */
    .short 0x15; .byte 0, 3; .long 0x5afe5afe       /* 3: jeq marker, else 7 */
    .short 0x20; .byte 0, 0; .long 0                /* 4: ld nr */
    .short 0x15; .byte 2, 0; .long 94               /* 5: jeq exit_group: 8 */
    .short 0x15; .byte 1, 0; .long 64               /* 6: jeq write: 8 */
    .short 0x06; .byte 0, 0; .long 0x000504d2       /* 7: ret SECCOMP_RET_ERRNO | 1234 */
    .short 0x06; .byte 0, 0; .long 0x7fff0000       /* 8: ret SECCOMP_RET_ALLOW */
filter_end:

    .bss
    .balign 8
rets:
    .skip 8 * 4096
