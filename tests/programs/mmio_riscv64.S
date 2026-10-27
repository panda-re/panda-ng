/* tests/mmio.py's guest program. Hypercall n passes (MAGIC, n, a, b, pc of the hypercall).
 * At hypercall 0 (a = params) the test writes params: a device's physical page, the offsets of
 * the register read and the register written in it, and the value to write. The program maps that
 * page from /dev/mem and makes hypercall 1 (a = table: the window's access labels, &ramword, the
 * mapping). Between hypercalls 1 and 2, 4-byte accesses: a store to the device, a load from it,
 * the store again, then a load and a store of RAM (ramword). Exit status 0, or 2 if open failed
 * and 3 if mmap did. */
    .option norvc
    .option norelax         /* no gp-relative addresses: nothing sets gp */
    .globl _start
_start:
    lla t0, params          /* fault the data page in, writable, before hypercall 0 */
    lbu t1, 0(t0)
    sb t1, 0(t0)
    li a0, 0x50616e64
    mv a7, a0
    li a1, 0
    lla a2, params
    lla a3, table
    lla a4, hc0
hc0:
    .4byte 0x00004013       /* xori x0, x0, 0: the hypercall instruction */
    li a0, -100             /* openat(AT_FDCWD, "/dev/mem", O_RDWR | O_SYNC) */
    lla a1, devmem
    li a2, 0x101002
    li a3, 0
    li a7, 56
    ecall
    li s8, 2
    bltz a0, fail
    mv s0, a0
    li a0, 0                /* mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, page) */
    li a1, 4096
    li a2, 3
    li a3, 1
    mv a4, s0
    lla t0, params
    ld a5, 0(t0)
    li a7, 222
    ecall
    li s8, 3
    li t1, -4096
    bgtu a0, t1, fail
    mv s1, a0
    lla t0, params
    lla t1, table
    sd s1, 48(t1)
    ld t2, 8(t0)
    add s2, s1, t2          /* the register read */
    ld t2, 16(t0)
    add s3, s1, t2          /* the register written */
    lw s4, 24(t0)           /* the value written */
    lla s5, ramword
    li a0, 0x50616e64
    mv a7, a0
    li a1, 1
    lla a2, table
    li a3, 0
    lla a4, hc1
hc1:
    .4byte 0x00004013
    j 1f                    /* end the TB: what follows is translated with the callbacks on */
1:
mw1:
    sw s4, 0(s3)            /* the device: store, load, store */
mr:
    lw s6, 0(s2)
mw2:
    sw s4, 0(s3)
rr:
    lw s7, 0(s5)            /* RAM: load, store */
rw:
    sw s4, 0(s5)
    li a0, 0x50616e64
    mv a7, a0
    li a1, 2
    lla a2, table
    li a3, 0
    lla a4, hc2
hc2:
    .4byte 0x00004013
    li s8, 0
fail:
    mv a0, s8               /* exit(s8) */
    li a7, 93
    ecall
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
