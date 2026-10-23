/* tests/mem.py's guest program; see mem_x86_64.S. No 16-byte access (mr5/mw5 are 0). Loaded values
 * in s2-s5, the exit code in s6. No compressed instructions. */
    .option norvc
    .globl _start
_start:
    lla a2, labels          /* fault in the label table, and buf writable, before hypercall 1 */
    lbu t0, 0(a2)
    lla s1, buf
    lbu t0, 0(s1)
    sb t0, 0(s1)
    li a0, 0x50616e64
    mv a7, a0
    li a1, 1
    li a3, 0
    lla a4, hc1
hc1:
    .4byte 0x00004013       /* xori x0, x0, 0: the hypercall instruction */
    j m0                    /* end the TB: what follows is translated with the callbacks on */
m0:
    lla s1, buf
    li t0, 0x11
mw1: sb t0, 0(s1)
    li t0, 0x2233
mw2: sh t0, 2(s1)
    li t0, 0x44556677
mw3: sw t0, 4(s1)
    li t0, 0x8899aabbccddeeff
mw4: sd t0, 8(s1)
mr1: lbu s2, 0(s1)
mr2: lhu s3, 2(s1)
mr3: lwu s4, 4(s1)
mr4: ld s5, 8(s1)
    li s6, 0
    li a0, 0x50616e64
    mv a7, a0
    li a1, 2
    lla a2, labels
    li a3, 0
    lla a4, hc2
hc2:
    .4byte 0x00004013
    li a1, 3
    lla a4, hc3
hc3:
    .4byte 0x00004013
    lla t3, fresh
mpf: sw a1, 0(t3)            /* first touch of this page: a page fault, then the store again */
    li a1, 4
    lla a4, hc4
hc4:
    .4byte 0x00004013
    mv a0, s6               /* exit(s6) */
    li a7, 93
    ecall
    .data
labels:                     /* for the test */
    .quad buf, src16, mw1, mw2, mw3, mw4, 0, 0, mr1, mr2, mr3, mr4, mpf, fresh, m0, hc2
    .balign 16
src16:
    .ascii "0123456789abcdef"
buf:
    .skip 64
    .bss
    .balign 4096
    .skip 4096              /* a page between .data's tail and the one touched */
fresh:
    .skip 4096
