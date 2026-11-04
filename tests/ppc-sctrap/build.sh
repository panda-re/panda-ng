#!/usr/bin/env bash
# Build sct.c for ppc32, ppc64 (ELFv2, big-endian) and ppc64le: freestanding, static.
#   tests/ppc-sctrap/build.sh <outdir>      (CLANG, LLD: the compiler and ld.lld to use)
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
OUT=${1:?outdir}
CLANG=${CLANG:-clang}
LLD=${LLD:-ld.lld}
mkdir -p "$OUT"
CFLAGS=(-O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -nostdlib -static
        -fuse-ld=lld "--ld-path=$LLD" -Wl,--no-dynamic-linker -Wall)
"$CLANG" -target powerpc-linux-gnu "${CFLAGS[@]}" -o "$OUT/sct32" "$HERE/sct.c"
"$CLANG" -target powerpc64-linux-gnu -mabi=elfv2 "${CFLAGS[@]}" -o "$OUT/sct64" "$HERE/sct.c"
"$CLANG" -target powerpc64le-linux-gnu "${CFLAGS[@]}" -o "$OUT/sct64le" "$HERE/sct.c"
