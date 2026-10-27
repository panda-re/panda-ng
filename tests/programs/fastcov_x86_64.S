/* tests/fastcov.py's program; see fastcov_aarch64.S. The hypercall is out 0x88 (ioperm first). */
    .intel_syntax noprefix
    .globl _start
_start:
    mov eax, 173            /* ioperm(0x88, 4, 1) */
    mov edi, 0x88
    mov esi, 4
    mov edx, 1
    syscall
    mov qword ptr [rip + input], 0   /* fault input's page in (a private copy) for the host */
    mov edi, 0x50616e64     /* hypercall 1 */
    mov eax, edi
    mov esi, 1
    lea rdx, [rip + input]
    xor r10, r10
    lea r8, [rip + hc1]
hc1:
    out 0x88, eax
    jmp 1f                    /* end the block: the VM stops here, before input is read */
1:
    mov r12, [rip + input]
    xor ecx, ecx
    xor ebx, ebx
round:
    bt r12, rcx
    jnc zero
    add rbx, 3              /* bit set */
    jmp next
zero:
    xor rbx, 5              /* bit clear */
next:
    inc ecx
    cmp ecx, 64
    jne round
    mov edi, 0x50616e64     /* hypercall 2 */
    mov eax, edi
    mov esi, 2
    mov rdx, rbx
    xor r10, r10
    lea r8, [rip + hc2]
hc2:
    out 0x88, eax
    mov eax, 60             /* exit(0) */
    xor edi, edi
    syscall
    .data
    .balign 8
input:
    .quad 0
