#!/usr/bin/env bash
# tests/machine_raw.sh ENGINE_BIN [ARCH...]: the configurable machine's per-target CPU setup without
# PANDA. Runs machine.py's bare-metal programs (tests/programs/cfg_<prog>.S) on
# ENGINE_BIN/qemu-system-<arch> -M configurable and checks the program's console line, which
# says whether the CPU's timer moved (MIPS Count, PPC timebase, x86 TSC): "CFG-OK <prog> TIMER-OK".
# Before the per-target setup, MIPS and PPC dereferenced NULL in the host on the timer read.
# Also runs the targets pandare2's machine.py can't (mips64 and ppc64 with the 32-bit programs,
# i386/x86_64 in real mode) and checks that a target with no setup is refused with an error.
# Default ARCHs: mips mipsel mips64 mips64el ppc ppc64 i386 x86_64 aarch64 arm riscv64 loongarch64.
set -u
E=$(cd "$1" && pwd); shift
H=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d); fail=0
python3 -c "
import base64,sys; sys.path.insert(0,'$H/programs'); from cfg_programs import PROGRAMS
for a,p in PROGRAMS.items(): open('$T/cfg_%s.elf'%a,'wb').write(base64.b64decode(p))"
printf 'ROMDATA!' > $T/rom.bin
rom() { echo "region.0.name=boot,region.0.base=$1,region.0.size=0x1000,region.0.kind=rom,region.0.file=$T/rom.bin"; }
uart() { echo "device.0.type=serial-mm,device.0.id=uart0,device.0.mmio.0=$1,device.0.irq.0=$2,device.0.prop.chardev=serial0"; }
for a in ${@:-mips mipsel mips64 mips64el ppc ppc64 i386 x86_64 aarch64 arm riscv64 loongarch64}; do
  q=$E/qemu-system-$a
  [ -x $q ] || { echo "SKIP $a (no qemu-system-$a)"; continue; }
  want="TIMER-OK"
  case $a in
    mips|mipsel|mips64|mips64el) p=$(case $a in *el) echo mipsel;; *) echo mips;; esac)
      cpu=$(case $a in mips64*) echo MIPS64R2-generic;; *) echo 24Kf;; esac)
      M="configurable,ram-base=0x40000000,$(rom 0x1fc00000),region.1.name=io,region.1.base=0x1f010000,region.1.size=0x1000,region.1.kind=unimp,$(uart 0x1f000000 cpu:2)";;
    ppc|ppc64) p=ppc; cpu=$([ $a = ppc64 ] && echo 970fx || echo 750)
      M="configurable,ram-base=0x0,$(rom 0x1fc00000),region.1.name=io,region.1.base=0x10010000,region.1.size=0x1000,region.1.kind=unimp,$(uart 0x10000000 cpu:5)";;
    i386|x86_64) p=x86; cpu=$([ $a = i386 ] && echo qemu32 || echo qemu64)
      # no sysbus UART in the x86 targets: the program's bytes go to an unimp region, logged by -d unimp
      M="configurable,entry=0xfff0,region.0.name=bios,region.0.base=0xffff0000,region.0.size=0x10000,region.0.kind=rom,region.1.name=con,region.1.base=0x80000,region.1.size=0x1000,region.1.kind=unimp";;
    aarch64|arm) p=$a; cpu=$([ $a = arm ] && echo cortex-a15 || echo cortex-a53); want="CFG-OK $a"
      M="configurable,ram-base=0x40000000,$(rom 0),device.0.type=pl011,device.0.id=uart0,device.0.mmio.0=0x09000000,device.0.irq.0=cpu:0,device.0.prop.chardev=serial0";;
    riscv64) p=riscv64; cpu=rv64; want="CFG-OK riscv64"
      M="configurable,ram-base=0x80000000,$(rom 0x20000000),$(uart 0x10000000 cpu:11)";;
    *) # a target with no CPU setup: refused, naming it
      out=$(timeout 10 $q -M configurable -display none -monitor none -serial null 2>&1); rc=$?
      if [ $rc = 1 ] && grep -q "no CPU setup for target $a" <<< "$out"; then echo "PASS $a: refused ($out)"
      else echo "FAIL $a: rc=$rc $out"; fail=1; fi
      continue;;
  esac
  m=16M; [ $p = x86 ] && m=0
  out=$(timeout 5 $q -M "$M" -cpu $cpu -m $m -kernel $T/cfg_$p.elf -display none -monitor none -serial stdio \
        $([ $p = x86 ] && echo -d unimp) 2>&1); rc=$?
  [ $p = x86 ] && out="$out
$(python3 -c "import re,sys; print(''.join(chr(int(c,16)) for c in re.findall(r'con: write of 1 bytes \((0x[0-9a-f]+)\)', sys.stdin.read())))" <<< "$out")"
  if grep -q "$want" <<< "$out" && [ $rc -lt 128 -o $rc = 124 ]; then echo "PASS $a ($cpu): $(grep -o 'CFG-OK.*' <<< "$out" | head -1)"
  else echo "FAIL $a ($cpu): rc=$rc $(echo "$out" | head -5)"; fail=1; fi
done
rm -rf $T
exit $fail
