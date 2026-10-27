#!/usr/bin/env bash
# The guest suite: every test in TESTS on every image, each image in parallel, each test on its own
# copy of the image (the tests revert and write snapshots; the shared images are never touched).
#
#   nix build .#python -o result-python
#   PANDA_PYTHON=$PWD/result-python/bin/python3 tests/run.sh ~/panda-images/*.json
#
# Images are built by pandare2.mkimage (tests/README.md). Arguments are image jsons or directories
# of them. Environment:
#   PANDA_PYTHON  Python with pandare2 (default: panda-python from PATH)
#   TESTS         space-separated test names (default: the 19 in DEFAULT_TESTS)
#   OUT           results and logs (default: $TMPDIR/panda-tests-<time>)
#   TIMEOUT       seconds per test (default 1800)
#   JOBS          images run at once (default: all)
#   KEEP=1        keep each test's image copy and work dir (default: kept only when it fails)
#
# Output: $OUT/results (one "PASS|FAIL|TIMEOUT <secs>s <image> <test>" line per run), logs in
# $OUT/<image>/<test>.log. Exits 1 if any run did not pass.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
DEFAULT_TESTS="guest api osi_isf blocks callbacks calls mem late reset teardown asidstory tbpc hookalias
  rr rrcb rrstop rrload fuzz fastrestore"
TESTS=${TESTS:-$DEFAULT_TESTS}
TIMEOUT=${TIMEOUT:-1800}
OUT=${OUT:-${TMPDIR:-/tmp}/panda-tests-$(date +%Y%m%d-%H%M%S)}
KEEP=${KEEP:-0}

if [ -z "${PANDA_PYTHON:-}" ]; then
  PANDA_PYTHON=$(command -v panda-python || echo panda-python)
fi
if ! "$PANDA_PYTHON" -c 'import pandare2' 2>/dev/null; then
  echo "run.sh: $PANDA_PYTHON can't import pandare2; set PANDA_PYTHON (nix build .#python -o result-python," \
    "then PANDA_PYTHON=\$PWD/result-python/bin/python3)" >&2
  exit 2
fi

images=()
for a in "$@"; do
  if [ -d "$a" ]; then
    for j in "$a"/*.json; do [ -e "$j" ] && images+=("$(realpath "$j")"); done
  else
    images+=("$(realpath "$a")")
  fi
done
if [ ${#images[@]} -eq 0 ]; then
  sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//' >&2
  exit 2
fi
JOBS=${JOBS:-${#images[@]}}

mkdir -p "$OUT"
OUT=$(realpath "$OUT")
# The tests' TMPDIRs: short, since a UNIX socket's path is limited to 108 bytes
# (fastrestore_vsock's vhost-user sockets).
RUNTMP=$(mktemp -d "${TMPDIR:-/tmp}/pt.XXXXXX")
: >"$OUT/results"
# Stopping run.sh stops every test it started. Not `kill 0`: timeout(1) puts each test in a
# process group of its own.
descendants() {
  local c
  for c in $(pgrep -P "$1"); do
    echo "$c"
    descendants "$c"
  done
}
stop() {
  trap - INT TERM
  # shellcheck disable=SC2046
  kill $(descendants $$) 2>/dev/null
  sleep 1
  rm -rf "$RUNTMP"
  exit 130
}
trap stop INT TERM
echo "run.sh: ${#images[@]} image(s) x $(echo "$TESTS" | wc -w) test(s), $JOBS at once, timeout ${TIMEOUT}s; results in $OUT"

# One test on a private copy of the image: the json and qcow2 are copied, the symbol files
# (<name>.isf.json.xz, <name>.kernelinfo.conf) linked, since the json names them relative to itself.
run_one() {
  local json=$1 test=$2
  local name dir work log rc start secs status arch tmp
  name=$(basename "$json" .json)
  dir=$(dirname "$json")
  work="$OUT/$name/$test"
  log="$OUT/$name/$test.log"
  rm -rf "$work"
  mkdir -p "$work/img"
  tmp=$(mktemp -d "$RUNTMP/XXXX")
  for f in "$dir/$name".*; do
    case $f in
      *.qcow2) cp --reflink=auto --sparse=always "$f" "$work/img/" ;;
      *.json) cp "$f" "$work/img/" ;;
      *) ln -s "$f" "$work/img/" ;;
    esac
  done
  arch=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["arch"])' "$json")

  local args=("$work/img/$name.json")
  # The rr tests take a work dir for their recordings.
  case $test in rr | rrcb | rrstop | rrload | rr_blocks) mkdir -p "$work/rr" && args+=("$work/rr") ;; esac
  # rrcb compares icounts across record/replay; on aarch64/riscv64 the guest's reads of it are
  # not yet exact ("Bad icount read"), so those images check callbacks without it.
  local env=(TMPDIR="$tmp")
  [ "$arch" != x86_64 ] && env+=(RRCB_NO_ICOUNT=1)

  start=$(date +%s)
  set +e
  (cd "$work" && env "${env[@]}" timeout -k 30 "$TIMEOUT" "$PANDA_PYTHON" "$HERE/$test.py" "${args[@]}") \
    >"$log" 2>&1
  rc=$?
  set -e
  secs=$(($(date +%s) - start))
  case $rc in
    0) status=PASS ;;
    124 | 137) status=TIMEOUT ;;
    *) status=FAIL ;;
  esac
  echo "$status ${secs}s $name $test" | tee -a "$OUT/results"
  echo "rc=$rc" >>"$log"
  rm -rf "$tmp"
  if [ "$status" = PASS ] && [ "$KEEP" != 1 ]; then rm -rf "$work"; fi
}

run_image() {
  local json=$1 t
  mkdir -p "$OUT/$(basename "$json" .json)"
  for t in $TESTS; do run_one "$json" "$t"; done
}

running=0
for json in "${images[@]}"; do
  run_image "$json" &
  running=$((running + 1))
  if [ "$running" -ge "$JOBS" ]; then
    wait -n || true
    running=$((running - 1))
  fi
done
wait
rm -rf "$RUNTMP"

total=$(wc -l <"$OUT/results")
passed=$(grep -c '^PASS' "$OUT/results" || true)
echo "run.sh: $passed/$total passed ($OUT/results)"
[ "$passed" -eq "$total" ]
