#!/usr/bin/env bash
# tests/modules/run.sh BUILD SDK [OUT]: panda modules on an aarch64 engine build (no PANDA needed).
#  1. -device from a module: ext-counter on -M virt; the guest reads its ID and count.
#  2. the vmstate guard: a module device without vmstate on the configurable machine warns, and
#     so does a built-in one the configurable machine created (B7).
#  3. the ABI check: a module built against a different hash is refused; PANDA_MODULE_ABI=ignore
#     loads it with a warning.
#  4. errors: not a panda module; -M on a module with no machine type; loading a path twice.
#  5. -module help prints the SDK's hash.
#  6. the target check (B1): a module built for another target (arm into aarch64, mips into
#     mipsel, ppc into ppc64, when the build and SDK have them) is refused.
#  7. type name clashes (B3): with a built-in type, and between two copies of a module, are
#     errors naming both, not an abort.
#  8. symbol binding (B4): the module's own qemu_get_thread_id() (an engine function's name) is
#     not exported and the module calls its own copy (the guest reads SELF=4d4f4453).
# Prints PASS/FAIL per check; exit status 1 if any failed.
set -u
BUILD=$(cd "$1" && pwd); SDK=$(cd "$2" && pwd); OUT=${3:-$(mktemp -d)}; mkdir -p "$OUT"
H=$(cd "$(dirname "$0")" && pwd)
Q=$BUILD/qemu-system-aarch64
fail=0
ok() { if eval "$2"; then echo "PASS $1"; else echo "FAIL $1"; fail=1; fi; }

"$SDK/bin/panda-board-cc" aarch64-softmmu -o "$OUT/ext-devices.so" "$H/ext-devices.c"
as "$H/counter.S" -o "$OUT/counter.o" && ld -Ttext=0x40080000 -e _start "$OUT/counter.o" -o "$OUT/counter.elf"
run() { timeout 20 "$Q" -display none -monitor none -semihosting "$@"; }

run -module "$OUT/ext-devices.so" -M virt -cpu cortex-a53 -m 64M -device ext-counter,addr=0xb000000 \
    -kernel "$OUT/counter.elf" -serial stdio > "$OUT/1.out" 2>&1
ok "-device ext-counter from a module (guest reads ID and 3 writes)" \
   'grep -q "ID=434e5452 COUNT=00000003" "$OUT/1.out"'
ok "the module calls its own qemu_get_thread_id(), not the engine's (SELF=4d4f4453)" \
   'grep -q "SELF=4d4f4453" "$OUT/1.out"'
ok "... which is not in the module's dynamic symbol table" \
   '! nm -D --defined-only "$OUT/ext-devices.so" | grep -qw qemu_get_thread_id'
ok "... the module is linked -Bsymbolic" 'readelf -d "$OUT/ext-devices.so" | grep -q "(SYMBOLIC)\|FLAGS.*SYMBOLIC"'
ok "... and exports panda_module_info" 'nm -D --defined-only "$OUT/ext-devices.so" | grep -qw panda_module_info'

run -module "$OUT/ext-devices.so" -M configurable,ram-base=0x40000000,device.0.type=pl011,device.0.mmio.0=0x9000000,device.0.prop.chardev=serial0,device.1.type=ext-nostate,device.1.id=nostate,device.1.mmio.0=0xb000000,device.2.type=unimplemented-device,device.2.id=unimp,device.2.prop.name=x,device.2.prop.size=0x1000,device.2.mmio.0=0xc000000 \
    -cpu cortex-a53 -m 64M -kernel "$OUT/counter.elf" -serial stdio > "$OUT/2.out" 2>&1
ok "vmstate guard warns about a module device without vmstate" \
   'grep -q "device /machine/peripheral/nostate (ext-nostate, from .*ext-devices.so) has no vmstate" "$OUT/2.out"'
ok "... and not about ext-counter (it has vmstate)" '! grep -q "ext-counter.*no vmstate" "$OUT/2.out"'
ok "... and about a built-in device without vmstate that the configurable machine created" \
   'grep -q "device /machine/peripheral/unimp (unimplemented-device, built in) has no vmstate" "$OUT/2.out"'
ok "... but not about pl011 (it has vmstate)" '! grep -q "(pl011, .*no vmstate" "$OUT/2.out"'

# -L and the resolved path: SDK may be a symlink to a nix store path, which must be copied, never edited
rm -rf "$OUT/sdk-other"; cp -rL "$(cd "$SDK" && pwd -P)/." "$OUT/sdk-other"; chmod -R u+w "$OUT/sdk-other"
sed -i 's/"\([0-9a-f]\{8\}\)/"deadbeef/' "$OUT/sdk-other/src/build/panda-abi.h"
"$OUT/sdk-other/bin/panda-board-cc" aarch64-softmmu -o "$OUT/ext-other.so" "$H/ext-devices.c"
run -module "$OUT/ext-other.so" -M virt -cpu cortex-a53 -kernel "$OUT/counter.elf" -serial none > "$OUT/3.out" 2>&1
ok "a module built against another ABI hash is refused" \
   'grep -q "built against panda ABI deadbeef.*this engine is" "$OUT/3.out" && grep -q "Rebuild it with this engine" "$OUT/3.out"'
PANDA_MODULE_ABI=ignore run -module "$OUT/ext-other.so" -M virt -cpu cortex-a53 -m 64M \
    -device ext-counter,addr=0xb000000 -kernel "$OUT/counter.elf" -serial stdio > "$OUT/3b.out" 2>&1
ok "PANDA_MODULE_ABI=ignore loads it with a warning" \
   'grep -q "loading anyway" "$OUT/3b.out" && grep -q "COUNT=00000003" "$OUT/3b.out"'

# any shared library the engine links that isn't a module (a nix engine has no libz, only libzstd)
z=$(ldd "$Q" | awk '$1 ~ /^lib(z|zstd)\.so/ {print $3; exit}')
run -module "$z" -M virt > "$OUT/4.out" 2>&1
ok "not a panda module" 'grep -q "is not a panda module" "$OUT/4.out"'
run -M "$OUT/ext-devices.so" > "$OUT/4b.out" 2>&1
ok "-M on a module with no machine type" 'grep -q "pick one with PATH:NAME; it registered: no machine types" "$OUT/4b.out"'
run -module "$OUT/ext-devices.so" -module "$OUT/ext-devices.so" -M virt -cpu cortex-a53 -m 64M \
    -device ext-counter,addr=0xb000000 -kernel "$OUT/counter.elf" -serial stdio > "$OUT/4c.out" 2>&1
ok "the same module twice is loaded once" 'grep -q "COUNT=00000003" "$OUT/4c.out"'

# 6. the target check
cross() {  # cross MODULE-TARGET ENGINE-TARGET
    local m=$1 e=$2
    [ -f "$SDK/cflags.$m-softmmu" ] && [ -x "$BUILD/qemu-system-$e" ] || { echo "SKIP $m module into $e (not built)"; return; }
    "$SDK/bin/panda-board-cc" $m-softmmu -o "$OUT/ext-$m.so" "$H/ext-devices.c"
    timeout 20 "$BUILD/qemu-system-$e" -display none -module "$OUT/ext-$m.so" -M none > "$OUT/6-$m-$e.out" 2>&1
    ok "a module built for $m is refused by qemu-system-$e" \
       'grep -q "ext-$m.so: built for target $m, this engine is $e" "$OUT/6-$m-$e.out" && grep -q "panda-board-cc $e-softmmu" "$OUT/6-$m-$e.out"'
}
cross arm aarch64
[ -f "$OUT/ext-arm.so" ] && {
  PANDA_MODULE_ABI=ignore timeout 20 "$Q" -display none -module "$OUT/ext-arm.so" -M none > "$OUT/6-ignore.out" 2>&1; rc=$?
  ok "PANDA_MODULE_ABI=ignore does not override a target mismatch (rc=$rc)" \
     '[ $rc = 1 ] && grep -q "built for target arm, this engine is aarch64" "$OUT/6-ignore.out" && ! grep -q "loading anyway" "$OUT/6-ignore.out"'
}
cross mips mipsel
cross mipsel mips
cross ppc ppc64
cross riscv32 riscv64

# a module from the first SDK (a bare panda_module_abi string, no panda_module_info)
echo 'const char panda_module_abi[] = "0123456789abcdef";' > "$OUT/oldsdk.c"
cc -shared -fPIC -o "$OUT/oldsdk.so" "$OUT/oldsdk.c"
run -module "$OUT/oldsdk.so" -M none > "$OUT/6-old.out" 2>&1
ok "a module from the older SDK: rebuild it, not 'not a panda module'" \
   'grep -q "built with an older panda SDK" "$OUT/6-old.out" && grep -q "Rebuild it with this engine" "$OUT/6-old.out"'

# 7. type name clashes
"$SDK/bin/panda-board-cc" aarch64-softmmu -o "$OUT/ext-clash.so" "$H/ext-clash.c"
run -module "$OUT/ext-clash.so" -M none > "$OUT/7.out" 2>&1; rc=$?
ok "a module type named like a built-in one is an error, not an abort (rc=$rc)" \
   '[ $rc = 1 ] && grep -q "ext-clash.so registers QOM type .pl011., already registered by the engine (built in)" "$OUT/7.out"'
cp "$OUT/ext-devices.so" "$OUT/ext-copy.so"
run -module "$OUT/ext-devices.so" -module "$OUT/ext-copy.so" -M none > "$OUT/7b.out" 2>&1; rc=$?
ok "two modules with the same type name: an error naming the first (rc=$rc)" \
   '[ $rc = 1 ] && grep -q "ext-copy.so registers QOM type .ext-counter., already registered by .*ext-devices.so" "$OUT/7b.out"'

# 9. the SDK names the host headers it needs; panda-board-cc checks them up front
python3 - "$OUT/sdk-other/panda-sdk.json" <<'PY2'
import json, sys
j = json.load(open(sys.argv[1])); j["host_includes"] = j.get("host_includes", []) + ["/nonexistent/glib-2.0"]
json.dump(j, open(sys.argv[1], "w"))
PY2
"$OUT/sdk-other/bin/panda-board-cc" aarch64-softmmu -o "$OUT/ext-nohost.so" "$H/ext-devices.c" > "$OUT/9.out" 2>&1; rc=$?
ok "panda-board-cc refuses up front when the SDK's host headers are missing" \
   '[ $rc != 0 ] && grep -q "host headers that are not here: /nonexistent/glib-2.0" "$OUT/9.out"'

h=$("$Q" -module help | awk '{print $3}')
ok "-module help prints the SDK's ABI hash" '[ "$h" = "$(python3 -c "import json;print(json.load(open(\"$SDK/panda-sdk.json\"))[\"panda_abi\"])")" ]'
exit $fail
