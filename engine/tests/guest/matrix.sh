#!/bin/sh
# matrix.sh <name> <tests> [extra opts] : run on every kernel in parallel (env: KERNELS, SMP, TMO, XOPTS)
H=$(cd "$(dirname "$0")" && pwd); cd $H
. "$H/env.sh"
N=$1; T=$2; shift 2
C="marker=on kwrite=@KWRITE sig=@SIG siginfo=@SIGINFO tries=4 mode=user $XOPTS"
K=${KERNELS:-$(cd $GEN && ls syms-*.env | sed 's/syms-\(.*\)\.env/\1/')}
for k in $K; do v=${k%%-*}; a=${k#*-}
  ./t.sh $a $v $N "$T" $C "$@" > /dev/null &
done
wait
