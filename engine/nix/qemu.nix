# penguin-qemu, built from an upstream QEMU release tarball + the IGLOO series.
#
# `src` comes from nix/source.nix (tarball + patches/<version>/series + the
# vendored src/). This runs ./build.sh, which now reads its feature set from
# configs/<profile>.json: configure a
# TCG-only `build-system` for every Penguin system arch, ninja the
# libqemu-system-*.so shared libraries + qemu-img, run the CFFI generators
# (scripts/penguin-cffi-gen.py + scripts/penguin-env-cffi-gen.py), then package
# everything with scripts/penguin-qemu-package.py.
#
# Two outputs:
#   out  -- the unpacked tree (bin/ include/ lib/ share/), i.e. the contents
#           that the penguin image lays down under /usr/local. This is what
#           penguin's mk-penguin-qemu.nix consumes when this flake replaces the
#           prebuilt release tarball input.
#   dist -- penguin-qemu.tar.gz, the release artifact the CI publishes.
#
# The compiled CFFI env modules (lib/penguin-qemu-env/_penguin_qemu_env_*.so)
# are tied to the building CPython's ABI, so this MUST build against the same
# nixpkgs (hence the same python3) that penguin's image uses -- both flakes pin
# the identical nixpkgs commit for exactly this reason.
{
  lib,
  stdenv,
  src,
  version,
  pkg-config,
  ninja,
  python3,
  perl,
  git, # meson resolves a couple of subproject wraps; git must be on PATH
  glib,
  pixman,
  zlib,
  libcap_ng,
  libslirp, # satisfies the slirp.wrap with the system lib (no network)
  dtc, # provides libfdt (--enable-fdt=system) and the dtc compiler
  # Libraries backing the --enable-* group. NOT hand-listed here: flake.nix
  # derives them from configs/<profile>.json's `nixDeps`, so the flag list and
  # the dependency list cannot drift apart. rehosting/qemu keeps the same two
  # lists in two files with a comment asking a human to sync them.
  extraBuildInputs ? [ ],
  # Restrict the built system arch set if desired (build.sh's default covers
  # the full Penguin matrix). Comma-separated, matching PENGUIN_SYSTEM_ARCHES.
  systemArches ? null,
  # KVM acceleration libraries are host-arch specific. When enabled (the
  # default, matching the released artifact), build.sh auto-detects the host
  # arch's KVM target -- x86_64-softmmu on x86_64, aarch64-softmmu on aarch64 --
  # and produces libqemu-kvm-<arch>.so alongside the TCG system libraries. KVM
  # only needs the Linux UAPI headers at build time, not /dev/kvm, so it builds
  # fine in the sandbox. Set false for a host-arch-independent TCG-only build.
  enableKvm ? true,
}:

let
  # The python used both to run configure/meson and to compile the CFFI
  # extension modules. pyelftools>=0.31 (DWARF5) and cffi are the script deps;
  # pip/setuptools/wheel let QEMU's mkvenv install its vendored meson wheel
  # offline.
  pythonForBuild = python3.withPackages (ps: [
    ps.pyelftools
    ps.cffi
    ps.setuptools
    ps.wheel
    ps.pip
  ]);
in
stdenv.mkDerivation {
  pname = "penguin-qemu";
  inherit version src;

  outputs = [
    "out"
    "dist"
  ];

  nativeBuildInputs = [
    pkg-config
    ninja
    pythonForBuild
    perl
    git
  ];

  buildInputs = [
    glib
    pixman
    zlib
    libcap_ng
    libslirp
    dtc
  ]
  ++ extraBuildInputs;

  # The store source is read-only but build.sh writes build-system/ and
  # pyvenv/ into the tree, so work from a writable copy.
  unpackPhase = ''
    runHook preUnpack
    cp -r ${src} qemu-src
    chmod -R u+w qemu-src
    cd qemu-src
    # No subproject vendoring needed: the upstream RELEASE TARBALL ships
    # subprojects/{keycodemapdb,berkeley-softfloat-3,berkeley-testfloat-3}
    # already populated and build-ready (the berkeley packagefiles overlay is
    # pre-applied upstream), and slirp/dtc are satisfied by system libraries.
    # Building from a git base required three fetchFromGitLab pins with
    # per-version hashes; the tarball base removes that whole class of work.
    runHook postUnpack
  '';

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    ${lib.optionalString (systemArches != null) ''export PENGUIN_SYSTEM_ARCHES="${systemArches}"''}
    ${lib.optionalString (!enableKvm) ''export PENGUIN_KVM_TARGETS=none''}
    patchShebangs scripts build.sh
    ./build.sh
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p "$dist"
    cp penguin-qemu.tar.gz "$dist/penguin-qemu.tar.gz"

    # Unpacked tree (the /usr/local overlay payload).
    mkdir -p "$out"
    tar xzf penguin-qemu.tar.gz -C "$out"
    runHook postInstall
  '';

  # qemu-img and the .so libraries are ELF with store-path rpaths already; no
  # stripping surprises needed beyond the defaults.
  meta = {
    description = "Penguin's PANDA-QEMU fork (libqemu-system shared libs + qemu-img + CFFI bindings)";
    homepage = "https://github.com/rehosting/qemu_builder";
    license = lib.licenses.gpl2Plus;
  };
}
