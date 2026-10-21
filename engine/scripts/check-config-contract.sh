#!/usr/bin/env bash
# The config contract gate: every library configs/<profile>.json declares as a
# nixDep must actually end up in the built artifact's runtime closure.
#
#   ./scripts/check-config-contract.sh <penguin-qemu-store-path> [profile]
#
# WHY. The --enable-* group in configs/default.json is a set of HARD enables, so
# in principle a missing library is a configure failure and this is redundant.
# In practice the failure that actually bites is subtler: a flag gets flipped to
# --disable-* (or dropped) while its nixDep stays declared, and the artifact
# silently loses a feature nobody notices for weeks. Checking the closure
# catches the declaration and the reality disagreeing in either direction.
#
# It uses nix's own reference tracking rather than a hand-maintained
# nixDep-to-soname table, because those tables go stale: `libusb1` links as
# `libusb-1.0.so.0`, `rdma-core` as `librdmacm.so.1` + `libibverbs.so.1`,
# `bzip2` as `libbz2.so.1`. The store paths need no such mapping.
set -euo pipefail

OUT="${1:?usage: check-config-contract.sh <penguin-qemu-store-path> [profile]}"
PROFILE="${2:-default}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG="$REPO_ROOT/configs/$PROFILE.json"

case "$OUT" in
    /nix/store/*) ;;
    *) echo "FAIL: $OUT is not a /nix/store path; the closure cannot be queried" >&2; exit 1 ;;
esac

mapfile -t names < <(python3 -c "
import json
d = json.load(open('$CONFIG'))
print('\n'.join(sorted({x for a in d['configureArgs'] for x in a.get('nixDeps', [])})))
")
[ ${#names[@]} -gt 0 ] || { echo "no nixDeps declared in $CONFIG; nothing to check"; exit 0; }

# Resolve the declared deps through THIS FLAKE's nixpkgs. Using <nixpkgs> here
# would compare against the channel's revision, whose store hashes differ from
# the pinned one, and every dep would look absent.
depfile=$(nix build --no-link --print-out-paths "$REPO_ROOT#declaredNixDeps")

# The full runtime closure, so a library reached only transitively still counts.
closure=$(nix-store -q --requisites "$OUT")

rc=0
n=0
while read -r attr paths; do
    [ -n "$attr" ] || continue
    n=$((n + 1))
    hit=""
    for want in $paths; do
        if grep -qxF "$want" <<<"$closure"; then hit="$want"; break; fi
    done
    if [ -n "$hit" ]; then
        echo "  ok    $attr -> $(basename "$hit")"
    else
        echo "  FAIL  $attr declared in $PROFILE but no output is in the runtime closure" >&2
        for want in $paths; do echo "          checked $want" >&2; done
        rc=1
    fi
done < "$depfile"

[ $rc -eq 0 ] && echo "PASS: all $n declared nixDeps are in the artifact's runtime closure"
exit $rc
