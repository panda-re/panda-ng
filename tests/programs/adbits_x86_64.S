/* tests/fastrestore.py's hardware A/D program. Maps a tmpfs file MAP_SHARED, reads it (the kernel
 * maps the page writable but clean: no D bit), makes hypercall 1 (MAGIC, 1, page), writes the page
 * (the CPU's page walker sets the PTE's D bit itself: no fault, no kernel store), makes hypercall 2,
 * exits 0. */
    .intel_syntax noprefix
    .globl _start
_start:
    mov eax, 173            /* ioperm(0x88, 4, 1): the hypercall is out 0x88 */
    mov edi, 0x88
    mov esi, 4
    mov edx, 1
    syscall
    mov eax, 257            /* openat(AT_FDCWD, path, O_RDWR|O_CREAT, 0600) */
    mov rdi, -100
    lea rsi, [rip + path]
    mov edx, 0x42
    mov r10d, 0x180
    syscall
    mov r12, rax
    mov eax, 77             /* ftruncate(fd, 4096) */
    mov rdi, r12
    mov esi, 4096
    syscall
    mov eax, 9              /* mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0) */
    xor edi, edi
    mov esi, 4096
    mov edx, 3
    mov r10d, 1
    mov r8, r12
    xor r9d, r9d
    syscall
    mov r13, rax
    mov bl, [r13]           /* read fault: a clean, writable PTE */
    mov edi, 0x50616e64     /* hypercall 1 */
    mov eax, edi
    mov esi, 1
    mov rdx, r13
    xor r10, r10
    lea r8, [rip + hc1]
hc1:
    out 0x88, eax
    mov qword ptr [r13], 0x1234   /* the walker sets D */
    mov edi, 0x50616e64     /* hypercall 2 */
    mov eax, edi
    mov esi, 2
    mov rdx, r13
    xor r10, r10
    lea r8, [rip + hc2]
hc2:
    out 0x88, eax
    mov eax, 60             /* exit(0) */
    xor edi, edi
    syscall
    .data
path:
    .asciz "/dev/shm/adbits"
