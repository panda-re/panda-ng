#!/usr/bin/env bash
# Taint, and the record/replay under it, on every arch: small busybox guests (tests/mkbusybox.py)
# built with the PANDA under test, then TESTS through tests/run.sh. About a minute for all of them
# on 4 cores, after a 330 MB download (cached in DL).
#
#   nix build .#python -o result-python
#   PANDA_PYTHON=$PWD/result-python/bin/python3 tests/busybox.sh [arch...]
#
# Environment:
#   PANDA_PYTHON  Python with pandare2 (default: panda-python from PATH)
#   DL            downloads: linux_builder's kernels, rehosting/busybox (default ~/.cache/panda-busybox)
#   IMAGES        where the images are built (default $OUT/images)
#   TESTS         default "taint taint_mmio taint_errors"
#   OUT, JOBS     as for run.sh (JOBS default 4)
# Arches: default all of mkbusybox's.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# engine/tests/guest/prepare.sh's pins
LB_VERSION=v4.0.1
BUSYBOX_REV=eeba468aa
# rehosting/busybox has no 32-bit powerpc; Debian's port does
PPC32_DEB=http://ftp.ports.debian.org/debian-ports/pool-powerpc/main/b/busybox/busybox-static_1.38.0-3_powerpc.deb
PPC32_SHA256=a0326ff75b3aaa94b0a5e3edec8a7160230a622dab9723ec41cc4c49b6c60d3f

PANDA_PYTHON=${PANDA_PYTHON:-panda-python}
DL=${DL:-$HOME/.cache/panda-busybox}
OUT=${OUT:-${TMPDIR:-/tmp}/panda-busybox-$(date +%Y%m%d-%H%M%S)}
IMAGES=${IMAGES:-$OUT/images}
export TESTS=${TESTS:-taint taint_mmio taint_errors} JOBS=${JOBS:-4} OUT PANDA_PYTHON
ARCHES=${*:-x86_64 aarch64 armel mipsel mipseb mips64el mips64eb powerpc powerpc64 powerpc64le riscv64 loongarch64}

mkdir -p "$DL" "$IMAGES"
if [ ! -d "$DL/kernels/6.13" ]; then
  echo ">>> kernels: linux_builder $LB_VERSION"
  curl -fsSL "https://github.com/rehosting/linux_builder/releases/download/$LB_VERSION/kernels-latest.tar.gz" |
    tar -xz -C "$DL" kernels/6.13
fi
if [ ! -d "$DL/busybox/x86_64" ]; then
  echo ">>> busybox: rehosting/busybox $BUSYBOX_REV"
  nix build --out-link "$DL/busybox-dist" "github:rehosting/busybox/$BUSYBOX_REV#dist"
  mkdir -p "$DL/busybox"
  for d in "$DL"/busybox-dist/*; do ln -sfn "$(readlink -f "$d")" "$DL/busybox/$(basename "$d")"; done
fi
if [[ " $ARCHES " == *" powerpc "* ]] && [ ! -x "$DL/busybox/powerpc/busybox" ]; then
  echo ">>> busybox: Debian powerpc busybox-static"
  t=$(mktemp -d)
  curl -fsSL -o "$t/bb.deb" "$PPC32_DEB"
  echo "$PPC32_SHA256  $t/bb.deb" | sha256sum -c --quiet
  (cd "$t" && ar x bb.deb && tar -xf data.tar.* ./usr/bin/busybox)
  mkdir -p "$DL/busybox/powerpc" && cp "$t/usr/bin/busybox" "$DL/busybox/powerpc/"
  rm -rf "$t"
fi

# shellcheck disable=SC2086
"$PANDA_PYTHON" "$HERE/mkbusybox.py" $ARCHES -o "$IMAGES" --kernels "$DL/kernels" --busybox "$DL/busybox"
# shellcheck disable=SC2046
"$HERE/run.sh" $(for a in $ARCHES; do echo "$IMAGES/busybox-$a.json"; done)
