/* tests/syscalls.py's guest program; see syscalls_x86_64.S (no vector registers here).
 * No compressed instructions. */
    .option norvc
    .option norelax         /* no gp-relative lla (gp is never set) */
    .globl _start
_start:
    lla a0, rs              /* rseq(&rs, 32, 0, RSEQ_SIG) */
    li a1, 32
    li a2, 0
    li a3, 0x53053053
    li a7, 293
    ecall
    li s2, 1
    bnez a0, out
    lla a0, ca              /* clone3(&ca, 88) */
    li a1, 88
    li a7, 435
    ecall
    beqz a0, child
    li s2, 2
    bltz a0, out
    mv s1, a0
    li a1, 0                /* wait4(pid, 0, 0, 0) */
    li a2, 0
    li a3, 0
    li a7, 260
    ecall
    li s2, 3
    bne a0, s1, out
    li a0, 100              /* close_range(100, 200, 0) */
    li a1, 200
    li a2, 0
    li a7, 436
    ecall
    li s2, 4
    bnez a0, out
    li a0, -100             /* faccessat2(AT_FDCWD, "/", R_OK, AT_EACCESS) */
    lla a1, root
    li a2, 4
    li a3, 0x200
    li a7, 439
    ecall
    li s2, 5
    bnez a0, out
    li a0, -100             /* fchmodat2(AT_FDCWD, missing, 0644, AT_SYMLINK_NOFOLLOW) */
    lla a1, missing
    li a2, 0644
    li a3, 0x100
    li a7, 452
    ecall
    li s2, 6
    li t0, -2
    bne a0, t0, out
    li a0, 0x1122334455667788  /* lookup_dcookie */
    li a1, 0x5678
    li a2, 128
    li a7, 18
    ecall
    li s2, 7
    li t0, -38
    bne a0, t0, out
    li a0, 9                  /* nfsservctl */
    li a1, 0x5678
    li a2, 0x9abc
    li a7, 42
    ecall
    li s2, 8
    li t0, -38
    bne a0, t0, out
    li s2, 0
out:
    mv a0, s2               /* exit(s2) */
    li a7, 93
    ecall
child:
    li a0, 0                /* the vfork child: exit(0) */
    li a7, 93
    ecall
    .data
    .balign 32
rs: .zero 32
ca: .quad 0x4100, 0, 0, 0, 17, stack, 4096, 0, 0, 0, 0
root: .asciz "/"
missing: .asciz "/nonexistent-panda-syscalls"
    .bss
    .balign 16
stack: .zero 4096           /* the clone3 child's stack */
