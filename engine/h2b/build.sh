#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Build the H2b shadow helpers for one target from an engine build tree.
#
#   engine/h2b/build.sh <arch> <meson build dir> <out.so> <helper source>...
#   e.g. engine/h2b/build.sh x86_64 qemu-dev/build h2b/libtaint-h2b-x86_64.so target/i386/tcg/cc_helper.c
#
# Each source is compiled to bitcode with the engine's own flags (compile_commands.json; minus
# gcc-only flags), the bitcode is linked, the taint-h2b pass emits the shadow helpers of the pure
# helpers in it, and everything else is dropped: the result is a small library with no dependency
# on the engine, loaded by taint.c from $PANDA_TAINT_H2B.
#
# Tools: H2B_CLANG (a wrapped clang, for the plugin and the final link), H2B_LLVM (bin dir with
# opt and llvm-link), H2B_LLVM_DEV (LLVM headers). All LLVM 21.
set -euo pipefail

ARCH=$1 BUILD=$(realpath "$2") OUT=$(realpath -m "$3")
shift 3
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
: "${H2B_CLANG:?wrapped clang}" "${H2B_LLVM:?llvm bin dir}" "${H2B_LLVM_DEV:?llvm dev prefix}"
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT

"$H2B_CLANG++" -shared -fPIC -std=c++17 -fno-rtti -O1 -I"$H2B_LLVM_DEV/include" \
    "$HERE/TaintH2B.cpp" -o "$WORK/libTaintH2B.so"

bcs=()
for src in "$@"; do
    bc="$WORK/$(basename "$src" .c).bc"
    python3 - "$BUILD" "$ARCH" "$src" "$bc" > "$WORK/cc.sh" <<'EOF'
import json, shlex, sys
build, arch, src, out = sys.argv[1:]
cands = [e for e in json.load(open(f"{build}/compile_commands.json")) if e["file"].endswith(src)]
# the target's own object, else one shared by the target family (libcommon_arm, ...)
cands = ([e for e in cands if f"-{arch}-softmmu" in e["output"]] or
         [e for e in cands if "-softmmu" not in e["output"]])
for e in cands[:1]:
        a = shlex.split(e["command"])
        args, skip = [], False
        for x in a[1:]:
            if skip:
                skip = False
            elif x in ("-o", "-MF", "-MQ"):
                skip = True
            elif x in ("-MD", "-c") or x.startswith(("-fzero-init-padding", "-ftrivial-auto-var-init")):
                pass
            else:
                args.append(x)
        print("cd", shlex.quote(build))
        print('exec "$CLANG" -emit-llvm -c -O1 -fPIC -Wno-unknown-warning-option '
              "-Wno-unused-command-line-argument", " ".join(map(shlex.quote, args)),
              "-o", shlex.quote(out))
        break
else:
    sys.exit(f"{src}: not in {build}/compile_commands.json for {arch}")
EOF
    CLANG="$H2B_CLANG" bash "$WORK/cc.sh"
    bcs+=("$bc")
done
"$H2B_LLVM/llvm-link" "${bcs[@]}" -o "$WORK/all.bc"
"$H2B_LLVM/opt" -inline-threshold=4000 -load-pass-plugin="$WORK/libTaintH2B.so" \
    -passes='cgscc(inline),taint-h2b,internalize,globaldce,default<O2>' \
    -internalize-public-api-list=taint_h2b_table "$WORK/all.bc" -o "$WORK/h2b.bc"
mkdir -p "$(dirname "$OUT")"
"$H2B_CLANG" -shared -fPIC -O2 -Wno-unused-command-line-argument "$WORK/h2b.bc" -o "$OUT"
[ -n "${H2B_KEEP_BC:-}" ] && cp "$WORK/all.bc" "$WORK/h2b.bc" "$H2B_KEEP_BC/"
echo "OK: $OUT"
