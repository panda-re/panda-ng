#!/usr/bin/env bash
# Portability probe. Applies patches/<base-version>/series to upstream QEMU refs
# we did NOT base on, and reports a per-version FAILURE CLASS for each patch
# rather than a single pass/fail.
#
#   ./scripts/probe-versions.sh 11.1.0 /path/to/qemu.git v11.0.0 v10.2.0 upstream/master
#
# Classes:
#   ok        applied with strict context
#   drift     strict failed, 3-way resolved it  -> context moved, content still fits
#   moved     a file the patch targets does not exist at this ref -> upstream rename
#   conflict  3-way failed too -> upstream changed the code we patch
#
# PROBE_STRICT_ONLY=1 skips the 3-way attempt and reports every non-strict
# failure as `conflict`. Use it when the probe runs against a clone that does
# NOT contain our patches' pre-image blobs -- e.g. a fresh upstream clone in CI.
# `git am --3way` reconstructs the pre-image from the patch's index-line hashes,
# so without those blobs it fails for a reason that has nothing to do with
# upstream drift, and the drift/conflict split would be meaningless.
set -euo pipefail

VERSION="${1:?usage: probe-versions.sh <series-version> <qemu-git-dir> <ref>...}"
GITDIR="${2:?}"; shift 2

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PATCHES="$REPO_ROOT/patches"
SERIES="$PATCHES/$VERSION/series"

for REF in "$@"; do
    WORK="$(mktemp -d)"
    git -C "$GITDIR" worktree add -q --detach "$WORK/t" "$REF" 2>/dev/null || {
        echo "== $REF: cannot check out"; rm -rf "$WORK"; continue; }
    cd "$WORK/t"
    ok=0 drift=0 moved=0 conflict=0
    declare -a notes=()
    while read -r p; do
        case "$p" in ''|\#*) continue ;; esac
        name=$(basename "$p" .patch)
        # does every file the patch touches exist here?
        missing=""
        while read -r f; do
            [ -e "$f" ] || missing="$missing $f"
        done < <(grep '^+++ b/' "$PATCHES/$p" | sed 's#^+++ b/##')
        if [ -n "$missing" ]; then
            moved=$((moved+1)); notes+=("  moved    $name ->$missing")
            continue
        fi
        if git -c user.email=p@l -c user.name=p am -q "$PATCHES/$p" >/dev/null 2>&1; then
            ok=$((ok+1)); continue
        fi
        git am --abort >/dev/null 2>&1 || true
        if [ "${PROBE_STRICT_ONLY:-}" = "1" ]; then
            conflict=$((conflict+1)); notes+=("  conflict $name (strict-only mode)")
        elif git -c user.email=p@l -c user.name=p am -q --3way "$PATCHES/$p" >/dev/null 2>&1; then
            drift=$((drift+1)); notes+=("  drift    $name")
        else
            git am --abort >/dev/null 2>&1 || true
            conflict=$((conflict+1)); notes+=("  conflict $name")
        fi
    done < "$SERIES"
    echo "== $REF   ok=$ok drift=$drift moved=$moved conflict=$conflict"
    for n in "${notes[@]:-}"; do [ -n "$n" ] && echo "$n"; done
    cd /; git -C "$GITDIR" worktree remove --force "$WORK/t" >/dev/null 2>&1 || true
    rm -rf "$WORK"
    unset notes
done
