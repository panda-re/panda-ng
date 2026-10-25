#!/usr/bin/env bash
# Make the panda board SDK from a finished engine build: what an external board or device
# (a "panda module") compiles against, without the engine's source and build trees.
#
#   scripts/panda-sdk.sh BUILD_DIR OUT [SRC_DIR]     (in the engine source tree; the nix engine
#                                                    runs it into its "sdk" output)
#
# OUT/src mirrors the source tree's headers (include/, target/, hw/, host/, tcg/,
# linux-headers/) and OUT/src/build the build's generated headers (config-*.h, qapi/, trace/,
# panda-abi.h ...), so the compile lines QEMU used (compile_commands.json) work unchanged when
# run in OUT/src/build. OUT/cflags.<target> is one such line per target, plus -DBUILD_DSO,
# -include qemu/panda-module.h (which stamps the module with a PandaModuleInfo: the panda ABI
# hash and the target), -fPIC, and -fvisibility=hidden (with -Wl,-Bsymbolic at link time, so the
# engine's symbols, or an earlier module's, can't replace the module's own functions: modules
# are dlopen()ed RTLD_GLOBAL into an executable that exports ~32k symbols).
# OUT/bin/panda-board-cc compiles and links a module:
#
#   panda-board-cc arm-softmmu -o board.so a.c b.c [-Iinclude ...]
#
# The host libraries' headers QEMU uses (glib, pixman) are referenced where the engine's build
# found them; on nix the mkBoard helper supplies the same store paths.
set -euo pipefail

BUILD=$(cd "${1:?usage: panda-sdk.sh BUILD_DIR OUT [SRC_DIR]}" && pwd)
OUT="${2:?}"
SRC=$(cd "${3:-$BUILD/..}" && pwd)
[ -f "$BUILD/compile_commands.json" ] || { echo "$BUILD: no compile_commands.json" >&2; exit 1; }
[ -f "$BUILD/panda-abi.h" ] || { echo "$BUILD: no panda-abi.h (an engine without 0130?)" >&2; exit 1; }
[ -e "$OUT" ] && { echo "refusing to overwrite $OUT" >&2; exit 1; }
mkdir -p "$OUT/src/build" "$OUT/bin"
OUT=$(cd "$OUT" && pwd)

# Source headers: target/ only for the built targets' base arches.
arches=$(for f in "$BUILD"/*-config-target.h; do
    t=$(basename "$f" -config-target.h)
    sed -n 's/^TARGET_BASE_ARCH=//p; s/^TARGET_ARCH=//p' "$SRC/configs/targets/$t.mak" | tail -1
done | sort -u)
(cd "$SRC" && find include $(for a in $arches; do echo target/$a; done) hw host tcg linux-headers subprojects/libvduse \
     -name '*.h' -o -name '*.h.inc' -o -name '*.c.inc' 2>/dev/null) > "$OUT/.list"
tar -C "$SRC" -cf - -T "$OUT/.list" | tar -C "$OUT/src" -xf -
# Generated headers.
(cd "$BUILD" && find . -path ./pyvenv -prune -o -path ./meson-private -prune -o -path ./qemu-bundle -prune -o \
     \( -name '*.h' -o -name '*.h.inc' \) -print) > "$OUT/.list"
tar -C "$BUILD" -cf - -T "$OUT/.list" | tar -C "$OUT/src/build" -xf -
rm "$OUT/.list"

python3 - "$BUILD" "$SRC" "$OUT" <<'PY'
import glob, json, os, re, shlex, sys
build, src, out = sys.argv[1:]
cmds = json.load(open(os.path.join(build, "compile_commands.json")))
targets = sorted(os.path.basename(p)[:-len("-config-target.h")]
                 for p in glob.glob(os.path.join(build, "*-config-target.h")))
abi = re.search(r'"([0-9a-f]+)"', open(os.path.join(build, "panda-abi.h")).read()).group(1)
cflags = {}
for t in targets:
    lib = f"libqemu-{t}.a.p/"
    # A per-target compile (it sees cpu.h, target_ulong, CONFIG_TARGET): the flags any board
    # file of this target can use.
    cands = [e for e in cmds if e["output"].startswith(lib) and "/target/" in "/" + e["file"]]
    if not cands:
        continue
    cmd = cands[0]["command"]
    cmd = re.sub(r" -MD -MQ \S+ -MF \S+ -o \S+ -c \S+$", "", cmd)
    cmd = cmd.replace(src + "/", "../").replace(src, "..")
    argv = shlex.split(cmd)
    cc, flags = argv[0], argv[1:]
    flags = [a for a in flags if a not in ("-fPIE", "-fpie", "-fno-pic", "-fno-PIC")]
    flags += ["-DBUILD_DSO", "-include", "qemu/panda-module.h", "-fPIC", "-fvisibility=hidden"]
    cflags[t] = {"cc": cc, "flags": flags}
    with open(os.path.join(out, f"cflags.{t}"), "w") as f:
        f.write(" ".join(shlex.quote(a) for a in flags) + "\n")
# Host library headers (glib, pixman, ...) the cflags name by absolute path: panda-board-cc
# checks they exist, so a module built outside the engine's environment fails up front.
host = sorted({a[2:] if a.startswith("-I") else a for c in cflags.values() for a in c["flags"]
               if a.startswith("-I/")})
json.dump({"panda_abi": abi, "targets": sorted(cflags), "cc": next(iter(cflags.values()))["cc"],
           "host_includes": host, "module_info_version": 1,
           "qemu_version": open(os.path.join(src, "VERSION")).read().strip(),
           "workdir": "src/build"}, open(os.path.join(out, "panda-sdk.json"), "w"), indent=1)
PY

cat > "$OUT/bin/panda-board-cc" <<'SH'
#!/usr/bin/env bash
# panda-board-cc TARGET -o OUT.so SOURCES... [-I... -D...]: compile and link a panda module.
set -euo pipefail
SDK=$(cd "$(dirname "$0")/.." && pwd)
T="${1:?usage: panda-board-cc TARGET -o OUT.so SOURCES...}"; shift
[ -f "$SDK/cflags.$T" ] || { echo "no target $T in this SDK: $(ls "$SDK" | sed -n 's/^cflags\.//p')" >&2; exit 1; }
out=""; srcs=(); extra=()
while [ $# -gt 0 ]; do
    case "$1" in
        -o) out=$(realpath -m "$2"); shift 2 ;;
        -*) extra+=("$1"); shift ;;
        *) srcs+=("$(realpath "$1")"); shift ;;
    esac
done
[ -n "$out" ] && [ ${#srcs[@]} -gt 0 ] || { echo "need -o OUT.so and sources" >&2; exit 1; }
# Relative -I paths are the caller's: make them absolute (we compile in the SDK).
for i in "${!extra[@]}"; do
    case "${extra[$i]}" in -I/*) ;; -I*) extra[$i]="-I$(realpath "${extra[$i]#-I}")" ;; esac
done
CC=${CC:-$(python3 -c "import json;print(json.load(open('$SDK/panda-sdk.json'))['cc'])")}
missing=$(python3 -c "import json,os;print(' '.join(p for p in json.load(open('$SDK/panda-sdk.json')).get('host_includes',[]) if not os.path.isdir(p)))")
[ -z "$missing" ] || { echo "panda-board-cc: this SDK's engine was built with host headers that are not here: $missing" >&2
    echo "(on nix, build with the flake's mkBoard, which brings them; elsewhere install the same glib/pixman -dev packages there)" >&2; exit 1; }
eval "flags=($(cat "$SDK/cflags.$T"))"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
objs=(); pids=(); fail=0
cd "$SDK/src/build"
for s in "${srcs[@]}"; do
    o="$tmp/$(basename "${s%.*}").o"
    $CC "${flags[@]}" "${extra[@]}" -iquote "$(dirname "$s")" -c "$s" -o "$o" &
    objs+=("$o"); pids+=($!)
done
for p in "${pids[@]}"; do wait "$p" || fail=1; done
[ $fail = 0 ] || { echo "panda-board-cc: compile failed" >&2; exit 1; }
case "$(uname)" in
    Darwin) $CC -bundle -undefined dynamic_lookup -o "$out" "${objs[@]}" ;;
    *) $CC -shared -Wl,-Bsymbolic -o "$out" "${objs[@]}" ;;
esac
SH
chmod +x "$OUT/bin/panda-board-cc"
echo "SDK at $OUT: $(du -sh "$OUT" | cut -f1), targets: $(python3 -c "import json;print(' '.join(json.load(open('$OUT/panda-sdk.json'))['targets']))"), ABI $(python3 -c "import json;print(json.load(open('$OUT/panda-sdk.json'))['panda_abi'][:12])")"
