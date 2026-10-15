#!/bin/sh
# usage: t.sh <arch> <ver> <name> <tests> [plugin opts with @KWRITE @SEARCH @FIXUP @SIG @SIGINFO @EXTABLE]
# writes ../out-pk/<arch>-<ver>-<name>.{con,log}
H=$(cd "$(dirname "$0")" && pwd); A=$1; V=$2; N=$3; T=$4; shift 4
. "$H/env.sh"
. $GEN/syms-$V-$A.env
O=${OUT:-$OUTROOT/out-pk}; mkdir -p $O
OPTS=$(echo "$*" | sed "s/@KWRITE/$KWRITE/;s/@SEARCH/$SEARCH/;s/@FIXUP/$FIXUP/;s/@SIGINFO/$SIGINFO/;s/@SIG/$SIG/;s/@EXTABLE/$EXTABLE/")
PLUGIN=${PLUGINSO:-$PLUGINS/libpagein.so} LOG=$O/$A-$V-$N.log $H/run.sh $A $V $T $OPTS > $O/$A-$V-$N.con 2>&1
echo "== $A $V $N rc=$? ($T): $OPTS"
