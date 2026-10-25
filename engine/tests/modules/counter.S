/* aarch64 guest for tests/modules/run.sh: reads ext-counter's ID, writes it 3 times, reads the
 * count and the module's qemu_get_thread_id() register, prints "ID=<id> COUNT=<n> SELF=<v>" on
 * the PL011 of -M virt (or the configurable machine's), and exits through semihosting. */
    .equ UART, 0x09000000
    .equ CNTR, 0x0b000000
    .globl _start
_start:
    ldr x9, =0x40800000
    mov sp, x9
    ldr x10, =CNTR
    ldr w19, [x10]
    mov w9, #1
    str w9, [x10, #4]
    str w9, [x10, #4]
    str w9, [x10, #4]
    ldr w20, [x10, #8]
    ldr w21, [x10, #12]
    adr x0, s_id
    bl puts
    mov x0, x19
    bl puthex
    adr x0, s_count
    bl puts
    mov x0, x20
    bl puthex
    adr x0, s_self
    bl puts
    mov x0, x21
    bl puthex
    adr x0, s_nl
    bl puts
    mov x0, #0x18
    adr x1, block
    hlt #0xf000
1:  b 1b
putc:
    ldr x2, =UART
    str w0, [x2]
    ret
puts:
    mov x5, x30
    mov x4, x0
2:  ldrb w0, [x4], #1
    cbz w0, 3f
    bl putc
    b 2b
3:  ret x5
puthex:
    mov x5, x30
    mov x4, x0
    mov x6, #28
4:  lsr x0, x4, x6
    and x0, x0, #15
    cmp x0, #10
    add x7, x0, #'0'
    add x0, x0, #('a' - 10)
    csel x0, x7, x0, lt
    bl putc
    subs x6, x6, #4
    b.pl 4b
    ret x5
    .ltorg
s_id:    .asciz "ID="
s_count: .asciz " COUNT="
s_self:  .asciz " SELF="
s_nl:    .asciz "\n"
    .balign 8
block: .quad 0x20026, 0
