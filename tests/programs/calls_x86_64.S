/* tests/calls.py's guest program: a known call graph, edges and hook counts. Hypercall n passes
 * (MAGIC, n, &labels, 0, pc of the hypercall). Between hypercalls 1 and 2:
 *   for i in 0..N-1: f(i)           f(i): if i odd: g(); h()      g(): h()      h(): return
 *   r(D)                            r(n): if n: r(n - 1)
 * Every label below starts a basic block (each follows a branch, call or return), so the test
 * knows the program's edges and how often each runs. f's block has a nop first: hook_mid, the
 * second instruction, is a hook address inside a block. Then exit(0). */
    .intel_syntax noprefix
    .set N, 20
    .set D, 4
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
    jmp setup
setup:
    xor ebx, ebx            /* i */
    jmp loop
loop:
    mov edi, ebx
    call f
ret_main:
    inc ebx
    cmp ebx, N
    jne loop
done:
    mov edi, D
    call r
ret_r0:
    jmp fin
f:
    nop
hook_mid:
    test edi, 1
    jz f_even
f_odd:
    call g
f_even:
    call h
f_ret:
    ret
g:
    call h
g_ret:
    ret
h:
    ret
r:
    test edi, edi
    jz r_base
r_rec:
    dec edi
    call r
r_ret:
    ret
r_base:
    ret
fin:
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 2
    lea rdx, [rip + labels]
    xor r10, r10
    lea r8, [rip + hc2]
hc2:
    out 0x88, eax
    mov eax, 60             /* exit(0) */
    xor edi, edi
    syscall
    .data
labels:                     /* for the test: N, D, then the labels' addresses */
    .quad N, D, setup, loop, ret_main, done, ret_r0, f, hook_mid, f_odd, f_even, f_ret, g, g_ret, h, r, r_rec, r_ret, r_base, fin
