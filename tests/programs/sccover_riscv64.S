/* tests/syscall_cover.py's guest program (riscv64): every system call number through the entry
 * instruction a 64-bit task has, with the kernel refusing them all.
 *
 * It installs a seccomp filter that returns -1234 (SECCOMP_RET_ERRNO) for every call except
 * write and exit_group with 0x5afe5afe in argument 0's high 32 bits (the kernel truncates both
 * to int), then makes, numbering the calls seq = 0, 1, ...:
 *   ecall,  nr 0..599                    (the table, and past its end)
 *   ecall,  nr from the specials table   (-1, 2^32, int overflow, ...)
 * Arguments: argument i of call seq is base[i] + seq, in all 64 bits of a0-a5.
 * Each call's a0 after it goes to rets[seq]; the program writes rets[0..seq) to stdout with
 * write(0x5afe5afe00000001, rets, 8 * seq) and exits with exit_group(0x5afe5afe00000000).
 * Exit status 1: seccomp couldn't be installed. */
    .option norvc
    .option norelax
    .text
    .globl _start

    .macro args64
    ld a0, 0(s3)
    add a0, a0, s1
    ld a1, 8(s3)
    add a1, a1, s1
    ld a2, 16(s3)
    add a2, a2, s1
    ld a3, 24(s3)
    add a3, a3, s1
    ld a4, 32(s3)
    add a4, a4, s1
    ld a5, 40(s3)
    add a5, a5, s1
    .endm

    .macro saveret
    slli t0, s1, 3
    add t0, s2, t0
    sd a0, 0(t0)
    addi s1, s1, 1
    .endm

_start:
    li a7, 167              /* prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) */
    li a0, 38
    li a1, 1
    li a2, 0
    li a3, 0
    li a4, 0
    ecall
    bnez a0, fail
    li a7, 277              /* seccomp(SECCOMP_SET_MODE_FILTER, 0, &fprog) */
    li a0, 1
    li a1, 0
    la a2, fprog
    ecall
    bnez a0, fail

    li s1, 0                /* seq */
    la s2, rets
    la s3, base

    li s4, 0                /* ecall, 0..599 */
    li s6, 600
1:  args64
    mv a7, s4
    ecall
    saveret
    addi s4, s4, 1
    bltu s4, s6, 1b

    la s5, specials         /* ecall, specials */
    li s4, 0
    li s6, 6                /* the specials */
1:  args64
    slli t0, s4, 3
    add t0, s5, t0
    ld a7, 0(t0)
    ecall
    saveret
    addi s4, s4, 1
    bltu s4, s6, 1b

    li a7, 64               /* write(1, rets, 8 * seq) */
    li a0, 0x5afe5afe00000001
    mv a1, s2
    slli a2, s1, 3
    ecall
    li a0, 0x5afe5afe00000000
    j out
fail:
    li a0, 0x5afe5afe00000001
out:
    li a7, 94               /* exit_group */
    ecall
1:  j 1b

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
    .short 0x15; .byte 0, 5; .long 0xc00000f3       /* 1: jeq AUDIT_ARCH_RISCV64, else 7 */
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
