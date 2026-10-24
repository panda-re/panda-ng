/* tests/asidstory.py's guest program: M times, hypercall (MAGIC, 5, getpid(), i, pc of the
 * hypercall), then sleep 10 ms; then exit(0). Several run at once. */
    .intel_syntax noprefix
    .set M, 20
    .globl _start
_start:
    mov eax, 173            /* ioperm(0x88, 4, 1): the hypercall is out 0x88 */
    mov edi, 0x88
    mov esi, 4
    mov edx, 1
    syscall
    mov eax, 39             /* getpid */
    syscall
    mov r12, rax
    xor ebx, ebx            /* i */
1:
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 5
    mov rdx, r12
    mov r10, rbx
    lea r8, [rip + hc]
hc:
    out 0x88, eax
    mov eax, 35             /* nanosleep(&ts, 0): let the others run */
    lea rdi, [rip + ts]
    xor esi, esi
    syscall
    inc ebx
    cmp ebx, M
    jne 1b
    mov eax, 60             /* exit(0) */
    xor edi, edi
    syscall
    .data
ts: .quad 0, 10000000       /* 10 ms */
