{
  description = "panda-next: the PANDA engine (qemu_builder + --enable-panda), its generated headers, and plugins";

  inputs = {
    # The engine: qemu_builder's patched 11.1 source (series + vendored src/, PANDA included).
    # Local development: --override-input qemu-builder path:/path/to/qemu_builder
    qemu-builder.url = "git+https://github.com/rehosting/qemu_builder?ref=workspace/panda-next-pandang";
    # One nixpkgs for everything, the one qemu_builder (and penguin) pin.
    nixpkgs.follows = "qemu-builder/nixpkgs";
    libpanda-ng-src = {
      url = "git+https://github.com/panda-re/libpanda-ng";
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
      # CI writes the release version to VERSION before building.
      version = nixpkgs.lib.fileContents ./VERSION;

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
                # QEMU's bundled meson drops nix's rpaths at install; put them back.
                autoPatchelfHook
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

              # After autoPatchelf (fixup): the engine has to load its libraries.
              doInstallCheck = true;
              installCheckPhase = ''
                $out/bin/qemu-system-${lib.removeSuffix "-softmmu" (builtins.head targets)} --version
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

          # Everything a user installs: the engine, its plugin-interface library and headers, and
          # the plugins, under one prefix.
          dist = pkgs.symlinkJoin {
            name = "panda-next-${version}";
            paths = [
              engine
              plugins
            ];
          };

          # A .deb of dist's runtime closure, copied as is into /nix/store, with dist linked to
          # /opt/panda-next and its binaries to /usr/bin. It needs nothing from the host's packages,
          # so one .deb per arch installs on any Debian or Ubuntu release.
          debArch =
            {
              x86_64-linux = "amd64";
              aarch64-linux = "arm64";
            }
            .${system};
          deb =
            pkgs.runCommand "panda-next_${version}_${debArch}.deb"
              {
                nativeBuildInputs = [ pkgs.dpkg ];
                closure = pkgs.closureInfo { rootPaths = [ dist ]; };
              }
              ''
                root=$TMPDIR/root
                mkdir -p $root/nix/store $root/opt $root/usr/bin $root/DEBIAN
                xargs -a $closure/store-paths cp -a -t $root/nix/store/
                ln -s ${dist} $root/opt/panda-next
                for b in ${dist}/bin/*; do
                  ln -s /opt/panda-next/bin/$(basename $b) $root/usr/bin/
                done
                cat > $root/DEBIAN/control <<EOF
                Package: panda-next
                Version: ${version}
                Architecture: ${debArch}
                Maintainer: PANDA developers
                Description: PANDA engine (QEMU with the PANDA API) and plugins
                 Built with nix. The runtime closure is under /nix/store; the install is at
                 /opt/panda-next.
                EOF
                chmod -R u+w $root
                dpkg-deb --root-owner-group -Zxz --build $root $out
              '';
        in
        {
          inherit
            engine
            plugins
            dist
            deb
            mkEngine
            ;
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
          inherit (p)
            engine
            engine-x86_64
            plugins
            dist
            deb
            ;
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
