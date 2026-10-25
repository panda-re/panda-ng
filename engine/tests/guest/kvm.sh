#!/bin/sh
# kvm.sh [phase...]: full.sh on penguin's x86_64 kernels under KVM -> ../out-kvm
H=$(cd "$(dirname "$0")" && pwd)
. "$H/env.sh"
[ -f $GEN/initrd-x86_64-kvm.gz ] || ZFLAGS=-DPAGEIN_KVM SUFFIX=-kvm $H/mkinitrd.sh x86_64
ACCEL=kvm OUT=${OUT:-$OUTROOT/out-kvm} KERNELS="6.13-x86_64 4.10-x86_64" WKERNELS=6.13-x86_64 \
  XOPTS="kvm=on $XOPTS" exec $H/full.sh "$@"
