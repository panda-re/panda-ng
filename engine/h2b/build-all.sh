#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Build every H2b library in engine/h2b/sources whose target the engine build has.
#
#   engine/h2b/build-all.sh <meson build dir> <out dir>
#
# Writes <out dir>/libtaint-h2b-<arch>.so, which taint.c loads by default from
# <libpanda's dir>/panda-taint/ (PANDA_TAINT_H2B overrides; empty disables). Tools as build.sh.
set -euo pipefail

BUILD=$1 OUT=$2
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
mkdir -p "$OUT"
grep -v '^#' "$HERE/sources" | while read -r arch srcs; do
    [ -n "$arch" ] || continue
    if [ ! -e "$BUILD/libpanda-$arch-softmmu.so" ]; then
        echo "h2b: $arch: not in this build, skipped"
        continue
    fi
    # shellcheck disable=SC2086
    bash "$HERE/build.sh" "$arch" "$BUILD" "$OUT/libtaint-h2b-$arch.so" $srcs
done
