#!/bin/sh
# ositest.sh [out dir]: host-side OSI's correctness (apis/hostosi.py on
# system/penguin-osi.c, the engine's fd-path and exe filters), against the
# guest's own /proc, per kernel and ABI.  Exit 1 if any run is not ok.
#   CONFIGS="x86_64:6.13 aarch64:6.13 ..."  arch:kernel pairs (x86_64:4.10 is left
#     out: the syscall trap core delivers it one event, then none)
#   ABIS="osi c32-osi"                       the native and 32-bit tests
# Each run: <out>/<arch>-<kernel>-<test>.{con,log,cmp}; the table: <out>/summary.txt
H=$(cd "$(dirname "$0")" && pwd)
. "$H/env.sh"
O=${1:-$OUTROOT/out-ositest}
CONFIGS=${CONFIGS:-"x86_64:6.13 aarch64:6.13 aarch64:4.10"}
ABIS=${ABIS:-"osi c32-osi"}
mkdir -p "$O"
python3 "$H/osicdef.py" || exit 1
for c in $CONFIGS; do a=${c%:*}; v=${c#*:}
  for t in $ABIS; do
    (PENHOST=osi LOG=$O/$a-$v-$t.log TMO=${TMO:-600} "$H/run.sh" $a $v $t \
       < /dev/null > "$O/$a-$v-$t.con" 2>&1
     python3 "$H/osicmp.py" "$O/$a-$v-$t.con" "$O/$a-$v-$t.log" > "$O/$a-$v-$t.cmp" 2>&1) &
  done
done
wait
rc=0
for c in $CONFIGS; do a=${c%:*}; v=${c#*:}
  for t in $ABIS; do
    f=$O/$a-$v-$t.cmp
    r=$(grep '^RESULT' "$f" | cut -d' ' -f2-)
    keys=$(grep '^enter:' "$f" | cut -d' ' -f4)
    filt=$(grep '^filters:' "$f" | cut -d' ' -f2)
    mat=$(grep '^matcher:' "$f" | cut -d' ' -f2)
    printf '%-8s %-5s %-8s keys=%-6s filters=%-4s matcher=%-9s %s\n' \
      $a $v $t "${keys:--}" "${filt:--}" "${mat:--}" "${r:-no result}"
    [ "$r" = ok ] || rc=1
  done
done | tee "$O/summary.txt"
grep -qv ' ok$' "$O/summary.txt" && rc=1
exit $rc
