#!/usr/bin/env bash
# Import the PANDA engine from a rehosting/qemu_builder tree into engine/.
#
#   tools/engine-import.sh [--commit] [--verify <qemu-tarball>] <qemu_builder-git-dir> [<ref>]
#
# Transitional. Until the cutover, the engine is still developed in qemu_builder branches (the lanes
# merge into its `integ`); this copies a committed qemu_builder state into panda-ng's engine/ and
# records where it came from in engine/IMPORT. At the cutover (the last import) the engine becomes
# panda-ng's own and this script is deleted (engine/README.md, "Provenance and the import script").
#
# What it does:
#   - reads <ref> (default HEAD) of the qemu_builder repo with `git archive`: committed state only,
#     so an uncommitted edit in a qemu_builder worktree is never imported by accident;
#   - mirrors the paths in MIRROR into engine/ (a file removed there is removed here), minus
#     DROP (penguin's packaging, which panda-ng doesn't build) and minus OWNED (files panda-ng
#     carries its own version of; TRACKED are qemu_builder files reimplemented elsewhere);
#   - writes engine/IMPORT: the source commit, and for each OWNED file the qemu_builder blob it
#     was last reconciled with. If qemu_builder changed one of those since, the import still
#     succeeds but prints the diff to port by hand, and the old blob stays recorded until
#     IMPORT_ACK_OWNED=1 is passed (after porting) so the warning cannot be lost;
#   - `git add -A engine/` (the flake only sees tracked files). With --commit it also commits, as
#     Luke Craig <lacraig3@gmail.com> (LANES.md), with the source commit in the message.
#
# Idempotent: importing the same commit twice leaves the tree and engine/IMPORT unchanged.
# Exit status: 0, or 1 if an OWNED/TRACKED file changed upstream (the import is still done).
# --verify runs engine/scripts/verify-series.sh on the result (the patched tree must hash to
# base.json's patchedTree, exactly as in qemu_builder).
set -euo pipefail

usage() { sed -n '2,4p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 2; }

COMMIT=0
TARBALL=
while [ $# -gt 0 ]; do
    case "$1" in
        --commit) COMMIT=1; shift ;;
        --verify) TARBALL="${2:?--verify needs a tarball}"; shift 2 ;;
        -h|--help) usage ;;
        -*) echo "unknown option $1" >&2; usage ;;
        *) break ;;
    esac
done
QB="${1:-}"; REF="${2:-HEAD}"
[ -n "$QB" ] || usage

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE="$ROOT/engine"

# Paths taken from qemu_builder, relative to its root.
MIRROR=(base.json patches src scripts tests)
# Penguin's packaging: never imported. Each is listed with its reason in engine/README.md.
DROP=(
    src/scripts/penguin-cffi-gen.py        # penguin's CFFI bindings for libqemu-system-*.so
    src/scripts/penguin-env-cffi-gen.py    # penguin's CPUArchState CFFI env modules
    src/scripts/penguin-qemu-package.py    # penguin-qemu.tar.gz layout
    src/scripts/__pycache__                # a committed .pyc
    scripts/check-config-contract.sh       # checks configs/default.json, which is not imported
)
# panda-ng's own versions; the import never overwrites them.
OWNED=(
    scripts/import-series.sh               # no configs/ or build.sh to overlay
    scripts/offcost-gate.sh                # STOCK_REF reads engine/ of a panda-ng ref
    scripts/check-delta-present.sh         # i386 takes the x86 I/O-port path
)
# qemu_builder files panda-ng reimplements outside the mirrored paths: tracked like OWNED.
TRACKED=(
    nix/source.nix                         # -> engine/nix/source.nix
    flake.nix                              # the engine source wiring -> panda-ng's flake.nix
)

git -C "$QB" rev-parse --git-dir >/dev/null 2>&1 || { echo "$QB is not a git checkout" >&2; exit 1; }
SHA=$(git -C "$QB" rev-parse --verify "$REF^{commit}")
SUBJECT=$(git -C "$QB" log -1 --format=%s "$SHA")
for p in "${MIRROR[@]}"; do
    git -C "$QB" cat-file -e "$SHA:$p" 2>/dev/null || { echo "$REF has no $p: not a qemu_builder tree?" >&2; exit 1; }
done

STAGE=$(mktemp -d); trap 'rm -rf "$STAGE" "$STAGE.keep"' EXIT
git -C "$QB" archive "$SHA" "${MIRROR[@]}" | tar -x -C "$STAGE"
for p in "${DROP[@]}"; do rm -rf "${STAGE:?}/$p"; done

mkdir -p "$ENGINE"
# OWNED files are set aside, each mirrored path is replaced wholesale (so a file removed in
# qemu_builder is removed here), and the OWNED files are put back.
KEEP="$STAGE.keep"; mkdir -p "$KEEP"
for p in "${OWNED[@]}"; do
    if [ -e "$ENGINE/$p" ]; then mkdir -p "$KEEP/$(dirname "$p")"; cp -p "$ENGINE/$p" "$KEEP/$p"; fi
    rm -f "$STAGE/$p"
done
for p in "${MIRROR[@]}"; do
    rm -rf "${ENGINE:?}/$p"
    cp -R "$STAGE/$p" "$ENGINE/$p"
done
(cd "$KEEP" && find . -type f) | while read -r f; do cp -p "$KEEP/$f" "$ENGINE/$f"; done
rm -rf "$KEEP"

# engine/IMPORT: the provenance record. Blob ids (git hash-object) of the qemu_builder versions
# of OWNED/TRACKED files, so the next import can tell whether they changed upstream.
IMPORT="$ENGINE/IMPORT"
# (bash 3.2 on macOS: no associative arrays.)
recorded() { [ -f "$IMPORT" ] && awk -v k="owned:$1" '$1 == k { print $2 }' "$IMPORT" || true; }
warn=0
{
    echo "# Written by tools/engine-import.sh. Do not edit by hand."
    echo "source rehosting/qemu_builder"
    echo "commit $SHA"
    echo "subject $SUBJECT"
    for p in "${OWNED[@]}" "${TRACKED[@]}"; do
        now=$(git -C "$QB" rev-parse --verify -q "$SHA:$p" || echo absent)
        was=$(recorded "$p")
        if [ -n "$was" ] && [ "$was" != "$now" ] && [ "${IMPORT_ACK_OWNED:-0}" != 1 ]; then
            # Keep the old blob so the warning repeats until someone ports the change.
            echo "owned:$p $was"
            warn=1
            {
                echo "WARNING: qemu_builder changed $p, which panda-ng carries its own version of."
                echo "  Port it by hand, then re-run with IMPORT_ACK_OWNED=1. The upstream change:"
                if [ "$now" = absent ]; then echo "  (removed upstream)"
                else git -C "$QB" diff "$was" "$now" | sed 's/^/    /'; fi
            } >&2
        else
            echo "owned:$p $now"
        fi
    done
} > "$IMPORT.new"
mv "$IMPORT.new" "$IMPORT"

git -C "$ROOT" add -A engine
# panda-ng's .gitignore ignores *.so, *.out, *.d, ...: anything qemu_builder tracks is added anyway.
for p in "${MIRROR[@]}"; do
    git -C "$ROOT" ls-files --others --ignored --exclude-standard -- "engine/$p" |
        while IFS= read -r f; do git -C "$ROOT" add -f -- "$f"; done
done
if git -C "$ROOT" diff --cached --quiet -- engine; then
    echo "engine/ already matches qemu_builder ${SHA:0:12} ($SUBJECT): nothing to import"
else
    git -C "$ROOT" diff --cached --stat -- engine | tail -1
    if [ "$COMMIT" = 1 ]; then
        git -C "$ROOT" -c user.name="Luke Craig" -c user.email=lacraig3@gmail.com commit -q \
            -m "engine: import qemu_builder ${SHA:0:12}" \
            -m "$SUBJECT" \
            -m "tools/engine-import.sh $SHA" -- engine
        echo "committed $(git -C "$ROOT" rev-parse --short HEAD): engine from qemu_builder ${SHA:0:12}"
    else
        echo "staged engine/ from qemu_builder ${SHA:0:12}; commit with --commit or by hand"
    fi
fi

if [ -n "$TARBALL" ]; then
    ver=$(python3 -c "import json,sys;(v,)=json.load(open(sys.argv[1]));print(v)" "$ENGINE/base.json")
    "$ENGINE/scripts/verify-series.sh" "$ver" "$TARBALL"
fi
exit $warn
