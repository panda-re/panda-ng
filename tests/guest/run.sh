#!/bin/sh
# usage: run.sh <arch> <4.10|6.13> <tests,comma,sep> [plugin args...]  (env: SMP PLUGIN LOG TMO QBUILD ACCEL=kvm QEXTRA)
# PYHOST=<scenario> PROF=<profile>: QEMU in-process under pyhost.py (penguin_syscall_* hooks)
# PENHOST=<scenario>: under penhost.py (penguin's qemu_compat, Syscalls with backend=trap, mem)
H=$(cd "$(dirname "$0")" && pwd)
. "$H/env.sh"
: "${QBUILD:?set QBUILD to a QEMU build directory (see README.md)}"; export QBUILD
A=$1; V=$2; T=$3; shift 3
. $H/archs.sh; arch_facts $A $V || exit 2
K=$KERNEL_DIR/$V/$KFILE
SW=$(mktemp -p ${TMPDIR:-/tmp} swap.XXXXXX); truncate -s 64M $SW
case $DISK in
  mmio) DRV="-device virtio-blk-device,drive=hd0 -drive file=$SW,id=hd0,format=raw,if=none,cache=unsafe";;
  pcileg) DRV="-device virtio-blk-pci,drive=hd0,disable-modern=on,disable-legacy=off -drive file=$SW,id=hd0,format=raw,if=none,cache=unsafe";;
  virtio) DRV="-drive file=$SW,id=hd0,format=raw,if=virtio,cache=unsafe";;
esac
INITRD=$GEN/initrd-$A.gz
if [ "$ACCEL" = kvm ]; then
  # penguin's KVM runs use the host CPU; the int3 marker build of pagetest
  CPU=${KCPU:-host}; EXTRA="$EXTRA -accel kvm"; INITRD=${KINITRD:-$GEN/initrd-$A-kvm.gz}
fi
P=""
if [ -n "$PLUGIN" ]; then
  E=""; [ -n "$ENDIAN" ] && E="endian=$ENDIAN"
  P="-plugin $PLUGIN,$(echo "$E $*" | xargs | tr ' ' ',') -d plugin -D ${LOG:-$H/plugin.log}"
  P=$(echo "$P" | sed 's/,,*/,/g; s/,\( \|$\)/\1/')
fi
RUN=$QBUILD/qemu-system-$QARCH
if [ -n "$PENHOST" ]; then
  # penguin's own stack: qemu_compat + Syscalls (trap backend) + mem
  case $A in aarch64) KA=arm64;; *) KA=$A;; esac
  RUN="python3 $H/penhost.py $QBUILD/libqemu-system-$QARCH.so $A $KERNEL_DIR/$V/cosi.$KA.json.xz $PENHOST ${LOG:-$H/plugin.log} -- qemu-system-$QARCH -L $QBUILD/pc-bios -L $QBUILD/../pc-bios"
elif [ -n "$PYHOST" ]; then
  RUN="python3 $H/pyhost.py $QBUILD/libqemu-system-$QARCH.so $PROF $PYHOST ${LOG:-$H/plugin.log} -- qemu-system-$QARCH -L $QBUILD/pc-bios -L $QBUILD/../pc-bios"
fi
timeout ${TMO:-300} $RUN -M $MACH ${CPU:+-cpu $CPU} -m $MEM -smp ${SMP:-1} -nographic -no-reboot $EXTRA \
   -kernel $K -initrd $INITRD $DRV \
   $QEXTRA -append "console=$CON nokaslr norandmaps panic=-1 oops=panic quiet pt=$T" $P
rc=$?; rm -f $SW; exit $rc
