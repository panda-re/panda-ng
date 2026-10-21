#!/usr/bin/env bash
# Turn tarball + series into a git tree you can develop in normally.
#
#   ./scripts/import-series.sh 11.1.0 <tarball> /tmp/qemu-dev
#
# The committed series stays the source of truth; this tree is scratch. Each
# patch becomes one commit, and the pristine tarball is tagged base-<tag> so
# export-series.sh knows where the series starts.
set -euo pipefail

VERSION="${1:?usage: import-series.sh <version> <tarball> <workdir>}"
TARBALL="${2:?}"
WORKDIR="${3:?}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PATCHES="$REPO_ROOT/patches"
# shellcheck source=scripts/lib-extract.sh
. "$REPO_ROOT/scripts/lib-extract.sh"
TAG=$(python3 -c "import json;print(json.load(open('$REPO_ROOT/base.json'))['$VERSION']['tag'])")

[ -e "$WORKDIR" ] && { echo "refusing to overwrite existing $WORKDIR" >&2; exit 1; }
extract_tarball "$TARBALL" "$WORKDIR"
cd "$WORKDIR"
git init -q .
git add -A
git -c user.email=dev@local -c user.name=dev commit -qm "pristine qemu-${VERSION}"
git tag "base-${TAG}"

n=0
while read -r p; do
    case "$p" in ''|\#*) continue ;; esac
    n=$((n + 1))
    git -c user.email=dev@local -c user.name=dev am -q "$PATCHES/$p"
done < "$PATCHES/$VERSION/series"

# The vendored half is not in the series; overlay it so the tree actually builds.
cp -r "$REPO_ROOT/src/." .
mkdir -p configs && cp -r "$REPO_ROOT/configs/." configs/
install -m 0755 "$REPO_ROOT/build.sh" build.sh
git add -A
git -c user.email=dev@local -c user.name=dev commit -qm "VENDORED: src/ + configs/ + build.sh (NOT part of the series -- do not export)"

echo "OK: $n patches + vendored overlay at $WORKDIR (base tag: base-${TAG})"
echo "    NOTE: the last commit is the vendored overlay. Develop on top of it;"
echo "    export-series.sh ignores it when regenerating the series."
