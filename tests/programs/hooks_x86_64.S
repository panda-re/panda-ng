/* tests/hooks.py's guest program. Hypercall (MAGIC, op, &labels, 0, pc of the hypercall):
 * op 1, then three phases separated by op 3 and op 4, then op 2 and exit(0). Each phase runs R
 * rounds of: body (three adjacent instructions h0 h1 h2 in one block), pic at its own address,
 * pic again through a second mapping of this file ALT_DELTA above the first (the same code at
 * another virtual address), cross (a block that runs from one page into the next: x0 on the first page, xs an
 * instruction that straddles the boundary, x1 on the second page) and getpid. */
    .intel_syntax noprefix
    .set R, 100
    .set ALT_DELTA, 0x200000
    .globl _start
_start:
    mov eax, 173            /* ioperm(0x88, 4, 1): the hypercall is out 0x88 */
    mov edi, 0x88
    mov esi, 4
    mov edx, 1
    syscall
    mov eax, 2              /* open("/proc/self/exe", O_RDONLY) */
    lea rdi, [rip + exe]
    xor esi, esi
    xor edx, edx
    syscall
    mov r8, rax             /* mmap(ALT, 0x3000, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED, fd, 0) */
    mov eax, 9
    lea rdi, [rip + __ehdr_start]   /* ALT: the file's first page, ALT_DELTA further */
    add rdi, ALT_DELTA
    mov esi, 0x3000
    mov edx, 5
    mov r10d, 0x12
    xor r9d, r9d
    syscall
    mov al, [rip + labels]  /* fault the label table's page in before hypercall 1 reads it */
    mov ebx, 1
    call hc
    call phase
    mov ebx, 3
    call hc
    call phase
    mov ebx, 4
    call hc
    call phase
    mov ebx, 2
    call hc
    mov eax, 60             /* exit(0) */
    xor edi, edi
    syscall
hc:
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, ebx
    lea rdx, [rip + labels]
    xor r10, r10
    lea r8, [rip + hcpc]
hcpc:
    out 0x88, eax
    ret
phase:
    mov r13d, R
1:
    call body
    call pic
    lea rax, [rip + pic]
    add rax, ALT_DELTA
    call rax
    call cross
    mov eax, 39             /* getpid */
    syscall
    dec r13d
    jnz 1b
    ret
body:
h0:
    add r14, 1
h1:
    add r14, 2
h2:
    add r14, 3
    ret
pic:
p0:
    add r14, 5
    ret
    .balign 4096
    .skip 0xff4
cross:
x0:
    add r14, 1              /* 4 bytes, ends at 0xff8 */
xs:
    movabs rax, 0x1122334455667788      /* 10 bytes: 0xff8 to 0x1001 */
x1:
    add r14, 3
    ret
    .data
labels:                     /* for the test */
    .quad h0, h1, h2, p0, x0, xs, x1
exe:
    .asciz "/proc/self/exe"
