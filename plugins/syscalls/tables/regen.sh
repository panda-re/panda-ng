#!/usr/bin/env bash
# Regenerates the syscalls plugin's tables: generated-in/linux_{riscv32,loongarch64,x32,arm_oabi}
# _prototypes.txt from a Linux source tree (linux_extra_prototypes.py), then generated/ from those
# and syscalls2's prototype files (gen_tables.py).
#
#   plugins/syscalls/tables/regen.sh <linux source: directory or .tar.xz>
#
# `nix run .#regen-syscalls` (from the repository root) runs it on the flake's pinned kernel, and
# the flake check syscalls-tables fails when the committed files differ from its output.
# PYTHON: a python3 with jinja2 (default: python3).
set -euo pipefail
src=$(realpath "$1")
cd "$(dirname "$0")"
"${PYTHON:-python3}" linux_extra_prototypes.py "$src" generated-in
"${PYTHON:-python3}" gen_tables.py
