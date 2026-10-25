/* tests/machine_raw.sh's program for ppc 40x CPUs on the configurable machine: writes PIT (40x),
 * reads the timebase twice, and prints "CFG-OK ppc40x TIMER-OK" on a serial-mm UART at 0x10000000.
 * Before the configurable machine set up this CPU family's timers, the PIT (40x) write dereferenced
 * NULL in the host (tb_env->opaque). Linked at 0 (RAM); e500 runs through the 1 GiB identity TLB
 * entry the machine installs at reset, 40x in real mode. */
    .equ UART, 0x10000000
    .text
    .globl _start
_start:
    lis 9, 0x0400
    mtspr 987, 9               /* PIT (40x) */
    mfspr 20, 268               /* TBL */
    lis 9, 1
    mtctr 9
1:  bdnz 1b
    mfspr 21, 268
    lis 9, UART@h
    lis 10, hello@ha
    addi 10, 10, hello@l
    cmpw 20, 21
    bne 2f
    lis 10, stuck@ha
    addi 10, 10, stuck@l
2:  lbz 11, 0(10)
    cmpwi 11, 0
    beq 3f
    stb 11, 0(9)
    addi 10, 10, 1
    b 2b
3:  b 3b
    .data
hello:
    .asciz "CFG-OK ppc40x TIMER-OK\n"
stuck:
    .asciz "CFG-OK TIMER-STUCK\n"
