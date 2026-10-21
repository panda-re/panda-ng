#!/bin/sh
# hktest.sh: pass/fail tests for TCG address hooks and qemu_plugin_register_ptr()
# (plugin/hooktest.c + guest hk-* tests), one 6.13 boot per case and arch.
# env: OUT (default ../out-hktest), ARCHS, PAR, CASES
#   count   hit counts per window, a mid-block hook, a delay-slot hook (mips),
#           register write, redirect, fault from a hook
#   direct  count with arg0 read and written through register_ptr
#   many    count with 10,000 extra hook sites
#   alias   same code mapped at two virtual addresses
#   dyn     insert/remove from a callback
#   multi   two hooks at one address; a hook removing itself after 37 hits
#   kern    a kernel-address hook (getppid's handler)
#   cond    conditional hooks on arg0 (==, <, mask, range, signed) and on a
#           host variable switched between windows
#   smp     2 vCPUs hitting the same hooks at once, register_ptr per vCPU
#   rp      register_ptr battery: argument/sp/ra registers found and equal to
#           the gdbstub on every hit, lookups change no register, pc/flags NULL
# exit status: 0 if every case passed (SKIP allowed), 1 otherwise
H=$(cd "$(dirname "$0")" && pwd); cd $H
. "$H/env.sh"
O=${OUT:-$OUTROOT/out-hktest}; mkdir -p $O
P=$PLUGINS/libhooktest.so
ALL="x86_64 aarch64 armel mipsel mipseb mips64el mips64eb powerpc64 powerpc64le riscv64 loongarch64"
ARCHS=${ARCHS:-$ALL}
PAR=${PAR:-6}
CASES=${CASES:-count direct many alias dyn multi kern cond smp rp}

# one <case> <arch> <guest test> <plugin args...>
one() {
  n=$1; a=$2; t=$3; shift 3
  echo "$*" > $O/$a-$n.args
  SMP=${SMP:-1} PLUGIN=$P LOG=$O/$a-$n.log TMO=600 ./run.sh $a 6.13 $t "$@" > $O/$a-$n.con 2>&1
  echo "RC $?" >> $O/$a-$n.con
}

arch() {
  a=$1; S=$(./hksyms.sh $a); PH=$(echo $S | grep -o 'phase=[^ ]*')
  for c in $CASES; do
    case $c in
    count)  one count $a hk-count:100 api=hook $S;;
    direct) one direct $a hk-count:100 api=hook direct=on $S;;
    many)   one many $a hk-count:100 api=hook many=10000 $S;;
    alias)  one alias $a hk-alias:100 api=hook $S;;
    dyn)    one dyn $a hk-dyn:100 api=hook dyn=on $S;;
    multi)  one multi $a hk-count:100 api=hook dup=on selfrm=37 $S;;
    kern)   (. $GEN/syms-6.13-$a.env; one kern $a hk-kern:1000 api=hook ktarget=$GETPPID $PH);;
    cond)   one cond $a hk-dyn:100 api=hook cond=on $S;;
    smp)    case $a in
            mips*) echo "malta with penguin's mips kernels panics at boot with 2 CPUs, plugin or not" > $O/$a-smp.skip;;
            *) (SMP=2; one smp $a hk-smp:20000 api=hook direct=on reads=1 $S);;
            esac;;
    rp)     one rp $a hk-count:100 api=hook rptest=on $S;;
    esac
  done
}

i=0
for a in $ARCHS; do
  arch $a &
  i=$((i + 1)); [ $((i % PAR)) = 0 ] && wait
done; wait
python3 $H/hktest.py $O
