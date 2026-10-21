#!/usr/bin/env bash
# The off-cost gate: build stock QEMU and this series from the same tarball with the same configure
# line, then run tests/guest/offcost.sh on both.
#
#   STOCK_REF=origin/main ./scripts/offcost-gate.sh 11.1.0 <tarball> <workdir> [out]
#
# Stock is the series at STOCK_REF (default origin/main: what penguin ships), imported with that
# ref's own scripts. Not the pristine tarball: penguin's arm kernels issue the hypercall
# instruction the series handles, and stock 11.1.0 kills them with an undefined-instruction oops.
# STOCK_REF=pristine uses the bare tarball anyway (ARCHS=x86_64 only).
# <workdir>/stock and <workdir>/series are import-series.sh trees. Both build
# qemu-system-{x86_64,aarch64} with the guest harness's configure flags plus -Db_staticpic=true
# (patch 0002 makes that the series' default; passing it to both keeps code layout comparable).
# The guest inputs must already be prepared (tests/guest/prepare.sh, or KERNEL_DIR/GEN set).
# env: JOBS (ninja -j), and offcost.sh's ARCHS REPS TOL RRTOL TMO.
set -euo pipefail

VERSION="${1:?usage: offcost-gate.sh <version> <tarball> <workdir> [out]}"
TARBALL="${2:?}"
WORKDIR="${3:?}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${4:-$WORKDIR/out}"
# shellcheck source=scripts/lib-extract.sh
. "$REPO_ROOT/scripts/lib-extract.sh"

STOCK_REF="${STOCK_REF:-origin/main}"
if [ ! -e "$WORKDIR/stock" ]; then
    if [ "$STOCK_REF" = pristine ]; then
        extract_tarball "$TARBALL" "$WORKDIR/stock"
    else
        mkdir -p "$WORKDIR/stock-repo"
        git -C "$REPO_ROOT" archive "$STOCK_REF" | tar x -C "$WORKDIR/stock-repo"
        "$WORKDIR/stock-repo/scripts/import-series.sh" "$VERSION" "$TARBALL" "$WORKDIR/stock"
    fi
fi
[ -e "$WORKDIR/series" ] || "$REPO_ROOT/scripts/import-series.sh" "$VERSION" "$TARBALL" "$WORKDIR/series"

for t in stock series; do
    mkdir -p "$WORKDIR/$t/build"
    (
        cd "$WORKDIR/$t/build"
        [ -f config.status ] || ../configure --target-list=x86_64-softmmu,aarch64-softmmu \
            --enable-plugins --disable-docs --disable-werror --disable-gtk --disable-sdl \
            --disable-opengl --disable-virglrenderer --disable-spice --disable-vnc \
            -Db_staticpic=true > configure.log 2>&1
        ninja ${JOBS:+-j$JOBS} qemu-system-x86_64 qemu-system-aarch64 > build.log 2>&1
    ) || { echo "offcost: $t build failed, see $WORKDIR/$t/build/" >&2; exit 2; }
done

exec "$REPO_ROOT/tests/guest/offcost.sh" "$WORKDIR/stock/build" "$WORKDIR/series/build" "$OUT"
