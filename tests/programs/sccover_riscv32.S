/* tests/syscall_cover.py's 32-bit guest program (riscv32, run by the riscv64 kernel as a compat
 * task, mstatus.UXL = 32): every system call number through ecall, with the kernel refusing them
 * all.
 *
 * It installs a seccomp filter that returns -1234 (SECCOMP_RET_ERRNO) for every call except
 * write and exit_group with 0x5afe5afe in argument 6 (a5), then makes, numbering the calls
 * seq = 0, 1, ...:
 *   ecall,  nr 0..599                    (the table, and past its end)
 *   ecall,  nr from the specials table
 * Argument i of call seq is base[i] + seq (a0-a5).
 * Each call's a0 after it goes to rets[seq]; the program writes rets[0..seq) to stdout with
 * write(1, rets, 4 * seq) and exits with exit_group(0), both with the marker in a5.
 * Exit status 1: seccomp couldn't be installed. */
    .option norvc
    .option norelax
    .text
    .globl _start

    .macro args32
    li a0, 0x11100000
    add a0, a0, s1
    li a1, 0x22200000
    add a1, a1, s1
    li a2, 0x33300000
    add a2, a2, s1
    li a3, 0x44400000
    add a3, a3, s1
    li a4, 0x55500000
    add a4, a4, s1
    li a5, 0x66600000
    add a5, a5, s1
    .endm

    .macro saveret
    slli t0, s1, 2
    add t0, s2, t0
    sw a0, 0(t0)
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

    li s4, 0                /* ecall, 0..599 */
    li s6, 600
1:  args32
    mv a7, s4
    ecall
    saveret
    addi s4, s4, 1
    bltu s4, s6, 1b

    la s5, specials         /* ecall, specials */
    li s4, 0
    li s6, 4                /* the specials */
1:  args32
    slli t0, s4, 2
    add t0, s5, t0
    lw a7, 0(t0)
    ecall
    saveret
    addi s4, s4, 1
    bltu s4, s6, 1b

    li a7, 64               /* write(1, rets, 4 * seq) */
    li a0, 1
    mv a1, s2
    slli a2, s1, 2
    li a5, 0x5afe5afe
    ecall
    li a0, 0
    j out
fail:
    li a0, 1
out:
    li a5, 0x5afe5afe
    li a7, 94               /* exit_group */
    ecall
1:  j 1b

    .data
    .balign 4
specials:
    .long -1, 0x7fffffff, 0x80000000, 0x40000000
specials_end:
    .balign 4
    /* struct sock_fprog { unsigned short len; struct sock_filter *filter; } */
fprog:
    .short (filter_end - filter) / 8
    .short 0
    .long filter
    /* struct sock_filter on struct seccomp_data: nr at 0, arch at 4, args[5] at 56. The arch:
     * a riscv64 kernel reports AUDIT_ARCH_RISCV64 for its compat tasks too (syscall_get_arch). */
filter:
    .short 0x20; .byte 0, 0; .long 4                /* 0: ld arch */
    .short 0x15; .byte 1, 0; .long 0x400000f3       /* 1: jeq AUDIT_ARCH_RISCV32: 3 */
    .short 0x15; .byte 0, 5; .long 0xc00000f3       /* 2: jeq AUDIT_ARCH_RISCV64, else 8 */
    .short 0x20; .byte 0, 0; .long 56               /* 3: ld args[5] */
    .short 0x15; .byte 0, 3; .long 0x5afe5afe       /* 4: jeq marker, else 8 */
    .short 0x20; .byte 0, 0; .long 0                /* 5: ld nr */
    .short 0x15; .byte 2, 0; .long 94               /* 6: jeq exit_group: 9 */
    .short 0x15; .byte 1, 0; .long 64               /* 7: jeq write: 9 */
    .short 0x06; .byte 0, 0; .long 0x000504d2       /* 8: ret SECCOMP_RET_ERRNO | 1234 */
    .short 0x06; .byte 0, 0; .long 0x7fff0000       /* 9: ret SECCOMP_RET_ALLOW */
filter_end:

    .bss
    .balign 4
rets:
    .skip 4 * 4096
