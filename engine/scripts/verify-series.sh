#!/usr/bin/env bash
# The gate. Applies patches/<version>/series to the PRISTINE upstream QEMU
# release tarball and proves the result is exactly what we expect.
#
#   ./scripts/verify-series.sh 11.1.0 /path/to/qemu-11.1.0.tar.xz
#   ./scripts/verify-series.sh 11.1.0 <tarball> --record   # (re)record patchedTree
#
# Three assertions, in increasing strength:
#
#   1. the tarball is the one base.json pins       (sha256)
#   2. the series applies with STRICT context      (git am, no --3way)
#   3. the patched tree hashes to base.json's `patchedTree`
#
# (3) is the one that earns its keep. linux_builder's 6db7363 had to repair a
# patch that was CORRUPT rather than stale -- a corruption that still applied.
# A recorded tree hash makes that unrepresentable: any change to what the series
# produces, from any cause, fails the gate.
#
# STRICT CONTEXT IS DELIBERATE. `git am` without --3way is used on purpose: a
# patch that needs 3-way resolution means upstream moved under us, and we want
# that to be a loud failure that a human ports, not a silent auto-merge.
set -euo pipefail

VERSION="${1:?usage: verify-series.sh <version> <tarball> [--record]}"
TARBALL="${2:?}"
RECORD="${3:-}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PATCHES="$REPO_ROOT/patches"
# shellcheck source=scripts/lib-extract.sh
. "$REPO_ROOT/scripts/lib-extract.sh"
BASE_JSON="$REPO_ROOT/base.json"

read -r WANT_SHA WANT_TREE < <(python3 - "$BASE_JSON" "$VERSION" <<'PY'
import json, sys
b = json.load(open(sys.argv[1]))[sys.argv[2]]
print(b["sha256"], b.get("patchedTree") or "-")
PY
)

echo ">>> 1/3 tarball identity"
GOT_SHA=$(sha256sum "$TARBALL" | cut -d' ' -f1)
if [ "$GOT_SHA" != "$WANT_SHA" ]; then
    echo "FAIL: tarball sha256 mismatch" >&2
    echo "  want $WANT_SHA" >&2
    echo "  got  $GOT_SHA" >&2
    exit 1
fi
echo "    ok  $GOT_SHA"

WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
extract_tarball "$TARBALL" "$WORK/src"
cd "$WORK/src"
git init -q .
# QEMU tarballs ship expanded submodule contents; nothing in the series touches
# roms/ or tests/lcitool, but they must be tracked for the tree hash to be stable.
git add -A
git -c user.email=v@local -c user.name=verify commit -qm "pristine qemu-${VERSION}"
echo "    pristine tree $(git rev-parse HEAD^{tree})"

SERIES="$PATCHES/$VERSION/series"
TOTAL=$(grep -cvE '^\s*(#.*)?$' "$SERIES")
echo ">>> 2/3 applying $TOTAL patches with strict context"
n=0
while read -r p; do
    case "$p" in ''|\#*) continue ;; esac
    n=$((n + 1))
    if ! git -c user.email=v@local -c user.name=verify am -q "$PATCHES/$p" 2>/dev/null; then
        echo "FAIL: patch $n/$TOTAL did not apply: $p" >&2
        git am --show-current-patch=diff 2>/dev/null | head -40 >&2 || true
        git am --abort 2>/dev/null || true
        exit 1
    fi
done < "$SERIES"
echo "    ok  $n/$TOTAL applied cleanly"

GOT_TREE=$(git rev-parse HEAD^{tree})
echo ">>> 3/3 patched tree"
if [ "$RECORD" = "--record" ]; then
    python3 - "$BASE_JSON" "$VERSION" "$GOT_TREE" <<'PY'
import json, sys
p, ver, tree = sys.argv[1], sys.argv[2], sys.argv[3]
d = json.load(open(p)); d[ver]["patchedTree"] = tree
open(p, "w").write(json.dumps(d, indent=2) + "\n")
PY
    echo "    RECORDED patchedTree = $GOT_TREE"
    echo "PASS: $n patches applied cleanly to pristine qemu-${VERSION} (tree recorded)"
    exit 0
fi
if [ "$WANT_TREE" = "-" ]; then
    echo "    no patchedTree recorded yet; re-run with --record"
    echo "PASS (partial): $n patches applied cleanly to pristine qemu-${VERSION}"
    exit 0
fi
if [ "$GOT_TREE" != "$WANT_TREE" ]; then
    echo "FAIL: patched tree mismatch -- the series no longer produces the recorded tree" >&2
    echo "  want $WANT_TREE" >&2
    echo "  got  $GOT_TREE" >&2
    exit 1
fi
echo "    ok  $GOT_TREE"
echo "PASS: $n patches applied cleanly to pristine qemu-${VERSION}; tree matches base.json"
