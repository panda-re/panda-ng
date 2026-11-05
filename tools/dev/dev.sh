#!/usr/bin/env bash
# panda-ng's local development build: the engine, the plugins and pandare2's bindings, built
# incrementally in .dev/ for a few targets (docs/guides/development.md).
#
#   tools/dev/dev.sh build [--targets x86_64,aarch64] [--plugins osi,syscalls2] [-j N]
#   tools/dev/dev.sh build plugins          # only some steps: engine headers h2b plugins bindings
#   .dev/bin/python3 tests/smoke.py x86_64  # pandare2 from python/core on the dev build
#   tools/dev/dev.sh shell | env | status | clean
#
# Run from anywhere; the build is in <repo>/.dev (PANDA_DEV_DIR overrides). Each step runs in the
# flake's dev shell (devShells.<system>.dev), entered without the caller's environment, like
# `nix develop -i`. The shell's environment is cached in .dev/env.sh (and kept from garbage
# collection by the profile .dev/nix/profile); it is made again when flake.nix, flake.lock or
# what the engine's source is fetched from changes.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
DEV=${PANDA_DEV_DIR:-$ROOT/.dev}
mkdir -p "$DEV/nix"
DEV=$(cd "$DEV" && pwd)

key=$(cat "$ROOT/flake.nix" "$ROOT/flake.lock" "$ROOT/engine/base.json" "$ROOT/engine/nix/source.nix" |
  sha256sum | cut -d' ' -f1)
if [ ! -s "$DEV/env.sh" ] || [ "$(cat "$DEV/env.key" 2>/dev/null)" != "$key" ]; then
  echo "dev: entering the flake's dev shell (devShells.dev)" >&2
  nix print-dev-env --profile "$DEV/nix/profile" "$ROOT#dev" >"$DEV/env.sh.new"
  mv "$DEV/env.sh.new" "$DEV/env.sh"
  echo "$key" >"$DEV/env.key"
fi

# The dev shell, as `nix develop -i -k HOME` gives it: nothing from the caller's environment but
# these (an impure shell picks up the host's libraries, e.g. libgcrypt, in QEMU's configure).
# PATH is set, empty: bash gives an unset PATH a default (/usr/bin, ...), which env.sh would keep.
keep=(PATH= HOME="$HOME" TERM="${TERM:-dumb}" USER="${USER:-}" LOGNAME="${LOGNAME:-}"
  PANDA_DEV_DIR="$DEV" PANDA_DEV_ROOT="$ROOT")
BASH_BIN=$(command -v bash)
[ -n "${MAKEFLAGS:-}" ] && keep+=(MAKEFLAGS="$MAKEFLAGS")
inner='source "$PANDA_DEV_DIR/env.sh" >/dev/null 2>&1
trap '\''rm -rf "$NIX_BUILD_TOP"'\'' EXIT
cd "$PANDA_DEV_ROOT"
'
if [ "${1:-}" = shell ]; then
  shift
  exec env -i "${keep[@]}" "$BASH_BIN" --noprofile --norc -c "$inner"'
python3 tools/dev/dev.py env > "$NIX_BUILD_TOP/rc"
echo "PS1=\"(panda-dev) \\w\\$ \"" >> "$NIX_BUILD_TOP/rc"
bash --noprofile --rcfile "$NIX_BUILD_TOP/rc" -i "$@"' bash "$@"
fi
exec env -i "${keep[@]}" "$BASH_BIN" --noprofile --norc -c "$inner"'python3 tools/dev/dev.py "$@"' bash "$@"
