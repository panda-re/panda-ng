#!/bin/sh
# setpctest.sh [out dir]: qemu_plugin_set_pc() from a syscall entry callback
# (plugins/scsetpc.c on guest sc-bench): the skipped getppid must not be
# delivered again.  One line per arch; exit 1 if any is not PASS.
#   ARCHS="x86_64 aarch64 armel"   the arches with QEMU syscall events
H=$(cd "$(dirname "$0")" && pwd)
. "$H/env.sh"
O=${1:-$OUTROOT/out-setpctest}
ARCHS=${ARCHS:-"x86_64 aarch64 armel"}
mkdir -p "$O"
rc=0
for a in $ARCHS; do
  PLUGIN=$PLUGINS/libscsetpc.so LOG=$O/$a.log TMO=${TMO:-300} "$H/run.sh" $a 6.13 sc-bench:2000 n=50 \
    < /dev/null > "$O/$a.con" 2>&1
  r=$(grep -h '^SETPC' "$O/$a.log" | head -1)
  printf '%-8s %s\n' $a "${r:-no SETPC line: the plugin or the guest did not run}"
done | tee "$O/summary.txt"
grep -qv 'PASS$' "$O/summary.txt" && rc=1
exit $rc
