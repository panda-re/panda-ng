/* tests/api.py's guest program. Hypercall n passes (MAGIC, n, &marker, sp, pc of the hypercall);
 * between hypercalls 1 and 2 it stores "STOREDOK" at marker+16 and loads it, then it writes marker to stdout. */
    .intel_syntax noprefix
    .globl _start
_start:
    mov eax, 173            /* ioperm(0x88, 4, 1): the hypercall is out 0x88 */
    mov edi, 0x88
    mov esi, 4
    mov edx, 1
    syscall
    mov al, [rip + marker]  /* fault the marker's page in, writable, before hypercall 1 */
    mov [rip + marker], al
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 1
    lea rdx, [rip + marker]
    mov r10, rsp
    lea r8, [rip + hc1]
hc1:
    out 0x88, eax
    jmp 1f                  /* end the TB: what follows is translated with the callbacks on */
1:
    movabs rax, 0x4b4f4445524f5453    /* "STOREDOK" */
    mov [rip + marker + 16], rax
    mov rcx, [rip + marker + 16]      /* and a load of it */
    mov qword ptr [rsp - 8], 0        /* the call below, which ends this TB, fills this slot: */
    call 2f                           /* whether end_block_exec sees it says when that runs */
2:
    pop rax
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 2
    lea rdx, [rip + marker]
    mov r10, rsp
    lea r8, [rip + hc2]
hc2:
    out 0x88, eax
    mov eax, 1              /* write(1, marker, 32) */
    mov edi, 1
    lea rsi, [rip + marker]
    mov edx, 32
    syscall
    mov eax, 60             /* exit(0) */
    xor edi, edi
    syscall
    .data
marker:
    .ascii "AAAAAAAABBBBBBBBCCCCCCCCtailtail"
