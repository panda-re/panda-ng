#!/usr/bin/env bash
# Regenerates syscalls2's Linux system call tables and the code generated from them:
# generated-in/linux_<abi>_prototypes.txt from a Linux source tree (linux_prototypes.py), then
# generated/ from every generated-in table (make_all_generated.sh).
#
#   scripts/regen.sh <linux source: directory or .tar.xz>
#
# `nix run .#regen-syscalls` (from the repository root) runs it on the flake's pinned kernel, and
# the flake check syscalls2-tables fails when the committed files differ from its output.
# PYTHON: a python3 with jinja2 (default: python3).
set -euo pipefail
src=$(realpath "$1")
cd "$(dirname "$0")"
"${PYTHON:-python3}" linux_prototypes.py "$src" ../generated-in
PYTHON=${PYTHON:-python3} bash make_all_generated.sh
