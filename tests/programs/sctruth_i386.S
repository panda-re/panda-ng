/* tests/syscall_truth.py's 32-bit workload: real calls through both entry paths a 32-bit task has
 * (int 0x80, and the vDSO's __kernel_vsyscall: sysenter on Intel CPUs, syscall on AMD), for the
 * kernel's ftrace to record. 200 rounds, through each path, of:
 *   getpid, write(/dev/null, buf, i), read(/dev/zero, buf, i),
 *   mmap2(0, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, i) (argument 6 in ebp), munmap,
 *   getppid
 * then fork through each path (the children exit 7 and 8; waitpid), and exit_group(0).
 * Exit status 2: no AT_SYSINFO. */
    .intel_syntax noprefix
    .code32
    .globl _start

    .macro round call:vararg
    mov eax, 20
    \call
    mov eax, 4
    mov ebx, [devnull_fd]
    lea ecx, [buf]
    mov edx, [i]
    \call
    mov eax, 3
    mov ebx, [devzero_fd]
    lea ecx, [buf]
    mov edx, [i]
    \call
    mov eax, 192
    xor ebx, ebx
    mov ecx, 4096
    mov edx, 1
    mov esi, 0x22
    mov edi, -1
    mov ebp, [i]
    \call
    mov ebx, eax
    mov eax, 91
    mov ecx, 4096
    \call
    mov eax, 64
    \call
    .endm

    .macro forkwait status, call:vararg
    mov eax, 2
    \call
    test eax, eax
    jnz 1f
    mov eax, 1
    mov ebx, \status
    \call
1:  mov ebx, eax
    mov eax, 7
    xor ecx, ecx
    xor edx, edx
    \call
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

    mov eax, 5                      /* open("/dev/null", O_WRONLY) */
    lea ebx, [devnull]
    mov ecx, 1
    int 0x80
    mov [devnull_fd], eax
    mov eax, 5                      /* open("/dev/zero", O_RDONLY) */
    lea ebx, [devzero]
    xor ecx, ecx
    int 0x80
    mov [devzero_fd], eax

    mov dword ptr [i], 0
4:  round int 0x80
    round call dword ptr [vsys]
    inc dword ptr [i]
    cmp dword ptr [i], 200
    jb 4b

    forkwait 7, int 0x80
    forkwait 8, call dword ptr [vsys]
    xor ebx, ebx
    jmp out
noaux:
    mov ebx, 2
out:
    mov eax, 252
    int 0x80
    hlt

    .data
devnull: .asciz "/dev/null"
devzero: .asciz "/dev/zero"
    .bss
    .balign 4
vsys: .skip 4
devnull_fd: .skip 4
devzero_fd: .skip 4
i: .skip 4
buf: .skip 256
