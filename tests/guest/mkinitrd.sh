#!/bin/sh
# usage: mkinitrd.sh <penguin arch>   -> initrd-<arch>${SUFFIX}.gz  (env: ZFLAGS, SUFFIX)
set -e
H=$(cd "$(dirname "$0")" && pwd); A=$1
. "$H/env.sh"
. $H/archs.sh; arch_facts $A
BB=$BUSYBOX_DIST/$A/busybox
R=$(mktemp -d); mkdir -p "$R"/bin "$R"/proc "$R"/sys "$R"/dev "$R"/mnt
cp $BB "$R"/bin/busybox; chmod 755 "$R"/bin/busybox
$ZCC -target $ZT -static -O2 -pthread $ZFLAGS "$H"/pagetest.c -o "$R"/pagetest 2>/dev/null
D=$(mktemp -d)
python3 - "$D" <<'PY'
import sys
d=sys.argv[1]
with open(d+'/data.bin','w') as f:
    for pg in range(8):
        line=''.join(f'[pg{pg} off{o:04x}]' for o in range(0,4096,16))
        f.write(line[:4096])
open(d+'/small.bin','w').write('tiny file, well under one page\n')
with open(d+'/span.bin','w') as f:
    for b in range(64):
        f.write(''.join(f'[sp{b:02d} off{o:04x}]..' for o in range(0,4096,16)))
PY
mksquashfs "$D" "$R"/data.sqsh -comp gzip -noI -noD -noF -noX -quiet -no-progress >/dev/null
rm -rf "$D"
case $A in   # a 32-bit build for compat syscalls (c32- tests)
  x86_64) ZT32=x86-linux-musl;; aarch64) ZT32=arm-linux-musleabi;; *) ZT32="";;
esac
if [ -n "$ZT32" ]; then
  mkdir -p "$R"/c32
  $ZCC -target $ZT32 -static -O2 -pthread "$H"/pagetest.c -o "$R"/c32/pagetest
fi
cp "$H"/init "$R"/init; chmod 755 "$R"/init
case $A in x86_64|aarch64|armel|riscv64|mipsel)   # PIE workload for the CF_PCREL measurement
  python3 "$H"/mkpiework.py 2000 > "$R"/piework.c
  $ZCC -target $ZT -O1 -fPIE -pie -nostdlib -ffreestanding -fno-stack-protector "$R"/piework.c -o "$R"/piework 2>/dev/null
  rm "$R"/piework.c;;
esac
cp "$R"/pagetest "$GEN"/pagetest-$A$SUFFIX   # for symbol addresses (hksyms.sh)
(cd "$R" && find . | cpio -o -H newc 2>/dev/null | gzip -1 > "$GEN"/initrd-$A$SUFFIX.gz)
rm -rf "$R"
echo built "$GEN"/initrd-$A$SUFFIX.gz
