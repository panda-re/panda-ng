/* tests/hooks.py's guest program; see hooks_x86_64.S. Instructions are 4 bytes, so none
 * straddles the page boundary: xs is the last instruction on the first page. */
    .set R, 100
    .globl _start
_start:
    mov x0, #-100           /* openat(AT_FDCWD, "/proc/self/exe", O_RDONLY) */
    adr x1, exe
    mov x2, #0
    mov x3, #0
    mov x8, #56
    svc #0
    mov x4, x0              /* mmap(ALT, 0x3000, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED, fd, 0) */
    adr x0, __ehdr_start    /* ALT: the file's first page, ALT_DELTA (0x200000) further */
    movz x10, #0x20, lsl #16
    add x0, x0, x10
    mov x1, #0x3000
    mov x2, #5
    mov x3, #0x12
    mov x5, #0
    mov x8, #222
    svc #0
    adr x2, labels          /* fault the label table's page in before hypercall 1 reads it */
    ldrb w9, [x2]
    mov x19, #1
    bl hc
    bl phase
    mov x19, #3
    bl hc
    bl phase
    mov x19, #4
    bl hc
    bl phase
    mov x19, #2
    bl hc
    mov x0, #0              /* exit(0) */
    mov x8, #93
    svc #0
hc:
    movz x0, #0x6e64
    movk x0, #0x5061, lsl #16
    mov x8, x0
    mov x1, x19
    adr x2, labels
    mov x3, #0
    adr x4, hcpc
hcpc:
    .inst 0xd500501f        /* the hypercall instruction */
    ret
phase:
    mov x21, x30
    mov x20, #R
1:
    bl body
    bl pic
    adr x9, pic             /* pic through the second mapping: ALT_DELTA further */
    movz x10, #0x20, lsl #16
    add x9, x9, x10
    blr x9
    bl cross
    mov x8, #172            /* getpid */
    svc #0
    subs x20, x20, #1
    b.ne 1b
    mov x30, x21
    ret
body:
h0:
    add x22, x22, #1
h1:
    add x22, x22, #2
h2:
    add x22, x22, #3
    ret
pic:
p0:
    add x22, x22, #5
    ret
exe:
    .asciz "/proc/self/exe"
    .balign 4096
    .skip 0xff8
cross:
x0:
    add x22, x22, #1
xs:
    add x22, x22, #2        /* the last instruction on the page */
x1:
    add x22, x22, #3        /* the first on the next */
    ret
    .data
labels:                     /* for the test */
    .quad h0, h1, h2, p0, x0, xs, x1
