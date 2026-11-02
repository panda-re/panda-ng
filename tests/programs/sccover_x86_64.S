/* tests/syscall_cover.py's guest program (x86_64): every system call number through each entry
 * instruction a 64-bit task has, with the kernel refusing them all.
 *
 * It installs a seccomp filter that returns -1234 (SECCOMP_RET_ERRNO) for every call except
 * write and exit_group from the x86_64 table with 0x5afe5afe in argument 0's high 32 bits (the
 * kernel truncates both to int), then makes, numbering the calls seq = 0, 1, ...:
 *   syscall,   nr 0..599 but 335              (64-bit table, and past its end; uretprobe (335)
 *                                             bypasses seccomp and raises SIGILL outside a
 *                                             uretprobe trampoline)
 *   syscall,   nr 0x40000000 + 0..599         (x32)
 *   syscall,   nr from the specials table     (-1, 2^32, int overflow, ...)
 *   int 0x80,  nr 0..599                      (i386 table, from a 64-bit task)
 * Arguments: argument i of call seq is base[i] + seq, in all 64 bits of its register (int 0x80
 * takes ebx, ecx, edx, esi, edi, ebp: the kernel and the trap see the low 32 bits).
 * Each call's rax after it goes to rets[seq]; the program writes rets[0..seq) to stdout with
 * write(0x5afe5afe00000001, rets, 8 * seq) and exits with exit_group(0x5afe5afe00000000).
 * Exit status 1: seccomp couldn't be installed. */
    .intel_syntax noprefix
    .globl _start

    .macro args64
    mov rdi, [rip + base + 0]
    add rdi, r12
    mov rsi, [rip + base + 8]
    add rsi, r12
    mov rdx, [rip + base + 16]
    add rdx, r12
    mov r10, [rip + base + 24]
    add r10, r12
    mov r8, [rip + base + 32]
    add r8, r12
    mov r9, [rip + base + 40]
    add r9, r12
    .endm

    .macro args80
    mov rbx, [rip + base + 0]
    add rbx, r12
    mov rcx, [rip + base + 8]
    add rcx, r12
    mov rdx, [rip + base + 16]
    add rdx, r12
    mov rsi, [rip + base + 24]
    add rsi, r12
    mov rdi, [rip + base + 32]
    add rdi, r12
    mov rbp, [rip + base + 40]
    add rbp, r12
    .endm

_start:
    mov eax, 157            /* prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) */
    mov edi, 38
    mov esi, 1
    xor edx, edx
    xor r10d, r10d
    xor r8d, r8d
    syscall
    test rax, rax
    jnz fail
    mov eax, 317            /* seccomp(SECCOMP_SET_MODE_FILTER, 0, &fprog) */
    mov edi, 1
    xor esi, esi
    lea rdx, [rip + fprog]
    syscall
    test rax, rax
    jnz fail

    xor r12d, r12d          /* seq */
    lea r15, [rip + rets]

    xor r13d, r13d          /* syscall, 0..599 but uretprobe */
1:  cmp r13, 335
    je 2f
    args64
    mov rax, r13
    syscall
    mov [r15 + r12 * 8], rax
    inc r12
2:  inc r13
    cmp r13, 600
    jb 1b

    mov r13d, 0x40000000    /* syscall, x32 0..599 */
1:  args64
    mov rax, r13
    syscall
    mov [r15 + r12 * 8], rax
    inc r12
    inc r13
    cmp r13, 0x40000000 + 600
    jb 1b

    xor r14d, r14d          /* syscall, specials */
1:  args64
    lea rax, [rip + specials]
    mov rax, [rax + r14 * 8]
    syscall
    mov [r15 + r12 * 8], rax
    inc r12
    inc r14
    cmp r14, (specials_end - specials) / 8
    jb 1b

    xor r13d, r13d          /* int 0x80, 0..599 */
1:  args80
    mov rax, r13
    mov r14, 0x7777777700000000     /* high bits the kernel and trap ignore */
    or rax, r14
    int 0x80
    mov [r15 + r12 * 8], rax
    inc r12
    inc r13
    cmp r13, 600
    jb 1b

    mov eax, 1              /* write(1, rets, 8 * seq) */
    mov rdi, 0x5afe5afe00000001
    mov rsi, r15
    lea rdx, [r12 * 8]
    syscall
    mov rdi, 0x5afe5afe00000000
    jmp out
fail:
    mov rdi, 0x5afe5afe00000001
out:
    mov eax, 231            /* exit_group */
    syscall
    hlt

    .data
    .balign 8
base:
    .quad 0xa0a1000011100000, 0xa2a3000022200000, 0xa4a5000033300000
    .quad 0xa6a7000044400000, 0xa8a9000055500000, 0xaaab000066600000
specials:
    .quad -1, 0x100000000, 0x7fffffff, 0x80000000, 0xffffffff, 0x8000000000000000
    .quad 0x3fffffff
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
    .short 0x15; .byte 0, 5; .long 0xc000003e       /* 1: jeq AUDIT_ARCH_X86_64, else 7 */
    .short 0x20; .byte 0, 0; .long 20               /* 2: ld args[0] >> 32 */
    .short 0x15; .byte 0, 3; .long 0x5afe5afe       /* 3: jeq marker, else 7 */
    .short 0x20; .byte 0, 0; .long 0                /* 4: ld nr */
    .short 0x15; .byte 2, 0; .long 231              /* 5: jeq exit_group: 8 */
    .short 0x15; .byte 1, 0; .long 1                /* 6: jeq write: 8 */
    .short 0x06; .byte 0, 0; .long 0x000504d2       /* 7: ret SECCOMP_RET_ERRNO | 1234 */
    .short 0x06; .byte 0, 0; .long 0x7fff0000       /* 8: ret SECCOMP_RET_ALLOW */
filter_end:

    .bss
    .balign 8
rets:
    .skip 8 * 4096
