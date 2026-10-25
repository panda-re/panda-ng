/* tests/asidstory.py's guest program: start barrier (write a byte to fd 4, then read a byte from
 * fd 3; the shell holds both FIFOs open and releases the copies once all of them have written:
 * after the exec, so exec time can't serialize them; exit(2) if the read fails), then M times,
 * hypercall (MAGIC, 5, getpid(), i, pc of the hypercall), sleep 10 ms; then exit(0). Several run
 * at once. */
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
    mov eax, 1              /* write(4, &buf, 1): ready */
    mov edi, 4
    lea rsi, [rip + buf]
    mov edx, 1
    syscall
    xor eax, eax            /* read(3, &buf, 1): wait for the release */
    mov edi, 3
    lea rsi, [rip + buf]
    mov edx, 1
    syscall
    cmp rax, 1
    je 2f
    mov eax, 60             /* exit(2) */
    mov edi, 2
    syscall
2:
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
buf: .byte 0
