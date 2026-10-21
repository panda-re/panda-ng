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