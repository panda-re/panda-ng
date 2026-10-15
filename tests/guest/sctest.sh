#!/bin/sh
# sctest.sh [func|bench]...: syscall hooks (plugin/syscalls.c) on 6.13 kernels
# env: OUT (default ../out-sctest), ARCHS, PAR
#   func   guest sc-func with sc-rules.txt, 1 vCPU and (not mips) 2 vCPUs;
#          verdicts from sctest.py, exit status 1 on any failure
#   bench  guest sc-bench: ns per getppid / getpgid / getsid under rule sets,
#          and sc-tbench (in-guest ftrace syscall tracepoints), REPS times each
#   tfunc  sc-func in trap mode (QEMU syscall events), 1 and 2 vCPUs
#   sctrap guest sc-trap (signals, restarts, vfork, execve, threads) with
#          sc-trap-rules.txt in trap mode, natively and from the 32-bit build
#   trapbench  bench's rule sets in trap mode
#   pen    the penguin_syscall_* hooks under pyhost.py: sc-func (1 and 2
#          vCPUs), sc-trap, sc-pen, sc-fault (native and 32-bit; and natively
#          with the callbacks on penguin's own apis/mem.py), sc-reap (native
#          and 32-bit), 3500 execs
#   pyplug penguin's own Syscalls plugin (apis/syscalls.py, backend=trap) and
#          mem.py on penguin's qemu_compat, under penhost.py: sc-pen, sc-fault,
#          sc-reap, native and 32-bit
#   penbench   bench's rule sets as penguin_syscall_* hooks
#   pybench    bench's rule sets as hooks of penguin's Syscalls (backend=trap)
# trap mode needs QEMU syscall events: x86_64 and aarch64 so far
# env: REPS (default 3)
H=$(cd "$(dirname "$0")" && pwd); cd $H
. "$H/env.sh"
O=${OUT:-$OUTROOT/out-sctest}; mkdir -p $O
P=$PLUGINS/libsyscalls.so
ALL="x86_64 aarch64 armel mipsel mipseb mips64el mips64eb powerpc64 powerpc64le riscv64 loongarch64"
ARCHS=${ARCHS:-$ALL}
PAR=${PAR:-6}
REPS=${REPS:-3}

# one <name> <arch> <guest test> <rules file|none>   (env: MODE)
one() {
  n=$1; a=$2; t=$3; r=$4
  s0=$(date +%s.%N)
  if [ -n "$PYHOST$PENHOST" ]; then
    SMP=${SMP:-1} PROF=$GEN/sysprof-6.13-$a.txt LOG=$O/$a-$n.log TMO=600 ./run.sh $a 6.13 $t \
      < /dev/null > $O/$a-$n.con 2>&1
  elif [ "$r" = none ]; then
    SMP=${SMP:-1} TMO=600 ./run.sh $a 6.13 $t < /dev/null > $O/$a-$n.con 2>&1
  else
    SMP=${SMP:-1} PLUGIN=$P LOG=$O/$a-$n.log TMO=600 ./run.sh $a 6.13 $t \
      profile=$GEN/sysprof-6.13-$a.txt rules=$r ${MODE:+mode=$MODE} < /dev/null > $O/$a-$n.con 2>&1
  fi
  rc=$?
  echo "RC $rc" >> $O/$a-$n.con
  echo "WALL $(echo "$(date +%s.%N) - $s0" | bc)" >> $O/$a-$n.con
}

# bench rule sets: name|rules (one per line, ';' separated)
BENCH="empty|
one|getppid count
all|* count
fail|getpgid count a0=4242
pass|getpgid count a0=0
ret|getppid count return
comm|getppid count comm=pagetest
allret|* count return
log|getppid log"

func() { a=$1
  one func $a sc-func $H/sc-rules.txt
  case $a in mips*) ;; *) (SMP=2; one func-smp $a sc-func $H/sc-rules.txt);; esac; }
echo "$BENCH" | while IFS='|' read n rs; do echo "$rs" | tr ';' '\n' > $O/rules-$n.txt; done
bench() { a=$1
  for k in $(seq $REPS); do
    one bench-none-$k $a sc-bench:200000 none
    one tbench-$k $a sc-tbench:200000 none
    echo "$BENCH" | while IFS='|' read n rs; do
      # every logged event is a line: fewer calls
      N=200000; [ $n = log ] && N=20000
      one bench-$n-$k $a sc-bench:$N $O/rules-$n.txt
    done
  done; }

TRAPARCHS="x86_64 aarch64"
tfunc() { a=$1
  case " $TRAPARCHS " in *" $a "*) ;; *) return;; esac
  (MODE=trap; one tfunc $a sc-func $H/sc-rules.txt
   SMP=2; one tfunc-smp $a sc-func $H/sc-rules.txt); }
sctrap() { a=$1
  case " $TRAPARCHS " in *" $a "*) ;; *) return;; esac
  (MODE=trap; SMP=2; one trap $a sc-trap $H/sc-trap-rules.txt
   one trap32 $a c32-sc-trap $H/sc-trap-rules.txt
   SMP=1; one evict $a sc-execs:3500 $H/sc-evict-rules.txt); }
trapbench() { a=$1
  case " $TRAPARCHS " in *" $a "*) ;; *) return;; esac
  for k in $(seq $REPS); do
    echo "$BENCH" | while IFS='|' read n rs; do
      N=200000; [ $n = log ] && N=20000
      MODE=trap one trapbench-$n-$k $a sc-bench:$N $O/rules-$n.txt
    done
  done; }

pen() { a=$1
  case " $TRAPARCHS " in *" $a "*) ;; *) return;; esac
  (export PYHOST=func; one pfunc $a sc-func -; SMP=2 one pfunc-smp $a sc-func -
   PYHOST=trap SMP=2 one ptrap $a sc-trap -; PYHOST=trap one ptrap32 $a c32-sc-trap -
   PYHOST=pen one pen $a sc-pen -; PYHOST=pen one pen32 $a c32-sc-pen -
   PYHOST=fault one pfault $a sc-fault -; PYHOST=fault one pfault32 $a c32-sc-fault -
   PYHOST=pmem one pmem $a sc-fault -
   PYHOST=reap one preap $a sc-reap -; PYHOST=reap one preap32 $a c32-sc-reap -
   # the table's eviction: without the reaper, which drops each exec'd child's call
   PYHOST=evict NOREAP=1 one pevict $a sc-execs:3500 -); }
pyplug() { a=$1     # penguin's Syscalls with backend=trap, under penhost.py
  case " $TRAPARCHS " in *" $a "*) ;; *) return;; esac
  (export PENHOST=pen; one npen $a sc-pen -; one npen32 $a c32-sc-pen -
   PENHOST=fault one nfault $a sc-fault -; PENHOST=fault one nfault32 $a c32-sc-fault -
   PENHOST=reap one nreap $a sc-reap -; PENHOST=reap one nreap32 $a c32-sc-reap -); }
pybench() { a=$1    # bench's rule sets as hooks of penguin's Syscalls (backend=trap)
  case " $TRAPARCHS " in *" $a "*) ;; *) return;; esac
  for k in $(seq $REPS); do
    echo "$BENCH" | while IFS='|' read n rs; do
      N=200000; [ $n = log ] && N=20000
      PENHOST=bench-$n one pybench-$n-$k $a sc-bench:$N -
    done
  done; }
penbench() { a=$1
  case " $TRAPARCHS " in *" $a "*) ;; *) return;; esac
  for k in $(seq $REPS); do
    echo "$BENCH" | while IFS='|' read n rs; do
      N=200000; [ $n = log ] && N=20000
      PYHOST=bench-$n one penbench-$n-$k $a sc-bench:$N -
    done
  done; }

for p in ${*:-func bench}; do
  i=0
  for a in $ARCHS; do
    $p $a &
    i=$((i + 1)); [ $((i % PAR)) = 0 ] && wait
  done; wait
done
python3 $H/sctest.py $O
