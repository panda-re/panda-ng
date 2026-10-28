/* tests/syscalls.py's guest program: system calls glibc 2.41 makes that syscalls2's Linux tables
 * lacked before they were generated from Linux 6.19, each with known arguments:
 *   rseq(&rs, 32, 0, RSEQ_SIG)                                  -> 0
 *   clone3(&ca, 88): CLONE_VM | CLONE_VFORK, SIGCHLD, own stack -> 0 in the child (which exits),
 *                                                                  its pid in the parent
 *   wait4(pid, 0, 0, 0)                                         -> pid
 *   close_range(100, 200, 0)                                    -> 0
 *   faccessat2(AT_FDCWD, "/", R_OK, AT_EACCESS)                 -> 0
 *   fchmodat2(AT_FDCWD, "/nonexistent-panda-syscalls", 0644, AT_SYMLINK_NOFOLLOW) -> -ENOENT
 * exit(n) at the first call (n = 1..6, in that order) that returns something else, exit(0) after
 * them all. Before close_range it loads xmm0 and xmm15 with the bytes 0x00..0x0f and 0xf0..0xff,
 * which tests/syscalls.py reads back through pandare2 at close_range's enter. */
    .intel_syntax noprefix
    .globl _start
_start:
    mov eax, 334            /* rseq */
    lea rdi, [rip + rs]
    mov esi, 32
    xor edx, edx
    mov r10d, 0x53053053
    syscall
    mov edi, 1
    test rax, rax
    jnz out
    mov eax, 435            /* clone3 */
    lea rdi, [rip + ca]
    mov esi, 88
    syscall
    test rax, rax
    jz child
    mov edi, 2
    js out
    mov r12, rax
    mov eax, 61             /* wait4 */
    mov rdi, r12
    xor esi, esi
    xor edx, edx
    xor r10d, r10d
    syscall
    mov edi, 3
    cmp rax, r12
    jne out
    movdqu xmm0, [rip + x0]
    movdqu xmm15, [rip + x15]
    mov eax, 436            /* close_range */
    mov edi, 100
    mov esi, 200
    xor edx, edx
    syscall
    mov edi, 4
    test rax, rax
    jnz out
    mov eax, 439            /* faccessat2 */
    mov rdi, -100
    lea rsi, [rip + root]
    mov edx, 4
    mov r10d, 0x200
    syscall
    mov edi, 5
    test rax, rax
    jnz out
    mov eax, 452            /* fchmodat2 */
    mov rdi, -100
    lea rsi, [rip + missing]
    mov edx, 0644
    mov r10d, 0x100
    syscall
    mov edi, 6
    cmp rax, -2
    jne out
    /* Retired calls still need typed callbacks when the guest kernel returns ENOSYS. */
    mov eax, 212            /* lookup_dcookie */
    mov rdi, 0x1122334455667788
    mov esi, 0x5678
    mov edx, 128
    syscall
    mov edi, 7
    cmp rax, -38
    jne out
    mov eax, 180            /* nfsservctl */
    mov edi, 9
    mov esi, 0x5678
    mov edx, 0x9abc
    syscall
    mov edi, 8
    cmp rax, -38
    jne out
    mov eax, 156            /* _sysctl */
    mov edi, 0x1234
    syscall
    mov edi, 9
    cmp rax, -38
    jne out
    xor edi, edi
out:
    mov eax, 60             /* exit(edi) */
    syscall
child:
    mov eax, 60             /* the vfork child: exit(0) */
    xor edi, edi
    syscall
    .data
    .balign 32
rs: .zero 32                /* struct rseq: 32-byte aligned */
ca: .quad 0x4100, 0, 0, 0, 17, stack, 4096, 0, 0, 0, 0  /* struct clone_args: flags, pidfd, child_tid,
                               parent_tid, exit_signal, stack, stack_size, tls, set_tid, set_tid_size, cgroup */
x0: .byte 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
x15: .byte 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
root: .asciz "/"
missing: .asciz "/nonexistent-panda-syscalls"
    .bss
    .balign 16
stack: .zero 4096           /* the clone3 child's stack */
