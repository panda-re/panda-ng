/* tests/blocks_bare.py's bare-metal guest for mips (malta, -kernel): no OS, no interrupts. The same
 * loop as blocks_<arch>.S: blocks "loop" N times, "odd" and "even" N/2 times each, chained to one
 * another; then "done" spins. The result is in v0: sum(1..N) + 3 * N/2. mips TBs never use
 * CF_PCREL. */
    .set noreorder
    .set N, 1000
    .text
    .globl _start
_start:
setup:                      /* block "setup" */
    li      $t0, N
    li      $v0, 0
    b       loop
    nop
loop:                       /* block "loop": N times */
    addu    $v0, $v0, $t0
    andi    $t1, $t0, 1
    beqz    $t1, even
    nop
odd:                        /* block "odd": N/2 times (falls into even's instructions) */
    addiu   $v0, $v0, 3
even:                       /* block "even": N/2 times */
    addiu   $t0, $t0, -1
    bnez    $t0, loop
    nop
done:                       /* block "done": spins */
    b       done
    nop
