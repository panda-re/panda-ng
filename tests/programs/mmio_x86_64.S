/* tests/mmio.py's guest program. Hypercall n passes (MAGIC, n, a, b, pc of the hypercall).
 * At hypercall 0 (a = params) the test writes params: a device's physical page, the offsets of
 * the register read and the register written in it, and the value to write. The program maps that
 * page from /dev/mem and makes hypercall 1 (a = table: the window's access labels, &ramword, the
 * mapping). Between hypercalls 1 and 2, 4-byte accesses: a store to the device, a load from it,
 * the store again, then a load and a store of RAM (ramword). Exit status 0, or 2 if open failed
 * and 3 if mmap did. */
    .intel_syntax noprefix
    .globl _start
_start:
    mov eax, 173            /* ioperm(0x88, 4, 1): the hypercall is out 0x88 */
    mov edi, 0x88
    mov esi, 4
    mov edx, 1
    syscall
    mov al, [rip + params]  /* fault the data page in, writable, before hypercall 0 */
    mov [rip + params], al
    mov edi, 0x50616e64
    mov eax, edi
    xor esi, esi
    lea rdx, [rip + params]
    lea r10, [rip + table]
    lea r8, [rip + hc0]
hc0:
    out 0x88, eax
    mov eax, 2              /* open("/dev/mem", O_RDWR | O_SYNC) */
    lea rdi, [rip + devmem]
    mov esi, 0x101002
    xor edx, edx
    syscall
    mov edi, 2
    test rax, rax
    js fail
    mov r12, rax
    mov eax, 9              /* mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, page) */
    xor edi, edi
    mov esi, 4096
    mov edx, 3
    mov r10d, 1
    mov r8, r12
    mov r9, [rip + params]
    syscall
    mov edi, 3
    cmp rax, -4096
    ja fail
    mov r13, rax
    mov [rip + table + 48], r13
    mov rbx, r13
    add rbx, [rip + params + 8]     /* the register read */
    mov r14, r13
    add r14, [rip + params + 16]    /* the register written */
    mov r15d, [rip + params + 24]   /* the value written */
    lea rbp, [rip + ramword]
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 1
    lea rdx, [rip + table]
    xor r10d, r10d
    lea r8, [rip + hc1]
hc1:
    out 0x88, eax
    jmp 1f                  /* end the TB: what follows is translated with the callbacks on */
1:
mw1:
    mov [r14], r15d         /* the device: store, load, store */
mr:
    mov ecx, [rbx]
mw2:
    mov [r14], r15d
rr:
    mov r9d, [rbp]          /* RAM: load, store */
rw:
    mov [rbp], r15d
    mov edi, 0x50616e64
    mov eax, edi
    mov esi, 2
    lea rdx, [rip + table]
    xor r10d, r10d
    lea r8, [rip + hc2]
hc2:
    out 0x88, eax
    xor edi, edi
fail:
    mov eax, 60             /* exit(edi) */
    syscall
    .data
    .balign 8
params:
    .quad 0, 0, 0, 0
table:
    .quad mw1, mr, mw2, rr, rw, ramword, 0
ramword:
    .quad 0x1122334455667788
devmem:
    .asciz "/dev/mem"
