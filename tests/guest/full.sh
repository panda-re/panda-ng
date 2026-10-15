#!/bin/sh
# full.sh [phase...]: the whole matrix on every penguin kernel (env: KERNELS, XOPTS, WKERNELS)
H=$(cd "$(dirname "$0")" && pwd); cd $H
. "$H/env.sh"
V="vext=on search=@SEARCH fixup=@FIXUP extable=@EXTABLE"
W="op=write"
S=/mnt/small.bin
ph() { echo "### $1 $(date +%T)"; }
smpsplit() { # smpsplit <name> <tests> [opts]: SMP=2, but 1 CPU on malta
  ALL=${KERNELS:-$(cd $GEN && ls syms-*.env | sed 's/syms-\(.*\)\.env/\1/')}
  M=$(echo "$ALL" | grep mips | xargs); O=$(echo "$ALL" | grep -v mips | xargs)
  [ -n "$O" ] && KERNELS="$O" SMP=2 ./matrix.sh "$@" &
  [ -n "$M" ] && KERNELS="$M" SMP=1 ./matrix.sh "$@" &
  wait
}
for p in ${*:-reads writes haz whaz kraw contend debugw window span wspan multi par}; do case $p in
reads)  ph reads;   TMO=600 ./matrix.sh reads "user-file,kern-file,user-swap,kern-swap" $V;;
writes) ph writes;  TMO=600 ./matrix.sh writes "user-wfile,kern-wfile,user-wzero,kern-wzero,user-wswap,kern-wswap" $W $V;;
haz)    ph haz;     TMO=400 ./matrix.sh haz-user-raw "user-novma,user-eof:$S,user-protnone" &
                    TMO=400 ./matrix.sh haz-vext "user-novma,user-eof:$S,user-protnone,kern-novma,kern-eof:$S" $V; wait;;
whaz)   ph whaz;    TMO=400 ./matrix.sh whaz-user-raw "user-wro,user-novma,user-protnone" $W &
                    TMO=400 ./matrix.sh whaz-vext "user-wro,user-novma,user-protnone,kern-wro,kern-novma" $W $V; wait;;
kraw)   ph kraw;    TMO=300 ./matrix.sh kern-novma-raw "kern-novma" &
                    TMO=300 ./matrix.sh kern-wro-raw "kern-wro" $W; wait;;
contend) ph contend
        # malta does not boot these kernels with -smp 2 (with or without the
        # plugin); penguin runs 1 CPU, where preemption still contends
        ALL=${KERNELS:-$(cd $GEN && ls syms-*.env | sed 's/syms-\(.*\)\.env/\1/')}
        M=$(echo "$ALL" | grep mips | xargs); O=$(echo "$ALL" | grep -v mips | xargs)
        for n in raw vext; do o=""; [ $n = vext ] && o="$V"
          [ -n "$O" ] && KERNELS="$O" SMP=2 TMO=1500 ./matrix.sh contend-$n "kern-contend:/mnt/data.bin:100" $o &
          [ -n "$M" ] && KERNELS="$M" SMP=1 TMO=1500 ./matrix.sh contend-$n "kern-contend:/mnt/data.bin:100" $o &
        done; wait;;
debugw) ph debugw;  TMO=300 ./matrix.sh debugw "user-wzero,user-wro" $W wapi=debug;;
window) ph window;  KERNELS="${WKERNELS:-6.13-riscv64 6.13-powerpc64 6.13-powerpc64le 6.13-x86_64}" TMO=300 ./matrix.sh window-current "kern-file" mode=current;;
span)   ph span;    TMO=900 ./matrix.sh span "user-span:2,user-span:3,kern-span:2,kern-span:3" $V;;
wspan)  ph wspan;   TMO=400 ./matrix.sh wspan "user-wspan:3,kern-wspan:3,user-wspanro:3,kern-wspanro:3" $W $V;;
multi)  ph multi;   TMO=1200 ./matrix.sh multi "user-multi:0,user-multi:1,user-multi:2,kern-multi:0,kern-multi:1,kern-multi:2" op=multi $V &
                    TMO=900 ./matrix.sh multi-seq "user-multi:2,kern-multi:2" op=multi batch=off $V; wait;;
par)    ph par;     TMO=1500 smpsplit par "user-par:60,kern-par:60" $V;;
esac; done
echo "### done $(date +%T)"
