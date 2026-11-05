# Developing panda-ng

`nix build` builds the engine for every target, then the plugins and pandare2 against the
engine's installed headers. That is the release build, and what CI runs; it starts from scratch
whenever the engine's source or headers change. For working on the engine, the plugins or
pandare2, `tools/dev/dev.sh` keeps an incremental build of all three for a few targets in `.dev/`
at the repository root, and a Python that runs pandare2 from `python/core` on it.

## Build

```sh
tools/dev/dev.sh build --targets x86_64          # first run: import, configure, build
tools/dev/dev.sh build                           # later runs: only what changed
.dev/bin/python3 tests/smoke.py x86_64
```

Every run goes through the same steps, and each does its work only when its inputs changed:

| step | does | when |
|---|---|---|
| `engine` | `engine/scripts/import-series.sh` makes `.dev/qemu/` (the tarball, one commit per patch, the `engine/src` overlay), configured as the flake configures it; `ninja` builds `libpanda-<arch>-softmmu.so`, the plugin interfaces and the firmware | always (`ninja` is a no-op when nothing changed). A changed series is applied again to a fresh checkout of the base and the tree moved to the result, so only files whose content changed are rebuilt; `engine/src` files are copied when their content differs |
| `headers` | libpanda-ng (`tools/libpanda-ng`) generates `panda_{c,cpp,python}_<arch>.h` into `.dev/prefix/include/panda-ng` | when the content of a header in the engine's tree or build changed. Only headers whose output differs are rewritten, so the plugins rebuild only on a real change |
| `h2b` | the taint shadow helpers (`engine/h2b`) | when their sources changed |
| `plugins` | the plugins' meson build in `.dev/plugins`, against `.dev/prefix` | always (`ninja`) |
| `bindings` | pandare2's cffi bindings in `python/core/pandare2/autogen/` | when `panda_python_<arch>.h` or a plugin header they read changed |

Options of `build`:

- `--targets x86_64,aarch64`: the arches to build. Remembered; the first default is the host's.
  Changing the list configures QEMU again, which rebuilds most of the engine.
- `--plugins osi,syscalls2`: build only these plugins (all of `plugins/config.panda` by default).
- step names, to run only those: `tools/dev/dev.sh build plugins bindings`.
- `-j N` (default: the CPUs the process may run on, so `taskset` limits it), `--force-headers`,
  `--no-h2b`.

## Run

`.dev/bin/python3` (also `.dev/bin/panda-python`) is the dev shell's Python with `python/core` on
`PYTHONPATH`, `PANDA_LIB` set to `.dev/prefix/lib` and `PANDA_PLUGIN_DIR` to
`.dev/plugins/plugins`. Use it wherever a test wants pandare2's Python:

```sh
PANDA_PYTHON=$PWD/.dev/bin/python3 TESTS="syscalls api" tests/run.sh ~/panda-images/debian-13-x86_64.json
.dev/bin/python3 tests/static/refs.py x86_64
```

(`run.sh` runs each test on a copy of the image; a test run directly writes snapshots into the
image it is given.)

`tools/dev/dev.sh env` prints those variables as shell exports. `tools/dev/dev.sh shell` opens the
dev shell (QEMU's and the plugins' toolchains) with them set, for running
`ninja -C .dev/qemu/build` or a debugger by hand. `tools/dev/dev.sh status` shows what is built,
and `tools/dev/dev.sh clean [engine|plugins]` removes it.

On 8 cores of a 2026 x86_64 server, for x86_64: the first build takes about 4 minutes and
1.8 GB; a run with nothing to do 3 s; a changed engine source file 4 s; a changed engine header
about 40 s (every plugin includes the generated headers); a changed plugin source file 2 s; a
changed patch about 20 s plus what it rebuilds.

## How it relates to the nix build

The steps run in the flake's `dev` shell (`nix develop .#dev`), entered without the caller's
environment, as `nix develop -i` does. `dev.sh` caches the shell's environment in `.dev/env.sh`
and makes it again when `flake.nix`, `flake.lock` or `engine/base.json` changes. The configure
line and the layout of `.dev/prefix` (`lib/`, `include/panda-ng`, `include/panda`,
`share/qemu`) are the flake's (`mkEngine`), with the libraries linked into the build tree rather
than installed. The differences: only the chosen targets are built, the plugins are built with meson's `debug` build
type (`-O0 -g`), and the engine keeps the build tree's RUNPATHs.

`.dev/qemu` is the build's own tree: it is moved to each new series with `git reset --hard`.
Changes to the series are made in a tree of your own and exported with
`engine/scripts/export-series.sh` (the
[engine README](https://github.com/panda-re/panda-ng/blob/main/engine/README.md)); the next
`dev.sh build` picks them up. The full `nix build` stays the check before a pull request.

The Rust bindings (`rust/panda-bindings`) are checked in, not generated from the dev headers:
after an engine header change, `nix run .#regen-rust-bindings` makes them from the nix-built
engine. The Rust plugin (`rust_skeleton`) builds with cargo, against those bindings.
