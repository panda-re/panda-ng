# syscalls2 maintenance

**Note:** This document is work in progress. `README.md` describes the plugin's callbacks and API.

In this document we highlight how support for new operating systems can be added to `syscalls2`. Overall, this is a three step process:

A. Extract the system call prototypes for the new OS. The prototypes in general consist of the system call *number* and function *signature*. (For Linux this and step B are one command: see below.)

B. Generate headers and source files from the prototypes, so that `syscalls2` will be able to intercept the system calls for the target OS.

C. Modify the hand-written syscalls2 code to include the profile for the new target.

## Linux: one command

From the repository root:

```sh
nix run .#regen-syscalls
```

It runs `scripts/regen.sh` on the flake's pinned kernel source (nixpkgs' `linux_latest`, the
version is in the first line of each table), which

1. writes `generated-in/linux_<abi>_prototypes.txt` for x64, x86, arm (EABI and the ARM-private
   calls), arm64, riscv64, mips (o32), mips64 (o32, n64 and n32 by number), mips64n32 and ppc/ppc64
   with `scripts/linux_prototypes.py`: numbers from the kernel's `*.tbl` for each ABI, prototypes
   from `include/linux/syscalls.h` (its `#if`s evaluated for the ABI) and the arch's own
   declarations (its docstring has the rules); a system call with no prototype fails the run;
2. runs `scripts/make_all_generated.sh`, which rewrites `generated/` from every table and
   `generated-in/context_target.json`.

The flake check `syscalls2-tables` (part of `nix flake check`) regenerates both and fails when
the committed files differ, so a table can't go stale or be edited by hand. Syscall numbers only
grow, so the newest kernel's tables describe older guests as well. A nixpkgs bump that moves
`linux_latest` makes the check fail until `nix run .#regen-syscalls` is committed with it.

Without nix: `PYTHON=<python3 with jinja2> plugins/syscalls2/scripts/regen.sh <linux source
tree or .tar.xz>`.

## Extracting system call prototypes (Windows)

The files used to generate the Windows prototype files are in the `scripts` folder:

- `make_all_prototypes.sh` - calls `prototype_parser.py` for each target
- `prototype_parser.py` - uses the `prototype_parser_config.py` to create the desired prototype file
- `prototype_parser_config.py` - configuration information needed to generate each prototype

### Windows
The `make_all_prototypes.sh` script already creates prototypes for a great many Windows versions.  It is the next step, generating the support files, that drops many Windows versions from the processing.

TBD:  How to extract system call prototypes for another Windows version

## Generating `syscalls2` support files

`scripts/README.md` says how to set up the scripts' Python environment.

The files used to generate the C/C++ headers and sources are in the `scripts`, `generated-in` and `generated-tpl` folders:

- `scripts/make_all_generated.sh` - calls `syscall_parser.py` for the desired targets
- `generated-in/context_target.json` - lists system calls that do not return for certain targets; specified by `make_all_generated.sh` as input to `syscall_parser.py`.
- `generated-tpl/` - contains template files used by `syscall_parser.py`
- `generated-in/` - contains the prototypes.txt files (Linux: `linux_prototypes.py`; Windows: `make_all_prototypes.sh`) used as input to `make_all_generated.sh`

Some of the files created by `make_all_generated.sh` contain information on more than one target.  Thus it is necessary to have all input prepared, and to run the script with all targets specified.  The output will be written to the `generated` folder.

The high-level steps are the same for both Linux and Windows:

Modify `make_all_generated.sh` to add your new target to the `syscall_parser.py` arguments (and, for a new Linux ABI, `ABIS` in `linux_prototypes.py`).

If your new target has any system calls that do not return, add a section for your target naming them in the `context_target.json` file.

In `syscall_parser.py`, it may be necessary to add a new architecture and/or OS, and to tweak the type lists and/or their processing to properly convert the types encountered in your new target's prototype file.

## Modifying syscalls2 hand-written code

Modify `syscalls2.h` to add function prototypes for the generated enter and return callbacks.  See the comment above the list in the file for directions on how to easily generate this list from the files created by `make_all_generated.sh`.

Modify `syscalls2.cpp` to add a new profile for your new target, and add code to select it under the appropriate conditions when the plugin is initialized.

Review `syscalls2_info.c` and tweak if necessary to ensure the correct syscalls information file will be loaded for the new target.

And now, finally, rebuild the plugins (`nix build .#plugins`). `meson.build` compiles every `generated/syscall_switch_*.cpp` and `generated/dso_info_*.c`, so new generated files need no build change.
