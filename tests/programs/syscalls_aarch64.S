/* tests/syscalls.py's guest program; see syscalls_x86_64.S (no vector registers here). */
    .globl _start
_start:
    adr x0, rs              /* rseq(&rs, 32, 0, RSEQ_SIG) */
    mov x1, #32
    mov x2, #0
    movz w3, #0x3053
    movk w3, #0x5305, lsl #16
    mov x8, #293
    svc #0
    mov x9, #1
    cbnz x0, out
    adr x0, ca              /* clone3(&ca, 88) */
    mov x1, #88
    mov x8, #435
    svc #0
    cbz x0, child
    mov x9, #2
    tbnz x0, #63, out
    mov x19, x0
    mov x1, #0              /* wait4(pid, 0, 0, 0) */
    mov x2, #0
    mov x3, #0
    mov x8, #260
    svc #0
    mov x9, #3
    cmp x0, x19
    b.ne out
    mov x0, #100            /* close_range(100, 200, 0) */
    mov x1, #200
    mov x2, #0
    mov x8, #436
    svc #0
    mov x9, #4
    cbnz x0, out
    movn x0, #99            /* faccessat2(AT_FDCWD, "/", R_OK, AT_EACCESS) */
    adr x1, root
    mov x2, #4
    mov x3, #0x200
    mov x8, #439
    svc #0
    mov x9, #5
    cbnz x0, out
    movn x0, #99            /* fchmodat2(AT_FDCWD, missing, 0644, AT_SYMLINK_NOFOLLOW) */
    adr x1, missing
    mov x2, #0644
    mov x3, #0x100
    mov x8, #452
    svc #0
    mov x9, #6
    cmn x0, #2
    b.ne out
    mov x9, #0
out:
    mov x0, x9              /* exit(x9) */
    mov x8, #93
    svc #0
child:
    mov x0, #0              /* the vfork child: exit(0) */
    mov x8, #93
    svc #0
    .data
    .balign 32
rs: .zero 32
ca: .quad 0x4100, 0, 0, 0, 17, stack, 4096, 0, 0, 0, 0
root: .asciz "/"
missing: .asciz "/nonexistent-panda-syscalls"
    .bss
    .balign 16
stack: .zero 4096           /* the clone3 child's stack */
