/* tests/mmio.py's guest program. Hypercall n passes (MAGIC, n, a, b, pc of the hypercall).
 * At hypercall 0 (a = params) the test writes params: a device's physical page, the offsets of
 * the register read and the register written in it, and the value to write. The program maps that
 * page from /dev/mem and makes hypercall 1 (a = table: the window's access labels, &ramword, the
 * mapping). Between hypercalls 1 and 2, 4-byte accesses: a store to the device, a load from it,
 * the store again, then a load and a store of RAM (ramword). Exit status 0, or 2 if open failed
 * and 3 if mmap did. */
    .globl _start
_start:
    adr x10, params         /* fault the data page in, writable, before hypercall 0 */
    ldrb w9, [x10]
    strb w9, [x10]
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #0
    adr x2, params
    adr x3, table
    adr x4, hc0
hc0:
    .inst 0xd500501f        /* the hypercall instruction */
    mov x0, #-100           /* openat(AT_FDCWD, "/dev/mem", O_RDWR | O_SYNC) */
    adr x1, devmem
    movz x2, #0x1002
    movk x2, #0x10, lsl #16
    mov x3, #0
    mov x8, #56
    svc #0
    mov x9, #2
    tbnz x0, #63, fail
    mov x19, x0
    mov x0, #0              /* mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, page) */
    mov x1, #4096
    mov x2, #3
    mov x3, #1
    mov x4, x19
    adr x10, params
    ldr x5, [x10]
    mov x8, #222
    svc #0
    mov x9, #3
    cmn x0, #4096
    b.hi fail
    mov x20, x0
    adr x10, params
    adr x11, table
    str x20, [x11, #48]
    ldr x12, [x10, #8]
    add x21, x20, x12       /* the register read */
    ldr x12, [x10, #16]
    add x22, x20, x12       /* the register written */
    ldr w23, [x10, #24]     /* the value written */
    adr x24, ramword
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #1
    adr x2, table
    mov x3, #0
    adr x4, hc1
hc1:
    .inst 0xd500501f
    b 1f                    /* end the TB: what follows is translated with the callbacks on */
1:
mw1:
    str w23, [x22]          /* the device: store, load, store */
mr:
    ldr w25, [x21]
mw2:
    str w23, [x22]
rr:
    ldr w26, [x24]          /* RAM: load, store */
rw:
    str w23, [x24]
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, #2
    adr x2, table
    mov x3, #0
    adr x4, hc2
hc2:
    .inst 0xd500501f
    mov x9, #0
fail:
    mov x0, x9              /* exit(x9) */
    mov x8, #93
    svc #0
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
