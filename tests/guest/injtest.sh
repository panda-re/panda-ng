#!/bin/sh
# injtest.sh [out dir]: syscall injection (Syscalls.inject on the trap
# engine's SYSCALL command), per kernel and ABI: pagetest sc-inject checks
# itself (SC check lines) against penhost.py's "inject" scenario.  Exit 1 if
# any run is not ok.
#   CONFIGS="x86_64:6.13 aarch64:6.13 ..."  arch:kernel pairs (x86_64:4.10 is left
#     out: the syscall trap core delivers it one event, then none)
#   ABIS="sc-inject c32-sc-inject"           native, and 32-bit (refused)
# Each run: <out>/<arch>-<kernel>-<test>.{con,log}; the table: <out>/summary.txt
H=$(cd "$(dirname "$0")" && pwd)
. "$H/env.sh"
O=${1:-$OUTROOT/out-injtest}
CONFIGS=${CONFIGS:-"x86_64:6.13 aarch64:6.13 aarch64:4.10"}
ABIS=${ABIS:-"sc-inject c32-sc-inject"}
mkdir -p "$O"
for c in $CONFIGS; do a=${c%:*}; v=${c#*:}
  for t in $ABIS; do
    PENHOST=inject LOG=$O/$a-$v-$t.log TMO=${TMO:-300} "$H/run.sh" $a $v $t \
      < /dev/null > "$O/$a-$v-$t.con" 2>&1 &
  done
done
wait
for c in $CONFIGS; do a=${c%:*}; v=${c#*:}
  for t in $ABIS; do
    con=$O/$a-$v-$t.con; log=$O/$a-$v-$t.log
    n=$(grep -c "^SC check " "$con"); bad=$(tr -d "\r" < "$con" | grep "^SC check .* FAIL" | cut -d" " -f3 | xargs)
    ex=$(tr -d "\r" < "$con" | sed -n "s/^EXIT $t //p"); us=$(sed -n 's/.*us_per_call=\([0-9.]*\).*/\1/p' "$log")
    left=$(sed -n 's/^SC exit pending=\([0-9]*\).*/\1/p' "$log")
    r=ok
    { [ "$n" -gt 0 ] && [ -z "$bad" ] && [ "$ex" = 0 ] && [ "$left" = 0 ]; } || r="FAIL${bad:+ $bad}"
    printf '%-8s %-5s %-14s checks=%-2s exit=%-2s pending=%-2s us_per_call=%-5s %s\n' \
      $a $v $t "$n" "${ex:--}" "${left:--}" "${us:--}" "$r"
  done
done | tee "$O/summary.txt"
! grep -qv ' ok$' "$O/summary.txt"
