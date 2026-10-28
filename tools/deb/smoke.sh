#!/usr/bin/env bash
# Run only in a disposable stock Debian/Ubuntu container with no host /nix mount.
# smoke.sh package.deb tests-directory evidence-directory
set -euo pipefail
DEB=$(readlink -f "$1")
TESTS=$(readlink -f "$2")
OUT=$3
HERE=$(cd "$(dirname "$0")" && pwd)
initial_state=$(dpkg-query -W -f='${db:Status-Status}' panda-next 2>/dev/null || true)
if { [ -n "$initial_state" ] && [ "$initial_state" != not-installed ] && [ "$initial_state" != config-files ]; } ||
   [ -e /opt/panda-next ] || command -v panda-python >/dev/null; then
  printf '%s\n' 'FAIL: panda-next is already installed' >&2
  exit 2
fi
unset PANDA_LIB PANDA_PLUGIN_DIR PANDA_QEMU_LIBDIR PYTHONPATH
export PYTHONNOUSERSITE=1
mkdir -p "$OUT"
OUT=$(readlink -f "$OUT")
sha256sum "$DEB" > "$OUT/artifact.sha256"
cleanup() {
  rc=$?
  trap - EXIT
  # A failed install can leave an unpacked/half-configured package. Do not mask
  # that original failure, and do not remove an unrelated preexisting package.
  state=$(dpkg-query -W -f='${db:Status-Status}' panda-next 2>/dev/null || true)
  if [ -n "$state" ] && [ "$state" != not-installed ] && [ "$state" != config-files ]; then
    if ! dpkg -r panda-next > "$OUT/remove.log" 2>&1; then
      [ "$rc" != 0 ] || rc=1
    fi
  else
    printf 'No unpacked package to remove (state=%s)\n' "$state" > "$OUT/remove.log"
  fi
  # Bash may retain a successful launcher lookup after dpkg removed the file.
  hash -r
  if [ -e /opt/panda-next ] || command -v panda-python >/dev/null; then
    printf '%s\n' 'FAIL: package entrypoints remain after removal' >&2
    [ "$rc" != 0 ] || rc=1
  fi
  exit "$rc"
}
trap cleanup EXIT
dpkg -i "$DEB" > "$OUT/install.log" 2>&1
cd /tmp
for arch in x86_64 aarch64 riscv64; do
  panda-system-$arch --version > "$OUT/launcher-$arch.log" 2>&1
  panda-system-$arch -machine help >> "$OUT/launcher-$arch.log" 2>&1
  grep -q 'QEMU emulator version' "$OUT/launcher-$arch.log"
  grep -q 'Supported machines' "$OUT/launcher-$arch.log"
  timeout 900 panda-python "$TESTS/smoke.py" "$arch" > "$OUT/smoke-$arch.log" 2>&1
done
panda-python "$HERE/../wheel/installed.py" deb "$TESTS" "$OUT/installed"
