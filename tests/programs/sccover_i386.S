/* tests/syscall_cover.py's 32-bit guest program (i386, run by the x86_64 kernel): every system call
 * number through each entry instruction a 32-bit task has, with the kernel refusing them all.
 *
 * It installs a seccomp filter that returns -1234 (SECCOMP_RET_ERRNO) for every call except
 * write and exit_group from the i386 table with 0x5afe5afe in argument 6 (ebp), then makes,
 * numbering the calls seq = 0, 1, ...:
 *   int 0x80,             nr 0..599
 *   the vDSO's __kernel_vsyscall (AT_SYSINFO): sysenter on Intel CPUs, syscall on AMD; nr 0..599
 *   int 0x80,             nr from the specials table
 * Argument i of call seq is base[i] + seq (ebx, ecx, edx, esi, edi, ebp).
 * Each call's eax after it goes to rets[seq]; the program writes AT_SYSINFO and then rets[0..seq)
 * to stdout with write(1, out, 4 + 4 * seq), and exits with exit_group(0).
 * Exit status 1: seccomp couldn't be installed; 2: no AT_SYSINFO. */
    .intel_syntax noprefix
    .code32
    .globl _start

    .macro args32
    mov eax, [seq]
    lea ebx, [eax + 0x11100000]
    lea ecx, [eax + 0x22200000]
    lea edx, [eax + 0x33300000]
    lea esi, [eax + 0x44400000]
    lea edi, [eax + 0x55500000]
    lea ebp, [eax + 0x66600000]
    .endm

    .macro saveret
    mov ecx, [seq]
    mov [rets + ecx * 4], eax
    inc dword ptr [seq]
    .endm

_start:
    mov eax, [esp]                  /* argc; then argv, 0, envp, 0, auxv */
    lea esi, [esp + eax * 4 + 8]
1:  mov eax, [esi]
    add esi, 4
    test eax, eax
    jnz 1b
2:  mov eax, [esi]
    test eax, eax
    jz noaux
    cmp eax, 32                     /* AT_SYSINFO */
    je 3f
    add esi, 8
    jmp 2b
3:  mov eax, [esi + 4]
    mov [vsys], eax

    mov eax, 172                    /* prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) */
    mov ebx, 38
    mov ecx, 1
    xor edx, edx
    xor esi, esi
    xor edi, edi
    int 0x80
    test eax, eax
    jnz fail
    mov eax, 354                    /* seccomp(SECCOMP_SET_MODE_FILTER, 0, &fprog) */
    mov ebx, 1
    xor ecx, ecx
    lea edx, [fprog]
    int 0x80
    test eax, eax
    jnz fail

    mov dword ptr [nr], 0           /* int 0x80, 0..599 */
1:  args32
    mov eax, [nr]
    int 0x80
    saveret
    inc dword ptr [nr]
    cmp dword ptr [nr], 600
    jb 1b

    mov dword ptr [nr], 0           /* the vDSO, 0..599 */
1:  args32
    mov eax, [nr]
    call dword ptr [vsys]
    saveret
    inc dword ptr [nr]
    cmp dword ptr [nr], 600
    jb 1b

    mov dword ptr [nr], 0           /* int 0x80, specials */
1:  args32
    mov eax, [nr]
    mov eax, [specials + eax * 4]
    int 0x80
    saveret
    inc dword ptr [nr]
    cmp dword ptr [nr], (specials_end - specials) / 4
    jb 1b

    mov eax, 4                      /* write(1, &vsys, 4 + 4 * seq) */
    mov ebx, 1
    lea ecx, [vsys]
    mov edx, [seq]
    lea edx, [edx * 4 + 4]
    mov ebp, 0x5afe5afe
    int 0x80
    xor ebx, ebx
    jmp out
noaux:
    mov ebx, 2
    jmp out
fail:
    mov ebx, 1
out:
    mov eax, 252                    /* exit_group */
    mov ebp, 0x5afe5afe
    int 0x80
    hlt

    .data
    .balign 4
specials:
    .long -1, 0x7fffffff, 0x80000000, 0x40000000
specials_end:
    /* struct sock_fprog { unsigned short len; struct sock_filter *filter; } */
fprog:
    .short (filter_end - filter) / 8
    .short 0
    .long filter
    /* struct sock_filter { u16 code; u8 jt, jf; u32 k; } on struct seccomp_data:
     * nr at 0, arch at 4, args[5]'s low half at 56 */
filter:
    .short 0x20; .byte 0, 0; .long 4                /* 0: ld arch */
    .short 0x15; .byte 0, 5; .long 0x40000003       /* 1: jeq AUDIT_ARCH_I386, else 7 */
    .short 0x20; .byte 0, 0; .long 56               /* 2: ld args[5] */
    .short 0x15; .byte 0, 3; .long 0x5afe5afe       /* 3: jeq marker, else 7 */
    .short 0x20; .byte 0, 0; .long 0                /* 4: ld nr */
    .short 0x15; .byte 2, 0; .long 252              /* 5: jeq exit_group: 8 */
    .short 0x15; .byte 1, 0; .long 4                /* 6: jeq write: 8 */
    .short 0x06; .byte 0, 0; .long 0x000504d2       /* 7: ret SECCOMP_RET_ERRNO | 1234 */
    .short 0x06; .byte 0, 0; .long 0x7fff0000       /* 8: ret SECCOMP_RET_ALLOW */
filter_end:

    .bss
    .balign 4
seq:
    .skip 4
nr:
    .skip 4
vsys:                               /* written out first, then rets */
    .skip 4
rets:
    .skip 4 * 4096
