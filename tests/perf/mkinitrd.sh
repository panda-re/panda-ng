#!/bin/sh
# Promote the idlecost initrd recipe without historical absolute paths or archive extraction.
# Usage: mkinitrd.sh STATIC_BUSYBOX STATIC_IDLEWORK NEW_OUT.gz
# Build idlework.c separately with the pinned guest compiler and save its exact
# compiler/configuration record. Both inputs must be static and target-compatible.
set -eu
[ "$#" = 3 ] || { printf '%s\n' 'usage: mkinitrd.sh BUSYBOX IDLEWORK NEW_OUT.gz' >&2; exit 2; }
H=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BB=$(realpath "$1"); IW=$(realpath "$2")
[ -x "$BB" ] && [ -x "$IW" ]
# noclobber also refuses an existing output symlink.
set -C
exec 3>"$3"
R=$(mktemp -d /tmp/perf-init.XXXXXX)
trap 'rm -rf -- "$R"' EXIT HUP INT TERM
mkdir -p "$R/bin" "$R/proc" "$R/sys" "$R/dev"
cp "$BB" "$R/bin/busybox"
cp "$IW" "$R/idlework"
cp "$H/init" "$R/init"
chmod 755 "$R/init" "$R/idlework" "$R/bin/busybox"
# Separate commands preserve failure status (no shell pipeline hides cpio failure).
(cd "$R" && printf '%s\n' . bin bin/busybox dev idlework init proc sys | cpio -o -H newc > archive.cpio)
gzip -n -1 -c "$R/archive.cpio" >&3
