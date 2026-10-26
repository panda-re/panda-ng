# libpanda-ng

This tool takes built QEMU binaries with source and generates header files for PANDA plugins.

The goal here is to build header files that are:
- Able to be fed into CFFI/compatible with PyCparser
- Able to be used in a C++ project (some manual changes)
- Supports C/C++/Python/Rust

It does so by running a python script inside of gdb with a reference to the QEMU binary. This script will then use then:
- Take a small header and run it through a C preprocessor
- Split the header into sys includes and the rest of the includes
- Parse the output of the preprocessor and simplify expressions with gdb simplify

The output of this script are header files per architecture per language that can be used in a PANDA plugin.

## In panda-ng

This is panda-re/libpanda-ng at 7b257bb with local fixes (aarch64 hosts, QEMU 11, a private
preprocessor directory instead of `/tmp/simple.{c,out}`). The engine build (`flake.nix`, `mkEngine`)
uses this directory (everything in it but this README), runs `run_all.sh` on its build tree and
installs the headers under `include/panda-ng/`.

To run it by hand against a configured and built engine tree (one with `build/libpanda-*.so`):

    nix develop .#libpanda-ng -c bash tools/libpanda-ng/run_all.sh <engine-tree>
    nix develop .#libpanda-ng -c bash tools/libpanda-ng/run_one.sh <engine-tree> <engine-tree>/build/libpanda-x86_64-softmmu.so

The headers (`panda_{c,cpp,...}_<target>.h`) are written to the current directory (the flake runs
the scripts from a copy of this directory, so there they land next to the scripts). The
`libpanda-ng` shell, not `engine`, because `build.py` runs inside gdb's embedded Python, which
needs the `PYTHONPATH` that shell sets (cffi, pycparser, tree-sitter).
