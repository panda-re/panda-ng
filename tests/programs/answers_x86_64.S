/* tests/hypercall_rr.py's guest program. N times: hypercall (MAGIC, 1, &buf, i, pc), whose
 * handler writes a quad into buf and returns a value (rax); both go into a checksum. Then
 * hypercall (MAGIC, 2, checksum, 0, pc) and exit(0). */
    .intel_syntax noprefix
    .set N, 16
    .globl _start
_start:
    mov eax, 173            /* ioperm(0x88, 4, 1): the hypercall is out 0x88 */
    mov edi, 0x88
    mov esi, 4
    mov edx, 1
    syscall
    mov qword ptr [rip + buf], 0    /* fault buf's page in, writable, before the handler writes it */
    xor r12, r12            /* checksum */
    xor ebx, ebx            /* i */
loop:
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 1
    lea rdx, [rip + buf]
    mov r10, rbx
    lea r8, [rip + hc1]
hc1:
    out 0x88, eax
    add r12, rax
    add r12, [rip + buf]
    inc ebx
    cmp ebx, N
    jne loop
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 2
    mov rdx, r12
    xor r10, r10
    lea r8, [rip + hc2]
hc2:
    out 0x88, eax
    mov eax, 60             /* exit(0) */
    xor edi, edi
    syscall
    .data
buf:
    .quad 0
