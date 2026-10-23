/* tests/fastrestore.py's hardware A/D program; see adbits_x86_64.S. No compressed instructions. */
    .option norvc
    .globl _start
_start:
    li a0, -100             /* openat(AT_FDCWD, path, O_RDWR|O_CREAT, 0600) */
    lla a1, path
    li a2, 0x42
    li a3, 0x180
    li a7, 56
    ecall
    mv s1, a0
    li a1, 4096             /* ftruncate(fd, 4096) */
    li a7, 46
    ecall
    li a0, 0                /* mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0) */
    li a1, 4096
    li a2, 3
    li a3, 1
    mv a4, s1
    li a5, 0
    li a7, 222
    ecall
    mv s2, a0
    lbu t0, 0(s2)           /* read fault: a clean, writable PTE */
    li a0, 0x50616e64       /* hypercall 1 */
    mv a7, a0
    li a1, 1
    mv a2, s2
    li a3, 0
    lla a4, hc1
hc1:
    .4byte 0x00004013
    li t0, 0x1234
    sd t0, 0(s2)            /* the walker sets D (Svadu, or no Svade) */
    li a0, 0x50616e64       /* hypercall 2 */
    mv a7, a0
    li a1, 2
    mv a2, s2
    li a3, 0
    lla a4, hc2
hc2:
    .4byte 0x00004013
    li a0, 0                /* exit(0) */
    li a7, 93
    ecall
    .data
path:
    .asciz "/dev/shm/adbits"
