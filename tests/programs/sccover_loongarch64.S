/* tests/syscall_cover.py's guest program (loongarch64): every system call number through the
 * entry instruction a task has, with the kernel refusing them all.
 *
 * It installs a seccomp filter that returns -1234 (SECCOMP_RET_ERRNO) for every call except
 * write and exit_group with 0x5afe5afe in argument 0's high 32 bits (the kernel truncates both
 * to int), then makes, numbering the calls seq = 0, 1, ...:
 *   syscall 0,     nr 0..599                    (the table, and past its end)
 *   syscall 0x5a,  nr 0..599                    (Linux ignores the code)
 *   syscall 0,     nr from the specials table   (-1, 2^32, int overflow, ...)
 * Arguments: argument i of call seq is base[i] + seq, in all 64 bits of a0-a5.
 * Each call's a0 after it goes to rets[seq]; the program writes rets[0..seq) to stdout with
 * write(0x5afe5afe00000001, rets, 8 * seq) and exits with exit_group(0x5afe5afe00000000).
 * Exit status 1: seccomp couldn't be installed. */
    .text
    .globl _start

    .macro args64
    ld.d $a0, $s3, 0
    add.d $a0, $a0, $s1
    ld.d $a1, $s3, 8
    add.d $a1, $a1, $s1
    ld.d $a2, $s3, 16
    add.d $a2, $a2, $s1
    ld.d $a3, $s3, 24
    add.d $a3, $a3, $s1
    ld.d $a4, $s3, 32
    add.d $a4, $a4, $s1
    ld.d $a5, $s3, 40
    add.d $a5, $a5, $s1
    .endm

    .macro saveret
    alsl.d $t0, $s1, $s2, 3
    st.d $a0, $t0, 0
    addi.d $s1, $s1, 1
    .endm

_start:
    ori $a7, $zero, 167     /* prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) */
    ori $a0, $zero, 38
    ori $a1, $zero, 1
    move $a2, $zero
    move $a3, $zero
    move $a4, $zero
    syscall 0
    bnez $a0, fail
    ori $a7, $zero, 277     /* seccomp(SECCOMP_SET_MODE_FILTER, 0, &fprog) */
    ori $a0, $zero, 1
    move $a1, $zero
    la.local $a2, fprog
    syscall 0
    bnez $a0, fail

    move $s1, $zero         /* seq */
    la.local $s2, rets
    la.local $s3, base

    move $s4, $zero         /* syscall 0, 0..599 */
    ori $s6, $zero, 600
1:  args64
    move $a7, $s4
    syscall 0
    saveret
    addi.d $s4, $s4, 1
    bltu $s4, $s6, 1b

    move $s4, $zero         /* syscall 0x5a, 0..599 */
1:  args64
    move $a7, $s4
    syscall 0x5a
    saveret
    addi.d $s4, $s4, 1
    bltu $s4, $s6, 1b

    la.local $s5, specials  /* syscall 0, specials */
    move $s4, $zero
    ori $s6, $zero, 6       /* the specials */
1:  args64
    alsl.d $t0, $s4, $s5, 3
    ld.d $a7, $t0, 0
    syscall 0
    saveret
    addi.d $s4, $s4, 1
    bltu $s4, $s6, 1b

    ori $a7, $zero, 64      /* write(1, rets, 8 * seq) */
    li.d $a0, 0x5afe5afe00000001
    move $a1, $s2
    slli.d $a2, $s1, 3
    syscall 0
    li.d $a0, 0x5afe5afe00000000
    b out
fail:
    li.d $a0, 0x5afe5afe00000001
out:
    ori $a7, $zero, 94      /* exit_group */
    syscall 0
1:  b 1b

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
    .short 0x15; .byte 0, 5; .long 0xc0000102       /* 1: jeq AUDIT_ARCH_LOONGARCH64, else 7 */
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
