#!/usr/bin/env bash
# Write a development tree back out as the committed patch series.
#
#   ./scripts/export-series.sh 11.1.0 /tmp/qemu-dev
#
# Regenerates patches/<version>/*.patch and the series file from every commit
# above base-<tag>, SKIPPING the vendored-overlay commit that import-series.sh
# adds (its files are not part of the series by construction).
#
# WARNING, learned from linux_builder: this deletes patches/<version>/*.patch
# before rewriting. A failed export therefore destroys the committed series.
# Commit the repo before running it.
set -euo pipefail

VERSION="${1:?usage: export-series.sh <version> <workdir>}"
WORKDIR="${2:?}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PATCHES="$REPO_ROOT/patches"
TAG=$(python3 -c "import json;print(json.load(open('$REPO_ROOT/base.json'))['$VERSION']['tag'])")

if ! git -C "$REPO_ROOT" diff --quiet || ! git -C "$REPO_ROOT" diff --cached --quiet; then
    echo "refusing to export with uncommitted changes in $REPO_ROOT" >&2
    echo "(this rewrites patches/$VERSION/ in place; commit first so it is recoverable)" >&2
    exit 1
fi

STAGE=$(mktemp -d); trap 'rm -rf "$STAGE"' EXIT
# Determinism matters: the series is committed, so an export with no code
# change must produce no diff. --no-numbered drops the "nn/NN" counter (which
# would otherwise shift when the VENDORED commit is filtered out below),
# --zero-commit blanks the ephemeral dev-tree SHA, and --full-index pins blob
# hashes that git would otherwise abbreviate to a repo-size-dependent length.
git -C "$WORKDIR" format-patch --no-signature --no-numbered --zero-commit \
    --full-index -o "$STAGE" "base-${TAG}..HEAD" >/dev/null

rm -f "$PATCHES/$VERSION"/*.patch
: > "$PATCHES/$VERSION/series"
n=0
for f in "$STAGE"/*.patch; do
    # skip the vendored-overlay commits
    if grep -qm1 '^Subject: .*VENDORED:' "$f"; then continue; fi
    n=$((n + 1))
    # number what is kept: format-patch counted the VENDORED commits too, and
    # work done on top of the overlay would otherwise leave a gap in the series
    name=$(printf '%04d' "$n")-$(basename "$f" | cut -d- -f2-)
    cp "$f" "$PATCHES/$VERSION/$name"
    echo "$VERSION/$name" >> "$PATCHES/$VERSION/series"
done

echo "OK: exported $n patches for $VERSION"
echo "    the recorded patchedTree is now stale -- re-record it:"
echo "      ./scripts/verify-series.sh $VERSION <tarball> --record"
