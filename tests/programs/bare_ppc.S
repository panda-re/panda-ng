/* tests/blocks_bare.py's bare-metal guest for ppc (g3beige, -bios: entered at the reset vector
 * 0xfff00100 in real mode): no OS, no interrupts. The same loop as blocks_<arch>.S: blocks "loop"
 * N times, "odd" and "even" N/2 times each, chained to one another; then "done" spins. The result
 * is in r4: sum(1..N) + 3 * N/2. ppc TBs never use CF_PCREL. */
    .set N, 1000
    .text
    .globl _start
_start:
setup:                      /* block "setup" */
    li      3, N
    li      4, 0
    b       loop
loop:                       /* block "loop": N times */
    add     4, 4, 3
    andi.   5, 3, 1
    beq     0, even
odd:                        /* block "odd": N/2 times (falls into even's instructions) */
    addi    4, 4, 3
even:                       /* block "even": N/2 times */
    addic.  3, 3, -1
    bne     0, loop
done:                       /* block "done": spins */
    b       done
