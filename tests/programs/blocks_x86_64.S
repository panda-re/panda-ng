/* tests/blocks.py's guest program. Hypercall n passes (MAGIC, n, &labels, 0, pc of the hypercall);
 * between hypercalls 1 and 2: a loop of N iterations whose blocks run a known number of times,
 * a block that faults part way through (first write to a .bss page), and a block ending in a
 * system call. Then exit(0). */
    .intel_syntax noprefix
    .set N, 1000
    .globl _start
_start:
    mov eax, 173            /* ioperm(0x88, 4, 1): the hypercall is out 0x88 */
    mov edi, 0x88
    mov esi, 4
    mov edx, 1
    syscall
    mov al, [rip + labels]  /* fault the label table's page in before hypercall 1 reads it */
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 1
    lea rdx, [rip + labels]
    xor r10, r10
    lea r8, [rip + hc1]
hc1:
    out 0x88, eax
    jmp setup               /* end the TB: what follows runs with the callbacks on */
setup:                      /* block "setup" */
    mov ecx, N
    xor eax, eax
    jmp loop                /* end the block, so "loop" starts every iteration */
loop:                       /* block "loop": N times */
    add eax, ecx
    test ecx, 1
    jz even
odd:                        /* block "odd": N/2 times (falls into even's instructions) */
    add eax, 3
even:                       /* block "even": N/2 times */
    dec ecx
    jnz loop
fault:                      /* block "fault": its third instruction faults (first write to the page) */
    lea rdi, [rip + page]
    mov edx, 7
fault_store:
    mov [rdi], edx
    add edx, 1
    mov [rdi + 8], edx
    jmp sys
sys:                        /* block "sys": ends in a system call (getpid) */
    mov eax, 39
    syscall
    mov edi, 0x50616e64     /* hypercall 2 */
    mov eax, edi
    mov esi, 2
    xor edx, edx
    xor r10, r10
    lea r8, [rip + hc2]
hc2:
    out 0x88, eax
    mov eax, 60             /* exit(0) */
    xor edi, edi
    syscall
    .data
labels:                     /* for the test: the blocks' addresses */
    .quad setup, loop, odd, even, fault, fault_store, sys
    .bss
    .balign 4096
    .skip 4096              /* a whole page between .data's tail and the one written */
page:
    .skip 4096
