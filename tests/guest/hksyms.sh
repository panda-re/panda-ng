#!/bin/sh
# usage: hksyms.sh <arch> -> hooktest plugin args for guest/pagetest.c's hk_* functions
#   mid= is hk_target's second instruction (inside the block), ds= the delay
#   slot of hk_target's return on mips
H=$(cd "$(dirname "$0")" && pwd)
. "$H/env.sh"
E=$GEN/pagetest-$1
llvm-readelf -s $E | awk '
  { m[$8] = "0x" $2 }
  END { printf "target=%s target2=%s phase=%s regs=%s redir=%s other=%s fault=%s",
        m["hk_target"], m["hk_target2"], m["hk_phase"], m["hk_regs"], m["hk_redirect"], m["hk_other"], m["hk_fault"] }'
case $1 in
loongarch64)   # fixed-size instructions; llvm-objdump 14 has no loongarch
  T=$(llvm-readelf -s $E | awk '$8 == "hk_target" { print $2 }')
  printf ' mid=0x%x' $((0x$T + 4));;
*)
  llvm-objdump -d --no-show-raw-insn --disassemble-symbols=hk_target $E | awk '
    /^ *[0-9a-f]+:/ { a = $1; sub(":", "", a); n++
      if (n == 2) printf " mid=0x%s", a
      if (jr) { printf " ds=0x%s", a; jr = 0 }
      if ($2 == "jr") jr = 1 }';;
esac
echo
