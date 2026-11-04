/* tests/syscall_truth.py's 64-bit workload: real calls through both entry instructions a 64-bit
 * task has, for the kernel's ftrace to record. 200 rounds of:
 *   syscall:   getpid, write(/dev/null, buf, i), read(/dev/zero, buf, i),
 *              mmap(0, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, i << 12), munmap
 *   int 0x80:  getpid, write, read, getppid (the i386 table)
 * then fork through int 0x80 (the child exits 7; waitpid) and through syscall (the child exits 8;
 * wait4), and exit_group(0). */
    .intel_syntax noprefix
    .globl _start

_start:
    mov eax, 2                      /* open("/dev/null", O_WRONLY) */
    lea rdi, [rip + devnull]
    mov esi, 1
    syscall
    mov r14, rax
    mov eax, 2                      /* open("/dev/zero", O_RDONLY) */
    lea rdi, [rip + devzero]
    xor esi, esi
    syscall
    mov r15, rax

    xor r12d, r12d
1:  mov eax, 39
    syscall
    mov eax, 1
    mov rdi, r14
    lea rsi, [rip + buf]
    mov rdx, r12
    syscall
    xor eax, eax
    mov rdi, r15
    lea rsi, [rip + buf]
    mov rdx, r12
    syscall
    mov eax, 9
    xor edi, edi
    mov esi, 4096
    mov edx, 1
    mov r10d, 0x22
    mov r8, -1
    mov r9, r12
    shl r9, 12
    syscall
    mov rdi, rax
    mov eax, 11
    mov esi, 4096
    syscall

    mov eax, 20
    int 0x80
    mov eax, 4
    mov ebx, r14d
    lea rcx, [rip + buf]
    mov edx, r12d
    int 0x80
    mov eax, 3
    mov ebx, r15d
    lea rcx, [rip + buf]
    mov edx, r12d
    int 0x80
    mov eax, 64
    int 0x80

    inc r12
    cmp r12, 200
    jb 1b

    mov eax, 2                      /* fork, i386 table */
    int 0x80
    test eax, eax
    jnz 2f
    mov eax, 1                      /* exit(7) */
    mov ebx, 7
    int 0x80
2:  mov ebx, eax                    /* waitpid(pid, 0, 0) */
    mov eax, 7
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov eax, 57                     /* fork */
    syscall
    test rax, rax
    jnz 3f
    mov eax, 60                     /* exit(8) */
    mov edi, 8
    syscall
3:  mov rdi, rax                    /* wait4(pid, 0, 0, 0) */
    mov eax, 61
    xor esi, esi
    xor edx, edx
    xor r10d, r10d
    syscall

    mov eax, 231
    xor edi, edi
    syscall
    hlt

    .data
devnull: .asciz "/dev/null"
devzero: .asciz "/dev/zero"
    .bss
buf: .skip 256
