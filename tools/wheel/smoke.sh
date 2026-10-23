#!/bin/bash
# smoke.sh <wheel> <tests dir>: install a built pandare2 wheel into a fresh venv on a stock
# Debian/Ubuntu (glibc >= 2.38) and check it. Run it in a clean container, e.g. (what release CI does)
#   docker run --rm -v $PWD:/w:ro ubuntu:24.04 bash /w/tools/wheel/smoke.sh /w/pandare2-*.whl /w/tests
# Checks: pip installs it; License-Expression and every License-File are there; every ELF's
# libraries resolve, the non-glibc ones inside the venv, and no library is in it twice; the
# qemu-system-<arch> console scripts run; the host serial path (glibc-compat shim) works;
# tests/smoke.py boots x86_64, aarch64 and riscv64 with plugins loaded. Exit status 0 = all passed.
set -u
WHL=$1; TESTS=$2
HERE=$(dirname "$(readlink -f "$0")")
fail=0
check() { if [ "$1" = 0 ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }

export DEBIAN_FRONTEND=noninteractive
(apt-get update -qq && apt-get install -y -qq python3 python3-venv >/dev/null) || { echo "FAIL setup"; exit 1; }
echo "HOST $(. /etc/os-release; echo "$PRETTY_NAME") $(uname -m) $(ldd --version | head -1) $(python3 --version)"
python3 -m venv /tmp/venv
V=/tmp/venv/bin
$V/pip install -q "$WHL"; check $? "pip install $(basename "$WHL")"
$V/python -c "import pandare2"; check $? "import pandare2"
SP=$($V/python -c "import os, pandare2; print(os.path.dirname(os.path.dirname(pandare2.__file__)))")

$V/python - <<'PY'; check $? "license metadata"
import importlib.metadata as m, sys
d = m.distribution("pandare2")
md = d.metadata
files = md.get_all("License-File") or []
lic = d._path / "licenses"
missing = [f for f in files if not (lic / f).is_file()]
print("  License-Expression:", md["License-Expression"])
print(f"  License-File: {len(files)}, missing: {missing}")
sys.exit(not md["License-Expression"] or "GPL-2.0-only" not in md["License-Expression"] or missing
         or not (lic / "SOURCES.txt").is_file() or md.get("License") == "MIT")
PY

n=0; bad=0
for f in $(find "$SP/pandare2" "$SP/pandare2.libs" -type f ! -path '*/data/share/*'); do
  head -c4 "$f" | grep -q ELF || continue
  n=$((n+1))
  out=$(ldd "$f" 2>&1)
  # Everything but glibc resolves inside the venv.
  if echo "$out" | grep -q "not found" || echo "$out" | grep "=>" \
      | grep -v -E "(libc|libm|libdl|libpthread|librt|libresolv)\.so|ld-linux" | grep -q -v " $SP/"; then
    echo "  $f:"; echo "$out" | sed 's/^/    /'; bad=$((bad+1))
  fi
done
check $bad "ldd: $n ELF files, $bad with a library missing or from outside the venv"
dups=$(find "$SP/pandare2" "$SP/pandare2.libs" -name '*.so*' ! -path '*/data/share/*' -printf '%f\n' \
  | sed -E 's/-[0-9a-f]{8}(\.so)/\1/' | sort | uniq -d)
[ -z "$dups" ]; check $? "no library in the wheel twice ${dups}"

for a in x86_64 aarch64 riscv64; do
  $V/qemu-system-$a -version | grep -q "QEMU emulator version" && $V/qemu-system-$a -machine help | grep -q "Supported machines"
  check $? "qemu-system-$a -version, -machine help"
done
$V/python "$HERE/serial_check.py" $V/qemu-system-x86_64; check $? "host serial (cfset[io]speed)"

cd /tmp
for a in x86_64 aarch64 riscv64; do
  timeout 900 $V/python "$TESTS/smoke.py" $a > /tmp/smoke-$a.log 2>&1
  rc=$?
  r=$(grep PANDA_SMOKE /tmp/smoke-$a.log)
  # Ran 20000+ blocks (it stops itself there) and its plugins were loaded.
  [ $rc = 0 ] && echo "$r" | grep -q -E "blocks=[0-9]{5,} plugins=\['cskeleton'"
  check $? "smoke.py $a: $r"
  [ $rc = 0 ] || tail -20 /tmp/smoke-$a.log
done
exit $fail
