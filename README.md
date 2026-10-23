# panda-ng (panda-next)

PANDA on QEMU 11.1: the engine (qemu_builder's patched QEMU built with `--enable-panda`), the
headers libpanda-ng generates from it, panda-ng's plugins, and `pandare2`, the Python interface.
Everything is built by this repository's nix flake. There are three ways to use it:

| you want | use |
|---|---|
| `pip install` into your own Python | the **wheel**: `pandare2-<v>-py3-none-manylinux_2_38_<arch>.whl` |
| to build on it with nix | the **flake**: `packages`, `apps`, `overlays.default`, `lib` |
| plain QEMU | **`qemu-system-<arch>`**, from the flake (`nix run`) or the wheel (console scripts) |

Supported hosts: x86_64-linux and aarch64-linux. Targets: x86_64, i386, arm, aarch64, mips,
mipsel, mips64, mips64el, ppc, ppc64, loongarch64, riscv32, riscv64.

## The wheel

```sh
python3 -m venv venv
venv/bin/pip install pandare2-*-manylinux_2_38_$(uname -m).whl
venv/bin/python -c 'from pandare2 import Panda; Panda(arch="x86_64", mem="256M")'
venv/bin/qemu-system-riscv64 -version
```

The wheel needs glibc 2.38 or newer (Debian 13, Ubuntu 24.04, Fedora 39, RHEL 10 and later) and
nothing else from the host: the engine (`libpanda-<arch>-softmmu.so`), the plugins, QEMU's
firmware, the `qemu-system-<arch>` launcher, and every library they use apart from glibc
(glib, pixman, libslirp, libstdc++, ...) are inside it. It works with any Python 3 the host has
(pandare2's bindings are cffi ABI mode, so the wheel is `py3-none`). It is about 120 MB.

Build it with `nix build .#wheel` (the wheel is in `result/`). How it's made:

1. `enginePortable`: the engine built with `nix/glibc-compat/termios-compat.c` linked in. nix's
   glibc is 2.42, whose `cfsetispeed`/`cfsetospeed` have new symbol versions (speed_t became the
   baud rate itself). Those were the only symbols newer than GLIBC_2.38 anywhere in the engine,
   plugins or their libraries; the shim translates the rate to the old code and calls the
   original version, which every glibc has. The engine is otherwise the same as `.#engine`.
2. `pip wheel` builds the pure-Python pandare2 (with the cffi bindings generated from that
   engine's headers).
3. `tools/wheel/repair.py` adds the engine, plugins, firmware and launcher under
   `pandare2/data/`, and does what `auditwheel repair` does: it copies each needed library except
   glibc's into `pandare2.libs/` under a hashed name, rewrites DT_NEEDED and RUNPATH (`$ORIGIN`),
   gives the launcher the standard ELF interpreter, fails if anything still points into
   `/nix/store` or needs a glibc newer than 2.38, and tags the wheel
   `manylinux_2_<highest GLIBC_2.N needed>_<arch>`. (auditwheel itself won't bundle libstdc++,
   and nix's is newer than the hosts'.)

pandare2 looks for its engine, plugins and firmware in `pandare2/data/` first, so the same
source works from the wheel, from nix, and from a build tree. `PANDA_PLUGIN_DIR` overrides the
plugin directory.

Older hosts (glibc < 2.38: Ubuntu 22.04, Debian 12) would need the engine built in a manylinux
container instead; pip refuses the wheel there rather than failing at import.

## The flake

Packages (`nix build .#<name>`), for x86_64-linux and aarch64-linux:

| package | what |
|---|---|
| `engine` | QEMU + PANDA: `lib/libpanda-<arch>-softmmu.so`, `include/panda-ng/`, `share/qemu/`, QEMU's tools |
| `engine-x86_64` | one target, for iterating on the build |
| `plugins` (default) | `lib/panda/plugins/libpanda-<plugin>_<arch>-softmmu.so` |
| `pandare2` | the Python package, wired to this engine and these plugins |
| `python` | nix's Python with pandare2 (+ protobuf) |
| `qemu` | `bin/qemu-system-<arch>` for every target (+ qemu-img, qemu-io, qemu-nbd) |
| `qemu-system-<arch>` | one of those |
| `plugin-sdk` | headers for building a plugin outside this tree |
| `dist` | engine + plugins + python under one prefix (`panda-python`, `panda-system-<arch>`) |
| `deb` | `dist`'s closure as a .deb (bundles nix's Python; any Debian/Ubuntu) |
| `wheel` | the pip wheel |

Apps: `nix run .#qemu-system-<arch> -- <qemu args>`, `nix run .#python -- script.py`.

### Using it from another flake

[`examples/consumer`](examples/consumer/flake.nix) is a complete one: it builds an out-of-tree C
plugin against the engine, runs a pandare2 script that loads it, and re-exports `qemu`.

```nix
{
  inputs.panda-ng.url = "github:panda-re/panda-ng/workspace/panda-next";
  inputs.nixpkgs.follows = "panda-ng/nixpkgs";   # share panda-ng's builds

  outputs = { nixpkgs, panda-ng, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; overlays = [ panda-ng.overlays.default ]; };
      pn = panda-ng.lib.${system};
    in {
      packages.${system} = {
        # overlay: pkgs.pandaNext.<package>, and pandare2 in every Python package set
        python = pkgs.python3.withPackages (ps: [ ps.pandare2 ]);
        qemu = pkgs.pandaNext.qemu;
        # lib: build a plugin against the engine's headers
        myplugin = pn.buildPandaPlugin { pname = "myplugin"; src = ./plugin; };
      };
    };
}
```

- `overlays.default` adds `pandaNext` (everything in the table above, plus the helpers below)
  built with the consumer's nixpkgs, and `pandare2` to every Python package set through
  `pythonPackagesExtensions`.
- `lib.<system>`: this flake's `engine`, `plugins`, `pluginDir`, `pluginSdk`, `qemu`, `pandare2`,
  `python`, and:
  - `buildPandaPlugin { pname, src, targets ? [ "x86_64" "aarch64" "riscv64" ], buildInputs, extraCFlags }`:
    compiles every `*.c` in `src` into `lib/panda/plugins/libpanda-<pname>_<arch>-softmmu.so`.
  - `pluginDirWith [ drv ... ]`: a plugin directory with panda-ng's plugins and yours, for
    `PANDA_PLUGIN_DIR` or `Panda(plugin_path=...)`.
  - `mkEngine { targets, portable }`: an engine for other targets.
- `lib.mkPandaNext pkgs`: the whole set for any nixpkgs (what the overlay calls).

Local development against a qemu_builder checkout:
`nix build --override-input qemu-builder git+file:///path/to/qemu_builder?ref=<branch> .#python`.
Checks: `nix flake check` runs `-version` and `-machine help` on every `qemu-system-<arch>`.

## qemu-system-&lt;arch&gt;

```sh
nix run .#qemu-system-aarch64 -- -M virt -cpu max -bios edk2-aarch64-code.fd -m 1G \
    -display none -serial stdio -drive file=disk.qcow2,if=virtio
venv/bin/qemu-system-x86_64 -version        # from the wheel
```

The engine is built as a library, `libpanda-<arch>-softmmu.so` (the object pandare2 loads), and
`qemu-system-<arch>` is [`launcher/panda-qemu.c`](launcher/panda-qemu.c): a few kB that
dlopens that library (`RTLD_GLOBAL`) and calls its `main()`. It picks the arch from its name
(`qemu-system-<arch>`, `panda-system-<arch>`, or `panda-qemu <arch> ...`), finds the library in
`../lib` (or `$PANDA_QEMU_LIBDIR`), and QEMU finds its firmware in `../share/qemu` relative to
the launcher. Why a launcher rather than the engine's own `bin/qemu-system-*` executables or
linking those with `--export-dynamic`:

- it runs exactly the code pandare2 runs, and everything the library exports (the PANDA API) is
  visible to plugins QEMU loads; the engine's executables export only `qemu_plugin_*` (80
  symbols, no `panda_*`), so a PANDA plugin can't load in them;
- `--export-dynamic` would fix the exports, but each executable is still a second ~25 MB copy of
  its target (308 MB for 13), which the wheel and the .deb would have to carry next to the
  libraries; the launcher costs nothing.

The engine still installs its own `bin/qemu-system-*` (the `engine` and `dist` outputs keep
them for now).
