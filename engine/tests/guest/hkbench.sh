#!/bin/sh
# hkbench.sh [phase...]: address-hook measurements (plugin/hooktest.c, guest hk-* tests)
# env: OUT (default ../out-hk), ARCHS, REPS, PAR (parallel runs)
#   hit     per-hit cost: no plugin / instruction callbacks / hooks / removed hook
#   reads   cost of plugin register reads in a hook (0, 1, 10 per hit)
#   direct  10 register reads per hit: gdbstub (new / reused buffer) vs
#           qemu_plugin_register_ptr(), plus the checks with direct=on
#   cond    per-hit cost of a conditional hook whose condition fails / holds
#   ins     insert/remove latency from a callback, after a broad warm-up
#   boot    whole-run time (translation): none / insn / hook / hook + 10000 sites
#   exec    CF_PCREL: exec a PIE 50 times, ASLR off/on, PCREL on/off
#   probe   in-guest uprobe and kprobe vs hypervisor hooks on the same code
#   check   correctness: counts, register write, redirect, fault, alias, dynamic
H=$(cd "$(dirname "$0")" && pwd); cd $H
. "$H/env.sh"
O=${OUT:-$OUTROOT/out-hk}; mkdir -p $O
P=$PLUGINS/libhooktest.so
ALL="x86_64 aarch64 armel mipsel mipseb mips64el mips64eb powerpc64 powerpc64le riscv64 loongarch64"
ARCHS=${ARCHS:-$ALL}
REPS=${REPS:-5}
PAR=${PAR:-6}

# one <name> <arch> <tests> [plugin args|none]: one boot, 6.13 kernel
one() {
  n=$1; a=$2; t=$3; shift 3
  s=$(date +%s.%N)
  if [ "$1" = none ]; then
    TMO=900 ./run.sh $a 6.13 $t > $O/$a-$n.con 2>&1
  else
    QEXTRA="-trace tcg_hooks_commit" PLUGIN=$P LOG=$O/$a-$n.log TMO=900 ./run.sh $a 6.13 $t "$@" > $O/$a-$n.con 2>&1
  fi
  rc=$?; e=$(date +%s.%N)
  echo "WALL $(echo "$e - $s" | bc) rc=$rc" >> $O/$a-$n.con
}

# run jobs for every arch, PAR at a time
forarch() {
  i=0
  for a in $ARCHS; do
    "$@" $a &
    i=$((i + 1)); [ $((i % PAR)) = 0 ] && wait
  done; wait
}

hit() { a=$1; S=$(./hksyms.sh $a); T=$(echo $S | cut -d' ' -f1); PH=$(echo $S | grep -o 'phase=[^ ]*')
  one hit-none $a hk-bench:2000000 none
  one hit-insn $a hk-bench:2000000 api=insn $T $PH
  one hit-hook $a hk-bench:2000000 api=hook $T $PH
  one hit-hookdyn $a hk-bench:2000000 api=hook dyn=on arm=on $T $PH; }
reads() { a=$1; S=$(./hksyms.sh $a); T=$(echo $S | cut -d' ' -f1); PH=$(echo $S | grep -o 'phase=[^ ]*')
  for r in 0 1 10; do one reads$r $a hk-bench:1000000 api=hook reads=$r $T $PH; done; }
direct() { a=$1; S=$(./hksyms.sh $a); T=$(echo $S | cut -d' ' -f1); PH=$(echo $S | grep -o 'phase=[^ ]*')
  one rd-0 $a hk-bench:1000000 api=hook reads=0 $T $PH
  one rd-gdb $a hk-bench:1000000 api=hook reads=10 $T $PH
  one rd-reuse $a hk-bench:1000000 api=hook reads=10 reuse=on $T $PH
  one rd-direct $a hk-bench:1000000 api=hook reads=10 direct=on $T $PH
  one check-direct $a hk-count:100 api=hook direct=on $S; }
cond() { a=$1; S=$(./hksyms.sh $a); T=$(echo $S | cut -d' ' -f1); PH=$(echo $S | grep -o 'phase=[^ ]*')
  one cond-plain $a hk-bench:2000000 api=hook $T $PH
  one cond-fail $a hk-bench:2000000 api=hook condbench=fail $T $PH
  one cond-pass $a hk-bench:2000000 api=hook condbench=pass $T $PH; }
ins() { a=$1; one ins $a hk-ins:100:1 api=hook dyn=on $(./hksyms.sh $a); }
boot() { a=$1; S=$(./hksyms.sh $a)
  for r in $(seq $REPS); do
    one boot-none-$r $a hk-count:1 none
    one boot-insn-$r $a hk-count:1 api=insn $S
    one boot-hook-$r $a hk-count:1 api=hook $S
    one boot-many-$r $a hk-count:1 api=hook many=10000 $S
  done; }
exec_() { a=$1
  case $a in x86_64|aarch64|armel|riscv64|mipsel) ;; *) return;; esac
  for asl in 0 2; do for r in $(seq ${EXECREPS:-2}); do
    one exec-pcrel-aslr$asl-$r $a hk-exec:50:$asl none
    one exec-nopcrel-aslr$asl-$r $a hk-exec:50:$asl api=hook target=0x1000
  done; done; }
probe() { a=$1; . $GEN/syms-6.13-$a.env; S=$(./hksyms.sh $a); T=$(echo $S | cut -d' ' -f1); PH=$(echo $S | grep -o 'phase=[^ ]*')
  one probe-guest $a hk-ubench:20000,hk-kbench:100000:1 none
  one probe-hvk $a hk-kbench:100000:0 api=hook ktarget=$GETPPID $PH
  one probe-hvu $a hk-bench:200000 api=hook $T $PH; }

check() { a=$1; S=$(./hksyms.sh $a)
  one check-count $a hk-count:100 api=hook $S
  one check-alias-hook $a hk-alias:100 api=hook $S
  one check-alias-insn $a hk-alias:100 api=insn $S
  one check-dyn $a hk-dyn:100 api=hook dyn=on $S; }

for p in ${*:-hit reads ins boot exec probe check}; do
  echo "### $p $(date +%T)"
  case $p in
  hit) forarch hit;;
  reads) ARCHS="x86_64 aarch64 mipsel" forarch reads;;
  direct) forarch direct;;
  cond) forarch cond;;
  ins) forarch ins;;
  boot) ARCHS="x86_64 aarch64" PAR=2 forarch boot;;
  exec) forarch exec_;;
  probe) forarch probe;;
  check) forarch check;;
  esac
done
echo "### done $(date +%T)"
