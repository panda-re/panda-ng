{
  description = "panda-next: the PANDA engine (qemu_builder + --enable-panda), its generated headers, and plugins";

  inputs = {
    # The engine: qemu_builder's patched 11.1 source (series + vendored src/, PANDA included).
    # Local development: --override-input qemu-builder path:/path/to/qemu_builder
    qemu-builder.url = "github:rehosting/qemu_builder?ref=workspace/panda-next-pandang";
    # One nixpkgs for everything, the one qemu_builder (and penguin) pin.
    nixpkgs.follows = "qemu-builder/nixpkgs";
    libpanda-ng-src = {
      url = "github:panda-re/libpanda-ng";
      flake = false;
    };
  };

  outputs =
    {
      nixpkgs,
      qemu-builder,
      libpanda-ng-src,
      ...
    }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      version = "0.0.0-next";

      defaultTargets = [
        "x86_64-softmmu"
        "i386-softmmu"
        "arm-softmmu"
        "aarch64-softmmu"
        "mips-softmmu"
        "mipsel-softmmu"
        "mips64-softmmu"
        "mips64el-softmmu"
        "ppc-softmmu"
        "ppc64-softmmu"
        "loongarch64-softmmu"
        "riscv32-softmmu"
        "riscv64-softmmu"
      ];

      perSystem =
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          lib = pkgs.lib;

          # libpanda-ng with local fixes until they land upstream.
          libpanda-ng = pkgs.applyPatches {
            name = "libpanda-ng";
            src = libpanda-ng-src;
            patches = [ ./nix/libpanda-ng-fixes.patch ];
          };

          gdbPythonPath = pkgs.python3.pkgs.makePythonPath (
            with pkgs.python3.pkgs;
            [
              cffi
              pycparser
              tree-sitter
              tree-sitter-grammars.tree-sitter-c
            ]
          );

          # The engine. Keeps nothing of the build tree but what it installs, plus the headers
          # libpanda-ng generates from it (which need the tree, so they are made here).
          mkEngine =
            {
              targets ? defaultTargets,
            }:
            pkgs.stdenv.mkDerivation {
              pname = "panda-engine";
              inherit version;
              src = qemu-builder.packages.${system}.src;

              nativeBuildInputs = with pkgs; [
                pkg-config
                ninja
                perl
                git
                gdb
                (python3.withPackages (ps: [
                  ps.setuptools
                  ps.wheel
                  ps.pip
                  ps.cffi
                  ps.tree-sitter
                  ps.tree-sitter-grammars.tree-sitter-c
                ]))
              ];
              buildInputs = with pkgs; [
                glib
                pixman
                zlib
                zstd
                libcap_ng
                libslirp
                dtc
              ];

              # The source is read-only in the store; configure writes pyvenv/ and build/.
              unpackPhase = ''
                cp -r $src source
                chmod -R u+w source
                cd source
                patchShebangs scripts
              '';

              configurePhase = ''
                runHook preConfigure
                mkdir build && cd build
                ../configure --prefix=$out \
                  --target-list=${lib.concatStringsSep "," targets} \
                  --enable-plugins --enable-panda \
                  --disable-docs --disable-werror --disable-gtk --disable-sdl --disable-opengl \
                  --disable-virglrenderer --disable-spice --disable-vnc --disable-containers
                cd ..
                runHook postConfigure
              '';

              buildPhase = ''
                runHook preBuild
                ninja -C build -j$NIX_BUILD_CORES
                # libpanda-ng: gdb reads each build/libpanda-<target>.so and writes
                # panda_{c,cpp,...}_<target>.h next to its scripts.
                # gdb embeds its own python3 (same nixpkgs); give it build.py's modules.
                cp -r --no-preserve=mode ${libpanda-ng} ../libpanda-ng
                (cd ../libpanda-ng && PYTHONPATH=${gdbPythonPath} bash run_all.sh $PWD/../source)
                runHook postBuild
              '';

              installPhase = ''
                runHook preInstall
                ninja -C build install
                cp build/contrib/plugins/libpanda_plugin_interface.so $out/lib/
                mkdir -p $out/include/panda-ng
                cp ../libpanda-ng/*.h $out/include/panda-ng/
                runHook postInstall
              '';
            };

          engine = mkEngine { };

          pluginTools = {
            nativeBuildInputs = with pkgs; [
              pkg-config
              meson
              ninja
              cargo
              rustc
            ];
            buildInputs = with pkgs; [
              glib
              curl
              (python3.withPackages (ps: [ ps.pycparser ]))
            ];
          };

          # panda-ng's plugins, built against the engine's generated headers.
          plugins = pkgs.stdenv.mkDerivation {
            pname = "panda-plugins";
            inherit version;
            src = ./.;
            inherit (pluginTools) nativeBuildInputs buildInputs;
            NIX_CFLAGS_COMPILE = "-std=c++17";
            mesonFlags = [ (lib.mesonOption "targets" (lib.concatStringsSep "," defaultTargets)) ];
            preConfigure = ''
              mkdir -p local_packages
              ln -s ${engine}/include/panda-ng local_packages/panda-ng
            '';
          };
        in
        {
          inherit engine plugins mkEngine;
          # For running libpanda-ng by hand against a build tree:
          #   nix develop .#libpanda-ng -c bash run_one.sh <qemu-src-with-build/> <libpanda-*.so>
          libpanda-ng-shell = pkgs.mkShell {
            packages = with pkgs; [
              gdb
              pkg-config
              glib
            ];
            PYTHONPATH = gdbPythonPath;
          };
          plugins-shell = pkgs.mkShell (pluginTools // { NIX_CFLAGS_COMPILE = "-std=c++17"; });
          # A fast engine for iterating on the build: one target.
          engine-x86_64 = mkEngine { targets = [ "x86_64-softmmu" ]; };
        };
    in
    {
      packages = forAllSystems (
        system:
        let
          p = perSystem system;
        in
        {
          inherit (p) engine engine-x86_64 plugins;
          default = p.plugins;
        }
      );
      devShells = forAllSystems (system: {
        libpanda-ng = (perSystem system).libpanda-ng-shell;
        # The plugins' toolchain without the engine: point local_packages/panda-ng at headers yourself.
        plugins = (perSystem system).plugins-shell;
      });
      lib = forAllSystems (system: { inherit (perSystem system) mkEngine; });
      formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixfmt-rfc-style);
    };
}
