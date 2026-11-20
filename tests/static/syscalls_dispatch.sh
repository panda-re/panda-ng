#!/usr/bin/env bash
# syscalls2's dispatch (sc2_dispatch.inc), the generated x64 and i386 tables and PPP functions,
# against a stub engine. No engine build or guest needed. <binary> --bench idle|typed|all|context
# times an entry and its return through the dispatch alone.
#
# PANDA_TEST = dict(area="syscalls", tier="static", needs=["c++"], args=["{work}/dispatch"],
#      env={"PANDA_SOURCE": "{repo}"}, nix_check="syscalls2-dispatch")
set -euo pipefail
root=${PANDA_SOURCE:-$(cd "$(dirname "$0")/../.." && pwd)}
out=$(realpath -m "$1")
d=$root/plugins/syscalls2
t=$root/tests/static/syscalls2
"${CXX:-c++}" -std=c++17 -O2 -DTARGET_I386 -DTARGET_X86_64 \
    -I"$t/include" -I"$root/include" -I"$root/engine/src/include" -I"$d" -I"$d/generated" \
    "$t/dispatch.cpp" "$d/generated/syscall_table_linux_x64.cpp" \
    "$d/generated/syscall_table_linux_x86.cpp" -o "$out"
if [ "$#" = 1 ]; then "$out"; fi
