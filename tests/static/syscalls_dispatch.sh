#!/usr/bin/env bash
# The generated Linux x64 dispatcher, real PPP registry, and an instrumented engine boundary.
# No engine build or guest needed. Optional second argument selects a baseline enter source
# for comparison with <binary> --bench idle|enter|return|all-return.
set -euo pipefail
root=${PANDA_SOURCE:-$(cd "$(dirname "$0")/../.." && pwd)}
out=$(realpath -m "$1")
d=$root/plugins/syscalls2
t=$root/tests/static/syscalls2
"${CXX:-c++}" -std=c++17 -O2 -DTARGET_I386 -DTARGET_X86_64 \
    -I"$t/include" -I"$root/include" -I"$root/engine/src/include" -I"$d" -I"$d/generated" \
    "$t/dispatch.cpp" "${2:-$d/generated/syscall_switch_enter_linux_x64.cpp}" \
    "$d/generated/syscall_switch_return_linux_x64.cpp" -o "$out"
if [ "$#" = 1 ]; then "$out"; fi
