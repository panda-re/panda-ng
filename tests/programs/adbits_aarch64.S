/* tests/fastrestore.py's hardware A/D program; see adbits_x86_64.S. */
    .globl _start
_start:
    mov x0, #-100           /* openat(AT_FDCWD, path, O_RDWR|O_CREAT, 0600) */
    adr x1, path
    mov x2, #0x42
    mov x3, #0x180
    mov x8, #56
    svc #0
    mov x19, x0
    mov x1, #4096           /* ftruncate(fd, 4096) */
    mov x8, #46
    svc #0
    mov x0, #0              /* mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0) */
    mov x1, #4096
    mov x2, #3
    mov x3, #1
    mov x4, x19
    mov x5, #0
    mov x8, #222
    svc #0
    mov x20, x0
    ldrb w9, [x20]          /* read fault: a clean, writable PTE */
    movz x0, #0x6e64        /* hypercall 1 */
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #1
    mov x2, x20
    mov x3, #0
    adr x4, hc1
hc1:
    .inst 0xd500501f
    mov x9, #0x1234
    str x9, [x20]           /* the walker sets the dirty state (FEAT_HAFDBS) */
    movz x0, #0x6e64        /* hypercall 2 */
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #2
    mov x2, x20
    mov x3, #0
    adr x4, hc2
hc2:
    .inst 0xd500501f
    mov x0, #0              /* exit(0) */
    mov x8, #93
    svc #0
    .data
path:
    .asciz "/dev/shm/adbits"
