/* tests/flush_smp.py's guest program, one copy pinned to each vCPU. A loop: hypercall
 * (MAGIC, 1, &after, &mark, pc of the hypercall), then the block at `after`, until the test sets
 * r15; then exit(0). `mark` is the block's second instruction. */
    .intel_syntax noprefix
    .globl _start
_start:
    mov eax, 173            /* ioperm(0x88, 4, 1): the hypercall is out 0x88 */
    mov edi, 0x88
    mov esi, 4
    mov edx, 1
    syscall
    xor r15d, r15d
    xor ebx, ebx
loop:
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 1
    lea rdx, [rip + after]
    lea r10, [rip + mark]
    lea r8, [rip + hc]
hc:
    out 0x88, eax
    jmp after               /* end the TB: `after` is the next block */
after:
    inc rbx
mark:
    test r15, r15
    jz loop
    mov eax, 60             /* exit(0) */
    xor edi, edi
    syscall
