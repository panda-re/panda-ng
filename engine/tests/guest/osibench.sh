#!/bin/sh
# osibench.sh [out dir]: what a hooked write(2) costs under the engine's OSI
# filters (penhost.py's BENCH sets, sc-fdbench in the guest), against no
# hook; then osibench.py's table (<out>/summary.txt).
#   ARCHS="x86_64 aarch64"  KERNEL=6.13  REPS=5  N=300000 (writes per run)
#   SETS="empty wnum wfd wfdx wfdnear wfdstart wexe wfdpass"
# Arches run in parallel, each one's runs in sequence (a run's timing is
# its own vCPU's; two arches share the host).
H=$(cd "$(dirname "$0")" && pwd)
. "$H/env.sh"
O=${1:-$OUTROOT/out-osibench}
ARCHS=${ARCHS:-"x86_64 aarch64"}
SETS=${SETS:-"empty wnum wfd wfdx wfdnear wfdstart wexe wfdpass"}
mkdir -p "$O"
for a in $ARCHS; do
  (for k in $(seq ${REPS:-5}); do
     for n in $SETS; do
       PENHOST=bench-$n LOG=$O/$a-$n-$k.log TMO=${TMO:-600} "$H/run.sh" $a ${KERNEL:-6.13} \
         sc-fdbench:${N:-300000} < /dev/null > "$O/$a-$n-$k.con" 2>&1
     done
   done) &
done
wait
python3 "$H/osibench.py" "$O" $ARCHS | tee "$O/summary.txt"
