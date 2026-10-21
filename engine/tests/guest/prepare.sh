#!/bin/sh
# prepare.sh: fetch the harness's inputs and build what it boots.
#   dl/kernels       linux_builder $LB_VERSION's kernels-latest.tar.gz (penguin's pin)
#   dl/busybox-dist  rehosting/busybox $BUSYBOX_REV #dist (nix)
#   dl/zig           a venv with ziglang $ZIG_VERSION (the cross compiler)
#   gen/plugins      plugins/*.c against $QSRC/include/plugins (needs QBUILD or QSRC)
#   gen/initrd-*.gz  per arch: busybox, pagetest (native and, for x86_64 and
#                    aarch64, a 32-bit build), init, the squashfs test data
#   gen/syms-*.env   per kernel, gen/sysprof-*.txt per 6.13 kernel, from its ISF
#   gen/ours-*       the takeover scenario's program (x86_64, aarch64)
# Steps whose output exists are skipped; remove it to redo one.
set -e
H=$(cd "$(dirname "$0")" && pwd)
. "$H/env.sh"
LB_VERSION=${LB_VERSION:-v4.0.1}
BUSYBOX_REV=${BUSYBOX_REV:-eeba468aa}
ZIG_VERSION=${ZIG_VERSION:-0.16.0}
ARCHES=${ARCHES:-"x86_64 aarch64 armel mipsel mipseb mips64el mips64eb powerpc64 powerpc64le riscv64 loongarch64"}
mkdir -p "$H/dl" "$GEN" "$PLUGINS"

if [ ! -d "$KERNEL_DIR/6.13" ]; then
  echo ">>> kernels: linux_builder $LB_VERSION"
  curl -sSL "https://github.com/rehosting/linux_builder/releases/download/$LB_VERSION/kernels-latest.tar.gz" |
    tar -xz -C "$(dirname "$KERNEL_DIR")"
fi
if [ ! -d "$BUSYBOX_DIST" ]; then
  echo ">>> busybox: rehosting/busybox $BUSYBOX_REV"
  nix build --out-link "$BUSYBOX_DIST" "github:rehosting/busybox/$BUSYBOX_REV#dist"
fi
if [ ! -x "$H/dl/zig/bin/python" ] && [ "$ZCC" = "$H/dl/zig/bin/python -m ziglang cc" ]; then
  echo ">>> zig $ZIG_VERSION"
  python3 -m venv "$H/dl/zig" && "$H/dl/zig/bin/pip" install -q "ziglang==$ZIG_VERSION"
fi
if [ -n "$QSRC" ]; then
  for p in pagein syscalls hooktest; do
    [ -f "$PLUGINS/lib$p.so" ] && continue
    echo ">>> plugin $p"
    gcc -shared -fPIC -O2 -Wall -o "$PLUGINS/lib$p.so" "$H/plugins/$p.c" \
        -I"$QSRC/include/plugins" $(pkg-config --cflags glib-2.0)
  done
else
  echo ">>> plugins skipped: set QBUILD (or QSRC)"
fi
. "$H/archs.sh"
for a in $ARCHES; do
  [ -f "$GEN/initrd-$a.gz" ] || "$H/mkinitrd.sh" $a
  for v in 4.10 6.13; do
    arch_facts $a $v >/dev/null || continue
    [ -f "$KERNEL_DIR/$v/$KFILE" ] || continue
    [ -f "$GEN/syms-$v-$a.env" ] || python3 "$H/syms.py" $v $a
    # syscall profiles: the 6.13 kernels (sctest.sh's)
    [ $v = 4.10 ] || [ -f "$GEN/sysprof-$v-$a.txt" ] || python3 "$H/sysprof.py" $v $a
  done
done
for a in x86_64 aarch64; do
  arch_facts $a 6.13
  [ -f "$GEN/ours-$a" ] || $ZCC -target $ZT -static -Os "$H/ours.c" -o "$GEN/ours-$a"
done
echo ">>> ready: $GEN"
