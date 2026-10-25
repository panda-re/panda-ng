/* A real-mode program for the configurable machine on i386/x86_64 (no PANDA: tests/machine_raw.sh).
 * Linked at 0xffffff00 into a ROM region at 0xffff0000; the CPU resets to CS:IP f000:fff0 (run
 * with entry=0xfff0). Reads the TSC twice and prints "CFG-OK x86 TIMER-OK" on a serial-mm UART
 * at 0x80000 (ES = 0x8000). x86 has no hypercall on this machine (it is pc.c's I/O port 0x88). */
    .code16
    .text
    .globl _start
_start:
    rdtsc
    mov %eax, %esi
    mov $50000, %cx
1:  loop 1b
    rdtsc
    mov $0x8000, %bx
    mov %bx, %es
    mov $(hello - _start + 0xff00), %bx
    cmp %eax, %esi
    jne 2f
    mov $(stuck - _start + 0xff00), %bx
2:  mov %cs:(%bx), %al
    test %al, %al
    jz 3f
    mov %al, %es:0
    inc %bx
    jmp 2b
3:  hlt
    jmp 3b
hello:
    .asciz "CFG-OK x86 TIMER-OK\n"
stuck:
    .asciz "CFG-OK TIMER-STUCK\n"
    .org 0xf0
reset:
    jmp _start
    .org 0x100
