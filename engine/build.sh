#!/bin/bash
# Build penguin-qemu from an already-prepared source tree.
#
# This is a faithful port of rehosting/qemu's build.sh with one change: the
# feature set, the Penguin arch list and the arch->target map are no longer
# hardcoded here. They are read from configs/<profile>.json, so "which features
# are on" is reviewable data rather than a shell array, and is diffable across
# upstream QEMU versions.
#
#   PENGUIN_QEMU_PROFILE=default ./build.sh
#
# Expects to run inside a tree that already has the series applied and the
# vendored Penguin sources overlaid (see nix/source.nix, or import-series.sh for
# a development tree).
set -e

QEMU_DIR="$(dirname "$0")"
cd "$QEMU_DIR"

PROFILE="${PENGUIN_QEMU_PROFILE:-default}"
CONFIG="${PENGUIN_QEMU_CONFIG:-configs/$PROFILE.json}"
[ -f "$CONFIG" ] || { echo "no such config: $CONFIG" >&2; exit 1; }

read_cfg() { python3 -c "
import json,sys
d=json.load(open('$CONFIG'))
$1" ; }

mapfile -t COMMON_CONFIGURE_ARGS < <(read_cfg "print('\n'.join(a['flag'] for a in d['configureArgs']))")
PENGUIN_SYSTEM_ARCHES="${PENGUIN_SYSTEM_ARCHES:-$(read_cfg "print(','.join(d['systemArches']))")}"

echo ">>> profile $PROFILE: ${#COMMON_CONFIGURE_ARGS[@]} configure args, arches: $PENGUIN_SYSTEM_ARCHES"

penguin_system_arch_to_qemu_target() {
    read_cfg "
a=d['archToTarget'].get('$1')
if a is None:
    sys.stderr.write('Unsupported Penguin system arch: $1\n'); sys.exit(1)
print(a)"
}

configure_build_dir() {
    local build_dir="$1"; shift
    mkdir -p "$build_dir"
    if [ ! -f "$build_dir/config.status" ]; then
        ( cd "$build_dir" && ../configure "${COMMON_CONFIGURE_ARGS[@]}" "$@" )
    fi
}

append_unique() {
    local value="$1"; shift
    local existing
    for existing in "$@"; do
        [ "$existing" = "$value" ] && return 1
    done
    printf "%s\n" "$value"
}

build_system_target_list() {
    local arch target targets=()
    IFS=',' read -ra split_arches <<< "$PENGUIN_SYSTEM_ARCHES"
    for arch in "${split_arches[@]}"; do
        target="$(penguin_system_arch_to_qemu_target "$arch")"
        if append_unique "$target" "${targets[@]}" >/dev/null; then
            targets+=("$target")
        fi
    done
    local IFS=,
    printf "%s\n" "${targets[*]}"
}

build_system_lib_list() {
    local arch target lib libs=()
    IFS=',' read -ra split_arches <<< "$PENGUIN_SYSTEM_ARCHES"
    for arch in "${split_arches[@]}"; do
        target="$(penguin_system_arch_to_qemu_target "$arch")"
        lib="libqemu-system-${target%-softmmu}.so"
        if append_unique "$lib" "${libs[@]}"; then
            libs+=("$lib")
        fi
    done
    printf "%s\n" "${libs[@]}"
}

# Both `intel64` and `x86_64` map to x86_64-softmmu, so only one library is
# built; each alias then gets its own copy. Penguin's qemu loader resolves
# libqemu-system-<arch>.so by arch name with no intel64<->x86_64 fallback, so
# the package must ship BOTH names or x86_64 guests fail to load.
stage_system_lib_aliases() {
    local arch target source alias
    IFS=',' read -ra split_arches <<< "$PENGUIN_SYSTEM_ARCHES"
    for arch in "${split_arches[@]}"; do
        target="$(penguin_system_arch_to_qemu_target "$arch")"
        source="build-system/libqemu-system-${target%-softmmu}.so"
        alias="build-system/libqemu-system-${arch}.so"
        [ "$source" != "$alias" ] && cp -f "$source" "$alias"
    done
    return 0
}

targets_to_kvm_libs() {
    local target
    IFS=',' read -ra split_targets <<< "$1"
    for target in "${split_targets[@]}"; do
        printf "libqemu-kvm-%s.so\n" "${target%-softmmu}"
    done
}

system_targets="$(build_system_target_list)"

configure_build_dir build-system \
    --target-list="$system_targets" \
    --enable-tcg \
    --disable-kvm

mapfile -t system_libs < <(build_system_lib_list)
ninja -C build-system "${system_libs[@]}" qemu-img
stage_system_lib_aliases
python3 scripts/penguin-cffi-gen.py \
    --mode system \
    --build-dir build-system \
    --arches "$PENGUIN_SYSTEM_ARCHES"
python3 scripts/penguin-env-cffi-gen.py \
    --build-dir build-system \
    --manifest build-system/qemu_cffi_system_manifest.json

if [ "${PENGUIN_KVM_TARGETS:-}" = "none" ]; then
    kvm_targets=
elif [ -n "${PENGUIN_KVM_TARGETS:-}" ]; then
    kvm_targets="$PENGUIN_KVM_TARGETS"
else
    kvm_targets="$(read_cfg "print(d.get('kvmTargetByHost',{}).get('$(uname -m)',''))")"
fi

if [ -n "$kvm_targets" ]; then
    configure_build_dir build-kvm \
        --target-list="$kvm_targets" \
        --enable-kvm \
        --disable-tcg
    mapfile -t kvm_libs < <(targets_to_kvm_libs "$kvm_targets")
    ninja -C build-kvm "${kvm_libs[@]}"
    python3 scripts/penguin-cffi-gen.py \
        --mode kvm \
        --build-dir build-kvm \
        --targets "$kvm_targets"
    python3 scripts/penguin-env-cffi-gen.py \
        --build-dir build-kvm \
        --manifest build-kvm/qemu_cffi_kvm_manifest.json
fi

python3 scripts/penguin-qemu-package.py --output penguin-qemu.tar.gz
