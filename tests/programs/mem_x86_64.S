/* tests/mem.py's guest program. Hypercall n passes (MAGIC, n, &labels, 0, pc of the hypercall).
 * Between hypercalls 1 and 2: straight-line code storing 1, 2, 4, 8 and 16 bytes of known values
 * into buf and loading them back (the 16-byte copy from src16), loaded values left in r11-r14;
 * r15 is 0 at hypercall 2 and is the exit code (the test sets it). Between hypercalls 3 and 4:
 * one store to an untouched .bss page (a page fault). Then exit(r15). */
    .intel_syntax noprefix
    .globl _start
_start:
    mov eax, 173            /* ioperm(0x88, 4, 1): the hypercall is out 0x88 */
    mov edi, 0x88
    mov esi, 4
    mov edx, 1
    syscall
    mov al, [rip + labels]  /* fault in the label table, and buf writable, before hypercall 1 */
    lea rbx, [rip + buf]
    mov al, [rbx]
    mov [rbx], al
    mov al, [rip + src16]
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 1
    lea rdx, [rip + labels]
    xor r10, r10
    lea r8, [rip + hc1]
hc1:
    out 0x88, eax
    jmp m0                  /* end the TB: what follows is translated with the callbacks on */
m0:
    lea rbx, [rip + buf]
mw1: mov byte ptr [rbx], 0x11
mw2: mov word ptr [rbx + 2], 0x2233
mw3: mov dword ptr [rbx + 4], 0x44556677
    mov rax, 0x8899aabbccddeeff
mw4: mov [rbx + 8], rax
mr5: movdqu xmm0, [rip + src16]
mw5: movdqu [rbx + 16], xmm0
mr1: movzx r11d, byte ptr [rbx]
mr2: movzx r12d, word ptr [rbx + 2]
mr3: mov r13d, dword ptr [rbx + 4]
mr4: mov r14, [rbx + 8]
    xor r15d, r15d
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 2
    lea rdx, [rip + labels]
    xor r10, r10
    lea r8, [rip + hc2]
hc2:
    out 0x88, eax
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 3
    lea rdx, [rip + labels]
    xor r10, r10
    lea r8, [rip + hc3]
hc3:
    out 0x88, eax
    lea rbx, [rip + fresh]
mpf: mov [rbx], esi          /* first touch of this page: a page fault, then the store again */
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 4
    lea rdx, [rip + labels]
    xor r10, r10
    lea r8, [rip + hc4]
hc4:
    out 0x88, eax
    mov eax, 60             /* exit(r15) */
    mov rdi, r15
    syscall
    .data
labels:                     /* for the test */
    .quad buf, src16, mw1, mw2, mw3, mw4, mr5, mw5, mr1, mr2, mr3, mr4, mpf, fresh, m0, hc2
    .balign 16
src16:
    .ascii "0123456789abcdef"
buf:
    .skip 64
    .bss
    .balign 4096
    .skip 4096              /* a page between .data's tail and the one touched */
fresh:
    .skip 4096
